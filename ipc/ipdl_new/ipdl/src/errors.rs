/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use ast::Location;
use pipdl;
use std::error::Error;
use std::fmt;

fn error_msg(loc: &Location, err: &str) -> String {
    format!("{}: error: {}", loc, err)
}

#[must_use]
pub struct Errors {
    errors: Vec<String>,
}

impl From<pipdl::Error> for Errors {
    fn from(error: pipdl::Error) -> Self {
        Errors::one(
            &Location {
                file_name: error.span().start.file,
                lineno: error.span().start.line,
                colno: error.span().start.col,
            },
            error.description(),
        )
    }
}

impl fmt::Display for Errors {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(&self.errors.join("\n"))
    }
}

impl Errors {
    pub fn none() -> Errors {
        Errors { errors: Vec::new() }
    }

    pub fn one(loc: &Location, err: &str) -> Errors {
        Errors {
            errors: vec![error_msg(&loc, &err)],
        }
    }

    pub fn append(&mut self, mut other: Errors) {
        self.errors.append(&mut other.errors);
    }

    pub fn append_one(&mut self, loc: &Location, other: &str) {
        self.errors.push(error_msg(&loc, &other));
    }

    pub fn to_result(&self) -> Result<(), String> {
        if self.errors.is_empty() {
            Ok(())
        } else {
            Err(self.errors.join("\n"))
        }
    }

    pub fn is_empty(&self) -> bool {
        self.errors.is_empty()
    }
}
