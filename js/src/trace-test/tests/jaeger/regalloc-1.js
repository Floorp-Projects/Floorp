// |trace-test| error: TypeError

x = 2;

function tryItOut(c) {
  return eval("(function(){" + c + "})");
}

function doit() {
  var f = tryItOut("((( \"\" \n for each (eval in [null, this, null, this, (1/0), new String('q'), new String('q'), null, null, null, new String('q'), new String('q'), new String('q'), null]) if (this)).eval(x = x)));");
  f();
}

doit();

