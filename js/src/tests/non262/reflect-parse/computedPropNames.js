// |reftest| skip-if(!xulRuntime.shell)
function test() {

// Bug 924688: computed property names
assertExpr('a= {[field1]: "a", [field2=1]: "b"}',
          aExpr("=", ident("a"),
                objExpr([{ key: computedName(ident("field1")), value: lit("a")},
                         { key: computedName(aExpr("=", ident("field2"), lit(1))),
                           value: lit("b")}])));

assertExpr('a= {["field1"]: "a", field2 : "b"}',
          aExpr("=", ident("a"),
                objExpr([{ key: computedName(lit("field1")), value: lit("a") },
                         { key: ident("field2"), value: lit("b") }])));

assertExpr('a= {[1]: 1, 2 : 2}',
          aExpr("=", ident("a"),
                objExpr([{ key: computedName(lit(1)), value: lit(1) },
                         { key: lit(2), value: lit(2) }])));

// Bug 924688: computed property names - location information
var node = Reflect.parse("a = {[field1]: 5}");
Pattern({ body: [ { expression: { right: { properties: [ {key: { loc:
    { start: { line: 1, column: 5 }, end: { line: 1, column: 13 }}}}]}}}]}).match(node);

// Bug 1048384 - Getter/setter syntax with computed names
assertExpr("b = { get [meth]() { } }", aExpr("=", ident("b"),
              objExpr([{ key: computedName(ident("meth")), value: funExpr(null, [], blockStmt([])),
                method: false, kind: "get"}])));
assertExpr("b = { set [meth](a) { } }", aExpr("=", ident("b"),
              objExpr([{ key: computedName(ident("meth")), value: funExpr(null, [ident("a")],
                blockStmt([])), method: false, kind: "set"}])));

// Bug 1220702 - If it's not written as a method, `.method` should be false.
assertExpr("({[x]: function () {}})",
           objExpr([{
               key: computedName(ident("x")),
               value: funExpr(null, [], blockStmt([])),
               method: false,
               kind: "init"
           }]));

}

runtest(test);
