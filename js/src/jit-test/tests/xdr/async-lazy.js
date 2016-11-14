async function f1(a, b) {
  let x = await 10;
  return x;
};
var toStringResult = f1.toString();

async function f2(a, b) {
  // arguments.callee gets wrapped function from unwrapped function.
  return arguments.callee;
};

relazifyFunctions();

// toString gets unwrapped function from wrapped function.
assertEq(f1.toString(), toStringResult);

var ans = 0;
f1().then(x => { ans = x; });
drainJobQueue();
assertEq(ans, 10);

f2().then(x => { ans = x; });
drainJobQueue();
assertEq(ans, f2);
