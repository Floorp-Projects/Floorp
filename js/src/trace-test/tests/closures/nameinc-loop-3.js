actual = '';
expected = 'g 1 0,g 2 -1,g 3 -2,g 4 -3,g 5 -4,h 5 -5,f 5,undefined,g 1 0,g 2 -1,g 3 -2,g 4 -3,g 5 -4,h 5 -5,f 5,undefined,g 1 0,g 2 -1,g 3 -2,g 4 -3,g 5 -4,h 5 -5,f 5,undefined,g 1 0,g 2 -1,g 3 -2,g 4 -3,g 5 -4,h 5 -5,f 5,undefined,g 1 0,g 2 -1,g 3 -2,g 4 -3,g 5 -4,h 5 -5,f 5,undefined,';

var f = function() {
  var p = 0;

  function h() {
    var q = 0;

    function g() {
      for (var i = 0; i < 5; ++i) {
	p++;
	appendToActual('g ' + p + ' ' + q);
	q--;
      }
    }
    g();
    appendToActual('h ' + p + ' ' + q);
  }
  
  h();
  
  appendToActual('f ' + p);
}

for (var i = 0; i < 5; ++i) {
  f();
  appendToActual();
}



assertEq(actual, expected)
