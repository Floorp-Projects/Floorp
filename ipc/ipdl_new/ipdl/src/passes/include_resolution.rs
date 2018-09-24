/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use ast::{Location, TUId};
use errors::Errors;
use parser::{parse_file, FileURI, OwnedSourceBuffer, ParseTree};
use std::collections::{HashMap, HashSet};

pub struct IncludeResolver<'a, T: 'a>
where
    T: FileURI,
{
    include_dirs: &'a [T],
    include_files: HashMap<String, T>,
    id_file_map: TUIdFileMap<T>,
}

impl<'a, T> IncludeResolver<'a, T>
where
    T: FileURI,
{
    pub fn new(include_dirs: &'a [T]) -> IncludeResolver<'a, T> {
        IncludeResolver {
            include_dirs,
            include_files: HashMap::new(),
            id_file_map: TUIdFileMap::new(),
        }
    }

    pub fn get_include(&self, include_name: &str) -> Option<TUId> {
        match self.include_files.get(include_name) {
            Some(ref path) => self.id_file_map.get_tuid(path),
            None => None,
        }
    }

    pub fn resolve_include<'b>(&'b mut self, include_name: &str) -> Option<TUId> {
        if let Some(ref include_file_path) = self.include_files.get(include_name) {
            return Some(self.id_file_map.resolve_file_name(include_file_path));
        }

        // XXX The Python parser also checks '' for some reason.
        for include_dir in self.include_dirs {
            let mut new_include_path = include_dir.clone();
            new_include_path = match new_include_path.resolve_relative_path(include_name) {
                Ok(inc_path) => inc_path,
                Err(_) => continue,
            };

            let new_id = self.id_file_map.resolve_file_name(&new_include_path);
            self.include_files
                .insert(String::from(include_name), new_include_path);
            return Some(new_id);
        }

        None
    }

    fn print_include_context(include_context: &[T]) {
        for path in include_context {
            println!("  in file included from `{}':", path.to_utf8());
        }
    }

    #[allow(needless_pass_by_value)]
    pub fn resolve_includes<F: Fn(&str) -> Result<Box<OwnedSourceBuffer>, ()>>(
        &mut self,
        parse_tree: ParseTree<T>,
        mut source_string_loader: F,
    ) -> Result<(TUId, HashMap<TUId, ParseTree<T>>), Errors> {
        let mut work_list: Vec<(T, Vec<T>)> = Vec::new();
        let mut parsed_files = HashMap::new();
        let mut visited_files = HashSet::new();

        let resolved_path = parse_tree
            .file_path
            .resolve_relative_path("")
            .map_err(|()| {
                Errors::one(
                    &Location {
                        file_name: parse_tree.file_path.to_utf8().to_owned(),
                        lineno: 0,
                        colno: 0,
                    },
                    "Could not resolve file path",
                )
            })?;

        let file_id = self.id_file_map.resolve_file_name(&resolved_path);
        visited_files.insert(file_id);
        work_list.push((resolved_path.clone(), Vec::new()));

        while !work_list.is_empty() {
            let mut new_work_list = Vec::new();
            for (curr_file, include_context) in work_list {
                let curr_parse_tree = if curr_file == resolved_path {
                    parse_tree.clone()
                } else {
                    match parse_file(&curr_file, &mut source_string_loader) {
                        Ok(tu) => tu,
                        Err(err) => {
                            Self::print_include_context(&include_context);
                            return Err(Errors::from(err));
                        }
                    }
                };

                let mut include_errors = Errors::none();

                for include in &curr_parse_tree.translation_unit.data.includes {
                    let include_filename = format!(
                        "{}{}{}",
                        include.data.id.data,
                        ".ipdl",
                        if include.data.protocol.is_some() {
                            ""
                        } else {
                            "h"
                        }
                    );
                    let include_id = match self.resolve_include(&include_filename) {
                        Some(tuid) => tuid,
                        None => {
                            include_errors.append_one(
                                &Location {
                                    file_name: include_filename.clone(),
                                    lineno: 0,
                                    colno: 0,
                                },
                                &format!("Cannot resolve include {}", include_filename),
                            );
                            continue;
                        }
                    };

                    if visited_files.contains(&include_id) {
                        continue;
                    }

                    let mut new_include_context = include_context.clone();
                    new_include_context.push(curr_file.clone());

                    visited_files.insert(include_id);
                    new_work_list.push((
                        self.include_files
                            .get(&include_filename)
                            .expect("Resolve include is broken")
                            .clone(),
                        new_include_context,
                    ));
                }

                if !include_errors.is_empty() {
                    return Err(include_errors);
                }

                let curr_id = self.id_file_map.resolve_file_name(&curr_file);
                parsed_files.insert(curr_id, curr_parse_tree);
            }

            work_list = new_work_list;
        }

        Ok((file_id, parsed_files))
    }
}

pub struct TUIdFileMap<T>
where
    T: FileURI,
{
    next_id: TUId,
    file_ids: HashMap<T, TUId>,
    id_files: HashMap<TUId, T>,
}

impl<T> TUIdFileMap<T>
where
    T: FileURI,
{
    fn new() -> TUIdFileMap<T> {
        TUIdFileMap {
            next_id: 0,
            file_ids: HashMap::new(),
            id_files: HashMap::new(),
        }
    }

    fn get_tuid(&self, path: &T) -> Option<TUId> {
        self.file_ids.get(path).cloned()
    }

    fn resolve_file_name(&mut self, path: &T) -> TUId {
        if let Some(&id) = self.file_ids.get(path) {
            return id;
        }

        let id = self.next_id;
        self.next_id += 1;
        self.id_files.insert(id, path.clone());
        self.file_ids.insert(path.clone(), id);
        id
    }
}
