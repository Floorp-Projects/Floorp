load(libdir + "match.js");
load(libdir + "asserts.js");

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
    id: name
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

function parseAsModule(source)
{
    return Reflect.parse(source, {target: "module"});
}

program([
    exportDeclaration(
        null,
        [],
        null,
        false
    )
]).assert(parseAsModule("export {}"));

program([
    letDeclaration([
        {
            id: ident("a"),
            init: lit(1)
        }
    ]),
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
]).assert(parseAsModule("let a = 1; export { a }"));

program([
    letDeclaration([
        {
            id: ident("a"),
            init: lit(1)
        }
    ]),
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
]).assert(parseAsModule("let a = 1; export { a as b }"));

program([
    letDeclaration([
        {
            id: ident("as"),
            init: lit(1)
        }
    ]),
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
]).assert(parseAsModule("let as = 1; export { as as as }"));

program([
    letDeclaration([
        {
            id: ident("a"),
            init: lit(1)
        }
    ]),
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
]).assert(parseAsModule("let a = 1; export { a as true }"));

program([
    letDeclaration([
        {
            id: ident("a"),
            init: lit(1)
        },
        {
            id: ident("b"),
            init: lit(2)
        }
    ]),
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
]).assert(parseAsModule("let a = 1, b = 2; export { a, b }"));

program([
    letDeclaration([
        {
            id: ident("a"),
            init: lit(1)
        },
        {
            id: ident("c"),
            init: lit(2)
        }
    ]),
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
]).assert(parseAsModule("let a = 1, c = 2; export { a as b, c as d }"));

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
]).assert(parseAsModule("export { a } from 'b'"));

program([
    exportDeclaration(
        null,
        [
            exportBatchSpecifier()
        ],
        lit("a"),
        false
    )
]).assert(parseAsModule("export * from 'a'"));

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
]).assert(parseAsModule("export function f() {}"));

program([
    exportDeclaration(
        classDeclaration(
            ident("Foo")
        ),
        null,
        null,
        false
    )
]).assert(parseAsModule("export class Foo { constructor() {} }"));

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
]).assert(parseAsModule("export var a = 1, b = 2;"));

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
]).assert(parseAsModule("export const a = 1, b = 2;"));

program([
    exportDeclaration(
        letDeclaration([
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
]).assert(parseAsModule("export let a = 1, b = 2;"));

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
]).assert(parseAsModule("export default function() {}"));

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
]).assert(parseAsModule("export default function foo() {}"));

program([
    exportDeclaration(
        classDeclaration(
            ident("*default*")
        ),
        null,
        null,
        true
    )
]).assert(parseAsModule("export default class { constructor() {} }"));

program([
    exportDeclaration(
        classDeclaration(
            ident("Foo")
        ),
        null,
        null,
        true
    )
]).assert(parseAsModule("export default class Foo { constructor() {} }"));

program([
    exportDeclaration(
        lit(1234),
        null,
        null,
        true
    )
]).assert(parseAsModule("export default 1234"));

assertThrowsInstanceOf(function () {
   parseAsModule("export default 1234 5678");
}, SyntaxError);

var loc = parseAsModule("export { a as b } from 'c'", {
    loc: true
}).body[0].loc;

assertEq(loc.start.line, 1);
assertEq(loc.start.column, 0);
assertEq(loc.start.line, 1);
assertEq(loc.end.column, 26);

assertThrowsInstanceOf(function () {
   parseAsModule("function f() { export a }");
}, SyntaxError);

assertThrowsInstanceOf(function () {
   parseAsModule("if (true) export a");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("export {");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("export {} from");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("export {,} from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("export { true as a } from 'b'");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    parseAsModule("export { a } from 'b' f();");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    parseAsModule("export *");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    parseAsModule("export * from 'b' f();");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("export {}\nfrom ()");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("function() {}");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("class() { constructor() {} }");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("export x");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("export foo = 5");
}, SyntaxError);
