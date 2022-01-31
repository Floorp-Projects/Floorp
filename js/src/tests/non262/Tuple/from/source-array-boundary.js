// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var array = [Number.MAX_VALUE, Number.MIN_VALUE, Number.NaN, Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY];
var arrayIndex = -1;

function mapFn(value, index) {
    this.arrayIndex++;
    assertEq(value, array[this.arrayIndex]);
    assertEq(index, this.arrayIndex);
    return value;
}

var t = Tuple.from(array, mapFn, this);

assertEq(t.length, array.length);
assertEq(t[0], Number.MAX_VALUE);
assertEq(t[1], Number.MIN_VALUE);
assertEq(t[2], Number.NaN);
assertEq(t[3], Number.NEGATIVE_INFINITY);
assertEq(t[4], Number.POSITIVE_INFINITY);

reportCompare(0, 0);
