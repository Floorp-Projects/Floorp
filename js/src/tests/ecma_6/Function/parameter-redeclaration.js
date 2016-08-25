// 'var' is allowed to redeclare parameters.
function f1(a = 0) {
  var a;
}

// 'let' and 'const' at body-level are not allowed to redeclare parameters.
assertThrowsInstanceOf(() => {
  eval(`function f2(a = 0) {
    let a;
  }`);
}, SyntaxError);
assertThrowsInstanceOf(() => {
  eval(`function f3(a = 0) {
    const a;
  }`);
}, SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
