load(libdir + "asserts.js");
load(libdir + "eqArrayHelper.js");
load(libdir + "iteration.js");

let makeCall    = farg => Function("f", "arg", "return f(" + farg + ");");
let makeFunCall = farg => Function("f", "arg", "return f.call(null, " + farg + ");");
let makeNew     = farg => Function("f", "arg", "return new f(" + farg + ").value;");

function checkCommon(f, makeFn) {
  assertEqArray(makeFn("...[1, 2, 3]")(f), [1, 2, 3]);
  assertEqArray(makeFn("1, ...[2], 3")(f), [1, 2, 3]);
  assertEqArray(makeFn("1, ...[2], ...[3]")(f), [1, 2, 3]);
  assertEqArray(makeFn("1, ...[2, 3]")(f), [1, 2, 3]);
  assertEqArray(makeFn("1, ...[], 2, 3")(f), [1, 2, 3]);

  // other iterable objects
  assertEqArray(makeFn("...arg")(f, Int32Array([1, 2, 3])), [1, 2, 3]);
  assertEqArray(makeFn("...arg")(f, "abc"), ["a", "b", "c"]);
  assertEqArray(makeFn("...arg")(f, [1, 2, 3][std_iterator]()), [1, 2, 3]);
  assertEqArray(makeFn("...arg")(f, Set([1, 2, 3])), [1, 2, 3]);
  assertEqArray(makeFn("...arg")(f, Map([["a", "A"], ["b", "B"], ["c", "C"]])).map(([k, v]) => k + v), ["aA", "bB", "cC"]);
  let itr = {};
  itr[std_iterator] = function() {
      return {
          i: 1,
          next: function() {
              if (this.i < 4)
                  return { value: this.i++, done: false };
              else
                  return { value: undefined, done: true };
          }
      };
  };
  assertEqArray(makeFn("...arg")(f, itr), [1, 2, 3]);
  function gen() {
      for (let i = 1; i < 4; i ++)
          yield i;
  }
  assertEqArray(makeFn("...arg")(f, gen()), [1, 2, 3]);

  assertEqArray(makeFn("...arg=[1, 2, 3]")(f), [1, 2, 3]);

  // According to the draft spec, null and undefined are to be treated as empty
  // arrays. However, they are not iterable. If the spec is not changed to be in
  // terms of iterables, these tests should be fixed.
  //assertEqArray(makeFn(1, ...null, 2, 3)(f), [1, 2, 3]);
  //assertEqArray(makeFn(1, ...undefined, 2, 3)(f), [1, 2, 3]);
  assertThrowsInstanceOf(makeFn("1, ...null, 2, 3"), TypeError);
  assertThrowsInstanceOf(makeFn("1, ...undefined, 2, 3"), TypeError);
}

function checkNormal(f, makeFn) {
  checkCommon(f, makeFn);

  assertEqArray(makeFn("...[]")(f), [undefined, undefined, undefined]);
  assertEqArray(makeFn("...[1]")(f), [1, undefined, undefined]);
  assertEqArray(makeFn("...[1, 2]")(f), [1, 2, undefined]);
  assertEqArray(makeFn("...[1, 2, 3, 4]")(f), [1, 2, 3]);

  assertEqArray(makeFn("...[undefined]")(f), [undefined, undefined, undefined]);
}

checkNormal(function(a, b, c) [a, b, c], makeCall);
checkNormal(function(a, b, c) [a, b, c], makeFunCall);
checkNormal((a, b, c) => [a, b, c], makeCall);
checkNormal((a, b, c) => [a, b, c], makeFunCall);
function normalClass(a, b, c) {
  this.value = [a, b, c];
  assertEq(Object.getPrototypeOf(this), normalClass.prototype);
}
checkNormal(normalClass, makeNew);

function checkDefault(f, makeFn) {
  checkCommon(f, makeFn);

  assertEqArray(makeFn("...[]")(f), [-1, -2, -3]);
  assertEqArray(makeFn("...[1]")(f), [1, -2, -3]);
  assertEqArray(makeFn("...[1, 2]")(f), [1, 2, -3]);
  assertEqArray(makeFn("...[1, 2, 3, 4]")(f), [1, 2, 3]);

  assertEqArray(makeFn("...[undefined]")(f), [-1, -2, -3]);
}

checkDefault(function(a = -1, b = -2, c = -3) [a, b, c], makeCall);
checkDefault(function(a = -1, b = -2, c = -3) [a, b, c], makeFunCall);
checkDefault((a = -1, b = -2, c = -3) => [a, b, c], makeCall);
checkDefault((a = -1, b = -2, c = -3) => [a, b, c], makeFunCall);
function defaultClass(a = -1, b = -2, c = -3) {
  this.value = [a, b, c];
  assertEq(Object.getPrototypeOf(this), defaultClass.prototype);
}
checkDefault(defaultClass, makeNew);

function checkRest(f, makeFn) {
  checkCommon(f, makeFn);

  assertEqArray(makeFn("...[]")(f), []);
  assertEqArray(makeFn("1, ...[2, 3, 4], 5")(f), [1, 2, 3, 4, 5]);
  assertEqArray(makeFn("1, ...[], 2")(f), [1, 2]);
  assertEqArray(makeFn("1, ...[2, 3], 4, ...[5, 6]")(f), [1, 2, 3, 4, 5, 6]);

  assertEqArray(makeFn("...[undefined]")(f), [undefined]);
}

checkRest(function(...x) x, makeCall);
checkRest(function(...x) x, makeFunCall);
checkRest((...x) => x, makeCall);
checkRest((...x) => x, makeFunCall);
function restClass(...x) {
  this.value = x;
  assertEq(Object.getPrototypeOf(this), restClass.prototype);
}
checkRest(restClass, makeNew);
