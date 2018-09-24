/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use ast;
use errors::Errors;
use parser;
use passes::include_resolution::IncludeResolver;
use pipdl;

pub struct ParseTreeToTU<'includedirs, T: 'includedirs> where T: parser::FileURI {
    include_resolver: &'includedirs IncludeResolver<'includedirs, T>,
}

impl<'includedirs, T> ParseTreeToTU<'includedirs, T> where T: parser::FileURI {
    pub fn new(include_resolver: &'includedirs IncludeResolver<'includedirs, T>) -> Self {
        ParseTreeToTU { include_resolver }
    }

    pub fn parsetree_to_translation_unit(
        &self,
        parse_tree: parser::ParseTree<T>,
    ) -> Result<ast::TranslationUnit, Errors> {
        self.convert_translation_unit(parse_tree.translation_unit.data, parse_tree.file_path.to_utf8().to_owned())
    }

    fn convert_translation_unit(
        &self,
        pt_translation_unit: pipdl::TranslationUnit,
        pt_filename: String,
    ) -> Result<ast::TranslationUnit, Errors> {
        let mut structs = Vec::new();
        let mut unions = Vec::new();
        let mut protocol = None;
        let mut last_is_struct = false;

        for item in pt_translation_unit.items {
            match item {
                pipdl::Item::Struct(struct_item) => {
                    structs.push(self.convert_struct_item(struct_item.data));
                    last_is_struct = true;
                }
                pipdl::Item::Union(union_item) => {
                    unions.push(self.convert_union_item(union_item.data));
                    last_is_struct = false;
                }
                pipdl::Item::Protocol(protocol_item) => match protocol {
                    Some(_) => {
                        return Err(Errors::one(
                            &ast::Location {
                                file_name: pt_filename,
                                lineno: protocol_item.span.start.line,
                                colno: protocol_item.span.start.col,
                            },
                            "only one protocol definition per file",
                        ))
                    }
                    None => protocol = Some(self.convert_protocol_item(protocol_item.data)),
                },
            }
        }

        Ok(ast::TranslationUnit {
            cxx_includes: pt_translation_unit
                .cxx_includes
                .into_iter()
                .map(|x| x.data.file.data)
                .collect(),

            includes: pt_translation_unit
                .includes
                .into_iter()
                .map(|x| {
                    let filename = format!(
                        "{}{}{}",
                        x.data.id.data,
                        ".ipdl",
                        if x.data.protocol.is_some() { "" } else { "h" }
                    );
                    self.include_resolver
                        .get_include(&filename)
                        .expect("Cannot find include TUId when converting ParseTree to AST")
                })
                .collect(),

            using: pt_translation_unit
                .usings
                .into_iter()
                .map(|x| self.convert_using_stmt(x.data))
                .collect(),

            namespace: match &protocol {
                Some(p) => p.0.clone(),
                None => {
                    // There's not really a canonical "thing" in headers. So
                    // somewhat arbitrarily use the namespace of the last
                    // interesting thing that was declared.
                    if last_is_struct {
                        structs.last().expect("last_is_struct is broken").0.clone()
                    } else {
                        match unions.last() {
                            Some(u) => u.0.clone(),
                            None => {
                                return Err(Errors::one(
                                    &ast::Location {
                                        file_name: pt_filename,
                                        lineno: 0,
                                        colno: 0,
                                    },
                                    "file is empty",
                                ))
                            }
                        }
                    }
                }
            },
            structs,
            unions,
            protocol,
            file_type: ast::FileType::from_file_path(&pt_filename)
                .expect("Cannot determine file type when converting parse tree to AST"),
            file_name: pt_filename,
        })
    }

    fn convert_using_stmt(&self, pt_using_stmt: pipdl::Using) -> ast::UsingStmt {
        ast::UsingStmt {
            cxx_type: self.convert_cxx_type(pt_using_stmt.ty.data),
            header: pt_using_stmt.file.data,
            kind: self.convert_cxx_type_kind(&pt_using_stmt.kind.data),
            refcounted: pt_using_stmt.refcounted.is_some(),
        }
    }

    fn convert_cxx_type_kind(&self, pt_type_kind: &pipdl::CxxTypeKind) -> Option<ast::CxxTypeKind> {
        match pt_type_kind {
            pipdl::CxxTypeKind::Class => Some(ast::CxxTypeKind::Class),
            pipdl::CxxTypeKind::Struct => Some(ast::CxxTypeKind::Struct),
            pipdl::CxxTypeKind::None => None,
        }
    }

    fn convert_cxx_type(&self, pt_cxx_type: pipdl::CxxPath) -> ast::TypeSpec {
        ast::TypeSpec {
            spec: self.convert_cxx_path(pt_cxx_type),
            array: false,
            nullable: false,
        }
    }

    fn convert_cxx_path(&self, mut pt_cxx_path: pipdl::CxxPath) -> ast::QualifiedId {
        ast::QualifiedId {
            base_id: self.convert_cxx_path_seg(
                pt_cxx_path
                    .segs
                    .pop()
                    .expect("Empty path when converting into QualifiedId")
                    .data,
            ),
            quals: pt_cxx_path
                .segs
                .into_iter()
                .map(|x| self.convert_cxx_path_seg(x.data).id)
                .collect(),
        }
    }

    fn convert_cxx_path_seg(&self, pt_cxx_path_seg: pipdl::CxxPathSeg) -> ast::Identifier {
        ast::Identifier {
            id: match pt_cxx_path_seg.args {
                Some(args) => format!(
                    "{}<{}>",
                    pt_cxx_path_seg.id.data,
                    args.data
                        .into_iter()
                        .map(|x| x.data)
                        .collect::<Vec<String>>()
                        .concat()
                ),
                None => pt_cxx_path_seg.id.data,
            },
            loc: self.convert_location(pt_cxx_path_seg.id.span.start),
        }
    }

    fn convert_location(&self, pt_location: pipdl::Location) -> ast::Location {
        ast::Location {
            file_name: pt_location.file,
            lineno: pt_location.line,
            colno: pt_location.col,
        }
    }

    fn convert_struct_item(
        &self,
        pt_struct_item: pipdl::StructItem,
    ) -> (ast::Namespace, Vec<ast::StructField>) {
        (
            self.convert_path(pt_struct_item.path),
            pt_struct_item
                .fields
                .into_iter()
                .map(|x| self.convert_struct_field(x.data))
                .collect(),
        )
    }

    fn convert_path(&self, mut pt_path: Vec<pipdl::Spanned<String>>) -> ast::Namespace {
        ast::Namespace {
            name: self.convert_name(
                pt_path
                    .pop()
                    .expect("Empty path when converting into Namespace"),
            ),
            namespaces: pt_path.into_iter().map(|x| x.data).collect(),
        }
    }

    fn convert_struct_field(&self, pt_struct_field: pipdl::Field) -> ast::StructField {
        ast::StructField {
            type_spec: self.convert_type(pt_struct_field.ty.data),
            name: self.convert_name(pt_struct_field.name),
        }
    }

    fn convert_name(&self, pt_name: pipdl::Spanned<String>) -> ast::Identifier {
        ast::Identifier {
            id: pt_name.data,
            loc: self.convert_location(pt_name.span.start),
        }
    }

    fn convert_type(&self, pt_type: pipdl::Type) -> ast::TypeSpec {
        ast::TypeSpec {
            spec: ast::QualifiedId::new(self.convert_cxx_path_seg(pt_type.name.data)),
            array: pt_type.is_array.is_some(),
            nullable: pt_type.is_nullable.is_some(),
        }
    }

    fn convert_union_item(
        &self,
        pt_union_item: pipdl::UnionItem,
    ) -> (ast::Namespace, Vec<ast::TypeSpec>) {
        (
            self.convert_path(pt_union_item.path),
            pt_union_item
                .components
                .into_iter()
                .map(|x| self.convert_type(x.data))
                .collect(),
        )
    }

    fn convert_protocol_item(
        &self,
        pt_protocol_item: pipdl::ProtocolItem,
    ) -> (ast::Namespace, ast::Protocol) {
        (
            self.convert_path(pt_protocol_item.path),
            ast::Protocol {
                send_semantics: self.convert_send_semantics(&pt_protocol_item.send_semantics.data),
                nested: self.convert_nesting(pt_protocol_item.nested),
                managers: match pt_protocol_item.managers {
                    Some(managers) => managers
                        .data
                        .into_iter()
                        .map(|x| self.convert_name(x))
                        .collect(),
                    None => Vec::new(),
                },
                manages: pt_protocol_item
                    .manages
                    .into_iter()
                    .map(|x| self.convert_name(x.data))
                    .collect(),
                messages: pt_protocol_item
                    .groups
                    .into_iter()
                    .flat_map(|group| {
                        let (decls, direction) = (group.data.decls, group.data.direction.data);
                        decls
                            .into_iter()
                            .map(move |x| (x.data, direction.clone()))
                            .map(|(message_decl, direction)| {
                                self.convert_message_decl(message_decl, &direction)
                            })
                    })
                    .collect(),
            },
        )
    }

    fn convert_message_decl(
        &self,
        pt_message_decl: pipdl::MessageDecl,
        pt_direction: &pipdl::Direction,
    ) -> ast::MessageDecl {
        let mut compress = ast::Compress::None;
        let mut verify = false;

        for modifier in pt_message_decl.modifiers {
            match modifier.data {
                pipdl::MessageModifier::Compress => compress = ast::Compress::Enabled,
                pipdl::MessageModifier::CompressAll => compress = ast::Compress::All,
                pipdl::MessageModifier::Verify => verify = true,
            }
        }

        ast::MessageDecl {
            name: self.convert_name(pt_message_decl.name),
            send_semantics: self.convert_send_semantics(&pt_message_decl.send_semantics.data),
            nested: self.convert_nesting(pt_message_decl.nested),
            prio: self.convert_priority(pt_message_decl.priority),
            direction: self.convert_direction(&pt_direction),
            in_params: pt_message_decl
                .params
                .into_iter()
                .map(|x| self.convert_param(x.data))
                .collect(),
            out_params: match pt_message_decl.returns {
                Some(returns) => returns
                    .data
                    .into_iter()
                    .map(|x| self.convert_param(x.data))
                    .collect(),
                None => Vec::new(),
            },
            compress,
            verify,
        }
    }

    fn convert_param(&self, pt_param: pipdl::Param) -> ast::Param {
        ast::Param {
            name: self.convert_name(pt_param.name),
            type_spec: self.convert_type(pt_param.ty.data),
        }
    }

    fn convert_send_semantics(
        &self,
        pt_send_semantics: &pipdl::SendSemantics,
    ) -> ast::SendSemantics {
        match pt_send_semantics {
            pipdl::SendSemantics::Async => ast::SendSemantics::Async,
            pipdl::SendSemantics::Sync => ast::SendSemantics::Sync,
            pipdl::SendSemantics::Intr => ast::SendSemantics::Intr,
        }
    }

    fn convert_nesting(
        &self,
        pt_nesting: Option<pipdl::Spanned<pipdl::Spanned<pipdl::Nesting>>>,
    ) -> ast::Nesting {
        match pt_nesting {
            Some(x) => match x.data.data {
                pipdl::Nesting::None => ast::Nesting::None,
                pipdl::Nesting::InsideCpow => ast::Nesting::InsideCpow,
                pipdl::Nesting::InsideSync => ast::Nesting::InsideSync,
            },
            None => ast::Nesting::None,
        }
    }

    fn convert_priority(
        &self,
        pt_priority: Option<pipdl::Spanned<pipdl::Spanned<pipdl::Priority>>>,
    ) -> ast::Priority {
        match pt_priority {
            Some(x) => match x.data.data {
                pipdl::Priority::Normal => ast::Priority::Normal,
                pipdl::Priority::High => ast::Priority::High,
                pipdl::Priority::Input => ast::Priority::Input,
            },
            None => ast::Priority::Normal,
        }
    }

    fn convert_direction(&self, pt_direction: &pipdl::Direction) -> ast::Direction {
        match pt_direction {
            pipdl::Direction::ToChild => ast::Direction::ToChild,
            pipdl::Direction::ToParent => ast::Direction::ToParent,
            pipdl::Direction::Both => ast::Direction::ToParentOrChild,
        }
    }
}
