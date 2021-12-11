function testName(C, name, hasValue, hasGetter, hasSetter, isFunction=false) {
    if (isFunction) {
        assertEq(typeof C.name, "function");
    } else {
        assertEq(C.name, name);
    }

    var desc = Object.getOwnPropertyDescriptor(C, "name");
    if (!hasValue && !hasGetter && !hasSetter) {
        assertEq(desc, undefined);
        return;
    }

    if (hasValue) {
        assertEq("value" in desc, true);
        if (isFunction) {
            assertEq(typeof desc.value, "function");
        } else {
            assertEq(desc.value, name);
        }
        // FIXME: Methods defined in classes should not be enumerable
        // (bug 1144630).
        // assertEq(desc.enumerable, false);
        assertEq(desc.configurable, true);

        assertEq("get" in desc, false);
        assertEq("set" in desc, false);

        return;
    }

    assertEq("value" in desc, false);

    if (hasGetter) {
        assertEq("get" in desc, true);
        assertEq(desc.get(), name);
        // assertEq(desc.enumerable, false);
        assertEq(desc.configurable, true);
    } else {
        assertEq("get" in desc, true);
        assertEq(desc.get, undefined);
        // assertEq(desc.enumerable, false);
        assertEq(desc.configurable, true);
    }

    if (hasSetter) {
        assertEq("set" in desc, true);
        assertEq(typeof desc.set, "function");
        // assertEq(desc.enumerable, false);
        assertEq(desc.configurable, true);
    } else {
        assertEq("set" in desc, true);
        assertEq(desc.set, undefined);
        // assertEq(desc.enumerable, false);
        assertEq(desc.configurable, true);
    }
}

// ---- declaration ---

class Decl {
    constructor() {}
}
testName(Decl, "Decl", true, false, false);

class DeclWithFunc {
    constructor() {}
    static name() {}
}
testName(DeclWithFunc, "DeclWithFunc", true, false, false, true);

class DeclWithGetter {
    constructor() {}
    static get name() { return "base"; }
}
testName(DeclWithGetter, "base", false, true, false);

class DeclWithSetter {
    constructor() {}
    static set name(v) {}
}
testName(DeclWithSetter, undefined, false, false, true);

class DeclWithGetterSetter {
    constructor() {}
    static get name() { return "base"; }
    static set name(v) {}
}
testName(DeclWithGetterSetter, "base", false, true, true);

function prop() {
  return "name";
}
class DeclWithComputed {
    constructor() {}
    static get [prop()]() { return "base"; }
}
testName(DeclWithGetterSetter, "base", false, true, true);

class ExtendedDecl1 extends Decl {
    constructor() {}
}
testName(ExtendedDecl1, "ExtendedDecl1", true, false, false);
delete ExtendedDecl1.name;
testName(ExtendedDecl1, "Decl", false, false, false);

class ExtendedDecl2 extends DeclWithGetterSetter {
    constructor() {}
    static get name() { return "extend"; }
}
testName(ExtendedDecl2, "extend", false, true, false);
delete ExtendedDecl2.name;
testName(ExtendedDecl2, "base", false, false, false);

class ExtendedDecl3 extends DeclWithGetterSetter {
    constructor() {}
    static set name(v) {}
}
testName(ExtendedDecl3, undefined, false, false, true);
delete ExtendedDecl3.name;
testName(ExtendedDecl3, "base", false, false, false);

// ---- expression ---

let Expr = class Expr {
    constructor() {}
};
testName(Expr, "Expr", true, false, false);

let ExprWithGetter = class ExprWithGetter {
    constructor() {}
    static get name() { return "base"; }
};
testName(ExprWithGetter, "base", false, true, false);

let ExprWithSetter = class ExprWithSetter {
    constructor() {}
    static set name(v) {}
};
testName(ExprWithSetter, undefined, false, false, true);

let ExprWithGetterSetter = class ExprWithGetterSetter {
    constructor() {}
    static get name() { return "base"; }
    static set name(v) {}
};
testName(ExprWithGetterSetter, "base", false, true, true);


let ExtendedExpr1 = class ExtendedExpr1 extends Expr {
    constructor() {}
};
testName(ExtendedExpr1, "ExtendedExpr1", true, false, false);
delete ExtendedExpr1.name;
testName(ExtendedExpr1, "Expr", false, false, false);

let ExtendedExpr2 = class ExtendedExpr2 extends ExprWithGetterSetter {
    constructor() {}
    static get name() { return "extend"; }
};
testName(ExtendedExpr2, "extend", false, true, false);
delete ExtendedExpr2.name;
testName(ExtendedExpr2, "base", false, false, false);

let ExtendedExpr3 = class ExtendedExpr3 extends ExprWithGetterSetter {
    constructor() {}
    static set name(v) {}
};
testName(ExtendedExpr3, undefined, false, false, true);
delete ExtendedExpr3.name;
testName(ExtendedExpr3, "base", false, false, false);

// ---- anonymous ----

// Anonymous class expressions use the empty string as the default name property.
// Use property assignment to avoid setting name property.
let tmp = {};
let Anon = tmp.value = class {
    constructor() {}
};
testName(Anon, "", true, false, false);

let AnonDefault = tmp.value = class { };
testName(AnonDefault, "", true, false, false);

let AnonWithGetter = tmp.value = class {
    constructor() {}
    static get name() { return "base"; }
};
testName(AnonWithGetter, "base", false, true, false);

let AnonWithSetter = tmp.value = class {
    constructor() {}
    static set name(v) {}
};
testName(AnonWithSetter, undefined, false, false, true);

let AnonWithGetterSetter = tmp.value = class {
    constructor() {}
    static get name() { return "base"; }
    static set name(v) {}
};
testName(AnonWithGetterSetter, "base", false, true, true);


let ExtendedAnon1 = tmp.value = class extends Anon {
    constructor() {}
};
testName(ExtendedAnon1, "", true, false, false);

let ExtendedAnonDefault = tmp.value = class extends Anon { };
testName(ExtendedAnonDefault, "", true, false, false);

let ExtendedAnon2 = tmp.value = class extends AnonWithGetterSetter {
    constructor() {}
    static get name() { return "extend"; }
};
testName(ExtendedAnon2, "extend", false, true, false);
delete ExtendedAnon2.name;
testName(ExtendedAnon2, "base", false, false, false);

let ExtendedAnon3 = tmp.value = class extends AnonWithGetterSetter {
    constructor() {}
    static set name(v) {}
};
testName(ExtendedAnon3, undefined, false, false, true);
delete ExtendedAnon3.name;
testName(ExtendedAnon3, "base", false, false, false);

// ---- mixed ----

let ExtendedExpr4 = class ExtendedExpr4 extends Anon {
    constructor() {}
}
testName(ExtendedExpr4, "ExtendedExpr4", true, false, false);
delete ExtendedExpr4.name;
testName(ExtendedExpr4, "", false, false, false);

let ExtendedExpr5 = class ExtendedExpr5 extends AnonWithGetterSetter {
    constructor() {}
    static get name() { return "extend"; }
}
testName(ExtendedExpr5, "extend", false, true, false);
delete ExtendedExpr5.name;
testName(ExtendedExpr5, "base", false, false, false);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
