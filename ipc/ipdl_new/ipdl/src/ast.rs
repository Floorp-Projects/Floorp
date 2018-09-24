/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fmt;
use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct QualifiedId {
    pub base_id: Identifier,
    pub quals: Vec<String>,
}

impl QualifiedId {
    pub fn new(base: Identifier) -> QualifiedId {
        QualifiedId { base_id: base, quals: Vec::new() }
    }

    pub fn qualify(mut self, id: Identifier) -> QualifiedId {
        self.quals.push(self.base_id.id);
        self.base_id = id;
        self
    }

    pub fn new_from_iter<'a, I> (mut ids: I) -> QualifiedId
        where I: Iterator<Item=&'a str>
    {
        let loc = Location { file_name: String::from("<builtin>"), lineno: 0, colno: 0 };
        let mut qual_id = QualifiedId::new(Identifier::new(String::from(ids.next().expect("Empty iterator when creating QID")), loc.clone()));
        for i in ids {
            qual_id = qual_id.qualify(Identifier::new(String::from(i), loc.clone()));
        }
        qual_id
    }

    pub fn short_name(&self) -> String {
        self.base_id.to_string()
    }

    pub fn full_name(&self) -> Option<String> {
        if self.quals.is_empty() {
            None
        } else {
            Some(self.to_string())
        }
    }

    pub fn loc(&self) -> &Location {
        &self.base_id.loc
    }
}

impl fmt::Display for QualifiedId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for q in &self.quals {
            try!(write!(f, "{}::", q));
        }
        write!(f, "{}", self.base_id)
    }
}

#[derive(Debug)]
pub struct TypeSpec {
    pub spec: QualifiedId,
    pub array: bool,
    pub nullable: bool,
}

impl TypeSpec {
    pub fn new(spec: QualifiedId) -> TypeSpec {
        TypeSpec { spec: spec, array: false, nullable: false }
    }

    pub fn loc(&self) -> &Location {
        self.spec.loc()
    }
}

#[derive(Debug)]
pub struct Param {
    pub name: Identifier,
    pub type_spec: TypeSpec,
}

#[derive(Debug)]
pub struct StructField {
    pub type_spec: TypeSpec,
    pub name: Identifier,
}

#[derive(Clone, Debug)]
pub struct Namespace {
    pub name: Identifier,
    pub namespaces: Vec<String>,
}

impl Namespace {
    pub fn qname(&self) -> QualifiedId {
        QualifiedId { base_id: self.name.clone(), quals: self.namespaces.clone() }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum Compress {
    None,
    Enabled,
    All,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum SendSemantics {
    Async,
    Sync,
    Intr,
}

impl SendSemantics {
    pub fn is_async(&self) -> bool {
        self == &SendSemantics::Async
    }

    pub fn is_sync(&self) -> bool {
        self == &SendSemantics::Sync
    }

    pub fn is_intr(&self) -> bool {
        self == &SendSemantics::Intr
    }
}

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
pub enum Nesting {
    None,
    InsideSync,
    InsideCpow,
}

impl Nesting {
    pub fn is_none(&self) -> bool {
        self == &Nesting::None
    }

    pub fn inside_sync(&self) -> bool {
        self == &Nesting::InsideSync
    }

    pub fn inside_cpow(&self) -> bool {
        self == &Nesting::InsideCpow
    }
}

#[derive(Debug, Clone, Copy)]
pub enum Priority {
    Normal,
    High,
    Input,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Direction {
    ToParent,
    ToChild,
    ToParentOrChild,
}

impl Direction {
    pub fn is_to_child(&self) -> bool {
        self == &Direction::ToChild
    }

    pub fn is_both(&self) -> bool {
        self == &Direction::ToParentOrChild
    }
}

#[derive(Debug, Clone)]
pub struct Location {
    pub file_name: String,
    pub lineno: usize,
    pub colno: usize,
}

impl fmt::Display for Location {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}:{}:{}", self.file_name, self.lineno, self.colno)
    }
}

#[derive(Debug, Clone)]
pub struct Identifier {
    pub id: String,
    pub loc: Location,
}

impl Identifier {
    pub fn new(name: String, loc: Location) -> Identifier {
        Identifier {
            id: name,
            loc: loc,
        }
    }
}

impl fmt::Display for Identifier {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.id)
    }
}

#[derive(Debug)]
pub struct MessageDecl {
    pub name: Identifier,
    pub send_semantics: SendSemantics,
    pub nested: Nesting,
    pub prio: Priority,
    pub direction: Direction,
    pub in_params: Vec<Param>,
    pub out_params: Vec<Param>,
    pub compress: Compress,
    pub verify: bool,
}

#[derive(Debug)]
pub struct Protocol {
    pub send_semantics: SendSemantics,
    pub nested: Nesting,
    pub managers: Vec<Identifier>,
    pub manages: Vec<Identifier>,
    pub messages: Vec<MessageDecl>,
}

#[derive(Debug)]
pub enum CxxTypeKind {
  Struct,
  Class,
}

#[derive(Debug)]
pub struct UsingStmt {
    pub cxx_type: TypeSpec,
    pub header: String,
    pub kind: Option<CxxTypeKind>,
    pub refcounted: bool,
}

#[derive(Clone, Debug, PartialEq)]
pub enum FileType {
    Protocol,
    Header,
}

impl FileType {
    pub fn from_file_path(file_path: &str) -> Option<FileType> {
        if let Some(e) = file_path.rsplit('.').next() {
            if e == "ipdlh" {
                Some(FileType::Header)
            } else {
                Some(FileType::Protocol)
            }
        } else {
            None
        }
    }
}

// Translation unit identifier.
pub type TUId = i32;

#[derive(Debug)]
pub struct TranslationUnit {
    pub namespace: Namespace,
    pub file_type: FileType,
    pub file_name: String,
    pub cxx_includes: Vec<String>,
    pub includes: Vec<TUId>,
    pub using: Vec<UsingStmt>,
    pub structs: Vec<(Namespace, Vec<StructField>)>,
    pub unions: Vec<(Namespace, Vec<TypeSpec>)>,
    pub protocol: Option<(Namespace, Protocol)>,
}

pub struct AST {
    pub main_tuid: TUId,
    pub translation_units: HashMap<TUId, TranslationUnit>,
}
