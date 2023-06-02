/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub struct OsLogger;

use log::{LevelFilter, SetLoggerError};

impl OsLogger {
    pub fn new(_subsystem: &str) -> Self {
        Self
    }

    pub fn level_filter(self, _level: LevelFilter) -> Self {
        self
    }

    pub fn category_level_filter(self, _category: &str, _level: LevelFilter) -> Self {
        self
    }

    pub fn init(self) -> Result<(), SetLoggerError> {
        Ok(())
    }
}
