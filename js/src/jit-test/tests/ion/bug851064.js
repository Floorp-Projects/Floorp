var base = "azertyuiopqsdfghjklmwxcvbn";
function createRopedString() {
  var test = "";
  for (var i=0; i<2; i++) {
    test += base;
  }
  return test;
}

assertEq(createRopedString().substr(0,10), base.substr(0,10));
assertEq(createRopedString().substr(0,26), base.substr(0,26));
assertEq(createRopedString().substr(26,10), base.substr(0,10));
assertEq(createRopedString().substr(24,10), base.substr(24,2) + base.substr(0,8));
