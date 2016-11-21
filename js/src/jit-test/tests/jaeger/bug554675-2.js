// |jit-test| need-for-each

function a(code) {
  var f = new Function("for each(let x in[false,'',/x/,'',{}]){if(x<x){(({}))}else if(x){}else{}}");
  try {
    f()
  } catch(e) {}
}
a()
a()

/* Don't crash (CLI only). */

