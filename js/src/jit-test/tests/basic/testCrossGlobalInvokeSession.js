otherGlobal = newGlobal("same-compartment");
otherGlobal.poison = Proxy.create({});
callee = new otherGlobal.Function("return this.poison;");

var caught = false;
try {
    [1,2,3,4,5,6,7,8].sort(callee);
} catch(e) {
    assertEq(e instanceof Error, true);
    caught = true;
}
assertEq(caught, true);
