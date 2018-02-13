// Receiver shadows
(function() {
    function check(p) { return p.x; }

    let a = { x: "a" };
    let b = { __proto__: a };
    let c = { __proto__: b };
    let d = { __proto__: c };

    assertEq(check(d), "a");
    assertEq(check(d), "a");
    d.x = "d";
    assertEq(check(d), "d");
})();

// Intermediate proto shadows
(function() {
    function check(p) { return p.x; }

    let a = { x: "a" };
    let b = { __proto__: a };
    let c = { __proto__: b };
    let d = { __proto__: c };

    assertEq(check(d), "a");
    assertEq(check(d), "a");
    c.x = "c";
    assertEq(check(d), "c");
})();

// Receiver proto changes
(function() {
    function check(p) { return p.x; }

    let a = { x: "a" };
    let b = { __proto__: a };
    let c = { __proto__: b };
    let d = { __proto__: c };

    assertEq(check(d), "a");
    assertEq(check(d), "a");
    d.__proto__ = { x: "?" };
    assertEq(check(d), "?");
})();

// Intermediate proto changes
(function() {
    function check(p) { return p.x; }

    let a = { x: "a" };
    let b = { __proto__: a };
    let c = { __proto__: b };
    let d = { __proto__: c };

    assertEq(check(d), "a");
    assertEq(check(d), "a");
    c.__proto__ = { x: "?" };
    assertEq(check(d), "?");
})();

// Uncacheable holder proto
(function() {
    function check(p) { return p.x; }

    function Base() { this.x = "a"; }
    let a = new Base;
    a.__proto__ = new Object;
    let b = { __proto__: a };
    let c = { __proto__: b };
    let d = { __proto__: c };

    assertEq(check(d), "a");
    assertEq(check(d), "a");
    b.__proto__ = { x: "?" };
    assertEq(check(d), "?");
})();

// Uncacheable intermediate proto
(function() {
    function check(p) { return p.x; }

    function Base() { this.x = "a"; }
    function Node() { }

    let a = new Base;
    let b = new Node; b.__proto__ = a;
    let c = { __proto__: b };
    let d = { __proto__: c };

    assertEq(check(d), "a");
    assertEq(check(d), "a");
    b.__proto__ = { x: "?" };
    assertEq(check(d), "?");
})();

// Uncacheable receiver proto
(function() {
    function check(p) { return p.x; }

    function Base() { this.x = "a"; }
    function Node() { }

    let a = new Base;
    let b = { __proto__: a };
    let c = { __proto__: b };
    let d = new Node; d.__proto__ = c;

    assertEq(check(d), "a");
    assertEq(check(d), "a");
    d.__proto__ = { x: "?" };
    assertEq(check(d), "?");
})();

// Uncacheable receiver proto (only receiver / holder)
(function() {
    function check(p) { return p.x; }

    function Base() { this.x = "a"; }
    function Node() { }

    let a = new Base;
    let b = new Node; b.__proto__ = a;

    assertEq(check(b), "a");
    assertEq(check(b), "a");
    b.__proto__ = { x: "?" };
    assertEq(check(b), "?");
})();
