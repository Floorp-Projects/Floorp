// |reftest| skip-if(!xulRuntime.shell)
function test() {

// destructuring in for-in and for-each-in loop heads

var axbycz = objPatt([assignProp("a", ident("x")),
                      assignProp("b", ident("y")),
                      assignProp("c", ident("z"))]);
var xyz = arrPatt([assignElem("x"), assignElem("y"), assignElem("z")]);

assertStmt("for (var {a:x,b:y,c:z} in foo);", forInStmt(varDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let {a:x,b:y,c:z} in foo);", forInStmt(letDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ({a:x,b:y,c:z} in foo);", forInStmt(axbycz, ident("foo"), emptyStmt));
assertStmt("for (var [x,y,z] in foo);", forInStmt(varDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let [x,y,z] in foo);", forInStmt(letDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ([x,y,z] in foo);", forInStmt(xyz, ident("foo"), emptyStmt));
assertStmt("for (var {a:x,b:y,c:z} of foo);", forOfStmt(varDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let {a:x,b:y,c:z} of foo);", forOfStmt(letDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ({a:x,b:y,c:z} of foo);", forOfStmt(axbycz, ident("foo"), emptyStmt));
assertStmt("for (var [x,y,z] of foo);", forOfStmt(varDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let [x,y,z] of foo);", forOfStmt(letDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ([x,y,z] of foo);", forOfStmt(xyz, ident("foo"), emptyStmt));
assertStmt("for each (var {a:x,b:y,c:z} in foo);", forEachInStmt(varDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each (let {a:x,b:y,c:z} in foo);", forEachInStmt(letDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each ({a:x,b:y,c:z} in foo);", forEachInStmt(axbycz, ident("foo"), emptyStmt));
assertStmt("for each (var [x,y,z] in foo);", forEachInStmt(varDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each (let [x,y,z] in foo);", forEachInStmt(letDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each ([x,y,z] in foo);", forEachInStmt(xyz, ident("foo"), emptyStmt));

assertStmt("for (const x in foo);",
           forInStmt(constDecl([{ id: ident("x"), init: null }]), ident("foo"), emptyStmt));
assertStmt("for (const {a:x,b:y,c:z} in foo);",
           forInStmt(constDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (const [x,y,z] in foo);",
           forInStmt(constDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));

assertStmt("for (const x of foo);",
           forOfStmt(constDecl([{id: ident("x"), init: null }]), ident("foo"), emptyStmt));
assertStmt("for (const {a:x,b:y,c:z} of foo);",
           forOfStmt(constDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (const [x,y,z] of foo);",
           forOfStmt(constDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));

assertStmt("for each (const x in foo);",
           forEachInStmt(constDecl([{ id: ident("x"), init: null }]), ident("foo"), emptyStmt));
assertStmt("for each (const {a:x,b:y,c:z} in foo);",
           forEachInStmt(constDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each (const [x,y,z] in foo);",
           forEachInStmt(constDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));

assertError("for (x = 22 in foo);", SyntaxError);-
assertError("for ({a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for ([x,y,z] = 22 in foo);", SyntaxError);
assertError("for (const x = 22 in foo);", SyntaxError);
assertError("for (const {a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for (const [x,y,z] = 22 in foo);", SyntaxError);
assertError("for each (const x = 22 in foo);", SyntaxError);
assertError("for each (const {a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for each (const [x,y,z] = 22 in foo);", SyntaxError);

}

runtest(test);
