(() => {
  let returnCalled = false;
  ({}).__proto__.return = () => {
    returnCalled = true;
    return { value: 3, done: true };
  };

  assertEq(returnCalled, false);
  let [a,b] = [1,2,3];
  assertEq(returnCalled, true);
  assertEq(a, 1);
  assertEq(b, 2);
})();
