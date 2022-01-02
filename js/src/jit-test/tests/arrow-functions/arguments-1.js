// no 'arguments' binding in arrow functions

var arguments = [];
var f = () => arguments;
assertEq(f(), arguments);
