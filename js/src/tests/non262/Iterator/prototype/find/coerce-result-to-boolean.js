// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

const fn = (value) => value;
assertEq([true].values().find(fn), true);
assertEq([1].values().find(fn), 1);
assertEq(['test'].values().find(fn), 'test');

assertEq([false].values().find(fn), undefined);
assertEq([0].values().find(fn), undefined);
assertEq([''].values().find(fn), undefined);
assertEq([null].values().find(fn), undefined);
assertEq([undefined].values().find(fn), undefined);
assertEq([NaN].values().find(fn), undefined);
assertEq([-0].values().find(fn), undefined);
assertEq([0n].values().find(fn), undefined);

let array = [];
assertEq([array].values().find(fn), array);

let object = {};
assertEq([object].values().find(fn), object);

const htmlDDA = createIsHTMLDDA();
assertEq([htmlDDA].values().find(fn), undefined);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
