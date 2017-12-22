var BUGNUMBER = 1322035;
var summary = 'RegExp.prototype.test should update lastIndex to correct position even if pattern starts with .*';

print(BUGNUMBER + ": " + summary);

var regExp = /.*x?/g;
regExp.test('12345');
assertEq(regExp.lastIndex, 5);

regExp = /.*x*/g;
regExp.test('12345');
assertEq(regExp.lastIndex, 5);

regExp = /.*()/g;
regExp.test('12345');
assertEq(regExp.lastIndex, 5);

regExp = /.*(x|)/g;
regExp.test('12345');
assertEq(regExp.lastIndex, 5);

if (typeof reportCompare === "function")
  reportCompare(true, true);
