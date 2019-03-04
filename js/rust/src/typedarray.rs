/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! High-level, safe bindings for JS typed array APIs. Allows creating new
//! typed arrays or wrapping existing JS reflectors, and prevents reinterpreting
//! existing buffers as different types except in well-defined cases.

use glue::GetFloat32ArrayLengthAndData;
use glue::GetFloat64ArrayLengthAndData;
use glue::GetInt16ArrayLengthAndData;
use glue::GetInt32ArrayLengthAndData;
use glue::GetInt8ArrayLengthAndData;
use glue::GetUint16ArrayLengthAndData;
use glue::GetUint32ArrayLengthAndData;
use glue::GetUint8ArrayLengthAndData;
use glue::GetUint8ClampedArrayLengthAndData;
use jsapi::*;
use jsapi::js::*;
use jsapi::JS::*;
use rust::RootedGuard;
use std::ptr;
use std::slice;

pub enum CreateWith<'a, T: 'a> {
    Length(u32),
    Slice(&'a [T]),
}

/// A rooted typed array.
pub struct TypedArray<'a, T: 'a + TypedArrayElement> {
    object: RootedGuard<'a, *mut JSObject>,
    computed: Option<(*mut T::Element, u32)>,
}

impl<'a, T: TypedArrayElement> TypedArray<'a, T> {
    /// Create a typed array representation that wraps an existing JS reflector.
    /// This operation will fail if attempted on a JS object that does not match
    /// the expected typed array details.
    pub fn from(cx: *mut JSContext,
                root: &'a mut Rooted<*mut JSObject>,
                object: *mut JSObject)
                -> Result<Self, ()> {
        if object.is_null() {
            return Err(());
        }
        unsafe {
            let mut guard = RootedGuard::new(cx, root, object);
            let unwrapped = T::unwrap_array(*guard);
            if unwrapped.is_null() {
                return Err(());
            }

            *guard = unwrapped;
            Ok(TypedArray {
                object: guard,
                computed: None,
            })
        }
    }

    fn data(&mut self) -> (*mut T::Element, u32) {
        if let Some(data) = self.computed {
            return data;
        }

        let data = unsafe { T::length_and_data(*self.object) };
        self.computed = Some(data);
        data
    }

    /// # Unsafety
    ///
    /// The returned slice can be invalidated if the underlying typed array
    /// is neutered.
    pub unsafe fn as_slice(&mut self) -> &[T::Element] {
        let (pointer, length) = self.data();
        slice::from_raw_parts(pointer as *const T::Element, length as usize)
    }

    /// # Unsafety
    ///
    /// The returned slice can be invalidated if the underlying typed array
    /// is neutered.
    ///
    /// The underlying `JSObject` can be aliased, which can lead to
    /// Undefined Behavior due to mutable aliasing.
    pub unsafe fn as_mut_slice(&mut self) -> &mut [T::Element] {
        let (pointer, length) = self.data();
        slice::from_raw_parts_mut(pointer, length as usize)
    }
}

impl<'a, T: TypedArrayElementCreator + TypedArrayElement> TypedArray<'a, T> {
    /// Create a new JS typed array, optionally providing initial data that will
    /// be copied into the newly-allocated buffer. Returns the new JS reflector.
    pub unsafe fn create(cx: *mut JSContext,
                         with: CreateWith<T::Element>,
                         result: MutableHandleObject)
                         -> Result<(), ()> {
        let length = match with {
            CreateWith::Length(len) => len,
            CreateWith::Slice(slice) => slice.len() as u32,
        };

        result.set(T::create_new(cx, length));
        if result.get().is_null() {
            return Err(());
        }

        if let CreateWith::Slice(data) = with {
            TypedArray::<T>::update_raw(data, result.handle());
        }

        Ok(())
    }

    ///  Update an existed JS typed array
    pub unsafe fn update(&mut self, data: &[T::Element]) {
        TypedArray::<T>::update_raw(data, self.object.handle());
    }

    unsafe fn update_raw(data: &[T::Element], result: HandleObject) {
        let (buf, length) = T::length_and_data(result.get());
        assert!(data.len() <= length as usize);
        ptr::copy_nonoverlapping(data.as_ptr(), buf, data.len());
    }
}

/// Internal trait used to associate an element type with an underlying representation
/// and various functions required to manipulate typed arrays of that element type.
pub trait TypedArrayElement {
    /// Underlying primitive representation of this element type.
    type Element;
    /// Unwrap a typed array JS reflector for this element type.
    unsafe fn unwrap_array(obj: *mut JSObject) -> *mut JSObject;
    /// Retrieve the length and data of a typed array's buffer for this element type.
    unsafe fn length_and_data(obj: *mut JSObject) -> (*mut Self::Element, u32);
}

/// Internal trait for creating new typed arrays.
pub trait TypedArrayElementCreator: TypedArrayElement {
    /// Create a new typed array.
    unsafe fn create_new(cx: *mut JSContext, length: u32) -> *mut JSObject;
    /// Get the data.
    unsafe fn get_data(obj: *mut JSObject) -> *mut Self::Element;
}

macro_rules! typed_array_element {
    ($t: ident,
     $element: ty,
     $unwrap: ident,
     $length_and_data: ident) => (
        /// A kind of typed array.
        pub struct $t;

        impl TypedArrayElement for $t {
            type Element = $element;
            unsafe fn unwrap_array(obj: *mut JSObject) -> *mut JSObject {
                $unwrap(obj)
            }

            unsafe fn length_and_data(obj: *mut JSObject) -> (*mut Self::Element, u32) {
                let mut len = 0;
                let mut shared = false;
                let mut data = ptr::null_mut();
                $length_and_data(obj, &mut len, &mut shared, &mut data);
                assert!(!shared);
                (data, len)
            }
        }
    );

    ($t: ident,
     $element: ty,
     $unwrap: ident,
     $length_and_data: ident,
     $create_new: ident,
     $get_data: ident) => (
        typed_array_element!($t, $element, $unwrap, $length_and_data);

        impl TypedArrayElementCreator for $t {
            unsafe fn create_new(cx: *mut JSContext, length: u32) -> *mut JSObject {
                $create_new(cx, length)
            }

            unsafe fn get_data(obj: *mut JSObject) -> *mut Self::Element {
                let mut shared = false;
                let data = $get_data(obj, &mut shared, ptr::null_mut());
                assert!(!shared);
                data
            }
        }
    );
}

typed_array_element!(Uint8,
                     u8,
                     UnwrapUint8Array,
                     GetUint8ArrayLengthAndData,
                     JS_NewUint8Array,
                     JS_GetUint8ArrayData);
typed_array_element!(Uint16,
                     u16,
                     UnwrapUint16Array,
                     GetUint16ArrayLengthAndData,
                     JS_NewUint16Array,
                     JS_GetUint16ArrayData);
typed_array_element!(Uint32,
                     u32,
                     UnwrapUint32Array,
                     GetUint32ArrayLengthAndData,
                     JS_NewUint32Array,
                     JS_GetUint32ArrayData);
typed_array_element!(Int8,
                     i8,
                     UnwrapInt8Array,
                     GetInt8ArrayLengthAndData,
                     JS_NewInt8Array,
                     JS_GetInt8ArrayData);
typed_array_element!(Int16,
                     i16,
                     UnwrapInt16Array,
                     GetInt16ArrayLengthAndData,
                     JS_NewInt16Array,
                     JS_GetInt16ArrayData);
typed_array_element!(Int32,
                     i32,
                     UnwrapInt32Array,
                     GetInt32ArrayLengthAndData,
                     JS_NewInt32Array,
                     JS_GetInt32ArrayData);
typed_array_element!(Float32,
                     f32,
                     UnwrapFloat32Array,
                     GetFloat32ArrayLengthAndData,
                     JS_NewFloat32Array,
                     JS_GetFloat32ArrayData);
typed_array_element!(Float64,
                     f64,
                     UnwrapFloat64Array,
                     GetFloat64ArrayLengthAndData,
                     JS_NewFloat64Array,
                     JS_GetFloat64ArrayData);
typed_array_element!(ClampedU8,
                     u8,
                     UnwrapUint8ClampedArray,
                     GetUint8ClampedArrayLengthAndData,
                     JS_NewUint8ClampedArray,
                     JS_GetUint8ClampedArrayData);
typed_array_element!(ArrayBufferU8,
                     u8,
                     UnwrapArrayBuffer,
                     GetArrayBufferLengthAndData,
                     NewArrayBuffer,
                     GetArrayBufferData);
typed_array_element!(ArrayBufferViewU8,
                     u8,
                     UnwrapArrayBufferView,
                     GetArrayBufferViewLengthAndData);

/// The Uint8ClampedArray type.
pub type Uint8ClampedArray<'a> = TypedArray<'a, ClampedU8>;
/// The Uint8Array type.
pub type Uint8Array<'a> = TypedArray<'a, Uint8>;
/// The Int8Array type.
pub type Int8Array<'a> = TypedArray<'a, Int8>;
/// The Uint16Array type.
pub type Uint16Array<'a> = TypedArray<'a, Uint16>;
/// The Int16Array type.
pub type Int16Array<'a> = TypedArray<'a, Int16>;
/// The Uint32Array type.
pub type Uint32Array<'a> = TypedArray<'a, Uint32>;
/// The Int32Array type.
pub type Int32Array<'a> = TypedArray<'a, Int32>;
/// The Float32Array type.
pub type Float32Array<'a> = TypedArray<'a, Float32>;
/// The Float64Array type.
pub type Float64Array<'a> = TypedArray<'a, Float64>;
/// The ArrayBuffer type.
pub type ArrayBuffer<'a> = TypedArray<'a, ArrayBufferU8>;
/// The ArrayBufferView type
pub type ArrayBufferView<'a> = TypedArray<'a, ArrayBufferViewU8>;

impl<'a> ArrayBufferView<'a> {
    pub fn get_array_type(&self) -> Scalar::Type {
        unsafe { JS_GetArrayBufferViewType(self.object.get()) }
    }
}

#[macro_export]
macro_rules! typedarray {
    (in($cx:expr) let $name:ident : $ty:ident = $init:expr) => {
        let mut __root = $crate::jsapi::JS::Rooted::new_unrooted();
        let $name = $crate::typedarray::$ty::from($cx, &mut __root, $init);
    };
    (in($cx:expr) let mut $name:ident : $ty:ident = $init:expr) => {
        let mut __root = $crate::jsapi::JS::Rooted::new_unrooted();
        let mut $name = $crate::typedarray::$ty::from($cx, &mut __root, $init);
    }
}
