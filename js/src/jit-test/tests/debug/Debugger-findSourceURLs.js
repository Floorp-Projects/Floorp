// findSourceURLs should return all URLs compiled in debuggee realms,
// except when a shrinking GC has occurred.

let g = newGlobal({newCompartment: true});
let startNumber = gcparam("gcNumber");
g.evaluate("function foo() {}", { fileName: "foo.js" });
g.evaluate("function bar() {}", { fileName: "bar.js" });
g.evaluate("function baz() {}", { fileName: "baz.js" });

let dbg = new Debugger(g);
let urls = dbg.findSourceURLs();

let endNumber = gcparam("gcNumber");
if (startNumber == endNumber) {
  assertEq(urls.includes("foo.js"), true);
  assertEq(urls.includes("bar.js"), true);
  assertEq(urls.includes("baz.js"), true);
}
