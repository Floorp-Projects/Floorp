load(libdir + "asserts.js");
load(libdir + "iteration.js");

assertEq(eval(...[]), undefined);
assertEq(eval(...["1 + 2"]), 3);

let a = 10, b = 1;
assertEq(eval(...["a + b"]), 11);

(function() {
  let a = 20;
  assertEq(eval(...["a + b"]), 21);
})();

with ({ a: 30 }) {
  assertEq(eval(...["a + b"]), 31);
}

let line0 = Error().lineNumber;
try {             // line0 + 1
  eval(...["("]); // line0 + 2
} catch (e) {
  assertEq(e.lineNumber, 1);
}

// other iterable objects
assertEq(eval(...["a + b"][std_iterator]()), 11);
assertEq(eval(...Set(["a + b"])), 11);
let itr = {};
itr[std_iterator] = function() {
    return {
        i: 0,
        next: function() {
            this.i++;
            if (this.i == 1)
                return { value: "a + b", done: false };
            else
                return { value: undefined, done: true };
        }
    };
};
assertEq(eval(...itr), 11);
function* gen() {
    yield "a + b";
}
assertEq(eval(...gen()), 11);

let c = ["C"], d = "D";
assertEq(eval(...c=["c[0] + d"]), "c[0] + dD");

// 12.2.4.1.2 Runtime Semantics: ArrayAccumulation
// If Type(spreadObj) is not Object, then throw a TypeError exception.
assertThrowsInstanceOf(() => eval("a + b", ...null), TypeError);
assertThrowsInstanceOf(() => eval("a + b", ...undefined), TypeError);
