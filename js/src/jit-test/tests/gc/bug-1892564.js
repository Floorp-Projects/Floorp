try { evalInWorker(`
gczeal(0);
try {
  Object.defineProperty(this, "x", {
    value:{
        parseInt: parseInt,
    }
  });
} catch(exc) {}
function dummyAssertCallFunction(f) {}
try { evaluate(\`
(function(global) {
  var ObjectCreate = global.Object.create;
  var ObjectDefineProperty = global.Object.defineProperty;
  function ArrayPush(arr, val) {
    var desc = ObjectCreate(null);
    desc.value = val;
    desc.enumerable = true;
    desc.configurable = true;
    desc.writable = true;
    ObjectDefineProperty(arr, arr.length, desc);
  }
  var testCasesArray = [];
  function TestCase(d, e, a, r) {
    this.description = d;
    ArrayPush(testCasesArray, this);
  }
  global.TestCase = TestCase;
})(this);
(function f42(x99) {
  new TestCase(new ArrayBuffer());
  f42(x99)
  function t9() {}
})();
\`); } catch(exc) {}
try { evaluate(\`
gczeal(14);
(function(global) {
  global.makeIterator = function makeIterator(overrides) {
    global.assertThrowsValue = function assertThrowsValue(f, val, msg) {};
  }
  global.assertDeepEq = (function(){
    var call = Function.prototype.call,
      Symbol_description = call.bind(Object.getOwnPropertyDescriptor(Symbol.prototype, "description").get),
      Map_has = call.bind(Map.prototype.has),
      Object_getOwnPropertyNames = Object.getOwnPropertyNames;
  })();
})(this);
\`); } catch(exc) {}
`); throw "x"; } catch(exc) {}
