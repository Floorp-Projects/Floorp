// |jit-test| --enable-import-attributes

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
importCall = (ident, args) => Pattern({
    type: "CallImport",
    ident: ident,
    arguments: args
});

objExpr = (elts) => Pattern({
    type: "ObjectExpression",
    properties: elts
});
property = (key, value) => Pattern({
    type: "Property",
    kind: "init",
    key: key,
    value: value,
});
lit = (val) => Pattern({
    type: "Literal",
    value: val
});
callExpression = (callee, args) => Pattern({
    type: "CallExpression",
    callee: callee,
    arguments: args
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
                [
                    ident("foo")
                ]
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
                    [
                        ident("foo")
                    ]
                )
            )
        )
    ]).assert(parse("x = import(foo);"));

    if (getRealmConfiguration("importAttributes")) {
        program([
            expressionStatement(
                importCall(
                    ident("import"),
                    [
                        ident("foo"),
                        objExpr([])
                    ]
                )
            )
        ]).assert(parse("import(foo, {});"));

        program([
            expressionStatement(
                importCall(
                    ident("import"),

                    [
                        ident("foo"),
                        objExpr([
                            property(
                                ident("assert"),
                                objExpr([]
                            ))
                        ])
                    ]

                )
            )
        ]).assert(parse("import(foo, { assert: {} });"));

        program([
            expressionStatement(
                importCall(
                    ident("import"),
                    [
                        ident("foo"),
                        objExpr([
                            property(
                                ident("assert"),
                                objExpr([
                                    property(
                                        ident("type"),
                                        lit('json')
                                    )
                                ]
                            ))
                        ])
                    ]
                )
            )
        ]).assert(parse("import(foo, { assert: { type: 'json' } });"));

        program([
            expressionStatement(
                importCall(
                    ident("import"),
                    [
                        ident("foo"),
                        objExpr([
                            property(
                                ident("assert"),
                                objExpr([
                                    property(
                                        ident("type"),
                                        lit('json')
                                    ),
                                    property(
                                        ident("foo"),
                                        lit('bar')
                                    )
                                ]
                            ))
                        ])
                    ]
                )
            )
        ]).assert(parse("import(foo, { assert: { type: 'json', foo: 'bar' } });"));

        program([
            expressionStatement(
                importCall(
                    ident("import"),
                    [
                        ident("foo"),
                        objExpr([
                            property(
                                ident("assert"),
                                objExpr([
                                    property(
                                        ident("type"),
                                        callExpression(ident('getType'), [])
                                    )
                                ]
                            ))
                        ])
                    ]
                )
            )
        ]).assert(parse("import(foo, { assert: { type: getType() } });"));
    }
}

function assertParseThrowsSyntaxError(source)
{
    assertThrowsInstanceOf(() => parseAsClassicScript(source), SyntaxError);
    assertThrowsInstanceOf(() => parseAsModuleScript(source), SyntaxError);
}

assertParseThrowsSyntaxError("import");
assertParseThrowsSyntaxError("import(");
assertParseThrowsSyntaxError("import(1,");
assertParseThrowsSyntaxError("x = import");
assertParseThrowsSyntaxError("x = import(");
assertParseThrowsSyntaxError("x = import(1,");
assertParseThrowsSyntaxError("x = import(1, 2");

if (!getRealmConfiguration("importAttributes")) {
    assertParseThrowsSyntaxError("import(1, 2");
    assertParseThrowsSyntaxError("import(1, 2)");
    assertParseThrowsSyntaxError("x = import(1, 2)");
}
else {
    assertParseThrowsSyntaxError("import(1, 2, 3)");
}
