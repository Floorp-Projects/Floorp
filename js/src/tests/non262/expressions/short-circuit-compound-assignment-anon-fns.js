// NamedEvaluation applies to short-circuit assignment.

{
  let a;
  a ??= function(){};
  assertEq(a.name, "a");
}

{
  let a = false;
  a ||= function(){};
  assertEq(a.name, "a");
}

{
  let a = true;
  a &&= function(){};
  assertEq(a.name, "a");
}

// No name assignments for parenthesised left-hand sides.

{
  let a;
  (a) ??= function(){};
  assertEq(a.name, "");
}

{
  let a = false;
  (a) ||= function(){};
  assertEq(a.name, "");
}

{
  let a = true;
  (a) &&= function(){};
  assertEq(a.name, "");
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
