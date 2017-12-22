// |reftest| skip
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Gary Kwong <gary@rumblingedge.com>

Object.create(evalcx('')).__defineSetter__('toString', function(){});
reportCompare(0, 0, "ok");
