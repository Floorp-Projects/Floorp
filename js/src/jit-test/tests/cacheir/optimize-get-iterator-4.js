(() => {
  let iterablesBase = [
    [1,2],
    [1,2,3],
    [1,2,3],
    [3,2,1],
  ];

  let iterables = [];
  for (let i = 0; i < 1000; i++) {
    iterables.push([...iterablesBase[i % iterablesBase.length]]);
  }

  iterables.push(new Map([[1, 3], [2,4]]).keys());

  function testDestructuringInitialization(a) {
    let [x,y] = a;
    return y;
  }

  function testDestructuringAssignment(a) {
    let x, y;
    [x,y] = a;
    return y;
  }

  for (let i = 0; i < iterables.length; i++) {
    assertEq(testDestructuringInitialization(iterables[i]), 2);
  }

  // refresh the last iterator
  iterables[iterables.length - 1] = new Map([[1, 3], [2,4]]).keys();
  for (let i = 0; i < iterables.length; i++) {
    assertEq(testDestructuringAssignment(iterables[i]), 2);
  }
})();
