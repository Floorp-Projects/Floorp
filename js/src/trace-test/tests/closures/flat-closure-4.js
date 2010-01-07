actual = '';
expected = ',,,,,[object Object],[object Object],[object Object],[object Object],[object Object],,,,,,[object Object],[object Object],[object Object],[object Object],[object Object],,,,,,[object Object],[object Object],[object Object],[object Object],[object Object],,,,,,[object Object],[object Object],[object Object],[object Object],[object Object],,,,,,';

function f() {
  var e;
  for each (e in ["", {}, "", {}, "", {}, "", {}, ""]) {
      var g = function() { for (var i = 0; i < 5; ++i) appendToActual(e); }
      g();
  }
}

f();


assertEq(actual, expected)
