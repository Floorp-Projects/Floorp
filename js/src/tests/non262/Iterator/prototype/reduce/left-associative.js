// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally

assertEq([1, 2, 3].values().reduce((x, y) => `(${x}+${y})`, 0), '(((0+1)+2)+3)');
assertEq([1, 2, 3].values().reduce((x, y) => `(${x}+${y})`), '((1+2)+3)');

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
