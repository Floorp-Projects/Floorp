load(libdir + "asserts.js");

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
  assertEq(e.lineNumber, line0 + 2);
}

// other iterable objects
assertEq(eval(...["a + b"].iterator()), 11);
assertEq(eval(...Set(["a + b"])), 11);
let itr = {
  iterator: function() {
    return {
      i: 0,
      next: function() {
        this.i++;
        if (this.i == 1)
          return "a + b";
        else
          throw StopIteration;
      }
    };
  }
};
assertEq(eval(...itr), 11);
let gen = {
  iterator: function() {
    yield "a + b";
  }
};
assertEq(eval(...gen), 11);

let c = ["C"], d = "D";
assertEq(eval(...c=["c[0] + d"]), "c[0] + dD");

// According to the draft spec, null and undefined are to be treated as empty
// arrays. However, they are not iterable. If the spec is not changed to be in
// terms of iterables, these tests should be fixed.
//assertEq(eval("a + b", ...null), 11);
//assertEq(eval("a + b", ...undefined), 11);
assertThrowsInstanceOf(() => eval("a + b", ...null), TypeError);
assertThrowsInstanceOf(() => eval("a + b", ...undefined), TypeError);
