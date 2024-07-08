use serde::Deserialize;
use swc_core::atoms::Atom;
use swc_core::common::DUMMY_SP;
use swc_core::ecma::ast::{
    Callee, Decl, ExprOrSpread, ExprStmt, Ident, Lit, MemberExpr, MemberProp, Pat, Stmt, Str,
    VarDecl, VarDeclKind,
};
use swc_core::ecma::{
    ast::{CallExpr, Expr, Program},
    transforms::testing::test_inline,
    visit::{as_folder, FoldWith, VisitMut},
};
use swc_core::plugin::{plugin_transform, proxies::TransformPluginProgramMetadata};
use swc_core::{quote, quote_expr};

enum NoraComparative {
    CallExpr(CallExpr),
    Callee(Callee),
    Expr(Expr),
    ExprOrSpread(ExprOrSpread),
    MemberProp(MemberProp),
    Ident(Ident),
    Atom(Atom),
    Lit(Lit),
    Stmt(Stmt),
    Decl(Decl),
    VarDecl(VarDecl),
    Pat(Pat),
    MemberExpr(MemberExpr),
}

impl PartialEq for NoraComparative {
    fn eq(&self, other: &Self) -> bool {
        match self {
            NoraComparative::CallExpr(call_expr) => {
                if let Self::CallExpr(other_call_expr) = other {
                    return Self::Callee(call_expr.callee.clone())
                        == Self::Callee(other_call_expr.callee.clone())
                        && call_expr.args.len() == other_call_expr.args.len()
                        && call_expr.args.iter().enumerate().all(|(idx, arg)| {
                            NoraComparative::ExprOrSpread(arg.clone())
                                == NoraComparative::ExprOrSpread(other_call_expr.args[idx].clone())
                        });
                }
                println!("callexpr");
                false
            }
            NoraComparative::Callee(callee) => {
                if let Self::Callee(other_callee) = other {
                    if let (Some(callee_expr), Some(other_callee_expr)) =
                        (callee.as_expr(), other_callee.as_expr())
                    {
                        return Self::Expr(*callee_expr.clone())
                            == Self::Expr(*other_callee_expr.clone());
                    }
                }
                println!("callee");
                false
            }
            NoraComparative::Expr(expr) => {
                if let Self::Expr(other_expr) = other {
                    if let (Some(ident), Some(other_ident)) =
                        (expr.as_ident(), other_expr.as_ident())
                    {
                        return Self::Ident(ident.clone()) == Self::Ident(other_ident.clone());
                    } else if let (Some(member_expr), Some(other_member_expr)) =
                        (expr.as_member(), other_expr.as_member())
                    {
                        return Self::MemberExpr(member_expr.clone())
                            == Self::MemberExpr(other_member_expr.clone());
                    } else if let (Some(expr_lit), Some(other_expr_lit)) =
                        (expr.as_lit(), other_expr.as_lit())
                    {
                        return Self::Lit(expr_lit.clone()) == Self::Lit(other_expr_lit.clone());
                    } else if let (Some(expr_call), Some(other_expr_call)) =
                        (expr.as_call(), other_expr.as_call())
                    {
                        return Self::CallExpr(expr_call.clone())
                            == Self::CallExpr(other_expr_call.clone());
                    }
                    unimplemented!()
                }
                println!("expr");
                println!("{:?}", expr);

                false
            }
            NoraComparative::ExprOrSpread(expr_or_spread) => {
                if let Self::ExprOrSpread(other_expr_or_spread) = other {
                    if expr_or_spread.spread.is_some() {
                        unimplemented!("expr_or_spread spread is uninplemented")
                    }
                    return Self::Expr(*expr_or_spread.expr.clone())
                        == Self::Expr(*other_expr_or_spread.expr.clone());
                }
                println!("expr_or_spread");
                false
            }
            NoraComparative::MemberProp(member_prop) => {
                if let Self::MemberProp(other_member_prop) = other {
                    if let (Some(member_prop_ident), Some(other_member_prop_ident)) =
                        (member_prop.as_ident(), other_member_prop.as_ident())
                    {
                        return Self::Ident(member_prop_ident.clone())
                            == Self::Ident(other_member_prop_ident.clone());
                    }
                    unimplemented!()
                }
                println!("member_prop");
                false
            }
            NoraComparative::Ident(ident) => {
                if let Self::Ident(other_ident) = other {
                    println!("{:?} ; {:?}", ident, other_ident);
                    return Self::Atom(ident.sym.clone()) == Self::Atom(ident.sym.clone());
                }
                println!("ident");
                false
            }
            NoraComparative::Lit(lit) => {
                if let Self::Lit(other_lit) = other {
                    if let (Lit::Str(lit_str), Lit::Str(other_lit)) = (lit, other_lit) {
                        return Self::Atom(lit_str.value.clone())
                            == Self::Atom(other_lit.value.clone());
                    } else if let (Lit::Bool(lit_bool), Lit::Bool(other_lit_bool)) =
                        (lit, other_lit)
                    {
                        return lit_bool.value == other_lit_bool.value;
                    }
                    println!("lit {:?} {:?}", lit, other_lit);
                }

                false
            }
            NoraComparative::Atom(atom) => {
                if let Self::Atom(other_atom) = other {
                    return atom.as_str() == other_atom.as_str();
                }
                false
            }
            NoraComparative::Stmt(stmt) => {
                if let Self::Stmt(other_stmt) = other {
                    if let (Some(stmt_expr), Some(other_stmt_expr)) =
                        (stmt.as_expr(), other_stmt.as_expr())
                    {
                        return Self::Expr(*stmt_expr.expr.clone())
                            == Self::Expr(*other_stmt_expr.expr.clone());
                    }
                    if let (Some(stmt_decl), Some(other_stmt_decl)) =
                        (stmt.as_decl(), other_stmt.as_decl())
                    {
                        return Self::Decl(stmt_decl.clone())
                            == Self::Decl(other_stmt_decl.clone());
                    } else {
                        unimplemented!("stmt {:?} {:?}", stmt, other_stmt)
                    }
                }
                false
            }
            NoraComparative::Decl(decl) => {
                if let Self::Decl(other_decl) = other {
                    if let (Some(var), Some(other_var)) = (decl.as_var(), other_decl.as_var()) {
                        return Self::VarDecl(*var.clone()) == Self::VarDecl(*other_var.clone());
                    }
                }
                false
            }
            NoraComparative::VarDecl(var_decl) => {
                if let Self::VarDecl(other_var_decl) = other {
                    return var_decl.kind == other_var_decl.kind
                        && var_decl.decls.iter().enumerate().all(|(idx, v)| {
                            Self::Pat(v.name.clone())
                                == Self::Pat(other_var_decl.decls[idx].name.clone())
                                && v.init.clone().is_none()
                                    == other_var_decl.decls[idx].init.is_none()
                                && if v.init.clone().is_some() {
                                    Self::Expr(*v.init.clone().unwrap())
                                        == Self::Expr(
                                            *other_var_decl.decls[idx].init.clone().unwrap(),
                                        )
                                } else {
                                    true
                                }
                        });
                }
                false
            }
            NoraComparative::Pat(pat) => {
                if let Self::Pat(other_pat) = other {
                    if let (Some(pat_ident), Some(other_pat_ident)) =
                        (pat.as_ident(), other_pat.as_ident())
                    {
                        println!("{:?} {:?}", pat_ident, other_pat_ident);
                        return Self::Atom(pat_ident.sym.clone())
                            == Self::Atom(other_pat_ident.sym.clone());
                    }
                }
                false
            }
            NoraComparative::MemberExpr(member_expr) => {
                if let Self::MemberExpr(other_member_expr) = other {
                    return Self::MemberProp(member_expr.prop.clone())
                        == Self::MemberProp(other_member_expr.prop.clone())
                        && Self::Expr(*member_expr.obj.clone())
                            == Self::Expr(*other_member_expr.obj.clone());
                }
                false
            }
        }
    }
}

pub struct TransformVisitor {
    path: String,
}

impl VisitMut for TransformVisitor {
    fn visit_mut_fn_decl(&mut self, n: &mut swc_core::ecma::ast::FnDecl) {
        if n.ident.sym.as_str() == "init_all" {
            let func_body = n.function.body.as_mut().unwrap();

            if self
                .path
                .ends_with("browser/chrome/browser/content/browser/preferences/preferences.js")
            {
                let aexpr = quote!(
                    "Services.telemetry.setEventRecordingEnabled('aboutpreferences', true);"
                        as Stmt
                );

                if let Some(idx) = func_body.stmts.iter().position(|stmt| {
                    NoraComparative::Stmt(stmt.clone()) == NoraComparative::Stmt(aexpr.clone())
                }) {
                    func_body.stmts.insert(
                        idx + 1,
                        quote!("register_module(\"paneCsk\", {init(){}});" as Stmt,),
                    );
                }
            }
        }
    }

    fn visit_mut_var_decl(&mut self, n: &mut swc_core::ecma::ast::VarDecl) {
        if (self
            .path
            .ends_with("browser/modules/sessionstore/TabState.sys.mjs"))
            && n.kind == VarDeclKind::Var
            && n.decls.len() == 1
            && n.decls[0].name.as_ident().unwrap().sym.as_str() == "TabStateInternal"
        {
            if let Some(obj) = n.decls[0].init.as_mut().unwrap().as_mut_object() {
                for prop_or_spread in obj.props.iter_mut() {
                    if let Some(Some(prop_method)) = prop_or_spread
                        .as_mut_prop()
                        .map(|prop| prop.as_mut_method())
                    {
                        if let Some(prop_method_ident) = prop_method.key.as_ident() {
                            if prop_method_ident.sym.as_str() == "_collectBaseTabData" {
                                if let Some(body) = prop_method.function.body.as_mut() {
                                    let astmt = quote!("let browser = tab.linkedBrowser;" as Stmt);
                                    if let Some(idx) = body.stmts.iter().position(|x| {
                                        NoraComparative::Stmt(x.clone())
                                            == NoraComparative::Stmt(astmt.clone())
                                    }) {
                                        body.stmts.insert(idx+1, quote!("tabData.floorpDisableHistory = tab.getAttribute(\"floorp-disablehistory\");" as Stmt))
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    fn visit_mut_block_stmt(&mut self, n: &mut swc_core::ecma::ast::BlockStmt) {
        for stmt in n.stmts.as_mut_slice() {
            if let Some(Some(assign)) = stmt
                .as_mut_expr()
                .map(|expr_stmt| expr_stmt.expr.as_mut_assign())
            {
                if let Some(Some(member)) = assign
                    .left
                    .as_mut_simple()
                    .map(|simple| simple.as_mut_member())
                {
                    if self.path.ends_with(
                        "browser/chrome/browser/content/browser/tabbrowser/tabbrowser.js",
                    ) {
                        let aexpr = quote!("window._gBrowser" as Expr);
                        let amember_expr = aexpr.as_member().unwrap();
                        if NoraComparative::MemberExpr(member.clone())
                            == NoraComparative::MemberExpr(amember_expr.clone())
                        {
                            if let Some(obj) = assign.right.as_mut_object() {
                                for prop in obj.props.iter_mut() {
                                    if let Some(Some(method)) =
                                        prop.as_mut_prop().map(|p| p.as_mut_method())
                                    {
                                        if method
                                            .key
                                            .as_ident()
                                            .map(|i| {
                                                i.sym.as_str() == "createTabsForSessionRestore"
                                            })
                                            .is_some()
                                        {
                                            let body = method.function.body.as_mut().unwrap();
                                            if let Some(idx) = body.stmts.iter().position(|stmt| {
                                                NoraComparative::Stmt(stmt.clone())
                                                    == NoraComparative::Stmt(quote!(
                                                        "let shouldUpdateForPinnedTabs = false"
                                                            as Stmt
                                                    ))
                                            }) {
                                                body.stmts.insert(idx+1, quote!("for (var i = 0; i < tabDataList.length; i++) {const tabData = tabDataList[i];if (tabData.floorpDisableHistory) {tabDataList.splice(i, 1);}}" as Stmt))
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct NoranekoHMRTransformerConfig {
    path: String,
}

/// An example plugin function with macro support.
/// `plugin_transform` macro interop pointers into deserialized structs, as well
/// as returning ptr back to host.
///
/// It is possible to opt out from macro by writing transform fn manually
/// if plugin need to handle low-level ptr directly via
/// `__transform_plugin_process_impl(
///     ast_ptr: *const u8, ast_ptr_len: i32,
///     unresolved_mark: u32, should_enable_comments_proxy: i32) ->
///     i32 /*  0 for success, fail otherwise.
///             Note this is only for internal pointer interop result,
///             not actual transform result */`
///
/// This requires manual handling of serialization / deserialization from ptrs.
/// Refer swc_plugin_macro to see how does it work internally.
#[plugin_transform]
pub fn process_transform(program: Program, data: TransformPluginProgramMetadata) -> Program {
    let config = serde_json::from_str::<NoranekoHMRTransformerConfig>(
        &data
            .get_transform_plugin_config()
            .expect("failed to get plugin config for noraneko-hmr-transformer"),
    )
    .expect("invalid config for enoraneko-hmr-transformermotion");
    program.fold_with(&mut as_folder(TransformVisitor { path: config.path }))
}

//? https://github.com/swc-project/swc/issues/356

// An example to test plugin transform.
// Recommended strategy to test plugin's transform is verify
// the Visitor's behavior, instead of trying to run `process_transform` with mocks
// unless explicitly required to do so.
test_inline!(
    Default::default(),
    |_| as_folder(TransformVisitor {
        path: "browser/chrome/browser/content/browser/preferences/preferences.js".to_string()
    }),
    function_inject,
    // Input codes
    r#"
    function init_all(){
        Services.telemetry.setEventRecordingEnabled('aboutpreferences', true);
    }"#,
    // Output codes after transformed with plugin
    r#"function init_all(){
        Services.telemetry.setEventRecordingEnabled('aboutpreferences', true);
        register_module("paneCsk", {init(){}});
    }"#
);

test_inline!(
    Default::default(),
    |_| as_folder(TransformVisitor {
        path: "browser/modules/sessionstore/TabState.sys.mjs".to_string()
    }),
    var_object_method_inject,
    // Input codes
    r#"var TabStateInternal = {
      _collectBaseTabData(tab, options) {
        let browser = tab.linkedBrowser;
      }
    }"#,
    // Output codes after transformed with plugin
    r#"var TabStateInternal = {
      _collectBaseTabData(tab, options) {
        let browser = tab.linkedBrowser;
        tabData.floorpDisableHistory = tab.getAttribute("floorp-disablehistory");
      }
    }"#
);

test_inline!(
    Default::default(),
    |_| as_folder(TransformVisitor {
        path: "browser/chrome/browser/content/browser/tabbrowser/tabbrowser.js".to_string()
    }),
    obj_var_obj_method,
    // Input codes
    r#"{
      window._gBrowser = {
        createTabsForSessionRestore(restoreTabsLazily, selectTab, tabDataList) {
          let shouldUpdateForPinnedTabs = false;
        }
      };
    }"#,
    // Output codes after transformed with plugin
    r#"{
      window._gBrowser = {
        createTabsForSessionRestore(restoreTabsLazily, selectTab, tabDataList) {
          let shouldUpdateForPinnedTabs = false;
          for (var i = 0; i < tabDataList.length; i++) {
            const tabData = tabDataList[i];
            if (tabData.floorpDisableHistory) {
                tabDataList.splice(i, 1);
            }
          }
        }
      };
    }"#
);
