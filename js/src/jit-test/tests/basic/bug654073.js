load(libdir + "immutable-prototype.js");

if (globalPrototypeChainIsMutable()) {
  this.__proto__ = null;
  Object.prototype.__proto__ = this;
}

for (var y in Object.prototype)
  continue;
for (var x in this)
  continue;
