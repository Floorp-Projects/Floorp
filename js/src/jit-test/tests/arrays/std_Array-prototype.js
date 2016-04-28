Object.prototype.prototype = {};
assertEq(Object.getPrototypeOf([].concat()), Array.prototype);
assertEq(Object.getPrototypeOf([].map(x => x)), Array.prototype);
assertEq(Object.getPrototypeOf([].filter(x => x)), Array.prototype);
assertEq(Object.getPrototypeOf([].slice()), Array.prototype);
assertEq(Object.getPrototypeOf([].splice()), Array.prototype);
