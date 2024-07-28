mod aaa;

// use serde::Deserialize;
// use swc_core::ecma::ast::{
//     AssignPatProp, AwaitExpr, BinExpr, BinaryOp, BindingIdent, Callee, Decl, ExprOrSpread,
//     ExprStmt, Ident, Import, KeyValuePatProp, Lit, MemberExpr, MemberProp, ModuleItem, ObjectPat,
//     ObjectPatProp, ParenExpr, Pat, PropName, Str, VarDecl, VarDeclarator,
// };
// use swc_core::plugin::{plugin_transform, proxies::TransformPluginProgramMetadata};
// use swc_core::{
//     common::DUMMY_SP,
//     ecma::{
//         ast::{CallExpr, Expr, Program, Stmt},
//         transforms::testing::test_inline,
//         visit::{as_folder, FoldWith, VisitMut, VisitMutWith},
//     },
// };
// mod aaa;

// pub struct TransformVisitor {
//     remove_import: Option<String>,
// }

// impl VisitMut for TransformVisitor {
//     fn visit_mut_module_items(&mut self, module_items: &mut std::vec::Vec<ModuleItem>) {
//         module_items.visit_mut_children_with(self);
//         module_items.retain(|item| {
//             if let Some(module_decl) = item.clone().as_module_decl() {
//                 if let Some(import) = module_decl.as_import() {
//                     if let Some(remove_import) = &self.remove_import {
//                         if import.src.value.as_str() == remove_import {
//                             return false;
//                         }
//                     }
//                 }
//             }
//             true
//         });
//         for module_item in module_items {
//             self.nora_visit_mut_module_item(module_item);
//         }
//     }
//     // Implement necessary visit_mut_* methods for actual custom transform.
//     // A comprehensive list of possible visitor methods can be found here:
//     // https://rustdoc.swc.rs/swc_ecma_visit/trait.VisitMut.html

// }

// impl TransformVisitor {
//     fn nora_visit_mut_module_item(&mut self, module_item: &mut swc_core::ecma::ast::ModuleItem) {
//         if let Some(module_decl) = module_item.clone().as_module_decl() {
//             if let Some(import) = module_decl.as_import() {
//                 let await_import = Expr::Await(AwaitExpr {
//                     span: DUMMY_SP,
//                     arg: Box::new(Expr::Call(CallExpr { span: DUMMY_SP, callee: Callee::Import(Import{span:DUMMY_SP,phase:swc_core::ecma::ast::ImportPhase::Evaluation}), args: vec![ExprOrSpread{
//                         spread: None,
//                         expr: Expr::Bin(BinExpr {
//                             span: DUMMY_SP,
//                             op: BinaryOp::Add,
//                             left: Box::new(Expr::Lit(Lit::Str(Str {
//                                 span: DUMMY_SP,
//                                 value: "data:text/javascript;charset=utf-8,".into(),
//                                 raw: None,
//                             }))),
//                             right: Expr::Call(CallExpr {
//                                 span: DUMMY_SP,
//                                 callee: Callee::Expr(
//                                     Expr::Ident(Ident::new("encodeURIComponent".into(), DUMMY_SP))
//                                         .into(),
//                                 ),
//                                 args: vec![ExprOrSpread {
//                                     spread: None,
//                                     expr: Expr::Await(AwaitExpr {
//                                         span: DUMMY_SP,
//                                         arg: Box::new(Expr::Call(CallExpr {
//                                             span: DUMMY_SP,
//                                             callee: Callee::Expr(
//                                                 Expr::Member(MemberExpr {
//                                                     span: DUMMY_SP,
//                                                     obj: Box::new(Expr::Paren(ParenExpr {
//                                                         span: DUMMY_SP,
//                                                         expr: Box::new(Expr::Await(AwaitExpr {
//                                                             span: DUMMY_SP,
//                                                             arg: Box::new(Expr::Call(CallExpr {
//                                                                 span: DUMMY_SP,
//                                                                 callee: Callee::Expr(
//                                                                     Expr::Ident(Ident::new(
//                                                                         "fetch".into(),
//                                                                         DUMMY_SP,
//                                                                     ))
//                                                                     .into(),
//                                                                 ),
//                                                                 type_args: None,
//                                                                 args: vec![ExprOrSpread {
//                                                                     spread: None,
//                                                                     expr: Expr::Lit(Lit::Str(Str {
//                                                                         span: DUMMY_SP,
//                                                                         value: import.src.value.clone(),
//                                                                         raw: None,
//                                                                     }))
//                                                                     .into(),
//                                                                 }],
//                                                             })),
//                                                         })),
//                                                     })),
//                                                     prop: MemberProp::Ident(Ident {
//                                                         span: DUMMY_SP,
//                                                         sym: "text".into(),
//                                                         optional: false,
//                                                     }),
//                                                 })
//                                                 .into(),
//                                             ),
//                                             args: vec![],
//                                             type_args: None,
//                                         })),
//                                     })
//                                     .into(),
//                                 }]
//                                 ,
//                                 type_args: None,
//                             })
//                             .into(),
//                         }).into()
//                     }], type_args: None })),
//                 });

//                 if import.specifiers.is_empty() {
//                     *module_item = ModuleItem::Stmt(Stmt::Expr(ExprStmt {
//                         span: DUMMY_SP,
//                         expr: Box::new(await_import),
//                     }));
//                 } else if import.specifiers.len() == 1 && import.specifiers[0].is_namespace() {
//                     *module_item = ModuleItem::Stmt(Stmt::Decl(Decl::Var(
//                         VarDecl {
//                             span: DUMMY_SP,
//                             kind: swc_core::ecma::ast::VarDeclKind::Const,
//                             declare: false,
//                             decls: vec![VarDeclarator {
//                                 span: DUMMY_SP,
//                                 name: Pat::Ident(BindingIdent {
//                                     id: import.specifiers[0].as_namespace().unwrap().local.clone(),
//                                     type_ann: None,
//                                 }),
//                                 init: Some(await_import.into()),
//                                 definite: false,
//                             }],
//                         }
//                         .into(),
//                     )));
//                 } else if import
//                     .specifiers
//                     .iter()
//                     .all(|specifier| specifier.is_named() || specifier.is_default())
//                 {
//                     let mut spec_syms = vec![];
//                     for specifier in &import.specifiers {
//                         if specifier.is_named() {
//                             let as_id = specifier
//                                 .as_named()
//                                 .unwrap()
//                                 .imported
//                                 .as_ref()
//                                 .map(|imp| imp.atom().clone());
//                             spec_syms.push((specifier.as_named().unwrap().local.clone(), as_id));
//                         } else {
//                             spec_syms.push((
//                                 specifier.as_default().unwrap().local.clone(),
//                                 Some("default".into()),
//                             ));
//                         }
//                     }

//                     *module_item = ModuleItem::Stmt(Stmt::Decl(Decl::Var(
//                         VarDecl {
//                             span: DUMMY_SP,
//                             kind: swc_core::ecma::ast::VarDeclKind::Const,
//                             declare: false,
//                             decls: vec![VarDeclarator {
//                                 span: DUMMY_SP,
//                                 name: Pat::Object(ObjectPat {
//                                     span: DUMMY_SP,
//                                     props: spec_syms
//                                         .iter()
//                                         .map(|(sym, as_id)| match as_id {
//                                             None => ObjectPatProp::Assign(AssignPatProp {
//                                                 span: DUMMY_SP,
//                                                 key: BindingIdent {
//                                                     id: sym.clone(),
//                                                     type_ann: None,
//                                                 },
//                                                 value: None,
//                                             }),
//                                             Some(id) => ObjectPatProp::KeyValue(KeyValuePatProp {
//                                                 key: PropName::Str(Str {
//                                                     span: DUMMY_SP,
//                                                     value: id.clone(),
//                                                     raw: Some(id.clone()),
//                                                 }),
//                                                 value: Pat::Ident(BindingIdent {
//                                                     id: sym.clone(),
//                                                     type_ann: None,
//                                                 })
//                                                 .into(),
//                                             }),
//                                         })
//                                         .collect(),
//                                     optional: false,
//                                     type_ann: None,
//                                 }),
//                                 init: Some(await_import.into()),
//                                 definite: false,
//                             }],
//                         }
//                         .into(),
//                     )));
//                 }
//             }
//         }
//     }
// }

// #[derive(Deserialize)]
// #[serde(rename_all = "camelCase")]
// struct NoranekoHMRTransformerConfig {
//     remove_import: Option<String>,
// }

// /// An example plugin function with macro support.
// /// `plugin_transform` macro interop pointers into deserialized structs, as well
// /// as returning ptr back to host.
// ///
// /// It is possible to opt out from macro by writing transform fn manually
// /// if plugin need to handle low-level ptr directly via
// /// `__transform_plugin_process_impl(
// ///     ast_ptr: *const u8, ast_ptr_len: i32,
// ///     unresolved_mark: u32, should_enable_comments_proxy: i32) ->
// ///     i32 /*  0 for success, fail otherwise.
// ///             Note this is only for internal pointer interop result,
// ///             not actual transform result */`
// ///
// /// This requires manual handling of serialization / deserialization from ptrs.
// /// Refer swc_plugin_macro to see how does it work internally.
// #[plugin_transform]
// pub fn process_transform(program: Program, data: TransformPluginProgramMetadata) -> Program {
//     let config = serde_json::from_str::<NoranekoHMRTransformerConfig>(
//         &data
//             .get_transform_plugin_config()
//             .expect("failed to get plugin config for noraneko-hmr-transformer"),
//     )
//     .expect("invalid config for enoraneko-hmr-transformermotion");
//     program.fold_with(&mut as_folder(TransformVisitor {
//         remove_import: config.remove_import,
//     }))
// }

// //? https://github.com/swc-project/swc/issues/356

// // An example to test plugin transform.
// // Recommended strategy to test plugin's transform is verify
// // the Visitor's behavior, instead of trying to run `process_transform` with mocks
// // unless explicitly required to do so.
// test_inline!(
//     Default::default(),
//     |_| as_folder(TransformVisitor {
//         remove_import: None
//     }),
//     import_simple,
//     // Input codes
//     r#"import "@nora/foo";"#,
//     // Output codes after transformed with plugin
//     r#"await import("data:text/javascript;charset=utf-8,"+encodeURIComponent(await (await fetch("@nora/foo")).text()));"#
// );

// test_inline!(
//     Default::default(),
//     |_| as_folder(TransformVisitor {
//         remove_import: None
//     }),
//     import_default,
//     // Input codes
//     r#"import a from "@nora/foo";"#,
//     // Output codes after transformed with plugin
//     r#"const {default:a} = await import("data:text/javascript;charset=utf-8,"+encodeURIComponent(await (await fetch("@nora/foo")).text()));"#
// );

// test_inline!(
//     Default::default(),
//     |_| as_folder(TransformVisitor {
//         remove_import: None
//     }),
//     import_object_simple,
//     // Input codes
//     r#"import {a} from "@nora/foo";"#,
//     // Outputa codes after transformed with plugin
//     r#"const {a} = await import("data:text/javascript;charset=utf-8,"+encodeURIComponent(await (await fetch("@nora/foo")).text()));"#
// );

// test_inline!(
//     Default::default(),
//     |_| as_folder(TransformVisitor {
//         remove_import: None
//     }),
//     import_object_complex,
//     // Input codes
//     r#"import {a,b,c} from "@nora/foo";"#,
//     // Outputa codes after transformed with plugin
//     r#"const {a,b,c} = await import("data:text/javascript;charset=utf-8,"+encodeURIComponent(await (await fetch("@nora/foo")).text()));"#
// );

// test_inline!(
//     Default::default(),
//     |_| as_folder(TransformVisitor {
//         remove_import: None
//     }),
//     import_object_complex_default_mix,
//     // Input codes
//     r#"import dd,{a,b,c} from "@nora/foo";"#,
//     // Outputa codes after transformed with plugin
//     r#"const {default:dd,a,b,c} = await import("data:text/javascript;charset=utf-8,"+encodeURIComponent(await (await fetch("@nora/foo")).text()));"#
// );

// test_inline!(
//     Default::default(),
//     |_| as_folder(TransformVisitor {
//         remove_import: None
//     }),
//     import_namespace,
//     // Input codes
//     r#"import * as a from "@nora/foo";"#,
//     // Outputa codes after transformed with plugin
//     r#"const a = await import("data:text/javascript;charset=utf-8,"+encodeURIComponent(await (await fetch("@nora/foo")).text()));"#
// );

// test_inline!(
//     Default::default(),
//     |_| as_folder(TransformVisitor {
//         remove_import: None
//     }),
//     import_as,
//     // Input codes
//     r#"import {a as _$a} from "@nora/foo";"#,
//     // Outputa codes after transformed with plugin
//     r#"const {a:_$a} = await import("data:text/javascript;charset=utf-8,"+encodeURIComponent(await (await fetch("@nora/foo")).text()));"#
// );

// test_inline!(
//     Default::default(),
//     |_| as_folder(TransformVisitor {
//         remove_import: Some("@nora/foo".to_string())
//     }),
//     import_remove,
//     // Input codes
//     r#"import {a as _$a} from "@nora/foo";"#,
//     // Outputa codes after transformed with plugin
//     r#""#
// );
