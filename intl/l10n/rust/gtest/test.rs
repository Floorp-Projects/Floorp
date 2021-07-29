/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use l10nregistry_ffi::{load_async, load_sync};
use moz_task;
use std::{
    sync::atomic::{AtomicBool, Ordering::Relaxed},
    sync::Arc,
};

#[no_mangle]
pub extern "C" fn Rust_L10NLoadAsync(it_worked: *mut bool) {
    let done = Arc::new(AtomicBool::new(false));
    let done2 = done.clone();

    moz_task::spawn_current_thread(async move {
        match load_async("resource://gre/localization/en-US/toolkit/about/aboutAbout.ftl").await {
            Ok(res) => {
                assert_eq!(res.len(), 460);
                assert!(res.starts_with(
                    b"# This Source Code Form is subject to the terms of the Mozilla Public"
                ));
                unsafe {
                    *it_worked = true;
                }
            }
            Err(err) => println!("{:?}", err),
        }

        done.store(true, Relaxed);
    })
    .unwrap();

    unsafe {
        moz_task::gtest_only::spin_event_loop_until(move || done2.load(Relaxed)).unwrap();
        *it_worked = true;
    }
}

#[no_mangle]
pub extern "C" fn Rust_L10NLoadSync(it_worked: *mut bool) {
    match load_sync("resource://gre/localization/en-US/toolkit/about/aboutAbout.ftl") {
        Ok(res) => {
            assert_eq!(res.len(), 460);
            assert!(res.starts_with(
                b"# This Source Code Form is subject to the terms of the Mozilla Public"
            ));
            unsafe {
                *it_worked = true;
            }
        }
        Err(err) => println!("{:?}", err),
    }
}
