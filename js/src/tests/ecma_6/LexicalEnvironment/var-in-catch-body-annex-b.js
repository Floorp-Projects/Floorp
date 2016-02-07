// Tests annex B.3.5.

assertThrowsInstanceOf(function () {
  eval(`
       function f() {
         let x;
         try {} catch (x) {
           var x;
         }
       }
       `);
}, SyntaxError);

assertThrowsInstanceOf(function () {
  eval(`
       function f() {
         try {} catch (x) {
           let y;
           var y;
         }
       }
       `);
}, SyntaxError);

assertThrowsInstanceOf(function () {
  eval(`
       function f() {
         try {} catch (x) {
           let x;
         }
       }
       `);
}, SyntaxError);

assertThrowsInstanceOf(function () {
  eval(`
       function f() {
         try {} catch (x) {
           const x;
         }
       }
       `);
}, SyntaxError);

var log = '';
var x = 'global-x';

function g() {
  x = 'g';
  try { throw 8; } catch (x) {
    var x = 42;
    log += x;
  }
  log += x;
}
g();

// Tests that var declaration is allowed in for-in head.
function h0() {
  try {} catch (e) {
    for (var e in {});
  }
}
h0();

// Tests that var declaration is allowed in C-for head.
function h1() {
  try {} catch (e) {
    for (var e;;);
  }
}
h1();

// Tests that redeclaring a var inside the catch is allowed.
function h3() {
  var e;
  try {} catch (e) {
    var e;
  }
}
h3();

// Tests that var declaration is not allowed in for-of head.
assertThrowsInstanceOf(function () {
  eval(`
       function h2() {
         try {} catch (e) { for (var e of {}); }
       }
       log += 'unreached';
       `);
}, SyntaxError);

if (typeof evaluate === "function") {
  assertThrowsInstanceOf(function () {
    evaluate(`
             let y;
             try {} catch (y) { var y; }
             `);
  }, SyntaxError);
}

assertEq(log, "42g");

if ("reportCompare" in this)
  reportCompare(true, true)
