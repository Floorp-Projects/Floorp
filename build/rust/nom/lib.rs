/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use nom::*;

pub use nom::separated_list0 as separated_list;
pub use nom::separated_list1 as separated_nonempty_list;

pub type IResult<I, O, E=(I, error::ErrorKind)> = nom::IResult<I, O, E>;
