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
]).assert(Reflect.parse("import a from 'b'"));

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
]).assert(Reflect.parse("import * as a from 'b'"));

program([
    importDeclaration(
        [],
        lit("a")
    )
]).assert(Reflect.parse("import {} from 'a'"));

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
]).assert(Reflect.parse("import { a } from 'b'"));

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
]).assert(Reflect.parse("import { a, } from 'b'"));

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
]).assert(Reflect.parse("import { a as b } from 'c'"));

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
]).assert(Reflect.parse("import { as as as } from 'a'"));

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
]).assert(Reflect.parse("import a, * as b from 'c'"));

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
]).assert(Reflect.parse("import d, {} from 'a'"));

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
]).assert(Reflect.parse("import d, { a } from 'b'"));

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
]).assert(Reflect.parse("import d, { a as b } from 'c'"));

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
]).assert(Reflect.parse("import d, { a, b } from 'c'"));

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
]).assert(Reflect.parse("import d, { a as b, c as d } from 'e'"));

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
]).assert(Reflect.parse("import { true as a } from 'b'"));

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
]).assert(Reflect.parse("import { a, b } from 'c'"));

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
]).assert(Reflect.parse("import { a as b, c as d } from 'e'"));

program([
    importDeclaration(
        [],
        lit("a")
    )
]).assert(Reflect.parse("import 'a'"));

var loc = Reflect.parse("import { a as b } from 'c'", {
    loc: true
}).body[0].loc;

assertEq(loc.start.line, 1);
assertEq(loc.start.column, 0);
assertEq(loc.start.line, 1);
assertEq(loc.end.column, 26);

assertThrowsInstanceOf(function () {
   Reflect.parse("function f() { import a from 'b' }");
}, SyntaxError);

assertThrowsInstanceOf(function () {
   Reflect.parse("if (true) import a from 'b'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import {");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import {}");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import {} from");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import {,} from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import { a as true } from 'b'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import { true } from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import a,");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import a, b from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import * as a,");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import * as a, {} from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import as a from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import * a from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import * as from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import , {} from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import d, from 'a'");
}, SyntaxError);

assertThrowsInstanceOf(function() {
    Reflect.parse("import * as true from 'b'");
}, SyntaxError);
