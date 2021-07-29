/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use l10nregistry::source::FileFetcher;

use std::io;

pub struct GeckoFileFetcher;

fn try_string_from_box_u8(input: Box<[u8]>) -> io::Result<String> {
    String::from_utf8(input.into())
        .map_err(|err| io::Error::new(io::ErrorKind::InvalidData, err.utf8_error()))
}

#[async_trait::async_trait(?Send)]
impl FileFetcher for GeckoFileFetcher {
    fn fetch_sync(&self, path: &str) -> io::Result<String> {
        crate::load::load_sync(path).and_then(try_string_from_box_u8)
    }

    async fn fetch(&self, path: &str) -> io::Result<String> {
        crate::load::load_async(path)
            .await
            .and_then(try_string_from_box_u8)
    }
}

pub struct MockFileFetcher {
    fs: Vec<(String, String)>,
}

impl MockFileFetcher {
    pub fn new(fs: Vec<(String, String)>) -> Self {
        Self { fs }
    }
}

#[async_trait::async_trait(?Send)]
impl FileFetcher for MockFileFetcher {
    fn fetch_sync(&self, path: &str) -> io::Result<String> {
        for (p, source) in &self.fs {
            if p == path {
                return Ok(source.clone());
            }
        }
        Err(io::Error::new(io::ErrorKind::NotFound, "File not found"))
    }

    async fn fetch(&self, path: &str) -> io::Result<String> {
        self.fetch_sync(path)
    }
}
