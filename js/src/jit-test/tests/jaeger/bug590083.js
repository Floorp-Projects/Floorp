eval("\
  (function(){\
    for(var w in [0]) {\
     function w(){}\
     print(w)\
    }\
  })\
")()

/* Don't crash. */

