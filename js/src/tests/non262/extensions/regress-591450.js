/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * This was causing the parser to assert at one point. Now it's not. Yay!
 */
function f(a,[x,y],b,[w,z],c) { function b() { } }

reportCompare(0, 0, "don't crash");
