(() => {
  ({}).__proto__[1] = 2;
  let [x,y] = [1];

  assertEq(x, 1);
  assertEq(y, undefined);
})();
