// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const fn = (value) => value;
assertEq([true].values().some(fn), true);
assertEq([1].values().some(fn), true);
assertEq([[]].values().some(fn), true);
assertEq([{}].values().some(fn), true);
assertEq(['test'].values().some(fn), true);

assertEq([false].values().some(fn), false);
assertEq([0].values().some(fn), false);
assertEq([''].values().some(fn), false);
assertEq([null].values().some(fn), false);
assertEq([undefined].values().some(fn), false);
assertEq([NaN].values().some(fn), false);
assertEq([-0].values().some(fn), false);
assertEq([0n].values().some(fn), false);

const htmlDDA = createIsHTMLDDA();
assertEq([htmlDDA].values().some(fn), false);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
