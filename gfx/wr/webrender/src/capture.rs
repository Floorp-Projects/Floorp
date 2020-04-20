/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fs::File;
use std::path::{Path, PathBuf};

use api::{CaptureBits, ExternalImageData, ImageDescriptor};
#[cfg(feature = "png")]
use api::ImageFormat;
use api::units::TexelRect;
#[cfg(feature = "png")]
use api::units::DeviceIntSize;
#[cfg(feature = "capture")]
use crate::print_tree::{PrintableTree, PrintTree};
use ron;
use serde;


#[derive(Clone)]
pub struct CaptureConfig {
    pub root: PathBuf,
    pub bits: CaptureBits,
    #[cfg(feature = "capture")]
    pretty: ron::ser::PrettyConfig,
}

impl CaptureConfig {
    #[cfg(feature = "capture")]
    pub fn new(root: PathBuf, bits: CaptureBits) -> Self {
        CaptureConfig {
            root,
            bits,
            #[cfg(feature = "capture")]
            pretty: ron::ser::PrettyConfig {
                enumerate_arrays: true,
                .. ron::ser::PrettyConfig::default()
            },
        }
    }

    #[cfg(feature = "capture")]
    pub fn file_path<P>(&self, name: P, ext: &str) -> PathBuf
    where P: AsRef<Path> {
        self.root.join(name).with_extension(ext)
    }

    #[cfg(feature = "capture")]
    pub fn serialize<T, P>(&self, data: &T, name: P)
    where
        T: serde::Serialize,
        P: AsRef<Path>,
    {
        use std::io::Write;

        let ron = ron::ser::to_string_pretty(data, self.pretty.clone())
            .unwrap();
        let path = self.file_path(name, "ron");
        let mut file = File::create(path)
            .unwrap();
        write!(file, "{}\n", ron)
            .unwrap();
    }

    #[cfg(feature = "capture")]
    pub fn serialize_tree<T, P>(&self, data: &T, name: P)
    where
        T: PrintableTree,
        P: AsRef<Path>
    {
        let path = self.root
            .join(name)
            .with_extension("tree");
        let file = File::create(path)
            .unwrap();
        let mut pt = PrintTree::new_with_sink("", file);
        data.print_with(&mut pt);
    }

    #[cfg(feature = "replay")]
    pub fn deserialize<T, P>(root: &PathBuf, name: P) -> Option<T>
    where
        T: for<'a> serde::Deserialize<'a>,
        P: AsRef<Path>,
    {
        use std::io::Read;

        let mut string = String::new();
        let path = root
            .join(name.as_ref())
            .with_extension("ron");
        File::open(path)
            .ok()?
            .read_to_string(&mut string)
            .unwrap();
        match ron::de::from_str(&string) {
            Ok(out) => Some(out),
            Err(e) => panic!("File {:?} deserialization failed: {:?}", name.as_ref(), e),
        }
    }

    #[cfg(feature = "png")]
    pub fn save_png(
        path: PathBuf, size: DeviceIntSize, format: ImageFormat, stride: Option<i32>, data: &[u8],
    ) {
        use png::{BitDepth, ColorType, Encoder};
        use std::io::BufWriter;
        use std::borrow::Cow;

        // `png` expects
        let data = match stride {
            Some(stride) if stride != format.bytes_per_pixel() * size.width => {
                let mut unstrided = Vec::new();
                for y in 0..size.height {
                    let start = (y * stride) as usize;
                    unstrided.extend_from_slice(&data[start..start+(size.width * format.bytes_per_pixel()) as usize]);
                }
                Cow::from(unstrided)
            }
            _ => Cow::from(data),
        };

        let color_type = match format {
            ImageFormat::RGBA8 => ColorType::RGBA,
            ImageFormat::BGRA8 => {
                warn!("Unable to swizzle PNG of BGRA8 type");
                ColorType::RGBA
            },
            ImageFormat::R8 => ColorType::Grayscale,
            ImageFormat::RG8 => ColorType::GrayscaleAlpha,
            _ => {
                error!("Unable to save PNG of {:?}", format);
                return;
            }
        };
        let w = BufWriter::new(File::create(path).unwrap());
        let mut enc = Encoder::new(w, size.width as u32, size.height as u32);
        enc.set_color(color_type);
        enc.set_depth(BitDepth::Eight);
        enc
            .write_header()
            .unwrap()
            .write_image_data(&*data)
            .unwrap();
    }
}

/// An image that `ResourceCache` is unable to resolve during a capture.
/// The image has to be transferred to `Renderer` and locked with the
/// external image handler to get the actual contents and serialize them.
#[derive(Deserialize, Serialize)]
pub struct ExternalCaptureImage {
    pub short_path: String,
    pub descriptor: ImageDescriptor,
    pub external: ExternalImageData,
}

/// A short description of an external image to be saved separately as
/// "externals/XX.ron", redirecting into a specific texture/blob with
/// the corresponding UV rectangle.
#[derive(Deserialize, Serialize)]
pub struct PlainExternalImage {
    /// Path to the RON file describing the texel data.
    pub data: String,
    /// External image data source.
    pub external: ExternalImageData,
    /// UV sub-rectangle of the image.
    pub uv: TexelRect,
}
