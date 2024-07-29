#![allow(clippy::collapsible_else_if)]

use std::fmt::format;

use oxc::allocator::Allocator;
use oxc::ast::ast::{
    CallExpression, ClassElement, Declaration, Expression, ModuleDeclaration, Statement,
};

use oxc::parser::Parser;
use oxc::span::SourceType;

use crate::mixin_parser::{AtValue, Mixin, MixinMeta};

fn find_match_call_expr<'stmt>(
    stmt: &'stmt Statement<'stmt>,
    id: String,
) -> Option<&'stmt CallExpression<'stmt>> {
    match stmt {
        Statement::ExpressionStatement(expr_stmt) => {
            if let Expression::CallExpression(call_expr) = &expr_stmt.expression {
                //let callee = &call_expr.callee;

                //println!("ccc {:?}", call_expr.callee_name());
                if call_expr.callee_name() == Some(&id) {
                    return Some(call_expr);
                }
            }
            None
        }
        Statement::TryStatement(try_stmt) => {
            for _stmt in &try_stmt.block.body {
                let tmp = find_match_call_expr(_stmt, id.clone());
                if tmp.is_some() {
                    //println!("is Some!");
                    return tmp;
                }
            }
            None
        }
        _ => None,
    }
}

pub fn transform(source: String, metadata: String) -> Result<String, String> {
    let source_text = {
        let mut tmp = source;
        while let Some(start) = tmp.find("/*@nora:inject:start*/") {
            let end = tmp.find("/*@nora:inject:end*/").unwrap();
            tmp = tmp[..start].to_string() + &tmp[end + "/*@nora:inject:end*/".len()..];
        }
        tmp
    };
    let value = serde_json::from_str::<Mixin>(&metadata).unwrap();

    let mut return_src = source_text.clone();
    let allocator = Allocator::default();
    let source_type = SourceType::from_path(value.meta.path())
        .map_err(|e| format!("SourceType UnknownExtension {}", e.0))?;
    let now = std::time::Instant::now();
    let ret = Parser::new(&allocator, &source_text, source_type).parse();
    let elapsed_time = now.elapsed();
    println!("{}ms.", elapsed_time.as_millis());

    //println!("AST:");
    // println!("{}", serde_json::to_string_pretty(&ret.program).unwrap());

    for stmt in ret.program.body {
        match &value.meta {
            MixinMeta::Class {
                path,
                class_name,
                extends,
                export,
            } => {
                if *export {
                    if let Some(ModuleDeclaration::ExportNamedDeclaration(ex)) =
                        stmt.as_module_declaration()
                    {
                        if let Some(Declaration::ClassDeclaration(c)) = &ex.declaration {
                            if c.id.as_ref().unwrap().name == class_name
                                && c.super_class.as_ref().map(|sc| sc.is_specific_id(extends))
                                    != Some(false)
                            {
                                //println!("same!");
                                for i in c.body.body.iter() {
                                    for inject in value.inject.iter() {
                                        match i {
                                            ClassElement::MethodDefinition(m) => {
                                                // println!(
                                                //     "{}",
                                                //     m.key.is_specific_id(&inject.meta.method)
                                                // );
                                                if m.key.is_specific_id(
                                                    inject.meta.method.as_ref().unwrap(),
                                                ) {
                                                    for method_stmt in m
                                                        .value
                                                        .body
                                                        .as_ref()
                                                        .unwrap()
                                                        .statements
                                                        .iter()
                                                    {
                                                        if inject.meta.at.value == AtValue::Invoke {
                                                            // println!(
                                                            //     "{}",
                                                            //     inject.meta.at.target.clone()
                                                            // );
                                                            if let Some(call_expr) =
                                                                find_match_call_expr(
                                                                    method_stmt,
                                                                    inject.meta.at.target.clone(),
                                                                )
                                                            {
                                                                let func_body = inject
                                                                    .func
                                                                    .split_once('{')
                                                                    .unwrap()
                                                                    .1
                                                                    .rsplit_once('}')
                                                                    .unwrap()
                                                                    .0;
                                                                return_src.insert_str(
                                                                    call_expr.span.start as usize,
                                                                    &format!("/*@nora:inject:start*/{func_body}/*@nora:inject:end*/"),
                                                                );
                                                                //println!("{return_src}");
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            ClassElement::AccessorProperty(_) => (),
                                            _ => (),
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            MixinMeta::Function {
                path,
                func_name,
                export,
            } => {
                if *export {
                    if let Some(ModuleDeclaration::ExportNamedDeclaration(ex)) =
                        stmt.as_module_declaration()
                    {}
                } else {
                    if let Some(Declaration::FunctionDeclaration(func_decl)) = stmt.as_declaration()
                    {
                        if func_decl.id.as_ref().map(|id| id.name.to_string())
                            == Some(func_name.to_string())
                        {
                            for inject in value.inject.iter() {
                                //println!("{}", inject.meta.at.target);
                                for stmt in &func_decl.body.as_ref().unwrap().statements {
                                    if inject.meta.at.value == AtValue::Invoke {
                                        let ret = find_match_call_expr(
                                            stmt,
                                            inject.meta.at.target.clone(),
                                        );
                                        if let Some(call_expr) = ret {
                                            let func_body = inject
                                                .func
                                                .split_once('{')
                                                .unwrap()
                                                .1
                                                .rsplit_once('}')
                                                .unwrap()
                                                .0;
                                            return_src.insert_str(
                                            call_expr.span.start as usize,
                                            &format!("/*@nora:inject:start*/{func_body}/*@nora:inject:end*/"),
                                            );
                                            //println!("{return_src}");
                                            break;
                                        }
                                        // println!("{:?}", ret);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if ret.errors.is_empty() {
        println!("Parsed Successfully.");
    } else {
        for error in ret.errors {
            let error = error.with_source_code(source_text.clone());
            println!("{error:?}");
            println!("Parsed with Errors.");
        }
    }

    Ok(return_src)
}

// fn main() -> Result<(), String> {
//     // let name = env::args().nth(1).unwrap_or_else(|| "test.js".to_string());
//     // let path = Path::new(&name);

//     // let _ = std::fs::write(path, return_src);

//     // Ok(())
// }
