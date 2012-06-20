function loop(actual = 0)  {
  if (function() { actual++ })
  {}
  return actual;
}

assertEq(loop(), 0);
