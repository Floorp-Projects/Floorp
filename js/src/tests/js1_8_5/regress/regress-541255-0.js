/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Gary Kwong
 */
(function(e) {
  eval("\
    [(function() {\
      x.k = function(){}\
    })() \
    for (x in [0])]\
  ")
})();
reportCompare(0, 0, "");
