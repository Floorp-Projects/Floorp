/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use heap::Heap;
use jsapi::root::*;
use std::os::raw::c_void;

pub enum Action {}
unsafe impl Sync for ProxyTraps {}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct ProxyTraps {
    pub enter: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            id: JS::HandleId,
            action: Action,
            bp: *mut bool,
        ) -> bool,
    >,
    pub getOwnPropertyDescriptor: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            id: JS::HandleId,
            desc: JS::MutableHandle<JS::PropertyDescriptor>,
        ) -> bool,
    >,
    pub defineProperty: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            id: JS::HandleId,
            desc: JS::Handle<JS::PropertyDescriptor>,
            result: *mut JS::ObjectOpResult,
        ) -> bool,
    >,
    pub ownPropertyKeys: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            props: JS::MutableHandleIdVector,
        ) -> bool,
    >,
    pub delete_: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            id: JS::HandleId,
            result: *mut JS::ObjectOpResult,
        ) -> bool,
    >,
    pub enumerate: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            objp: JS::MutableHandleObject,
        ) -> bool,
    >,
    pub getPrototypeIfOrdinary: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            isOrdinary: *mut bool,
            protop: JS::MutableHandleObject,
        ) -> bool,
    >,
    pub preventExtensions: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            result: *mut JS::ObjectOpResult,
        ) -> bool,
    >,
    pub isExtensible: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            succeeded: *mut bool,
        ) -> bool,
    >,
    pub has: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            id: JS::HandleId,
            bp: *mut bool,
        ) -> bool,
    >,
    pub get: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            receiver: JS::HandleValue,
            id: JS::HandleId,
            vp: JS::MutableHandleValue,
        ) -> bool,
    >,
    pub set: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            id: JS::HandleId,
            v: JS::HandleValue,
            receiver: JS::HandleValue,
            result: *mut JS::ObjectOpResult,
        ) -> bool,
    >,
    pub call: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            args: *const JS::CallArgs,
        ) -> bool,
    >,
    pub construct: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            args: *const JS::CallArgs,
        ) -> bool,
    >,
    pub hasOwn: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            id: JS::HandleId,
            bp: *mut bool,
        ) -> bool,
    >,
    pub getOwnEnumerablePropertyKeys: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            props: JS::MutableHandleIdVector,
        ) -> bool,
    >,
    pub nativeCall: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            test: JS::IsAcceptableThis,
            _impl: JS::NativeImpl,
            args: JS::CallArgs,
        ) -> bool,
    >,
    pub hasInstance: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            v: JS::MutableHandleValue,
            bp: *mut bool,
        ) -> bool,
    >,
    pub objectClassIs: ::std::option::Option<
        unsafe extern "C" fn(
            obj: JS::HandleObject,
            classValue: js::ESClass,
            cx: *mut JSContext,
        ) -> bool,
    >,
    pub className: ::std::option::Option<
        unsafe extern "C" fn(cx: *mut JSContext, proxy: JS::HandleObject) -> *const i8,
    >,
    pub fun_toString: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            indent: u32,
        ) -> *mut JSString,
    >,
    pub boxedValue_unbox: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            proxy: JS::HandleObject,
            vp: JS::MutableHandleValue,
        ) -> bool,
    >,
    pub defaultValue: ::std::option::Option<
        unsafe extern "C" fn(
            cx: *mut JSContext,
            obj: JS::HandleObject,
            hint: JSType,
            vp: JS::MutableHandleValue,
        ) -> bool,
    >,
    pub trace:
        ::std::option::Option<unsafe extern "C" fn(trc: *mut JSTracer, proxy: *mut JSObject)>,
    pub finalize:
        ::std::option::Option<unsafe extern "C" fn(fop: *mut JSFreeOp, proxy: *mut JSObject)>,
    pub objectMoved: ::std::option::Option<
        unsafe extern "C" fn(proxy: *mut JSObject, old: *mut JSObject) -> usize,
    >,
    pub isCallable: ::std::option::Option<unsafe extern "C" fn(obj: *mut JSObject) -> bool>,
    pub isConstructor: ::std::option::Option<unsafe extern "C" fn(obj: *mut JSObject) -> bool>,
}
impl ::std::default::Default for ProxyTraps {
    fn default() -> ProxyTraps {
        unsafe { ::std::mem::zeroed() }
    }
}
#[repr(C)]
#[derive(Copy, Clone)]
pub struct WrapperProxyHandler {
    pub mTraps: ProxyTraps,
}
impl ::std::default::Default for WrapperProxyHandler {
    fn default() -> WrapperProxyHandler {
        unsafe { ::std::mem::zeroed() }
    }
}
#[repr(C)]
#[derive(Copy, Clone)]
pub struct ForwardingProxyHandler {
    pub mTraps: ProxyTraps,
    pub mExtra: *const ::libc::c_void,
}
impl ::std::default::Default for ForwardingProxyHandler {
    fn default() -> ForwardingProxyHandler {
        unsafe { ::std::mem::zeroed() }
    }
}

extern "C" {
    pub fn InvokeGetOwnPropertyDescriptor(
        handler: *const ::libc::c_void,
        cx: *mut JSContext,
        proxy: JS::HandleObject,
        id: JS::HandleId,
        desc: JS::MutableHandle<JS::PropertyDescriptor>,
    ) -> bool;
    pub fn InvokeHasOwn(
        handler: *const ::libc::c_void,
        cx: *mut JSContext,
        proxy: JS::HandleObject,
        id: JS::HandleId,
        bp: *mut bool,
    ) -> bool;
    pub fn RUST_JS_NumberValue(d: f64) -> JS::Value;
    pub fn RUST_FUNCTION_VALUE_TO_JITINFO(v: JS::Value) -> *const JSJitInfo;
    pub fn CreateCallArgsFromVp(argc: u32, v: *mut JS::Value) -> JS::CallArgs;
    pub fn CallJitGetterOp(
        info: *const JSJitInfo,
        cx: *mut JSContext,
        thisObj: JS::HandleObject,
        specializedThis: *mut ::libc::c_void,
        argc: u32,
        vp: *mut JS::Value,
    ) -> bool;
    pub fn CallJitSetterOp(
        info: *const JSJitInfo,
        cx: *mut JSContext,
        thisObj: JS::HandleObject,
        specializedThis: *mut ::libc::c_void,
        argc: u32,
        vp: *mut JS::Value,
    ) -> bool;
    pub fn CallJitMethodOp(
        info: *const JSJitInfo,
        cx: *mut JSContext,
        thisObj: JS::HandleObject,
        specializedThis: *mut ::libc::c_void,
        argc: u32,
        vp: *mut JS::Value,
    ) -> bool;
    pub fn CreateProxyHandler(
        aTraps: *const ProxyTraps,
        aExtra: *const ::libc::c_void,
    ) -> *const ::libc::c_void;
    pub fn CreateWrapperProxyHandler(aTraps: *const ProxyTraps) -> *const ::libc::c_void;
    pub fn CreateRustJSPrincipal(
        origin: *const ::libc::c_void,
        destroy: Option<unsafe extern "C" fn(principal: *mut JSPrincipals)>,
        write: Option<
            unsafe extern "C" fn(cx: *mut JSContext, writer: *mut JSStructuredCloneWriter) -> bool,
        >,
    ) -> *mut JSPrincipals;
    pub fn GetPrincipalOrigin(principal: *const JSPrincipals) -> *const ::libc::c_void;
    pub fn GetCrossCompartmentWrapper() -> *const ::libc::c_void;
    pub fn GetSecurityWrapper() -> *const ::libc::c_void;
    pub fn NewCompileOptions(
        aCx: *mut JSContext,
        aFile: *const ::libc::c_char,
        aLine: u32,
    ) -> *mut JS::ReadOnlyCompileOptions;
    pub fn DeleteCompileOptions(aOpts: *mut JS::ReadOnlyCompileOptions);
    pub fn NewProxyObject(
        aCx: *mut JSContext,
        aHandler: *const ::libc::c_void,
        aPriv: JS::HandleValue,
        proto: *mut JSObject,
        parent: *mut JSObject,
        call: *mut JSObject,
        construct: *mut JSObject,
    ) -> *mut JSObject;
    pub fn WrapperNew(
        aCx: *mut JSContext,
        aObj: JS::HandleObject,
        aHandler: *const ::libc::c_void,
        aClass: *const JSClass,
        aSingleton: bool,
    ) -> *mut JSObject;
    pub fn NewWindowProxy(
        aCx: *mut JSContext,
        aObj: JS::HandleObject,
        aHandler: *const ::libc::c_void,
    ) -> *mut JSObject;
    pub fn GetWindowProxyClass() -> *const JSClass;
    pub fn GetProxyPrivate(obj: *mut JSObject) -> JS::Value;
    pub fn SetProxyPrivate(obj: *mut JSObject, private: *const JS::Value);
    pub fn GetProxyReservedSlot(obj: *mut JSObject, slot: u32) -> JS::Value;
    pub fn SetProxyReservedSlot(obj: *mut JSObject, slot: u32, val: *const JS::Value);
    pub fn RUST_JSID_IS_INT(id: JS::HandleId) -> bool;
    pub fn RUST_JSID_TO_INT(id: JS::HandleId) -> i32;
    pub fn int_to_jsid(i: i32) -> jsid;
    pub fn RUST_JSID_IS_STRING(id: JS::HandleId) -> bool;
    pub fn RUST_JSID_TO_STRING(id: JS::HandleId) -> *mut JSString;
    pub fn RUST_SYMBOL_TO_JSID(sym: *mut JS::Symbol) -> jsid;
    pub fn RUST_SET_JITINFO(func: *mut JSFunction, info: *const JSJitInfo);
    pub fn RUST_INTERNED_STRING_TO_JSID(cx: *mut JSContext, str: *mut JSString) -> jsid;
    pub fn RUST_js_GetErrorMessage(
        userRef: *mut ::libc::c_void,
        errorNumber: u32,
    ) -> *const JSErrorFormatString;
    pub fn IsProxyHandlerFamily(obj: *mut JSObject) -> u8;
    pub fn GetProxyHandlerExtra(obj: *mut JSObject) -> *const ::libc::c_void;
    pub fn GetProxyHandler(obj: *mut JSObject) -> *const ::libc::c_void;
    pub fn ReportError(aCx: *mut JSContext, aError: *const i8);
    pub fn IsWrapper(obj: *mut JSObject) -> bool;
    pub fn UnwrapObjectStatic(obj: *mut JSObject) -> *mut JSObject;
    pub fn UncheckedUnwrapObject(obj: *mut JSObject, stopAtOuter: u8) -> *mut JSObject;
    pub fn CreateRootedIdVector(cx: *mut JSContext) -> *mut JS::PersistentRootedIdVector;
    pub fn AppendToRootedIdVector(v: *mut JS::PersistentRootedIdVector, id: jsid) -> bool;
    pub fn SliceRootedIdVector(
        v: *const JS::PersistentRootedIdVector,
        length: *mut usize,
    ) -> *const jsid;
    pub fn DestroyRootedIdVector(v: *mut JS::PersistentRootedIdVector);
    pub fn GetMutableHandleIdVector(
        v: *mut JS::PersistentRootedIdVector,
    ) -> JS::MutableHandleIdVector;
    pub fn CreateRootedObjectVector(aCx: *mut JSContext) -> *mut JS::PersistentRootedObjectVector;
    pub fn AppendToRootedObjectVector(
        v: *mut JS::PersistentRootedObjectVector,
        obj: *mut JSObject,
    ) -> bool;
    pub fn DeleteRootedObjectVector(v: *mut JS::PersistentRootedObjectVector);
    pub fn CollectServoSizes(rt: *mut JSRuntime, sizes: *mut JS::ServoSizes) -> bool;
    pub fn CallIdTracer(trc: *mut JSTracer, idp: *mut Heap<jsid>, name: *const ::libc::c_char);
    pub fn CallValueTracer(
        trc: *mut JSTracer,
        valuep: *mut Heap<JS::Value>,
        name: *const ::libc::c_char,
    );
    pub fn CallObjectTracer(
        trc: *mut JSTracer,
        objp: *mut Heap<*mut JSObject>,
        name: *const ::libc::c_char,
    );
    pub fn CallStringTracer(
        trc: *mut JSTracer,
        strp: *mut Heap<*mut JSString>,
        name: *const ::libc::c_char,
    );
    #[cfg(feature = "bigint")]
    pub fn CallBigIntTracer(
        trc: *mut JSTracer,
        bip: *mut Heap<*mut JS::BigInt>,
        name: *const ::libc::c_char,
    );
    pub fn CallScriptTracer(
        trc: *mut JSTracer,
        scriptp: *mut Heap<*mut JSScript>,
        name: *const ::libc::c_char,
    );
    pub fn CallFunctionTracer(
        trc: *mut JSTracer,
        funp: *mut Heap<*mut JSFunction>,
        name: *const ::libc::c_char,
    );
    pub fn CallUnbarrieredObjectTracer(
        trc: *mut JSTracer,
        objp: *mut *mut JSObject,
        name: *const ::libc::c_char,
    );
    pub fn GetProxyHandlerFamily() -> *const c_void;

    pub fn GetInt8ArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut i8,
    );
    pub fn GetUint8ArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut u8,
    );
    pub fn GetUint8ClampedArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut u8,
    );
    pub fn GetInt16ArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut i16,
    );
    pub fn GetUint16ArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut u16,
    );
    pub fn GetInt32ArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut i32,
    );
    pub fn GetUint32ArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut u32,
    );
    pub fn GetFloat32ArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut f32,
    );
    pub fn GetFloat64ArrayLengthAndData(
        obj: *mut JSObject,
        length: *mut u32,
        isSharedMemory: *mut bool,
        data: *mut *mut f64,
    );

    pub fn NewJSAutoStructuredCloneBuffer(
        scope: JS::StructuredCloneScope,
        callbacks: *const JSStructuredCloneCallbacks,
    ) -> *mut JSAutoStructuredCloneBuffer;
    pub fn DeleteJSAutoStructuredCloneBuffer(buf: *mut JSAutoStructuredCloneBuffer);
    pub fn GetLengthOfJSStructuredCloneData(data: *mut JSStructuredCloneData) -> usize;
    pub fn CopyJSStructuredCloneData(src: *mut JSStructuredCloneData, dest: *mut u8);
    pub fn WriteBytesToJSStructuredCloneData(
        src: *const u8,
        len: usize,
        dest: *mut JSStructuredCloneData,
    ) -> bool;

    pub fn JSEncodeStringToUTF8(
        cx: *mut JSContext,
        string: JS::HandleString,
    ) -> *mut ::libc::c_char;

    pub fn IsDebugBuild() -> bool;
}

#[test]
fn jsglue_cpp_configured_correctly() {
    assert_eq!(cfg!(feature = "debugmozjs"), unsafe { IsDebugBuild() });
}
