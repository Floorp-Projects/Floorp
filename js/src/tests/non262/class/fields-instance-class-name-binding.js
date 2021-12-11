// Instance field initialisers can access the inner name binding for class definitions.
{
  class C {
    field = C;
  }
  assertEq(new C().field, C);
}
{
  let C = class Inner {
    field = Inner;
  };
  assertEq(new C().field, C);
}

// Instance field initialiser expressions always resolve the inner name binding.
{
  class C {
    field = () => C;
  }
  assertEq(new C().field(), C);

  const D = C;
  C = null;

  assertEq(new D().field(), D);
}
{
  let C = class Inner {
    field = () => Inner;
  }
  assertEq(new C().field(), C);

  const D = C;
  C = null;

  assertEq(new D().field(), D);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
