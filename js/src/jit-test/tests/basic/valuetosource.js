// |jit-test| --disable-tosource

const TEST_CASES = [
    [`undefined`, "(void 0)"],
    [`null`, "null"],
    [`true`, "true"],
    [`Symbol("abc")`, `Symbol("abc")`],
    [`15`, "15"],
    [`-0`, "-0"],
    [`"abc"`, `"abc"`],
    [`(function a() { return 1; })`, `(function a() { return 1; })`],
    [`[1, 2, 3]`, `[1, 2, 3]`],
    [`[1, {a: 0, b: 0}, 2]`, `[1, {a:0, b:0}, 2]`],
    [`({a: [1, 2, 3]})`, `({a:[1, 2, 3]})`],
    [`new Error("msg", "file", 1)`, `(new Error("msg", "file", 1))`],
    [`new TypeError("msg", "file", 1)`, `(new TypeError("msg", "file", 1))`],
    [`new class X extends Error {
        constructor() {
            super("msg", "file", 1);
            this.name = "X";
        }
    }`, `(new X("msg", "file", 1))`],
    [`/a(b)c/`, `/a(b)c/`],
    [`/abc/gi`, `/abc/gi`],
    [`new Boolean(false)`, `new Boolean(false)`],
    [`Object(false)`, `new Boolean(false)`],
    [`new String("abc")`, `new String("abc")`],
    [`Object("abc")`, `new String("abc")`],
    [`new Number(42)`, `new Number(42)`],
    [`Object(42)`, `new Number(42)`],
    [`new Date(1579447335)`, `new Date(1579447335)`],
]

let g = newGlobal({newCompartment: true});
for (let [actual, expected] of TEST_CASES) {
    assertEq(valueToSource(eval(actual)), expected);
    assertEq(valueToSource(g.eval(actual)), expected);
}
