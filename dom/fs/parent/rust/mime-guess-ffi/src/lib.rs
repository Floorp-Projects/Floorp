/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate mime_guess;
extern crate nsstring;

use nsstring::{nsACString, nsCString};
use std::path::Path;
use std::str;

#[no_mangle]
pub extern "C" fn mimeGuessFromPath(path: &nsACString, content_type: &mut nsCString) {
    let path_data = str::from_utf8(path.as_ref());
    if path_data.is_err() {
        return; // Not UTF8
    }

    let maybe_mime = mime_guess::from_path(Path::new(path_data.unwrap())).first_raw();
    if maybe_mime.is_none() {
        return; // Not recognized
    }

    content_type.assign(maybe_mime.unwrap());
}
