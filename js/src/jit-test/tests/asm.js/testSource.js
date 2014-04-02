(function() {
/*
 * NO ARGUMENT
 */

function f0() {
    "use asm";
    function g() {}
    return g;

}

var bodyOnly = '"use asm";\n\
    function g() {}\n\
    return g;\n';

var funcBody =  'function f0() {\n\
    "use asm";\n\
    function g() {}\n\
    return g;\n\n\
}';

assertEq(f0.toString(), funcBody);
assertEq(f0.toSource(), funcBody);

var f0 = function() {
    "use asm";
    function g() {}
    return g;

}

funcBody1 = funcBody.replace('function f0','function ');
assertEq(f0.toString(), funcBody1);
assertEq(f0.toSource(), '(' + funcBody1 + ')');

var g = function g0() {
    "use asm";
    function g() {}
    return g;

}

funcBody2 = funcBody.replace('function f0', 'function g0');
assertEq(g.toString(), funcBody2);
assertEq(g.toSource(), '(' + funcBody2 + ')');

f0 = new Function(bodyOnly);
assertEq(f0.toString(), "function anonymous() {\n" + bodyOnly + "\n}");
assertEq(f0.toSource(), "(function anonymous() {\n" + bodyOnly + "\n})");

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var m = new Function(bodyOnly);
    assertEq(isAsmJSModuleLoadedFromCache(m), true);
    assertEq(m.toString(), "function anonymous() {\n" + bodyOnly + "\n}");
    assertEq(m.toSource(), "(function anonymous() {\n" + bodyOnly + "\n})");
}

})();

(function() {
/*
 * ONE ARGUMENT
 */
function f1(glob) {
    "use asm";
    function g() {}
    return g;

}

var bodyOnly = '"use asm";\n\
    function g() {}\n\
    return g;\n';

var funcBody =  'function f1(glob) {\n\
    "use asm";\n\
    function g() {}\n\
    return g;\n\n\
}';

assertEq(f1.toString(), funcBody);
assertEq(f1.toSource(), funcBody);

f1 = function(glob) {
    "use asm";
    function g() {}
    return g;

}

funcBody1 = funcBody.replace('function f1', 'function ');
assertEq(f1.toString(), funcBody1);
assertEq(f1.toSource(), '(' + funcBody1 + ')');

var g = function g0(glob) {
    "use asm";
    function g() {}
    return g;

}

funcBody2 = funcBody.replace('function f1', 'function g0');
assertEq(g.toString(), funcBody2);
assertEq(g.toSource(), '(' + funcBody2 + ')');

f1 = new Function('glob', bodyOnly);
assertEq(f1.toString(), "function anonymous(glob) {\n" + bodyOnly + "\n}");
assertEq(f1.toSource(), "(function anonymous(glob) {\n" + bodyOnly + "\n})");

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var m = new Function('glob', bodyOnly);
    assertEq(isAsmJSModuleLoadedFromCache(m), true);
    assertEq(m.toString(), "function anonymous(glob) {\n" + bodyOnly + "\n}");
    assertEq(m.toSource(), "(function anonymous(glob) {\n" + bodyOnly + "\n})");
}

})();


(function() {
/*
 * TWO ARGUMENTS
 */
function f2(glob, ffi) {
    "use asm";
    function g() {}
    return g;

}

var bodyOnly = '"use asm";\n\
    function g() {}\n\
    return g;\n';

var funcBody =  'function f2(glob, ffi) {\n\
    "use asm";\n\
    function g() {}\n\
    return g;\n\n\
}';

assertEq(f2.toString(), funcBody);
assertEq(f2.toSource(), funcBody);

f2 = function (glob, ffi) {
    "use asm";
    function g() {}
    return g;

}

funcBody1 = funcBody.replace('function f2', 'function ');
assertEq(f2.toString(), funcBody1);
assertEq(f2.toSource(), '(' + funcBody1 + ')');

var g = function g0(glob, ffi) {
    "use asm";
    function g() {}
    return g;

}

var funcBody2 = funcBody.replace('function f2', 'function g0');
assertEq(g.toString(), funcBody2);
assertEq(g.toSource(), '(' + funcBody2 + ')');

f2 = new Function('glob', 'ffi', bodyOnly);
assertEq(f2.toString(), "function anonymous(glob, ffi) {\n" + bodyOnly + "\n}");
assertEq(f2.toSource(), "(function anonymous(glob, ffi) {\n" + bodyOnly + "\n})");

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var m = new Function('glob', 'ffi', bodyOnly);
    assertEq(isAsmJSModuleLoadedFromCache(m), true);
    assertEq(m.toString(), "function anonymous(glob, ffi) {\n" + bodyOnly + "\n}");
    assertEq(m.toSource(), "(function anonymous(glob, ffi) {\n" + bodyOnly + "\n})");
}

})();


(function() {
/*
 * THREE ARGUMENTS
 */
function f3(glob, ffi, heap) {
    "use asm";
    function g() {}
    return g;

}

var bodyOnly = '"use asm";\n\
    function g() {}\n\
    return g;\n';

var funcBody =  'function f3(glob, ffi, heap) {\n\
    "use asm";\n\
    function g() {}\n\
    return g;\n\n\
}';

assertEq(f3.toString(), funcBody);
assertEq(f3.toSource(), funcBody);

f3 = function (glob, ffi, heap) {
    "use asm";
    function g() {}
    return g;

}

funcBody1 = funcBody.replace('function f3', 'function ');
assertEq(f3.toString(), funcBody1);
assertEq(f3.toSource(), '(' + funcBody1 + ')');

var g = function g0(glob, ffi, heap) {
    "use asm";
    function g() {}
    return g;

}

funcBody2 = funcBody.replace('function f3', 'function g0');
assertEq(g.toString(), funcBody2);
assertEq(g.toSource(), '(' + funcBody2 + ')');

f3 = new Function('glob', 'ffi', 'heap', bodyOnly);
assertEq(f3.toString(), "function anonymous(glob, ffi, heap) {\n" + bodyOnly + "\n}");
assertEq(f3.toSource(), "(function anonymous(glob, ffi, heap) {\n" + bodyOnly + "\n})");

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var m = new Function('glob', 'ffi', 'heap', bodyOnly);
    assertEq(isAsmJSModuleLoadedFromCache(m), true);
    assertEq(m.toString(), "function anonymous(glob, ffi, heap) {\n" + bodyOnly + "\n}");
    assertEq(m.toSource(), "(function anonymous(glob, ffi, heap) {\n" + bodyOnly + "\n})");
}

})();
