/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Conversions of Rust values to and from `JSVal`.
//!
//! | IDL type                | Type                             |
//! |-------------------------|----------------------------------|
//! | any                     | `JSVal`                          |
//! | boolean                 | `bool`                           |
//! | byte                    | `i8`                             |
//! | octet                   | `u8`                             |
//! | short                   | `i16`                            |
//! | unsigned short          | `u16`                            |
//! | long                    | `i32`                            |
//! | unsigned long           | `u32`                            |
//! | long long               | `i64`                            |
//! | unsigned long long      | `u64`                            |
//! | unrestricted float      | `f32`                            |
//! | float                   | `Finite<f32>`                    |
//! | unrestricted double     | `f64`                            |
//! | double                  | `Finite<f64>`                    |
//! | USVString               | `String`                         |
//! | object                  | `*mut JSObject`                  |
//! | nullable types          | `Option<T>`                      |
//! | sequences               | `Vec<T>`                         |

#![deny(missing_docs)]

#[cfg(feature = "nonzero")]
use core::nonzero::NonZero;

use error::throw_type_error;
use glue::RUST_JS_NumberValue;
use heap::Heap;
use jsapi::root::*;
use jsval::{BooleanValue, Int32Value, NullValue, UInt32Value, UndefinedValue};
use jsval::{ObjectValue, ObjectOrNullValue, StringValue};
use rust::{ToBoolean, ToInt32, ToInt64, ToNumber, ToUint16, ToUint32, ToUint64};
use rust::{ToString, maybe_wrap_object_or_null_value, maybe_wrap_value};
use libc;
use num_traits::{Bounded, Zero};
use std::borrow::Cow;
use std::rc::Rc;
use std::{ptr, slice};

trait As<O>: Copy {
    fn cast(self) -> O;
}

macro_rules! impl_as {
    ($I:ty, $O:ty) => (
        impl As<$O> for $I {
            fn cast(self) -> $O {
                self as $O
            }
        }
    )
}

impl_as!(f64, u8);
impl_as!(f64, u16);
impl_as!(f64, u32);
impl_as!(f64, u64);
impl_as!(f64, i8);
impl_as!(f64, i16);
impl_as!(f64, i32);
impl_as!(f64, i64);

impl_as!(u8, f64);
impl_as!(u16, f64);
impl_as!(u32, f64);
impl_as!(u64, f64);
impl_as!(i8, f64);
impl_as!(i16, f64);
impl_as!(i32, f64);
impl_as!(i64, f64);

impl_as!(i32, i8);
impl_as!(i32, u8);
impl_as!(i32, i16);
impl_as!(u16, u16);
impl_as!(i32, i32);
impl_as!(u32, u32);
impl_as!(i64, i64);
impl_as!(u64, u64);

/// A trait to convert Rust types to `JSVal`s.
pub trait ToJSValConvertible {
    /// Convert `self` to a `JSVal`. JSAPI failure causes a panic.
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue);
}

/// An enum to better support enums through FromJSValConvertible::from_jsval.
#[derive(PartialEq, Eq, Clone, Debug)]
pub enum ConversionResult<T> {
    /// Everything went fine.
    Success(T),
    /// Conversion failed, without a pending exception.
    Failure(Cow<'static, str>),
}

impl<T> ConversionResult<T> {
    /// Map a function over the `Success` value.
    pub fn map<F, U>(self, mut f: F) -> ConversionResult<U>
    where
        F: FnMut(T) -> U
    {
        match self {
            ConversionResult::Success(t) => ConversionResult::Success(f(t)),
            ConversionResult::Failure(e) => ConversionResult::Failure(e),
        }
    }

    /// Returns Some(value) if it is `ConversionResult::Success`.
    pub fn get_success_value(&self) -> Option<&T> {
        match *self {
            ConversionResult::Success(ref v) => Some(v),
            _ => None,
        }
    }
}

/// A trait to convert `JSVal`s to Rust types.
pub trait FromJSValConvertible: Sized {
    /// Optional configurable behaviour switch; use () for no configuration.
    type Config;
    /// Convert `val` to type `Self`.
    /// Optional configuration of type `T` can be passed as the `option`
    /// argument.
    /// If it returns `Err(())`, a JSAPI exception is pending.
    /// If it returns `Ok(Failure(reason))`, there is no pending JSAPI exception.
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: Self::Config)
                         -> Result<ConversionResult<Self>, ()>;
}

/// Behavior for converting out-of-range integers.
#[derive(PartialEq, Eq, Clone)]
pub enum ConversionBehavior {
    /// Wrap into the integer's range.
    Default,
    /// Throw an exception.
    EnforceRange,
    /// Clamp into the integer's range.
    Clamp,
}

/// Use `T` with `ConversionBehavior::Default` but without requiring any
/// `Config` associated type.
pub struct Default<T>(pub T);

impl<T> FromJSValConvertible for Default<T>
where
    T: FromJSValConvertible<Config = ConversionBehavior>
{
    type Config = ();
    unsafe fn from_jsval(cx: *mut JSContext, val: JS::HandleValue, _: ())
                         -> Result<ConversionResult<Self>, ()> {
        T::from_jsval(cx, val, ConversionBehavior::Default).map(|conv| conv.map(Default))
    }
}

/// Use `T` with `ConversionBehavior::EnforceRange` but without requiring any
/// `Config` associated type.
pub struct EnforceRange<T>(pub T);

impl<T> FromJSValConvertible for EnforceRange<T>
    where
    T: FromJSValConvertible<Config = ConversionBehavior>
{
    type Config = ();
    unsafe fn from_jsval(cx: *mut JSContext, val: JS::HandleValue, _: ())
                         -> Result<ConversionResult<Self>, ()> {
        T::from_jsval(cx, val, ConversionBehavior::EnforceRange)
            .map(|conv| conv.map(EnforceRange))
    }
}

/// Use `T` with `ConversionBehavior::Clamp` but without requiring any `Config`
/// associated type.
pub struct Clamp<T>(pub T);

impl<T> FromJSValConvertible for Clamp<T>
    where
    T: FromJSValConvertible<Config = ConversionBehavior>
{
    type Config = ();
    unsafe fn from_jsval(cx: *mut JSContext, val: JS::HandleValue, _: ())
                         -> Result<ConversionResult<Self>, ()> {
        T::from_jsval(cx, val, ConversionBehavior::Clamp)
            .map(|conv| conv.map(Clamp))
    }
}

/// Try to cast the number to a smaller type, but
/// if it doesn't fit, it will return an error.
unsafe fn enforce_range<D>(cx: *mut JSContext, d: f64) -> Result<ConversionResult<D>, ()>
    where D: Bounded + As<f64>,
          f64: As<D>
{
    if d.is_infinite() {
        throw_type_error(cx, "value out of range in an EnforceRange argument");
        return Err(());
    }

    let rounded = d.round();
    if D::min_value().cast() <= rounded && rounded <= D::max_value().cast() {
        Ok(ConversionResult::Success(rounded.cast()))
    } else {
        throw_type_error(cx, "value out of range in an EnforceRange argument");
        Err(())
    }
}

/// Try to cast the number to a smaller type, but if it doesn't fit,
/// round it to the MAX or MIN of the source type before casting it to
/// the destination type.
fn clamp_to<D>(d: f64) -> D
    where D: Bounded + As<f64> + Zero,
          f64: As<D>
{
    if d.is_nan() {
        D::zero()
    } else if d > D::max_value().cast() {
        D::max_value()
    } else if d < D::min_value().cast() {
        D::min_value()
    } else {
        d.cast()
    }
}

// https://heycam.github.io/webidl/#es-void
impl ToJSValConvertible for () {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(UndefinedValue());
    }
}

impl FromJSValConvertible for JS::HandleValue {
    type Config = ();
    #[inline]
    unsafe fn from_jsval(cx: *mut JSContext,
                         value: JS::HandleValue,
                         _option: ())
                         -> Result<ConversionResult<JS::HandleValue>, ()> {
        if value.is_object() {
            js::AssertSameCompartment(cx, value.to_object());
        }
        Ok(ConversionResult::Success(value))
    }
}

impl FromJSValConvertible for JS::Value {
    type Config = ();
    unsafe fn from_jsval(_cx: *mut JSContext,
                         value: JS::HandleValue,
                         _option: ())
                         -> Result<ConversionResult<JS::Value>, ()> {
        Ok(ConversionResult::Success(value.get()))
    }
}

impl FromJSValConvertible for Heap<JS::Value> {
    type Config = ();
    unsafe fn from_jsval(_cx: *mut JSContext,
                         value: JS::HandleValue,
                         _option: ())
                         -> Result<ConversionResult<Self>, ()> {
        Ok(ConversionResult::Success(Heap::<JS::Value>::new(value.get())))
    }
}

impl ToJSValConvertible for JS::Value {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(*self);
        maybe_wrap_value(cx, rval);
    }
}

impl ToJSValConvertible for JS::HandleValue {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(self.get());
        maybe_wrap_value(cx, rval);
    }
}

impl ToJSValConvertible for Heap<JS::Value> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(self.get());
        maybe_wrap_value(cx, rval);
    }
}

#[inline]
unsafe fn convert_int_from_jsval<T, M>(cx: *mut JSContext, value: JS::HandleValue,
                                       option: ConversionBehavior,
                                       convert_fn: unsafe fn(*mut JSContext, JS::HandleValue) -> Result<M, ()>)
                                       -> Result<ConversionResult<T>, ()>
    where T: Bounded + Zero + As<f64>,
          M: Zero + As<T>,
          f64: As<T>
{
    match option {
        ConversionBehavior::Default => Ok(ConversionResult::Success(try!(convert_fn(cx, value)).cast())),
        ConversionBehavior::EnforceRange => enforce_range(cx, try!(ToNumber(cx, value))),
        ConversionBehavior::Clamp => Ok(ConversionResult::Success(clamp_to(try!(ToNumber(cx, value))))),
    }
}

// https://heycam.github.io/webidl/#es-boolean
impl ToJSValConvertible for bool {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(BooleanValue(*self));
    }
}

// https://heycam.github.io/webidl/#es-boolean
impl FromJSValConvertible for bool {
    type Config = ();
    unsafe fn from_jsval(_cx: *mut JSContext, val: JS::HandleValue, _option: ()) -> Result<ConversionResult<bool>, ()> {
        Ok(ToBoolean(val)).map(ConversionResult::Success)
    }
}

// https://heycam.github.io/webidl/#es-byte
impl ToJSValConvertible for i8 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(Int32Value(*self as i32));
    }
}

// https://heycam.github.io/webidl/#es-byte
impl FromJSValConvertible for i8 {
    type Config = ConversionBehavior;
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: ConversionBehavior)
                         -> Result<ConversionResult<i8>, ()> {
        convert_int_from_jsval(cx, val, option, ToInt32)
    }
}

// https://heycam.github.io/webidl/#es-octet
impl ToJSValConvertible for u8 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(Int32Value(*self as i32));
    }
}

// https://heycam.github.io/webidl/#es-octet
impl FromJSValConvertible for u8 {
    type Config = ConversionBehavior;
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: ConversionBehavior)
                         -> Result<ConversionResult<u8>, ()> {
        convert_int_from_jsval(cx, val, option, ToInt32)
    }
}

// https://heycam.github.io/webidl/#es-short
impl ToJSValConvertible for i16 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(Int32Value(*self as i32));
    }
}

// https://heycam.github.io/webidl/#es-short
impl FromJSValConvertible for i16 {
    type Config = ConversionBehavior;
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: ConversionBehavior)
                         -> Result<ConversionResult<i16>, ()> {
        convert_int_from_jsval(cx, val, option, ToInt32)
    }
}

// https://heycam.github.io/webidl/#es-unsigned-short
impl ToJSValConvertible for u16 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(Int32Value(*self as i32));
    }
}

// https://heycam.github.io/webidl/#es-unsigned-short
impl FromJSValConvertible for u16 {
    type Config = ConversionBehavior;
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: ConversionBehavior)
                         -> Result<ConversionResult<u16>, ()> {
        convert_int_from_jsval(cx, val, option, ToUint16)
    }
}

// https://heycam.github.io/webidl/#es-long
impl ToJSValConvertible for i32 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(Int32Value(*self));
    }
}

// https://heycam.github.io/webidl/#es-long
impl FromJSValConvertible for i32 {
    type Config = ConversionBehavior;
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: ConversionBehavior)
                         -> Result<ConversionResult<i32>, ()> {
        convert_int_from_jsval(cx, val, option, ToInt32)
    }
}

// https://heycam.github.io/webidl/#es-unsigned-long
impl ToJSValConvertible for u32 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(UInt32Value(*self));
    }
}

// https://heycam.github.io/webidl/#es-unsigned-long
impl FromJSValConvertible for u32 {
    type Config = ConversionBehavior;
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: ConversionBehavior)
                         -> Result<ConversionResult<u32>, ()> {
        convert_int_from_jsval(cx, val, option, ToUint32)
    }
}

// https://heycam.github.io/webidl/#es-long-long
impl ToJSValConvertible for i64 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(RUST_JS_NumberValue(*self as f64));
    }
}

// https://heycam.github.io/webidl/#es-long-long
impl FromJSValConvertible for i64 {
    type Config = ConversionBehavior;
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: ConversionBehavior)
                         -> Result<ConversionResult<i64>, ()> {
        convert_int_from_jsval(cx, val, option, ToInt64)
    }
}

// https://heycam.github.io/webidl/#es-unsigned-long-long
impl ToJSValConvertible for u64 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(RUST_JS_NumberValue(*self as f64));
    }
}

// https://heycam.github.io/webidl/#es-unsigned-long-long
impl FromJSValConvertible for u64 {
    type Config = ConversionBehavior;
    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         option: ConversionBehavior)
                         -> Result<ConversionResult<u64>, ()> {
        convert_int_from_jsval(cx, val, option, ToUint64)
    }
}

// https://heycam.github.io/webidl/#es-float
impl ToJSValConvertible for f32 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(RUST_JS_NumberValue(*self as f64));
    }
}

// https://heycam.github.io/webidl/#es-float
impl FromJSValConvertible for f32 {
    type Config = ();
    unsafe fn from_jsval(cx: *mut JSContext, val: JS::HandleValue, _option: ()) -> Result<ConversionResult<f32>, ()> {
        let result = ToNumber(cx, val);
        result.map(|f| f as f32).map(ConversionResult::Success)
    }
}

// https://heycam.github.io/webidl/#es-double
impl ToJSValConvertible for f64 {
    #[inline]
    unsafe fn to_jsval(&self, _cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(RUST_JS_NumberValue(*self));
    }
}

// https://heycam.github.io/webidl/#es-double
impl FromJSValConvertible for f64 {
    type Config = ();
    unsafe fn from_jsval(cx: *mut JSContext, val: JS::HandleValue, _option: ()) -> Result<ConversionResult<f64>, ()> {
        ToNumber(cx, val).map(ConversionResult::Success)
    }
}

/// Converts a `JSString`, encoded in "Latin1" (i.e. U+0000-U+00FF encoded as 0x00-0xFF) into a
/// `String`.
pub unsafe fn latin1_to_string(cx: *mut JSContext, s: *mut JSString) -> String {
    assert!(JS_StringHasLatin1Chars(s));

    let mut length = 0;
    let chars = JS_GetLatin1StringCharsAndLength(cx, ptr::null(), s, &mut length);
    assert!(!chars.is_null());

    let chars = slice::from_raw_parts(chars, length as usize);
    let mut s = String::with_capacity(length as usize);
    s.extend(chars.iter().map(|&c| c as char));
    s
}

// https://heycam.github.io/webidl/#es-USVString
impl ToJSValConvertible for str {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        let mut string_utf16: Vec<u16> = Vec::with_capacity(self.len());
        string_utf16.extend(self.encode_utf16());
        let jsstr = JS_NewUCStringCopyN(cx,
                                        string_utf16.as_ptr(),
                                        string_utf16.len() as libc::size_t);
        if jsstr.is_null() {
            panic!("JS_NewUCStringCopyN failed");
        }
        rval.set(StringValue(&*jsstr));
    }
}

// https://heycam.github.io/webidl/#es-USVString
impl ToJSValConvertible for String {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        (**self).to_jsval(cx, rval);
    }
}

// https://heycam.github.io/webidl/#es-USVString
impl FromJSValConvertible for String {
    type Config = ();
    unsafe fn from_jsval(cx: *mut JSContext, value: JS::HandleValue, _: ()) -> Result<ConversionResult<String>, ()> {
        let jsstr = ToString(cx, value);
        if jsstr.is_null() {
            debug!("ToString failed");
            return Err(());
        }
        if JS_StringHasLatin1Chars(jsstr) {
            return Ok(latin1_to_string(cx, jsstr)).map(ConversionResult::Success);
        }

        let mut length = 0;
        let chars = JS_GetTwoByteStringCharsAndLength(cx, ptr::null(), jsstr, &mut length);
        assert!(!chars.is_null());
        let char_vec = slice::from_raw_parts(chars, length as usize);
        Ok(String::from_utf16_lossy(char_vec)).map(ConversionResult::Success)
    }
}

impl<T: ToJSValConvertible> ToJSValConvertible for Option<T> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        match self {
            &Some(ref value) => value.to_jsval(cx, rval),
            &None => rval.set(NullValue()),
        }
    }
}

impl<T: ToJSValConvertible> ToJSValConvertible for Rc<T> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        (**self).to_jsval(cx, rval)
    }
}

impl<T: FromJSValConvertible> FromJSValConvertible for Option<T> {
    type Config = T::Config;
    unsafe fn from_jsval(cx: *mut JSContext,
                         value: JS::HandleValue,
                         option: T::Config)
                         -> Result<ConversionResult<Option<T>>, ()> {
        if value.get().is_null_or_undefined() {
            Ok(ConversionResult::Success(None))
        } else {
            Ok(match try!(FromJSValConvertible::from_jsval(cx, value, option)) {
                ConversionResult::Success(v) => ConversionResult::Success(Some(v)),
                ConversionResult::Failure(v) => ConversionResult::Failure(v),
            })
        }
    }
}

// https://heycam.github.io/webidl/#es-sequence
impl<T: ToJSValConvertible> ToJSValConvertible for Vec<T> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rooted!(in(cx) let js_array = JS_NewArrayObject1(cx, self.len() as libc::size_t));
        assert!(!js_array.handle().is_null());

        rooted!(in(cx) let mut val = UndefinedValue());
        for (index, obj) in self.iter().enumerate() {
            obj.to_jsval(cx, val.handle_mut());

            assert!(JS_DefineElement(
                cx,
                js_array.handle(),
                index as u32,
                val.handle(),
                JSPROP_ENUMERATE as _
            ));
        }

        rval.set(ObjectValue(js_array.handle().get()));
    }
}

/// Rooting guard for the iterator field of ForOfIterator.
/// Behaves like RootedGuard (roots on creation, unroots on drop),
/// but borrows and allows access to the whole ForOfIterator, so
/// that methods on ForOfIterator can still be used through it.
struct ForOfIteratorGuard<'a> {
    root: &'a mut JS::ForOfIterator
}

impl<'a> ForOfIteratorGuard<'a> {
    fn new(cx: *mut JSContext, root: &'a mut JS::ForOfIterator) -> Self {
        unsafe {
            root.iterator.register_with_root_lists(cx);
        }
        ForOfIteratorGuard {
            root: root
        }
    }
}

impl<'a> Drop for ForOfIteratorGuard<'a> {
    fn drop(&mut self) {
        unsafe {
            self.root.iterator.remove_from_root_stack();
        }
    }
}

impl<C: Clone, T: FromJSValConvertible<Config=C>> FromJSValConvertible for Vec<T> {
    type Config = C;

    unsafe fn from_jsval(cx: *mut JSContext,
                         value: JS::HandleValue,
                         option: C)
                         -> Result<ConversionResult<Vec<T>>, ()> {
        let mut iterator = JS::ForOfIterator {
            cx_: cx,
            iterator: JS::RootedObject::new_unrooted(),
            index: ::std::u32::MAX, // NOT_ARRAY
        };
        let iterator = ForOfIteratorGuard::new(cx, &mut iterator);
        let iterator = &mut *iterator.root;

        if !iterator.init(value, JS::ForOfIterator_NonIterableBehavior::AllowNonIterable) {
            return Err(())
        }

        if iterator.iterator.ptr.is_null() {
            return Ok(ConversionResult::Failure("Value is not iterable".into()));
        }

        let mut ret = vec![];

        loop {
            let mut done = false;
            rooted!(in(cx) let mut val = UndefinedValue());
            if !iterator.next(val.handle_mut(), &mut done) {
                return Err(())
            }

            if done {
                break;
            }

            ret.push(match try!(T::from_jsval(cx, val.handle(), option.clone())) {
                ConversionResult::Success(v) => v,
                ConversionResult::Failure(e) => return Ok(ConversionResult::Failure(e)),
            });
        }

        Ok(ret).map(ConversionResult::Success)
    }
}

// https://heycam.github.io/webidl/#es-object
impl ToJSValConvertible for *mut JSObject {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(ObjectOrNullValue(*self));
        maybe_wrap_object_or_null_value(cx, rval);
    }
}

// https://heycam.github.io/webidl/#es-object
#[cfg(feature = "nonzero")]
impl ToJSValConvertible for NonZero<*mut JSObject> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        use rust::maybe_wrap_object_value;
        rval.set(ObjectValue(self.get()));
        maybe_wrap_object_value(cx, rval);
    }
}

// https://heycam.github.io/webidl/#es-object
impl ToJSValConvertible for Heap<*mut JSObject> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(ObjectOrNullValue(self.get()));
        maybe_wrap_object_or_null_value(cx, rval);
    }
}

// JSFunction

impl ToJSValConvertible for *mut JSFunction {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(ObjectOrNullValue(*self as *mut JSObject));
        maybe_wrap_object_or_null_value(cx, rval);
    }
}

#[cfg(feature = "nonzero")]
impl ToJSValConvertible for NonZero<*mut JSFunction> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        use rust::maybe_wrap_object_value;
        rval.set(ObjectValue(self.get() as *mut JSObject));
        maybe_wrap_object_value(cx, rval);
    }
}

impl ToJSValConvertible for Heap<*mut JSFunction> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(ObjectOrNullValue(self.get() as *mut JSObject));
        maybe_wrap_object_or_null_value(cx, rval);
    }
}

impl ToJSValConvertible for JS::Handle<*mut JSFunction> {
    #[inline]
    unsafe fn to_jsval(&self, cx: *mut JSContext, rval: JS::MutableHandleValue) {
        rval.set(ObjectOrNullValue(self.get() as *mut JSObject));
        maybe_wrap_object_or_null_value(cx, rval);
    }
}

impl FromJSValConvertible for *mut JSFunction {
    type Config = ();

    unsafe fn from_jsval(cx: *mut JSContext,
                         val: JS::HandleValue,
                         _: ())
                         -> Result<ConversionResult<Self>, ()> {
        let func = JS_ValueToFunction(cx, val);
        if func.is_null() {
            Ok(ConversionResult::Failure("value is not a function".into()))
        } else {
            Ok(ConversionResult::Success(func))
        }
    }
}
