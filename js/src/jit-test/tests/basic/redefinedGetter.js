
function Thing() {}

function foo(x, expected) {
  for (var i = 0; i < 10; i++)
    assertEq(x.f, expected);
}

Object.defineProperty(Thing.prototype, "f", {get: function() { return 2; }, configurable:true});
foo(new Thing(), 2);
Object.defineProperty(Thing.prototype, "f", {get: function() { return 3; }});
foo(new Thing(), 3);

function OtherThing() {}

function bar(x, expected) {
  for (var i = 0; i < 10; i++)
    assertEq(x.f, expected);
}

Object.defineProperty(OtherThing.prototype, "f", {get: function() { return 2; }, configurable:true});
bar(new OtherThing(), 2);
delete OtherThing.prototype.f;
bar(new OtherThing(), undefined);
