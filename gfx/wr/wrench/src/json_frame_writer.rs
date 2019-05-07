/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// the json code is largely unfinished; allow these to silence a bunch of warnings
#![allow(unused_variables)]
#![allow(dead_code)]

use image::{save_buffer, ColorType};
use crate::premultiply::unpremultiply;
use serde_json;
use std::collections::HashMap;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::{fmt, fs};
use super::CURRENT_FRAME_NUMBER;
use time;
use webrender;
use webrender::api::*;
use webrender::api::channel::Payload;
use webrender::api::units::*;


enum CachedFont {
    Native(NativeFontHandle),
    Raw(Option<Vec<u8>>, u32, Option<PathBuf>),
}

struct CachedFontInstance {
    font_key: FontKey,
    glyph_size: Au,
}

struct CachedImage {
    width: i32,
    height: i32,
    stride: i32,
    format: ImageFormat,
    bytes: Option<Vec<u8>>,
    path: Option<PathBuf>,
}

pub struct JsonFrameWriter {
    frame_base: PathBuf,
    rsrc_base: PathBuf,
    rsrc_prefix: String,
    next_rsrc_num: u32,
    images: HashMap<ImageKey, CachedImage>,
    fonts: HashMap<FontKey, CachedFont>,
    font_instances: HashMap<FontInstanceKey, CachedFontInstance>,

    last_frame_written: u32,

    dl_descriptor: Option<BuiltDisplayListDescriptor>,
}

impl JsonFrameWriter {
    pub fn new(path: &Path) -> Self {
        let mut rsrc_base = path.to_owned();
        rsrc_base.push("res");
        fs::create_dir_all(&rsrc_base).ok();

        let rsrc_prefix = format!("{}", time::get_time().sec);

        JsonFrameWriter {
            frame_base: path.to_owned(),
            rsrc_base,
            rsrc_prefix,
            next_rsrc_num: 1,
            images: HashMap::new(),
            fonts: HashMap::new(),
            font_instances: HashMap::new(),

            dl_descriptor: None,

            last_frame_written: u32::max_value(),
        }
    }

    pub fn begin_write_display_list(
        &mut self,
        _: &Epoch,
        _: &PipelineId,
        _: &Option<ColorF>,
        _: &LayoutSize,
        display_list: &BuiltDisplayListDescriptor,
    ) {
        unsafe {
            if CURRENT_FRAME_NUMBER == self.last_frame_written {
                return;
            }
            self.last_frame_written = CURRENT_FRAME_NUMBER;
        }

        self.dl_descriptor = Some(display_list.clone());
    }

    pub fn finish_write_display_list(&mut self, frame: u32, data: &[u8]) {
        let payload = Payload::from_data(data);
        let dl_desc = self.dl_descriptor.take().unwrap();

        let dl = BuiltDisplayList::from_data(payload.display_list_data, dl_desc);

        let mut frame_file_name = self.frame_base.clone();
        let current_shown_frame = unsafe { CURRENT_FRAME_NUMBER };
        frame_file_name.push(format!("frame-{}.json", current_shown_frame));

        let mut file = fs::File::create(&frame_file_name).unwrap();

        let s = serde_json::to_string_pretty(&dl).unwrap();
        file.write_all(&s.into_bytes()).unwrap();
        file.write_all(b"\n").unwrap();
    }

    fn update_resources(&mut self, updates: &[ResourceUpdate]) {
        for update in updates {
            match *update {
                ResourceUpdate::AddImage(ref img) => {
                    let stride = img.descriptor.stride.unwrap_or(
                        img.descriptor.size.width * img.descriptor.format.bytes_per_pixel(),
                    );
                    let bytes = match img.data {
                        ImageData::Raw(ref v) => (**v).clone(),
                        ImageData::External(_) => {
                            return;
                        }
                    };
                    self.images.insert(
                        img.key,
                        CachedImage {
                            width: img.descriptor.size.width,
                            height: img.descriptor.size.height,
                            stride,
                            format: img.descriptor.format,
                            bytes: Some(bytes),
                            path: None,
                        },
                    );
                }
                ResourceUpdate::UpdateImage(ref img) => {
                    if let Some(ref mut data) = self.images.get_mut(&img.key) {
                        assert_eq!(data.width, img.descriptor.size.width);
                        assert_eq!(data.height, img.descriptor.size.height);
                        assert_eq!(data.format, img.descriptor.format);

                        if let ImageData::Raw(ref bytes) = img.data {
                            data.path = None;
                            data.bytes = Some((**bytes).clone());
                        } else {
                            // Other existing image types only make sense within the gecko integration.
                            println!(
                                "Wrench only supports updating buffer images ({}).",
                                "ignoring update commands"
                            );
                        }
                    }
                }
                ResourceUpdate::AddBlobImage(..)
                | ResourceUpdate::UpdateBlobImage(..) => {
                    println!("Blob images not supported (ignoring command).");
                }
                ResourceUpdate::DeleteImage(img) => {
                    self.images.remove(&img);
                }
                ResourceUpdate::AddFont(ref font) => match font {
                    &AddFont::Raw(key, ref bytes, index) => {
                        self.fonts
                            .insert(key, CachedFont::Raw(Some(bytes.clone()), index, None));
                    }
                    &AddFont::Native(key, ref handle) => {
                        self.fonts.insert(key, CachedFont::Native(handle.clone()));
                    }
                },
                ResourceUpdate::DeleteFont(_) => {}
                ResourceUpdate::AddFontInstance(ref instance) => {
                    self.font_instances.insert(
                        instance.key,
                        CachedFontInstance {
                            font_key: instance.font_key,
                            glyph_size: instance.glyph_size,
                        },
                    );
                }
                ResourceUpdate::DeleteFontInstance(_) => {}
                ResourceUpdate::SetBlobImageVisibleArea(..) => {}
            }
        }
    }

    fn next_rsrc_paths(
        prefix: &str,
        counter: &mut u32,
        base_path: &Path,
        base: &str,
        ext: &str,
    ) -> (PathBuf, PathBuf) {
        let mut path_file = base_path.to_owned();
        let mut path = PathBuf::from("res");

        let fstr = format!("{}-{}-{}.{}", prefix, base, counter, ext);
        path_file.push(&fstr);
        path.push(&fstr);

        *counter += 1;

        (path_file, path)
    }

    fn path_for_image(&mut self, key: ImageKey) -> Option<PathBuf> {
        if let Some(ref mut data) = self.images.get_mut(&key) {
            if data.path.is_some() {
                return data.path.clone();
            }
        } else {
            return None;
        };

        // Remove the data to munge it
        let mut data = self.images.remove(&key).unwrap();
        let mut bytes = data.bytes.take().unwrap();
        let (path_file, path) = Self::next_rsrc_paths(
            &self.rsrc_prefix,
            &mut self.next_rsrc_num,
            &self.rsrc_base,
            "img",
            "png",
        );

        let ok = match data.format {
            ImageFormat::BGRA8 => if data.stride == data.width * 4 {
                unpremultiply(bytes.as_mut_slice());
                save_buffer(
                    &path_file,
                    &bytes,
                    data.width as u32,
                    data.height as u32,
                    ColorType::RGBA(8),
                ).unwrap();
                true
            } else {
                false
            },
            ImageFormat::R8 => if data.stride == data.width {
                save_buffer(
                    &path_file,
                    &bytes,
                    data.width as u32,
                    data.height as u32,
                    ColorType::Gray(8),
                ).unwrap();
                true
            } else {
                false
            },
            _ => false,
        };

        if !ok {
            println!(
                "Failed to write image with format {:?}, dimensions {}x{}, stride {}",
                data.format,
                data.width,
                data.height,
                data.stride
            );
            return None;
        }

        data.path = Some(path.clone());
        // put it back
        self.images.insert(key, data);
        Some(path)
    }

    fn update_document(&mut self, txn: &TransactionMsg) {
        self.update_resources(&txn.resource_updates);
        for doc_msg in &txn.scene_ops {
            match *doc_msg {
                SceneMsg::SetDisplayList {
                    ref epoch,
                    ref pipeline_id,
                    ref background,
                    ref viewport_size,
                    ref list_descriptor,
                    ..
                } => {
                    self.begin_write_display_list(
                        epoch,
                        pipeline_id,
                        background,
                        viewport_size,
                        list_descriptor,
                    );
                }
                _ => {}
            }
        }
    }
}

impl fmt::Debug for JsonFrameWriter {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "JsonFrameWriter")
    }
}

impl webrender::ApiRecordingReceiver for JsonFrameWriter {
    fn write_msg(&mut self, _: u32, msg: &ApiMsg) {
        match *msg {
            ApiMsg::UpdateResources(ref updates) => self.update_resources(updates),
            ApiMsg::UpdateDocuments(_, ref txns) => {
                for txn in txns {
                    self.update_document(txn)
                }
            }
            ApiMsg::CloneApi(..) => {}
            _ => {}
        }
    }

    fn write_payload(&mut self, frame: u32, data: &[u8]) {
        if self.dl_descriptor.is_some() {
            self.finish_write_display_list(frame, data);
        }
    }
}
