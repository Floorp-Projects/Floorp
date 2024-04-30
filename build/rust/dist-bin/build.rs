/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

fn main() {
    // Hack around the lack of dist/bin in the search path set up by
    // various third party crates for nss. Bug 1892894.
    println!(
        "cargo:rustc-link-search=native={}",
        mozbuild::TOPOBJDIR
            .join("dist")
            .join("bin")
            .to_str()
            .unwrap()
    );
}
