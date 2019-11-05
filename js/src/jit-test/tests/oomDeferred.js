// |jit-test| --no-ion; skip-if: !('oomTest' in this)

// Ion is too slow executing this test, hence the --no-ion above

var test = () => {
  var ng = newGlobal({deferredParserAlloc: true});

  function foo() {
    var x = /hi/;
    var y = 128n;
    function inner(a, b) {
      return a + b;
    }
    var a = inner(y, y);
    return x.exec(a.toString());
  }

  ng.evaluate(foo.toString());
  ng.evaluate('foo()');
};
test();
oomTest(test);
