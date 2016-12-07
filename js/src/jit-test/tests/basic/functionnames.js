/*
 * Most of these test cases are adapted from:
 * http://johnjbarton.github.com/nonymous/index.html
 */

function assertName(fn, name) {
    assertEq(displayName(fn), name)
}

/* simple names */
var a = function b() {};
function c() {};
assertName(a, 'b');
assertName(c, 'c');

var a = function(){},
    b = function(){};
assertName(a, 'a');
assertName(b, 'b');

/* nested names */
var main = function() {
    function Foo(a) { assertName(a, 'main/foo<') }
    var foo = new Foo(function() {});
};
assertName(main, 'main')
main();

/* duplicated */
var Baz = Bar = function(){}
assertName(Baz, 'Bar');
assertName(Bar, 'Bar');

/* returned from an immediate function */
var Foo = function (){
    assertName(arguments.callee, 'Foo<')
    return function(){};
}();
assertName(Foo, 'Foo</<');

/* various properties and such */
var x = {fox: { bax: function(){} } };
assertName(x.fox.bax, 'bax');
var foo = {foo: {foo: {}}};
foo.foo.foo = function(){};
assertName(foo.foo.foo, 'foo.foo.foo');
var z = {
    foz: function() {
             var baz = function() {
                 var y = {bay: function() {}};
                 assertName(y.bay, 'bay');
             };
             assertName(baz, 'baz');
             baz();
         }
};
assertName(z.foz, 'foz');
z.foz();

var outer = function() {
    x.fox.bax.nx = function(){};
    var w = {fow: { baw: function(){} } };
    assertName(x.fox.bax.nx, 'outer/x.fox.bax.nx')
    assertName(w.fow.baw, 'baw');
};
assertName(outer, 'outer');
outer();
function Fuz(){};
Fuz.prototype = {
  add: function() {}
}
assertName(Fuz.prototype.add, 'add');

var x = 1;
x = function(){};
assertName(x, 'x');

var a = {b: {}};
a.b.c = (function() {
    assertName(arguments.callee, 'a.b.c<')
}());

a.b = function() {
    function foo(f) { assertName(f, 'a.b/<'); };
    return foo(function(){});
}
a.b();

var bar = 'bar';
a.b[bar] = function(){};
assertName(a.b.bar, 'a.b[bar]');

a.b = function() {
    assertName(arguments.callee, 'a.b<');
    return { a: function() {} }
}();
assertName(a.b.a, 'a');

a = {
    b: function(a) {
        if (a)
            return function() {};
        else
            return function() {};
    }
};
assertName(a.b, 'b');
assertName(a.b(true), 'b/<')
assertName(a.b(false), 'b/<')

function f(g) {
    assertName(g, 'x<');
    return g();
}
var x = f(function () { return function() {}; });
assertName(x, 'x</<');

var a = {'b': function(){}};
assertName(a.b, 'b');

function g(f) {
  assertName(f, '');
}
label: g(function () {});

var z = [function() {}];
assertName(z[0], 'z<');

/* fuzz bug from 785089 */
odeURIL:(function(){})

a = { 1: function () {} };
assertName(a[1], 'a[1]');

a = {
  "embedded spaces": function(){},
  "dots.look.like.property.references": function(){},
  "\"\'quotes\'\"": function(){},
  "!@#$%": function(){}
};
assertName(a["embedded spaces"], 'embedded spaces');
assertName(a["dots.look.like.property.references"], 'dots.look.like.property.references');
assertName(a["\"\'quotes\'\""], '"\'quotes\'"');
assertName(a["!@#$%"], '!@#$%');

a.b = {};
a.b.c = {};
a.b["c"]["d e"] = { f: { 1: { "g": { "h i": function() {} } } } };
assertName(a.b.c["d e"].f[1].g["h i"], 'h i');

this.m = function () {};
assertName(m, "this.m");

function N() {
  this.o = function () {}
}
let n = new N()
assertName(n.o, "N/this.o");
