// |reftest| skip-if(release_or_beta)

// Test assignment to const and function name bindings. The latter is kind of a
// const binding, but only throws in strict mode.

function notEvaluated() {
  throw new Error("should not be evaluated");
}

// AndAssignExpr with const lexical binding.
{
  const a = false;
  a &&= notEvaluated();
  assertEq(a, false);

  const b = true;
  assertThrowsInstanceOf(() => { b &&= 1; }, TypeError);
  assertEq(b, true);
}

// AndAssignExpr with function name binding.
{
  let f = function fn() {
    fn &&= true;
    assertEq(fn, f);
  };
  f();

  let g = function fn() {
    "use strict";
    assertThrowsInstanceOf(() => { fn &&= 1; }, TypeError);
    assertEq(fn, g);
  };
  g();
}

// OrAssignExpr with const lexical binding.
{
  const a = true;
  a ||= notEvaluated();
  assertEq(a, true);

  const b = false;
  assertThrowsInstanceOf(() => { b ||= 0; }, TypeError);
  assertEq(b, false);
}

// OrAssignExpr with function name binding.
{
  let f = function fn() {
    fn ||= notEvaluated();
    assertEq(fn, f);
  };
  f();

  let g = function fn() {
    "use strict";
    fn ||= notEvaluated();
    assertEq(fn, g);
  };
  g();
}

// CoalesceAssignExpr with const lexical binding.
{
  const a = true;
  a ??= notEvaluated();
  assertEq(a, true);

  const b = null;
  assertThrowsInstanceOf(() => { b ??= 0; }, TypeError);
  assertEq(b, null);
}

// CoalesceAssignExpr with function name binding.
{
  let f = function fn() {
    fn ??= notEvaluated();
    assertEq(fn, f);
  };
  f();

  let g = function fn() {
    "use strict";
    fn ??= notEvaluated();
    assertEq(fn, g);
  };
  g();
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
