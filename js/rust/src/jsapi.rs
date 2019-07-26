/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(feature = "debugmozjs")]
include!(concat!(env!("OUT_DIR"), "/jsapi_debug.rs"));

#[cfg(not(feature = "debugmozjs"))]
include!(concat!(env!("OUT_DIR"), "/jsapi.rs"));
