const g = newGlobal({ newCompartment: true });
const dbg = Debugger();
const gdbg = dbg.addDebuggee(g);

const nonASCIIStr = "\u00FF\u0080";

const source = gdbg.createSource({
  url: nonASCIIStr,
});
assertEq(source.url, nonASCIIStr);
