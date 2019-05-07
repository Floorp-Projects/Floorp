/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::api::{Epoch, PipelineId};
use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};
use std::io::{Cursor, Read};
use std::mem;
use std::sync::mpsc::Receiver;

#[derive(Clone)]
pub struct Payload {
    /// An epoch used to get the proper payload for a pipeline id frame request.
    ///
    /// TODO(emilio): Is this still relevant? We send the messages for the same
    /// pipeline in order, so we shouldn't need it. Seems like this was only
    /// wallpapering (in most cases) the underlying problem in #991.
    pub epoch: Epoch,
    /// A pipeline id to key the payload with, along with the epoch.
    pub pipeline_id: PipelineId,
    pub display_list_data: Vec<u8>,
}

impl Payload {
    /// Convert the payload to a raw byte vector, in order for it to be
    /// efficiently shared via shmem, for example.
    /// This is a helper static method working on a slice.
    pub fn construct_data(epoch: Epoch, pipeline_id: PipelineId, dl_data: &[u8]) -> Vec<u8> {
        let mut data = Vec::with_capacity(
            mem::size_of::<u32>() + 2 * mem::size_of::<u32>() + mem::size_of::<u64>() + dl_data.len(),
        );
        data.write_u32::<LittleEndian>(epoch.0).unwrap();
        data.write_u32::<LittleEndian>(pipeline_id.0).unwrap();
        data.write_u32::<LittleEndian>(pipeline_id.1).unwrap();
        data.write_u64::<LittleEndian>(dl_data.len() as u64)
            .unwrap();
        data.extend_from_slice(dl_data);
        data
    }
    /// Convert the payload to a raw byte vector, in order for it to be
    /// efficiently shared via shmem, for example.
    ///
    /// TODO(emilio, #1049): Consider moving the IPC boundary to the
    /// constellation in Servo and remove this complexity from WR.
    pub fn to_data(&self) -> Vec<u8> {
        Self::construct_data(self.epoch, self.pipeline_id, &self.display_list_data)
    }

    /// Deserializes the given payload from a raw byte vector.
    pub fn from_data(data: &[u8]) -> Payload {
        let mut payload_reader = Cursor::new(data);
        let epoch = Epoch(payload_reader.read_u32::<LittleEndian>().unwrap());
        let pipeline_id = PipelineId(
            payload_reader.read_u32::<LittleEndian>().unwrap(),
            payload_reader.read_u32::<LittleEndian>().unwrap(),
        );

        let dl_size = payload_reader.read_u64::<LittleEndian>().unwrap() as usize;
        let mut built_display_list_data = vec![0; dl_size];
        payload_reader
            .read_exact(&mut built_display_list_data[..])
            .unwrap();

        assert_eq!(payload_reader.position(), data.len() as u64);

        Payload {
            epoch,
            pipeline_id,
            display_list_data: built_display_list_data,
        }
    }
}


/// A helper to handle the interface difference between `IpcBytesSender`
/// and `Sender<Vec<u8>>`.
pub trait PayloadSenderHelperMethods {
    fn send_payload(&self, data: Payload) -> Result<(), Error>;
}

pub trait PayloadReceiverHelperMethods {
    fn recv_payload(&self) -> Result<Payload, Error>;

    // For an MPSC receiver, this is the identity function,
    // for an IPC receiver, it routes to a new mpsc receiver
    fn to_mpsc_receiver(self) -> Receiver<Payload>;
}

#[cfg(not(feature = "ipc"))]
include!("channel_mpsc.rs");

#[cfg(feature = "ipc")]
include!("channel_ipc.rs");
