function f(x,y,z) {
  z = 9;
  delete arguments[2];
  assertEq(arguments[2], undefined);
  o = arguments;
  assertEq(o[2], undefined);
  assertEq(o[2] == undefined, true);
}

for (var i = 0; i < HOTLOOP+2; ++i) {
    print(i);
    f(1,2,3)
}
