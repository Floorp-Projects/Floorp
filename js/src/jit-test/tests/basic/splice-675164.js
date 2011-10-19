function NPList() {}
NPList.prototype = new Array;

var list = new NPList();
list.push('a');

var cut = list.splice(0, 1);

assertEq(cut[0], 'a');
assertEq(cut.length, 1);
assertEq(list.length, 0);

var desc = Object.getOwnPropertyDescriptor(list, "0");
assertEq(desc, undefined);
assertEq("0" in list, false);
