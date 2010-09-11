/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

eval("\
  (function(){for(d in[0,Number]) {\
    __defineGetter__(\"\",function(){}),\
    [(__defineGetter__(\"x\",Math.pow))]\
  }})\
")()
delete gc
eval("\
  (function() {\
    for(e in __defineSetter__(\"x\",function(){})){}\
  })\
")()
delete gc

reportCompare(true, true, "don't crash");
