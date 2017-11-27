/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate js;

use js::glue::RUST_JSID_IS_STRING;
use js::glue::RUST_JSID_TO_STRING;
use js::jsapi::root::JS::CompartmentOptions;
use js::jsapi::root::js::GetPropertyKeys;
use js::jsapi::root::JSITER_OWNONLY;
use js::jsapi::root::JS_NewGlobalObject;
use js::jsapi::root::JS_StringEqualsAscii;
use js::jsapi::root::JS::OnNewGlobalHookOption;
use js::jsval::UndefinedValue;
use js::rust::IdVector;
use js::rust::Runtime;
use js::rust::SIMPLE_GLOBAL_CLASS;
use std::ptr;

#[test]
fn enumerate() {
    let rt = Runtime::new(false).unwrap();
    let cx = rt.cx();

    unsafe {
        rooted!(in(cx) let global =
            JS_NewGlobalObject(cx, &SIMPLE_GLOBAL_CLASS, ptr::null_mut(),
                               OnNewGlobalHookOption::FireOnNewGlobalHook,
                               &CompartmentOptions::default())
        );

        rooted!(in(cx) let mut rval = UndefinedValue());
        assert!(rt.evaluate_script(global.handle(), "({ 'a': 7 })",
                                   "test", 1, rval.handle_mut()).is_ok());
        assert!(rval.is_object());

        rooted!(in(cx) let object = rval.to_object());
        let ids = IdVector::new(cx);
        assert!(GetPropertyKeys(cx, object.handle(), JSITER_OWNONLY, ids.get()));

        assert_eq!(ids.len(), 1);
        rooted!(in(cx) let id = ids[0]);

        assert!(RUST_JSID_IS_STRING(id.handle()));
        rooted!(in(cx) let id = RUST_JSID_TO_STRING(id.handle()));

        let mut matches = false;
        assert!(JS_StringEqualsAscii(cx,
                                     id.get(),
                                     b"a\0" as *const _ as *const _,
                                     &mut matches));
        assert!(matches);
    }
}
