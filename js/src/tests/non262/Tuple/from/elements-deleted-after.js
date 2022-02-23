// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var originalTuple = #[0, 1, -2, 4, -8, 16];
var array = [0,1,-2,4,-8,16];
var t = #[];
var arrayIndex = -1;

function mapFn(value, index) {
    this.arrayIndex++;
    assertEq(value, array[this.arrayIndex]);
    assertEq(index, this.arrayIndex);
    array.splice(array.length - 1, 1);
    return 127;
}


t = Tuple.from(array, mapFn, this);

assertEq(t.length, originalTuple.length / 2);

for (var j = 0; j < originalTuple.length / 2; j++) {
    assertEq(t[j], 127);
}

reportCompare(0, 0);
