/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'array-pop-proxy.js';
var BUGNUMBER = 858381;
var summary = "Behavior of [].pop on proxies";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var p = new Proxy([0, 1, 2], {});
Array.prototype.pop.call(p);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
