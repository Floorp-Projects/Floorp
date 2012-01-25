/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Gary Kwong
 */

function f(e) {
  eval("\
    [((function g(o, bbbbbb) {\
      if (aaaaaa = bbbbbb) {\
        return window.r = []\
      }\
      g(aaaaaa, bbbbbb + 1);\
      ({})\
    })([], 0)) \
    for (window in this) \
    for each(x in [0, 0])\
    ]\
  ")
}
t = 1;
f();
reportCompare(0, 0, "");
