/*
 * Copyright 2018 Mozilla Foundation
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

//! Build script for the Baldr <-> Cranelift bindings.
//!
//! This file is executed by cargo when this crate is built. It generates the
//! `$OUT_DIR/bindings.rs` file which is then included by `src/bindings/low_level.rs`.

extern crate bindgen;

use std::env;
use std::fs::File;
use std::io::prelude::*;
use std::path::PathBuf;

fn main() {
    // Tell Cargo to regenerate the bindings if the header file changes.
    println!("cargo:rerun-if-changed=baldrapi.h");

    let mut generator = bindgen::builder()
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .disable_name_namespacing()
        .size_t_is_usize(true)
        // We whitelist the Baldr C functions and get the associated types for free.
        .whitelist_function("env_.*")
        .whitelist_function("global_.*")
        .whitelist_function("table_.*")
        .whitelist_function("funcType_.*")
        .whitelist_function("stackmaps_.*")
        .whitelist_type("Cranelift.*")
        // The enum classes defined in baldrapi.h and WasmBinaryConstants are all Rust-safe.
        .rustified_enum("BD_.*|Trap|TypeCode|FuncTypeIdDescKind")
        .whitelist_type("BD_.*|Trap|TypeCode|FuncTypeIdDescKind")
        .header("baldrapi.h")
        .clang_args(&[
            "-x",
            "c++",
            "-std=gnu++14",
            "-fno-sized-deallocation",
            "-fno-aligned-new",
            "-DRUST_BINDGEN",
        ])
        .clang_arg("-I../..");

    match env::var_os("MOZ_TOPOBJDIR") {
        Some(path) => {
            let path = PathBuf::from(path).join("js/src/rust/extra-bindgen-flags");

            let mut extra_flags = String::new();
            File::open(&path)
                .expect("Failed to open extra-bindgen-flags file")
                .read_to_string(&mut extra_flags)
                .expect("Failed to read extra-bindgen-flags file");

            let display_path = path.to_str().expect("path is utf8 encoded");
            println!("cargo:rerun-if-changed={}", display_path);

            let extra_flags: Vec<String> = extra_flags
                .split_whitespace()
                .map(|s| s.to_owned())
                .collect();
            for flag in extra_flags {
                generator = generator.clang_arg(flag);
            }
        }
        None => {
            println!("cargo:warning=MOZ_TOPOBJDIR should be set by default, otherwise the build is not guaranted to finish.");
        }
    }

    let command_line_opts = generator.command_line_flags();

    // In case of error, bindgen prints to stderr, and the yielded error is the empty type ().
    let bindings = generator.generate().unwrap_or_else(|_err| {
        panic!(
            r#"Unable to generate baldrapi.h bindings:
- flags: {}
"#,
            command_line_opts.join(" "),
        );
    });

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
