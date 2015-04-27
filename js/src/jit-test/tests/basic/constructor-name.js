function Ctor() {}

var nested = {};
nested.Ctor = function () {};
nested.object = {};

function makeInstance() {
  let LexicalCtor = function () {};
  return new LexicalCtor;
}

function makeObject() {
  let object = {};
  return object;
}

let tests = [
  { name: "Ctor",                     object: new Ctor        },
  { name: "nested.Ctor",              object: new nested.Ctor },
  { name: "makeInstance/LexicalCtor", object: makeInstance()  },
  { name: null,                       object: {}              },
  { name: null,                       object: nested.object   },
  { name: null,                       object: makeObject()    },
];

for (let { name, object } of tests) {
  assertEq(getConstructorName(object), name);
}
