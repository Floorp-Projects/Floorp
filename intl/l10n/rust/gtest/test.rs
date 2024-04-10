/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use l10nregistry_ffi::load::{load_async, load_sync};
use std::borrow::Borrow;

// We want to test a file that ships in every platform configuration, so we take
// something from `toolkit/`.  But we don't want to depend on the specifics of
// the text, or the packaging of that text, since those can change.  It would be
// best to register an untranslated `.ftl` for this test, but that's difficult.
// Second best is to ship an untranslated `.ftl`, but that is not well-supported
// by existing processes either.  So we settle for depending on the form of
// specific identifiers, whose names will appear in future searches, while
// depending on the specific messages or the file packaging.
fn assert_about_about_correct<T: Borrow<[u8]>> (res: T) {
    assert!(res.borrow().len() > 0);

    // `windows` is a convenient, if inefficient, way to look for a subslice.
    let needle = b"about-about-title";
    assert!(res.borrow().windows(needle.len()).position(|window| window == needle).is_some());

    let needle = b"about-about-note";
    assert!(res.borrow().windows(needle.len()).position(|window| window == needle).is_some());
}

#[no_mangle]
pub extern "C" fn Rust_L10NLoadAsync(it_worked: *mut bool) {
    let future = async move {
        match load_async("resource://gre/localization/en-US/toolkit/about/aboutAbout.ftl").await {
            Ok(res) => {
                assert_about_about_correct(res);
                unsafe {
                    *it_worked = true;
                }
            }
            Err(err) => panic!("{:?}", err),
        }
    };

    unsafe {
        moz_task::gtest_only::spin_event_loop_until("Rust_L10NLoadAsync", future).unwrap();
    }
}

#[no_mangle]
pub extern "C" fn Rust_L10NLoadSync(it_worked: *mut bool) {
    match load_sync("resource://gre/localization/en-US/toolkit/about/aboutAbout.ftl") {
        Ok(res) => {
            assert_about_about_correct(res);
            unsafe {
                *it_worked = true;
            }
        }
        Err(err) => panic!("{:?}", err),
    }
}
