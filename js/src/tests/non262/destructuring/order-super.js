var BUGNUMBER = 1204028;
var summary = "Destructuring should evaluate lhs reference before rhs in super property";

if (typeof assertEq === "undefined") {
  assertEq = function(a, b) {
    if (a !== b)
      throw new Error(`expected ${b}, got ${a}\n${new Error().stack}`);
  };
}

print(BUGNUMBER + ": " + summary);

let logs = [];
function log(x) {
  logs.push(x);
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

class C1 {
  constructor() {
    this.clear();
  }
  clear() {
    this.values = {};
  }
}
for (let name of [
  "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
  "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
  "0", "1", "length"
]) {
  Object.defineProperty(C1.prototype, name, {
    set: function(value) {
      log("lhs set " + name);
      this.values[name] = value;
    }
  });
}
class C2 extends C1 {
  constructor() {
    super();

    let clear = () => {
      logs = [];
      this.clear();
    };

    // Array.

    clear();
    [
      super.a
    ] = logger(["A"]);
    assertEq(logs.join(","),
             [
               "rhs get @@iterator",
               "rhs call @@iterator",

               "rhs get @@iterator()::next",
               "rhs call @@iterator()::next",
               "rhs get @@iterator()::next()::done",
               "rhs get @@iterator()::next()::value",
               "lhs set a",
             ].join(","));
    assertEq(this.values.a, "A");

    clear();
    [
      super[ (log("lhs before name a"), "a") ]
    ] = logger(["A"]);
    assertEq(logs.join(","),
             [
               "rhs get @@iterator",
               "rhs call @@iterator",

               "lhs before name a",
               "rhs get @@iterator()::next",
               "rhs call @@iterator()::next",
               "rhs get @@iterator()::next()::done",
               "rhs get @@iterator()::next()::value",
               "lhs set a",
             ].join(","));
    assertEq(this.values.a, "A");

    // Array rest.

    clear();
    [
      ...super.a
    ] = logger(["A", "B", "C"]);
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
               "rhs get @@iterator()::next()::value",
               "rhs get @@iterator()::next",
               "rhs call @@iterator()::next",
               "rhs get @@iterator()::next()::done",
               "lhs set a",
             ].join(","));
    assertEq(this.values.a.join(","), "A,B,C");

    clear();
    [
      ...super[ (log("lhs before name a"), "a") ]
    ] = logger(["A", "B", "C"]);;
    assertEq(logs.join(","),
             [
               "rhs get @@iterator",
               "rhs call @@iterator",

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
    assertEq(this.values.a.join(","), "A,B,C");

    // Array combined.

    clear();
    [
      super.a,
      super[ (log("lhs before name b"), "b") ],
      ...super[ (log("lhs before name c"), "c") ]
    ] = logger(["A", "B", "C"]);
    assertEq(logs.join(","),
             [
               "rhs get @@iterator",
               "rhs call @@iterator",

               "rhs get @@iterator()::next",
               "rhs call @@iterator()::next",
               "rhs get @@iterator()::next()::done",
               "rhs get @@iterator()::next()::value",
               "lhs set a",

               "lhs before name b",
               "rhs get @@iterator()::next",
               "rhs call @@iterator()::next",
               "rhs get @@iterator()::next()::done",
               "rhs get @@iterator()::next()::value",
               "lhs set b",

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
    assertEq(this.values.a, "A");
    assertEq(this.values.b, "B");
    assertEq(this.values.c.join(","), "C");

    // Object.

    clear();
    ({
      a: super.a
    } = logger({a: "A"}));
    assertEq(logs.join(","),
             [
               "rhs get a",
               "lhs set a",
             ].join(","));
    assertEq(this.values.a, "A");

    clear();
    ({
      a: super[ (log("lhs before name a"), "a") ]
    } = logger({a: "A"}));
    assertEq(logs.join(","),
             [
               "lhs before name a",
               "rhs get a",
               "lhs set a",
             ].join(","));
    assertEq(this.values.a, "A");

    // Object combined.

    clear();
    ({
      a: super.a,
      b: super[ (log("lhs before name b"), "b") ]
    } = logger({a: "A", b: "B"}));
    assertEq(logs.join(","),
             [
               "rhs get a",
               "lhs set a",

               "lhs before name b",
               "rhs get b",
               "lhs set b",
             ].join(","));
    assertEq(this.values.a, "A");
    assertEq(this.values.b, "B");

    // == Nested ==

    // Array -> Array

    clear();
    [
      [
        super[ (log("lhs before name a"), "a") ],
        ...super[ (log("lhs before name b"), "b") ]
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

               "lhs before name a",
               "rhs get @@iterator()::next()::value::@@iterator()::next",
               "rhs call @@iterator()::next()::value::@@iterator()::next",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::done",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value",
               "lhs set a",

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
    assertEq(this.values.a, "A");
    assertEq(this.values.b.length, 1);
    assertEq(this.values.b[0], "B");

    // Array rest -> Array

    clear();
    [
      ...[
        super[ (log("lhs before name a"), "a") ],
        ...super[ (log("lhs before name b"), "b") ]
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

               "lhs before name a",
               "lhs set a",

               "lhs before name b",
               "lhs set b",
             ].join(","));
    assertEq(this.values.a, "A");
    assertEq(this.values.b.join(","), "B");

    // Array -> Object
    clear();
    [
      {
        a: super[ (log("lhs before name a"), "a") ]
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

               "lhs before name a",
               "rhs get @@iterator()::next()::value::a",
               "lhs set a",
             ].join(","));
    assertEq(this.values.a, "A");

    // Array rest -> Object
    clear();
    [
      ...{
        0: super[ (log("lhs before name 0"), "0") ],
        1: super[ (log("lhs before name 1"), "1") ],
        length: super[ (log("lhs before name length"), "length") ],
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

               "lhs before name 0",
               "lhs set 0",

               "lhs before name 1",
               "lhs set 1",

               "lhs before name length",
               "lhs set length",
             ].join(","));
    assertEq(this.values["0"], "A");
    assertEq(this.values["1"], "B");
    assertEq(this.values.length, 2);

    // Object -> Array
    clear();
    ({
      a: [
        super[ (log("lhs before name b"), "b") ]
      ]
    } = logger({a: ["B"]}));
    assertEq(logs.join(","),
             [
               "rhs get a",
               "rhs get a::@@iterator",
               "rhs call a::@@iterator",

               "lhs before name b",
               "rhs get a::@@iterator()::next",
               "rhs call a::@@iterator()::next",
               "rhs get a::@@iterator()::next()::done",
               "rhs get a::@@iterator()::next()::value",
               "lhs set b",
             ].join(","));
    assertEq(this.values.b, "B");

    // Object -> Object
    clear();
    ({
      a: {
        b: super[ (log("lhs before name b"), "b") ]
      }
    } = logger({a: {b: "B"}}));
    assertEq(logs.join(","),
             [
               "rhs get a",
               "lhs before name b",
               "rhs get a::b",
               "lhs set b",
             ].join(","));
    assertEq(this.values.b, "B");

    // All combined

    clear();
    [
      super[ (log("lhs before name a"), "a") ],
      [
        super[ (log("lhs before name b"), "b") ],
        {
          c: super[ (log("lhs before name c"), "c") ],
          d: {
            e: super[ (log("lhs before name e"), "e") ],
            f: [
              super[ (log("lhs before name g"), "g") ]
            ]
          }
        }
      ],
      {
        h: super[ (log("lhs before name h"), "h") ],
        i: [
          super[ (log("lhs before name j"), "j") ],
          {
            k: [
              super[ (log("lhs before name l"), "l") ]
            ]
          }
        ]
      },
      ...[
        super[ (log("lhs before name m"), "m") ],
        [
          super[ (log("lhs before name n"), "n") ],
          {
            o: super[ (log("lhs before name o"), "o") ],
            p: {
              q: super[ (log("lhs before name q"), "q") ],
              r: [
                super[ (log("lhs before name s"), "s") ]
              ]
            }
          }
        ],
        ...{
          0: super[ (log("lhs before name t"), "t") ],
          1: [
            super[ (log("lhs before name u"), "u") ],
            {
              v: super[ (log("lhs before name v"), "v") ],
              w: {
                x: super[ (log("lhs before name x"), "x") ],
                y: [
                  super[ (log("lhs before name z"), "z") ]
                ]
              }
            }
          ],
          length: super[ (log("lhs before name length"), "length") ],
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

               "lhs before name c",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::c",
               "lhs set c",

               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d",

               "lhs before name e",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::e",
               "lhs set e",

               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::f",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::d::f::@@iterator",
               "rhs call @@iterator()::next()::value::@@iterator()::next()::value::d::f::@@iterator",

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

               "lhs before name h",
               "rhs get @@iterator()::next()::value::h",
               "lhs set h",

               "rhs get @@iterator()::next()::value::i",
               "rhs get @@iterator()::next()::value::i::@@iterator",
               "rhs call @@iterator()::next()::value::i::@@iterator",

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

               "lhs before name m",
               "lhs set m",

               "rhs get @@iterator()::next()::value::@@iterator",
               "rhs call @@iterator()::next()::value::@@iterator",

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

               "lhs before name o",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::o",
               "lhs set o",

               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p",

               "lhs before name q",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::q",
               "lhs set q",

               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator",
               "rhs call @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator",

               "lhs before name s",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator()::next",
               "rhs call @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator()::next",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator()::next()::done",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::p::r::@@iterator()::next()::value",
               "lhs set s",

               "lhs before name t",
               "lhs set t",

               "rhs get @@iterator()::next()::value::@@iterator",
               "rhs call @@iterator()::next()::value::@@iterator",

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

               "lhs before name v",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::v",
               "lhs set v",

               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w",

               "lhs before name x",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::x",
               "lhs set x",

               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator",
               "rhs call @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator",

               "lhs before name z",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator()::next",
               "rhs call @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator()::next",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator()::next()::done",
               "rhs get @@iterator()::next()::value::@@iterator()::next()::value::w::y::@@iterator()::next()::value",
               "lhs set z",

               "lhs before name length",
               "lhs set length",
             ].join(","));
    assertEq(this.values.a, "A");
    assertEq(this.values.b, "B");
    assertEq(this.values.c, "C");
    assertEq(this.values.e, "E");
    assertEq(this.values.g, "G");
    assertEq(this.values.h, "H");
    assertEq(this.values.j, "J");
    assertEq(this.values.l, "L");
    assertEq(this.values.m, "M");
    assertEq(this.values.n, "N");
    assertEq(this.values.o, "O");
    assertEq(this.values.q, "Q");
    assertEq(this.values.s, "S");
    assertEq(this.values.t, "T");
    assertEq(this.values.u, "U");
    assertEq(this.values.v, "V");
    assertEq(this.values.x, "X");
    assertEq(this.values.z, "Z");
    assertEq(this.values.length, 2);
  }
}

new C2();

if (typeof reportCompare === "function")
    reportCompare(true, true);
