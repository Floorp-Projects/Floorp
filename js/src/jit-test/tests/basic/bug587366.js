// Test flat string replacement, per ECMAScriptv5 15.5.4.11.
assertEq("1+2".replace("1+2", "$&+3"), "1+2+3");
assertEq(")".replace(")","*$&*"), "*)*");
