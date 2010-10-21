actual = '';
expected = '7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,';

function heavy(s, t, u) {return eval(s)}

for (var i = 0; i < 5; ++i) {
  var flat = heavy("(function () {var x = t * t; return function(){return x + u}})()", 2, 3);
  for (var j = 0; j < 5; ++j) {
    appendToActual(flat());
  }
 }


assertEq(actual, expected)
