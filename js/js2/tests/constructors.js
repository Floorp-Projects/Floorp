load("verify.js");

class C {
        var a:String;

        constructor C(p:String) {this.a = "New "+p}
        constructor make(p:String) {this.a = "Make "+p}
        static function obtain(p:String):C {return new C(p)}
      }

var c:C = new C("one");
var d:C = C.C("two");
var e:C = C.make("three");
var f:C = C.obtain("four");

verify( c.a, "New one" );
verify( d.a, "New two" );
verify( e.a, "Make three" );
verify( f.a, "New four" );



