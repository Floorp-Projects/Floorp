// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

//! `Encoder`s and `Decoder`s from items to/from `BytesMut` buffers.

use bincode::{self, Bounded, deserialize, serialize_into, serialized_size};
use bytes::{Buf, BufMut, BytesMut, LittleEndian};
use serde::de::DeserializeOwned;
use serde::ser::Serialize;
use std::io as std_io;
use std::io::Cursor;
use std::mem;

////////////////////////////////////////////////////////////////////////////////
// Split buffer into size delimited frames - This appears more complicated than
// might be necessary due to handling the possibility of messages being split
// across reads.

#[derive(Debug)]
enum FrameState {
    Head,
    Data(usize)
}

#[derive(Debug)]
pub struct Decoder {
    state: FrameState
}

impl Decoder {
    pub fn new() -> Self {
        Decoder {
            state: FrameState::Head
        }
    }

    fn decode_head(&mut self, src: &mut BytesMut) -> std_io::Result<Option<usize>> {
        let head_size = mem::size_of::<u16>();
        if src.len() < head_size {
            // Not enough data
            return Ok(None);
        }

        let n = {
            let mut src = Cursor::new(&mut *src);

            // match endianess
            let n = src.get_uint::<LittleEndian>(head_size);

            if n > u64::from(u16::max_value()) {
                return Err(std_io::Error::new(
                    std_io::ErrorKind::InvalidData,
                    "frame size too big"
                ));
            }

            // The check above ensures there is no overflow
            n as usize
        };

        // Consume the length field
        let _ = src.split_to(head_size);

        Ok(Some(n))
    }

    fn decode_data(&self, n: usize, src: &mut BytesMut) -> std_io::Result<Option<BytesMut>> {
        // At this point, the buffer has already had the required capacity
        // reserved. All there is to do is read.
        if src.len() < n {
            return Ok(None);
        }

        Ok(Some(src.split_to(n)))
    }

    pub fn split_frame(&mut self, src: &mut BytesMut) -> std_io::Result<Option<BytesMut>> {
        let n = match self.state {
            FrameState::Head => {
                match try!(self.decode_head(src)) {
                    Some(n) => {
                        self.state = FrameState::Data(n);

                        // Ensure that the buffer has enough space to read the
                        // incoming payload
                        src.reserve(n);

                        n
                    },
                    None => return Ok(None),
                }
            },
            FrameState::Data(n) => n,
        };

        match try!(self.decode_data(n, src)) {
            Some(data) => {
                // Update the decode state
                self.state = FrameState::Head;

                // Make sure the buffer has enough space to read the next head
                src.reserve(mem::size_of::<u16>());

                Ok(Some(data))
            },
            None => Ok(None),
        }
    }

    pub fn decode<ITEM: DeserializeOwned>(&mut self, src: &mut BytesMut) -> Result<Option<ITEM>, bincode::Error> {
        match try!(self.split_frame(src)) {
            Some(buf) => deserialize::<ITEM>(buf.as_ref()).and_then(|t| Ok(Some(t))),
            None => Ok(None),
        }
    }
}

impl Default for Decoder {
    fn default() -> Self {
        Self::new()
    }
}

pub fn encode<ITEM: Serialize>(dst: &mut BytesMut, item: &ITEM) -> Result<(), bincode::Error> {
    let head_len = mem::size_of::<u16>() as u64;
    let item_len = serialized_size(item);

    if head_len + item_len > u64::from(u16::max_value()) {
        return Err(Box::new(bincode::ErrorKind::IoError(std_io::Error::new(
            std_io::ErrorKind::InvalidInput,
            "frame too big"
        ))));
    }

    let n = (head_len + item_len) as usize;
    dst.reserve(n);
    dst.put_u16::<LittleEndian>(item_len as u16);
    serialize_into(&mut dst.writer(), item, Bounded(item_len))
}
