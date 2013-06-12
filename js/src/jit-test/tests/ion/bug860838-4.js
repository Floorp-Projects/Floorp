function DiagModule(stdlib, foreign) {
    "use asm";

    var sqrt = stdlib.Math.sqrt;
    var test = foreign.test;
    var foo = foreign.foo;

    function square(x) {
        x = x|0;
        return ((x|0)+(x|0))|0;
    }

    function diag() {
        var x = 0.0;
        while(1) {
          test(1, x);
          x = x+1.0
          if (x > 15.0)
            return 0;
        }
        return 0;
    }

    function diag_1() {
        foo();
        return 0;
    }


    return { diag: diag, diag_1:diag_1 };
}

var foreign = {
  test:function(a,b) {
    print(a+":"+b)
    var c = [0.0];
    if (b > 10)
        return c[1];
    return c[0];
  },
  foo:function() {
    with({}) {};
    gc();
  }
}

// make sure foreign is compiled

var fast = DiagModule(this, foreign);     // produces AOT-compiled version
print(fast.diag_1());      // 5
gc()
print(fast.diag());      // 5


