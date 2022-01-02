// Arrow functions do not have a .prototype property.

assertEq("prototype" in (a => a), false);
assertEq("prototype" in (() => {}), false);
