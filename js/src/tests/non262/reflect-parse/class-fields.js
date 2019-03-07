// |reftest| skip-if(!xulRuntime.shell)
// Classes
function testClassFields() {
    function constructor_(name) {
        let body = blockStmt([]);
        let method = funExpr(ident(name), [], body);
        return classMethod(ident("constructor"), method, "method", false);
    }

    let genConstructor = constructor_("C");

    assertExpr("(class C { x = 2; })", classExpr(ident("C"), null, [classField(ident("x"), lit(2)), genConstructor]));
    assertExpr("(class C { x = x; })", classExpr(ident("C"), null, [classField(ident("x"), ident("x")), genConstructor]))
    assertExpr("(class C { x; })", classExpr(ident("C"), null, [classField(ident("x"), undefined)]))
    assertExpr("(class C { x; y = 2; })", classExpr(ident("C"), null, [classField(ident("x"), undefined), classField(ident("y"), lit(2)), genConstructor]))
    assertExpr("(class C { x = 2; constructor(){} })", classExpr(ident("C"), null, [classField(ident("x"), lit(2)), genConstructor]))
}

runtest(testClassFields);
