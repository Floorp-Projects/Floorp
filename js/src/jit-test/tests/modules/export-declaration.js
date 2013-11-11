load(libdir + "match.js");
load(libdir + "asserts.js");

var { Pattern, MatchError } = Match;

program = (elts) => Pattern({
    type: "Program",
    body: elts
})
exportDeclaration = (declaration, specifiers, source) => Pattern({
    type: "ExportDeclaration",
    declaration: declaration,
    specifiers: specifiers,
    source: source
});
exportSpecifier = (id, name) => Pattern({
    type: "ExportSpecifier",
    id: id,
    name: name
});
exportBatchSpecifier = () => Pattern({
    type: "ExportBatchSpecifier"
});
blockStatement = (body) => Pattern({
    type: "BlockStatement",
    body: body
});
functionDeclaration = (id, params, body) => Pattern({
    type: "FunctionDeclaration",
    id: id,
    params: params,
    defaults: [],
    body: body,
    rest: null,
    generator: false
});
variableDeclaration = (decls) => Pattern({
    type: "VariableDeclaration",
    kind: "var",
    declarations: decls
});
constDeclaration = (decls) => Pattern({
    type: "VariableDeclaration",
    kind: "const",
    declarations: decls
});
letDeclaration = (decls) => Pattern({
    type: "VariableDeclaration",
    kind: "let",
    declarations: decls
});
ident = (name) => Pattern({
    type: "Identifier",
    name: name
});
lit = (val) => Pattern({
    type: "Literal",
    value: val
});

program([
    exportDeclaration(
        null,
        [],
        null
    )
]).assert(Reflect.parse("export {}"));

program([
    exportDeclaration(
        null,
        [
            exportSpecifier(
                ident("a"),
                ident("a")
            )
        ],
        null
    )
]).assert(Reflect.parse("export { a }"));

program([
    exportDeclaration(
        null,
        [
            exportSpecifier(
                ident("a"),
                ident("b")
            )
        ],
        null
    )
]).assert(Reflect.parse("export { a as b }"));

program([
    exportDeclaration(
        null,
        [
            exportSpecifier(
                ident("as"),
                ident("as")
            )
        ],
        null
    )
]).assert(Reflect.parse("export { as as as }"));

program([
    exportDeclaration(
        null,
        [
            exportSpecifier(
                ident("a"),
                ident("true")
            )
        ],
        null
    )
]).assert(Reflect.parse("export { a as true }"));

program([
    exportDeclaration(
        null,
        [
            exportSpecifier(
                ident("a"),
                ident("a")
            ),
            exportSpecifier(
                ident("b"),
                ident("b")
            ),
        ],
        null
    )
]).assert(Reflect.parse("export { a, b }"));

program([
    exportDeclaration(
        null,
        [
            exportSpecifier(
                ident("a"),
                ident("b")
            ),
            exportSpecifier(
                ident("c"),
                ident("d")
            ),
        ],
        null
    )
]).assert(Reflect.parse("export { a as b, c as d }"));

program([
    exportDeclaration(
        null,
        [
            exportBatchSpecifier()
        ],
        null
    )
]).assert(Reflect.parse("export *"));

program([
    exportDeclaration(
        null,
        [
            exportBatchSpecifier()
        ],
        lit("a")
    )
]).assert(Reflect.parse("export * from 'a'"));

program([
    exportDeclaration(
        functionDeclaration(
            ident("f"),
            [],
            blockStatement([])
        ),
        null,
        null
    )
]).assert(Reflect.parse("export function f() {}"));

program([
    exportDeclaration(
        variableDeclaration([
            {
                id: ident("a"),
                init: lit(1)
            }, {
                id: ident("b"),
                init: lit(2)
            }
        ]),
        null,
        null
    )
]).assert(Reflect.parse("export var a = 1, b = 2;"));

program([
    exportDeclaration(
        constDeclaration([
            {
                id: ident("a"),
                init: lit(1)
            }, {
                id: ident("b"),
                init: lit(2)
            }
        ]),
        null,
        null
    )
]).assert(Reflect.parse("export const a = 1, b = 2;"));

// FIXME: In scripts, top level lets are converted back to vars. Fix this when
// we implement compiling scripts as modules.
program([
    exportDeclaration(
        variableDeclaration([
            {
                id: ident("a"),
                init: lit(1)
            }, {
                id: ident("b"),
                init: lit(2)
            }
        ]),
        null,
        null
    )
]).assert(Reflect.parse("export let a = 1, b = 2;"));

// NOTE: binding lists are treated as if they were let declarations by esprima,
// so we follow that convention.
program([
    exportDeclaration(
        variableDeclaration([
            {
                id: ident("a"),
                init: lit(1)
            }, {
                id: ident("b"),
                init: lit(2)
            }
        ]),
        null,
        null
    )
]).assert(Reflect.parse("export a = 1, b = 2;"));

var loc = Reflect.parse("export { a as b } from 'c'", {
    loc: true
}).body[0].loc;

assertEq(loc.start.line, 1);
assertEq(loc.start.column, 0);
assertEq(loc.start.line, 1);
assertEq(loc.end.column, 26);

assertThrowsInstanceOf(function () {
   Reflect.parse("function f() { export a }");
}, SyntaxError);

assertThrowsInstanceOf(function () {
   Reflect.parse("if (true) export a");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("export {");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("export {} from");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("export {,} from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("export { true as a } from 'b'");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    Reflect.parse("export { a } from 'b' f();");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    Reflect.parse("export * from 'b' f();");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("export {}\nfrom ()");
}, SyntaxError);
