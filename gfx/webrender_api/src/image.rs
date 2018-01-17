/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use {DevicePoint, DeviceUintRect};
use {TileOffset, TileSize};
use IdNamespace;
use font::{FontInstanceKey, FontKey, FontTemplate};
use std::sync::Arc;

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ImageKey(pub IdNamespace, pub u32);

impl ImageKey {
    pub fn new(namespace: IdNamespace, key: u32) -> ImageKey {
        ImageKey(namespace, key)
    }

    pub fn dummy() -> ImageKey {
        ImageKey(IdNamespace(0), 0)
    }
}

/// An arbitrary identifier for an external image provided by the
/// application. It must be a unique identifier for each external
/// image.
#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash, Serialize, Deserialize)]
pub struct ExternalImageId(pub u64);

#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq, Serialize, Deserialize)]
pub enum ExternalImageType {
    Texture2DHandle,       // gl TEXTURE_2D handle
    Texture2DArrayHandle,  // gl TEXTURE_2D_ARRAY handle
    TextureRectHandle,     // gl TEXTURE_RECT handle
    TextureExternalHandle, // gl TEXTURE_EXTERNAL handle
    ExternalBuffer,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq, Serialize, Deserialize)]
pub struct ExternalImageData {
    pub id: ExternalImageId,
    pub channel_index: u8,
    pub image_type: ExternalImageType,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ImageFormat {
    Invalid = 0,
    R8 = 1,
    BGRA8 = 3,
    RGBAF32 = 4,
    RG8 = 5,
}

impl ImageFormat {
    pub fn bytes_per_pixel(self) -> u32 {
        match self {
            ImageFormat::R8 => 1,
            ImageFormat::BGRA8 => 4,
            ImageFormat::RGBAF32 => 16,
            ImageFormat::RG8 => 2,
            ImageFormat::Invalid => 0,
        }
    }
}

#[derive(Copy, Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageDescriptor {
    pub format: ImageFormat,
    pub width: u32,
    pub height: u32,
    pub stride: Option<u32>,
    pub offset: u32,
    pub is_opaque: bool,
}

impl ImageDescriptor {
    pub fn new(width: u32, height: u32, format: ImageFormat, is_opaque: bool) -> Self {
        ImageDescriptor {
            width,
            height,
            format,
            stride: None,
            offset: 0,
            is_opaque,
        }
    }

    pub fn compute_stride(&self) -> u32 {
        self.stride
            .unwrap_or(self.width * self.format.bytes_per_pixel())
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum ImageData {
    Raw(Arc<Vec<u8>>),
    Blob(BlobImageData),
    External(ExternalImageData),
}

impl ImageData {
    pub fn new(bytes: Vec<u8>) -> ImageData {
        ImageData::Raw(Arc::new(bytes))
    }

    pub fn new_shared(bytes: Arc<Vec<u8>>) -> ImageData {
        ImageData::Raw(bytes)
    }

    pub fn new_blob_image(commands: Vec<u8>) -> ImageData {
        ImageData::Blob(commands)
    }

    #[inline]
    pub fn is_blob(&self) -> bool {
        match self {
            &ImageData::Blob(_) => true,
            _ => false,
        }
    }

    #[inline]
    pub fn uses_texture_cache(&self) -> bool {
        match self {
            &ImageData::External(ext_data) => match ext_data.image_type {
                ExternalImageType::Texture2DHandle => false,
                ExternalImageType::Texture2DArrayHandle => false,
                ExternalImageType::TextureRectHandle => false,
                ExternalImageType::TextureExternalHandle => false,
                ExternalImageType::ExternalBuffer => true,
            },
            &ImageData::Blob(_) => true,
            &ImageData::Raw(_) => true,
        }
    }
}

pub trait BlobImageResources {
    fn get_font_data(&self, key: FontKey) -> &FontTemplate;
    fn get_image(&self, key: ImageKey) -> Option<(&ImageData, &ImageDescriptor)>;
}

pub trait BlobImageRenderer: Send {
    fn add(&mut self, key: ImageKey, data: BlobImageData, tiling: Option<TileSize>);

    fn update(&mut self, key: ImageKey, data: BlobImageData, dirty_rect: Option<DeviceUintRect>);

    fn delete(&mut self, key: ImageKey);

    fn request(
        &mut self,
        services: &BlobImageResources,
        key: BlobImageRequest,
        descriptor: &BlobImageDescriptor,
        dirty_rect: Option<DeviceUintRect>,
    );

    fn resolve(&mut self, key: BlobImageRequest) -> BlobImageResult;

    fn delete_font(&mut self, key: FontKey);

    fn delete_font_instance(&mut self, key: FontInstanceKey);
}

pub type BlobImageData = Vec<u8>;

pub type BlobImageResult = Result<RasterizedBlobImage, BlobImageError>;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct BlobImageDescriptor {
    pub width: u32,
    pub height: u32,
    pub offset: DevicePoint,
    pub format: ImageFormat,
}

pub struct RasterizedBlobImage {
    pub width: u32,
    pub height: u32,
    pub data: Vec<u8>,
}

#[derive(Clone, Debug)]
pub enum BlobImageError {
    Oom,
    InvalidKey,
    InvalidData,
    Other(String),
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct BlobImageRequest {
    pub key: ImageKey,
    pub tile: Option<TileOffset>,
}
