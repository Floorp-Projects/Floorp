function basic() {
  assertEq([0].item(0), 0);
  assertEq([0].item(-1), 0);

  assertEq([].item(0), undefined);
  assertEq([].item(-1), undefined);
  assertEq([].item(1), undefined);

  assertEq([0, 1].item(0), 0);
  assertEq([0, 1].item(1), 1);
  assertEq([0, 1].item(-2), 0);
  assertEq([0, 1].item(-1), 1);

  assertEq([0, 1].item(2), undefined);
  assertEq([0, 1].item(-3), undefined);
  assertEq([0, 1].item(-4), undefined);
  assertEq([0, 1].item(Infinity), undefined);
  assertEq([0, 1].item(-Infinity), undefined);
  assertEq([0, 1].item(NaN), 0); // ToInteger(NaN) = 0
}

function obj() {
  var o = {length: 0, [0]: "a", item: Array.prototype.item};

  assertEq(o.item(0), undefined);
  assertEq(o.item(-1), undefined);

  o.length = 1;
  assertEq(o.item(0), "a");
  assertEq(o.item(-1), "a");
  assertEq(o.item(1), undefined);
  assertEq(o.item(-2), undefined);
}

basic();
obj();

if (typeof reportCompare === "function")
    reportCompare(true, true);
