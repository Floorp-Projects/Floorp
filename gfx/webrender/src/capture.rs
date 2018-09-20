/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fs::File;
use std::path::{Path, PathBuf};

use api::{CaptureBits, ExternalImageData, ImageDescriptor, TexelRect};
#[cfg(feature = "png")]
use device::ReadPixelsFormat;
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
    pub fn serialize<T, P>(&self, data: &T, name: P)
    where
        T: serde::Serialize,
        P: AsRef<Path>,
    {
        use std::io::Write;

        let ron = ron::ser::to_string_pretty(data, self.pretty.clone())
            .unwrap();
        let path = self.root
            .join(name)
            .with_extension("ron");
        let mut file = File::create(path)
            .unwrap();
        write!(file, "{}\n", ron)
            .unwrap();
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
            .join(name)
            .with_extension("ron");
        File::open(path)
            .ok()?
            .read_to_string(&mut string)
            .unwrap();
        Some(ron::de::from_str(&string)
            .unwrap())
    }

    #[cfg(feature = "png")]
    pub fn save_png(
        path: PathBuf, size: (u32, u32), format: ReadPixelsFormat, data: &[u8],
    ) {
        use api::ImageFormat;
        use png::{BitDepth, ColorType, Encoder, HasParameters};
        use std::io::BufWriter;

        let color_type = match format {
            ReadPixelsFormat::Rgba8 => ColorType::RGBA,
            ReadPixelsFormat::Standard(ImageFormat::BGRA8) => {
                warn!("Unable to swizzle PNG of BGRA8 type");
                ColorType::RGBA
            },
            ReadPixelsFormat::Standard(ImageFormat::R8) => ColorType::Grayscale,
            ReadPixelsFormat::Standard(ImageFormat::RG8) => ColorType::GrayscaleAlpha,
            ReadPixelsFormat::Standard(fm) => {
                error!("Unable to save PNG of {:?}", fm);
                return;
            }
        };
        let w = BufWriter::new(File::create(path).unwrap());
        let mut enc = Encoder::new(w, size.0, size.1);
        enc
            .set(color_type)
            .set(BitDepth::Eight);
        enc
            .write_header()
            .unwrap()
            .write_image_data(&data)
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
