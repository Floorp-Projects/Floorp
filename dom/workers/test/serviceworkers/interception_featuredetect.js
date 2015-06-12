// Only succeeds if onfetch is available.
if (!("onfetch" in self)) {
  throw new Error("Not capable of interception");
}
