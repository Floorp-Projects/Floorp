// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const fn = (value) => value;
assertEq([true].values().every(fn), true);
assertEq([1].values().every(fn), true);
assertEq([[]].values().every(fn), true);
assertEq([{}].values().every(fn), true);
assertEq(['test'].values().every(fn), true);

assertEq([false].values().every(fn), false);
assertEq([0].values().every(fn), false);
assertEq([''].values().every(fn), false);
assertEq([null].values().every(fn), false);
assertEq([undefined].values().every(fn), false);
assertEq([NaN].values().every(fn), false);
assertEq([-0].values().every(fn), false);
assertEq([0n].values().every(fn), false);

const htmlDDA = createIsHTMLDDA();
assertEq([htmlDDA].values().every(fn), false);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
