function run_test() {
let sandbox1 = new Cu.Sandbox(null);
let sandbox2 = new Cu.Sandbox(null);
let arg = Cu.evalInSandbox('({ buf: new ArrayBuffer(2) })', sandbox1);

let clonedArg = Cu.cloneInto(Cu.waiveXrays(arg), sandbox2);
Assert.equal(typeof Cu.waiveXrays(clonedArg).buf, "object");

clonedArg = Cu.cloneInto(arg, sandbox2);
Assert.equal(typeof Cu.waiveXrays(clonedArg).buf, "object");
Assert.equal(Cu.waiveXrays(clonedArg).buf.constructor.name, "ArrayBuffer");
}
