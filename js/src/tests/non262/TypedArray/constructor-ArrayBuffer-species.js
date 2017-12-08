let logs = [];
for (let ctor of typedArrayConstructors) {
  let arr = new ctor([1, 2, 3, 4, 5, 6, 7, 8]);

  let ctorObj = {};

  let proxyProto = new Proxy({}, {
    get(that, name) {
      logs.push("get proto." + String(name));
      if (name == "constructor")
        return ctorObj;
      throw new Error("unexpected prop access");
    }
  });

  arr.buffer.constructor = {
    get [Symbol.species]() {
      logs.push("get @@species");
      let C = new Proxy(function(...args) {
        logs.push("call ctor");
        return new ArrayBuffer(...args);
      }, {
        get(that, name) {
          logs.push("get ctor." + String(name));
          if (name == "prototype") {
            return proxyProto;
          }
          throw new Error("unexpected prop access");
        }
      });
      return C;
    }
  };

  for (let ctor2 of typedArrayConstructors) {
    logs.length = 0;
    let arr2 = new ctor2(arr);
    assertDeepEq(logs, ["get @@species", "get ctor.prototype"]);

    logs.length = 0;
    assertEq(Object.getPrototypeOf(arr2.buffer), proxyProto);
    assertDeepEq(logs, []);

    logs.length = 0;
    assertEq(arr2.buffer.constructor, ctorObj);
    assertDeepEq(logs, ["get proto.constructor"]);
  }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
