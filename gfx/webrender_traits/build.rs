/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(all(feature = "serde_codegen", not(feature = "serde_derive")))]
mod inner {
    extern crate serde_codegen;

    use std::env;
    use std::path::Path;

    pub fn main() {
        let out_dir = env::var_os("OUT_DIR").unwrap();

        let src = Path::new("src/types.rs");
        let dst = Path::new(&out_dir).join("types.rs");

        serde_codegen::expand(&src, &dst).unwrap();
        println!("cargo:rerun-if-changed=src/types.rs");
    }
}

#[cfg(all(feature = "serde_derive", not(feature = "serde_codegen")))]
mod inner {
    pub fn main() {}
}

#[cfg(all(feature = "serde_codegen", feature = "serde_derive"))]
mod inner {
    pub fn main() {
        panic!("serde_codegen and serde_derive are both used. \
               You probably forgot --no-default-features.")
    }
}

#[cfg(not(any(feature = "serde_codegen", feature = "serde_derive")))]
mod inner {
    pub fn main() {
        panic!("Neither serde_codegen nor serde_derive are used. "
               "You probably want --features serde_derive --no-default-features.")
    }
}

fn main() {
    inner::main();
}
