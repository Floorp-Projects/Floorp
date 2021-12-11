{
  function f() { return "inner"; }
}

function f() { return "outer"; }

reportCompare(f(), "inner");
