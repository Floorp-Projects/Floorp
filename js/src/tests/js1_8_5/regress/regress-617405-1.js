/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Christian Holler <decoder@own-hero.net> and Jason Orendorff
 */

function C(){}
C.prototype = 1;
assertEq(Object.getOwnPropertyDescriptor(C, "prototype").configurable, false);

reportCompare(0, 0, "ok");
