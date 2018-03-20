/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use {WindowWrapper, NotifierEvent};
use image::png::PNGEncoder;
use image::{self, ColorType, GenericImage};
use std::fs::File;
use std::path::Path;
use std::sync::mpsc::Receiver;
use webrender::api::*;
use wrench::{Wrench, WrenchThing};
use yaml_frame_reader::YamlFrameReader;

pub enum ReadSurface {
    Screen,
    GpuCache,
}

pub struct SaveSettings {
    pub flip_vertical: bool,
    pub try_crop: bool,
}

pub fn save<P: Clone + AsRef<Path>>(
    path: P,
    orig_pixels: Vec<u8>,
    mut size: DeviceUintSize,
    settings: SaveSettings
) {
    let mut buffer = image::RgbaImage::from_raw(
        size.width,
        size.height,
        orig_pixels,
    ).expect("bug: unable to construct image buffer");

    if settings.flip_vertical {
        // flip image vertically (texture is upside down)
        buffer = image::imageops::flip_vertical(&buffer);
    }

    if settings.try_crop {
        if let Ok(existing_image) = image::open(path.clone()) {
            let old_dims = existing_image.dimensions();
            println!("Crop from {:?} to {:?}", size, old_dims);
            size.width = old_dims.0;
            size.height = old_dims.1;
            buffer = image::imageops::crop(
                &mut buffer,
                0,
                0,
                size.width,
                size.height
            ).to_image();
        }
    }

    let encoder = PNGEncoder::new(File::create(path).unwrap());
    encoder
        .encode(&buffer, size.width, size.height, ColorType::RGBA(8))
        .expect("Unable to encode PNG!");
}

pub fn save_flipped<P: Clone + AsRef<Path>>(
    path: P,
    orig_pixels: Vec<u8>,
    size: DeviceUintSize,
) {
    save(path, orig_pixels, size, SaveSettings {
        flip_vertical: true,
        try_crop: true,
    })
}

pub fn png(
    wrench: &mut Wrench,
    surface: ReadSurface,
    window: &mut WindowWrapper,
    mut reader: YamlFrameReader,
    rx: Receiver<NotifierEvent>,
) {
    reader.do_frame(wrench);

    // wait for the frame
    rx.recv().unwrap();
    wrench.render();

    let (device_size, data, settings) = match surface {
        ReadSurface::Screen => {
            let dim = window.get_inner_size();
            let rect = DeviceUintRect::new(DeviceUintPoint::zero(), dim);
            let data = wrench.renderer
                .read_pixels_rgba8(rect);
            (rect.size, data, SaveSettings {
                flip_vertical: true,
                try_crop: true,
            })
        }
        ReadSurface::GpuCache => {
            let (size, data) = wrench.renderer
                .read_gpu_cache();
            (size, data, SaveSettings {
                flip_vertical: false,
                try_crop: false,
            })
        }
    };

    let mut out_path = reader.yaml_path().clone();
    out_path.set_extension("png");

    save(out_path, data, device_size, settings);
}
