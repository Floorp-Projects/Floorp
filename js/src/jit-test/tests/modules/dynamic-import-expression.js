load(libdir + "match.js");
load(libdir + "asserts.js");

var { Pattern, MatchError } = Match;

program = (elts) => Pattern({
    type: "Program",
    body: elts
});
expressionStatement = (expression) => Pattern({
    type: "ExpressionStatement",
    expression: expression
});
assignmentExpression = (left, operator, right) => Pattern({
    type: "AssignmentExpression",
    operator: operator,
    left: left,
    right: right
});
ident = (name) => Pattern({
    type: "Identifier",
    name: name
});
importCall = (ident, singleArg) => Pattern({
    type: "CallImport",
    ident: ident,
    arg: singleArg
});

function parseAsClassicScript(source)
{
    return Reflect.parse(source);
}

function parseAsModuleScript(source)
{
    return Reflect.parse(source, {target: "module"});
}

for (let parse of [parseAsModuleScript, parseAsClassicScript]) {
    program([
        expressionStatement(
            importCall(
                ident("import"),
                ident("foo")
            )
        )
    ]).assert(parse("import(foo);"));

    program([
        expressionStatement(
            assignmentExpression(
                ident("x"),
                "=",
                importCall(
                    ident("import"),
                    ident("foo")
                )
            )
        )
    ]).assert(parse("x = import(foo);"));
}

function assertParseThrowsSyntaxError(source)
{
    assertThrowsInstanceOf(() => parseAsClassicScript(source), SyntaxError);
    assertThrowsInstanceOf(() => parseAsModuleScript(source), SyntaxError);
}

assertParseThrowsSyntaxError("import");
assertParseThrowsSyntaxError("import(");
assertParseThrowsSyntaxError("import(1,");
assertParseThrowsSyntaxError("import(1, 2");
assertParseThrowsSyntaxError("import(1, 2)");
assertParseThrowsSyntaxError("x = import");
assertParseThrowsSyntaxError("x = import(");
assertParseThrowsSyntaxError("x = import(1,");
assertParseThrowsSyntaxError("x = import(1, 2");
assertParseThrowsSyntaxError("x = import(1, 2)");

// import() is not implemented.
assertThrowsInstanceOf(() => eval("import('foo')"),
                       SyntaxError);
assertThrowsInstanceOf(() => parseModule("import('foo')"),
                       SyntaxError);
