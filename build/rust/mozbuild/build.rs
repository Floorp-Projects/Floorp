/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::path::PathBuf;

fn main() {
    let out_dir = PathBuf::from(std::env::var_os("OUT_DIR").unwrap());
    if let Some(topobjdir) = out_dir
        .ancestors()
        .find(|dir| dir.join("config.status").exists())
    {
        println!(
            "cargo:rustc-env=BUILDCONFIG_RS={}",
            topobjdir
                .join("build/rust/mozbuild/buildconfig.rs")
                .display()
        );
    }
}
