// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var withReversed = Tuple.prototype.withReversed;

var thisVals = [[undefined, "undefined"],
                [null, "null"],
                [42, "42"],
                ["1", "1"],
                [true, "true"],
                [false, "false"],
                [Symbol("s"), "Symbol(\"s\")"],
                [[], "[]"],
                [{}, "{}"]];

for (pair of thisVals) {
    thisVal = pair[0];
    errorMsg = "this is: " + pair[1];
    assertThrowsInstanceOf(() => withReversed.call(thisVal),
                           TypeError, errorMsg);
}

reportCompare(0, 0);
