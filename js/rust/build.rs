/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate bindgen;
extern crate cmake;
extern crate env_logger;
extern crate glob;

use std::env;
use std::path;

fn main() {
    env_logger::init();
    build_jsapi_bindings();
    build_jsglue_cpp();
}

/// Build the ./src/jsglue.cpp file containing C++ glue methods built on top of
/// JSAPI.
fn build_jsglue_cpp() {
    let dst = cmake::Config::new(".").build();

    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=static=jsglue");
    println!("cargo:rerun-if-changed=src/jsglue.cpp");
}

/// Find the public include directory within our mozjs-sys crate dependency.
fn get_mozjs_include_dir() -> path::PathBuf {
    let out_dir = env::var("OUT_DIR")
        .expect("cargo should invoke us with the OUT_DIR env var set");

    let mut target_build_dir = path::PathBuf::from(out_dir);
    target_build_dir.push("../../");

    let mut include_dir_glob = target_build_dir.display().to_string();
    include_dir_glob.push_str("mozjs_sys-*/out/dist/include");

    let entries = glob::glob(&include_dir_glob)
        .expect("Should find entries for mozjs-sys include dir");

    for entry in entries {
        if let Ok(path) = entry {
            return path.canonicalize()
                .expect("Should canonicalize include path");
        }
    }

    panic!("Should have found either a mozjs_sys in target/debug or in target/release");
}

/// Invoke bindgen on the JSAPI headers to produce raw FFI bindings for use from
/// Rust.
///
/// To add or remove which functions, types, and variables get bindings
/// generated, see the `const` configuration variables below.
fn build_jsapi_bindings() {
    let mut builder = bindgen::builder()
        .rust_target(bindgen::RustTarget::Stable_1_19)
        .header("./etc/wrapper.hpp")
        .raw_line("pub use self::root::*;")
        // Translate every enum with the "rustified enum" strategy. We should
        // investigate switching to the "constified module" strategy, which has
        // similar ergonomics but avoids some potential Rust UB footguns.
        .rustified_enum(".*")
        .enable_cxx_namespaces();

    if cfg!(feature = "debugmozjs") {
        builder = builder
            .clang_arg("-DJS_GC_ZEAL")
            .clang_arg("-DDEBUG")
            .clang_arg("-DJS_DEBUG");
    }

    if cfg!(feature = "bigint") {
        builder = builder.clang_arg("-DENABLE_BIGINT");
    }

    let include_dir = get_mozjs_include_dir();
    let include_dir = include_dir.to_str()
        .expect("Path to mozjs include dir should be utf-8");
    builder = builder.clang_arg("-I");
    builder = builder.clang_arg(include_dir);

    for ty in UNSAFE_IMPL_SYNC_TYPES {
        builder = builder.raw_line(format!("unsafe impl Sync for {} {{}}", ty));
    }

    for extra in EXTRA_CLANG_FLAGS {
        builder = builder.clang_arg(*extra);
    }

    for ty in WHITELIST_TYPES {
        builder = builder.whitelist_type(ty);
    }

    for var in WHITELIST_VARS {
        builder = builder.whitelist_var(var);
    }

    for func in WHITELIST_FUNCTIONS {
        builder = builder.whitelist_function(func);
    }

    if cfg!(feature = "bigint") {
        builder = builder.whitelist_type("JS::BigInt");
    }

    for ty in OPAQUE_TYPES {
        builder = builder.opaque_type(ty);
    }

    for ty in BLACKLIST_TYPES {
        builder = builder.blacklist_type(ty);
    }

    let bindings = builder.generate()
        .expect("Should generate JSAPI bindings OK");

    let out = path::PathBuf::from(env::var("OUT_DIR").unwrap());

    if cfg!(feature = "debugmozjs") {
        bindings.write_to_file(out.join("jsapi_debug.rs"))
            .expect("Should write bindings to file OK");
    } else {
        bindings.write_to_file(out.join("jsapi.rs"))
            .expect("Should write bindings to file OK");
    }

    println!("cargo:rerun-if-changed=etc/wrapper.hpp");
}

/// JSAPI types for which we should implement `Sync`.
const UNSAFE_IMPL_SYNC_TYPES: &'static [&'static str] = &[
    "JSClass",
    "JSFunctionSpec",
    "JSNativeWrapper",
    "JSPropertySpec",
    "JSTypedMethodJitInfo",
];

/// Flags passed through bindgen directly to Clang.
const EXTRA_CLANG_FLAGS: &'static [&'static str] = &[
    "-x", "c++",
    "-std=gnu++14",
    "-fno-sized-deallocation",
    "-DRUST_BINDGEN",
];

/// Types which we want to generate bindings for (and every other type they
/// transitively use).
const WHITELIST_TYPES: &'static [&'static str] = &[
    "JS::AutoCheckCannotGC",
    "JS::AutoIdVector",
    "JS::AutoObjectVector",
    "JS::CallArgs",
    "js::Class",
    "JS::RealmOptions",
    "JS::ContextOptions",
    "js::DOMCallbacks",
    "js::DOMProxyShadowsResult",
    "js::ESClass",
    "JS::ForOfIterator",
    "JS::Handle",
    "JS::HandleFunction",
    "JS::HandleId",
    "JS::HandleObject",
    "JS::HandleString",
    "JS::HandleValue",
    "JS::HandleValueArray",
    "JS::IsAcceptableThis",
    "JSAutoRealm",
    "JSAutoStructuredCloneBuffer",
    "JSClass",
    "JSClassOps",
    "JSContext",
    "JSErrNum",
    "JSErrorCallback",
    "JSErrorFormatString",
    "JSErrorReport",
    "JSExnType",
    "JSFlatString",
    "JSFunction",
    "JSFunctionSpec",
    "JS::GCDescription",
    "JSGCInvocationKind",
    "JSGCMode",
    "JSGCParamKey",
    "JS::GCProgress",
    "JSGCStatus",
    "JSJitCompilerOption",
    "JSJitGetterCallArgs",
    "JSJitMethodCallArgs",
    "JSJitSetterCallArgs",
    "JSNativeWrapper",
    "JSPropertySpec",
    "JSProtoKey",
    "JSObject",
    "JSString",
    "JSStructuredCloneReader",
    "JSStructuredCloneWriter",
    "JSScript",
    "JSType",
    "JSTypedMethodJitInfo",
    "JSValueTag",
    "JSValueType",
    "JSVersion",
    "JSWrapObjectCallbacks",
    "jsid",
    "JS::Latin1Char",
    "JS::detail::MaybeWrapped",
    "JS::MutableHandle",
    "JS::MutableHandleObject",
    "JS::MutableHandleValue",
    "JS::NativeImpl",
    "js::ObjectOps",
    "JS::ObjectOpResult",
    "JS::PromiseState",
    "JS::PropertyDescriptor",
    "JS::Rooted",
    "JS::RootedObject",
    "JS::RootingContext",
    "JS::RootKind",
    "js::Scalar::Type",
    "JS::ServoSizes",
    "js::shadow::Object",
    "js::shadow::ObjectGroup",
    "JS::SourceBufferHolder",
    "js::StackFormat",
    "JSStructuredCloneCallbacks",
    "JS::Symbol",
    "JS::SymbolCode",
    "JS::TraceKind",
    "JS::TransferableOwnership",
    "JS::Value",
    "JS::WarningReporter",
    "JS::shadow::Zone",
    "JS::Zone",
];

/// Global variables we want to generate bindings to.
const WHITELIST_VARS: &'static [&'static str] = &[
    "JS_STRUCTURED_CLONE_VERSION",
    "JSCLASS_.*",
    "JSFUN_.*",
    "JSID_TYPE_VOID",
    "JSITER_.*",
    "JSPROP_.*",
    "JS::FalseHandleValue",
    "JS::NullHandleValue",
    "JS::TrueHandleValue",
    "JS::UndefinedHandleValue",
];

/// Functions we want to generate bindings to.
const WHITELIST_FUNCTIONS: &'static [&'static str] = &[
    "INTERNED_STRING_TO_JSID",
    "JS::ExceptionStackOrNull",
    "JS_AddExtraGCRootsTracer",
    "JS_AddInterruptCallback",
    "JS::AddPromiseReactions",
    "js::AddRawValueRoot",
    "JS_AlreadyHasOwnPropertyById",
    "JS_AtomizeAndPinString",
    "js::AssertSameCompartment",
    "JS::BuildStackString",
    "JS::Call",
    "JS_CallFunctionName",
    "JS_CallFunctionValue",
    "JS::CallOriginalPromiseThen",
    "JS::CallOriginalPromiseResolve",
    "JS::CallOriginalPromiseReject",
    "JS::CompileFunction",
    "JS::Construct",
    "JS::ContextOptionsRef",
    "JS_CopyPropertiesFrom",
    "JS::CurrentGlobalOrNull",
    "JS_DeletePropertyById",
    "js::detail::IsWindowSlow",
    "JS::Evaluate",
    "JS_ForwardGetPropertyTo",
    "JS_ForwardSetPropertyTo",
    "JS::GCTraceKindToAscii",
    "js::GetArrayBufferLengthAndData",
    "js::GetArrayBufferViewLengthAndData",
    "JS_GetErrorPrototype",
    "js::GetFunctionNativeReserved",
    "JS_GetFunctionPrototype",
    "js::GetGlobalForObjectCrossCompartment",
    "JS_GetIteratorPrototype",
    "js::GetObjectProto",
    "JS_GetObjectPrototype",
    "JS_GetObjectRuntime",
    "JS_GetOwnPropertyDescriptorById",
    "JS::GetPromiseResult",
    "JS::GetPromiseState",
    "JS_GetPropertyDescriptorById",
    "js::GetPropertyKeys",
    "JS_GetPrototype",
    "JS_GetRuntime",
    "js::GetStaticPrototype",
    "JS_HasOwnPropertyById",
    "JS_HasProperty",
    "JS_HasPropertyById",
    "JS::HeapObjectPostBarrier",
    "JS::HeapValuePostBarrier",
    "JS_InitializePropertiesFromCompatibleNativeObject",
    "JS::InitSelfHostedCode",
    "JS::IsConstructor",
    "JS::IsPromiseObject",
    "JS_BeginRequest",
    "JS_ClearPendingException",
    "JS_DefineElement",
    "JS_DefineFunction",
    "JS_DefineFunctions",
    "JS_DefineProperties",
    "JS_DefineProperty",
    "JS_DefinePropertyById",
    "JS_DefineUCProperty",
    "JS::detail::InitWithFailureDiagnostic",
    "JS_DestroyContext",
    "JS::DisableIncrementalGC",
    "js::Dump.*",
    "JS_EncodeStringToUTF8",
    "JS_EndRequest",
    "JS::EnterRealm",
    "JS_EnumerateStandardClasses",
    "JS_ErrorFromException",
    "JS_FireOnNewGlobalObject",
    "JS_free",
    "JS_GC",
    "JS_GetArrayBufferData",
    "JS_GetArrayBufferViewType",
    "JS_GetFloat32ArrayData",
    "JS_GetFloat64ArrayData",
    "JS_GetFunctionObject",
    "JS_GetGCParameter",
    "JS_GetInt16ArrayData",
    "JS_GetInt32ArrayData",
    "JS_GetInt8ArrayData",
    "JS_GetLatin1StringCharsAndLength",
    "JS_GetParentRuntime",
    "JS_GetPendingException",
    "JS_GetProperty",
    "JS_GetPropertyById",
    "js::GetPropertyKeys",
    "JS_GetPrototype",
    "JS_GetReservedSlot",
    "JS::GetScriptedCallerGlobal",
    "JS_GetTwoByteStringCharsAndLength",
    "JS_GetUint16ArrayData",
    "JS_GetUint32ArrayData",
    "JS_GetUint8ArrayData",
    "JS_GetUint8ClampedArrayData",
    "JS::GetWellKnownSymbol",
    "JS_GlobalObjectTraceHook",
    "JS::HideScriptedCaller",
    "JS_InitStandardClasses",
    "JS_IsArrayObject",
    "JS_IsExceptionPending",
    "JS_IsGlobalObject",
    "JS::IsCallable",
    "JS::LeaveRealm",
    "JS_LinkConstructorAndPrototype",
    "JS_MayResolveStandardClass",
    "JS_NewArrayBuffer",
    "JS_NewArrayObject",
    "JS_NewContext",
    "JS_NewFloat32Array",
    "JS_NewFloat64Array",
    "JS_NewFunction",
    "js::NewFunctionWithReserved",
    "JS_NewGlobalObject",
    "JS_NewInt16Array",
    "JS_NewInt32Array",
    "JS_NewInt8Array",
    "JS_NewObject",
    "JS_NewObjectWithGivenProto",
    "JS_NewObjectWithoutMetadata",
    "JS_NewObjectWithUniqueType",
    "JS_NewPlainObject",
    "JS::NewPromiseObject",
    "JS_NewStringCopyN",
    "JS_NewUCStringCopyN",
    "JS_NewUint16Array",
    "JS_NewUint32Array",
    "JS_NewUint8Array",
    "JS_NewUint8ClampedArray",
    "js::ObjectClassName",
    "JS_ObjectIsDate",
    "JS_ParseJSON",
    "JS_ReadBytes",
    "JS_ReadStructuredClone",
    "JS_ReadUint32Pair",
    "JS_RemoveExtraGCRootsTracer",
    "js::RemoveRawValueRoot",
    "JS_ReportErrorASCII",
    "JS_ReportErrorNumberUTF8",
    "JS_RequestInterruptCallback",
    "JS_ResolveStandardClass",
    "js::RunJobs",
    "JS_SameValue",
    "js::SetDOMCallbacks",
    "js::SetDOMProxyInformation",
    "JS::SetEnqueuePromiseJobCallback",
    "js::SetFunctionNativeReserved",
    "JS_SetGCCallback",
    "JS::SetGCSliceCallback",
    "JS_SetGCParameter",
    "JS_SetGCZeal",
    "JS::SetGetIncumbentGlobalCallback",
    "JS_SetGlobalJitCompilerOption",
    "JS_SetImmutablePrototype",
    "JS_SetNativeStackQuota",
    "JS_SetOffthreadIonCompilationEnabled",
    "JS_SetParallelParsingEnabled",
    "JS_SetPendingException",
    "js::SetPreserveWrapperCallback",
    "JS::SetPromiseRejectionTrackerCallback",
    "JS_SetPrototype",
    "js::SetWindowProxy",
    "js::SetWindowProxyClass",
    "JS_SetProperty",
    "JS_SetReservedSlot",
    "JS_SetWrapObjectCallbacks",
    "JS_ShutDown",
    "JS_SplicePrototype",
    "js::StopDrainingJobQueue",
    "JS_StrictPropertyStub",
    "JS_StringEqualsAscii",
    "JS_StringHasLatin1Chars",
    "JS_WrapObject",
    "JS_WrapValue",
    "JS_WriteBytes",
    "JS_WriteStructuredClone",
    "JS_WriteUint32Pair",
    "JS::ResolvePromise",
    "JS::RejectPromise",
    "JS::SetWarningReporter",
    "js::ToBooleanSlow",
    "js::ToInt32Slow",
    "js::ToInt64Slow",
    "js::ToNumberSlow",
    "js::ToStringSlow",
    "js::ToUint16Slow",
    "js::ToUint32Slow",
    "js::ToUint64Slow",
    "JS_TransplantObject",
    "js::detail::ToWindowProxyIfWindowSlow",
    "JS::UnhideScriptedCaller",
    "js::UnwrapArrayBuffer",
    "js::UnwrapArrayBufferView",
    "js::UnwrapFloat32Array",
    "js::UnwrapFloat64Array",
    "js::UnwrapInt16Array",
    "js::UnwrapInt32Array",
    "js::UnwrapInt8Array",
    "js::UnwrapUint16Array",
    "js::UnwrapUint32Array",
    "js::UnwrapUint8Array",
    "js::UnwrapUint8ClampedArray",
    "js::UseInternalJobQueues",
    "JS_ValueToFunction",
];

/// Types that should be treated as an opaque blob of bytes whenever they show
/// up within a whitelisted type.
///
/// These are types which are too tricky for bindgen to handle, and/or use C++
/// features that don't have an equivalent in rust, such as partial template
/// specialization.
const OPAQUE_TYPES: &'static [&'static str] = &[
    "JS::ReadOnlyCompileOptions",
    "mozilla::BufferList",
    "mozilla::UniquePtr.*",
    "JS::Rooted<JS::Auto.*Vector.*>",
    "JS::Auto.*Vector"
];

/// Types for which we should NEVER generate bindings, even if it is used within
/// a type or function signature that we are generating bindings for.
const BLACKLIST_TYPES: &'static [&'static str] = &[
    // We provide our own definition because we need to express trait bounds in
    // the definition of the struct to make our Drop implementation correct.
    "JS::Heap",
];
