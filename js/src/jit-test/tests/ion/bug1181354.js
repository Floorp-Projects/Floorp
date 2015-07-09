for (a of []) {}
var log = "";
(function() {
  for (a of [,0]) {}
  const y = "FOO";
  log += y;
  function inner() { log += y; }
})()
assertEq(log, "FOO");
