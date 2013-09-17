let makeCall    = farg => Function("f", "arg", "return f(" + farg + ");");
let makeFunCall = farg => Function("f", "arg", "return f.call(null, " + farg + ");");
let makeNew     = farg => Function("f", "arg", "return new f(" + farg + ").length;");

function checkLength(f, makeFn) {
  assertEq(makeFn("...[1, 2, 3]")(f), 3);
  assertEq(makeFn("1, ...[2], 3")(f), 3);
  assertEq(makeFn("1, ...[2], ...[3]")(f), 3);
  assertEq(makeFn("1, ...[2, 3]")(f), 3);
  assertEq(makeFn("1, ...[], 2, 3")(f), 3);

  assertEq(makeFn("...[1]")(f), 1);
  assertEq(makeFn("...[1, 2]")(f), 2);
  assertEq(makeFn("...[1, 2, 3, 4]")(f), 4);
  assertEq(makeFn("1, ...[2, 3, 4], 5")(f), 5);

  assertEq(makeFn("...[undefined]")(f), 1);

  // other iterable objects
  assertEq(makeFn("...arg")(f, Int32Array([1, 2, 3])), 3);
  assertEq(makeFn("...arg")(f, "abc"), 3);
  assertEq(makeFn("...arg")(f, [1, 2, 3].iterator()), 3);
  assertEq(makeFn("...arg")(f, Set([1, 2, 3])), 3);
  assertEq(makeFn("...arg")(f, Map([["a", "A"], ["b", "B"], ["c", "C"]])), 3);
  let itr = {
    iterator: function() {
      return {
        i: 1,
        next: function() {
          if (this.i < 4)
            return this.i++;
          else
            throw StopIteration;
        }
      };
    }
  };
  assertEq(makeFn("...arg")(f, itr), 3);
  let gen = {
    iterator: function() {
      for (let i = 1; i < 4; i ++)
        yield i;
    }
  };
  assertEq(makeFn("...arg")(f, gen), 3);
}

checkLength(function(x) arguments.length, makeCall);
checkLength(function(x) arguments.length, makeFunCall);
checkLength((x) => arguments.length, makeCall);
checkLength((x) => arguments.length, makeFunCall);
function lengthClass(x) {
  this.length = arguments.length;
}
checkLength(lengthClass, makeNew);
