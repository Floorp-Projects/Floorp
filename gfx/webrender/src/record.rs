use bincode::serde::serialize;
use bincode;
use std::fs::OpenOptions;
use std::io::Write;
use webrender_traits::ApiMsg;
use byteorder::{LittleEndian, WriteBytesExt};

fn write_data(frame: u32, data: &[u8]) {
    let filename = format!("record/frame_{}.bin", frame);
    let mut file = OpenOptions::new().append(true).create(true).open(filename).unwrap();
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
        &ApiMsg::SetRootStackingContext(..) |
        &ApiMsg::SetRootPipeline(..) |
        &ApiMsg::Scroll(..)|
        &ApiMsg::TickScrollingBounce |
        &ApiMsg::WebGLCommand(..) => {
            let buff = serialize(msg, bincode::SizeLimit::Infinite).unwrap();
            write_data(frame, &buff);
       }
       _ => {}
    }
}

pub fn write_payload(frame: u32, data: &[u8]) {
    write_data(frame, &[]); //signal the payload
    write_data(frame, data);
}
