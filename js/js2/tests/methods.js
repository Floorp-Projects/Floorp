
load("verify.js");

class C {
        var x:Integer = 3;
        function m() {return x}
        function n(x) {return x+4}
      }

      var c:C = new C;
verify( c.m(), 3 );                  // returns 3
verify( c.n(7), 11 );                // returns 11
      var f:Function = c.m;  // f is a zero-argument function with this bound to c
verify( f(), 3 );                    // returns 3
      c.x = 8;
verify( f(), 8 );                    // returns 8