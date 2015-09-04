load(libdir + "match.js");
load(libdir + "asserts.js");

var { Pattern, MatchError } = Match;

program = (elts) => Pattern({
    type: "Program",
    body: elts
})
importDeclaration = (specifiers, source) => Pattern({
    type: "ImportDeclaration",
    specifiers: specifiers,
    source: source
});
importSpecifier = (id, name) => Pattern({
    type: "ImportSpecifier",
    id: id,
    name: name
});
ident = (name) => Pattern({
    type: "Identifier",
    name: name
})
lit = (val) => Pattern({
    type: "Literal",
    value: val
})

function parseAsModule(source)
{
    return Reflect.parse(source, {target: "module"});
}

program([
    importDeclaration(
        [
            importSpecifier(
                ident("default"),
                ident("a")
            )
        ],
        lit("b")
    )
]).assert(parseAsModule("import a from 'b'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("*"),
                ident("a")
            )
        ],
        lit("b")
    )
]).assert(parseAsModule("import * as a from 'b'"));

program([
    importDeclaration(
        [],
        lit("a")
    )
]).assert(parseAsModule("import {} from 'a'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("a"),
                ident("a")
            )
        ],
        lit("b")
    )
]).assert(parseAsModule("import { a } from 'b'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("a"),
                ident("a")
            )
        ],
        lit("b")
    )
]).assert(parseAsModule("import { a, } from 'b'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("a"),
                ident("b")
            )
        ],
        lit("c")
    )
]).assert(parseAsModule("import { a as b } from 'c'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("as"),
                ident("as")
            )
        ],
        lit("a")
    )
]).assert(parseAsModule("import { as as as } from 'a'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("default"),
                ident("a")
            ),
            importSpecifier(
                ident("*"),
                ident("b")
            )
        ],
        lit("c")
    )
]).assert(parseAsModule("import a, * as b from 'c'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("default"),
                ident("d")
            )
        ],
        lit("a")
    )
]).assert(parseAsModule("import d, {} from 'a'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("default"),
                ident("d")
            ),
            importSpecifier(
                ident("a"),
                ident("a")
            )
        ],
        lit("b")
    )
]).assert(parseAsModule("import d, { a } from 'b'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("default"),
                ident("d")
            ),
            importSpecifier(
                ident("a"),
                ident("b")
            )
        ],
        lit("c")
    )
]).assert(parseAsModule("import d, { a as b } from 'c'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("default"),
                ident("d")
            ),
            importSpecifier(
                ident("a"),
                ident("a")
            ),
            importSpecifier(
                ident("b"),
                ident("b")
            ),
        ],
        lit("c")
    )
]).assert(parseAsModule("import d, { a, b } from 'c'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("default"),
                ident("d")
            ),
            importSpecifier(
                ident("a"),
                ident("b")
            ),
            importSpecifier(
                ident("c"),
                ident("d")
            ),
        ],
        lit("e")
    )
]).assert(parseAsModule("import d, { a as b, c as d } from 'e'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("true"),
                ident("a")
            )
        ],
        lit("b")
    )
]).assert(parseAsModule("import { true as a } from 'b'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("a"),
                ident("a")
            ),
            importSpecifier(
                ident("b"),
                ident("b")
            ),
        ],
        lit("c")
    )
]).assert(parseAsModule("import { a, b } from 'c'"));

program([
    importDeclaration(
        [
            importSpecifier(
                ident("a"),
                ident("b")
            ),
            importSpecifier(
                ident("c"),
                ident("d")
            ),
        ],
        lit("e")
    )
]).assert(parseAsModule("import { a as b, c as d } from 'e'"));

program([
    importDeclaration(
        [],
        lit("a")
    )
]).assert(parseAsModule("import 'a'"));

var loc = parseAsModule("import { a as b } from 'c'", {
    loc: true
}).body[0].loc;

assertEq(loc.start.line, 1);
assertEq(loc.start.column, 0);
assertEq(loc.start.line, 1);
assertEq(loc.end.column, 26);

assertThrowsInstanceOf(function () {
   parseAsModule("function f() { import a from 'b' }");
}, SyntaxError);

assertThrowsInstanceOf(function () {
   parseAsModule("if (true) import a from 'b'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import {");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import {}");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import {} from");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import {,} from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import { a as true } from 'b'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import { true } from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import a,");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import a, b from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import * as a,");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import * as a, {} from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import as a from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import * a from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import * as from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import , {} from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import d, from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    parseAsModule("import * as true from 'b'");
}, SyntaxError);
