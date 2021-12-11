let g = newGlobal({newCompartment: true});
let dbg = new Debugger();
let gw = dbg.addDebuggee(g);

g.error1 = new Error()
g.error2 = new g.Error()
g.error3 = new g.TypeError();

let error1DO = gw.getOwnPropertyDescriptor('error1').value;
let error2DO = gw.getOwnPropertyDescriptor('error2').value;
let error3DO = gw.getOwnPropertyDescriptor('error3').value;

assertEq(error1DO.isError, true);
assertEq(error2DO.isError, true);
assertEq(error3DO.isError, true);

g.nonError = new Array();
let nonErrorDO = gw.getOwnPropertyDescriptor('nonError').value;
assertEq(nonErrorDO.isError, false);
