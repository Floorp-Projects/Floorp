var proto = Object.create(HTMLElement.prototype);
proto.magicNumber = 42;
proto.createdCallback = function() {
  finishTest(this.magicNumber === 42);
};
document.registerElement("x-foo", { prototype: proto });

document.createElement("x-foo");
