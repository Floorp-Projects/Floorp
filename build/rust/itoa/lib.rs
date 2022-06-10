// Licensed under the Apache License, Version 2.0
// <http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <http://opensource.org/licenses/MIT>, at your option.

pub use itoa::*;
use std::{fmt, io};

// APIs that were in itoa 0.4 but aren't in 1.0.

#[inline]
pub fn write<W: io::Write, V: Integer>(mut wr: W, value: V) -> io::Result<usize> {
    let mut buf = Buffer::new();
    let s = buf.format(value);
    match wr.write_all(s.as_bytes()) {
        Ok(()) => Ok(s.len()),
        Err(e) => Err(e),
    }
}

/// Write integer to an `fmt::Write`.
#[inline]
pub fn fmt<W: fmt::Write, V: Integer>(mut wr: W, value: V) -> fmt::Result {
    let mut buf = Buffer::new();
    wr.write_str(buf.format(value))
}
