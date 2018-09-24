/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This is a basic recursive-descent parser for ipdl. It intentionally doesn't
//! have some featres which most languages would have, such as unicode support
//! in identifiers, as it is unnecessary for our usecase.

// Our code is Clippy aware, so we disable warnings for unknown lints
// which are triggered when we silence a clippy lint
#![allow(unknown_lints)]

use std::error;
use std::fmt;

#[macro_use]
pub mod util;
pub use util::*;

/// Public error type for messages with resolved type information.
pub struct Error(Box<ParserError>);

impl Error {
    pub fn span(&self) -> Span {
        self.0.span()
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        &self.0.message()
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(&format!(
            "{}:{}:{}: error: {}",
            self.0.span().start.file,
            self.0.span().start.line,
            self.0.span().start.col,
            self.0.message()
        ))
    }
}

impl fmt::Debug for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[derive(Debug, Clone)]
pub struct CxxInclude {
    pub file: Spanned<String>,
}

fn cxx_include(i: In) -> PResult<Spanned<CxxInclude>> {
    let start = i.loc();
    let (i, _) = kw(i, "include")?;
    let (i, file) = string(i)?;
    commit! {
        let (i, _) = punct(i, ";")?;
        let end = i.loc();
        Ok((i.clone(), Spanned::new(Span { start, end }, CxxInclude { file })))
    }
}

#[derive(Debug, Clone)]
pub struct Include {
    pub protocol: Option<Spanned<()>>,
    pub id: Spanned<String>,
}

fn include(i: In) -> PResult<Spanned<Include>> {
    let start = i.loc();
    let (i, _) = kw(i, "include")?;
    let (i, pcol) = maybe(i.clone(), kw(i, "protocol"))?;

    let pcol_is_some = pcol.is_some();

    let common_end_code = |i, id| {
        let (i, _) = punct(i, ";")?;
        let end = i.loc();
        Ok((
            i,
            Spanned::new(Span { start, end }, Include { protocol: pcol, id }),
        ))
    };

    if pcol_is_some {
        // If we have the protocol keyword, then it can't be a C++ include anymore, and we start committing
        commit! {
            let (i, id) = ident(i)?;
            common_end_code(i, id)
        }
    } else {
        // Otherwise we first parse the ident
        let (i, id) = ident(i)?;
        commit! {
            common_end_code(i, id)
        }
    }
}

#[derive(Debug, Clone)]
pub enum CxxTypeKind {
    Class,
    Struct,
    None,
}

#[derive(Debug, Clone)]
pub struct CxxPathSeg {
    pub id: Spanned<String>,
    pub args: Option<Spanned<Vec<Spanned<String>>>>,
}

fn template_args(i: In) -> PResult<Spanned<Vec<Spanned<String>>>> {
    let start = i.loc();
    let (i, _) = punct(i, "<")?;
    commit! {
        let (i, args) = sep(i, ident, ",")?;
        let (i, _) = punct(i, ">")?;

        let end = i.loc();
        Ok((i, Spanned::new(Span { start, end }, args)))
    }
}

fn cxx_path_seg(i: In) -> PResult<Spanned<CxxPathSeg>> {
    let start = i.loc();
    let (i, id) = ident(i)?;
    let (i, args) = maybe(i.clone(), template_args(i))?;
    let end = i.loc();
    Ok((
        i,
        Spanned::new(Span { start, end }, CxxPathSeg { id, args }),
    ))
}

#[derive(Debug, Clone)]
pub struct CxxPath {
    pub segs: Vec<Spanned<CxxPathSeg>>,
}

fn cxx_path(i: In) -> PResult<Spanned<CxxPath>> {
    let start = i.loc();
    let (i, segs) = sep(i, cxx_path_seg, "::")?;
    let end = i.loc();
    Ok((i, Spanned::new(Span { start, end }, CxxPath { segs })))
}

#[derive(Debug, Clone)]
pub struct Using {
    pub refcounted: Option<Spanned<()>>,
    pub kind: Spanned<CxxTypeKind>,
    pub ty: Spanned<CxxPath>,
    pub file: Spanned<String>,
}

fn using(i: In) -> PResult<Spanned<Using>> {
    let start = i.loc();
    let (i, _) = kw(i, "using")?;
    commit! {
        let (i, refcounted) = maybe(i.clone(), kw(i, "refcounted"))?;
        let kw_start = i.loc();
        let (i, kind) = any!(
            i, "struct or class keyword",
            kw(i.clone(), "struct") => CxxTypeKind::Struct,
            kw(i.clone(), "class") => CxxTypeKind::Class,
            Ok((i.clone(), ())) => CxxTypeKind::None,
        )?;
        let kw_end = i.loc();

        let (i, ty) = cxx_path(i)?;
        let (i, _) = kw(i, "from")?;
        let (i, file) = string(i)?;
        let (i, _) = punct(i, ";")?;

        let end = i.loc();

        Ok((i, Spanned::new(Span { start, end }, Using { refcounted, kind: Spanned::new(Span { start: kw_start, end: kw_end }, kind), ty, file })))
    }
}

#[derive(Debug, Clone)]
pub struct Type {
    pub is_nullable: Option<Spanned<()>>,
    pub name: Spanned<CxxPathSeg>,
    pub is_array: Option<Spanned<()>>,
}

fn ty(i: In) -> PResult<Spanned<Type>> {
    fn array_brackets(i: In) -> PResult<Spanned<()>> {
        let (i, _) = punct(i, "[")?;
        commit! { punct(i, "]") }
    }

    let start = i.loc();
    let (i, nullable) = maybe(i.clone(), kw(i, "nullable"))?;
    let (i, name) = cxx_path_seg(i)?;
    let (i, array) = maybe(i.clone(), array_brackets(i))?;

    let end = i.loc();

    Ok((
        i,
        Spanned::new(
            Span { start, end },
            Type {
                is_nullable: nullable,
                name,
                is_array: array,
            },
        ),
    ))
}

fn component(i: In) -> PResult<Spanned<Type>> {
    let (i, ty) = ty(i)?;
    commit! {
        let (i, _) = punct(i, ";")?;
        Ok((i, ty))
    }
}

#[derive(Debug, Clone)]
pub struct UnionItem {
    pub path: Vec<Spanned<String>>,
    pub components: Vec<Spanned<Type>>,
}

fn union_item(i: In) -> PResult<Spanned<UnionItem>> {
    let start = i.loc();
    let (i, _) = kw(i, "union")?;
    commit! {
        let (i, name) = ident(i)?;
        let (i, _) = punct(i, "{")?;
        let (i, components) = many(i, component)?;
        let (i, _) = punct(i, "}")?;
        let (i, _) = punct(i, ";")?;

        let end = i.loc();

        Ok((i, Spanned::new(Span { start, end }, UnionItem {
            path: vec![name],
            components,
        })))
    }
}

#[derive(Debug, Clone)]
pub struct Field {
    pub ty: Spanned<Type>,
    pub name: Spanned<String>,
}

fn field(i: In) -> PResult<Spanned<Field>> {
    let start = i.loc();
    let (i, ty) = ty(i)?;
    commit! {
        let (i, name) = ident(i)?;
        let (i, _) = punct(i, ";")?;
        let end = i.loc();
        Ok((i, Spanned::new(Span {start, end}, Field { ty, name })))
    }
}

#[derive(Debug, Clone)]
pub struct StructItem {
    pub path: Vec<Spanned<String>>,
    pub fields: Vec<Spanned<Field>>,
}

fn struct_item(i: In) -> PResult<Spanned<StructItem>> {
    let start = i.loc();
    let (i, _) = kw(i, "struct")?;
    commit! {
        let (i, name) = ident(i)?;
        let (i, _) = punct(i, "{")?;
        let (i, fields) = many(i, field)?;
        let (i, _) = punct(i, "}")?;
        let (i, _) = punct(i, ";")?;

        let end = i.loc();

        Ok((i, Spanned::new(Span { start, end }, StructItem {
            path: vec![name],
            fields,
        })))
    }
}

#[derive(Debug, Clone)]
pub enum Nesting {
    None,
    InsideSync,
    InsideCpow,
}

#[allow(needless_pass_by_value)]
fn nesting(i: In) -> PResult<Spanned<Nesting>> {
    let start = i.loc();
    let (i, nesting) = any!(
        i, "nesting specifier (not, inside_sync, or inside_cpow)",
        kw(i.clone(), "not") => Nesting::None,
        kw(i.clone(), "inside_sync") => Nesting::InsideSync,
        kw(i.clone(), "inside_cpow") => Nesting::InsideCpow,
    )?;
    let end = i.loc();
    Ok((i, Spanned::new(Span { start, end }, nesting)))
}

#[derive(Debug, Clone)]
pub enum Priority {
    Normal,
    High,
    Input,
}

#[allow(needless_pass_by_value)]
fn priority(i: In) -> PResult<Spanned<Priority>> {
    let start = i.loc();
    let (i, priority) = any!(
        i, "priority specifier (normal, high, or input)",
        kw(i.clone(), "normal") => Priority::Normal,
        kw(i.clone(), "high") => Priority::High,
        kw(i.clone(), "input") => Priority::Input,
    )?;
    let end = i.loc();
    Ok((i, Spanned::new(Span { start, end }, priority)))
}

#[derive(Debug, Clone)]
pub enum SendSemantics {
    Async,
    Sync,
    Intr,
}

#[derive(Debug, Clone)]
pub enum MessageModifier {
    Verify,
    Compress,
    CompressAll,
}

#[allow(needless_pass_by_value)]
fn message_modifier(i: In) -> PResult<Spanned<MessageModifier>> {
    let start = i.loc();
    let (i, message_modifier) = any!(
        i, "message modifier (verify, compress, or compressall)",
        kw(i.clone(), "verify") => MessageModifier::Verify,
        kw(i.clone(), "compress") => MessageModifier::Compress,
        kw(i.clone(), "compressall") => MessageModifier::CompressAll,
    )?;
    let end = i.loc();
    Ok((i, Spanned::new(Span { start, end }, message_modifier)))
}

#[derive(Debug, Clone)]
pub struct Param {
    pub ty: Spanned<Type>,
    pub name: Spanned<String>,
}

fn param(i: In) -> PResult<Spanned<Param>> {
    let start = i.loc();
    let (i, ty) = ty(i)?;
    commit! {
        let (i, name) = ident(i)?;
        let end = i.loc();
        Ok((i, Spanned::new(Span { start, end }, Param { ty, name })))
    }
}

#[derive(Debug, Clone)]
pub struct MessageDecl {
    pub nested: Option<Spanned<Spanned<Nesting>>>,
    pub priority: Option<Spanned<Spanned<Priority>>>,
    pub send_semantics: Spanned<SendSemantics>,
    pub name: Spanned<String>,
    pub params: Vec<Spanned<Param>>,
    pub returns: Option<Spanned<Vec<Spanned<Param>>>>,
    pub modifiers: Vec<Spanned<MessageModifier>>,
}

fn returns(i: In) -> PResult<Spanned<Vec<Spanned<Param>>>> {
    let start = i.loc();
    let (i, _) = kw(i, "returns")?;
    commit! {
        let (i, _) = punct(i, "(")?;
        let (i, p) = sep(i, param, ",")?;
        let (i, _) = punct(i, ")")?;
        let end = i.loc();
        Ok((i, Spanned::new(Span { start, end }, p)))
    }
}

fn message_nested(i: In) -> PResult<Spanned<Spanned<Nesting>>> {
    let start = i.loc();
    let (i, _) = kw(i, "nested")?;
    commit! {
        let (i, _) = punct(i, "(")?;
        let (i, nested) = nesting(i)?;
        let (i, _) = punct(i, ")")?;

        let end = i.loc();

        Ok((i, Spanned::new(Span { start, end }, nested)))
    }
}

fn message_prio(i: In) -> PResult<Spanned<Spanned<Priority>>> {
    let start = i.loc();
    let (i, _) = kw(i, "prio")?;
    commit! {
        let (i, _) = punct(i, "(")?;
        let (i, prio) = priority(i)?;
        let (i, _) = punct(i, ")")?;

        let end = i.loc();

        Ok((i, Spanned::new(Span { start, end }, prio)))
    }
}

fn message_decl(i: In) -> PResult<Spanned<MessageDecl>> {
    let start = i.loc();

    // XXX(nika): This is really gross, maybe clean it up?
    let mut nested = None;
    let mut priority = None;
    drive!(
        i,
        any!(
        i, "message prefix",
        message_prio(i.clone()) => |p| priority = Some(p),
        message_nested(i.clone()) => |n| nested = Some(n),
    )
    );

    let send_semantics_start = i.loc();
    let (i, send_semantics) = any!(
        i, "send semantics (async, sync, or intr)",
        kw(i.clone(), "async") => SendSemantics::Async,
        kw(i.clone(), "sync") => SendSemantics::Sync,
        kw(i.clone(), "intr") => SendSemantics::Intr,
    )?;
    let send_semantics_end = i.loc();

    commit! {
        let (i, name) = ident(i)?;
        let (i, _) = punct(i, "(")?;
        let (i, params) = sep(i, param, ",")?;
        let (i, _) = punct(i, ")")?;
        let (i, returns) = maybe(i.clone(), returns(i))?;
        let (i, modifiers) = many(i, message_modifier)?;
        let (i, _) = punct(i, ";")?;

        let end = i.loc();

        Ok((i, Spanned::new(Span { start, end }, MessageDecl {
            nested,
            priority,
            send_semantics: Spanned::new(Span { start: send_semantics_start, end: send_semantics_end }, send_semantics),
            name,
            params,
            returns,
            modifiers,
        })))
    }
}

#[derive(Debug, Clone)]
pub enum Direction {
    ToChild,
    ToParent,
    Both,
}

#[allow(needless_pass_by_value)]
fn direction(i: In) -> PResult<Spanned<Direction>> {
    let start = i.loc();
    let (i, direction) = any!(
        i, "direction (child, parent, or both)",
        kw(i.clone(), "child") => Direction::ToChild,
        kw(i.clone(), "parent") => Direction::ToParent,
        kw(i.clone(), "both") => Direction::Both,
    )?;
    let end = i.loc();
    Ok((i, Spanned::new(Span { start, end }, direction)))
}

#[derive(Debug, Clone)]
pub struct MessageGroup {
    pub direction: Spanned<Direction>,
    pub decls: Vec<Spanned<MessageDecl>>,
}

fn message_group(i: In) -> PResult<Spanned<MessageGroup>> {
    let start = i.loc();
    let (i, direction) = direction(i)?;
    commit! {
        let (i, _) = punct(i, ":")?;
        let (i, decls) = many(i, message_decl)?;

        let end = i.loc();

        Ok((i, Spanned::new(Span { start, end }, MessageGroup {
            direction,
            decls,
        })))
    }
}

#[derive(Debug, Clone)]
pub struct ProtocolItem {
    pub path: Vec<Spanned<String>>,
    pub nested: Option<Spanned<Spanned<Nesting>>>,
    pub send_semantics: Spanned<SendSemantics>,
    pub managers: Option<Spanned<Vec<Spanned<String>>>>,
    pub manages: Vec<Spanned<Spanned<String>>>,
    pub groups: Vec<Spanned<MessageGroup>>,
}

fn protocol_nested(i: In) -> PResult<(Spanned<Spanned<Nesting>>, Spanned<SendSemantics>)> {
    let nested_start = i.loc();
    let (i, _) = kw(i, "nested")?;
    commit! {
        let (i, _) = punct(i, "(")?;
        let (i, _) = kw(i, "upto")?;
        let (i, n) = nesting(i)?;
        let (i, _) = punct(i, ")")?;
        let nested_end = i.loc();
        let ss_start = i.loc();
        let (i, ss) = any!(
            i, "send semantics (async or sync)",
            kw(i.clone(), "async") => SendSemantics::Async,
            kw(i.clone(), "sync") => SendSemantics::Sync,
        )?;
        let ss_end = i.loc();
        Ok((i, (Spanned::new(Span { start: nested_start, end: nested_end }, n), Spanned::new(Span { start: ss_start, end: ss_end}, ss))))
    }
}

fn managers(i: In) -> PResult<Spanned<Vec<Spanned<String>>>> {
    let start = i.loc();
    let (i, _) = kw(i, "manager")?;
    commit! {
        let (i, managers) = sep(i, ident, "or")?;
        let (i, _) = punct(i, ";")?;
        let end = i.loc();
        Ok((i, Spanned::new(Span {start, end}, managers)))
    }
}

fn manages(i: In) -> PResult<Spanned<Spanned<String>>> {
    let start = i.loc();
    let (i, _) = kw(i, "manages")?;
    commit! {
        let (i, name) = ident(i)?;
        let (i, _) = punct(i, ";")?;
        let end = i.loc();
        Ok((i, Spanned::new(Span { start, end }, name)))
    }
}

#[allow(needless_pass_by_value)]
fn protocol_item(i: In) -> PResult<Spanned<ProtocolItem>> {
    let start = i.loc();
    let mut ss_span = Span::new(i.loc().file);

    let ss_start = i.loc();
    let (i, (nested, send_semantics)) = any!(
        i, "protocol item prefixes",
        kw(i.clone(), "async") => |_x| (None, SendSemantics::Async),
        kw(i.clone(), "sync") => |_x| (None, SendSemantics::Sync),
        kw(i.clone(), "intr") => |_x| (None, SendSemantics::Intr),
        protocol_nested(i.clone()) => |x| {ss_span = x.1.span; (Some(x.0), x.1.data)},
        Ok((i.clone(), ())) => |_x| (None, SendSemantics::Async),
    )?;
    let ss_end = i.loc();

    if ss_span.is_null() {
        ss_span = Span {
            start: ss_start,
            end: ss_end,
        };
    }

    let (i, _) = kw(i, "protocol")?;
    commit! {
        let (i, name) = ident(i)?;
        let (i, _) = punct(i, "{")?;
        let (i, managers) = maybe(i.clone(), managers(i))?;
        let (i, manages) = many(i, manages)?;
        let (i, groups) = many(i, message_group)?;
        let (i, _) = punct(i, "}")?;
        let (i, _) = punct(i, ";")?;

        let end = i.loc();
        Ok((i, Spanned::new(Span { start, end }, ProtocolItem {
            send_semantics: Spanned::new(ss_span, send_semantics),
            nested,
            path: vec![name],
            managers,
            manages,
            groups,
        })))
    }
}

#[derive(Debug, Clone)]
#[allow(large_enum_variant)]
pub enum Item {
    Struct(Spanned<StructItem>),
    Union(Spanned<UnionItem>),
    Protocol(Spanned<ProtocolItem>),
}

fn namespace(i: In) -> PResult<Vec<Item>> {
    let (i, _) = kw(i, "namespace")?;

    commit! {
        let (i, name) = ident(i)?;
        let (i, _) = punct(i, "{")?;
        let (i, mut items) = items(i)?;
        let (i, _) = punct(i, "}")?;
        for it in &mut items {
            match *it  {
                Item::Struct(ref mut i) =>
                    i.data.path.insert(0, name.clone()),
                Item::Union(ref mut i) =>
                    i.data.path.insert(0, name.clone()),
                Item::Protocol(ref mut i) =>
                    i.data.path.insert(0, name.clone()),
            }
        }
        Ok((i, items))
    }
}

fn items(i: In) -> PResult<Vec<Item>> {
    let mut v = Vec::new();
    drive!(
        i,
        any!(
        i, "item (struct, union, protocol, or namespace)",
        struct_item(i.clone()) => |x| v.push(Item::Struct(x)),
        union_item(i.clone()) => |x| v.push(Item::Union(x)),
        protocol_item(i.clone()) => |x| v.push(Item::Protocol(x)),
        namespace(i.clone()) => |x| v.extend(x),
    )
    );
    Ok((i, v))
}

#[derive(Debug, Clone)]
pub struct TranslationUnit {
    pub cxx_includes: Vec<Spanned<CxxInclude>>,
    pub includes: Vec<Spanned<Include>>,
    pub usings: Vec<Spanned<Using>>,
    pub items: Vec<Item>,
}

fn translation_unit(i: In) -> PResult<Spanned<TranslationUnit>> {
    // Prelude.
    let mut usings = Vec::new();
    let mut includes = Vec::new();
    let mut cxx_includes = Vec::new();

    let start = i.loc();

    drive!(
        i,
        any!(
        i, "include or using declaration",
        using(i.clone()) => |u| usings.push(u),
        include(i.clone()) => |u| includes.push(u),
        cxx_include(i.clone()) => |u| cxx_includes.push(u),
    )
    );

    // Body.
    let (i, items) = items(i)?;

    // Make sure we're at EOF
    let i = skip_ws(i)?;
    if !i.rest().is_empty() {
        return i.expected("item (struct, union, protocol, or namespace)");
    }

    let end = i.loc();

    Ok((
        i,
        Spanned::new(
            Span { start, end },
            TranslationUnit {
                cxx_includes,
                includes,
                usings,
                items,
            },
        ),
    ))
}

/// Entry point - parses a whole translation unit.
pub fn parse<'filepath>(
    src: &str,
    file: &'filepath str,
) -> Result<Spanned<TranslationUnit>, Error> {
    match translation_unit(In::new(src, file.to_owned())) {
        Ok((_, v)) => Ok(v),
        Err(err) => Err(Error(Box::new(err))),
    }
}
