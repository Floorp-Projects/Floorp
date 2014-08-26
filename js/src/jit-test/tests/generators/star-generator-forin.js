load(libdir + "eqArrayHelper.js");

var iter;
var hit;

hit = 0;
iter = function*(){ hit++; };
assertEqArray([x for (x in iter)],
              []);
assertEqArray([x for each (x in iter)],
              []);
assertEq(hit, 0);

hit = 0;
iter = function*(){ hit++; };
iter["foo"] = "bar";
assertEqArray([x for (x in iter)],
              ["foo"]);
assertEqArray([x for each (x in iter)],
              ["bar"]);
assertEq(hit, 0);

hit = 0;
iter = function*(){ hit++; }();
assertEqArray([x for (x in iter)],
              []);
assertEqArray([x for each (x in iter)],
              []);
assertEq(hit, 0);

hit = 0;
iter = function*(){ hit++; }();
iter["foo"] = "bar";
assertEqArray([x for (x in iter)],
              ["foo"]);
assertEqArray([x for each (x in iter)],
              ["bar"]);
assertEq(hit, 0);

hit = 0;
iter = function*(){ hit++; }();
iter["foo"] = 10;
iter["bar"] = 20;
assertEqArray([x for (x in iter)].sort(),
              ["bar", "foo"]);
assertEqArray([x for each (x in iter)].sort(),
              [10, 20]);
assertEq(hit, 0);
