// |jit-test| need-for-each

Function("\n\
  for each(y in[0,0,0]) {\n\
    for(x in[0,0,0,0,0,0,0,0,0,new Boolean(true),0,0,0,new Boolean(true)]) {}\n\
  }\n\
")()

