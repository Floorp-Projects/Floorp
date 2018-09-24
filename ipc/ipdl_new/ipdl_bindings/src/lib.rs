/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate ipdl;
extern crate nsstring;
extern crate thin_vec;
#[macro_use]
extern crate mfbt_maybe;

use ipdl::ast;
use ipdl::parser;
use ipdl::parser::FileURI;
use mfbt_maybe::{Maybe, MaybeTrait};
use nsstring::{nsACString, nsCStr, nsCString};
use std::collections::HashMap;
use std::iter::FromIterator;
use std::str;
use thin_vec::ThinVec;
use std::hash::Hash;
use std::hash::Hasher;

maybe!(CxxTypeKind);
maybe!(NamedProtocol);

fn vec_to_thinvec<T, F: Into<T>>(vec: Vec<F>) -> ThinVec<T> {
    ThinVec::from_iter(vec.into_iter().map(|x| x.into()))
}

#[repr(C)]
pub struct QualifiedId {
    pub base_id: Identifier,
    pub quals: ThinVec<nsCString>,
}

impl From<ast::QualifiedId> for QualifiedId {
    fn from(qi: ast::QualifiedId) -> Self {
        QualifiedId {
            base_id: qi.base_id.into(),
            quals: ThinVec::from_iter(qi.quals.into_iter().map(|x| x.into())),
        }
    }
}

#[repr(C)]
pub struct TypeSpec {
    pub spec: QualifiedId,
    pub array: bool,
    pub nullable: bool,
}

impl From<ast::TypeSpec> for TypeSpec {
    fn from(ts: ast::TypeSpec) -> Self {
        TypeSpec {
            spec: ts.spec.into(),
            array: ts.array,
            nullable: ts.nullable,
        }
    }
}

#[repr(C)]
pub struct Param {
    pub name: Identifier,
    pub type_spec: TypeSpec,
}

impl From<ast::Param> for Param {
    fn from(p: ast::Param) -> Self {
        Param {
            name: p.name.into(),
            type_spec: p.type_spec.into(),
        }
    }
}

#[repr(C)]
pub struct StructField {
    pub type_spec: TypeSpec,
    pub name: Identifier,
}

impl From<ast::StructField> for StructField {
    fn from(sf: ast::StructField) -> Self {
        StructField {
            type_spec: sf.type_spec.into(),
            name: sf.name.into(),
        }
    }
}

#[repr(C)]
pub struct Namespace {
    pub name: Identifier,
    pub namespaces: ThinVec<nsCString>,
}

impl From<ast::Namespace> for Namespace {
    fn from(ns: ast::Namespace) -> Self {
        Namespace {
            name: ns.name.into(),
            namespaces: vec_to_thinvec(ns.namespaces),
        }
    }
}

#[repr(C)]
pub enum Compress {
    None,
    Enabled,
    All,
}

impl From<ast::Compress> for Compress {
    fn from(c: ast::Compress) -> Self {
        match c {
            ast::Compress::None => Compress::None,
            ast::Compress::Enabled => Compress::Enabled,
            ast::Compress::All => Compress::All,
        }
    }
}

#[repr(C)]
pub enum SendSemantics {
    Async,
    Sync,
    Intr,
}

impl From<ast::SendSemantics> for SendSemantics {
    fn from(ss: ast::SendSemantics) -> Self {
        match ss {
            ast::SendSemantics::Async => SendSemantics::Async,
            ast::SendSemantics::Sync => SendSemantics::Sync,
            ast::SendSemantics::Intr => SendSemantics::Intr,
        }
    }
}

#[repr(C)]
pub enum Nesting {
    None,
    InsideSync,
    InsideCpow,
}

impl From<ast::Nesting> for Nesting {
    fn from(n: ast::Nesting) -> Self {
        match n {
            ast::Nesting::InsideCpow => Nesting::InsideCpow,
            ast::Nesting::InsideSync => Nesting::InsideSync,
            ast::Nesting::None => Nesting::None,
        }
    }
}

#[repr(C)]
pub enum Priority {
    Normal,
    High,
    Input,
}

impl From<ast::Priority> for Priority {
    fn from(p: ast::Priority) -> Self {
        match p {
            ast::Priority::High => Priority::High,
            ast::Priority::Input => Priority::Input,
            ast::Priority::Normal => Priority::Normal,
        }
    }
}

#[repr(C)]
pub enum Direction {
    ToParent,
    ToChild,
    ToParentOrChild,
}

impl From<ast::Direction> for Direction {
    fn from(d: ast::Direction) -> Self {
        match d {
            ast::Direction::ToChild => Direction::ToChild,
            ast::Direction::ToParent => Direction::ToParent,
            ast::Direction::ToParentOrChild => Direction::ToParentOrChild,
        }
    }
}

#[repr(C)]
pub struct Location {
    pub file_name: nsCString,
    pub lineno: usize,
    pub colno: usize,
}

impl From<ast::Location> for Location {
    fn from(l: ast::Location) -> Self {
        Location {
            file_name: l.file_name.into(),
            lineno: l.lineno,
            colno: l.colno,
        }
    }
}

#[repr(C)]
pub struct Identifier {
    pub id: nsCString,
    pub loc: Location,
}

impl From<ast::Identifier> for Identifier {
    fn from(id: ast::Identifier) -> Self {
        Identifier {
            id: id.id.into(),
            loc: id.loc.into(),
        }
    }
}

#[repr(C)]
pub struct MessageDecl {
    pub name: Identifier,
    pub send_semantics: SendSemantics,
    pub nested: Nesting,
    pub prio: Priority,
    pub direction: Direction,
    pub in_params: ThinVec<Param>,
    pub out_params: ThinVec<Param>,
    pub compress: Compress,
    pub verify: bool,
}

impl From<ast::MessageDecl> for MessageDecl {
    fn from(md: ast::MessageDecl) -> Self {
        MessageDecl {
            name: md.name.into(),
            send_semantics: md.send_semantics.into(),
            nested: md.nested.into(),
            prio: md.prio.into(),
            direction: md.direction.into(),
            in_params: vec_to_thinvec(md.in_params),
            out_params: vec_to_thinvec(md.out_params),
            compress: md.compress.into(),
            verify: md.verify,
        }
    }
}

#[repr(C)]
pub struct Protocol {
    pub send_semantics: SendSemantics,
    pub nested: Nesting,
    pub managers: ThinVec<Identifier>,
    pub manages: ThinVec<Identifier>,
    pub messages: ThinVec<MessageDecl>,
}

impl From<ast::Protocol> for Protocol {
    fn from(p: ast::Protocol) -> Self {
        Protocol {
            send_semantics: p.send_semantics.into(),
            nested: p.nested.into(),
            managers: vec_to_thinvec(p.managers),
            manages: vec_to_thinvec(p.manages),
            messages: vec_to_thinvec(p.messages),
        }
    }
}

#[repr(C)]
pub enum CxxTypeKind {
    Struct,
    Class,
}

impl From<ast::CxxTypeKind> for CxxTypeKind {
    fn from(cxxtk: ast::CxxTypeKind) -> Self {
        match cxxtk {
            ast::CxxTypeKind::Class => CxxTypeKind::Class,
            ast::CxxTypeKind::Struct => CxxTypeKind::Struct,
        }
    }
}

#[repr(C)]
pub struct UsingStmt {
    pub cxx_type: TypeSpec,
    pub header: nsCString,
    pub kind: Maybe<CxxTypeKind>,
    pub refcounted: bool,
}

impl From<ast::UsingStmt> for UsingStmt {
    fn from(using: ast::UsingStmt) -> Self {
        UsingStmt {
            cxx_type: using.cxx_type.into(),
            header: using.header.into(),
            kind: using.kind.into(),
            refcounted: using.refcounted,
        }
    }
}

#[repr(C)]
pub enum FileType {
    Protocol,
    Header,
}

impl From<ast::FileType> for FileType {
    fn from(ft: ast::FileType) -> Self {
        match ft {
            ast::FileType::Protocol => FileType::Protocol,
            ast::FileType::Header => FileType::Header,
        }
    }
}

// Translation unit identifier.
pub type TUId = i32;

#[repr(C)]
pub struct Struct {
    pub ns: Namespace,
    pub fields: ThinVec<StructField>,
}

impl From<(ast::Namespace, Vec<ast::StructField>)> for Struct {
    fn from(s: (ast::Namespace, Vec<ast::StructField>)) -> Self {
        Struct {
            ns: s.0.into(),
            fields: vec_to_thinvec(s.1),
        }
    }
}

#[repr(C)]
pub struct Union {
    pub ns: Namespace,
    pub types: ThinVec<TypeSpec>,
}

impl From<(ast::Namespace, Vec<ast::TypeSpec>)> for Union {
    fn from(u: (ast::Namespace, Vec<ast::TypeSpec>)) -> Self {
        Union {
            ns: u.0.into(),
            types: vec_to_thinvec(u.1),
        }
    }
}

#[repr(C)]
pub struct NamedProtocol {
    pub ns: Namespace,
    pub protocol: Protocol,
}

impl From<(ast::Namespace, ast::Protocol)> for NamedProtocol {
    fn from(p: (ast::Namespace, ast::Protocol)) -> Self {
        NamedProtocol {
            ns: p.0.into(),
            protocol: p.1.into(),
        }
    }
}

#[repr(C)]
pub struct TranslationUnit {
    pub ns: Namespace,
    pub file_type: FileType,
    pub file_name: nsCString,
    pub cxx_includes: ThinVec<nsCString>,
    pub includes: ThinVec<TUId>,
    pub using_stmt: ThinVec<UsingStmt>,
    pub structs: ThinVec<Struct>,
    pub unions: ThinVec<Union>,
    pub protocol: Maybe<NamedProtocol>,
}

impl From<ast::TranslationUnit> for TranslationUnit {
    fn from(tu: ast::TranslationUnit) -> Self {
        TranslationUnit {
            ns: tu.namespace.into(),
            file_type: tu.file_type.into(),
            file_name: tu.file_name.into(),
            cxx_includes: vec_to_thinvec(tu.cxx_includes),
            includes: vec_to_thinvec(tu.includes),
            using_stmt: vec_to_thinvec(tu.using),
            structs: vec_to_thinvec(tu.structs),
            unions: vec_to_thinvec(tu.unions),
            protocol: tu.protocol.into(),
        }
    }
}

pub struct AST {
    pub main_tuid: TUId,
    pub translation_units: HashMap<TUId, TranslationUnit>,
}

impl From<ast::AST> for AST {
    fn from(ast: ast::AST) -> Self {
        AST {
            main_tuid: ast.main_tuid,
            translation_units: ast
                .translation_units
                .into_iter()
                .map(|(k, v)| (k, v.into()))
                .collect(),
        }
    }
}

#[no_mangle]
pub extern "C" fn ipdl_ast_get_tu(ast: &AST, tuid: TUId) -> *const TranslationUnit {
    ast.translation_units
        .get(&tuid)
        .map(|x| x as *const TranslationUnit)
        .unwrap_or(::std::ptr::null())
}

#[no_mangle]
pub extern "C" fn ipdl_ast_main_tuid(ast: &AST) -> TUId {
    ast.main_tuid
}

struct FFIURI {
    data: nsCString,
    resolve_relative_path_fn: fn(&nsACString, &nsACString, &mut nsACString) -> u8,
    equals_fn: fn(&nsACString, &nsACString) -> u8,
}

impl parser::FileURI for FFIURI {
    fn resolve_relative_path(&self, relative_path: &str) -> Result<Self, ()> {
        let mut result = nsCString::new();
        let ok = (self.resolve_relative_path_fn)(&self.data, &nsCStr::from(relative_path), &mut result);
        if ok != 0 {
            Ok(FFIURI {
                data: result,
                resolve_relative_path_fn: self.resolve_relative_path_fn,
                equals_fn: self.equals_fn,
            })
        } else {
            Err(())
        }
    }

    fn to_utf8(&self) -> &str {
        str::from_utf8(&self.data).unwrap()
    }
}

impl PartialEq for FFIURI {
    fn eq(&self, other: &FFIURI) -> bool {
        (self.equals_fn)(&self.data, &other.data) != 0
    }
}

impl Eq for FFIURI {}

impl Hash for FFIURI {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.data.hash(state);
    }
}

impl Clone for FFIURI {
    fn clone(&self) -> FFIURI {
        let new_data = nsCString::from(self.data.as_ref());
        FFIURI {
            data: new_data,
            resolve_relative_path_fn: self.resolve_relative_path_fn,
            equals_fn: self.equals_fn,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ipdl_parse_file(
    ipdl_file: &nsACString,
    error_string: &mut nsACString,
    source_string_loader: fn(&nsACString, &mut nsACString) -> u8,
    resolve_relative_path: fn(&nsACString, &nsACString, &mut nsACString) -> u8,
    equals: fn(&nsACString, &nsACString) -> u8,
) -> *const AST {
    let uri = FFIURI {
        data: nsCString::from(ipdl_file),
        resolve_relative_path_fn: resolve_relative_path,
        equals_fn: equals,
    };

    let parent_uri = match uri.resolve_relative_path("./") {
        Ok(pu) => pu,
        Err(()) => {
            error_string.assign("Could not get IPDL file parent directory");
            return ::std::ptr::null()
        },
    };

    Box::into_raw(Box::new(match parser::parse(
        &uri,
        &[parent_uri],
        |uri: &str| -> Result<Box<parser::OwnedSourceBuffer>, ()> {
            let ns_uri = nsCStr::from(uri);
            let mut result = nsCString::new();
            let ok = source_string_loader(&ns_uri, &mut result);
            if ok != 0 {
                Ok(Box::new(result))
            } else {
                Err(())
            }
        },
    ) {
        Ok(ast) => ast.into(),
        Err(error) => {
            error_string.assign(&format!("{}", error));
            return ::std::ptr::null()
        },
    }))
}

#[no_mangle]
pub unsafe extern "C" fn ipdl_free_ast(ast: *const AST) {
    drop(Box::from_raw(ast as *mut AST))
}
