/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use pipdl;

use passes::include_resolution::IncludeResolver;
use passes::parsetree_to_tu::ParseTreeToTU;
use passes::type_check;

use errors;

use std::collections::HashMap;
use std::hash::Hash;
use std::str;

use std::path::PathBuf;

use ast::{Location, AST};

pub trait OwnedSourceBuffer: Drop + AsRef<[u8]> {
    fn to_utf8(&self) -> &str;
}
impl<T> OwnedSourceBuffer for T
where
    T: Drop + AsRef<[u8]>,
{
    fn to_utf8(&self) -> &str {
        str::from_utf8(self.as_ref()).unwrap()
    }
}

pub trait FileURI: Clone + Eq + Hash {
    fn resolve_relative_path(&self, relative_path: &str) -> Result<Self, ()>;
    fn to_utf8(&self) -> &str;
}

impl FileURI for PathBuf {
    fn resolve_relative_path(&self, relative_path: &str) -> Result<Self, ()> {
        let mut new_path = self.clone();
        new_path.push(relative_path);
        new_path.canonicalize().map_err(|_| ())
    }

    fn to_utf8(&self) -> &str {
        self.to_str().unwrap()
    }
}

#[derive(Clone)]
pub struct ParseTree<T>
where
    T: FileURI,
{
    pub translation_unit: pipdl::Spanned<pipdl::TranslationUnit>,
    pub file_path: T,
}

pub fn parse_file<F: Fn(&str) -> Result<Box<OwnedSourceBuffer>, ()>, T: FileURI>(
    file_path: &T,
    source_string_loader: &mut F,
) -> Result<ParseTree<T>, errors::Errors> {
    let file_path_str = file_path.to_utf8();
    let file_text = source_string_loader(file_path_str).map_err(|()| {
        errors::Errors::one(
            &Location {
                file_name: file_path_str.to_owned(),
                lineno: 0,
                colno: 0,
            },
            "Error loading source string",
        )
    })?;

    let parse_tree = ParseTree {
        translation_unit: pipdl::parse(file_text.to_utf8(), file_path_str)?,
        file_path: file_path.clone(),
    };

    Ok(parse_tree)
}

pub fn parse<F: Fn(&str) -> Result<Box<OwnedSourceBuffer>, ()>, T: FileURI>(
    file_path: &T,
    include_dirs: &[T],
    mut source_string_loader: F,
) -> Result<AST, errors::Errors> {
    let parse_tree = parse_file(file_path, &mut source_string_loader)?;

    let mut include_resolver = IncludeResolver::new(include_dirs);

    let (main_tuid, result) = include_resolver.resolve_includes(parse_tree, source_string_loader)?;

    let parsetree_to_translation_unit = ParseTreeToTU::new(&include_resolver);

    let ast = AST {
        main_tuid,
        translation_units: {
            result
                .into_iter()
                .map(|(tuid, parse_tree)| {
                    Ok((
                        tuid,
                        parsetree_to_translation_unit.parsetree_to_translation_unit(parse_tree)?,
                    ))
                })
                .collect::<Result<HashMap<_, _>, errors::Errors>>()?
        },
    };

    type_check::check(&ast.translation_units)?;

    Ok(ast)
}
