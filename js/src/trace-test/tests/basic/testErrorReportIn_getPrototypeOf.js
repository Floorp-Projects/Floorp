actual = "";
expect = "TypeError: \"kittens\" is not an object";
try {
    Object.getPrototypeOf.apply(null, ["kittens",4,3])
} catch (e) {
    actual = "" + e;
}

assertEq(actual, expect);
