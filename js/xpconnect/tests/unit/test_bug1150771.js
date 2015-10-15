function run_test() {
let Cu = Components.utils;
let sandbox1 = new Cu.Sandbox(null);
let sandbox2 = new Cu.Sandbox(null);
let arg = Cu.evalInSandbox('({ buf: new ArrayBuffer(2) })', sandbox1);

let clonedArg = Cu.cloneInto(Cu.waiveXrays(arg), sandbox2);
do_check_eq(typeof Cu.waiveXrays(clonedArg).buf, "object");

clonedArg = Cu.cloneInto(arg, sandbox2);
do_check_eq(typeof Cu.waiveXrays(clonedArg).buf, "object");
do_check_eq(Cu.waiveXrays(clonedArg).buf.constructor.name, "ArrayBuffer");
}
