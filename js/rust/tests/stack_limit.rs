/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate js;

use js::jsapi::root::JS::RealmOptions;
use js::jsapi::root::JS_NewGlobalObject;
use js::jsapi::root::JS::OnNewGlobalHookOption;
use js::jsval::UndefinedValue;
use js::rust::{Runtime, SIMPLE_GLOBAL_CLASS};

use std::ptr;

#[test]
fn stack_limit() {
    let rt = Runtime::new(false).unwrap();
    let cx = rt.cx();

    unsafe {
        let h_option = OnNewGlobalHookOption::FireOnNewGlobalHook;
        let c_option = RealmOptions::default();
        let global = JS_NewGlobalObject(cx, &SIMPLE_GLOBAL_CLASS,
                                        ptr::null_mut(), h_option, &c_option);
        rooted!(in(cx) let global_root = global);
        let global = global_root.handle();
        rooted!(in(cx) let mut rval = UndefinedValue());
        assert!(rt.evaluate_script(global, "function f() { f.apply() } f()",
                                   "test", 1, rval.handle_mut()).is_err());
    }
}
