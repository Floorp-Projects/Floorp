/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use fluent::FluentResource;
use nsstring::nsACString;
use std::{
    mem::{self, ManuallyDrop},
    rc::Rc,
};

#[no_mangle]
pub extern "C" fn fluent_resource_new(
    name: &nsACString,
    has_errors: &mut bool,
) -> *const FluentResource {
    let res = match FluentResource::try_new(name.to_string()) {
        Ok(res) => {
            *has_errors = false;
            res
        }
        Err((res, _)) => {
            *has_errors = true;
            res
        }
    };
    Rc::into_raw(Rc::new(res))
}

#[no_mangle]
pub unsafe extern "C" fn fluent_resource_addref(res: *const FluentResource) {
    let raw = ManuallyDrop::new(Rc::from_raw(res));
    mem::forget(Rc::clone(&raw));
}

#[no_mangle]
pub unsafe extern "C" fn fluent_resource_release(res: *const FluentResource) {
    let _ = Rc::from_raw(res);
}
