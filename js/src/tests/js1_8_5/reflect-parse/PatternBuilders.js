// |reftest| skip

loadRelativeToScript('Match.js');

var { Pattern, MatchError } = Match;

function program(elts) Pattern({ type: "Program", body: elts })
function exprStmt(expr) Pattern({ type: "ExpressionStatement", expression: expr })
function throwStmt(expr) Pattern({ type: "ThrowStatement", argument: expr })
function returnStmt(expr) Pattern({ type: "ReturnStatement", argument: expr })
function yieldExpr(expr) Pattern({ type: "YieldExpression", argument: expr })
function lit(val) Pattern({ type: "Literal", value: val })
function comp(name) Pattern({ type: "ComputedName", name: name })
function spread(val) Pattern({ type: "SpreadExpression", expression: val})
var thisExpr = Pattern({ type: "ThisExpression" });
function funDecl(id, params, body, defaults=[], rest=null) Pattern(
    { type: "FunctionDeclaration",
      id: id,
      params: params,
      defaults: defaults,
      body: body,
      rest: rest,
      generator: false })
function genFunDecl(id, params, body) Pattern({ type: "FunctionDeclaration",
                                                id: id,
                                                params: params,
                                                defaults: [],
                                                body: body,
                                                generator: true })
function varDecl(decls) Pattern({ type: "VariableDeclaration", declarations: decls, kind: "var" })
function letDecl(decls) Pattern({ type: "VariableDeclaration", declarations: decls, kind: "let" })
function constDecl(decls) Pattern({ type: "VariableDeclaration", declarations: decls, kind: "const" })
function ident(name) Pattern({ type: "Identifier", name: name })
function dotExpr(obj, id) Pattern({ type: "MemberExpression", computed: false, object: obj, property: id })
function memExpr(obj, id) Pattern({ type: "MemberExpression", computed: true, object: obj, property: id })
function forStmt(init, test, update, body) Pattern({ type: "ForStatement", init: init, test: test, update: update, body: body })
function forOfStmt(lhs, rhs, body) Pattern({ type: "ForOfStatement", left: lhs, right: rhs, body: body })
function forInStmt(lhs, rhs, body) Pattern({ type: "ForInStatement", left: lhs, right: rhs, body: body, each: false })
function forEachInStmt(lhs, rhs, body) Pattern({ type: "ForInStatement", left: lhs, right: rhs, body: body, each: true })
function breakStmt(lab) Pattern({ type: "BreakStatement", label: lab })
function continueStmt(lab) Pattern({ type: "ContinueStatement", label: lab })
function blockStmt(body) Pattern({ type: "BlockStatement", body: body })
function literal(val) Pattern({ type: "Literal",  value: val })
var emptyStmt = Pattern({ type: "EmptyStatement" })
function ifStmt(test, cons, alt) Pattern({ type: "IfStatement", test: test, alternate: alt, consequent: cons })
function labStmt(lab, stmt) Pattern({ type: "LabeledStatement", label: lab, body: stmt })
function withStmt(obj, stmt) Pattern({ type: "WithStatement", object: obj, body: stmt })
function whileStmt(test, stmt) Pattern({ type: "WhileStatement", test: test, body: stmt })
function doStmt(stmt, test) Pattern({ type: "DoWhileStatement", test: test, body: stmt })
function switchStmt(disc, cases) Pattern({ type: "SwitchStatement", discriminant: disc, cases: cases })
function caseClause(test, stmts) Pattern({ type: "SwitchCase", test: test, consequent: stmts })
function defaultClause(stmts) Pattern({ type: "SwitchCase", test: null, consequent: stmts })
function catchClause(id, guard, body) Pattern({ type: "CatchClause", param: id, guard: guard, body: body })
function tryStmt(body, guarded, unguarded, fin) Pattern({ type: "TryStatement", block: body, guardedHandlers: guarded, handler: unguarded, finalizer: fin })
function letStmt(head, body) Pattern({ type: "LetStatement", head: head, body: body })

function classStmt(id, heritage, body) Pattern({ type: "ClassStatement",
                                                 name: id,
                                                 heritage: heritage,
                                                 body: body})
function classExpr(id, heritage, body) Pattern({ type: "ClassExpression",
                                                 name: id,
                                                 heritage: heritage,
                                                 body: body})
function classMethod(id, body, kind, static) Pattern({ type: "ClassMethod",
                                                       name: id,
                                                       body: body,
                                                       kind: kind,
                                                       static: static })


function funExpr(id, args, body, gen) Pattern({ type: "FunctionExpression",
                                                id: id,
                                                params: args,
                                                body: body,
                                                generator: false })
function genFunExpr(id, args, body) Pattern({ type: "FunctionExpression",
                                              id: id,
                                              params: args,
                                              body: body,
                                              generator: true })
function arrowExpr(args, body) Pattern({ type: "ArrowFunctionExpression",
                                         params: args,
                                         body: body })

function unExpr(op, arg) Pattern({ type: "UnaryExpression", operator: op, argument: arg })
function binExpr(op, left, right) Pattern({ type: "BinaryExpression", operator: op, left: left, right: right })
function aExpr(op, left, right) Pattern({ type: "AssignmentExpression", operator: op, left: left, right: right })
function updExpr(op, arg, prefix) Pattern({ type: "UpdateExpression", operator: op, argument: arg, prefix: prefix })
function logExpr(op, left, right) Pattern({ type: "LogicalExpression", operator: op, left: left, right: right })

function condExpr(test, cons, alt) Pattern({ type: "ConditionalExpression", test: test, consequent: cons, alternate: alt })
function seqExpr(exprs) Pattern({ type: "SequenceExpression", expressions: exprs })
function newExpr(callee, args) Pattern({ type: "NewExpression", callee: callee, arguments: args })
function callExpr(callee, args) Pattern({ type: "CallExpression", callee: callee, arguments: args })
function arrExpr(elts) Pattern({ type: "ArrayExpression", elements: elts })
function objExpr(elts) Pattern({ type: "ObjectExpression", properties: elts })
function computedName(elts) Pattern({ type: "ComputedName", name: elts })
function templateLit(elts) Pattern({ type: "TemplateLiteral", elements: elts })
function taggedTemplate(tagPart, templatePart) Pattern({ type: "TaggedTemplate", callee: tagPart,
                arguments : templatePart })
function template(raw, cooked, ...args) Pattern([{ type: "CallSiteObject", raw: raw, cooked:
cooked}, ...args])
function compExpr(body, blocks, filter, style) {
  if (style == "legacy" || !filter)
    return Pattern({ type: "ComprehensionExpression", body, blocks, filter, style });
  else
    return Pattern({ type: "ComprehensionExpression", body, blocks: blocks.concat(compIf(filter)), filter: null, style });
}
function genExpr(body, blocks, filter, style) {
  if (style == "legacy" || !filter)
    return Pattern({ type: "GeneratorExpression", body, blocks, filter, style });
  else
    return Pattern({ type: "GeneratorExpression", body, blocks: blocks.concat(compIf(filter)), filter: null, style });
}
function graphExpr(idx, body) Pattern({ type: "GraphExpression", index: idx, expression: body })
function letExpr(head, body) Pattern({ type: "LetExpression", head: head, body: body })
function idxExpr(idx) Pattern({ type: "GraphIndexExpression", index: idx })

function compBlock(left, right) Pattern({ type: "ComprehensionBlock", left: left, right: right, each: false, of: false })
function compEachBlock(left, right) Pattern({ type: "ComprehensionBlock", left: left, right: right, each: true, of: false })
function compOfBlock(left, right) Pattern({ type: "ComprehensionBlock", left: left, right: right, each: false, of: true })
function compIf(test) Pattern({ type: "ComprehensionIf", test: test })

function arrPatt(elts) Pattern({ type: "ArrayPattern", elements: elts })
function objPatt(elts) Pattern({ type: "ObjectPattern", properties: elts })

function assignElem(target, defaultExpr = null, targetIdent = typeof target == 'string' ? ident(target) : target) defaultExpr ? aExpr('=', targetIdent, defaultExpr) : targetIdent
function assignProp(property, target, defaultExpr = null, shorthand = !target, targetProp = target || ident(property)) Pattern({
    type: "Property", key: ident(property), shorthand,
    value: defaultExpr ? aExpr('=', targetProp, defaultExpr) : targetProp })
