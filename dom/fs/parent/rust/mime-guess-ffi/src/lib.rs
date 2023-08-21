/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate mime_guess;
extern crate nserror;
extern crate nsstring;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG, NS_ERROR_NOT_AVAILABLE, NS_OK};
use nsstring::{nsACString, nsCString};
use std::path::Path;
use std::str;

#[no_mangle]
pub extern "C" fn mimeGuessFromPath(path: &nsACString, content_type: &mut nsCString) -> nsresult {
    let path_data = str::from_utf8(path.as_ref());
    if path_data.is_err() {
        return NS_ERROR_INVALID_ARG; // Not UTF8
    }

    let content_path = Path::new(path_data.unwrap());
    if content_path.extension().is_none() {
        return NS_ERROR_NOT_AVAILABLE; // No mime type information
    }

    let maybe_mime = mime_guess::from_path(content_path).first_raw();
    if maybe_mime.is_none() {
        return NS_ERROR_FAILURE; // Not recognized
    }

    content_type.assign(maybe_mime.unwrap());

    NS_OK
}
