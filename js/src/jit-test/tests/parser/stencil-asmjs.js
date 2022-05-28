// |jit-test| skip-if: isLcovEnabled()

let source = `
  var m = function() {
    "use asm"
    function g(){}
    return g;
  }

  function check() {
    var objM = new m;
    var g = m();
    // g is a ctor returning an primitive value, thus an empty object
    assertEq(Object.getOwnPropertyNames(new g).length, 0);
  }

  check()
`;

// Check that on-demand delazification and concurrent delazification are not
// attempting to parse "use asm" functions.
const options = {
    fileName: "tests/asm.js/testBug999790.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "CheckConcurrentWithOnDemand",
    newContext: true,
};
evaluate(source, options);
