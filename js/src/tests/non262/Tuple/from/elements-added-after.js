// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var tupleIndex = -1;
var originalLength = 7;
var obj = {
  length: originalLength,
  0: 2,
  1: 4,
  2: 8,
  3: 16,
  4: 32,
  5: 64,
  6: 128
};
var tuple = #[2, 4, 8, 16, 32, 64, 128];

function mapFn(value, index) {
    tupleIndex++;
    assertEq(value, obj[tupleIndex]);
    assertEq(index, tupleIndex);
    obj[originalLength + tupleIndex] = 2 * tupleIndex + 1;

    return obj[tupleIndex];
}


var t = Tuple.from(obj, mapFn);
assertEq(t, tuple);

reportCompare(0, 0);
