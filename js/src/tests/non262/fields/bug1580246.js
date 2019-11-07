// Don't Crash
var testStr = `
class C extends Object {
  constructor() {
    eval(\`a => b => {
      class Q { f = 1; }  // inhibits lazy parsing
      super();
    }\`);
  }
}
new C;`
assertEq(raisesException(ReferenceError)(testStr), true);

reportCompare(true, true);
