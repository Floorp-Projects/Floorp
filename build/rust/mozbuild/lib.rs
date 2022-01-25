/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use once_cell::sync::Lazy;
use std::env;
use std::path::PathBuf;

pub static TOPOBJDIR: Lazy<PathBuf> = Lazy::new(|| {
    let path = PathBuf::from(
        env::var_os("MOZ_TOPOBJDIR").expect("MOZ_TOPOBJDIR must be set in the environment"),
    );
    assert!(
        path.is_absolute() && path.is_dir(),
        "MOZ_TOPOBJDIR must be an absolute directory, was: {}",
        path.display()
    );
    path
});

#[macro_export]
macro_rules! objdir_path {
    ($path:literal) => {
        concat!(env!("MOZ_TOPOBJDIR"), "/", $path)
    };
}

pub static TOPSRCDIR: Lazy<PathBuf> = Lazy::new(|| {
    let path =
        PathBuf::from(env::var_os("MOZ_SRC").expect("MOZ_SRC must be set in the environment"));
    assert!(
        path.is_absolute() && path.is_dir(),
        "MOZ_SRC must be an absolute directory, was: {}",
        path.display()
    );
    path
});

pub mod config {
    include!(env!("BUILDCONFIG_RS"));
}
