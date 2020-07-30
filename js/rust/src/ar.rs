/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use jsapi::root::*;

#[derive(Debug)]
pub struct AutoRealm(JSAutoRealm);

impl AutoRealm {
    pub unsafe fn with_obj(cx: *mut JSContext, target: *mut JSObject) -> AutoRealm {
        AutoRealm(JSAutoRealm::new(cx, target))
    }

    pub unsafe fn with_script(cx: *mut JSContext, target: *mut JSScript) -> AutoRealm {
        AutoRealm(JSAutoRealm::new1(cx, target))
    }
}
