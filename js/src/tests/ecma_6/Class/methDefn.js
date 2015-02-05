var BUGNUMBER = 924672;
var summary = 'Method Definitions'

print(BUGNUMBER + ": " + summary);

// Function definitions.
function syntaxError (script) {
    try {
        Function(script);
    } catch (e) {
        if (e instanceof SyntaxError) {
            return;
        }
    }
    throw new Error('Expected syntax error: ' + script);
}


// Tests begin.

syntaxError("{a(){}}");
syntaxError("b = {a(");
syntaxError("b = {a)");
syntaxError("b = {a(}");
syntaxError("b = {a)}");
syntaxError("b = {a()");
syntaxError("b = {a()}");
syntaxError("b = {a(){}");
syntaxError("b = {a){}");
syntaxError("b = {a}}");
syntaxError("b = {a{}}");
syntaxError("b = {a({}}");
syntaxError("b = {a@(){}}");
syntaxError("b = {a() => 0}");

b = {a(){return 5;}};
assertEq(b.a(), 5);

b = {a(){return "hey";}};
assertEq(b.a(), "hey");

b = {a(){return arguments;}};
assertEq(b.a().length, 0);

b = {a(c){return arguments;}};
assertEq(b.a().length, 0);

b = {a(c){return arguments;}};
assertEq(b.a(1).length, 1);

b = {a(c){return arguments;}};
assertEq(b.a(1)[0], 1);

b = {a(c,d){return arguments;}};
assertEq(b.a(1,2).length, 2);

b = {a(c,d){return arguments;}};
assertEq(b.a(1,2)[0], 1);

b = {a(c,d){return arguments;}};
assertEq(b.a(1,2)[1], 2);

// Methods along with other properties.
syntaxError("b = {,a(){}}");
syntaxError("b = {@,a(){}}");
syntaxError("b = {a(){},@}");
syntaxError("b = {a : 5 , (){}}");
syntaxError("b = {a : 5@ , a(){}}");
syntaxError("b = {a : , a(){}}");
syntaxError("b = {a : 5, a()}}");
syntaxError("b = {a : 5, a({}}");
syntaxError("b = {a : 5, a){}}");
syntaxError("b = {a : 5, a(){}");
var c = "d";
b = { a   : 5,
      [c] : "hey",
      e() {return 6;},
      get f() {return 7;},
      set f(g) {this.h = 9;}
}
assertEq(b.a, 5);
assertEq(b.d, "hey");
assertEq(b.e(), 6);
assertEq(b.f, 7);
assertEq(b.h !== 9, true);
b.f = 15;
assertEq(b.h, 9);


var i = 0;
var a = {
    foo0 : function (){return 0;},
    ["foo" + ++i](){return 1;},
    ["foo" + ++i](){return 2;},
    ["foo" + ++i](){return 3;},
    foo4(){return 4;}
};
assertEq(a.foo0(), 0);
assertEq(a.foo1(), 1);
assertEq(a.foo2(), 2);
assertEq(a.foo3(), 3);
assertEq(a.foo4(), 4);

// Symbols.
var unique_sym = Symbol("1"), registered_sym = Symbol.for("2");
a = { [unique_sym](){return 2;}, [registered_sym](){return 3;} };
assertEq(a[unique_sym](), 2);
assertEq(a[registered_sym](), 3);

// Method characteristics.
a = { b(){ return 4;} };
b = Object.getOwnPropertyDescriptor(a, "b");
assertEq(b.configurable, true);
assertEq(b.enumerable, true);
assertEq(b.writable, true);
assertEq(b.value(), 4);

// Defining several methods using eval.
var code = "({";
for (i = 0; i < 1000; i++)
    code += "['foo' + " + i + "]() {return 'ok';}, "
code += "['bar']() {return 'all ok'}});";
var obj = eval(code);
for (i = 0; i < 1000; i++)
    assertEq(obj["foo" + i](), "ok");
assertEq(obj["bar"](), "all ok");

// this
var obj = {
    a : "hey",
    meth(){return this.a;}
}
assertEq(obj.meth(), "hey");

// Inheritance
var obj2 = Object.create(obj);
assertEq(obj2.meth(), "hey");

var obj = {
    a() {
        return "hey";
    }
}
assertEq(obj.a.call(), "hey");

// Duplicates
var obj = {
    meth : 3,
    meth() { return 4; },
    meth() { return 5; }
}
assertEq(obj.meth(), 5);

var obj = {
    meth() { return 4; },
    meth() { return 5; },
    meth : 3
}
assertEq(obj.meth, 3);
assertThrowsInstanceOf(function() {obj.meth();}, TypeError);

// Strict mode
a = {b(c){"use strict";return c;}};
assertEq(a.b(1), 1);
a = {["b"](c){"use strict";return c;}};
assertEq(a.b(1), 1);

// Tests provided by benvie in the bug to distinguish from ES5 desugar.
assertEq(({ method() {} }).method.name, "method");
assertThrowsInstanceOf(function() {({ method() { method() } }).method() }, ReferenceError);


reportCompare(0, 0, "ok");
