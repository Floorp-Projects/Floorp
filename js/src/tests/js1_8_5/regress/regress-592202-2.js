/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
eval("\
  let(b)((\
    function(){\
      let(d=b)\
      ((function(){\
        b=b\
      })())\
    }\
  )())\
")
reportCompare(0, 0, "ok");
