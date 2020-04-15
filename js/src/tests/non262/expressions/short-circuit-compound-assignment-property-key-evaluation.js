// |reftest| skip-if(release_or_beta)

// Test that property keys are only evaluated once.

class PropertyKey {
  constructor(key) {
    this.key = key;
    this.count = 0;
  }

  toString() {
    this.count++;
    return this.key;
  }

  valueOf() {
    throw new Error("unexpected valueOf call");
  }
}

// AndAssignExpr
{
  let obj = {p: true};
  let pk = new PropertyKey("p");

  obj[pk] &&= false;

  assertEq(obj.p, false);
  assertEq(pk.count, 1);

  obj[pk] &&= true;

  assertEq(obj.p, false);
  assertEq(pk.count, 2);
}

// OrAssignExpr
{
  let obj = {p: false};
  let pk = new PropertyKey("p");

  obj[pk] ||= true;

  assertEq(obj.p, true);
  assertEq(pk.count, 1);

  obj[pk] ||= false;

  assertEq(obj.p, true);
  assertEq(pk.count, 2);
}

// CoalesceAssignExpr
{
  let obj = {p: null};
  let pk = new PropertyKey("p");

  obj[pk] ??= true;

  assertEq(obj.p, true);
  assertEq(pk.count, 1);

  obj[pk] ??= false;

  assertEq(obj.p, true);
  assertEq(pk.count, 2);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
