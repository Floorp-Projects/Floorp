/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const samples = {
  array: ['x = [1, "2", {three: 3}, []]', "x = []"],

  boolean: ["true", "false"],

  date: ["new Date()"],

  function: ["x = () => { 2 }"],

  node: [
    `x = document.createElement("div");
     x.setAttribute("id", "myNodeId");
     x.setAttribute("class", "my-class and another");
     x.textContent = "My node id";
     x;`,
    `x = document.createElementNS("http://www.w3.org/2000/svg", "clipPath");
     x.setAttribute("id", "myNodeId");
     x.setAttribute("class", "my-class and another");
     x;`,
    "document.createComment('my comment node')",
    "document.createTextNode('foo')",
    `x = document.createAttribute('foo');
     x.value = "bar";
     x;`,
  ],

  "map & sets": [
    `
      ({
        "small set": new Set([1,2,3,4]),
        "small map": new Map([
          ["a", {suba: 1}],
          [{bkey: "b"}, 2]]
        ),
        "medium set": new Set(
          Array.from({length: 20})
            .map((el, i) => ({
              [String.fromCharCode(65 + i)]: i + 1,
              test: {
                [i] : "item" + i
              }
            })
          )),
        "medium map": new Map(
          Array
          .from({length: 20})
          .map((el, i) => [
            {
              [String.fromCharCode(65 + i)]: i + 1,
              test: {[i] : "item" + i, body: document.body}
            },
            Symbol(i + 1)
          ])
        ),
        "large set": new Set(
          Array.from({length: 2000})
          .map((el, i) => ({
            [String.fromCharCode(65 + i)]: i + 1,
            test: {
              [i] : "item" + i
            }
          })
        )),
        "large map": new Map(
          Array
          .from({length: 2000})
          .map((el, i) => [
            {
              [String.fromCharCode(65 + i)]: i + 1,
              document
            },
            Symbol(i + 1)
          ])
        ),
      })
    `,
  ],

  number: ["1", "-1", "-3.14", "0", "-0", "Infinity", "-Infinity", "NaN"],

  object: [
    "x = {a: 2}",
    `
Object.create(null, Object.getOwnPropertyDescriptors({
  get myStringGetter() {
    return "hello"
  },
  get myNumberGetter() {
    return 123;
  },
  get myUndefinedGetter() {
    return undefined;
  },
  get myNullGetter() {
    return null;
  },
  get myObjectGetter() {
    return {foo: "bar"}
  },
  get myArrayGetter() {
    return Array.from({length: 100000}, (_, i) => i)
  },
  get myMapGetter() {
    return new Map([["foo", {bar: "baz"}]])
  },
  get mySetGetter() {
    return new Set([1, {bar: "baz"}]);
  },
  get myProxyGetter() {
    var handler = { get: function(target, name) {
      return name in target ? target[name] : 37; }
    };
    return new Proxy({a: 1}, handler);
  },
  get myThrowingGetter() {
    return a.b.c.d.e.f;
  },
  get myLongStringGetter() {
    return "ab ".repeat(1e5)
  },
  set mySetterOnly(x) {}
}))
`,
  ],

  promise: [
    "Promise.resolve([1, 2, 3])",
    "Promise.reject(new Error('This is wrong'))",
    "new Promise(() => {})",
  ],

  proxy: [
    `
    var handler = {
        get: function(target, name) {
            return name in target ?
                target[name] :
                37;
        }
    };
    new Proxy({a: 1}, handler);
  `,
  ],

  regexp: ["new RegExp('^[-]?[0-9]+[.]?[0-9]+$')"],

  string: [
    "'foo'",
    "'bar\nbaz\nyup'",
    "'http://example.com'",
    "'blah'.repeat(10000)",
    "'http://example.com '.repeat(1000)",
  ],

  symbol: ["Symbol('foo')", "Symbol()"],

  errors: [
    "throw new Error('This is a simple error message.');",
    `
      var error = new Error('Complicated error message');
      error.stack =
        "unserializeProfileOfArbitraryFormat@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:26705:11\\n" +
        "_callee7$@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:27948:27\\n" +
        "tryCatch@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:64198:37\\n" +
        "invoke@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:64432:22\\n" +
        "defineIteratorMethods/</prototype[method]@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:64250:16\\n" +
        "step@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:5257:22\\n" +
        "step/<@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:5268:13\\n" +
        "run@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:64973:22\\n" +
        "notify/<@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:64986:28\\n" +
        "flush@http://localhost:4242/2969802751c9e11c0c2d.bundle.js:65282:9\\n";

      error;
    `,
    `
    var error = new Error('Complicated error message');
    error.stack =
      "onPacket@resource://devtools/shared/base-loader.js -> resource://devtools/shared/client/debugger-client.js:856:9\\n" +
      "send/<@resource://devtools/shared/base-loader.js -> resource://devtools/shared/transport/transport.js:569:13\\n" +
      "exports.makeInfallible/<@resource://devtools/shared/base-loader.js -> resource://devtools/shared/ThreadSafeDevToolsUtils.js:109:14\\n" +
      "exports.makeInfallible/<@resource://devtools/shared/base-loader.js -> resource://devtools/shared/ThreadSafeDevToolsUtils.js:109:14\\n";

    error;
    `,
  ],
};

samples.yolo = Object.keys(samples).reduce((res, key) => {
  return [...res, ...samples[key]];
}, []);

module.exports = samples;
