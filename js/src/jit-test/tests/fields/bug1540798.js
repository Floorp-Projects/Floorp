try { evaluate(`
class constructor  { get;                                           } // Long line is long
// Long line XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
test();
function test() {
    try {
      x;
    } catch (v) {
      gc();
    }
}
test();
`); } catch(exc) {}
constructor.toString();
