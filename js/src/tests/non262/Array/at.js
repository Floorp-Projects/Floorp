function basic() {
  assertEq([0].at(0), 0);
  assertEq([0].at(-1), 0);

  assertEq([].at(0), undefined);
  assertEq([].at(-1), undefined);
  assertEq([].at(1), undefined);

  assertEq([0, 1].at(0), 0);
  assertEq([0, 1].at(1), 1);
  assertEq([0, 1].at(-2), 0);
  assertEq([0, 1].at(-1), 1);

  assertEq([0, 1].at(2), undefined);
  assertEq([0, 1].at(-3), undefined);
  assertEq([0, 1].at(-4), undefined);
  assertEq([0, 1].at(Infinity), undefined);
  assertEq([0, 1].at(-Infinity), undefined);
  assertEq([0, 1].at(NaN), 0); // ToInteger(NaN) = 0
}

function obj() {
  var o = {length: 0, [0]: "a", at: Array.prototype.at};

  assertEq(o.at(0), undefined);
  assertEq(o.at(-1), undefined);

  o.length = 1;
  assertEq(o.at(0), "a");
  assertEq(o.at(-1), "a");
  assertEq(o.at(1), undefined);
  assertEq(o.at(-2), undefined);
}

basic();
obj();

if (typeof reportCompare === "function")
    reportCompare(true, true);
