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

mod bindings; // High-level bindings for C++ data structures.
mod compile; // Cranelift function compiler.
mod isa; // `TargetISA` configuration.
mod utils; // Helpers for other source files.
mod wasm2clif; // WebAssembly to Cranelift translation callbacks.

use log::{self, error, info};
use std::ptr;

use crate::bindings::{CompiledFunc, FuncCompileInput, ModuleEnvironment, StaticEnvironment};
use crate::compile::BatchCompiler;

#[no_mangle]
pub extern "C" fn cranelift_initialize() {
    // Gecko might set a logger before we do, which is all fine; try to initialize ours, and reset
    // the FilterLevel env_logger::try_init might have set to what it was in case of initialization
    // failure
    let filter = log::max_level();
    match env_logger::try_init() {
        Ok(_) => {}
        Err(_) => {
            log::set_max_level(filter);
        }
    }
}

/// Allocate a compiler for a module environment and return an opaque handle.
///
/// This is declared in `clifapi.h`.
#[no_mangle]
pub unsafe extern "C" fn cranelift_compiler_create<'a, 'b>(
    static_env: *const StaticEnvironment,
    env: *const bindings::LowLevelModuleEnvironment,
) -> *mut BatchCompiler<'a, 'b> {
    let env = env.as_ref().unwrap();
    let static_env = static_env.as_ref().unwrap();
    match BatchCompiler::new(static_env, ModuleEnvironment::new(env)) {
        Ok(compiler) => Box::into_raw(Box::new(compiler)),
        Err(err) => {
            error!("When constructing the batch compiler: {}", err);
            ptr::null_mut()
        }
    }
}

/// Deallocate compiler.
///
/// This is declared in `clifapi.h`.
#[no_mangle]
pub unsafe extern "C" fn cranelift_compiler_destroy(compiler: *mut BatchCompiler) {
    assert!(
        !compiler.is_null(),
        "NULL pointer passed to cranelift_compiler_destroy"
    );
    // Convert the pointer back into the box it came from. Then drop it.
    let _box = Box::from_raw(compiler);
}

/// Compile a single function.
///
/// This is declared in `clifapi.h`.
#[no_mangle]
pub unsafe extern "C" fn cranelift_compile_function(
    compiler: *mut BatchCompiler,
    data: *const FuncCompileInput,
    result: *mut CompiledFunc,
) -> bool {
    let compiler = compiler.as_mut().unwrap();
    let data = data.as_ref().unwrap();

    // Reset the compiler to a clean state.
    compiler.clear();

    if let Err(e) = compiler.translate_wasm(data) {
        error!("Wasm translation error: {}\n", e);
        info!("Translated function: {}", compiler);
        return false;
    };

    if let Err(e) = compiler.compile(data.stackmaps()) {
        error!("Cranelift compilation error: {}\n", e);
        info!("Compiled function: {}", compiler);
        return false;
    };

    // TODO(bbouvier) if destroy is called while one of these objects is alive, you're going to
    // have a bad time. Would be nice to be able to enforce lifetimes accross languages, somehow.
    let result = result.as_mut().unwrap();
    result.reset(&compiler.current_func);

    true
}
