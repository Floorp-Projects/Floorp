/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Functionality for managing source code for shaders.
//!
//! This module is used during precompilation (build.rs) and regular compilation,
//! so it has minimal dependencies.

use std::borrow::Cow;
use std::fs::File;
use std::io::Read;
use std::path::Path;
use std::collections::HashSet;
use std::collections::hash_map::DefaultHasher;

pub use crate::shader_features::*;

#[derive(PartialEq, Eq, Hash, Debug, Clone, Default)]
#[cfg_attr(feature = "serialize_program", derive(Deserialize, Serialize))]
pub struct ProgramSourceDigest(u64);

impl ::std::fmt::Display for ProgramSourceDigest {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{:02x}", self.0)
    }
}

impl From<DefaultHasher> for ProgramSourceDigest {
    fn from(hasher: DefaultHasher) -> Self {
        use std::hash::Hasher;
        ProgramSourceDigest(hasher.finish())
    }
}

const SHADER_IMPORT: &str = "#include ";

pub struct ShaderSourceParser {
    included: HashSet<String>,
}

impl ShaderSourceParser {
    pub fn new() -> Self {
        ShaderSourceParser {
            included: HashSet::new(),
        }
    }

    /// Parses a shader string for imports. Imports are recursively processed, and
    /// prepended to the output stream.
    pub fn parse<F: FnMut(&str), G: Fn(&str) -> Cow<'static, str>>(
        &mut self,
        source: Cow<'static, str>,
        get_source: &G,
        output: &mut F,
    ) {
        for line in source.lines() {
            if line.starts_with(SHADER_IMPORT) {
                let imports = line[SHADER_IMPORT.len() ..].split(',');

                // For each import, get the source, and recurse.
                for import in imports {
                    if self.included.insert(import.into()) {
                        let include = get_source(import);
                        self.parse(include, get_source, output);
                    } else {
                        output(&format!("// {} is already included\n", import));
                    }
                }
            } else {
                output(line);
                output("\n");
            }
        }
    }
}

/// Reads a shader source file from disk into a String.
pub fn shader_source_from_file(shader_path: &Path) -> String {
    assert!(shader_path.exists(), "Shader not found {:?}", shader_path);
    let mut source = String::new();
    File::open(&shader_path)
        .expect("Shader not found")
        .read_to_string(&mut source)
        .unwrap();
    source
}
