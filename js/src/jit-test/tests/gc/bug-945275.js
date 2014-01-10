function TestCase(n) {
  this.name = undefined;
  this.description = undefined;
}
gczeal(7,1);
eval("\
function reportCompare() new TestCase;\
reportCompare();\
Object.defineProperty(Object.prototype, 'name', {});\
reportCompare();\
");
