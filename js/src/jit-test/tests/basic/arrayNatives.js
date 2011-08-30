
/* Array natives applied to non-arrays. */

var oa = {};
Array.pop(oa);
assertEq(oa.length, 0);

var ob = {};
Array.push(ob, "twelve");
assertEq(ob.length, 1);

var oc = {};
Array.shift(oc);
assertEq(oc.length, 0);

var od = {};
Array.unshift(od, "eight");
assertEq(od.length, 1);
