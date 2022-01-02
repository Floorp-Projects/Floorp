function TestCase(n, d, e, a) {}
function reportCompare () {
      var testcase = new TestCase();
}
reportCompare();
schedulegc(10);
this.TestCase=Number;
reportCompare(4294967295.5);
