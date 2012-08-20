function runTest() {
  if (Math) {
    function createTester(options) {
      return function() {
        return options.blah;
      };
    }

    return createTester({blah:"bar"});
  }
}

assertEq(runTest()(), "bar");
