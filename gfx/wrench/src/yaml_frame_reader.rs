/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use clap;
use euclid::SideOffsets2D;
use image;
use image::GenericImage;
use parse_function::parse_function;
use premultiply::premultiply;
use std::collections::HashMap;
use std::fs::File;
use std::io::Read;
use std::path::{Path, PathBuf};
use webrender::api::*;
use wrench::{FontDescriptor, Wrench, WrenchThing};
use yaml_helper::{StringEnum, YamlHelper, make_perspective};
use yaml_rust::{Yaml, YamlLoader};
use {BLACK_COLOR, PLATFORM_DEFAULT_FACE_NAME, WHITE_COLOR};

fn rsrc_path(item: &Yaml, aux_dir: &PathBuf) -> PathBuf {
    let filename = item.as_str().unwrap();
    let mut file = aux_dir.clone();
    file.push(filename);
    file
}

impl FontDescriptor {
    fn from_yaml(item: &Yaml, aux_dir: &PathBuf) -> FontDescriptor {
        if !item["family"].is_badvalue() {
            FontDescriptor::Properties {
                family: item["family"].as_str().unwrap().to_owned(),
                weight: item["weight"].as_i64().unwrap_or(400) as u32,
                style: item["style"].as_i64().unwrap_or(0) as u32,
                stretch: item["stretch"].as_i64().unwrap_or(5) as u32,
            }
        } else if !item["font"].is_badvalue() {
            let path = rsrc_path(&item["font"], aux_dir);
            FontDescriptor::Path {
                path,
                font_index: item["font-index"].as_i64().unwrap_or(0) as u32,
            }
        } else {
            FontDescriptor::Family {
                name: PLATFORM_DEFAULT_FACE_NAME.clone(),
            }
        }
    }
}

fn broadcast<T: Clone>(base_vals: &[T], num_items: usize) -> Vec<T> {
    if base_vals.len() == num_items {
        return base_vals.to_vec();
    }

    assert_eq!(
        num_items % base_vals.len(),
        0,
        "Cannot broadcast {} elements into {}",
        base_vals.len(),
        num_items
    );

    let mut vals = vec![];
    loop {
        if vals.len() == num_items {
            break;
        }
        vals.extend_from_slice(base_vals);
    }
    vals
}

fn generate_checkerboard_image(
    border: u32,
    tile_x_size: u32,
    tile_y_size: u32,
    tile_x_count: u32,
    tile_y_count: u32,
) -> (ImageDescriptor, ImageData) {
    let width = 2 * border + tile_x_size * tile_x_count;
    let height = 2 * border + tile_y_size * tile_y_count;
    let mut pixels = Vec::new();

    for y in 0 .. height {
        for x in 0 .. width {
            if y < border || y >= (height - border) ||
               x < border || x >= (width - border) {
                pixels.push(0);
                pixels.push(0);
                pixels.push(0xff);
                pixels.push(0xff);
            } else {
                let xon = ((x - border) % (2 * tile_x_size)) < tile_x_size;
                let yon = ((y - border) % (2 * tile_y_size)) < tile_y_size;
                let value = if xon ^ yon { 0xff } else { 0x7f };
                pixels.push(value);
                pixels.push(value);
                pixels.push(value);
                pixels.push(0xff);
            }
        }
    }

    (
        ImageDescriptor::new(width, height, ImageFormat::BGRA8, true, false),
        ImageData::new(pixels),
    )
}

fn generate_xy_gradient_image(w: u32, h: u32) -> (ImageDescriptor, ImageData) {
    let mut pixels = Vec::with_capacity((w * h * 4) as usize);
    for y in 0 .. h {
        for x in 0 .. w {
            let grid = if x % 100 < 3 || y % 100 < 3 { 0.9 } else { 1.0 };
            pixels.push((y as f32 / h as f32 * 255.0 * grid) as u8);
            pixels.push(0);
            pixels.push((x as f32 / w as f32 * 255.0 * grid) as u8);
            pixels.push(255);
        }
    }

    (
        ImageDescriptor::new(w, h, ImageFormat::BGRA8, true, false),
        ImageData::new(pixels),
    )
}

fn generate_solid_color_image(
    r: u8,
    g: u8,
    b: u8,
    a: u8,
    w: u32,
    h: u32,
) -> (ImageDescriptor, ImageData) {
    let buf_size = (w * h * 4) as usize;
    let mut pixels = Vec::with_capacity(buf_size);
    // Unsafely filling the buffer is horrible. Unfortunately doing this idiomatically
    // is terribly slow in debug builds to the point that reftests/image/very-big.yaml
    // takes more than 20 seconds to run on a recent laptop.
    unsafe {
        pixels.set_len(buf_size);
        let color: u32 = ::std::mem::transmute([b, g, r, a]);
        let mut ptr: *mut u32 = ::std::mem::transmute(&mut pixels[0]);
        let end = ptr.offset((w * h) as isize);
        while ptr < end {
            *ptr = color;
            ptr = ptr.offset(1);
        }
    }

    (
        ImageDescriptor::new(w, h, ImageFormat::BGRA8, a == 255, false),
        ImageData::new(pixels),
    )
}



fn is_image_opaque(format: ImageFormat, bytes: &[u8]) -> bool {
    match format {
        ImageFormat::BGRA8 => {
            let mut is_opaque = true;
            for i in 0 .. (bytes.len() / 4) {
                if bytes[i * 4 + 3] != 255 {
                    is_opaque = false;
                    break;
                }
            }
            is_opaque
        }
        ImageFormat::RG8 => true,
        ImageFormat::R8 => false,
        ImageFormat::RGBAF32 => unreachable!(),
    }
}

pub struct YamlFrameReader {
    frame_built: bool,
    yaml_path: PathBuf,
    aux_dir: PathBuf,
    frame_count: u32,

    display_lists: Vec<(PipelineId, LayoutSize, BuiltDisplayList)>,
    queue_depth: u32,

    include_only: Vec<String>,

    watch_source: bool,
    list_resources: bool,

    /// A HashMap of offsets which specify what scroll offsets particular
    /// scroll layers should be initialized with.
    scroll_offsets: HashMap<ExternalScrollId, LayoutPoint>,

    image_map: HashMap<(PathBuf, Option<i64>), (ImageKey, LayoutSize)>,

    fonts: HashMap<FontDescriptor, FontKey>,
    font_instances: HashMap<(FontKey, Au, FontInstanceFlags, Option<ColorU>), FontInstanceKey>,
    font_render_mode: Option<FontRenderMode>,
    allow_mipmaps: bool,

    /// A HashMap that allows specifying a numeric id for clip and clip chains in YAML
    /// and having each of those ids correspond to a unique ClipId.
    clip_id_map: HashMap<u64, ClipId>,
}

impl YamlFrameReader {
    pub fn new(yaml_path: &Path) -> YamlFrameReader {
        YamlFrameReader {
            watch_source: false,
            list_resources: false,
            frame_built: false,
            yaml_path: yaml_path.to_owned(),
            aux_dir: yaml_path.parent().unwrap().to_owned(),
            frame_count: 0,
            display_lists: Vec::new(),
            queue_depth: 1,
            include_only: vec![],
            scroll_offsets: HashMap::new(),
            fonts: HashMap::new(),
            font_instances: HashMap::new(),
            font_render_mode: None,
            image_map: HashMap::new(),
            clip_id_map: HashMap::new(),
            allow_mipmaps: false,
        }
    }

    pub fn deinit(mut self, wrench: &mut Wrench) {
        let mut txn = Transaction::new();

        for (_, font_instance) in self.font_instances.drain() {
            txn.delete_font_instance(font_instance);
        }

        for (_, font) in self.fonts.drain() {
            txn.delete_font(font);
        }

        wrench.api.update_resources(txn.resource_updates);
    }

    pub fn yaml_path(&self) -> &PathBuf {
        &self.yaml_path
    }

    pub fn new_from_args(args: &clap::ArgMatches) -> YamlFrameReader {
        let yaml_file = args.value_of("INPUT").map(|s| PathBuf::from(s)).unwrap();

        let mut y = YamlFrameReader::new(&yaml_file);
        y.list_resources = args.is_present("list-resources");
        y.watch_source = args.is_present("watch");
        y.queue_depth = args.value_of("queue")
            .map(|s| s.parse::<u32>().unwrap())
            .unwrap_or(1);
        y.include_only = args.values_of("include")
            .map(|v| v.map(|s| s.to_owned()).collect())
            .unwrap_or(vec![]);
        y
    }

    pub fn reset(&mut self) {
        self.scroll_offsets.clear();
        self.display_lists.clear();
    }

    pub fn build(&mut self, wrench: &mut Wrench) {
        let mut file = File::open(&self.yaml_path).unwrap();
        let mut src = String::new();
        file.read_to_string(&mut src).unwrap();

        let mut yaml_doc = YamlLoader::load_from_str(&src).expect("Failed to parse YAML file");
        assert_eq!(yaml_doc.len(), 1);

        self.reset();

        let yaml = yaml_doc.pop().unwrap();
        if let Some(pipelines) = yaml["pipelines"].as_vec() {
            for pipeline in pipelines {
                self.build_pipeline(wrench, pipeline["id"].as_pipeline_id().unwrap(), pipeline);
            }
        }

        assert!(!yaml["root"].is_badvalue(), "Missing root stacking context");
        let root_pipeline_id = wrench.root_pipeline_id;
        self.build_pipeline(wrench, root_pipeline_id, &yaml["root"]);
    }

    pub fn build_pipeline(
        &mut self,
        wrench: &mut Wrench,
        pipeline_id: PipelineId,
        yaml: &Yaml
    ) {
        // Don't allow referencing clips between pipelines for now.
        self.clip_id_map.clear();

        let content_size = self.get_root_size_from_yaml(wrench, yaml);
        let mut builder = DisplayListBuilder::new(pipeline_id, content_size);
        let mut info = LayoutPrimitiveInfo::new(LayoutRect::zero());
        self.add_stacking_context_from_yaml(&mut builder, wrench, yaml, true, &mut info);
        self.display_lists.push(builder.finalize());
    }

    fn to_complex_clip_region(&mut self, item: &Yaml) -> ComplexClipRegion {
        let rect = item["rect"]
            .as_rect()
            .expect("Complex clip entry must have rect");
        let radius = item["radius"]
            .as_border_radius()
            .unwrap_or(BorderRadius::zero());
        let mode = item["clip-mode"]
            .as_clip_mode()
            .unwrap_or(ClipMode::Clip);
        ComplexClipRegion::new(rect, radius, mode)
    }

    fn to_complex_clip_regions(&mut self, item: &Yaml) -> Vec<ComplexClipRegion> {
        match *item {
            Yaml::Array(ref array) => array
                .iter()
                .map(|entry| self.to_complex_clip_region(entry))
                .collect(),
            Yaml::BadValue => vec![],
            _ => {
                println!("Unable to parse complex clip region {:?}", item);
                vec![]
            }
        }
    }

    fn to_sticky_offset_bounds(&mut self, item: &Yaml) -> StickyOffsetBounds {
        match *item {
            Yaml::Array(ref array) => StickyOffsetBounds::new(
                array[0].as_f32().unwrap_or(0.0),
                array[1].as_f32().unwrap_or(0.0),
            ),
            _ => StickyOffsetBounds::new(0.0, 0.0),
        }
    }

    pub fn u64_to_clip_id(&self, number: u64, pipeline_id: PipelineId) -> ClipId {
        match number {
            0 => ClipId::root_reference_frame(pipeline_id),
            1 => ClipId::root_scroll_node(pipeline_id),
            _ => self.clip_id_map[&number],
        }

    }

    pub fn to_clip_id(&self, item: &Yaml, pipeline_id: PipelineId) -> Option<ClipId> {
        match *item {
            Yaml::Integer(value) => Some(self.u64_to_clip_id(value as u64, pipeline_id)),
            Yaml::String(ref id_string) if id_string == "root-reference-frame" =>
                Some(ClipId::root_reference_frame(pipeline_id)),
            Yaml::String(ref id_string) if id_string == "root-scroll-node" =>
                Some(ClipId::root_scroll_node(pipeline_id)),
            _ => None,
        }
    }

    pub fn add_clip_id_mapping(&mut self, numeric_id: u64, real_id: ClipId) {
        assert!(numeric_id != 0, "id=0 is reserved for the root reference frame");
        assert!(numeric_id != 1, "id=1 is reserved for the root scroll node");
        self.clip_id_map.insert(numeric_id, real_id);
    }

    fn to_clip_and_scroll_info(
        &self,
        item: &Yaml,
        pipeline_id: PipelineId
    ) -> Option<ClipAndScrollInfo> {
        match *item {
            Yaml::Array(ref array) if array.len() == 2 => {
                let scroll_id = match self.to_clip_id(&array[0], pipeline_id) {
                    Some(id) => id,
                    None => return None,
                };
                let clip_id = match self.to_clip_id(&array[1], pipeline_id) {
                    Some(id) => id,
                    None => return None,
                };
                Some(ClipAndScrollInfo::new(scroll_id, clip_id))
            }
            _ => self.to_clip_id(item, pipeline_id).map(|id| ClipAndScrollInfo::simple(id)),
        }
    }

    fn to_hit_testing_tag(&self, item: &Yaml) -> Option<ItemTag> {
        match *item {
            Yaml::Array(ref array) if array.len() == 2 => {
                match (array[0].as_i64(), array[1].as_i64()) {
                    (Some(first), Some(second)) => Some((first as u64, second as u16)),
                    _ => None,
                }
            }
            _ => None,
        }

    }

    pub fn add_or_get_image(
            &mut self,
            file: &Path,
            tiling: Option<i64>,
            wrench: &mut Wrench,
    ) -> (ImageKey, LayoutSize) {
        let key = (file.to_owned(), tiling);
        if let Some(k) = self.image_map.get(&key) {
            return *k;
        }

        if self.list_resources { println!("{}", file.to_string_lossy()); }
        let (descriptor, image_data) = match image::open(file) {
            Ok(image) => {
                let (image_width, image_height) = image.dimensions();
                let (format, bytes) = match image {
                    image::ImageLuma8(_) => {
                        (ImageFormat::R8, image.raw_pixels())
                    }
                    image::ImageRgba8(_) => {
                        let mut pixels = image.raw_pixels();
                        premultiply(pixels.as_mut_slice());
                        (ImageFormat::BGRA8, pixels)
                    }
                    image::ImageRgb8(_) => {
                        let bytes = image.raw_pixels();
                        let mut pixels = Vec::with_capacity(image_width as usize * image_height as usize * 4);
                        for bgr in bytes.chunks(3) {
                            pixels.extend_from_slice(&[
                                bgr[2],
                                bgr[1],
                                bgr[0],
                                0xff
                            ]);
                        }
                        (ImageFormat::BGRA8, pixels)
                    }
                    _ => panic!("We don't support whatever your crazy image type is, come on"),
                };
                let descriptor = ImageDescriptor::new(
                    image_width,
                    image_height,
                    format,
                    is_image_opaque(format, &bytes[..]),
                    self.allow_mipmaps,
                );
                let data = ImageData::new(bytes);
                (descriptor, data)
            }
            _ => {
                // This is a hack but it is convenient when generating test cases and avoids
                // bloating the repository.
                match parse_function(
                    file.components()
                        .last()
                        .unwrap()
                        .as_os_str()
                        .to_str()
                        .unwrap(),
                ) {
                    ("xy-gradient", args, _) => generate_xy_gradient_image(
                        args.get(0).unwrap_or(&"1000").parse::<u32>().unwrap(),
                        args.get(1).unwrap_or(&"1000").parse::<u32>().unwrap(),
                    ),
                    ("solid-color", args, _) => generate_solid_color_image(
                        args.get(0).unwrap_or(&"255").parse::<u8>().unwrap(),
                        args.get(1).unwrap_or(&"255").parse::<u8>().unwrap(),
                        args.get(2).unwrap_or(&"255").parse::<u8>().unwrap(),
                        args.get(3).unwrap_or(&"255").parse::<u8>().unwrap(),
                        args.get(4).unwrap_or(&"1000").parse::<u32>().unwrap(),
                        args.get(5).unwrap_or(&"1000").parse::<u32>().unwrap(),
                    ),
                    ("checkerboard", args, _) => {
                        let border = args.get(0).unwrap_or(&"4").parse::<u32>().unwrap();

                        let (x_size, y_size, x_count, y_count) = match args.len() {
                            3 => {
                                let size = args.get(1).unwrap_or(&"32").parse::<u32>().unwrap();
                                let count = args.get(2).unwrap_or(&"8").parse::<u32>().unwrap();
                                (size, size, count, count)
                            }
                            5 => {
                                let x_size = args.get(1).unwrap_or(&"32").parse::<u32>().unwrap();
                                let y_size = args.get(2).unwrap_or(&"32").parse::<u32>().unwrap();
                                let x_count = args.get(3).unwrap_or(&"8").parse::<u32>().unwrap();
                                let y_count = args.get(4).unwrap_or(&"8").parse::<u32>().unwrap();
                                (x_size, y_size, x_count, y_count)
                            }
                            _ => {
                                panic!("invalid checkerboard function");
                            }
                        };

                        generate_checkerboard_image(
                            border,
                            x_size,
                            y_size,
                            x_count,
                            y_count,
                        )
                    }
                    _ => {
                        panic!("Failed to load image {:?}", file.to_str());
                    }
                }
            }
        };
        let tiling = tiling.map(|tile_size| tile_size as u16);
        let image_key = wrench.api.generate_image_key();
        let mut txn = Transaction::new();
        txn.add_image(image_key, descriptor, image_data, tiling);
        wrench.api.update_resources(txn.resource_updates);
        let val = (
            image_key,
            LayoutSize::new(descriptor.size.width as f32, descriptor.size.height as f32),
        );
        self.image_map.insert(key, val);
        val
    }

    fn get_or_create_font(&mut self, desc: FontDescriptor, wrench: &mut Wrench) -> FontKey {
        let list_resources = self.list_resources;
        *self.fonts
            .entry(desc.clone())
            .or_insert_with(|| match desc {
                FontDescriptor::Path {
                    ref path,
                    font_index,
                } => {
                    if list_resources { println!("{}", path.to_string_lossy()); }
                    let mut file = File::open(path).expect("Couldn't open font file");
                    let mut bytes = vec![];
                    file.read_to_end(&mut bytes)
                        .expect("failed to read font file");
                    wrench.font_key_from_bytes(bytes, font_index)
                }
                FontDescriptor::Family { ref name } => wrench.font_key_from_name(name),
                FontDescriptor::Properties {
                    ref family,
                    weight,
                    style,
                    stretch,
                } => wrench.font_key_from_properties(family, weight, style, stretch),
            })
    }

    pub fn allow_mipmaps(&mut self, allow_mipmaps: bool) {
        self.allow_mipmaps = allow_mipmaps;
    }

    pub fn set_font_render_mode(&mut self, render_mode: Option<FontRenderMode>) {
        self.font_render_mode = render_mode;
    }

    fn get_or_create_font_instance(
        &mut self,
        font_key: FontKey,
        size: Au,
        bg_color: Option<ColorU>,
        flags: FontInstanceFlags,
        wrench: &mut Wrench,
    ) -> FontInstanceKey {
        let font_render_mode = self.font_render_mode;

        *self.font_instances
            .entry((font_key, size, flags, bg_color))
            .or_insert_with(|| {
                wrench.add_font_instance(
                    font_key,
                    size,
                    flags,
                    font_render_mode,
                    bg_color,
                )
            })
    }

    fn to_image_mask(&mut self, item: &Yaml, wrench: &mut Wrench) -> Option<ImageMask> {
        if item.as_hash().is_none() {
            return None;
        }

        let file = match item["image"].as_str() {
            Some(filename) => {
                let mut file = self.aux_dir.clone();
                file.push(filename);
                file
            }
            None => {
                warn!("No image provided for the image-mask!");
                return None;
            }
        };

        let (image_key, image_dims) =
            self.add_or_get_image(&file, None, wrench);
        let image_rect = item["rect"]
            .as_rect()
            .unwrap_or(LayoutRect::new(LayoutPoint::zero(), image_dims));
        let image_repeat = item["repeat"].as_bool().expect("Expected boolean");
        Some(ImageMask {
            image: image_key,
            rect: image_rect,
            repeat: image_repeat,
        })
    }

    fn to_gradient(&mut self, dl: &mut DisplayListBuilder, item: &Yaml) -> Gradient {
        let start = item["start"].as_point().expect("gradient must have start");
        let end = item["end"].as_point().expect("gradient must have end");
        let stops = item["stops"]
            .as_vec()
            .expect("gradient must have stops")
            .chunks(2)
            .map(|chunk| {
                GradientStop {
                    offset: chunk[0]
                        .as_force_f32()
                        .expect("gradient stop offset is not f32"),
                    color: chunk[1]
                        .as_colorf()
                        .expect("gradient stop color is not color"),
                }
            })
            .collect::<Vec<_>>();
        let extend_mode = if item["repeat"].as_bool().unwrap_or(false) {
            ExtendMode::Repeat
        } else {
            ExtendMode::Clamp
        };

        dl.create_gradient(start, end, stops, extend_mode)
    }

    fn to_radial_gradient(&mut self, dl: &mut DisplayListBuilder, item: &Yaml) -> RadialGradient {
        let center = item["center"].as_point().expect("radial gradient must have center");
        let radius = item["radius"].as_size().expect("radial gradient must have a radius");
        let stops = item["stops"]
            .as_vec()
            .expect("radial gradient must have stops")
            .chunks(2)
            .map(|chunk| {
                GradientStop {
                    offset: chunk[0]
                        .as_force_f32()
                        .expect("gradient stop offset is not f32"),
                    color: chunk[1]
                        .as_colorf()
                        .expect("gradient stop color is not color"),
                }
            })
            .collect::<Vec<_>>();
        let extend_mode = if item["repeat"].as_bool().unwrap_or(false) {
            ExtendMode::Repeat
        } else {
            ExtendMode::Clamp
        };

        dl.create_radial_gradient(center, radius, stops, extend_mode)
    }

    fn handle_rect(
        &mut self,
        dl: &mut DisplayListBuilder,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let bounds_key = if item["type"].is_badvalue() {
            "rect"
        } else {
            "bounds"
        };
        info.rect = item[bounds_key]
            .as_rect()
            .expect("rect type must have bounds");
        let color = item["color"].as_colorf().unwrap_or(*WHITE_COLOR);
        dl.push_rect(&info, color);
    }

    fn handle_clear_rect(
        &mut self,
        dl: &mut DisplayListBuilder,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        info.rect = item["bounds"]
            .as_rect()
            .expect("clear-rect type must have bounds");
        dl.push_clear_rect(&info);
    }

    fn handle_line(
        &mut self,
        dl: &mut DisplayListBuilder,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let color = item["color"].as_colorf().unwrap_or(*BLACK_COLOR);
        let orientation = item["orientation"]
            .as_str()
            .and_then(LineOrientation::from_str)
            .expect("line must have orientation");
        let style = item["style"]
            .as_str()
            .and_then(LineStyle::from_str)
            .expect("line must have style");

        let wavy_line_thickness = if let LineStyle::Wavy = style {
            item["thickness"].as_f32().expect("wavy lines must have a thickness")
        } else {
            0.0
        };

        if item["baseline"].is_badvalue() {
            let bounds_key = if item["type"].is_badvalue() {
                "rect"
            } else {
                "bounds"
            };

            info.rect = item[bounds_key]
                .as_rect()
                .expect("line type must have bounds");
        } else {
            // Legacy line representation
            let baseline = item["baseline"].as_f32().expect("line must have baseline");
            let start = item["start"].as_f32().expect("line must have start");
            let end = item["end"].as_f32().expect("line must have end");
            let width = item["width"].as_f32().expect("line must have width");

            info.rect = match orientation {
                LineOrientation::Horizontal => {
                    LayoutRect::new(LayoutPoint::new(start, baseline),
                                    LayoutSize::new(end - start, width))
                }
                LineOrientation::Vertical => {
                    LayoutRect::new(LayoutPoint::new(baseline, start),
                                    LayoutSize::new(width, end - start))
                }
            };
        }

        dl.push_line(
            &info,
            wavy_line_thickness,
            orientation,
            &color,
            style,
        );
    }

    fn handle_gradient(
        &mut self,
        dl: &mut DisplayListBuilder,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let bounds_key = if item["type"].is_badvalue() {
            "gradient"
        } else {
            "bounds"
        };
        let bounds = item[bounds_key]
            .as_rect()
            .expect("gradient must have bounds");
        info.rect = bounds;
        let gradient = self.to_gradient(dl, item);
        let tile_size = item["tile-size"].as_size().unwrap_or(bounds.size);
        let tile_spacing = item["tile-spacing"].as_size().unwrap_or(LayoutSize::zero());

        dl.push_gradient(&info, gradient, tile_size, tile_spacing);
    }

    fn handle_radial_gradient(
        &mut self,
        dl: &mut DisplayListBuilder,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let bounds_key = if item["type"].is_badvalue() {
            "radial-gradient"
        } else {
            "bounds"
        };
        let bounds = item[bounds_key]
            .as_rect()
            .expect("radial gradient must have bounds");
        info.rect = bounds;
        let gradient = self.to_radial_gradient(dl, item);
        let tile_size = item["tile-size"].as_size().unwrap_or(bounds.size);
        let tile_spacing = item["tile-spacing"].as_size().unwrap_or(LayoutSize::zero());

        dl.push_radial_gradient(&info, gradient, tile_size, tile_spacing);
    }

    fn handle_border(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let bounds_key = if item["type"].is_badvalue() {
            "border"
        } else {
            "bounds"
        };
        info.rect = item[bounds_key]
            .as_rect()
            .expect("borders must have bounds");
        let widths = item["width"]
            .as_vec_f32()
            .expect("borders must have width(s)");
        let widths = broadcast(&widths, 4);
        let widths = BorderWidths {
            top: widths[0],
            left: widths[1],
            bottom: widths[2],
            right: widths[3],
        };
        let border_details = if let Some(border_type) = item["border-type"].as_str() {
            match border_type {
                "normal" => {
                    let colors = item["color"]
                        .as_vec_colorf()
                        .expect("borders must have color(s)");
                    let styles = item["style"]
                        .as_vec_string()
                        .expect("borders must have style(s)");
                    let styles = styles
                        .iter()
                        .map(|s| match s.as_str() {
                            "none" => BorderStyle::None,
                            "solid" => BorderStyle::Solid,
                            "double" => BorderStyle::Double,
                            "dotted" => BorderStyle::Dotted,
                            "dashed" => BorderStyle::Dashed,
                            "hidden" => BorderStyle::Hidden,
                            "ridge" => BorderStyle::Ridge,
                            "inset" => BorderStyle::Inset,
                            "outset" => BorderStyle::Outset,
                            "groove" => BorderStyle::Groove,
                            s => {
                                panic!("Unknown border style '{}'", s);
                            }
                        })
                        .collect::<Vec<BorderStyle>>();
                    let radius = item["radius"]
                        .as_border_radius()
                        .unwrap_or(BorderRadius::zero());

                    let colors = broadcast(&colors, 4);
                    let styles = broadcast(&styles, 4);

                    let top = BorderSide {
                        color: colors[0],
                        style: styles[0],
                    };
                    let right = BorderSide {
                        color: colors[1],
                        style: styles[1],
                    };
                    let bottom = BorderSide {
                        color: colors[2],
                        style: styles[2],
                    };
                    let left = BorderSide {
                        color: colors[3],
                        style: styles[3],
                    };
                    Some(BorderDetails::Normal(NormalBorder {
                        top,
                        left,
                        bottom,
                        right,
                        radius,
                    }))
                }
                "image" => {
                    let file = rsrc_path(&item["image-source"], &self.aux_dir);
                    let (image_key, _) = self
                        .add_or_get_image(&file, None, wrench);
                    let image_width = item["image-width"]
                        .as_i64()
                        .expect("border must have image-width");
                    let image_height = item["image-height"]
                        .as_i64()
                        .expect("border must have image-height");
                    let fill = item["fill"].as_bool().unwrap_or(false);
                    let slice = item["slice"].as_vec_u32().expect("border must have slice");
                    let slice = broadcast(&slice, 4);
                    let outset = item["outset"]
                        .as_vec_f32()
                        .expect("border must have outset");
                    let outset = broadcast(&outset, 4);
                    let repeat_horizontal = match item["repeat-horizontal"]
                        .as_str()
                        .expect("border must have repeat-horizontal")
                    {
                        "stretch" => RepeatMode::Stretch,
                        "repeat" => RepeatMode::Repeat,
                        "round" => RepeatMode::Round,
                        "space" => RepeatMode::Space,
                        s => panic!("Unknown box border image repeat mode {}", s),
                    };
                    let repeat_vertical = match item["repeat-vertical"]
                        .as_str()
                        .expect("border must have repeat-vertical")
                    {
                        "stretch" => RepeatMode::Stretch,
                        "repeat" => RepeatMode::Repeat,
                        "round" => RepeatMode::Round,
                        "space" => RepeatMode::Space,
                        s => panic!("Unknown box border image repeat mode {}", s),
                    };
                    Some(BorderDetails::NinePatch(NinePatchBorder {
                        source: NinePatchBorderSource::Image(image_key),
                        width: image_width as u32,
                        height: image_height as u32,
                        slice: SideOffsets2D::new(slice[0], slice[1], slice[2], slice[3]),
                        fill,
                        repeat_horizontal,
                        repeat_vertical,
                        outset: SideOffsets2D::new(outset[0], outset[1], outset[2], outset[3]),
                    }))
                }
                "gradient" => {
                    let gradient = self.to_gradient(dl, item);
                    let outset = item["outset"]
                        .as_vec_f32()
                        .expect("borders must have outset");
                    let outset = broadcast(&outset, 4);
                    Some(BorderDetails::Gradient(GradientBorder {
                        gradient,
                        outset: SideOffsets2D::new(outset[0], outset[1], outset[2], outset[3]),
                    }))
                }
                "radial-gradient" => {
                    let gradient = self.to_radial_gradient(dl, item);
                    let outset = item["outset"]
                        .as_vec_f32()
                        .expect("borders must have outset");
                    let outset = broadcast(&outset, 4);
                    Some(BorderDetails::RadialGradient(RadialGradientBorder {
                        gradient,
                        outset: SideOffsets2D::new(outset[0], outset[1], outset[2], outset[3]),
                    }))
                }
                _ => {
                    println!("Unable to parse border {:?}", item);
                    None
                }
            }
        } else {
            println!("Unable to parse border {:?}", item);
            None
        };
        if let Some(details) = border_details {
            dl.push_border(&info, widths, details);
        }
    }

    fn handle_box_shadow(
        &mut self,
        dl: &mut DisplayListBuilder,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let bounds_key = if item["type"].is_badvalue() {
            "box-shadow"
        } else {
            "bounds"
        };
        let bounds = item[bounds_key]
            .as_rect()
            .expect("box shadow must have bounds");
        info.rect = bounds;
        let box_bounds = item["box-bounds"].as_rect().unwrap_or(bounds);
        let offset = item["offset"].as_vector().unwrap_or(LayoutVector2D::zero());
        let color = item["color"]
            .as_colorf()
            .unwrap_or(ColorF::new(0.0, 0.0, 0.0, 1.0));
        let blur_radius = item["blur-radius"].as_force_f32().unwrap_or(0.0);
        let spread_radius = item["spread-radius"].as_force_f32().unwrap_or(0.0);
        let border_radius = item["border-radius"]
            .as_border_radius()
            .unwrap_or(BorderRadius::zero());
        let clip_mode = if let Some(mode) = item["clip-mode"].as_str() {
            match mode {
                "outset" => BoxShadowClipMode::Outset,
                "inset" => BoxShadowClipMode::Inset,
                s => panic!("Unknown box shadow clip mode {}", s),
            }
        } else {
            BoxShadowClipMode::Outset
        };

        dl.push_box_shadow(
            &info,
            box_bounds,
            offset,
            color,
            blur_radius,
            spread_radius,
            border_radius,
            clip_mode,
        );
    }

    fn handle_yuv_image(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        // TODO(gw): Support other YUV color spaces.
        let color_space = YuvColorSpace::Rec709;

        let yuv_data = match item["format"].as_str().expect("no format supplied") {
            "planar" => {
                let y_path = rsrc_path(&item["src-y"], &self.aux_dir);
                let (y_key, _) = self.add_or_get_image(&y_path, None, wrench);

                let u_path = rsrc_path(&item["src-u"], &self.aux_dir);
                let (u_key, _) = self.add_or_get_image(&u_path, None, wrench);

                let v_path = rsrc_path(&item["src-v"], &self.aux_dir);
                let (v_key, _) = self.add_or_get_image(&v_path, None, wrench);

                YuvData::PlanarYCbCr(y_key, u_key, v_key)
            }
            "nv12" => {
                let y_path = rsrc_path(&item["src-y"], &self.aux_dir);
                let (y_key, _) = self.add_or_get_image(&y_path, None, wrench);

                let uv_path = rsrc_path(&item["src-uv"], &self.aux_dir);
                let (uv_key, _) = self.add_or_get_image(&uv_path, None, wrench);

                YuvData::NV12(y_key, uv_key)
            }
            "interleaved" => {
                let yuv_path = rsrc_path(&item["src"], &self.aux_dir);
                let (yuv_key, _) = self.add_or_get_image(&yuv_path, None, wrench);

                YuvData::InterleavedYCbCr(yuv_key)
            }
            _ => {
                panic!("unexpected yuv format");
            }
        };

        let bounds = item["bounds"].as_vec_f32().unwrap();
        info.rect = LayoutRect::new(
            LayoutPoint::new(bounds[0], bounds[1]),
            LayoutSize::new(bounds[2], bounds[3]),
        );

        dl.push_yuv_image(
            &info,
            yuv_data,
            color_space,
            ImageRendering::Auto,
        );
    }

    fn handle_image(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let filename = &item[if item["type"].is_badvalue() {
                                 "image"
                             } else {
                                 "src"
                             }];
        let tiling = item["tile-size"].as_i64();
        let file = rsrc_path(filename, &self.aux_dir);
        let (image_key, image_dims) =
            self.add_or_get_image(&file, tiling, wrench);

        let bounds_raws = item["bounds"].as_vec_f32().unwrap();
        info.rect = if bounds_raws.len() == 2 {
            LayoutRect::new(LayoutPoint::new(bounds_raws[0], bounds_raws[1]), image_dims)
        } else if bounds_raws.len() == 4 {
            LayoutRect::new(
                LayoutPoint::new(bounds_raws[0], bounds_raws[1]),
                LayoutSize::new(bounds_raws[2], bounds_raws[3]),
            )
        } else {
            panic!(
                "image expected 2 or 4 values in bounds, got '{:?}'",
                item["bounds"]
            );
        };

        let stretch_size = item["stretch-size"].as_size().unwrap_or(image_dims);
        let tile_spacing = item["tile-spacing"]
            .as_size()
            .unwrap_or(LayoutSize::new(0.0, 0.0));
        let rendering = match item["rendering"].as_str() {
            Some("auto") | None => ImageRendering::Auto,
            Some("crisp-edges") => ImageRendering::CrispEdges,
            Some("pixelated") => ImageRendering::Pixelated,
            Some(_) => panic!(
                "ImageRendering can be auto, crisp-edges, or pixelated -- got {:?}",
                item
            ),
        };
        let alpha_type = match item["alpha-type"].as_str() {
            Some("premultiplied-alpha") | None => AlphaType::PremultipliedAlpha,
            Some("alpha") => AlphaType::Alpha,
            Some(_) => panic!(
                "AlphaType can be premultiplied-alpha or alpha -- got {:?}",
                item
            ),
        };
        dl.push_image(&info, stretch_size, tile_spacing, rendering, alpha_type, image_key);
    }

    fn handle_text(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let size = item["size"].as_pt_to_au().unwrap_or(Au::from_f32_px(16.0));
        let color = item["color"].as_colorf().unwrap_or(*BLACK_COLOR);
        let bg_color = item["bg-color"].as_colorf().map(|c| c.into());

        let mut flags = FontInstanceFlags::empty();
        if item["synthetic-italics"].as_bool().unwrap_or(false) {
            flags |= FontInstanceFlags::SYNTHETIC_ITALICS;
        }
        if item["synthetic-bold"].as_bool().unwrap_or(false) {
            flags |= FontInstanceFlags::SYNTHETIC_BOLD;
        }
        if item["embedded-bitmaps"].as_bool().unwrap_or(false) {
            flags |= FontInstanceFlags::EMBEDDED_BITMAPS;
        }
        if item["transpose"].as_bool().unwrap_or(false) {
            flags |= FontInstanceFlags::TRANSPOSE;
        }
        if item["flip-x"].as_bool().unwrap_or(false) {
            flags |= FontInstanceFlags::FLIP_X;
        }
        if item["flip-y"].as_bool().unwrap_or(false) {
            flags |= FontInstanceFlags::FLIP_Y;
        }

        assert!(
            item["blur-radius"].is_badvalue(),
            "text no longer has a blur radius, use PushShadow and PopAllShadows"
        );

        let desc = FontDescriptor::from_yaml(item, &self.aux_dir);
        let font_key = self.get_or_create_font(desc, wrench);
        let font_instance_key = self.get_or_create_font_instance(font_key,
                                                                 size,
                                                                 bg_color,
                                                                 flags,
                                                                 wrench);

        assert!(
            !(item["glyphs"].is_badvalue() && item["text"].is_badvalue()),
            "text item had neither text nor glyphs!"
        );

        let (glyphs, rect) = if item["text"].is_badvalue() {
            // if glyphs are specified, then the glyph positions can have the
            // origin baked in.
            let origin = item["origin"]
                .as_point()
                .unwrap_or(LayoutPoint::new(0.0, 0.0));
            let glyph_indices = item["glyphs"].as_vec_u32().unwrap();
            let glyph_offsets = item["offsets"].as_vec_f32().unwrap();
            assert_eq!(glyph_offsets.len(), glyph_indices.len() * 2);

            let glyphs = glyph_indices
                .iter()
                .enumerate()
                .map(|k| {
                    GlyphInstance {
                        index: *k.1,
                        point: LayoutPoint::new(
                            origin.x + glyph_offsets[k.0 * 2],
                            origin.y + glyph_offsets[k.0 * 2 + 1],
                        ),
                    }
                })
                .collect::<Vec<_>>();
            // TODO(gw): We could optionally use the WR API to query glyph dimensions
            //           here and calculate the bounding region here if we want to.
            let rect = item["bounds"]
                .as_rect()
                .expect("Text items with glyphs require bounds [for now]");
            (glyphs, rect)
        } else {
            let text = item["text"].as_str().unwrap();
            let origin = item["origin"]
                .as_point()
                .expect("origin required for text without glyphs");
            let (glyph_indices, glyph_positions, bounds) = wrench.layout_simple_ascii(
                font_key,
                font_instance_key,
                text,
                size,
                origin,
                flags,
            );

            let glyphs = glyph_indices
                .iter()
                .zip(glyph_positions)
                .map(|arg| {
                    let gi = GlyphInstance {
                        index: *arg.0 as u32,
                        point: arg.1,
                    };
                    gi
                })
                .collect::<Vec<_>>();
            (glyphs, bounds)
        };
        info.rect = rect;

        dl.push_text(&info, &glyphs, font_instance_key, color, None);
    }

    fn handle_iframe(
        &mut self,
        dl: &mut DisplayListBuilder,
        item: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        info.rect = item["bounds"].as_rect().expect("iframe must have bounds");
        let pipeline_id = item["id"].as_pipeline_id().unwrap();
        let ignore = item["ignore_missing_pipeline"].as_bool().unwrap_or(true);
        dl.push_iframe(&info, pipeline_id, ignore);
    }

    pub fn get_complex_clip_for_item(&mut self, yaml: &Yaml) -> Option<ComplexClipRegion> {
        let complex_clip = &yaml["complex-clip"];
        if complex_clip.is_badvalue() {
            return None;
        }
        Some(self.to_complex_clip_region(complex_clip))
    }

    pub fn get_item_type_from_yaml(item: &Yaml) -> &str {
        let shorthands = [
            "rect",
            "image",
            "text",
            "glyphs",
            "box-shadow", // Note: box_shadow shorthand check has to come before border.
            "border",
            "gradient",
            "radial-gradient",
        ];

        for shorthand in shorthands.iter() {
            if !item[*shorthand].is_badvalue() {
                return shorthand;
            }
        }
        item["type"].as_str().unwrap_or("unknown")
    }

    pub fn add_display_list_items_from_yaml(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        yaml: &Yaml,
    ) {
        // A very large number (but safely far away from finite limits of f32)
        let big_number = 1.0e30;
        // A rect that should in practical terms serve as a no-op for clipping
        let full_clip = LayoutRect::new(
            LayoutPoint::new(-big_number / 2.0, -big_number / 2.0),
            LayoutSize::new(big_number, big_number));

        for item in yaml.as_vec().unwrap() {
            let item_type = Self::get_item_type_from_yaml(item);

            // We never skip stacking contexts and reference frames because
            // they are structural elements of the display list.
            if item_type != "stacking-context" &&
                item_type != "reference-frame" &&
                self.include_only.contains(&item_type.to_owned()) {
                continue;
            }

            let clip_scroll_info = self.to_clip_and_scroll_info(
                &item["clip-and-scroll"],
                dl.pipeline_id
            );
            if let Some(clip_scroll_info) = clip_scroll_info {
                dl.push_clip_and_scroll_info(clip_scroll_info);
            }

            let complex_clip = self.get_complex_clip_for_item(item);
            let clip_rect = item["clip-rect"].as_rect().unwrap_or(full_clip);

            let mut pushed_clip = false;
            if let Some(complex_clip) = complex_clip {
                match item_type {
                    "clip" | "clip-chain" | "scroll-frame" => {},
                    _ => {
                        let id = dl.define_clip(clip_rect, vec![complex_clip], None);
                        dl.push_clip_id(id);
                        pushed_clip = true;
                    }
                }
            }

            let mut info = LayoutPrimitiveInfo::with_clip_rect(LayoutRect::zero(), clip_rect);
            info.is_backface_visible = item["backface-visible"].as_bool().unwrap_or(true);;
            info.tag = self.to_hit_testing_tag(&item["hit-testing-tag"]);

            match item_type {
                "rect" => self.handle_rect(dl, item, &mut info),
                "clear-rect" => self.handle_clear_rect(dl, item, &mut info),
                "line" => self.handle_line(dl, item, &mut info),
                "image" => self.handle_image(dl, wrench, item, &mut info),
                "yuv-image" => self.handle_yuv_image(dl, wrench, item, &mut info),
                "text" | "glyphs" => self.handle_text(dl, wrench, item, &mut info),
                "scroll-frame" => self.handle_scroll_frame(dl, wrench, item),
                "sticky-frame" => self.handle_sticky_frame(dl, wrench, item),
                "clip" => self.handle_clip(dl, wrench, item),
                "clip-chain" => self.handle_clip_chain(dl, item),
                "border" => self.handle_border(dl, wrench, item, &mut info),
                "gradient" => self.handle_gradient(dl, item, &mut info),
                "radial-gradient" => self.handle_radial_gradient(dl, item, &mut info),
                "box-shadow" => self.handle_box_shadow(dl, item, &mut info),
                "iframe" => self.handle_iframe(dl, item, &mut info),
                "stacking-context" => {
                    self.add_stacking_context_from_yaml(dl, wrench, item, false, &mut info)
                }
                "reference-frame" => self.handle_reference_frame(dl, wrench, item, &mut info),
                "shadow" => self.handle_push_shadow(dl, item, &mut info),
                "pop-all-shadows" => self.handle_pop_all_shadows(dl),
                _ => println!("Skipping unknown item type: {:?}", item),
            }

            if pushed_clip {
                dl.pop_clip_id();

            }

            if clip_scroll_info.is_some() {
                dl.pop_clip_id();
            }

        }
    }

    pub fn handle_scroll_frame(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        yaml: &Yaml,
    ) {
        let clip_rect = yaml["bounds"]
            .as_rect()
            .expect("scroll frame must have a bounds");
        let content_size = yaml["content-size"].as_size().unwrap_or(clip_rect.size);
        let content_rect = LayoutRect::new(clip_rect.origin, content_size);

        let numeric_id = yaml["id"].as_i64().map(|id| id as u64);

        let complex_clips = self.to_complex_clip_regions(&yaml["complex"]);
        let image_mask = self.to_image_mask(&yaml["image-mask"], wrench);

        let external_id =  yaml["scroll-offset"].as_point().map(|size| {
            let id = ExternalScrollId((self.scroll_offsets.len() + 1) as u64, dl.pipeline_id);
            self.scroll_offsets.insert(id, LayoutPoint::new(size.x, size.y));
            id
        });

        let real_id = dl.define_scroll_frame(
            external_id,
            content_rect,
            clip_rect,
            complex_clips,
            image_mask,
            ScrollSensitivity::Script,
        );
        if let Some(numeric_id) = numeric_id {
            self.add_clip_id_mapping(numeric_id, real_id);
        }

        if !yaml["items"].is_badvalue() {
            dl.push_clip_id(real_id);
            self.add_display_list_items_from_yaml(dl, wrench, &yaml["items"]);
            dl.pop_clip_id();
        }
    }

    pub fn handle_sticky_frame(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        yaml: &Yaml,
    ) {
        let bounds = yaml["bounds"].as_rect().expect("sticky frame must have a bounds");
        let numeric_id = yaml["id"].as_i64().map(|id| id as u64);

        let real_id = dl.define_sticky_frame(
            bounds,
            SideOffsets2D::new(
                yaml["margin-top"].as_f32(),
                yaml["margin-right"].as_f32(),
                yaml["margin-bottom"].as_f32(),
                yaml["margin-left"].as_f32(),
            ),
            self.to_sticky_offset_bounds(&yaml["vertical-offset-bounds"]),
            self.to_sticky_offset_bounds(&yaml["horizontal-offset-bounds"]),
            yaml["previously-applied-offset"].as_vector().unwrap_or(LayoutVector2D::zero()),
        );

        if let Some(numeric_id) = numeric_id {
            self.add_clip_id_mapping(numeric_id, real_id);
        }

        if !yaml["items"].is_badvalue() {
            dl.push_clip_id(real_id);
            self.add_display_list_items_from_yaml(dl, wrench, &yaml["items"]);
            dl.pop_clip_id();
        }
    }

    pub fn handle_push_shadow(
        &mut self,
        dl: &mut DisplayListBuilder,
        yaml: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let blur_radius = yaml["blur-radius"].as_f32().unwrap_or(0.0);
        let offset = yaml["offset"].as_vector().unwrap_or(LayoutVector2D::zero());
        let color = yaml["color"].as_colorf().unwrap_or(*BLACK_COLOR);

        dl.push_shadow(
            &info,
            Shadow {
                blur_radius,
                offset,
                color,
            },
        );
    }

    pub fn handle_pop_all_shadows(&mut self, dl: &mut DisplayListBuilder) {
        dl.pop_all_shadows();
    }

    pub fn handle_clip_chain(&mut self, builder: &mut DisplayListBuilder, yaml: &Yaml) {
        let numeric_id = yaml["id"].as_i64().expect("clip chains must have an id");
        let clip_ids: Vec<ClipId> = yaml["clips"]
            .as_vec_u64()
            .unwrap_or_else(Vec::new)
            .iter().map(|id| self.u64_to_clip_id(*id, builder.pipeline_id))
            .collect();

        let parent = self.to_clip_id(&yaml["parent"], builder.pipeline_id);
        let parent = match parent {
            Some(ClipId::ClipChain(clip_chain_id)) => Some(clip_chain_id),
            Some(_) => panic!("Tried to create a ClipChain with a non-ClipChain parent"),
            None => None,
        };

        let real_id = builder.define_clip_chain(parent, clip_ids);
        self.add_clip_id_mapping(numeric_id as u64, ClipId::ClipChain(real_id));
    }

    pub fn handle_clip(&mut self, dl: &mut DisplayListBuilder, wrench: &mut Wrench, yaml: &Yaml) {
        let clip_rect = yaml["bounds"].as_rect().expect("clip must have a bounds");
        let numeric_id = yaml["id"].as_i64();
        let complex_clips = self.to_complex_clip_regions(&yaml["complex"]);
        let image_mask = self.to_image_mask(&yaml["image-mask"], wrench);

        let real_id = dl.define_clip(clip_rect, complex_clips, image_mask);
        if let Some(numeric_id) = numeric_id {
            self.add_clip_id_mapping(numeric_id as u64, real_id);
        }

        if !yaml["items"].is_badvalue() {
            dl.push_clip_id(real_id);
            self.add_display_list_items_from_yaml(dl, wrench, &yaml["items"]);
            dl.pop_clip_id();
        }
    }

    pub fn get_root_size_from_yaml(&mut self, wrench: &mut Wrench, yaml: &Yaml) -> LayoutSize {
        yaml["bounds"]
            .as_rect()
            .map(|rect| rect.size)
            .unwrap_or(wrench.window_size_f32())
    }

    pub fn push_reference_frame(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        yaml: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) -> ClipId {
        let default_bounds = LayoutRect::new(LayoutPoint::zero(), wrench.window_size_f32());
        let bounds = yaml["bounds"].as_rect().unwrap_or(default_bounds);
        let default_transform_origin = LayoutPoint::new(
            bounds.origin.x + bounds.size.width * 0.5,
            bounds.origin.y + bounds.size.height * 0.5,
        );

        info.rect = bounds;

        let transform_origin = yaml["transform-origin"]
            .as_point()
            .unwrap_or(default_transform_origin);

        let perspective_origin = yaml["perspective-origin"]
            .as_point()
            .unwrap_or(default_transform_origin);

        let transform = yaml["transform"]
            .as_transform(&transform_origin)
            .map(|transform| transform.into());

        let perspective = match yaml["perspective"].as_f32() {
            Some(value) if value != 0.0 => {
                Some(make_perspective(perspective_origin, value as f32))
            }
            Some(_) => None,
            _ => yaml["perspective"].as_matrix4d(),
        };

        let reference_frame_id = dl.push_reference_frame(info, transform.into(), perspective);

        let numeric_id = yaml["reference-frame-id"].as_i64();
        if let Some(numeric_id) = numeric_id {
            self.add_clip_id_mapping(numeric_id as u64, reference_frame_id);
        }

        reference_frame_id
    }

    pub fn handle_reference_frame(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        yaml: &Yaml,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let reference_frame_id = self.push_reference_frame(dl, wrench, yaml, info);

        if !yaml["items"].is_badvalue() {
            dl.push_clip_id(reference_frame_id);
            self.add_display_list_items_from_yaml(dl, wrench, &yaml["items"]);
            dl.pop_clip_id();
        }

        dl.pop_reference_frame();
    }

    pub fn add_stacking_context_from_yaml(
        &mut self,
        dl: &mut DisplayListBuilder,
        wrench: &mut Wrench,
        yaml: &Yaml,
        is_root: bool,
        info: &mut LayoutPrimitiveInfo,
    ) {
        let default_bounds = LayoutRect::new(LayoutPoint::zero(), wrench.window_size_f32());
        let bounds = yaml["bounds"].as_rect().unwrap_or(default_bounds);
        info.rect = bounds;
        info.clip_rect = bounds;

        let reference_frame_id = if !yaml["transform"].is_badvalue() ||
            !yaml["perspective"].is_badvalue() {
            let reference_frame_id = self.push_reference_frame(dl, wrench, yaml, info);
            info.rect.origin = LayoutPoint::zero();
            info.clip_rect.origin = LayoutPoint::zero();
            Some(reference_frame_id)
        } else {
            None
        };

        let clip_node_id = self.to_clip_id(&yaml["clip-node"], dl.pipeline_id);
        let transform_style = yaml["transform-style"]
            .as_transform_style()
            .unwrap_or(TransformStyle::Flat);
        let mix_blend_mode = yaml["mix-blend-mode"]
            .as_mix_blend_mode()
            .unwrap_or(MixBlendMode::Normal);
        let glyph_raster_space = yaml["glyph-raster-space"]
            .as_glyph_raster_space()
            .unwrap_or(GlyphRasterSpace::Screen);

        if is_root {
            if let Some(size) = yaml["scroll-offset"].as_point() {
                let external_id = ExternalScrollId(0, dl.pipeline_id);
                self.scroll_offsets.insert(external_id, LayoutPoint::new(size.x, size.y));
            }
        }

        let filters = yaml["filters"].as_vec_filter_op().unwrap_or(vec![]);

        if let Some(reference_frame_id) = reference_frame_id {
            dl.push_clip_id(reference_frame_id);
        }

        dl.push_stacking_context(
            &info,
            clip_node_id,
            transform_style,
            mix_blend_mode,
            filters,
            glyph_raster_space,
        );

        if !yaml["items"].is_badvalue() {
            self.add_display_list_items_from_yaml(dl, wrench, &yaml["items"]);
        }

        dl.pop_stacking_context();

        if reference_frame_id.is_some() {
            dl.pop_clip_id();
        }

        if reference_frame_id.is_some() {
            dl.pop_reference_frame();
        }
    }
}

impl WrenchThing for YamlFrameReader {
    fn do_frame(&mut self, wrench: &mut Wrench) -> u32 {
        if !self.frame_built || self.watch_source {
            self.build(wrench);
            self.frame_built = false;
        }

        self.frame_count += 1;

        if !self.frame_built || wrench.should_rebuild_display_lists() {
            wrench.begin_frame();
            wrench.send_lists(
                self.frame_count,
                self.display_lists.clone(),
                &self.scroll_offsets,
            );
        } else {
            wrench.refresh();
        }

        self.frame_built = true;
        self.frame_count
    }

    fn next_frame(&mut self) {}

    fn prev_frame(&mut self) {}

    fn queue_frames(&self) -> u32 {
        self.queue_depth
    }
}
