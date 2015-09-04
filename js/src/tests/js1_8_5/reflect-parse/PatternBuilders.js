// |reftest| skip

loadRelativeToScript('Match.js');

var { Pattern, MatchError } = Match;

function program(elts) {
    return Pattern({ type: "Program", body: elts });
}
function exprStmt(expr) {
    return Pattern({ type: "ExpressionStatement", expression: expr });
}
function throwStmt(expr) {
    return Pattern({ type: "ThrowStatement", argument: expr });
}
function returnStmt(expr) {
    return Pattern({ type: "ReturnStatement", argument: expr });
}
function yieldExpr(expr) {
    return Pattern({ type: "YieldExpression", argument: expr });
}
function lit(val) {
    return Pattern({ type: "Literal", value: val });
}
function comp(name) {
    return Pattern({ type: "ComputedName", name: name });
}
function spread(val) {
    return Pattern({ type: "SpreadExpression", expression: val});
}
var thisExpr = Pattern({ type: "ThisExpression" });
function funDecl(id, params, body, defaults=[], rest=null) {
    return Pattern({ type: "FunctionDeclaration",
                     id: id,
                     params: params,
                     defaults: defaults,
                     body: body,
                     rest: rest,
                     generator: false });
}
function genFunDecl(id, params, body) {
    return Pattern({ type: "FunctionDeclaration",
                     id: id,
                     params: params,
                     defaults: [],
                     body: body,
                     generator: true });
}
function varDecl(decls) {
    return Pattern({ type: "VariableDeclaration", declarations: decls, kind: "var" });
}
function letDecl(decls) {
    return Pattern({ type: "VariableDeclaration", declarations: decls, kind: "let" });
}
function constDecl(decls) {
    return Pattern({ type: "VariableDeclaration", declarations: decls, kind: "const" });
}
function ident(name) {
    return Pattern({ type: "Identifier", name: name });
}
function dotExpr(obj, id) {
    return Pattern({ type: "MemberExpression", computed: false, object: obj, property: id });
}
function memExpr(obj, id) {
    return Pattern({ type: "MemberExpression", computed: true, object: obj, property: id });
}
function forStmt(init, test, update, body) {
    return Pattern({ type: "ForStatement", init: init, test: test, update: update, body: body });
}
function forOfStmt(lhs, rhs, body) {
    return Pattern({ type: "ForOfStatement", left: lhs, right: rhs, body: body });
}
function forInStmt(lhs, rhs, body) {
    return Pattern({ type: "ForInStatement", left: lhs, right: rhs, body: body, each: false });
}
function forEachInStmt(lhs, rhs, body) {
    return Pattern({ type: "ForInStatement", left: lhs, right: rhs, body: body, each: true });
}
function breakStmt(lab) {
    return Pattern({ type: "BreakStatement", label: lab });
}
function continueStmt(lab) {
    return Pattern({ type: "ContinueStatement", label: lab });
}
function blockStmt(body) {
    return Pattern({ type: "BlockStatement", body: body });
}
function literal(val) {
    return Pattern({ type: "Literal",  value: val });
}
var emptyStmt = Pattern({ type: "EmptyStatement" });
function ifStmt(test, cons, alt) {
    return Pattern({ type: "IfStatement", test: test, alternate: alt, consequent: cons });
}
function labStmt(lab, stmt) {
    return Pattern({ type: "LabeledStatement", label: lab, body: stmt });
}
function withStmt(obj, stmt) {
    return Pattern({ type: "WithStatement", object: obj, body: stmt });
}
function whileStmt(test, stmt) {
    return Pattern({ type: "WhileStatement", test: test, body: stmt });
}
function doStmt(stmt, test) {
    return Pattern({ type: "DoWhileStatement", test: test, body: stmt });
}
function switchStmt(disc, cases) {
    return Pattern({ type: "SwitchStatement", discriminant: disc, cases: cases });
}
function caseClause(test, stmts) {
    return Pattern({ type: "SwitchCase", test: test, consequent: stmts });
}
function defaultClause(stmts) {
    return Pattern({ type: "SwitchCase", test: null, consequent: stmts });
}
function catchClause(id, guard, body) {
    return Pattern({ type: "CatchClause", param: id, guard: guard, body: body });
}
function tryStmt(body, guarded, unguarded, fin) {
    return Pattern({ type: "TryStatement", block: body, guardedHandlers: guarded, handler: unguarded, finalizer: fin });
}
function letStmt(head, body) {
    return Pattern({ type: "LetStatement", head: head, body: body });
}

function superProp(id) {
    return dotExpr(Pattern({ type: "Super" }), id);
}
function superElem(id) {
    return memExpr(Pattern({ type: "Super" }), id);
}

function classStmt(id, heritage, body) {
    return Pattern({ type: "ClassStatement",
                     id: id,
                     superClass: heritage,
                     body: body});
}
function classExpr(id, heritage, body) {
    return Pattern({ type: "ClassExpression",
                     id: id,
                     superClass: heritage,
                     body: body});
}
function classMethod(id, body, kind, static) {
    return Pattern({ type: "ClassMethod",
                     name: id,
                     body: body,
                     kind: kind,
                     static: static });
}

function funExpr(id, args, body, gen) {
    return Pattern({ type: "FunctionExpression",
                     id: id,
                     params: args,
                     body: body,
                     generator: false });
}
function genFunExpr(id, args, body) {
    return Pattern({ type: "FunctionExpression",
                     id: id,
                     params: args,
                     body: body,
                     generator: true });
}
function arrowExpr(args, body) {
    return Pattern({ type: "ArrowFunctionExpression",
                     params: args,
                     body: body });
}

function metaProperty(meta, property) {
    return Pattern({ type: "MetaProperty",
                     meta: meta,
                     property: property });
}
function newTarget() {
    return metaProperty(ident("new"), ident("target"));
}

function unExpr(op, arg) {
    return Pattern({ type: "UnaryExpression", operator: op, argument: arg });
}
function binExpr(op, left, right) {
    return Pattern({ type: "BinaryExpression", operator: op, left: left, right: right });
}
function aExpr(op, left, right) {
    return Pattern({ type: "AssignmentExpression", operator: op, left: left, right: right });
}
function updExpr(op, arg, prefix) {
    return Pattern({ type: "UpdateExpression", operator: op, argument: arg, prefix: prefix });
}
function logExpr(op, left, right) {
    return Pattern({ type: "LogicalExpression", operator: op, left: left, right: right });
}

function condExpr(test, cons, alt) {
    return Pattern({ type: "ConditionalExpression", test: test, consequent: cons, alternate: alt });
}
function seqExpr(exprs) {
    return Pattern({ type: "SequenceExpression", expressions: exprs });
}
function newExpr(callee, args) {
    return Pattern({ type: "NewExpression", callee: callee, arguments: args });
}
function callExpr(callee, args) {
    return Pattern({ type: "CallExpression", callee: callee, arguments: args });
}
function arrExpr(elts) {
    return Pattern({ type: "ArrayExpression", elements: elts });
}
function objExpr(elts) {
    return Pattern({ type: "ObjectExpression", properties: elts });
}
function computedName(elts) {
    return Pattern({ type: "ComputedName", name: elts });
}
function templateLit(elts) {
    return Pattern({ type: "TemplateLiteral", elements: elts });
}
function taggedTemplate(tagPart, templatePart) {
    return Pattern({ type: "TaggedTemplate", callee: tagPart, arguments : templatePart });
}
function template(raw, cooked, ...args) {
    return Pattern([{ type: "CallSiteObject", raw: raw, cooked: cooked}, ...args]);
}

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
function graphExpr(idx, body) {
    return Pattern({ type: "GraphExpression", index: idx, expression: body });
}
function idxExpr(idx) {
    return Pattern({ type: "GraphIndexExpression", index: idx });
}

function compBlock(left, right) {
    return Pattern({ type: "ComprehensionBlock", left: left, right: right, each: false, of: false });
}
function compEachBlock(left, right) {
    return Pattern({ type: "ComprehensionBlock", left: left, right: right, each: true, of: false });
}
function compOfBlock(left, right) {
    return Pattern({ type: "ComprehensionBlock", left: left, right: right, each: false, of: true });
}
function compIf(test) {
    return Pattern({ type: "ComprehensionIf", test: test });
}

function arrPatt(elts) {
    return Pattern({ type: "ArrayPattern", elements: elts });
}
function objPatt(elts) {
    return Pattern({ type: "ObjectPattern", properties: elts });
}

function assignElem(target, defaultExpr = null, targetIdent = typeof target == 'string' ? ident(target) : target) {
    return defaultExpr ? aExpr('=', targetIdent, defaultExpr) : targetIdent;
}
function assignProp(property, target, defaultExpr = null, shorthand = !target, targetProp = target || ident(property)) {
    return Pattern({
        type: "Property", key: ident(property), shorthand,
        value: defaultExpr ? aExpr('=', targetProp, defaultExpr) : targetProp });
}
