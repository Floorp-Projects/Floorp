function testConcat(o1, o2, len, json) {
    let res = Array.prototype.concat.call(o1, o2);
    assertEq(res.length, len);
    assertEq(JSON.stringify(res), json);
}

const o1 = {[Symbol.isConcatSpreadable]: true, length: 2, 0: 4, 1: 5, 2: 6};
const o2 = {[Symbol.isConcatSpreadable]: true, length: 2, 0: 4, 2: 6};

testConcat([1, 2, 3], o1, 5, "[1,2,3,4,5]");
testConcat([1, 2, 3], o2, 5, "[1,2,3,4,null]");

testConcat(o1, [1, 2, 3], 5, "[4,5,1,2,3]");
testConcat(o2, [1, 2, 3], 5, "[4,null,1,2,3]");

testConcat(o1, o1, 4, "[4,5,4,5]");
testConcat(o2, o2, 4, "[4,null,4,null]");

testConcat(o1, o2, 4, "[4,5,4,null]");
testConcat(o2, o1, 4, "[4,null,4,5]");

o1[Symbol.isConcatSpreadable] = false;

testConcat(o1, o2, 3, '[{"0":4,"1":5,"2":6,"length":2},4,null]');
testConcat(o2, o1, 3, '[4,null,{"0":4,"1":5,"2":6,"length":2}]');

const ta = new Int32Array([4, 5, 6]);
testConcat([1, 2, 3], ta, 4, '[1,2,3,{"0":4,"1":5,"2":6}]');
testConcat(ta, ta, 2, '[{"0":4,"1":5,"2":6},{"0":4,"1":5,"2":6}]');

ta[Symbol.isConcatSpreadable] = true;
testConcat([1, 2, 3], ta, 6, "[1,2,3,4,5,6]");
testConcat(ta, ta, 6, "[4,5,6,4,5,6]");
