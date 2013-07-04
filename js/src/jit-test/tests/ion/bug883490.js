//|jit-test| error: TypeError
function coerceForeign(stdlib, foreign)
{
    "use asm";

    var g = foreign.g;
    var h = foreign.h;

    function f() {
        +g(0);
        +g(1);
        +g(2);
        +h(2);
        +h(3);
    }

    return f;
}
function blaat() {

}

var t = coerceForeign(undefined, {
  g: function(a) {
    if (a == 2)
      var blaat = new blaat();
  },
  h: function(b) {
    print(b);
  }
})

t();
