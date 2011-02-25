/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
i = 42
eval("let(y){(function(){let({}=y){(function(){let({}=y=[])(i)})()}})()}")
reportCompare(0, 0, "ok");
