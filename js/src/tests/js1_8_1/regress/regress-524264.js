/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */
uneval(function () { do yield; while (0); });
reportCompare("no assertion failure", "no assertion failure", "bug 524264");
