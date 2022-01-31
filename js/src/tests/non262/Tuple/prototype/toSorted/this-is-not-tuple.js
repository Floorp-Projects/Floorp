// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var withSorted = Tuple.prototype.withSorted;

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
    assertThrowsInstanceOf(() => withSorted.call(thisVal),
                           TypeError, errorMsg);
}

reportCompare(0, 0);
