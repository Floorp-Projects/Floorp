
load("verify.js");

class C {
virtual var x:Integer;
var y:Integer;
}

class D extends C {
override function set x(a:Integer):Integer {return y = a*2}
}


  var c:C = new C;
  c.x = 5;

verify(c.x, 5.0);
verify(c.y, NaN);

  var d:D = new D;
  d.x = 5;

verify(d.x, NaN);
verify(d.y, 10.0);

