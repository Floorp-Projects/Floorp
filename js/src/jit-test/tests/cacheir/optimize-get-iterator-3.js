(() => {
  let nextCalled = 0;
  ([])[Symbol.iterator]().__proto__.next = () => {
    nextCalled++;
    return {value: nextCalled, done: false};
  };

  assertEq(nextCalled, 0);
  let [a,b] = [1,2,3];
  assertEq(nextCalled, 2);
  assertEq(a, 1);
  assertEq(b, 2);
})();
