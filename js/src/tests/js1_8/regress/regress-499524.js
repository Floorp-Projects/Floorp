/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function isSyntaxError(code) {
  try {
    eval(code);
    return false;
  } catch (exception) {
    if (SyntaxError.prototype.isPrototypeOf(exception))
      return true;
    throw exception;
  };
};

/* 
 * Duplicate parameter names must be tolerated (as per ES3), unless
 * the parameter list uses destructuring, in which case we claim the
 * user has opted in to a modicum of sanity, and we forbid duplicate
 * parameter names.
 */
assertEq(isSyntaxError("function f(x,x){}"),                false);

assertEq(isSyntaxError("function f(x,[x]){})"),             true);
assertEq(isSyntaxError("function f(x,{y:x}){})"),           true);
assertEq(isSyntaxError("function f(x,{x}){})"),             true);

assertEq(isSyntaxError("function f([x],x){})"),             true);
assertEq(isSyntaxError("function f({y:x},x){})"),           true);
assertEq(isSyntaxError("function f({x},x){})"),             true);

assertEq(isSyntaxError("function f([x,x]){}"),              true);
assertEq(isSyntaxError("function f({x,x}){}"),              true);
assertEq(isSyntaxError("function f({y:x,z:x}){}"),          true);

assertEq(isSyntaxError("function f(x,x,[y]){}"),            true);
assertEq(isSyntaxError("function f(x,x,{y}){}"),            true);
assertEq(isSyntaxError("function f([y],x,x){}"),            true);
assertEq(isSyntaxError("function f({y},x,x){}"),            true);

assertEq(isSyntaxError("function f(a,b,c,d,e,f,g,h,b,[y]){}"),  true);
assertEq(isSyntaxError("function f([y],a,b,c,d,e,f,g,h,a){}"),  true);
assertEq(isSyntaxError("function f([a],b,c,d,e,f,g,h,i,a){}"),  true);
assertEq(isSyntaxError("function f(a,b,c,d,e,f,g,h,i,[a]){}"),  true);
assertEq(isSyntaxError("function f(a,b,c,d,e,f,g,h,i,[a]){}"),  true);

reportCompare(true, true);
