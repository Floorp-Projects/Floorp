// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details
#![allow(dead_code)] // TODO: Remove.

#![recursion_limit = "1024"]
#[macro_use]
extern crate error_chain;

#[macro_use]
extern crate log;

#[macro_use]
extern crate serde_derive;
extern crate serde;
extern crate bincode;

extern crate mio;

extern crate cubeb_core;

extern crate libc;
extern crate byteorder;

extern crate memmap;

mod connection;
pub mod errors;
pub mod messages;
mod msg;
pub mod shm;

pub use connection::*;
pub use messages::{ClientMessage, ServerMessage};
use std::env::temp_dir;
use std::path::PathBuf;

fn get_temp_path(name: &str) -> PathBuf {
    let mut path = temp_dir();
    path.push(name);
    path
}

pub fn get_uds_path() -> PathBuf {
    get_temp_path("cubeb-sock")
}

pub fn get_shm_path(dir: &str) -> PathBuf {
    get_temp_path(&format!("cubeb-shm-{}", dir))
}
