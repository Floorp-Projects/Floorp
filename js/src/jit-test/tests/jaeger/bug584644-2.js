// |jit-test| need-for-each

this.__defineSetter__("x", function(v) { return /a/.exec(v); })
Function("\
  for each(w in[0,0,0]) {\
    for each(y in[0,0,0,0,0,0,0,0,x,0,0,0,0,0,0,0,0,0,x,0,0,0,0,0,0,0,x]) {}\
  }\
")()

