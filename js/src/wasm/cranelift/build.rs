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
//! `$OUT_DIR/bindings.rs` file which is then included by `src/baldrapi.rs`.

extern crate bindgen;

use std::env;
use std::fs::File;
use std::io::prelude::*;
use std::path::PathBuf;

enum Arch {
    X86,
    X64,
    Arm,
    Aarch64
}

fn main() {
    // Tell Cargo to regenerate the bindings if the header file changes.
    println!("cargo:rerun-if-changed=baldrapi.h");

    let mut bindings = bindgen::builder()
        .disable_name_namespacing()
        // We whitelist the Baldr C functions and get the associated types for free.
        .whitelist_function("env_.*")
        .whitelist_function("global_.*")
        .whitelist_function("table_.*")
        .whitelist_function("funcType_.*")
        .whitelist_type("Cranelift.*")
        // The enum classes defined in baldrapi.h and WasmBinaryConstants are all Rust-safe.
        .rustified_enum("BD_.*|Trap|TypeCode|FuncTypeIdDescKind")
        .whitelist_type("BD_.*|Trap|TypeCode|FuncTypeIdDescKind")
        .header("baldrapi.h")
        .clang_args(&["-x", "c++", "-std=gnu++14", "-fno-sized-deallocation", "-DRUST_BINDGEN"])
        .clang_arg("-I../..");

    let arch = {
        let target_arch = env::var("CARGO_CFG_TARGET_ARCH");
        match target_arch.as_ref().map(|x| x.as_str()) {
            Ok("aarch64") => Arch::Aarch64,
            Ok("arm") => Arch::Arm,
            Ok("x86") => Arch::X86,
            Ok("x86_64") => Arch::X64,
            _ => panic!("unknown arch")
        }
    };

    match env::var("CARGO_CFG_TARGET_OS").as_ref().map(|x| x.as_str()) {
        Ok("android") => {
            bindings = bindings.clang_arg("-DOS_ANDROID=1");
            bindings = match arch {
                Arch::Aarch64 => { bindings.clang_arg("--target=aarch64-linux-android") }
                Arch::Arm => { bindings.clang_arg("--target=armv7-linux-androideabi") }
                Arch::X86 => { bindings.clang_arg("--target=i686-linux-android") }
                Arch::X64 => { bindings.clang_arg("--target=x86_64-linux-android") }
            };
        }
        Ok("linux") | Ok("freebsd") | Ok("dragonfly") | Ok("openbsd") | Ok("bitrig") | Ok("netbsd")
            | Ok("macos") | Ok("ios") => {
            // Nothing to do in particular for these OSes, until proven the contrary.
        }
        Ok("windows") => {
            bindings = bindings.clang_arg("-DOS_WIN=1")
                .clang_arg("-DWIN32=1");
            bindings = match env::var("CARGO_CFG_TARGET_ENV").as_ref().map(|x| x.as_str()) {
                Ok("msvc") => {
                    bindings = bindings.clang_arg("-fms-compatibility-version=19");
                    bindings = bindings.clang_arg("-D_CRT_USE_BUILTIN_OFFSETOF");
                    bindings = bindings.clang_arg("-DHAVE_VISIBILITY_HIDDEN_ATTRIBUTE=1");
                    match arch {
                        Arch::X86 => { bindings.clang_arg("--target=i686-pc-win32") },
                        Arch::X64 => { bindings.clang_arg("--target=x86_64-pc-win32") },
                        Arch::Aarch64 => { bindings.clang_arg("--target=aarch64-pc-windows-msvc") }
                        _ => panic!("unknown Windows architecture for msvc build")
                    }
                }
                Ok("gnu") => {
                    match arch {
                        Arch::X86 => { bindings.clang_arg("--target=i686-pc-mingw32") },
                        Arch::X64 => { bindings.clang_arg("--target=x86_64-w64-mingw32") },
                        _ => panic!("unknown Windows architecture for gnu build")
                    }
                }
                _ => panic!("unknown Windows build environment")
            };
        }
        os => panic!("unknown target os {:?}!", os)
    }

    let path = PathBuf::from(env::var_os("MOZ_TOPOBJDIR").unwrap()).join("js/src/rust/extra-bindgen-flags");

    let mut extra_flags = String::new();
        File::open(&path)
            .expect("Failed to open extra-bindgen-flags file")
            .read_to_string(&mut extra_flags)
            .expect("Failed to read extra-bindgen-flags file");

    let display_path = path.to_str().expect("path is utf8 encoded");
    println!("cargo:rerun-if-changed={}", display_path);

    let extra_flags: Vec<String> = extra_flags.split_whitespace().map(|s| s.to_owned()).collect();
    for flag in extra_flags {
        bindings = bindings.clang_arg(flag);
    }

    let bindings = bindings
        .generate()
        .expect("Unable to generate baldrapi.h bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
