// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 671947;
var summary = "Unqualified function invocation uses the global object of the called property as |this|";
var actual = "------------------------";
var expect = "ooaoboabuuaubuabooaoboab";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

this.name = "o";

function f() {
  return this ? this.name : "t";
}
function g() {
  "use strict";
  return this ? this.name : "u";
}
function h() {
  return this ? this.name : "v";
}

var sb = newGlobal('same-compartment');
sb.parent = this;

evalcx('\n' +
       ' this.name="i";\n' +
       ' this.f = parent.f;\n' +
       ' this.g = parent.g;\n' +
       ' this.a = { name:"a", f:parent.f, g:parent.g };\n' +
       ' this.b = { name:"b", f:parent.f, g:parent.g };\n' +
       ' Object.defineProperty(this, "h", { get: (function(){ return parent.h; })});\n' +
       ' Object.defineProperty(a, "h", { get: (function(){ return parent.h; })});\n' +
       ' Object.defineProperty(b, "h", { get: (function(){ return parent.h; })});\n' +

       ' var results = "";\n' +

       ' /* Three of the first four cases pass undefined (promoted inside the callee to the callee\'s global object). */\n' +
       ' /* a.f() is the one exception, which passes the base, a, as the this object. */\n' +
       ' results += (function(){return f();})();\n' +
       ' results += (function(){return (1,f)();})();\n' +
       ' results += (function(){return a.f();})();\n' +
       ' results += (function(){return eval("f()");})();\n' +
       ' /* Same cases as above, but wrapped in a with. The first & last of these cases pass b, */\n' +
       ' /* the object scoped by the with, as the this value. */\n' +
       ' /* a.f() still passes the explicit base, a. (1,f)() is a little tricksier - this passes */\n' +
       ' /* undefined (promoted to the callee global object) since the comma operator calles GetValue */\n' +
       ' /* on the reference (see ES5 11.14.) */\n' +
       ' results += (function(){with(b){ return (function(){ return f();})(); }})();\n' +
       ' results += (function(){with(b){ return (function(){ return (1,f)();})(); }})();\n' +
       ' results += (function(){with(b){ return (function(){ return a.f();})(); }})();\n' +
       ' results += (function(){with(b){ return (function(){ return eval("f()");})(); }})();\n' +

       ' /* Same tests as above, but with a strict callee. */\n' +
       ' /* We expect the same results, except undefined this is not replaced with the global object. */\n' +
       ' results += (function(){return g();})();\n' +
       ' results += (function(){return (1,g)();})();\n' +
       ' results += (function(){return a.g();})();\n' +
       ' results += (function(){return eval("g()");})();\n' +
       ' results += (function(){with(b){ return g(); }})();\n' +
       ' results += (function(){with(b){ return (1,g)(); }})();\n' +
       ' results += (function(){with(b){ return a.g(); }})();\n' +
       ' results += (function(){with(b){ return (function(){ return eval("g()");})(); }})();\n' +

       ' /* Same as the first set, but h is a getter property. */\n' +
       ' results += (function(){return h();})();\n' +
       ' results += (function(){return (1,h)();})();\n' +
       ' results += (function(){return a.h();})();\n' +
       ' results += (function(){return eval("h()");})();\n' +
       ' results += (function(){with(b){ return h(); }})();\n' +
       ' results += (function(){with(b){ return (1,h)(); }})();\n' +
       ' results += (function(){with(b){ return a.h(); }})();\n' +
       ' results += (function(){with(b){ return (function(){ return eval("h()");})(); }})();\n' +

       ' parent.actual = results;\n' +
       '',
       sb);

reportCompare(expect, actual, "ok");
