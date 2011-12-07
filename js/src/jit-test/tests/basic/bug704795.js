Function("\
  gczeal(4,false);\
  function f(){\
    \"use strict\";\
    this.x = Object\
  }\
  for each(y in[0,0]){\
    try{\
      new f\
    }\
    catch(e){}\
  }\
")()