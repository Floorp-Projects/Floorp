// |reftest| skip
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Gary Kwong <gary@rumblingedge.com>

var expect = 42;
var actual = (function({
    x: [w]
},
x) {
    with({}) {return w;}
})({x:[42]});

reportCompare(expect, actual, "ok");
