/* Copyright 2018 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#![allow(unused)]
#![warn(unused_must_use)]

extern crate cranelift_codegen;
extern crate cranelift_wasm;
#[macro_use]
extern crate target_lexicon;
#[macro_use]
extern crate log;
extern crate env_logger;

mod baldrapi;  // Low-level C API, ignore this.
mod baldrdash; // High-level Rust API, use this.
mod compile;   // Cranelift function compiler.
mod cpu;       // CPU detection and `TargetISA` configuration.
mod utils;     // Helpers for other source files.
mod wasm2clif; // WebAssembly to Cranelift translation callbacks.

use baldrdash::{ CompiledFunc, FuncCompileInput, ModuleEnvironment, StaticEnvironment };
use compile::BatchCompiler;
use std::ptr;

#[no_mangle]
pub extern "C" fn cranelift_initialize() {
    // Gecko might set a logger before we do, which is all fine; just try to
    // initialize it, and silently ignore a re-initialization failure.
    let _ = env_logger::try_init();
}

/// Allocate a compiler for a module environment and return an opaque handle.
///
/// This is declared in `clifapi.h`.
#[no_mangle]
pub extern "C" fn cranelift_compiler_create<'a, 'b>(
    static_env: *const StaticEnvironment,
    env: *const baldrapi::CraneliftModuleEnvironment,
) -> *mut BatchCompiler<'a, 'b> {
    let env = unsafe { env.as_ref().unwrap() };
    let static_env = unsafe { static_env.as_ref().unwrap() };
    match BatchCompiler::new(static_env, ModuleEnvironment::new(env)) {
        Ok(compiler) => Box::into_raw(Box::new(compiler)),
        Err(err) => {
            error!("When constructing the batch compiler: {}", err);
            ptr::null_mut()
        },
    }
}

/// Deallocate compiler.
///
/// This is declared in `clifapi.h`.
#[no_mangle]
pub extern "C" fn cranelift_compiler_destroy(compiler: *mut BatchCompiler) {
    assert!(
        !compiler.is_null(),
        "NULL pointer passed to cranelift_compiler_destroy"
    );
    // Convert the pointer back into the box it came from. Then drop it.
    let _box = unsafe { Box::from_raw(compiler) };
}

/// Compile a single function.
///
/// This is declared in `clifapi.h`.
#[no_mangle]
pub extern "C" fn cranelift_compile_function(
    compiler: *mut BatchCompiler,
    data: *const FuncCompileInput,
    result: *mut CompiledFunc,
) -> bool {
    let compiler = unsafe { compiler.as_mut().unwrap() };
    let data = unsafe { data.as_ref().unwrap() };

    if let Err(e) = compiler.translate_wasm(data) {
        error!("Wasm translation error: {}\n{}", e, compiler);
        return false;
    };

    if let Err(e) = compiler.compile() {
        error!("Cranelift compilation error: {}\n{}", e, compiler);
        return false;
    };

    // TODO(bbouvier) if destroy is called while one of these objects is alive, you're going to
    // have a bad time. Would be nice to be able to enforce lifetimes accross languages, somehow.
    let result = unsafe { result.as_mut().unwrap() };
    result.reset(&compiler.current_func);

    true
}
