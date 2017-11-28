/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate js;

use js::jsapi::*;
use js::jsval::UndefinedValue;
use js::rust::Runtime as Runtime_;
use js::rust::SIMPLE_GLOBAL_CLASS;
use js::typedarray::{CreateWith, Uint32Array};
use std::ptr;

#[test]
fn typedarray() {
    let rt = Runtime_::new(false).unwrap();
    let cx = rt.cx();

    unsafe {
        rooted!(in(cx) let global =
            JS_NewGlobalObject(cx, &SIMPLE_GLOBAL_CLASS, ptr::null_mut(),
                               JS::OnNewGlobalHookOption::FireOnNewGlobalHook,
                               &JS::CompartmentOptions::default())
        );

        let _ac = js::ac::AutoCompartment::with_obj(cx, global.get());

        rooted!(in(cx) let mut rval = UndefinedValue());
        assert!(rt.evaluate_script(global.handle(), "new Uint8Array([0, 2, 4])",
                                   "test", 1, rval.handle_mut()).is_ok());
        assert!(rval.is_object());

        typedarray!(in(cx) let array: Uint8Array = rval.to_object());
        assert_eq!(array.unwrap().as_slice(), &[0, 2, 4][..]);

        typedarray!(in(cx) let array: Uint16Array = rval.to_object());
        assert!(array.is_err());

        typedarray!(in(cx) let view: ArrayBufferView = rval.to_object());
        assert_eq!(view.unwrap().get_array_type(), js::jsapi::js::Scalar::Type::Uint8);

        rooted!(in(cx) let mut rval = ptr::null_mut());
        assert!(Uint32Array::create(cx, CreateWith::Slice(&[1, 3, 5]), rval.handle_mut()).is_ok());

        typedarray!(in(cx) let array: Uint32Array = rval.get());
        assert_eq!(array.unwrap().as_slice(), &[1, 3, 5][..]);

        typedarray!(in(cx) let mut array: Uint32Array = rval.get());
        array.as_mut().unwrap().update(&[2, 4, 6]);
        assert_eq!(array.unwrap().as_slice(), &[2, 4, 6][..]);

        rooted!(in(cx) let rval = ptr::null_mut());
        typedarray!(in(cx) let array: Uint8Array = rval.get());
        assert!(array.is_err());

        rooted!(in(cx) let mut rval = ptr::null_mut());
        assert!(Uint32Array::create(cx, CreateWith::Length(5), rval.handle_mut()).is_ok());

        typedarray!(in(cx) let array: Uint32Array = rval.get());
        assert_eq!(array.unwrap().as_slice(), &[0, 0, 0, 0, 0]);

        typedarray!(in(cx) let mut array: Uint32Array = rval.get());
        array.as_mut().unwrap().update(&[0, 1, 2, 3]);
        assert_eq!(array.unwrap().as_slice(), &[0, 1, 2, 3, 0]);

        typedarray!(in(cx) let view: ArrayBufferView = rval.get());
        assert_eq!(view.unwrap().get_array_type(), js::jsapi::js::Scalar::Type::Uint32);
    }
}

#[test]
#[should_panic]
fn typedarray_update_panic() {
    let rt = Runtime_::new(false).unwrap();
    let cx = rt.cx();

    unsafe {
        rooted!(in(cx) let global =
            JS_NewGlobalObject(cx, &SIMPLE_GLOBAL_CLASS, ptr::null_mut(),
                               JS::OnNewGlobalHookOption::FireOnNewGlobalHook,
                               &JS::CompartmentOptions::default())
        );

        let _ac = js::ac::AutoCompartment::with_obj(cx, global.get());
        rooted!(in(cx) let mut rval = ptr::null_mut());
        let _ = Uint32Array::create(cx, CreateWith::Slice(&[1, 2, 3, 4, 5]), rval.handle_mut());
        typedarray!(in(cx) let mut array: Uint32Array = rval.get());
        array.as_mut().unwrap().update(&[0, 2, 4, 6, 8, 10]);
    }
}
