/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* LD_PRELOAD library that intercepts some libc functions and lowercases
 * paths under a given set of directories before calling the real libc
 * functions.
 *
 * The set of directories is defined with the LOWERCASE_DIRS environment
 * variable, separated with a `:`.
 *
 * Only the parts of the directories below the LOWERCASE_DIRS directories
 * are lowercased.
 *
 * For example, with LOWERCASE_DIRS=/Foo:/Bar:
 *   `/home/QuX` is unchanged.
 *   `/Foo/QuX` becomes `/Foo/qux`.
 *   `/foo/QuX` is unchanged.
 *   `/Bar/QuX` becomes `/Bar/qux`.
 *   etc.
 *
 * This is, by no means, supposed to be a generic LD_PRELOAD library. It
 * only intercepts the libc functions that matter in order to build Firefox.
 */

use std::borrow::Cow;
use std::env::{self, current_dir};
use std::ffi::{c_void, CStr, CString, OsStr, OsString};
use std::mem::transmute;
use std::os::raw::{c_char, c_int};
use std::os::unix::ffi::{OsStrExt, OsStringExt};
use std::path::{Path, PathBuf};
use std::ptr::null;

use once_cell::sync::Lazy;
use path_dedot::ParseDot;

#[cfg(not(all(target_arch = "x86_64", target_os = "linux", target_env = "gnu")))]
compile_error!("Platform is not supported");

static LOWERCASE_DIRS: Lazy<Vec<PathBuf>> = Lazy::new(|| match env::var_os("LOWERCASE_DIRS") {
    None => Vec::new(),
    Some(value) => value
        .as_bytes()
        .split(|&c| c == b':')
        .map(|p| canonicalize_path(Path::new(OsStr::from_bytes(p))).into_owned())
        .collect(),
});

fn canonicalize_path(path: &Path) -> Cow<Path> {
    let path = if path.is_absolute() {
        Cow::Borrowed(path)
    } else {
        match current_dir() {
            Ok(cwd) => Cow::Owned(cwd.join(path)),
            Err(_) => Cow::Borrowed(path),
        }
    };

    // TODO: avoid allocation when the path doesn't need .. / . removals.
    Cow::Owned(path.parse_dot().unwrap())
}

#[test]
fn test_canonicalize_path() {
    use std::env::set_current_dir;
    use std::iter::repeat;
    use tempfile::tempdir;

    fn do_test(curdir: &Path) {
        let foobarbaz = curdir.join("foo/bar/baz");

        assert_eq!(foobarbaz, canonicalize_path(Path::new("foo/bar/baz")));
        assert_eq!(foobarbaz, canonicalize_path(Path::new("./foo/bar/baz")));
        assert_eq!(foobarbaz, canonicalize_path(Path::new("foo/./bar/baz")));
        assert_eq!(foobarbaz, canonicalize_path(Path::new("foo/././bar/baz")));
        assert_eq!(
            foobarbaz,
            canonicalize_path(Path::new("foo/././bar/qux/../baz"))
        );
        assert_eq!(
            foobarbaz,
            canonicalize_path(Path::new("foo/./bar/../qux/../bar/baz"))
        );
        assert_eq!(
            foobarbaz,
            canonicalize_path(Path::new("foo/bar/./../../foo/bar/baz"))
        );

        let depth = curdir.components().count();
        for depth in depth..=depth + 1 {
            let path = repeat("..").take(depth).collect::<Vec<_>>();
            let mut path = path.join("/");
            path.push_str("/foo/bar/baz");

            assert_eq!(
                Path::new("/foo/bar/baz"),
                canonicalize_path(Path::new(&path))
            );
        }
    }

    let orig_curdir = current_dir().unwrap();

    do_test(&orig_curdir);

    let tempdir = tempdir().unwrap();
    set_current_dir(&tempdir).unwrap();

    do_test(tempdir.path());

    set_current_dir(orig_curdir).unwrap();
}

fn normalize_path(path: &CStr) -> Cow<CStr> {
    let orig_path = path;
    let path = Path::new(OsStr::from_bytes(orig_path.to_bytes()));
    match normalize_path_for_dirs(&path, &LOWERCASE_DIRS) {
        Cow::Borrowed(_) => Cow::Borrowed(orig_path),
        Cow::Owned(p) => Cow::Owned(CString::new(p.into_os_string().into_vec()).unwrap()),
    }
}

fn normalize_path_for_dirs<'a>(path: &'a Path, dirs: &[PathBuf]) -> Cow<'a, Path> {
    let orig_path = path;
    let path = canonicalize_path(path);

    for lowercase_dir in dirs.iter() {
        if path.starts_with(lowercase_dir) {
            // TODO: avoid allocation when the string doesn't actually need
            // modification.
            let mut lowercased_path = path.into_owned().into_os_string().into_vec();
            lowercased_path[lowercase_dir.as_os_str().as_bytes().len()..].make_ascii_lowercase();
            return Cow::Owned(OsString::from_vec(lowercased_path).into());
        }
    }

    Cow::Borrowed(orig_path)
}

#[test]
fn test_normalize_path() {
    let paths = vec![
        Path::new("/Foo/Bar").to_owned(),
        Path::new("/Qux").to_owned(),
        current_dir().unwrap().join("Fuga"),
    ];

    assert_eq!(
        normalize_path_for_dirs(Path::new("/foo/bar/Baz"), &paths),
        Path::new("/foo/bar/Baz")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("/Foo/Bar/Baz"), &paths),
        Path::new("/Foo/Bar/baz")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("/Foo/BarBaz"), &paths),
        Path::new("/Foo/BarBaz")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("/Foo/Bar"), &paths),
        Path::new("/Foo/Bar")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("/Foo/Bar/Baz/../Qux"), &paths),
        Path::new("/Foo/Bar/qux")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("/Foo/Bar/Baz/../../Qux"), &paths),
        Path::new("/Foo/Bar/Baz/../../Qux")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("/Qux/Foo/Bar/Baz"), &paths),
        Path::new("/Qux/foo/bar/baz")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("/foo/../Qux/Baz"), &paths),
        Path::new("/Qux/baz")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("fuga/Foo/Bar"), &paths),
        Path::new("fuga/Foo/Bar")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("Fuga/Foo/Bar"), &paths),
        current_dir().unwrap().join("Fuga/foo/bar")
    );
    assert_eq!(
        normalize_path_for_dirs(Path::new("Fuga/../Foo/Bar"), &paths),
        Path::new("Fuga/../Foo/Bar")
    );
}

macro_rules! wrappers {
    ($(fn $name:ident($( $a:ident : $t:ty ),*) $( -> $ret:ty)?;)*) => {
        $(
            paste::item! {
                #[allow(non_upper_case_globals)]
                static [< real $name >]: Lazy<extern "C" fn($($t),*) $( -> $ret)?> =
                    Lazy::new(|| unsafe {
                        transmute(libc::dlsym(
                            libc::RTLD_NEXT,
                            concat!(stringify!($name), "\0").as_ptr() as _
                        ))
                    });
                #[no_mangle]
                unsafe extern "C" fn $name($($a : $t),*) $(-> $ret)? {
                    $( wrappers!(@normalize ($a: $t)); )*
                    [< real $name >]($($a),*)
                }
            }
        )*
    };
    (@normalize ($a:ident: *const c_char)) => {
        let $a = if $a.is_null() {
            None
        } else {
            Some(normalize_path(CStr::from_ptr($a)))
        };
        let $a = $a.as_ref().map(|p| p.as_ptr()).unwrap_or(null());
    };
    (@normalize ($a:ident: $t:ty)) => {}
}

// Note: actual definitions for e.g. fopen/fopen64 would be using c_char
// instead of c_void for mode, but the wrappers macro treats all `*const c_char`s
// as "to maybe be lowercased".
wrappers! {
    fn open(path: *const c_char, flags: c_int, mode: libc::mode_t) -> c_int;
    fn open64(path: *const c_char, flags: c_int, mode: libc::mode_t) -> c_int;
    fn fopen(path: *const c_char, mode: *const c_void) -> *mut libc::FILE;
    fn fopen64(path: *const c_char, mode: *const c_void) -> *mut libc::FILE;

    fn opendir(path: *const c_char) -> *mut libc::DIR;

    fn stat(path: *const c_char, buf: *mut libc::stat) -> c_int;
    fn stat64(path: *const c_char, buf: *mut libc::stat64) -> c_int;
    fn __xstat(ver: c_int, path: *const c_char, buf: *mut libc::stat) -> c_int;
    fn __xstat64(ver: c_int, path: *const c_char, buf: *mut libc::stat64) -> c_int;

    fn __lxstat(ver: c_int, path: *const c_char, buf: *mut libc::stat) -> c_int;
    fn __lxstat64(ver: c_int, path: *const c_char, buf: *mut libc::stat64) -> c_int;
    fn __fxstatat(ver: c_int, fd: c_int, path: *const c_char, buf: *mut libc::stat, flag: c_int) -> c_int;
    fn __fxstatat64(ver: c_int, fd: c_int, path: *const c_char, buf: *mut libc::stat64, flag: c_int) -> c_int;

    fn access(path: *const c_char, mode: c_int) -> c_int;

    fn mkdir(path: *const c_char, mode: libc::mode_t) -> c_int;

    fn chdir(path: *const c_char) -> c_int;

    fn symlink(target: *const c_char, linkpath: *const c_char) -> c_int;
}
