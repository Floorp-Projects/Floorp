/* Copyright 2021 Mozilla Foundation
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

use anyhow::{Context as _, Result};
use std::env;
use std::path::Path;

mod convert;

fn main() -> Result<()> {
    let files = env::args().collect::<Vec<String>>();
    for path in &files[1..] {
        let source =
            std::fs::read_to_string(path).with_context(|| format!("failed to read `{}`", path))?;

        let mut full_script = String::new();
        full_script.push_str(&convert::harness());
        full_script.push_str(&convert::convert(path, &source)?);

        std::fs::write(
            Path::new(path).with_extension("js").file_name().unwrap(),
            &full_script,
        )?;
    }

    Ok(())
}
