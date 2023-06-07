// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct Width(pub u16);
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct Height(pub u16);

pub fn terminal_size() -> Option<(Width, Height)> {
    Some((Width(80), Height(25)))
}

#[cfg(unix)]
pub fn terminal_size_using_fd(_fd: std::os::unix::io::RawFd) -> Option<(Width, Height)> {
    Some((Width(80), Height(25)))
}

#[cfg(windows)]
pub fn terminal_size_using_handle(
    _handle: std::os::windows::io::RawHandle,
) -> Option<(Width, Height)> {
    Some((Width(80), Height(25)))
}
