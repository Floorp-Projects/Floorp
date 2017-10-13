var proto = Object.create(HTMLElement.prototype);
proto.magicNumber = 42;
proto.connectedCallback = function() {
  finishTest(this.magicNumber === 42);
};
document.registerElement("x-foo", { prototype: proto });

document.firstChild.appendChild(document.createElement("x-foo"));
