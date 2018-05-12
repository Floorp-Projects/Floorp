/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


use jsapi::root::*;
use libc::c_void;
use std::mem;

#[cfg(target_pointer_width = "64")]
const JSVAL_TAG_SHIFT: usize = 47;

#[cfg(target_pointer_width = "64")]
const JSVAL_TAG_MAX_DOUBLE: u32 = 0x1FFF0u32;

#[cfg(target_pointer_width = "32")]
const JSVAL_TAG_CLEAR: u32 = 0xFFFFFF80;

#[cfg(target_pointer_width = "64")]
#[repr(u32)]
#[allow(dead_code)]
#[derive(Clone, Copy, Debug)]
enum ValueTag {
    INT32     = JSVAL_TAG_MAX_DOUBLE | (JSValueType::JSVAL_TYPE_INT32 as u32),
    UNDEFINED = JSVAL_TAG_MAX_DOUBLE | (JSValueType::JSVAL_TYPE_UNDEFINED as u32),
    STRING    = JSVAL_TAG_MAX_DOUBLE | (JSValueType::JSVAL_TYPE_STRING as u32),
    SYMBOL    = JSVAL_TAG_MAX_DOUBLE | (JSValueType::JSVAL_TYPE_SYMBOL as u32),
    BOOLEAN   = JSVAL_TAG_MAX_DOUBLE | (JSValueType::JSVAL_TYPE_BOOLEAN as u32),
    MAGIC     = JSVAL_TAG_MAX_DOUBLE | (JSValueType::JSVAL_TYPE_MAGIC as u32),
    NULL      = JSVAL_TAG_MAX_DOUBLE | (JSValueType::JSVAL_TYPE_NULL as u32),
    OBJECT    = JSVAL_TAG_MAX_DOUBLE | (JSValueType::JSVAL_TYPE_OBJECT as u32),
}

#[cfg(target_pointer_width = "32")]
#[repr(u32)]
#[allow(dead_code)]
#[derive(Clone, Copy, Debug)]
enum ValueTag {
    PRIVATE   = 0,
    INT32     = JSVAL_TAG_CLEAR as u32 | (JSValueType::JSVAL_TYPE_INT32 as u32),
    UNDEFINED = JSVAL_TAG_CLEAR as u32 | (JSValueType::JSVAL_TYPE_UNDEFINED as u32),
    STRING    = JSVAL_TAG_CLEAR as u32 | (JSValueType::JSVAL_TYPE_STRING as u32),
    SYMBOL    = JSVAL_TAG_CLEAR as u32 | (JSValueType::JSVAL_TYPE_SYMBOL as u32),
    BOOLEAN   = JSVAL_TAG_CLEAR as u32 | (JSValueType::JSVAL_TYPE_BOOLEAN as u32),
    MAGIC     = JSVAL_TAG_CLEAR as u32 | (JSValueType::JSVAL_TYPE_MAGIC as u32),
    NULL      = JSVAL_TAG_CLEAR as u32 | (JSValueType::JSVAL_TYPE_NULL as u32),
    OBJECT    = JSVAL_TAG_CLEAR as u32 | (JSValueType::JSVAL_TYPE_OBJECT as u32),
}

#[cfg(target_pointer_width = "64")]
#[repr(u64)]
#[allow(dead_code)]
#[derive(Clone, Copy, Debug)]
enum ValueShiftedTag {
    MAX_DOUBLE = (((JSVAL_TAG_MAX_DOUBLE as u64) << JSVAL_TAG_SHIFT) | 0xFFFFFFFFu64),
    INT32      = ((ValueTag::INT32 as u64)      << JSVAL_TAG_SHIFT),
    UNDEFINED  = ((ValueTag::UNDEFINED as u64)  << JSVAL_TAG_SHIFT),
    STRING     = ((ValueTag::STRING as u64)     << JSVAL_TAG_SHIFT),
    SYMBOL     = ((ValueTag::SYMBOL as u64)     << JSVAL_TAG_SHIFT),
    BOOLEAN    = ((ValueTag::BOOLEAN as u64)    << JSVAL_TAG_SHIFT),
    MAGIC      = ((ValueTag::MAGIC as u64)      << JSVAL_TAG_SHIFT),
    NULL       = ((ValueTag::NULL as u64)       << JSVAL_TAG_SHIFT),
    OBJECT     = ((ValueTag::OBJECT as u64)     << JSVAL_TAG_SHIFT),
}


#[cfg(target_pointer_width = "64")]
const JSVAL_PAYLOAD_MASK: u64 = 0x00007FFFFFFFFFFF;

#[inline(always)]
fn AsJSVal(val: u64) -> JS::Value {
    JS::Value {
        asBits_: val,
    }
}

#[cfg(target_pointer_width = "64")]
#[inline(always)]
fn BuildJSVal(tag: ValueTag, payload: u64) -> JS::Value {
    AsJSVal(((tag as u32 as u64) << JSVAL_TAG_SHIFT) | payload)
}

#[cfg(target_pointer_width = "32")]
#[inline(always)]
fn BuildJSVal(tag: ValueTag, payload: u64) -> JS::Value {
    AsJSVal(((tag as u32 as u64) << 32) | payload)
}

#[inline(always)]
pub fn NullValue() -> JS::Value {
    BuildJSVal(ValueTag::NULL, 0)
}

#[inline(always)]
pub fn UndefinedValue() -> JS::Value {
    BuildJSVal(ValueTag::UNDEFINED, 0)
}

#[inline(always)]
pub fn Int32Value(i: i32) -> JS::Value {
    BuildJSVal(ValueTag::INT32, i as u32 as u64)
}

#[cfg(target_pointer_width = "64")]
#[inline(always)]
pub fn DoubleValue(f: f64) -> JS::Value {
    let bits: u64 = unsafe { mem::transmute(f) };
    assert!(bits <= ValueShiftedTag::MAX_DOUBLE as u64);
    AsJSVal(bits)
}

#[cfg(target_pointer_width = "32")]
#[inline(always)]
pub fn DoubleValue(f: f64) -> JS::Value {
    let bits: u64 = unsafe { mem::transmute(f) };
    let val = AsJSVal(bits);
    assert!(val.is_double());
    val
}

#[inline(always)]
pub fn UInt32Value(ui: u32) -> JS::Value {
    if ui > 0x7fffffff {
        DoubleValue(ui as f64)
    } else {
        Int32Value(ui as i32)
    }
}

#[cfg(target_pointer_width = "64")]
#[inline(always)]
pub fn StringValue(s: &JSString) -> JS::Value {
    let bits = s as *const JSString as usize as u64;
    assert!((bits >> JSVAL_TAG_SHIFT) == 0);
    BuildJSVal(ValueTag::STRING, bits)
}

#[cfg(target_pointer_width = "32")]
#[inline(always)]
pub fn StringValue(s: &JSString) -> JS::Value {
    let bits = s as *const JSString as usize as u64;
    BuildJSVal(ValueTag::STRING, bits)
}

#[inline(always)]
pub fn BooleanValue(b: bool) -> JS::Value {
    BuildJSVal(ValueTag::BOOLEAN, b as u64)
}

#[cfg(target_pointer_width = "64")]
#[inline(always)]
pub fn ObjectValue(o: *mut JSObject) -> JS::Value {
    let bits = o as usize as u64;
    assert!((bits >> JSVAL_TAG_SHIFT) == 0);
    BuildJSVal(ValueTag::OBJECT, bits)
}

#[cfg(target_pointer_width = "32")]
#[inline(always)]
pub fn ObjectValue(o: *mut JSObject) -> JS::Value {
    let bits = o as usize as u64;
    BuildJSVal(ValueTag::OBJECT, bits)
}

#[inline(always)]
pub fn ObjectOrNullValue(o: *mut JSObject) -> JS::Value {
    if o.is_null() {
        NullValue()
    } else {
        ObjectValue(o)
    }
}

#[cfg(target_pointer_width = "64")]
#[inline(always)]
pub fn PrivateValue(o: *const c_void) -> JS::Value {
    let ptrBits = o as usize as u64;
    assert!((ptrBits & 1) == 0);
    AsJSVal(ptrBits >> 1)
}

#[cfg(target_pointer_width = "32")]
#[inline(always)]
pub fn PrivateValue(o: *const c_void) -> JS::Value {
    let ptrBits = o as usize as u64;
    assert!((ptrBits & 1) == 0);
    BuildJSVal(ValueTag::PRIVATE, ptrBits)
}

impl JS::Value {
    #[inline(always)]
    unsafe fn asBits(&self) -> u64 {
        self.asBits_
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_undefined(&self) -> bool {
        unsafe {
            self.asBits() == ValueShiftedTag::UNDEFINED as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_undefined(&self) -> bool {
        unsafe {
            (self.asBits() >> 32) == ValueTag::UNDEFINED as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_null(&self) -> bool {
        unsafe {
            self.asBits() == ValueShiftedTag::NULL as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_null(&self) -> bool {
        unsafe {
            (self.asBits() >> 32) == ValueTag::NULL as u64
        }
    }

    #[inline(always)]
    pub fn is_null_or_undefined(&self) -> bool {
        self.is_null() || self.is_undefined()
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_boolean(&self) -> bool {
        unsafe {
            (self.asBits() >> JSVAL_TAG_SHIFT) == ValueTag::BOOLEAN as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_boolean(&self) -> bool {
        unsafe {
            (self.asBits() >> 32) == ValueTag::BOOLEAN as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_int32(&self) -> bool {
        unsafe {
            (self.asBits() >> JSVAL_TAG_SHIFT) == ValueTag::INT32 as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_int32(&self) -> bool {
        unsafe {
            (self.asBits() >> 32) == ValueTag::INT32 as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_double(&self) -> bool {
        unsafe {
            self.asBits() <= ValueShiftedTag::MAX_DOUBLE as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_double(&self) -> bool {
        unsafe {
            (self.asBits() >> 32) <= JSVAL_TAG_CLEAR as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_number(&self) -> bool {
        const JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET: u64 = ValueShiftedTag::UNDEFINED as u64;
        unsafe {
            self.asBits() < JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_number(&self) -> bool {
        const JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET: u64 = ValueTag::INT32 as u64;
        unsafe {
            (self.asBits() >> 32) <= JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_primitive(&self) -> bool {
        const JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET: u64 = ValueShiftedTag::OBJECT as u64;
        unsafe {
            self.asBits() < JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_primitive(&self) -> bool {
        const JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET: u64 = ValueTag::OBJECT as u64;
        unsafe {
            (self.asBits() >> 32) < JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_string(&self) -> bool {
        unsafe {
            (self.asBits() >> JSVAL_TAG_SHIFT) == ValueTag::STRING as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_string(&self) -> bool {
        unsafe {
            (self.asBits() >> 32) == ValueTag::STRING as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_object(&self) -> bool {
        unsafe {
            assert!((self.asBits() >> JSVAL_TAG_SHIFT) <= ValueTag::OBJECT as u64);
            self.asBits() >= ValueShiftedTag::OBJECT as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_object(&self) -> bool {
        unsafe {
            (self.asBits() >> 32) == ValueTag::OBJECT as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_symbol(&self) -> bool {
        unsafe {
            (self.asBits() >> JSVAL_TAG_SHIFT) == ValueTag::SYMBOL as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_symbol(&self) -> bool {
        unsafe {
            (self.asBits() >> 32) == ValueTag::SYMBOL as u64
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn to_boolean(&self) -> bool {
        assert!(self.is_boolean());
        unsafe {
            (self.asBits() & JSVAL_PAYLOAD_MASK) != 0
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn to_boolean(&self) -> bool {
        assert!(self.is_boolean());
        unsafe {
            (self.asBits() & 0x00000000FFFFFFFF) != 0
        }
    }

    #[inline(always)]
    pub fn to_int32(&self) -> i32 {
        assert!(self.is_int32());
        unsafe {
            (self.asBits() & 0x00000000FFFFFFFF) as i32
        }
    }

    #[inline(always)]
    pub fn to_double(&self) -> f64 {
        assert!(self.is_double());
        unsafe { mem::transmute(self.asBits()) }
    }

    #[inline(always)]
    pub fn to_number(&self) -> f64 {
        assert!(self.is_number());
        if self.is_double() {
            self.to_double()
        } else {
            self.to_int32() as f64
        }
    }

    #[inline(always)]
    pub fn to_object(&self) -> *mut JSObject {
        assert!(self.is_object());
        self.to_object_or_null()
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn to_string(&self) -> *mut JSString {
        assert!(self.is_string());
        unsafe {
            let ptrBits = self.asBits() & JSVAL_PAYLOAD_MASK;
            ptrBits as usize as *mut JSString
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn to_string(&self) -> *mut JSString {
        assert!(self.is_string());
        unsafe {
            let ptrBits: u32 = (self.asBits() & 0x00000000FFFFFFFF) as u32;
            ptrBits as *mut JSString
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_object_or_null(&self) -> bool {
        const JSVAL_LOWER_INCL_SHIFTED_TAG_OF_OBJ_OR_NULL_SET: u64 = ValueShiftedTag::NULL as u64;
        unsafe {
            assert!((self.asBits() >> JSVAL_TAG_SHIFT) <= ValueTag::OBJECT as u64);
            self.asBits() >= JSVAL_LOWER_INCL_SHIFTED_TAG_OF_OBJ_OR_NULL_SET
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_object_or_null(&self) -> bool {
        const JSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET: u64 = ValueTag::NULL as u64;
        unsafe {
            assert!((self.asBits() >> 32) <= ValueTag::OBJECT as u64);
            (self.asBits() >> 32) >= JSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn to_object_or_null(&self) -> *mut JSObject {
        assert!(self.is_object_or_null());
        unsafe {
            let ptrBits = self.asBits() & JSVAL_PAYLOAD_MASK;
            assert!((ptrBits & 0x7) == 0);
            ptrBits as usize as *mut JSObject
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn to_object_or_null(&self) -> *mut JSObject {
        assert!(self.is_object_or_null());
        unsafe {
            let ptrBits: u32 = (self.asBits() & 0x00000000FFFFFFFF) as u32;
            ptrBits as *mut JSObject
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn to_private(&self) -> *const c_void {
        assert!(self.is_double());
        unsafe {
            assert!((self.asBits() & 0x8000000000000000u64) == 0);
            (self.asBits() << 1) as usize as *const c_void
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn to_private(&self) -> *const c_void {
        unsafe {
            let ptrBits: u32 = (self.asBits() & 0x00000000FFFFFFFF) as u32;
            ptrBits as *const c_void
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn is_gcthing(&self) -> bool {
        const JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET: u64 = ValueShiftedTag::STRING as u64;
        unsafe {
            self.asBits() >= JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn is_gcthing(&self) -> bool {
        const JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET: u64 = ValueTag::STRING as u64;
        unsafe {
            (self.asBits() >> 32) >= JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "64")]
    pub fn to_gcthing(&self) -> *mut c_void {
        assert!(self.is_gcthing());
        unsafe {
            let ptrBits = self.asBits() & JSVAL_PAYLOAD_MASK;
            assert!((ptrBits & 0x7) == 0);
            ptrBits as *mut c_void
        }
    }

    #[inline(always)]
    #[cfg(target_pointer_width = "32")]
    pub fn to_gcthing(&self) -> *mut c_void {
        assert!(self.is_gcthing());
        unsafe {
            let ptrBits: u32 = (self.asBits() & 0x00000000FFFFFFFF) as u32;
            ptrBits as *mut c_void
        }
    }

    #[inline(always)]
    pub fn is_markable(&self) -> bool {
        self.is_gcthing() && !self.is_null()
    }

    #[inline(always)]
    pub fn trace_kind(&self) -> JS::TraceKind {
        assert!(self.is_markable());
        if self.is_object() {
            JS::TraceKind::Object
        } else {
            JS::TraceKind::String
        }
    }
}
