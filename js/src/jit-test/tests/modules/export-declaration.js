load(libdir + "match.js");
load(libdir + "asserts.js");
load(libdir + "class.js");

var { Pattern, MatchError } = Match;

program = (elts) => Pattern({
    type: "Program",
    body: elts
})
exportDeclaration = (declaration, specifiers, source, isDefault) => Pattern({
    type: "ExportDeclaration",
    declaration: declaration,
    specifiers: specifiers,
    source: source,
    isDefault: isDefault
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
classDeclaration = (name) => Pattern({
    type: "ClassStatement",
    name: name
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
        null,
        false
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
        null,
        false
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
        null,
        false
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
        null,
        false
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
        null,
        false
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
        null,
        false
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
        null,
        false
    )
]).assert(Reflect.parse("export { a as b, c as d }"));

program([
    exportDeclaration(
        null,
        [
            exportSpecifier(
                ident("a"),
                ident("a")
            )
        ],
        lit("b"),
        false
    )
]).assert(Reflect.parse("export { a } from 'b'"));

program([
    exportDeclaration(
        null,
        [
            exportBatchSpecifier()
        ],
        lit("a"),
        false
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
        null,
        false
    )
]).assert(Reflect.parse("export function f() {}"));

if (classesEnabled()) {
    program([
        exportDeclaration(
            classDeclaration(
                ident("Foo")
            ),
            null,
            null,
            false
        )
    ]).assert(Reflect.parse("export class Foo { constructor() {} }"));
}

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
        null,
        false
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
        null,
        false
    )
]).assert(Reflect.parse("export const a = 1, b = 2;"));

// FIXME: In scripts, top level lets are converted back to vars. Fix this when
// we implement compiling scripts as modules (bug 589199).
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
        null,
        false
    )
]).assert(Reflect.parse("export let a = 1, b = 2;"));

program([
    exportDeclaration(
        functionDeclaration(
            ident("*default*"),
            [],
            blockStatement([])
        ),
        null,
        null,
        true
    )
]).assert(Reflect.parse("export default function() {}"));

program([
    exportDeclaration(
        functionDeclaration(
            ident("foo"),
            [],
            blockStatement([])
        ),
        null,
        null,
        true
    )
]).assert(Reflect.parse("export default function foo() {}"));

if (classesEnabled()) {
    program([
        exportDeclaration(
            classDeclaration(
                ident("*default*")
            ),
            null,
            null,
            true
        )
    ]).assert(Reflect.parse("export default class { constructor() {} }"));

    program([
        exportDeclaration(
            classDeclaration(
                ident("Foo")
            ),
            null,
            null,
            true
        )
    ]).assert(Reflect.parse("export default class Foo { constructor() {} }"));
}

program([
    exportDeclaration(
        lit(1234),
        null,
        null,
        true
    )
]).assert(Reflect.parse("export default 1234"));

assertThrowsInstanceOf(function () {
   Reflect.parse("export default 1234 5678");
}, SyntaxError);

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
    Reflect.parse("export *");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    Reflect.parse("export * from 'b' f();");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("export {}\nfrom ()");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("function() {}");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("class() { constructor() {} }");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("export x");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("export foo = 5");
}, SyntaxError);
