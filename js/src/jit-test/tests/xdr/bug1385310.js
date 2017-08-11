load(libdir + 'bytecode-cache.js');
var test = "";

// The main test function.
test = `
var test = function (isContent) {
  // Returns generator object that iterates through pref values.
  let prefVals = (for (prefVal of [false, true]) prefVal);

  for (x of prefVals) ;
}

test()
`;
evalWithCache(test, {
  incremental: true,
  assertEqBytecode: true,
  assertEqResult : true
});
