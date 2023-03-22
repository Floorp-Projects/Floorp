use std::path::{Path, PathBuf};

/// Construct a [`PathBuf`] from individual [`Path`] components.
///
/// This is a simple and legible way to construct `PathBuf`s that use the system's native path
/// separator character. (It's ugly to see paths mixing `\` and `/`.)
///
/// # Examples
///
/// ```rust
/// # use std::path::Path;
/// # use vendor_webgpu_cts::path::join_path;
/// assert_eq!(&*join_path(["foo", "bar", "baz"]), Path::new("foo/bar/baz"));
/// ```
pub(crate) fn join_path<I, P>(iter: I) -> PathBuf
where
    I: IntoIterator<Item = P>,
    P: AsRef<Path>,
{
    let mut path = PathBuf::new();
    path.extend(iter);
    path
}
