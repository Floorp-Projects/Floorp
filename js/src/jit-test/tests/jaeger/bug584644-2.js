this.__defineSetter__("x",/a/)
Function("\
  for each(w in[0,0,0]) {\
    for each(y in[0,0,0,0,0,0,0,0,x,0,0,0,0,0,0,0,0,0,x,0,0,0,0,0,0,0,x]) {}\
  }\
")()

