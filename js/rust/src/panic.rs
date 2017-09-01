/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::any::Any;
use std::cell::RefCell;
use std::panic::{UnwindSafe, catch_unwind, resume_unwind};

thread_local!(static PANIC_RESULT: RefCell<Option<Box<Any + Send>>> = RefCell::new(None));

/// If there is a pending panic, resume unwinding.
pub fn maybe_resume_unwind() {
    if let Some(error) = PANIC_RESULT.with(|result| result.borrow_mut().take()) {
        resume_unwind(error);
    }
}

/// Generic wrapper for JS engine callbacks panic-catching
pub fn wrap_panic<F, R>(function: F, generic_return_type: R) -> R
    where F: FnOnce() -> R + UnwindSafe
{
    let result = catch_unwind(function);
    match result {
        Ok(result) => result,
        Err(error) => {
            PANIC_RESULT.with(|result| {
                assert!(result.borrow().is_none());
                *result.borrow_mut() = Some(error);
            });
            generic_return_type
        }
    }
}
