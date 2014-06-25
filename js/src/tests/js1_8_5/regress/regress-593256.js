/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

eval("\
  (function(){for(d in[0,Number]) {\
    this.__defineGetter__(\"\",function(){}),\
    [(this.__defineGetter__(\"x\",Math.pow))]\
  }})\
")()
delete gc
eval("\
  (function() {\
    for(e in this.__defineSetter__(\"x\",function(){})){}\
  })\
")()
delete gc

reportCompare(true, true, "don't crash");
