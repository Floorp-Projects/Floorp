(() => {
  let returnCalled = false;

  function foo() {
    ({}).__proto__.return = () => {
      returnCalled = true;
      return { value: 3, done: true };
    };
    return 2;
  }

  assertEq(returnCalled, false);
  let [a,[b=foo()]] = [1,[],3];
  assertEq(returnCalled, true);
  assertEq(a, 1);
  assertEq(b, 2);
})();
