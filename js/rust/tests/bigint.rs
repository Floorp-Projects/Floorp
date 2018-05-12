#[macro_use]
extern crate js;

use js::jsapi::root::JS::CompartmentOptions;
use js::jsapi::root::JS_NewGlobalObject;
use js::jsapi::root::JS::OnNewGlobalHookOption;
use js::jsval::UndefinedValue;
use js::rust::{Runtime, SIMPLE_GLOBAL_CLASS};

use std::ptr;

#[test]
fn is_bigint() {
    let rt = Runtime::new(false).unwrap();
    let cx = rt.cx();

    unsafe {
        rooted!(in(cx) let global =
            JS_NewGlobalObject(cx, &SIMPLE_GLOBAL_CLASS, ptr::null_mut(),
                               OnNewGlobalHookOption::FireOnNewGlobalHook,
                               &CompartmentOptions::default())
        );

        rooted!(in(cx) let mut rval = UndefinedValue());
        assert!(rt.evaluate_script(global.handle(), "BigInt(0)",
                                   "test", 1, rval.handle_mut()).is_ok());
        assert!(rval.is_bigint());
    }
}

#[test]
fn is_not_bigint() {
    let rt = Runtime::new(false).unwrap();
    let cx = rt.cx();

    unsafe {
        rooted!(in(cx) let global =
            JS_NewGlobalObject(cx, &SIMPLE_GLOBAL_CLASS, ptr::null_mut(),
                               OnNewGlobalHookOption::FireOnNewGlobalHook,
                               &CompartmentOptions::default())
        );

        rooted!(in(cx) let mut rval = UndefinedValue());
        assert!(rt.evaluate_script(global.handle(), "'not a BigInt'",
                                   "test", 1, rval.handle_mut()).is_ok());
        assert!(!rval.is_bigint());
    }
}
