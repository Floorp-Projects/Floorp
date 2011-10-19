try
{
  Array.prototype.splice.call({ get length() { throw 'error'; } });
  throw new Error("should have thrown, didn't");
}
catch (e)
{
  assertEq(e, "error", "wrong error thrown: " + e);
}
