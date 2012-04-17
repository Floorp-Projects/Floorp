// |reftest| skip
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Gary Kwong and Jason Orendorff
 */
var obj = {};
obj.__defineSetter__("x", function() {});
obj.watch("x", function() {});
obj.__defineSetter__("x", /a/);
