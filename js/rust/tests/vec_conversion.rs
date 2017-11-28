/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate js;

use js::ac::AutoCompartment;
use js::conversions::ConversionBehavior;
use js::conversions::ConversionResult;
use js::conversions::FromJSValConvertible;
use js::conversions::ToJSValConvertible;
use js::jsapi::root::JS::CompartmentOptions;
use js::jsapi::root::JS_InitStandardClasses;
use js::jsapi::root::JS_NewGlobalObject;
use js::jsapi::root::JS::OnNewGlobalHookOption;
use js::jsval::UndefinedValue;
use js::rust::{Runtime, SIMPLE_GLOBAL_CLASS};

use std::ptr;

fn assert_is_array(cx: *mut js::jsapi::root::JSContext,
                   val: js::jsapi::root::JS::HandleValue) {
    let mut is_array = false;
    assert!(unsafe {
        js::jsapi::root::JS_IsArrayObject(cx, val, &mut is_array as *mut _)
    });
    assert!(is_array);
}

#[test]
fn vec_conversion() {
    let rt = Runtime::new(false).unwrap();
    let cx = rt.cx();

    let h_option = OnNewGlobalHookOption::FireOnNewGlobalHook;
    let c_option = CompartmentOptions::default();

    unsafe {
        let global = JS_NewGlobalObject(cx, &SIMPLE_GLOBAL_CLASS,
                                        ptr::null_mut(), h_option, &c_option);
        rooted!(in(cx) let global_root = global);
        let global = global_root.handle();

        let _ac = AutoCompartment::with_obj(cx, global.get());
        assert!(JS_InitStandardClasses(cx, global));

        rooted!(in(cx) let mut rval = UndefinedValue());

        let orig_vec: Vec<f32> = vec![1.0, 2.9, 3.0];
        orig_vec.to_jsval(cx, rval.handle_mut());
        assert_is_array(cx, rval.handle());
        let converted = Vec::<f32>::from_jsval(cx, rval.handle(), ()).unwrap();
        assert_eq!(&orig_vec, converted.get_success_value().unwrap());

        let orig_vec: Vec<i32> = vec![1, 2, 3];
        orig_vec.to_jsval(cx, rval.handle_mut());
        assert_is_array(cx, rval.handle());
        let converted = Vec::<i32>::from_jsval(cx, rval.handle(),
                                               ConversionBehavior::Default).unwrap();

        assert_eq!(&orig_vec, converted.get_success_value().unwrap());

        rt.evaluate_script(global, "new Set([1, 2, 3])",
                           "test", 1, rval.handle_mut()).unwrap();
        let converted =
          Vec::<i32>::from_jsval(cx, rval.handle(),
                                 ConversionBehavior::Default).unwrap();

        assert_eq!(&orig_vec, converted.get_success_value().unwrap());

        rt.evaluate_script(global, "({})", "test", 1, rval.handle_mut()).unwrap();
        let converted = Vec::<i32>::from_jsval(cx, rval.handle(),
                                               ConversionBehavior::Default);
        assert!(match converted {
            Ok(ConversionResult::Failure(_)) => true,
            _ => false,
        });
    }
}
