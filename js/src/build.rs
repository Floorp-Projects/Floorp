// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

extern crate num_cpus;
extern crate glob;

use std::env;
use std::path;
use std::process::{Command, Stdio};

fn main() {
    let out_dir = env::var("OUT_DIR").expect("Should have env var OUT_DIR");
    let target = env::var("TARGET").expect("Should have env var TARGET");

    let js_src = env::var("CARGO_MANIFEST_DIR").expect("Should have env var CARGO_MANIFEST_DIR");

    env::set_var("MAKEFLAGS", format!("-j{}", num_cpus::get()));

    // The Rust package used in Spidermonkey inherits the default profile from the toplevel
    // Cargo directory. In particular, both profiles (dev or release) set panic=abort,
    // which defines an exception handling personality that will trigger calls to abort().
    // The Rust bindings package wants to be able to unwind at runtime, so override the
    // profile's value here.
    env::set_var("RUSTFLAGS", "-C panic=unwind");

    env::set_current_dir(&js_src).unwrap();

    let variant = format!("{}{}",
                          if cfg!(feature = "bigint") { "bigint" } else { "plain" },
                          if cfg!(feature = "debugmozjs") { "debug" } else { "" });

    let python = env::var("PYTHON").unwrap_or("python2.7".into());
    let mut cmd = Command::new(&python);
    cmd.args(&["./devtools/automation/autospider.py",
               // Only build SpiderMonkey, don't run all the tests.
               "--build-only",
               // Disable Mozilla's jemalloc; Rust has its own jemalloc that we
               // can swap in instead and everything using a single malloc is
               // good.
               "--no-jemalloc",
               // Don't try to clobber the output directory. Without
               // this option, the build will fail because the directory
               // already exists but wasn't created by autospider.
               "--dep",
               "--objdir", &out_dir,
               &variant])
        .env("SOURCE", &js_src)
        .env("PWD", &js_src)
        .stdout(Stdio::inherit())
        .stderr(Stdio::inherit());
    println!("Running command: {:?}", cmd);
    let result = cmd
        .status()
        .expect("Should spawn autospider OK");
    assert!(result.success(), "autospider should exit OK");

    println!("cargo:rustc-link-search=native={}/js/src/build", out_dir);
    println!("cargo:rustc-link-search=native={}/js/src", out_dir);
    println!("cargo:rustc-link-lib=static=js_static");

    maybe_add_spidermonkey_rust_lib();

    println!("cargo:rustc-link-search=native={}/dist/bin", out_dir);
    println!("cargo:rustc-link-lib=nspr4");

    if cfg!(feature = "bigint") {
        println!("cargo:rustc-link-lib=gmp");
    }

    if target.contains("windows") {
        println!("cargo:rustc-link-lib=winmm");
        if target.contains("gnu") {
            println!("cargo:rustc-link-lib=stdc++");
        }
    } else {
        println!("cargo:rustc-link-lib=stdc++");
    }

    println!("cargo:outdir={}", out_dir);
}

/// Find if Spidermonkey built the Spidermonkey Rust library, and add it to the
/// link if it was the case.
fn maybe_add_spidermonkey_rust_lib() {
    let out_dir = env::var("OUT_DIR")
        .expect("cargo should invoke us with the OUT_DIR env var set");

    let mut target_build_dir = path::PathBuf::from(out_dir);
    target_build_dir.push("../../");

    let mut build_dir = target_build_dir.display().to_string();
    build_dir.push_str("mozjs_sys-*/out/*/debug");

    let entries = match glob::glob(&build_dir){
        Err(_) => { return; }
        Ok(entries) => entries
    };

    for entry in entries {
        if let Ok(path) = entry {
            let path = path.canonicalize()
                .expect("Should canonicalize debug build path");
            let path = path.to_str()
                .expect("Should be utf8");
            println!("cargo:rustc-link-search=native={}", path);
            println!("cargo:rustc-link-lib=static=jsrust");
            return;
        }
    }
}
