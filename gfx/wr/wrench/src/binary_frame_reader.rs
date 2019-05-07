/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use bincode::deserialize;
use byteorder::{LittleEndian, ReadBytesExt};
use clap;
use std::any::TypeId;
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::{mem, process};
use webrender::WEBRENDER_RECORDING_HEADER;
use webrender::api::{ApiMsg, SceneMsg};
use crate::wrench::{Wrench, WrenchThing};

#[derive(Clone)]
enum Item {
    Message(ApiMsg),
    Data(Vec<u8>),
}

pub struct BinaryFrameReader {
    file: File,
    eof: bool,
    frame_offsets: Vec<u64>,

    skip_uploads: bool,
    replay_api: bool,
    play_through: bool,

    frame_data: Vec<Item>,
    frame_num: u32,
    frame_read: bool,
}

impl BinaryFrameReader {
    pub fn new(file_path: &Path) -> BinaryFrameReader {
        let mut file = File::open(&file_path).expect("Can't open recording file");
        let header = file.read_u64::<LittleEndian>().unwrap();
        assert_eq!(
            header,
            WEBRENDER_RECORDING_HEADER,
            "Binary recording is missing recording header!"
        );

        let apimsg_type_id = unsafe {
            assert_eq!(mem::size_of::<TypeId>(), mem::size_of::<u64>());
            mem::transmute::<TypeId, u64>(TypeId::of::<ApiMsg>())
        };

        let written_apimsg_type_id = file.read_u64::<LittleEndian>().unwrap();
        if written_apimsg_type_id != apimsg_type_id {
            println!(
                "Warning: binary file ApiMsg type mismatch: expected 0x{:x}, found 0x{:x}",
                apimsg_type_id,
                written_apimsg_type_id
            );
        }

        BinaryFrameReader {
            file,
            eof: false,
            frame_offsets: vec![],

            skip_uploads: false,
            replay_api: false,
            play_through: false,

            frame_data: vec![],
            frame_num: 0,
            frame_read: false,
        }
    }

    pub fn new_from_args(args: &clap::ArgMatches) -> BinaryFrameReader {
        let bin_file = args.value_of("INPUT").map(|s| PathBuf::from(s)).unwrap();
        let mut r = BinaryFrameReader::new(&bin_file);
        r.skip_uploads = args.is_present("skip-uploads");
        r.replay_api = args.is_present("api");
        r.play_through = args.is_present("play");
        r
    }

    // FIXME I don't think we can skip uploads without also skipping
    // payload (I think? Unused payload ranges may also be ignored.). But
    // either way we'd need to track image updates and deletions -- if we
    // delete an image, we can't go back to a previous frame.
    //
    // We could probably introduce a mode where going backwards replays all
    // frames up until that frame, so that we know we can be accurate.
    fn should_skip_upload_msg(&self, msg: &ApiMsg) -> bool {
        if !self.skip_uploads {
            return false;
        }

        match *msg {
            ApiMsg::UpdateResources(..) => true,
            _ => false,
        }
    }

    // a frame exists if we either haven't hit eof yet, or if
    // we have, then if we've seen its offset.
    fn frame_exists(&self, frame: u32) -> bool {
        !self.eof || (frame as usize) < self.frame_offsets.len()
    }
}

impl WrenchThing for BinaryFrameReader {
    fn do_frame(&mut self, wrench: &mut Wrench) -> u32 {
        // save where the frame begins as we read through the file
        if self.frame_num as usize >= self.frame_offsets.len() {
            self.frame_num = self.frame_offsets.len() as u32;
            let pos = self.file.seek(SeekFrom::Current(0)).unwrap();
            println!("Frame {} offset: {}", self.frame_offsets.len(), pos);
            self.frame_offsets.push(pos);
        }

        let first_time = !self.frame_read;
        if first_time {
            let offset = self.frame_offsets[self.frame_num as usize];
            self.file.seek(SeekFrom::Start(offset)).unwrap();

            wrench.set_title(&format!("frame {}", self.frame_num));

            self.frame_data.clear();
            let mut found_frame_marker = false;
            let mut found_display_list = false;
            let mut found_pipeline = false;
            while let Ok(mut len) = self.file.read_u32::<LittleEndian>() {
                if len > 0 {
                    let mut buffer = vec![0; len as usize];
                    self.file.read_exact(&mut buffer).unwrap();
                    let msg = deserialize(&buffer).unwrap();
                    let mut store_message = true;
                    // In order to detect the first valid frame, we
                    // need to find:
                    // (a) SetRootPipeline
                    // (b) SetDisplayList
                    // (c) GenerateFrame that occurs *after* (a) and (b)
                    match msg {
                        ApiMsg::UpdateDocuments(_, ref txns) => {
                            for txn in txns {
                                if txn.generate_frame {
                                    // TODO: is this appropriate, or do we need a ternary value / something else?
                                    found_frame_marker = true;
                                }
                                for doc_msg in &txn.scene_ops {
                                    match *doc_msg {
                                        SceneMsg::SetDisplayList { .. } => {
                                            found_frame_marker = false;
                                            found_display_list = true;
                                        }
                                        SceneMsg::SetRootPipeline(..) => {
                                            found_frame_marker = false;
                                            found_pipeline = true;
                                        }
                                        _ => {}
                                    }
                                }
                            }
                        }
                        // Wrench depends on the document always existing
                        ApiMsg::DeleteDocument(_) => {
                            store_message = false;
                        }
                        _ => {}
                    }
                    if store_message {
                        self.frame_data.push(Item::Message(msg));
                    }
                    // Frames are marked by the GenerateFrame message.
                    // On the first frame, we additionally need to find at least
                    // a SetDisplayList and a SetRootPipeline.
                    // After the first frame, any GenerateFrame message marks a new
                    // frame being rendered.
                    if found_frame_marker && (self.frame_num > 0 || (found_display_list && found_pipeline)) {
                        break;
                    }
                } else {
                    len = self.file.read_u32::<LittleEndian>().unwrap();
                    let mut buffer = vec![0; len as usize];
                    self.file.read_exact(&mut buffer).unwrap();
                    self.frame_data.push(Item::Data(buffer));
                }
            }

            if self.eof == false &&
                self.file.seek(SeekFrom::Current(0)).unwrap() == self.file.metadata().unwrap().len()
            {
                self.eof = true;
            }

            self.frame_read = true;
        }

        if first_time || self.replay_api {
            wrench.begin_frame();
            let frame_items = self.frame_data.clone();
            for item in frame_items {
                match item {
                    Item::Message(msg) => if !self.should_skip_upload_msg(&msg) {
                        wrench.api.send_message(msg);
                    },
                    Item::Data(buf) => {
                        wrench.api.send_payload(&buf);
                    }
                }
            }
        } else if self.play_through {
            if !self.frame_exists(self.frame_num + 1) {
                process::exit(0);
            }
            self.next_frame();
            self.do_frame(wrench);
        } else {
            wrench.refresh();
        }

        self.frame_num
    }

    // note that we don't loop here; we could, but that'll require
    // some tracking to avoid reuploading resources every run.  We
    // sort of try in should_skip_upload_msg, but this needs work.
    fn next_frame(&mut self) {
        if self.frame_exists(self.frame_num + 1) {
            self.frame_num += 1;
            self.frame_read = false;
        }
    }

    fn prev_frame(&mut self) {
        if self.frame_num > 0 {
            self.frame_num -= 1;
            self.frame_read = false;
        }
    }
}
