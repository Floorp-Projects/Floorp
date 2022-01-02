
function Foo() {
  var x = this.property;
  this.property = 5;
  glob = x;
}
Foo.prototype.property = 10;
for (var i = 0; i < 10; i++) {
  new Foo();
  assertEq(glob, 10);
}

function Bar() {
  this.property;
  this.other = 5;
}
Bar.prototype.other = 10;
Object.defineProperty(Bar.prototype, "property", {
  get: function() { glob = this.other; }
});
for (var i = 0; i < 10; i++) {
  new Bar();
  assertEq(glob, 10);
}
