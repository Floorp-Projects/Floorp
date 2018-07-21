/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate serde_bytes;

use font::{FontInstanceKey, FontKey, FontTemplate};
use std::sync::Arc;
use {DevicePoint, DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use {IdNamespace, TileOffset, TileSize};
use euclid::size2;

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ImageKey(pub IdNamespace, pub u32);

impl ImageKey {
    pub const DUMMY: Self = ImageKey(IdNamespace(0), 0);

    pub fn new(namespace: IdNamespace, key: u32) -> Self {
        ImageKey(namespace, key)
    }
}

/// An arbitrary identifier for an external image provided by the
/// application. It must be a unique identifier for each external
/// image.
#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash, Serialize, Deserialize)]
pub struct ExternalImageId(pub u64);

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq, Serialize, Deserialize)]
pub enum TextureTarget {
    Default = 0,
    Array = 1,
    Rect = 2,
    External = 3,
}

#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq, Serialize, Deserialize)]
pub enum ExternalImageType {
    TextureHandle(TextureTarget),
    Buffer,
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
    R8 = 1,
    BGRA8 = 3,
    RGBAF32 = 4,
    RG8 = 5,
    RGBAI32 = 6,
}

impl ImageFormat {
    pub fn bytes_per_pixel(self) -> u32 {
        match self {
            ImageFormat::R8 => 1,
            ImageFormat::BGRA8 => 4,
            ImageFormat::RGBAF32 => 16,
            ImageFormat::RG8 => 2,
            ImageFormat::RGBAI32 => 16,
        }
    }
}

#[derive(Copy, Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageDescriptor {
    pub format: ImageFormat,
    pub size: DeviceUintSize,
    pub stride: Option<u32>,
    pub offset: u32,
    pub is_opaque: bool,
    pub allow_mipmaps: bool,
}

impl ImageDescriptor {
    pub fn new(
        width: u32,
        height: u32,
        format: ImageFormat,
        is_opaque: bool,
        allow_mipmaps: bool,
    ) -> Self {
        ImageDescriptor {
            size: size2(width, height),
            format,
            stride: None,
            offset: 0,
            is_opaque,
            allow_mipmaps,
        }
    }

    pub fn compute_stride(&self) -> u32 {
        self.stride.unwrap_or(self.size.width * self.format.bytes_per_pixel())
    }

    pub fn compute_total_size(&self) -> u32 {
        self.compute_stride() * self.size.height
    }

    pub fn full_rect(&self) -> DeviceUintRect {
        DeviceUintRect::new(
            DeviceUintPoint::zero(),
            self.size,
        )
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum ImageData {
    Raw(#[serde(with = "serde_image_data_raw")] Arc<Vec<u8>>),
    Blob(#[serde(with = "serde_image_data_raw")] Arc<BlobImageData>),
    External(ExternalImageData),
}

mod serde_image_data_raw {
    extern crate serde_bytes;

    use std::sync::Arc;
    use serde::{Deserializer, Serializer};

    pub fn serialize<S: Serializer>(bytes: &Arc<Vec<u8>>, serializer: S) -> Result<S::Ok, S::Error> {
        serde_bytes::serialize(bytes.as_slice(), serializer)
    }

    pub fn deserialize<'de, D: Deserializer<'de>>(deserializer: D) -> Result<Arc<Vec<u8>>, D::Error> {
        serde_bytes::deserialize(deserializer).map(Arc::new)
    }
}

impl ImageData {
    pub fn new(bytes: Vec<u8>) -> Self {
        ImageData::Raw(Arc::new(bytes))
    }

    pub fn new_shared(bytes: Arc<Vec<u8>>) -> Self {
        ImageData::Raw(bytes)
    }

    pub fn new_blob_image(commands: BlobImageData) -> Self {
        ImageData::Blob(Arc::new(commands))
    }

    #[inline]
    pub fn is_blob(&self) -> bool {
        match *self {
            ImageData::Blob(_) => true,
            _ => false,
        }
    }

    #[inline]
    pub fn uses_texture_cache(&self) -> bool {
        match *self {
            ImageData::External(ref ext_data) => match ext_data.image_type {
                ExternalImageType::TextureHandle(_) => false,
                ExternalImageType::Buffer => true,
            },
            ImageData::Blob(_) => true,
            ImageData::Raw(_) => true,
        }
    }
}

/// The resources exposed by the resource cache available for use by the blob rasterizer.
pub trait BlobImageResources {
    fn get_font_data(&self, key: FontKey) -> &FontTemplate;
    fn get_image(&self, key: ImageKey) -> Option<(&ImageData, &ImageDescriptor)>;
}

/// A handler on the render backend that can create rasterizer objects which will
/// be sent to the scene builder thread to execute the rasterization.
///
/// The handler is responsible for collecting resources, managing/updating blob commands
/// and creating the rasterizer objects, but isn't expected to do any rasterization itself.
pub trait BlobImageHandler: Send {
    /// Creates a snapshot of the current state of blob images in the handler.
    fn create_blob_rasterizer(&mut self) -> Box<AsyncBlobImageRasterizer>;

    /// A hook to let the blob image handler update any state related to resources that
    /// are not bundled in the blob recording itself.
    fn prepare_resources(
        &mut self,
        services: &BlobImageResources,
        requests: &[BlobImageParams],
    );

    /// Register a blob image.
    fn add(&mut self, key: ImageKey, data: Arc<BlobImageData>, tiling: Option<TileSize>);

    /// Update an already registered blob image.
    fn update(&mut self, key: ImageKey, data: Arc<BlobImageData>, dirty_rect: Option<DeviceUintRect>);

    /// Delete an already registered blob image.
    fn delete(&mut self, key: ImageKey);

    /// A hook to let the handler clean up any state related to a font which the resource
    /// cache is about to delete.
    fn delete_font(&mut self, key: FontKey);

    /// A hook to let the handler clean up any state related to a font instance which the
    /// resource cache is about to delete.
    fn delete_font_instance(&mut self, key: FontInstanceKey);

    /// A hook to let the handler clean up any state related a given namespace before the
    /// resource cache deletes them.
    fn clear_namespace(&mut self, namespace: IdNamespace);
}

/// A group of rasterization requests to execute synchronously on the scene builder thread.
pub trait AsyncBlobImageRasterizer : Send {
    fn rasterize(&mut self, requests: &[BlobImageParams]) -> Vec<(BlobImageRequest, BlobImageResult)>;
}


#[derive(Copy, Clone, Debug)]
pub struct BlobImageParams {
    pub request: BlobImageRequest,
    pub descriptor: BlobImageDescriptor,
    pub dirty_rect: Option<DeviceUintRect>,
}

pub type BlobImageData = Vec<u8>;

pub type BlobImageResult = Result<RasterizedBlobImage, BlobImageError>;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct BlobImageDescriptor {
    pub size: DeviceUintSize,
    pub offset: DevicePoint,
    pub format: ImageFormat,
}

pub struct RasterizedBlobImage {
    pub size: DeviceUintSize,
    pub data: Arc<Vec<u8>>,
}

#[derive(Clone, Debug)]
pub enum BlobImageError {
    Oom,
    InvalidKey,
    InvalidData,
    Other(String),
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct BlobImageRequest {
    pub key: ImageKey,
    pub tile: Option<TileOffset>,
}
