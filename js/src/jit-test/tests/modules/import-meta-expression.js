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
metaProperty = (meta, property) => Pattern({
    type: "MetaProperty",
    meta: meta,
    property: property
});
memberExpression = (object, property) => Pattern({
    type: "MemberExpression",
    object: object,
    property: property
});

function parseAsModule(source)
{
    return Reflect.parse(source, {target: "module"});
}

program([
    expressionStatement(
        metaProperty(
            ident("import"),
            ident("meta")
        )
    )
]).assert(parseAsModule("import.meta;"));

program([
    expressionStatement(
        assignmentExpression(
            ident("x"),
            "=",
            metaProperty(
                ident("import"),
                ident("meta")
            )
        )
    )
]).assert(parseAsModule("x = import.meta;"));

program([
    expressionStatement(
        assignmentExpression(
            memberExpression(
                metaProperty(
                    ident("import"),
                    ident("meta")
                ),
                ident("foo")
            ),
            "=",
            ident("x"),
        )
    )
]).assert(parseAsModule("import.meta.foo = x;"));

function assertModuleParseThrowsSyntaxError(source)
{
    assertThrowsInstanceOf(() => parseAsModule(source), SyntaxError);
}

function assertModuleParseThrowsReferenceError(source)
{
    assertThrowsInstanceOf(() => parseAsModule(source), ReferenceError);
}

assertModuleParseThrowsSyntaxError("import");
assertModuleParseThrowsSyntaxError("import.");
assertModuleParseThrowsSyntaxError("import.met");
assertModuleParseThrowsSyntaxError("import.metaa");
assertModuleParseThrowsSyntaxError("x = import");
assertModuleParseThrowsSyntaxError("x = import.");
assertModuleParseThrowsSyntaxError("x = import.met");
assertModuleParseThrowsSyntaxError("x = import.metaa");

assertModuleParseThrowsReferenceError("import.meta = x");
