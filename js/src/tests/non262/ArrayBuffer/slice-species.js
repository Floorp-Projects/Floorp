const tests = [
  [Int8Array,         [9, 10, 11, 12, 13, 14, 15, 16]],
  [Uint8Array,        [9, 10, 11, 12, 13, 14, 15, 16]],
  [Uint8ClampedArray, [9, 10, 11, 12, 13, 14, 15, 16]],
  [Int16Array,        [5, 6, 7, 8]],
  [Uint16Array,       [5, 6, 7, 8]],
  [Int32Array,        [3, 4]],
  [Uint32Array,       [3, 4]],
  [Float32Array,      [3, 4]],
  [Float64Array,      [2]],
];

let logs = [];
for (let [ctor, answer] of tests) {
  let arr = new ctor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);

  let proxyProto = new Proxy({}, {
    get(that, name) {
      throw new Error("unexpected prop access");
    }
  });

  class MyArrayBuffer extends ArrayBuffer {}

  arr.buffer.constructor = new Proxy({}, {
    get(that, name) {
      if (name == Symbol.species) {
        logs.push("get @@species");
        let C = new Proxy(function(...args) {
          logs.push("call ctor");
          return new MyArrayBuffer(...args);
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
      throw new Error("unexpected prop access");
    }
  });

  logs.length = 0;
  let buf = arr.buffer.slice(8, 16);
  assertEq(buf.constructor, MyArrayBuffer);
  assertDeepEq(logs, ["get @@species", "get ctor.prototype", "call ctor"]);
  assertDeepEq([...new ctor(buf)], answer);


  // modified @@species
  let a = arr.buffer;
  a.constructor = {
    [Symbol.species]: MyArrayBuffer
  };
  let b = a.slice(8, 16);
  assertEq(b.constructor, MyArrayBuffer);
  assertDeepEq([...new ctor(b)], answer);

  class MyArrayBufferWithSpecies extends ArrayBuffer {
    get [Symbol.species]() {
      return MyArrayBufferWithSpecies;
    }
  }
  a = arr.buffer;
  a.constructor = MyArrayBufferWithSpecies;
  b = a.slice(8, 16);
  assertEq(b.constructor, MyArrayBufferWithSpecies);
  assertDeepEq([...new ctor(b)], answer);

  // no @@species
  a = arr.buffer;
  a.constructor = {
    [Symbol.species]: undefined
  };
  b = a.slice(8, 16);
  assertEq(b.constructor, ArrayBuffer);
  assertDeepEq([...new ctor(b)], answer);

  a = arr.buffer;
  a.constructor = {
    [Symbol.species]: null
  };
  b = a.slice(8, 16);
  assertEq(b.constructor, ArrayBuffer);
  assertDeepEq([...new ctor(b)], answer);

  // invalid @@species
  for (let species of [0, 1.1, true, false, "a", /a/, Symbol.iterator, [], {}]) {
    a = arr.buffer;
    a.constructor = {
      [Symbol.species]: species
    };
    assertThrowsInstanceOf(() => a.slice(8, 16), TypeError);
  }

  // undefined constructor
  a = arr.buffer;
  a.constructor = undefined;
  b = a.slice(8, 16);
  assertEq(b.constructor, ArrayBuffer);
  assertDeepEq([...new ctor(b)], answer);

  // invalid constructor
  for (let ctor of [null, 0, 1.1, true, false, "a", Symbol.iterator]) {
    a = arr.buffer;
    a.constructor = ctor;
    assertThrowsInstanceOf(() => a.slice(8, 16), TypeError);
  }

  // @@species from different global
  let g = newGlobal();
  g.eval("var MyArrayBuffer = class MyArrayBuffer extends ArrayBuffer {};");
  a = arr.buffer;
  a.constructor = {
    [Symbol.species]: g.MyArrayBuffer
  };
  b = a.slice(8, 16);
  assertEq(b.constructor, g.MyArrayBuffer);
  assertDeepEq([...new ctor(b)], answer);

  a = arr.buffer;
  a.constructor = {
    [Symbol.species]: g.ArrayBuffer
  };
  b = a.slice(8, 16);
  assertEq(b.constructor, g.ArrayBuffer);
  assertDeepEq([...new ctor(b)], answer);

  // constructor from different global
  g.eval(`
var MyArrayBufferWithSpecies = class MyArrayBufferWithSpecies extends ArrayBuffer {
  get [Symbol.species]() {
    return MyArrayBufferWithSpecies;
  }
};
`);
  a = arr.buffer;
  a.constructor = g.MyArrayBufferWithSpecies;
  b = a.slice(8, 16);
  assertEq(b.constructor, g.MyArrayBufferWithSpecies);
  assertDeepEq([...new ctor(b)], answer);

  g.eval(`
var arr = new ${ctor.name}([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
var a = arr.buffer;
`);
  b = ArrayBuffer.prototype.slice.call(g.a, 8, 16);
  assertEq(b.constructor, g.ArrayBuffer);
  assertDeepEq([...new ctor(b)], answer);

  // running in different global
  b = g.a.slice(8, 16);
  assertEq(b.constructor, g.ArrayBuffer);
  assertDeepEq([...new ctor(b)], answer);

  // subclasses
  // not-modified @@species
  a = new MyArrayBuffer(16);
  b = a.slice(8, 16);
  assertEq(b.constructor, MyArrayBuffer);

  // modified @@species
  class MyArrayBuffer2 extends ArrayBuffer {
  }
  class MyArrayBuffer3 extends ArrayBuffer {
    static get [Symbol.species]() {
      return MyArrayBuffer2;
    }
  }
  a = new MyArrayBuffer3(16);
  b = a.slice(8, 16);
  assertEq(b.constructor, MyArrayBuffer2);
}

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
