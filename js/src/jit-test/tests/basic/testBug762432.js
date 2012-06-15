function getter() { return 1; }
function setter() { }
function getDescriptor(name) {
  if (name != 'prop')
    throw "Unknown property: " + name;
  return { configurable: true, enumerable: true, get: getter, set: setter };
}
function getNames() { return ['prop']; }
var handler = {
  getOwnPropertyDescriptor: getDescriptor,
  getPropertyDescriptor: getDescriptor,
  getOwnPropertyNames: getNames,
  getPropertyNames: getNames,
  defineProperty: function() {},
  delete: function() {}
};

// Make sure that __lookup{Getter,Setter}__ works on proxies.
var proxy = Proxy.create(handler);
assertEq(Object.prototype.__lookupGetter__.call(proxy, 'prop'), getter);
assertEq(Object.prototype.__lookupSetter__.call(proxy, 'prop'), setter);
