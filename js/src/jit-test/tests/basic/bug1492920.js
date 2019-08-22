var N = 20;
var k = 15;  // k < N

/* test 1: insertion of new blank object in ctor.__proto__ chain */

function C() {}
C.__proto__ = Object.create(Function.prototype);

for (var i = 0; i < N; i++) {
	var o = new C();
	assertEq(o instanceof C, true);
}

/* test 2: overriding of @@hasInstance on the proto chain, partway
 * through execution (should trigger a guard) */

function D() {}

for (var i = 0; i < N; i++) {
    var o = new D();
    if (i == k) {
        D.__proto__ = {[Symbol.hasInstance]() { return false; }};
    }
    assertEq(o instanceof D, i < k);
}

/* test 3: overriding of @@hasInstance on an intermediate object in the proto
 * chain */

function E() {}

E.__proto__ = Object.create(Object.create(Object.create(Function.prototype)));
var intermediateProto = E.__proto__.__proto__;

for (var i = 0; i < N; i++) {
  var o = new E;
  if (i == k) {
    intermediateProto.__proto__ = {[Symbol.hasInstance]() { return false; }};
  }
  assertEq(o instanceof E, i < k);
}
