var count = 0;
var a = {__noSuchMethod__: function() { count++; } }

function f() {
  for (var i = 0; i < 10; i++) {
    try {
      a.b();
    } catch (e) {
      assertEq(true, false);
    }
  }
}
f();

assertEq(count, 10);
