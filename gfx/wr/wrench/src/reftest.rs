/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{WindowWrapper, NotifierEvent};
use base64;
use semver;
use image::load as load_piston_image;
use image::png::PNGEncoder;
use image::{ColorType, ImageFormat};
use crate::parse_function::parse_function;
use crate::png::save_flipped;
use std::cmp;
use std::fmt::{Display, Error, Formatter};
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::mpsc::Receiver;
use webrender::RenderResults;
use webrender::api::*;
use webrender::api::units::*;
use crate::wrench::{Wrench, WrenchThing};
use crate::yaml_frame_reader::YamlFrameReader;


const OPTION_DISABLE_SUBPX: &str = "disable-subpixel";
const OPTION_DISABLE_AA: &str = "disable-aa";
const OPTION_DISABLE_DUAL_SOURCE_BLENDING: &str = "disable-dual-source-blending";
const OPTION_ALLOW_MIPMAPS: &str = "allow-mipmaps";

pub struct ReftestOptions {
    // These override values that are lower.
    pub allow_max_difference: usize,
    pub allow_num_differences: usize,
}

impl ReftestOptions {
    pub fn default() -> Self {
        ReftestOptions {
            allow_max_difference: 0,
            allow_num_differences: 0,
        }
    }
}

pub enum ReftestOp {
    Equal,
    NotEqual,
}

impl Display for ReftestOp {
    fn fmt(&self, f: &mut Formatter) -> Result<(), Error> {
        write!(
            f,
            "{}",
            match *self {
                ReftestOp::Equal => "==".to_owned(),
                ReftestOp::NotEqual => "!=".to_owned(),
            }
        )
    }
}

#[derive(Debug)]
enum ExtraCheck {
    DrawCalls(usize),
    AlphaTargets(usize),
    ColorTargets(usize),
    /// Checks the dirty region when rendering the test at |index| in the
    /// sequence, and compares its serialization to |region|.
    DirtyRegion { index: usize, region: String },
}

impl ExtraCheck {
    fn run(&self, results: &[RenderResults]) -> bool {
        match *self {
            ExtraCheck::DrawCalls(x) =>
                x == results.last().unwrap().stats.total_draw_calls,
            ExtraCheck::AlphaTargets(x) =>
                x == results.last().unwrap().stats.alpha_target_count,
            ExtraCheck::ColorTargets(x) =>
                x == results.last().unwrap().stats.color_target_count,
            ExtraCheck::DirtyRegion { index, ref region } => {
                *region == format!("{}", results[index].recorded_dirty_regions[0])
            }
        }
    }
}

pub struct Reftest {
    op: ReftestOp,
    test: Vec<PathBuf>,
    reference: PathBuf,
    font_render_mode: Option<FontRenderMode>,
    max_difference: usize,
    num_differences: usize,
    extra_checks: Vec<ExtraCheck>,
    disable_dual_source_blending: bool,
    allow_mipmaps: bool,
    zoom_factor: f32,
}

impl Display for Reftest {
    fn fmt(&self, f: &mut Formatter) -> Result<(), Error> {
        let paths: Vec<String> = self.test.iter().map(|t| t.display().to_string()).collect();
        write!(
            f,
            "{} {} {}",
            paths.join(", "),
            self.op,
            self.reference.display()
        )
    }
}

struct ReftestImage {
    data: Vec<u8>,
    size: DeviceIntSize,
}
enum ReftestImageComparison {
    Equal,
    NotEqual {
        max_difference: usize,
        count_different: usize,
    },
}

impl ReftestImage {
    fn compare(&self, other: &ReftestImage) -> ReftestImageComparison {
        assert_eq!(self.size, other.size);
        assert_eq!(self.data.len(), other.data.len());
        assert_eq!(self.data.len() % 4, 0);

        let mut count = 0;
        let mut max = 0;

        for (a, b) in self.data.chunks(4).zip(other.data.chunks(4)) {
            if a != b {
                let pixel_max = a.iter()
                    .zip(b.iter())
                    .map(|(x, y)| (*x as isize - *y as isize).abs() as usize)
                    .max()
                    .unwrap();

                count += 1;
                max = cmp::max(max, pixel_max);
            }
        }

        if count != 0 {
            ReftestImageComparison::NotEqual {
                max_difference: max,
                count_different: count,
            }
        } else {
            ReftestImageComparison::Equal
        }
    }

    fn create_data_uri(mut self) -> String {
        let width = self.size.width;
        let height = self.size.height;

        // flip image vertically (texture is upside down)
        let orig_pixels = self.data.clone();
        let stride = width as usize * 4;
        for y in 0 .. height as usize {
            let dst_start = y * stride;
            let src_start = (height as usize - y - 1) * stride;
            let src_slice = &orig_pixels[src_start .. src_start + stride];
            (&mut self.data[dst_start .. dst_start + stride])
                .clone_from_slice(&src_slice[.. stride]);
        }

        let mut png: Vec<u8> = vec![];
        {
            let encoder = PNGEncoder::new(&mut png);
            encoder
                .encode(&self.data[..], width as u32, height as u32, ColorType::RGBA(8))
                .expect("Unable to encode PNG!");
        }
        let png_base64 = base64::encode(&png);
        format!("data:image/png;base64,{}", png_base64)
    }
}

struct ReftestManifest {
    reftests: Vec<Reftest>,
}
impl ReftestManifest {
    fn new(manifest: &Path, environment: &ReftestEnvironment, options: &ReftestOptions) -> ReftestManifest {
        let dir = manifest.parent().unwrap();
        let f =
            File::open(manifest).expect(&format!("couldn't open manifest: {}", manifest.display()));
        let file = BufReader::new(&f);

        let mut reftests = Vec::new();

        for line in file.lines() {
            let l = line.unwrap();

            // strip the comments
            let s = &l[0 .. l.find('#').unwrap_or(l.len())];
            let s = s.trim();
            if s.is_empty() {
                continue;
            }

            let tokens: Vec<&str> = s.split_whitespace().collect();

            let mut max_difference = 0;
            let mut max_count = 0;
            let mut op = ReftestOp::Equal;
            let mut font_render_mode = None;
            let mut extra_checks = vec![];
            let mut disable_dual_source_blending = false;
            let mut zoom_factor = 1.0;
            let mut allow_mipmaps = false;
            let mut dirty_region_index = 0;

            let mut paths = vec![];
            for (i, token) in tokens.iter().enumerate() {
                match *token {
                    "include" => {
                        assert!(i == 0, "include must be by itself");
                        let include = dir.join(tokens[1]);

                        reftests.append(
                            &mut ReftestManifest::new(include.as_path(), environment, options).reftests,
                        );

                        break;
                    }
                    platform if platform.starts_with("skip_on") => {
                        // e.g. skip_on(android,debug) will skip only when
                        // running on a debug android build.
                        let (_, args, _) = parse_function(platform);
                        if args.iter().all(|arg| environment.has(arg)) {
                            break;
                        }
                    }
                    platform if platform.starts_with("platform") => {
                        let (_, args, _) = parse_function(platform);
                        if !args.iter().any(|arg| arg == &environment.platform) {
                            // Skip due to platform not matching
                            break;
                        }
                    }
                    function if function.starts_with("zoom") => {
                        let (_, args, _) = parse_function(function);
                        zoom_factor = args[0].parse().unwrap();
                    }
                    function if function.starts_with("fuzzy") => {
                        let (_, args, _) = parse_function(function);
                        max_difference = args[0].parse().unwrap();
                        max_count = args[1].parse().unwrap();
                    }
                    function if function.starts_with("draw_calls") => {
                        let (_, args, _) = parse_function(function);
                        extra_checks.push(ExtraCheck::DrawCalls(args[0].parse().unwrap()));
                    }
                    function if function.starts_with("alpha_targets") => {
                        let (_, args, _) = parse_function(function);
                        extra_checks.push(ExtraCheck::AlphaTargets(args[0].parse().unwrap()));
                    }
                    function if function.starts_with("color_targets") => {
                        let (_, args, _) = parse_function(function);
                        extra_checks.push(ExtraCheck::ColorTargets(args[0].parse().unwrap()));
                    }
                    function if function.starts_with("dirty") => {
                        let (_, args, _) = parse_function(function);
                        let region: String = args[0].parse().unwrap();
                        extra_checks.push(ExtraCheck::DirtyRegion {
                            index: dirty_region_index,
                            region,
                        });
                        dirty_region_index += 1;
                    }
                    options if options.starts_with("options") => {
                        let (_, args, _) = parse_function(options);
                        if args.iter().any(|arg| arg == &OPTION_DISABLE_SUBPX) {
                            font_render_mode = Some(FontRenderMode::Alpha);
                        }
                        if args.iter().any(|arg| arg == &OPTION_DISABLE_AA) {
                            font_render_mode = Some(FontRenderMode::Mono);
                        }
                        if args.iter().any(|arg| arg == &OPTION_DISABLE_DUAL_SOURCE_BLENDING) {
                            disable_dual_source_blending = true;
                        }
                        if args.iter().any(|arg| arg == &OPTION_ALLOW_MIPMAPS) {
                            allow_mipmaps = true;
                        }
                    }
                    "==" => {
                        op = ReftestOp::Equal;
                    }
                    "!=" => {
                        op = ReftestOp::NotEqual;
                    }
                    _ => {
                        paths.push(dir.join(*token));
                    }
                }
            }

            // Don't try to add tests for include lines.
            if paths.len() < 2 {
                assert_eq!(paths.len(), 0, "Only one path provided: {:?}", paths[0]);
                continue;
            }

            // The reference is the last path provided. If multiple paths are
            // passed for the test, they render sequentially before being
            // compared to the reference, which is useful for testing
            // invalidation.
            let reference = paths.pop().unwrap();
            let test = paths;

            reftests.push(Reftest {
                op,
                test,
                reference,
                font_render_mode,
                max_difference: cmp::max(max_difference, options.allow_max_difference),
                num_differences: cmp::max(max_count, options.allow_num_differences),
                extra_checks,
                disable_dual_source_blending,
                allow_mipmaps,
                zoom_factor,
            });
        }

        ReftestManifest { reftests: reftests }
    }

    fn find(&self, prefix: &Path) -> Vec<&Reftest> {
        self.reftests
            .iter()
            .filter(|x| {
                x.test.iter().any(|t| t.starts_with(prefix)) || x.reference.starts_with(prefix)
            })
            .collect()
    }
}

struct YamlRenderOutput {
    image: ReftestImage,
    results: RenderResults,
}

struct ReftestEnvironment {
    pub platform: &'static str,
    pub version: Option<semver::Version>,
    pub mode: &'static str,
}

impl ReftestEnvironment {
    fn new() -> Self {
        Self {
            platform: Self::platform(),
            version: Self::version(),
            mode: Self::mode(),
        }
    }

    fn has(&self, condition: &str) -> bool {
        if self.platform == condition || self.mode == condition {
            return true;
        }
        match (&self.version, &semver::VersionReq::parse(condition)) {
            (None, _) => false,
            (_, Err(_)) => false,
            (Some(v), Ok(r)) => r.matches(v),
        }
    }

    fn platform() -> &'static str {
        if cfg!(target_os = "windows") {
            "win"
        } else if cfg!(target_os = "linux") {
            "linux"
        } else if cfg!(target_os = "macos") {
            "mac"
        } else if cfg!(target_os = "android") {
            "android"
        } else {
            "other"
        }
    }

    fn version() -> Option<semver::Version> {
        if cfg!(target_os = "macos") {
            use std::str;
            let version_bytes = Command::new("defaults")
                .arg("read")
                .arg("loginwindow")
                .arg("SystemVersionStampAsString")
                .output()
                .expect("Failed to get macOS version")
                .stdout;
            let mut version_string = str::from_utf8(&version_bytes)
                .expect("Failed to read macOS version")
                .trim()
                .to_string();
            // On some machines this produces just the major.minor and on
            // some machines this gives major.minor.patch. But semver requires
            // the patch so we fake one if it's not there.
            if version_string.chars().filter(|c| *c == '.').count() == 1 {
                version_string.push_str(".0");
            }
            Some(semver::Version::parse(&version_string)
                 .expect(&format!("Failed to parse macOS version {}", version_string)))
        } else {
            None
        }
    }

    fn mode() -> &'static str {
        if cfg!(debug_assertions) {
            "debug"
        } else {
            "release"
        }
    }
}

pub struct ReftestHarness<'a> {
    wrench: &'a mut Wrench,
    window: &'a mut WindowWrapper,
    rx: &'a Receiver<NotifierEvent>,
    environment: ReftestEnvironment,
}
impl<'a> ReftestHarness<'a> {
    pub fn new(wrench: &'a mut Wrench, window: &'a mut WindowWrapper, rx: &'a Receiver<NotifierEvent>) -> Self {
        let environment = ReftestEnvironment::new();
        ReftestHarness { wrench, window, rx, environment }
    }

    pub fn run(mut self, base_manifest: &Path, reftests: Option<&Path>, options: &ReftestOptions) -> usize {
        let manifest = ReftestManifest::new(base_manifest, &self.environment, options);
        let reftests = manifest.find(reftests.unwrap_or(&PathBuf::new()));

        let mut total_passing = 0;
        let mut failing = Vec::new();

        for t in reftests {
            if self.run_reftest(t) {
                total_passing += 1;
            } else {
                failing.push(t);
            }
        }

        println!(
            "REFTEST INFO | {} passing, {} failing",
            total_passing,
            failing.len()
        );

        if !failing.is_empty() {
            println!("\nReftests with unexpected results:");

            for reftest in &failing {
                println!("\t{}", reftest);
            }
        }

        failing.len()
    }

    fn run_reftest(&mut self, t: &Reftest) -> bool {
        println!("REFTEST {}", t);

        self.wrench
            .api
            .send_debug_cmd(
                DebugCommand::ClearCaches(ClearCache::all())
            );

        self.wrench.set_page_zoom(ZoomFactor::new(t.zoom_factor));

        if t.disable_dual_source_blending {
            self.wrench
                .api
                .send_debug_cmd(
                    DebugCommand::EnableDualSourceBlending(false)
                );
        }

        let window_size = self.window.get_inner_size();
        let reference_image = match t.reference.extension().unwrap().to_str().unwrap() {
            "yaml" => None,
            "png" => Some(self.load_image(t.reference.as_path(), ImageFormat::PNG)),
            other => panic!("Unknown reftest extension: {}", other),
        };
        let test_size = reference_image.as_ref().map_or(window_size, |img| img.size);

        // The reference can be smaller than the window size, in which case
        // we only compare the intersection.
        //
        // Note also that, when we have multiple test scenes in sequence, we
        // want to test the picture caching machinery. But since picture caching
        // only takes effect after the result has been the same several frames in
        // a row, we need to render the scene multiple times.
        let mut images = vec![];
        let mut results = vec![];

        for filename in t.test.iter() {
            let output = self.render_yaml(
                &filename,
                test_size,
                t.font_render_mode,
                t.allow_mipmaps,
            );
            images.push(output.image);
            results.push(output.results);
        }

        let reference = match reference_image {
            Some(image) => image,
            None => {
                let output = self.render_yaml(
                    &t.reference,
                    test_size,
                    t.font_render_mode,
                    t.allow_mipmaps,
                );
                output.image
            }
        };

        if t.disable_dual_source_blending {
            self.wrench
                .api
                .send_debug_cmd(
                    DebugCommand::EnableDualSourceBlending(true)
                );
        }

        for extra_check in t.extra_checks.iter() {
            if !extra_check.run(&results) {
                println!(
                    "REFTEST TEST-UNEXPECTED-FAIL | {} | Failing Check: {:?} | Actual Results: {:?}",
                    t,
                    extra_check,
                    results,
                );
                println!("REFTEST TEST-END | {}", t);
                return false;
            }
        }

        let test = images.pop().unwrap();
        let comparison = test.compare(&reference);
        match (&t.op, comparison) {
            (&ReftestOp::Equal, ReftestImageComparison::Equal) => true,
            (
                &ReftestOp::Equal,
                ReftestImageComparison::NotEqual {
                    max_difference,
                    count_different,
                },
            ) => if max_difference > t.max_difference || count_different > t.num_differences {
                println!(
                    "{} | {} | {}: {}, {}: {}",
                    "REFTEST TEST-UNEXPECTED-FAIL",
                    t,
                    "image comparison, max difference",
                    max_difference,
                    "number of differing pixels",
                    count_different
                );
                println!("REFTEST   IMAGE 1 (TEST): {}", test.create_data_uri());
                println!(
                    "REFTEST   IMAGE 2 (REFERENCE): {}",
                    reference.create_data_uri()
                );
                println!("REFTEST TEST-END | {}", t);

                false
            } else {
                true
            },
            (&ReftestOp::NotEqual, ReftestImageComparison::Equal) => {
                println!("REFTEST TEST-UNEXPECTED-FAIL | {} | image comparison", t);
                println!("REFTEST TEST-END | {}", t);

                false
            }
            (&ReftestOp::NotEqual, ReftestImageComparison::NotEqual { .. }) => true,
        }
    }

    fn load_image(&mut self, filename: &Path, format: ImageFormat) -> ReftestImage {
        let file = BufReader::new(File::open(filename).unwrap());
        let img_raw = load_piston_image(file, format).unwrap();
        let img = img_raw.flipv().to_rgba();
        let size = img.dimensions();
        ReftestImage {
            data: img.into_raw(),
            size: DeviceIntSize::new(size.0 as i32, size.1 as i32),
        }
    }

    fn render_yaml(
        &mut self,
        filename: &Path,
        size: DeviceIntSize,
        font_render_mode: Option<FontRenderMode>,
        allow_mipmaps: bool,
    ) -> YamlRenderOutput {
        let mut reader = YamlFrameReader::new(filename);
        reader.set_font_render_mode(font_render_mode);
        reader.allow_mipmaps(allow_mipmaps);
        reader.do_frame(self.wrench);

        self.wrench.api.flush_scene_builder();

        // wait for the frame
        self.rx.recv().unwrap();
        let results = self.wrench.render();

        let window_size = self.window.get_inner_size();
        assert!(
            size.width <= window_size.width &&
            size.height <= window_size.height,
            format!("size={:?} ws={:?}", size, window_size)
        );

        // taking the bottom left sub-rectangle
        let rect = FramebufferIntRect::new(
            FramebufferIntPoint::new(0, window_size.height - size.height),
            FramebufferIntSize::new(size.width, size.height),
        );
        let pixels = self.wrench.renderer.read_pixels_rgba8(rect);
        self.window.swap_buffers();

        let write_debug_images = false;
        if write_debug_images {
            let debug_path = filename.with_extension("yaml.png");
            save_flipped(debug_path, pixels.clone(), size);
        }

        reader.deinit(self.wrench);

        YamlRenderOutput {
            image: ReftestImage { data: pixels, size },
            results,
        }
    }
}
