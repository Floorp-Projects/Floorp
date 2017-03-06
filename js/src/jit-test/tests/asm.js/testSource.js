setCachingEnabled(true);

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

funcBody1 = funcBody.replace('function f0','function');
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
assertEq(f0.toString(), "function anonymous(\n) {\n" + bodyOnly + "\n}");
assertEq(f0.toSource(), "(function anonymous(\n) {\n" + bodyOnly + "\n})");

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var m = new Function(bodyOnly);
    assertEq(isAsmJSModuleLoadedFromCache(m), true);
    assertEq(m.toString(), "function anonymous(\n) {\n" + bodyOnly + "\n}");
    assertEq(m.toSource(), "(function anonymous(\n) {\n" + bodyOnly + "\n})");
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

funcBody1 = funcBody.replace('function f1', 'function');
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
assertEq(f1.toString(), "function anonymous(glob\n) {\n" + bodyOnly + "\n}");
assertEq(f1.toSource(), "(function anonymous(glob\n) {\n" + bodyOnly + "\n})");

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var m = new Function('glob', bodyOnly);
    assertEq(isAsmJSModuleLoadedFromCache(m), true);
    assertEq(m.toString(), "function anonymous(glob\n) {\n" + bodyOnly + "\n}");
    assertEq(m.toSource(), "(function anonymous(glob\n) {\n" + bodyOnly + "\n})");
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

f2 = function(glob, ffi) {
    "use asm";
    function g() {}
    return g;

}

funcBody1 = funcBody.replace('function f2', 'function');
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
assertEq(f2.toString(), "function anonymous(glob,ffi\n) {\n" + bodyOnly + "\n}");
assertEq(f2.toSource(), "(function anonymous(glob,ffi\n) {\n" + bodyOnly + "\n})");

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var m = new Function('glob', 'ffi', bodyOnly);
    assertEq(isAsmJSModuleLoadedFromCache(m), true);
    assertEq(m.toString(), "function anonymous(glob,ffi\n) {\n" + bodyOnly + "\n}");
    assertEq(m.toSource(), "(function anonymous(glob,ffi\n) {\n" + bodyOnly + "\n})");
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

f3 = function(glob, ffi, heap) {
    "use asm";
    function g() {}
    return g;

}

funcBody1 = funcBody.replace('function f3', 'function');
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
assertEq(f3.toString(), "function anonymous(glob,ffi,heap\n) {\n" + bodyOnly + "\n}");
assertEq(f3.toSource(), "(function anonymous(glob,ffi,heap\n) {\n" + bodyOnly + "\n})");

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var m = new Function('glob', 'ffi', 'heap', bodyOnly);
    assertEq(isAsmJSModuleLoadedFromCache(m), true);
    assertEq(m.toString(), "function anonymous(glob,ffi,heap\n) {\n" + bodyOnly + "\n}");
    assertEq(m.toSource(), "(function anonymous(glob,ffi,heap\n) {\n" + bodyOnly + "\n})");
}

})();

/* Modules in "use strict" context */
(function() {

var funcSource =
    `function(glob, ffi, heap) {
        "use asm";
        function g() {}
        return g;
    }`;

var f4 = eval("\"use strict\";\n(" + funcSource + ")");

var expectedToString = funcSource;
var expectedToSource = '(' + expectedToString + ')';

assertEq(f4.toString(), expectedToString);
assertEq(f4.toSource(), expectedToSource);

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var f5 = eval("\"use strict\";\n(" + funcSource + ")");
    assertEq(isAsmJSModuleLoadedFromCache(f5), true);
    assertEq(f5.toString(), expectedToString);
    assertEq(f5.toSource(), expectedToSource);
}
})();

/* Functions */
(function() {

var noSrc = "function noArgument() {\n\
    return 42;\n\
}"
var oneSrc = "function oneArgument(x) {\n\
    x = x | 0;\n\
    return x + 1 | 0;\n\
}";
var twoSrc = "function twoArguments(x, y) {\n\
    x = x | 0;\n\
    y = y | 0;\n\
    return x + y | 0;\n\
}";
var threeSrc = "function threeArguments(a, b, c) {\n\
    a = +a;\n\
    b = +b;\n\
    c = +c;\n\
    return +(+(a * b) + c);\n\
}";

var funcBody = '\n\
    "use asm";\n'
    + noSrc + '\n'
    + oneSrc + '\n'
    + twoSrc + '\n'
    + threeSrc + '\n'
    + 'return {\n\
    no: noArgument,\n\
    one: oneArgument,\n\
    two: twoArguments,\n\
    three: threeArguments\n\
    }';

var g = new Function(funcBody);
var moduleG = g();

function checkFuncSrc(m) {
    assertEq(m.no.toString(), noSrc);
    assertEq(m.no.toSource(), noSrc);

    assertEq(m.one.toString(), oneSrc);
    assertEq(m.one.toSource(), oneSrc);

    assertEq(m.two.toString(), twoSrc);
    assertEq(m.two.toSource(), twoSrc);

    assertEq(m.three.toString(), threeSrc);
    assertEq(m.three.toSource(), threeSrc);
}
checkFuncSrc(moduleG);

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var g2 = new Function(funcBody);
    assertEq(isAsmJSModuleLoadedFromCache(g2), true);
    m = g2();
    checkFuncSrc(m);

    var moduleDecl = 'function g3() {' + funcBody + '}';
    eval(moduleDecl);
    m = g3();
    assertEq(isAsmJSModuleLoadedFromCache(g3), false);
    checkFuncSrc(m);

    eval('var x = 42;' + moduleDecl);
    m = g3();
    assertEq(isAsmJSModuleLoadedFromCache(g3), true);
    checkFuncSrc(m);
}

})();

/* Functions in "use strict" context */
(function () {

var funcCode = 'function g(x) {\n\
    x=x|0;\n\
    return x + 1 | 0;}';
var moduleCode = 'function () {\n\
    "use asm";\n' + funcCode + '\n\
    return g;\n\
    }',
    useStrict = '"use strict";';

var f5 = eval(useStrict + ";\n(" + moduleCode + "())");

var expectedToString = funcCode;
var expectedToSource = expectedToString

assertEq(f5.toString(), expectedToString);
assertEq(f5.toSource(), expectedToSource);

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var mf5 = eval("\"use strict\";\n(" + moduleCode + ")");
    assertEq(isAsmJSModuleLoadedFromCache(mf5), true);
    var f5 = mf5();
    assertEq(f5.toString(), expectedToString);
    assertEq(f5.toSource(), expectedToSource);
}

})();

/* Functions in "use strict" context with dynamic linking failure */
(function () {

var funcCode = 'function g(x) {\n\
    x=x|0;\n\
    return x + 1 | 0;}';
var moduleCode = 'function (glob) {\n\
    "use asm";\n\
    var fround = glob.Math.fround;\n\
    ' + funcCode + '\n\
    return g;\n\
    }',
    useStrict = '"use strict";';

var f6 = eval(useStrict + ";\n(" + moduleCode + "({Math:{}}))");

assertEq(f6.toString(), funcCode);
assertEq(f6.toSource(), funcCode);

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var mf6 = eval("\"use strict\";\n(" + moduleCode + ")");
    assertEq(isAsmJSModuleLoadedFromCache(mf6), true);
    var f6 = mf6({Math:{}});
    assertEq(f6.toString(), funcCode);
    assertEq(f6.toSource(), funcCode);
}

})();
