load(libdir + 'bytecode-cache.js');

var test = `
function f() {
  function f1() {
    return 1;
  }
  function f2() {
    return 20;
  }
  return f1() + f2();
}
function g() {
  return 300;
}
f() + g();
`;
evalWithCache(test, { assertEqBytecode: true, assertEqResult : true,
                      forceFullParse: true, });
