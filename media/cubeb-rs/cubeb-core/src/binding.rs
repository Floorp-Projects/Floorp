// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#[doc(hidden)]
pub trait IsNullPtr {
    fn is_ptr_null(&self) -> bool;
}

impl<T> IsNullPtr for *const T {
    fn is_ptr_null(&self) -> bool {
        self.is_null()
    }
}

impl<T> IsNullPtr for *mut T {
    fn is_ptr_null(&self) -> bool {
        self.is_null()
    }
}

#[doc(hidden)]
pub trait Binding: Sized {
    type Raw;

    unsafe fn from_raw(raw: Self::Raw) -> Self;
    fn raw(&self) -> Self::Raw;

    unsafe fn from_raw_opt<T>(raw: T) -> Option<Self>
    where
        T: Copy + IsNullPtr,
        Self: Binding<Raw = T>,
    {
        if raw.is_ptr_null() {
            None
        } else {
            Some(Binding::from_raw(raw))
        }
    }
}
