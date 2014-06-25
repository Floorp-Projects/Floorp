/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var a = {d: true, w: true};
Object.defineProperty(a, "d", {set: undefined});
delete a.d;
delete a.w;
a.d = true;

reportCompare(0, 0, "ok");
