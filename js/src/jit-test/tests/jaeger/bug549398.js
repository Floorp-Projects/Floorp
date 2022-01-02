(function () {
    eval("\
      for(var z = 0 ; z < 2 ; ++z) {\
        this\
      }\
      ", ([1,2,3]))
})()

/* Don't crash. */

