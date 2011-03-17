this.__proto__ = null;

function testLenientAndStrict(code, lenient_pred, strict_pred) {
  return (strict_pred("'use strict'; " + code) && 
          lenient_pred(code));
}
function raisesException(exception) {
  return function (code) {
    try {
      eval(code);
    } catch (actual) {
    }
  };
};
try {
function arr() {
  return Object.defineProperty(Object()* delete Object, 0, {writable: false});
}
assertEq(testLenientAndStrict('var a = arr(); [a.splice(0, 1), a]',
                              raisesException(TypeError),
                              raisesException(TypeError)),
         true);
} catch (e) {}
ForIn_2(this);
function ForIn_2(object) {
  for ( property in object ) {
    with ( object ) {
    }
  }
}
