/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Functionality for managing source code for shaders.
//!
//! This module is used during precompilation (build.rs) and regular compilation,
//! so it has minimal dependencies.

pub use sha2::{Digest, Sha256};
use std::borrow::Cow;
use std::fs::File;
use std::io::Read;
use std::path::Path;

#[derive(PartialEq, Eq, Hash, Debug, Clone, Default)]
#[cfg_attr(feature = "serialize_program", derive(Deserialize, Serialize))]
pub struct ProgramSourceDigest([u8; 32]);

impl ::std::fmt::Display for ProgramSourceDigest {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        for byte in self.0.iter() {
            f.write_fmt(format_args!("{:02x}", byte))?;
        }
        Ok(())
    }
}

impl From<Sha256> for ProgramSourceDigest {
    fn from(hasher: Sha256) -> Self {
        let mut digest = Self::default();
        digest.0.copy_from_slice(hasher.result().as_slice());
        digest
    }
}

const SHADER_IMPORT: &str = "#include ";

/// Parses a shader string for imports. Imports are recursively processed, and
/// prepended to the output stream.
pub fn parse_shader_source<F: FnMut(&str), G: Fn(&str) -> Cow<'static, str>>(
    source: Cow<'static, str>,
    get_source: &G,
    output: &mut F,
) {
    for line in source.lines() {
        if line.starts_with(SHADER_IMPORT) {
            let imports = line[SHADER_IMPORT.len() ..].split(',');

            // For each import, get the source, and recurse.
            for import in imports {
                let include = get_source(import);
                parse_shader_source(include, get_source, output);
            }
        } else {
            output(line);
            output("\n");
        }
    }
}

/// Reads a shader source file from disk into a String.
pub fn shader_source_from_file(shader_path: &Path) -> String {
    assert!(shader_path.exists(), "Shader not found");
    let mut source = String::new();
    File::open(&shader_path)
        .expect("Shader not found")
        .read_to_string(&mut source)
        .unwrap();
    source
}
