class C {
    x = function(){};
    0 = function(){};
    ["y" + 0] = function(){};
}

let c = new C();
assertEq(c["x"].name, "x");
assertEq(c[0].name, "0");
assertEq(c["y0"].name, "y0");

if (typeof reportCompare === "function")
  reportCompare(true, true);
