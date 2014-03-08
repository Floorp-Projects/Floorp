load(libdir + "asserts.js");

var target = {};
Object.getOwnPropertyDescriptor(new Proxy(target, {
  getOwnPropertyDescriptor: function () {
    return {value: 2, configurable: true};
   }
}), 'foo');

var target = {};
Object.preventExtensions(target);
assertThrowsInstanceOf(function () {
  Object.getOwnPropertyDescriptor(new Proxy(target, {
    getOwnPropertyDescriptor: function () {
        return {value: 2, configurable: true};
    }
  }), 'foo');
}, TypeError);
