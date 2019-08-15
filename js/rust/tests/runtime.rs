/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate js;
extern crate libc;

use js::jsapi::*;
use js::rust::{Runtime, SIMPLE_GLOBAL_CLASS};
use std::ptr;

#[test]
fn runtime() {
    unsafe {
        let runtime = Runtime::new(false).unwrap();

        let cx = runtime.cx();
        let h_option = JS::OnNewGlobalHookOption::FireOnNewGlobalHook;
        let c_option = JS::RealmOptions::default();

        rooted!(in(cx) let global = JS_NewGlobalObject(cx,
                                                       &SIMPLE_GLOBAL_CLASS,
                                                       ptr::null_mut(),
                                                       h_option,
                                                       &c_option));
        let _ar = js::ar::AutoRealm::with_obj(cx, global.get());
        rooted!(in(cx) let _object = JS_NewObject(cx, &CLASS as *const _));
    }

    assert!(Runtime::new(false).is_err());
}

unsafe extern fn finalize(_fop: *mut JSFreeOp, _object: *mut JSObject) {
    assert!(!Runtime::get().is_null());
}

static CLASS_OPS: JSClassOps = JSClassOps {
    addProperty: None,
    delProperty: None,
    enumerate: None,
    newEnumerate: None,
    resolve: None,
    mayResolve: None,
    finalize: Some(finalize),
    call: None,
    hasInstance: None,
    construct: None,
    trace: None,
};

static CLASS: JSClass = JSClass {
    name: b"EventTargetPrototype\0" as *const u8 as *const libc::c_char,
    flags: 0 | JSCLASS_FOREGROUND_FINALIZE,
    cOps: &CLASS_OPS as *const JSClassOps,
    spec: 0 as *mut _,
    ext: 0 as *mut _,
    oOps: 0 as *mut _
};
