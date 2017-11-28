/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate js;

use js::jsapi::*;
use js::jsval::UndefinedValue;
use js::panic::wrap_panic;
use js::rust::{Runtime, SIMPLE_GLOBAL_CLASS};
use std::ptr;
use std::str;

#[test]
#[should_panic]
fn panic() {
    let runtime = Runtime::new(false).unwrap();
    let context = runtime.cx();
    let h_option = JS::OnNewGlobalHookOption::FireOnNewGlobalHook;
    let c_option = JS::CompartmentOptions::default();

    unsafe {
        let global = JS_NewGlobalObject(context, &SIMPLE_GLOBAL_CLASS,
                                        ptr::null_mut(), h_option, &c_option);
        rooted!(in(context) let global_root = global);
        let global = global_root.handle();
        let _ac = js::ac::AutoCompartment::with_obj(context, global.get());
        let function = JS_DefineFunction(context, global,
                                         b"test\0".as_ptr() as *const _,
                                         Some(test), 0, 0);
        assert!(!function.is_null());
        rooted!(in(context) let mut rval = UndefinedValue());
        let _ = runtime.evaluate_script(global, "test();", "test.js", 0,
                                        rval.handle_mut());
    }
}

unsafe extern "C" fn test(_cx: *mut JSContext, _argc: u32, _vp: *mut JS::Value) -> bool {
    wrap_panic(|| {
        panic!()
    }, false)
}
