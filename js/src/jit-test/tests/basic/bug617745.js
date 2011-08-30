
var array1 = ['0'];
var array2 = (new Array(1)).splice(0,0, array1);
assertEq("" + array2, "");
