var BUGNUMBER = 1185106;
var summary = "async name token in property and object destructuring pattern";

print(BUGNUMBER + ": " + summary);

{
  let a = { async: 10 };
  assertEq(a.async, 10);
}

{
  let a = { async() {} };
  assertEq(a.async instanceof Function, true);
  assertEq(a.async.name, "async");
}

{
  let async = 11;
  let a = { async };
  assertEq(a.async, 11);
}

{
  let { async } = { async: 12 };
  assertEq(async, 12);
}

{
  let { async = 13 } = {};
  assertEq(async, 13);
}

{
  let { async: a = 14 } = {};
  assertEq(a, 14);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
