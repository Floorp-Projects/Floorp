function globalPrototypeChainIsMutable()
{
  if (typeof immutablePrototypesEnabled !== "function")
    return true;

  return !immutablePrototypesEnabled();
}
