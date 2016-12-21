var BUGNUMBER = 1204028;
var summary = "Destructuring should evaluate lhs reference before rhs";

print(BUGNUMBER + ": " + summary);

let storage = {
  clear() {
    this.values = {};
  }
};
storage.clear();
let obj = new Proxy(storage, {
  set(that, name, value) {
    log("lhs set " + name);
    storage.values[name] = value;
  }
});

let logs = [];
function log(x) {
  logs.push(x);
}

function clear() {
  logs = [];
  storage.clear();
}

let unwrapMap = new Map();
function unwrap(maybeWrapped) {
  if (unwrapMap.has(maybeWrapped))
    return unwrapMap.get(maybeWrapped);
  return maybeWrapped;
}
function ToString(name) {
  if (name == Symbol.iterator)
    return "@@iterator";
  return String(name);
}
function logger(obj, prefix=[]) {
  let wrapped = new Proxy(obj, {
    get(that, name) {
      if (name == "return") {
        // FIXME: Bug 1147371.
        // We ignore IteratorClose for now.
        return obj[name];
      }

      let names = prefix.concat(ToString(name));
      log("rhs get " + names.join("::"));
      let v = obj[name];
      if (typeof v === "object" || typeof v === "function")
        return logger(v, names);
      return v;
    },
    apply(that, thisArg, args) {
      let names = prefix.slice();
      log("rhs call " + names.join("::"));
      let v = obj.apply(unwrap(thisArg), args);
      if (typeof v === "object" || typeof v === "function") {
        names[names.length - 1] += "()";
        return logger(v, names);
      }
      return v;
    }
  });
  unwrapMap.set(wrapped, obj);
  return wrapped;
}

// Array.

clear();
[
  ( log("lhs before obj a"), obj ).a
] = logger(["A"]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "lhs before obj a",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "lhs set a",
         ].join(","));
assertEq(storage.values.a, "A");

clear();
[
  ( log("lhs before obj a"), obj )[ (log("lhs before name a"), "a") ]
] = logger(["A"]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "lhs before obj a",
           "lhs before name a",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "lhs set a",
         ].join(","));
assertEq(storage.values.a, "A");

// Array rest.

clear();
[
  ...( log("lhs before obj a"), obj ).a
] = logger(["A", "B", "C"]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "lhs before obj a",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "lhs set a",
         ].join(","));
assertEq(storage.values.a.join(","), "A,B,C");

clear();
[
  ...( log("lhs before obj a"), obj )[ (log("lhs before name a"), "a") ]
] = logger(["A", "B", "C"]);;
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "lhs before obj a",
           "lhs before name a",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "lhs set a",
         ].join(","));
assertEq(storage.values.a.join(","), "A,B,C");

// Array combined.

clear();
[
  ( log("lhs before obj a"), obj ).a,
  ( log("lhs before obj b"), obj )[ (log("lhs before name b"), "b") ],
  ...( log("lhs before obj c"), obj )[ (log("lhs before name c"), "c") ]
] = logger(["A", "B", "C"]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "lhs before obj a",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "lhs set a",

           "lhs before obj b",
           "lhs before name b",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "lhs set b",

           "lhs before obj c",
           "lhs before name c",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "lhs set c",
         ].join(","));
assertEq(storage.values.a, "A");
assertEq(storage.values.b, "B");
assertEq(storage.values.c.join(","), "C");

// Object.

clear();
({
  a: ( log("lhs before obj a"), obj ).a
} = logger({a: "A"}));
assertEq(logs.join(","),
         [
           "lhs before obj a",
           "rhs get a",
           "lhs set a",
         ].join(","));
assertEq(storage.values.a, "A");

clear();
({
  a: ( log("lhs before obj a"), obj )[ (log("lhs before name a"), "a") ]
} = logger({a: "A"}));
assertEq(logs.join(","),
         [
           "lhs before obj a",
           "lhs before name a",
           "rhs get a",
           "lhs set a",
         ].join(","));
assertEq(storage.values.a, "A");

// Object combined.

clear();
({
  a: ( log("lhs before obj a"), obj ).a,
  b: ( log("lhs before obj b"), obj )[ (log("lhs before name b"), "b") ]
} = logger({a: "A", b: "B"}));
assertEq(logs.join(","),
         [
           "lhs before obj a",
           "rhs get a",
           "lhs set a",

           "lhs before obj b",
           "lhs before name b",
           "rhs get b",
           "lhs set b",
         ].join(","));
assertEq(storage.values.a, "A");
assertEq(storage.values.b, "B");

// == Nested ==

// Array -> Array

clear();
[
  [
    ( log("lhs before obj a"), obj )[ (log("lhs before name a"), "a") ],
    ...( log("lhs before obj b"), obj )[ (log("lhs before name b"), "b") ]
  ]
] = logger([["A", "B"]]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next()::value::@@iterator",
           "rhs call @@iterator()::next()::value::@@iterator",

           "lhs before obj a",
           "lhs before name a",
           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value",
           "lhs set a",

           "lhs before obj b",
           "lhs before name b",
           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value",
           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "lhs set b",
         ].join(","));
assertEq(storage.values.a, "A");
assertEq(storage.values.b.length, 1);
assertEq(storage.values.b[0], "B");

// Array rest -> Array

clear();
[
  ...[
    ( log("lhs before obj a"), obj )[ (log("lhs before name a"), "a") ],
    ...( log("lhs before obj b"), obj )[ (log("lhs before name b"), "b") ]
  ]
] = logger(["A", "B"]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",

           "lhs before obj a",
           "lhs before name a",
           "lhs set a",

           "lhs before obj b",
           "lhs before name b",
           "lhs set b",
         ].join(","));
assertEq(storage.values.a, "A");
assertEq(storage.values.b.join(","), "B");

// Array -> Object
clear();
[
  {
    a: ( log("lhs before obj a"), obj )[ (log("lhs before name a"), "a") ]
  }
] = logger([{a: "A"}]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",

           "lhs before obj a",
           "lhs before name a",
           "rhs get @@iterator()::next()::value::a",
           "lhs set a",
         ].join(","));
assertEq(storage.values.a, "A");

// Array rest -> Object
clear();
[
  ...{
    0: ( log("lhs before obj 0"), obj )[ (log("lhs before name 0"), "0") ],
    1: ( log("lhs before obj 1"), obj )[ (log("lhs before name 1"), "1") ],
    length: ( log("lhs before obj length"), obj )[ (log("lhs before name length"), "length") ],
  }
] = logger(["A", "B"]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",

           "lhs before obj 0",
           "lhs before name 0",
           "lhs set 0",

           "lhs before obj 1",
           "lhs before name 1",
           "lhs set 1",

           "lhs before obj length",
           "lhs before name length",
           "lhs set length",
         ].join(","));
assertEq(storage.values["0"], "A");
assertEq(storage.values["1"], "B");
assertEq(storage.values.length, 2);

// Object -> Array
clear();
({
  a: [
    ( log("lhs before obj b"), obj )[ (log("lhs before name b"), "b") ]
  ]
} = logger({a: ["B"]}));
assertEq(logs.join(","),
         [
           "rhs get a",
           "rhs get a::@@iterator",
           "rhs call a::@@iterator",

           "lhs before obj b",
           "lhs before name b",
           "rhs get a::@@iterator()::next",
           "rhs call a::@@iterator()::next",
           "rhs get a::@@iterator()::next()::done",
           "rhs get a::@@iterator()::next()::value",
           "lhs set b",
         ].join(","));
assertEq(storage.values.b, "B");

// Object -> Object
clear();
({
  a: {
    b: ( log("lhs before obj b"), obj )[ (log("lhs before name b"), "b") ]
  }
} = logger({a: {b: "B"}}));
assertEq(logs.join(","),
         [
           "rhs get a",
           "lhs before obj b",
           "lhs before name b",
           "rhs get a::b",
           "lhs set b",
         ].join(","));
assertEq(storage.values.b, "B");

// All combined

clear();
[
  ( log("lhs before obj a"), obj )[ (log("lhs before name a"), "a") ],
  [
    ( log("lhs before obj b"), obj )[ (log("lhs before name b"), "b") ],
    {
      c: ( log("lhs before obj c"), obj )[ (log("lhs before name c"), "c") ],
      d: {
        e: ( log("lhs before obj e"), obj )[ (log("lhs before name e"), "e") ],
        f: [
          ( log("lhs before obj g"), obj )[ (log("lhs before name g"), "g") ]
        ]
      }
    }
  ],
  {
    h: ( log("lhs before obj h"), obj )[ (log("lhs before name h"), "h") ],
    i: [
      ( log("lhs before obj j"), obj )[ (log("lhs before name j"), "j") ],
      {
        k: [
          ( log("lhs before obj l"), obj )[ (log("lhs before name l"), "l") ]
        ]
      }
    ]
  },
  ...[
    ( log("lhs before obj m"), obj )[ (log("lhs before name m"), "m") ],
    [
      ( log("lhs before obj n"), obj )[ (log("lhs before name n"), "n") ],
      {
        o: ( log("lhs before obj o"), obj )[ (log("lhs before name o"), "o") ],
        p: {
          q: ( log("lhs before obj q"), obj )[ (log("lhs before name q"), "q") ],
          r: [
            ( log("lhs before obj s"), obj )[ (log("lhs before name s"), "s") ]
          ]
        }
      }
    ],
    ...{
      0: ( log("lhs before obj t"), obj )[ (log("lhs before name t"), "t") ],
      1: [
        ( log("lhs before obj u"), obj )[ (log("lhs before name u"), "u") ],
        {
          v: ( log("lhs before obj v"), obj )[ (log("lhs before name v"), "v") ],
          w: {
            x: ( log("lhs before obj x"), obj )[ (log("lhs before name x"), "x") ],
            y: [
              ( log("lhs before obj z"), obj )[ (log("lhs before name z"), "z") ]
            ]
          }
        }
      ],
      length: ( log("lhs before obj length"), obj )[ (log("lhs before name length"), "length") ],
    }
  ]
] = logger(["A",
            ["B", {c: "C", d: {e: "E", f: ["G"]}}],
            {h: "H", i: ["J", {k: ["L"]}]},
            "M",
            ["N", {o: "O", p: {q: "Q", r: ["S"]}}],
            "T", ["U", {v: "V", w: {x: "X", y: ["Z"]}}]]);
assertEq(logs.join(","),
         [
           "rhs get @@iterator",
           "rhs call @@iterator",

           "lhs before obj a",
           "lhs before name a",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "lhs set a",

           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next()::value::@@iterator",
           "rhs call @@iterator()::next()::value::@@iterator",

           "lhs before obj b",
           "lhs before name b",
           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value",
           "lhs set b",

           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value",

           "lhs before obj c",
           "lhs before name c",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::c",
           "lhs set c",

           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d",

           "lhs before obj e",
           "lhs before name e",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::e",
           "lhs set e",

           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::f",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::f::@@iterator",
           "rhs call @@iterator()::next()::value::@@iterator()::next()::value::d::f::@@iterator",

           "lhs before obj g",
           "lhs before name g",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::f::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next()::value::d::f::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::f::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::f::@@iterator()::next()::value",
           "lhs set g",

           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",

           "lhs before obj h",
           "lhs before name h",
           "rhs get @@iterator()::next()::value::h",
           "lhs set h",

           "rhs get @@iterator()::next()::value::i",
           "rhs get @@iterator()::next()::value::i::@@iterator",
           "rhs call @@iterator()::next()::value::i::@@iterator",

           "lhs before obj j",
           "lhs before name j",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next",
           "rhs call @@iterator()::next()::value::i::@@iterator()::next",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::value",
           "lhs set j",

           "rhs get @@iterator()::next()::value::i::@@iterator()::next",
           "rhs call @@iterator()::next()::value::i::@@iterator()::next",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::value",

           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::value::k",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::value::k::@@iterator",
           "rhs call @@iterator()::next()::value::i::@@iterator()::next()::value::k::@@iterator",

           "lhs before obj l",
           "lhs before name l",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::value::k::@@iterator()::next",
           "rhs call @@iterator()::next()::value::i::@@iterator()::next()::value::k::@@iterator()::next",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::value::k::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::i::@@iterator()::next()::value::k::@@iterator()::next()::value",
           "lhs set l",

           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",
           "rhs get @@iterator()::next()::value",
           "rhs get @@iterator()::next",
           "rhs call @@iterator()::next",
           "rhs get @@iterator()::next()::done",

           "lhs before obj m",
           "lhs before name m",
           "lhs set m",

           "rhs get @@iterator()::next()::value::@@iterator",
           "rhs call @@iterator()::next()::value::@@iterator",

           "lhs before obj n",
           "lhs before name n",
           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value",
           "lhs set n",

           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value",

           "lhs before obj o",
           "lhs before name o",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::o",
           "lhs set o",

           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p",

           "lhs before obj q",
           "lhs before name q",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::q",
           "lhs set q",

           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator",
           "rhs call @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator",

           "lhs before obj s",
           "lhs before name s",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator()::next()::value",
           "lhs set s",

           "lhs before obj t",
           "lhs before name t",
           "lhs set t",

           "rhs get @@iterator()::next()::value::@@iterator",
           "rhs call @@iterator()::next()::value::@@iterator",

           "lhs before obj u",
           "lhs before name u",
           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value",
           "lhs set u",

           "rhs get @@iterator()::next()::value::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value",

           "lhs before obj v",
           "lhs before name v",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::v",
           "lhs set v",

           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w",

           "lhs before obj x",
           "lhs before name x",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::x",
           "lhs set x",

           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator",
           "rhs call @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator",

           "lhs before obj z",
           "lhs before name z",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator()::next",
           "rhs call @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator()::next",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator()::next()::done",
           "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator()::next()::value",
           "lhs set z",

           "lhs before obj length",
           "lhs before name length",
           "lhs set length",
         ].join(","));
assertEq(storage.values.a, "A");
assertEq(storage.values.b, "B");
assertEq(storage.values.c, "C");
assertEq(storage.values.e, "E");
assertEq(storage.values.g, "G");
assertEq(storage.values.h, "H");
assertEq(storage.values.j, "J");
assertEq(storage.values.l, "L");
assertEq(storage.values.m, "M");
assertEq(storage.values.n, "N");
assertEq(storage.values.o, "O");
assertEq(storage.values.q, "Q");
assertEq(storage.values.s, "S");
assertEq(storage.values.t, "T");
assertEq(storage.values.u, "U");
assertEq(storage.values.v, "V");
assertEq(storage.values.x, "X");
assertEq(storage.values.z, "Z");
assertEq(storage.values.length, 2);

if (typeof reportCompare === "function")
    reportCompare(true, true);
