for (a of (eval("\
  (function() {\
    return function*() {\
      yield ((function() {\
        return d\
      })())\
    } ();\
  var d = []\
  })\
"))());
