/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate pkg_config;

fn main() {

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/lib.cpp");

    cc::Build::new()
        .file("src/lib.cpp")
        .file("src/viewporter-protocol.c")
        .file("src/xdg-shell-protocol.c")
        .compile("wayland");

    println!("cargo:rustc-link-lib=dylib=stdc++");

    pkg_config::Config::new()
	    .atleast_version("1")
	    .probe("egl")
	    .unwrap();
    pkg_config::Config::new()
	    .atleast_version("1")
	    .probe("gl")
	    .unwrap();
    pkg_config::Config::new()
	    .atleast_version("1")
	    .probe("wayland-client")
	    .unwrap();
	pkg_config::Config::new()
	    .atleast_version("1")
	    .probe("wayland-egl")
	    .unwrap();
}
