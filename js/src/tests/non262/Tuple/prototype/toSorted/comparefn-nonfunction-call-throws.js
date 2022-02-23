// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
  1. If _comparefn_ is not *undefined* and IsCallable(_comparefn_) is *false*, throw a *TypeError* exception.
  ...
*/

let sample = #[42, 43, 44, 45, 46];

let compareFnVals = [[null, "null"],
                     [true, "true"],
                     [false, "false"],
                     ['', "\'\'"],
                     [/a/g, "/a/g"],
                     [42, "42"],
                     [[], "[]"],
                     [{}, "{}"]];

for (pair of compareFnVals) {
    f = pair[0];
    errorMsg = "comparefn is: " + pair[1];
    assertThrowsInstanceOf(() => sample.withSorted(f),
                           TypeError, errorMsg);
}

reportCompare(0, 0);
