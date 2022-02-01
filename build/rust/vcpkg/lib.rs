/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::path::PathBuf;

pub struct Config;

pub struct Library {
    pub include_paths: Vec<PathBuf>,
}

pub struct Error;

impl Config {
    pub fn new() -> Config {
        Config
    }

    pub fn probe(&mut self, _: &str) -> Result<Library, Error> {
        Err(Error)
    }
}
