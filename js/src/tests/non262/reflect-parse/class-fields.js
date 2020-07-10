// |reftest| skip-if(!xulRuntime.shell||(function(){try{eval('c=class{x;}');return(false);}catch{return(true);}})())
// Classes
function testClassFields() {
    function constructor_(name) {
        let body = blockStmt([]);
        let method = funExpr(ident(name), [], body);
        return classMethod(ident("constructor"), method, "method", false);
    }

    let genConstructor = constructor_("C");

    // TODO: Should genConstructor be present?
    assertExpr("(class C { x = 2; })", classExpr(ident("C"), null, [classField(ident("x"), lit(2)), genConstructor]));
    assertExpr("(class C { x = x; })", classExpr(ident("C"), null, [classField(ident("x"), ident("x")), genConstructor]))
    assertExpr("(class C { x; })", classExpr(ident("C"), null, [classField(ident("x"), null), genConstructor]))
    assertExpr("(class C { x; y = 2; })", classExpr(ident("C"), null, [classField(ident("x"), null), classField(ident("y"), lit(2)), genConstructor]))
    assertExpr("(class C { x = 2; constructor(){} })", classExpr(ident("C"), null, [classField(ident("x"), lit(2)), genConstructor]))
}

runtest(testClassFields);
