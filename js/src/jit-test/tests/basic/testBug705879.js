f = eval("\
  (function() {\
    with({}) {\
      yield\
    }\
    for(let d in[gc()])\
    for(b in[0,function(){}]);\
  })\
")
for (e in f()) {}
