globalName = 907;
var globalInt = 0;
for (var i = 0; i < 500; i++)
  globalInt = globalName + i;

assertEq(globalInt, globalName + 499);
