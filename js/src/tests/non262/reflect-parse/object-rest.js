// |reftest| skip-if(!xulRuntime.shell)

function property(key, value = key, shorthand = key === value) {
    return { key, value, shorthand };
}

function assertDestrAssign(src, pattern) {
    assertExpr(`(${src} = 0)`, aExpr("=", pattern, lit(0)));
}

function assertDestrBinding(src, pattern) {
    assertDecl(`var ${src} = 0`, varDecl([{id: pattern, init: lit(0)}]));
}

function test() {
    // Target expression must be a simple assignment target in object assignment patterns.
    assertDestrAssign("{...x}", objPatt([spread(ident("x"))]));
    assertDestrAssign("{...(x)}", objPatt([spread(ident("x"))]));
    assertDestrAssign("{...obj.p}", objPatt([spread(dotExpr(ident("obj"), ident("p")))]));
    assertDestrAssign("{...(obj.p)}", objPatt([spread(dotExpr(ident("obj"), ident("p")))]));

    // Object binding patterns only allow binding identifiers.
    assertDestrBinding("{...x}", objPatt([spread(ident("x"))]));

    // The rest-property can be preceded by other properties.
    for (var assertDestr of [assertDestrAssign, assertDestrBinding]) {
        assertDestr("{a, ...x}", objPatt([property(ident("a")), spread(ident("x"))]));
        assertDestr("{a: b, ...x}", objPatt([property(ident("a"), ident("b")), spread(ident("x"))]));
        assertDestr("{[a]: b, ...x}", objPatt([property(comp(ident("a")), ident("b")), spread(ident("x"))]));
    }

    // Tests when __proto__ is used in the object pattern.
    for (var assertDestr of [assertDestrAssign, assertDestrBinding]) {
        assertDestr("{...__proto__}", objPatt([spread(ident("__proto__"))]));
        assertDestr("{__proto__, ...x}", objPatt([property(ident("__proto__")), spread(ident("x"))]));
    }
    assertDestrAssign("{__proto__: a, ...x}", objPatt([property(lit("__proto__"), ident("a")), spread(ident("x"))]));
    assertDestrBinding("{__proto__: a, ...x}", objPatt([property(ident("__proto__"), ident("a")), spread(ident("x"))]));
}

runtest(test);
