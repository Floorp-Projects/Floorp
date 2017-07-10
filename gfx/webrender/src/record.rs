/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use bincode::{Infinite, serialize};
use std::fmt::Debug;
use std::mem;
use std::any::TypeId;
use std::fs::File;
use std::io::Write;
use std::path::PathBuf;
use api::ApiMsg;
use byteorder::{LittleEndian, WriteBytesExt};

pub static WEBRENDER_RECORDING_HEADER: u64 = 0xbeefbeefbeefbe01u64;

pub trait ApiRecordingReceiver: Send + Debug {
    fn write_msg(&mut self, frame: u32, msg: &ApiMsg);
    fn write_payload(&mut self, frame: u32, data: &[u8]);
}

#[derive(Debug)]
pub struct BinaryRecorder {
    file: File,
}

impl BinaryRecorder {
    pub fn new(dest: &PathBuf) -> BinaryRecorder {
        let mut file = File::create(dest).unwrap();

        // write the header
        let apimsg_type_id = unsafe {
            assert!(mem::size_of::<TypeId>() == mem::size_of::<u64>());
            mem::transmute::<TypeId, u64>(TypeId::of::<ApiMsg>())
        };
        file.write_u64::<LittleEndian>(WEBRENDER_RECORDING_HEADER).ok();
        file.write_u64::<LittleEndian>(apimsg_type_id).ok();

        BinaryRecorder {
            file,
        }
    }

    fn write_length_and_data(&mut self, data: &[u8]) {
        self.file.write_u32::<LittleEndian>(data.len() as u32).ok();
        self.file.write(data).ok();
    }
}

impl ApiRecordingReceiver for BinaryRecorder {
    fn write_msg(&mut self, _: u32, msg: &ApiMsg) {
        if should_record_msg(msg) {
            let buf = serialize(msg, Infinite).unwrap();
            self.write_length_and_data(&buf);
        }
    }

    fn write_payload(&mut self, _: u32, data: &[u8]) {
        // signal payload with a 0 length
        self.file.write_u32::<LittleEndian>(0).ok();
        self.write_length_and_data(data);
    }
}

pub fn should_record_msg(msg: &ApiMsg) -> bool {
    match *msg {
        ApiMsg::AddRawFont(..) |
        ApiMsg::AddNativeFont(..) |
        ApiMsg::DeleteFont(..) |
        ApiMsg::AddImage(..) |
        ApiMsg::GenerateFrame(..) |
        ApiMsg::UpdateImage(..) |
        ApiMsg::DeleteImage(..) |
        ApiMsg::SetDisplayList(..) |
        ApiMsg::SetRootPipeline(..) |
        ApiMsg::Scroll(..) |
        ApiMsg::TickScrollingBounce |
        ApiMsg::WebGLCommand(..) |
        ApiMsg::SetWindowParameters(..) =>
            true,
        _ => false
    }
}
