/* Copyright 2018 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/// Helpers common to other source files here.
use std::error;
use std::fmt;

use cranelift_wasm::WasmError;

type DashError = Box<error::Error>;
pub type DashResult<T> = Result<T, DashError>;

/// A simple error type that contains a string message, used to wrap raw Cranelift error types
/// which don't implement std::error::Error.

#[derive(Debug)]
pub struct BasicError {
    msg: String,
}

impl BasicError {
    pub fn new(msg: String) -> Self {
        Self { msg }
    }
}

impl fmt::Display for BasicError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "BaldrDash error: {}", self.msg)
    }
}

impl error::Error for BasicError {
    fn description(&self) -> &str {
        &self.msg
    }
}

impl Into<WasmError> for BasicError {
    fn into(self) -> WasmError {
        WasmError::User(self.msg)
    }
}
