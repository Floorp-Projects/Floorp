for (a in (eval("\
  function() {\
    return function() {\
      yield ((function() {\
        return d\
      })())\
    } ();\
  var d = []\
  }\
"))());
