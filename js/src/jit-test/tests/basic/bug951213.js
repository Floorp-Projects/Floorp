
enableShellAllocationMetadataBuilder();
function foo(x, y) {
  this.g = x + y;
}
var a = 0;
var b = { valueOf: function() { return Object.defineProperty(Object.prototype, 'g', {}); } };
var c = new foo(a, b);
