
load("verify.js");

class C {
        static var v = "Cv";
        static var x = "Cx";
        static var y = "Cy";
        static var z = "Cz";
      }
/*
      interface A {
        static var x = "Ax";
        static var i = "Ai";
        static var j = "Aj";
      }

      interface B {
        static var x = "Bx";
        static var y = "By";
        static var j = "Bj";
      }
*/
      class D extends C implements A, B {
        static var v = "Dv";
      }



verify( C.v, "Cv" );
verify( C.x, "Cx" );
verify( C.y, "Cy" );
verify( C.z, "Cz" );
/*
verify( A.x, "Ax" );
verify( B.y, "By" );
*/

verify( D.v, "Dv" );
verify( D.x, "Cx" );
verify( D.y, "Cy" );
verify( D.z, "Cz" );

/*
verify( D.i, "Ai" );
*/

// verify( D.j;    // error because of ambiguity: "Aj" or "Bj"?

/*
verify( D.A::j, "Aj" );
verify( D.B::j, "Bj" );
verify( D.A::x, "Ax" );
verify( D.A::i, "Ai" );
*/

D.x = 5;
verify( C.x, 5 );
C.v = 7;
verify( D.v, "Dv" );
verify( C.v, 7 );

