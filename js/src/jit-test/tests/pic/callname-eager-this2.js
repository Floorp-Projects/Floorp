this.name = "outer";

var sb = evalcx('');
sb.name = "inner";
sb.parent = this;

var res = 0;

function f() {
    assertEq(this.name, "outer");
    res++;
}

// ff is a property of the inner global object. Generate a CALLNAME IC, then
// change ff to a function on the outer global. It should get the inner this
// value.
evalcx('this.ff = function() {};' +
      '(function() { ' +
           'eval("");' +
           'for(var i=0; i<10; i++) {' +
               'ff();' +
               'if (i == 5) ff = parent.f;' +
           '}' +
      '})()', sb);
assertEq(res, 4);
