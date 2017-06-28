// Count constructor calls
var cnt = 0;
class Base { constructor() { ++cnt; } }

// Force |JSFunction->hasScript()|
new Base();
assertEq(cnt, 1);

// Calling a ClassConstructor must throw
(function() {
    function f() { Base(); }
    try { f() } catch (e) {}
    try { f() } catch (e) {}
    assertEq(cnt, 1);
})();

// Spread-calling a ClassConstructor must throw
(function() {
    function f() { Base(...[]); }
    try { f() } catch (e) {}
    try { f() } catch (e) {}
    assertEq(cnt, 1);
})();

// Function.prototype.call must throw on ClassConstructor
(function() {
    function f() { Base.call(null); }
    try { f() } catch (e) {}
    try { f() } catch (e) {}
    assertEq(cnt, 1);
})();

// Function.prototype.apply must throw on ClassConstructor
(function() {
    function f() { Base.apply(null, []); }
    try { f() } catch (e) {}
    try { f() } catch (e) {}
    assertEq(cnt, 1);
})();

// Getter must throw if it is a ClassConstructor
(function() {
    var o = {};
    Object.defineProperty(o, "prop", { get: Base });
    function f() { o.prop };
    try { f() } catch (e) {}
    try { f() } catch (e) {}
    assertEq(cnt, 1);
})();

// Setter must throw if it is a ClassConstructor
(function() {
    var o = {};
    Object.defineProperty(o, "prop", { set: Base });
    function f() { o.prop = 1 };
    try { f() } catch (e) {}
    try { f() } catch (e) {}
    assertEq(cnt, 1);
})();

// Proxy apply must throw if it is a ClassConstructor
(function() {
    var o = new Proxy(function() { }, { apply: Base });
    function f() { o() };
    try { f() } catch (e) {}
    try { f() } catch (e) {}
    assertEq(cnt, 1);
})();

// Proxy get must throw if it is a ClassConstructor
(function() {
    var o = new Proxy({}, { get: Base });
    function f() { o.x }
    try { f() } catch (e) {}
    try { f() } catch (e) {}
    assertEq(cnt, 1);
})();
