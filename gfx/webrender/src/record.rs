/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use bincode::serde::serialize;
use bincode;
use std::mem;
use std::any::TypeId;
use std::fs::{File, OpenOptions};
use std::io::Write;
use std::ops::DerefMut;
use std::sync::Mutex;
use webrender_traits::ApiMsg;
use byteorder::{LittleEndian, WriteBytesExt};

lazy_static! {
    static ref WEBRENDER_RECORDING_DETOUR: Mutex<Option<Box<ApiRecordingReceiver>>> = Mutex::new(None);
}

pub static WEBRENDER_RECORDING_HEADER: u64 = 0xbeefbeefbeefbe01u64;
static mut CURRENT_FRAME_NUMBER: u32 = 0xffffffffu32;

pub trait ApiRecordingReceiver: Send {
    fn write_msg(&mut self, frame: u32, msg: &ApiMsg);
    fn write_payload(&mut self, frame: u32, data: &[u8]);
}

pub fn set_recording_detour(detour: Option<Box<ApiRecordingReceiver>>) {
    let mut recorder = WEBRENDER_RECORDING_DETOUR.lock();
    *recorder.as_mut().unwrap().deref_mut() = detour;
}

fn write_data(frame: u32, data: &[u8]) {
    let filename = format!("record/frame_{}.bin", frame);
    let mut file = if unsafe { CURRENT_FRAME_NUMBER != frame } {
        unsafe { CURRENT_FRAME_NUMBER = frame; }

        let mut file = File::create(filename).unwrap();
        let apimsg_type_id = unsafe {
            assert!(mem::size_of::<TypeId>() == mem::size_of::<u64>());
            mem::transmute::<TypeId, u64>(TypeId::of::<ApiMsg>())
        };

        file.write_u64::<LittleEndian>(WEBRENDER_RECORDING_HEADER).ok();
        file.write_u64::<LittleEndian>(apimsg_type_id).ok();
        file
    } else {
        OpenOptions::new().append(true).create(false).open(filename).unwrap()
    };
    file.write_u32::<LittleEndian>(data.len() as u32).ok();
    file.write(data).ok();
}

pub fn write_msg(frame: u32, msg: &ApiMsg) {
    match msg {
        &ApiMsg::AddRawFont(..) |
        &ApiMsg::AddNativeFont(..) |
        &ApiMsg::AddImage(..) |
        &ApiMsg::UpdateImage(..) |
        &ApiMsg::DeleteImage(..)|
        &ApiMsg::SetRootDisplayList(..) |
        &ApiMsg::SetRootPipeline(..) |
        &ApiMsg::Scroll(..) |
        &ApiMsg::TickScrollingBounce |
        &ApiMsg::WebGLCommand(..) => {
            let mut recorder = WEBRENDER_RECORDING_DETOUR.lock();
            if let Some(ref mut recorder) = recorder.as_mut().unwrap().as_mut() {
                recorder.write_msg(frame, &msg);
            } else {
                let buff = serialize(msg, bincode::SizeLimit::Infinite).unwrap();
                write_data(frame, &buff);
            }
       }
       _ => {}
    }
}

pub fn write_payload(frame: u32, data: &[u8]) {
    let mut recorder = WEBRENDER_RECORDING_DETOUR.lock();
    if let Some(ref mut recorder) = recorder.as_mut().unwrap().as_mut() {
        recorder.write_payload(frame, data);
    } else {
        write_data(frame, &[]); //signal the payload
        write_data(frame, data);
    }
}
