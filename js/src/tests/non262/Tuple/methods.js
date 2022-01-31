// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

let tup = #[1,2,3];
let tup2 = #[1,2,3];
let empty = #[];

var tReversed = tup.toReversed();
assertEq(tReversed, #[3, 2, 1]);
assertEq(tup, tup2);
assertEq(#[].toReversed(), #[]);


let tup5 = #[42, 1, 5, 0, 333, 10];
let sorted_result = tup5.toSorted();
let expected_result = #[0, 1, 10, 333, 42, 5];
assertEq(sorted_result, expected_result);
let sorted_result2 = tup5.toSorted((x, y) => y > x);
let expected_result2 = #[333, 42, 10, 5, 1, 0];
assertEq(sorted_result2, expected_result2);

assertThrowsInstanceOf(() => tup5.toSorted("monkeys"),
                       TypeError,
                       "invalid Array.prototype.toSorted argument")

/* toSpliced */
/* examples from:
  https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/splice */

function unchanged(t) {
    assertEq(t, #['angel', 'clown', 'mandarin', 'sturgeon']);
}

// Remove no elements before index 2, insert "drum"
let myFish = #['angel', 'clown', 'mandarin', 'sturgeon']
var myFishSpliced = myFish.toSpliced(2, 0, 'drum')
unchanged(myFish);
assertEq(myFishSpliced, #['angel', 'clown', 'drum', 'mandarin', 'sturgeon']);


// Remove no elements before index 2, insert "drum" and "guitar"
myFishSpliced = myFish.toSpliced(2, 0, 'drum', 'guitar');
unchanged(myFish);
assertEq(myFishSpliced, #['angel', 'clown', 'drum', 'guitar', 'mandarin', 'sturgeon'])

// Remove 1 element at index 3
let myFish1 = #['angel', 'clown', 'drum', 'mandarin', 'sturgeon'];
myFishSpliced = myFish1.toSpliced(3, 1);
assertEq(myFish1, #['angel', 'clown', 'drum', 'mandarin', 'sturgeon']);
assertEq(myFishSpliced, #['angel', 'clown', 'drum', 'sturgeon']);

// Remove 1 element at index 2, and insert 'trumpet'
let myFish2 = #['angel', 'clown', 'drum', 'sturgeon']
myFishSpliced = myFish2.toSpliced(2, 1, 'trumpet');
assertEq(myFish2, #['angel', 'clown', 'drum', 'sturgeon']);
assertEq(myFishSpliced, #['angel', 'clown', 'trumpet', 'sturgeon']);

// Remove 2 elements at index 0, and insert 'parrot', 'anemone', and 'blue'
let myFish3 = #['angel', 'clown', 'trumpet', 'sturgeon']
myFishSpliced = myFish3.toSpliced(0, 2, 'parrot', 'anemone', 'blue');
assertEq(myFish3, #['angel', 'clown', 'trumpet', 'sturgeon']);
assertEq(myFishSpliced, #['parrot', 'anemone', 'blue', 'trumpet', 'sturgeon']);

// Remove 2 elements, starting at index 2
let myFish4 = #['parrot', 'anemone', 'blue', 'trumpet', 'sturgeon']
myFishSpliced = myFish4.toSpliced(2, 2);
assertEq(myFish4, #['parrot', 'anemone', 'blue', 'trumpet', 'sturgeon']);
assertEq(myFishSpliced, #['parrot', 'anemone', 'sturgeon']);

// Remove 1 element from index -2
myFishSpliced = myFish.toSpliced(-2, 1);
unchanged(myFish);
assertEq(myFishSpliced, #['angel', 'clown', 'sturgeon']);

// Remove all elements, starting from index 2
myFishSpliced = myFish.toSpliced(2);
unchanged(myFish);
assertEq(myFishSpliced, #['angel', 'clown']);

assertThrowsInstanceOf(() => myFish.toSpliced(1, 0, new Object(42)),
                       TypeError,
                       "Record and Tuple can only contain primitive values");

//******************
function concatTest(t, expected, ...args) {
    let result = t.concat(...args);
    assertEq(result, expected);
}

let tupConcat = tup.concat(#[4,5,6]);
assertEq(tup, tup2);
assertEq(tupConcat, #[1,2,3,4,5,6]);

concatTest(tup, tup, #[]);
concatTest(empty, tup, #[1,2,3]);
concatTest(tup, #[1,2,3,1,2,3,4,5,6],1,2,3,4,5,6);
concatTest(tup, #[1,2,3,1,2,3,4,5,6],1,#[2,3,4],5,6);
concatTest(tup, #[1,2,3,1,2,3,4],[1,2,3,4]);
concatTest(tup, #[1,2,3,1,2,3,4,5,6],1,[2,3,4],5,6);
concatTest(tup, #[1,2,3,1,2,3,4,5,6],[1,2,3],[4,5,6]);
concatTest(tup, #[1,2,3,1,2,3,4,5,6],#[1,2,3],#[4,5,6]);

// .includes()

assertEq(tup.includes(1), true);
assertEq(tup.includes(2), true);
assertEq(tup.includes(3), true);
assertEq(empty.includes(1), false);
assertEq(empty.includes(0), false);
assertEq(empty.includes(0, 1), false);
assertEq(tup.includes(2, 1), true);
assertEq(tup.includes(2, 2), false);
assertEq(tup.includes(2, -1), false);
assertEq(tup.includes(2, -2), true);
assertEq(tup.includes(0, Infinity), false);
assertEq(tup.includes(2, -Infinity), true);
assertEq(tup.includes(2, undefined), true);

// .indexOf()
assertEq(tup.indexOf(1), 0);
assertEq(tup.indexOf(2), 1);
assertEq(tup.indexOf(3), 2);
assertEq(empty.indexOf(1), -1);
assertEq(empty.indexOf(0), -1);
assertEq(empty.indexOf(0, 1), -1);
assertEq(tup.indexOf(2, 1), 1);
assertEq(tup.indexOf(2, 2), -1);
assertEq(tup.indexOf(2, -1), -1);
assertEq(tup.indexOf(2, -2), 1);
assertEq(tup.indexOf(0, Infinity), -1);
assertEq(tup.indexOf(2, -Infinity), 1);
assertEq(tup.indexOf(2, undefined), 1);

// .join()
assertEq(tup.join(), "1,2,3");
assertEq(tup.join("~"),"1~2~3");
assertEq(#[1].join(), "1");
assertEq(empty.join(), "");
assertEq(#[1,2,undefined,3].join(), "1,2,,3");
assertEq(#[1,null,2,3].join(), "1,,2,3");

// .lastIndexOf()
assertEq(tup.lastIndexOf(1), 0);
assertEq(tup.lastIndexOf(2), 1);
assertEq(tup.lastIndexOf(3), 2);
assertEq(empty.lastIndexOf(1), -1);
assertEq(empty.lastIndexOf(0), -1);
assertEq(empty.lastIndexOf(0, 1), -1);
assertEq(tup.lastIndexOf(2, 1), 1);
assertEq(tup.lastIndexOf(2, 0), -1);
assertEq(tup.lastIndexOf(2, -3), -1);
assertEq(tup.lastIndexOf(2, -2), 1);
assertEq(tup.lastIndexOf(2, -Infinity), -1);
var duplicates = #[1,2,3,1,3,1,5,6,1,2,10];
assertEq(duplicates.lastIndexOf(2), 9);
assertEq(duplicates.lastIndexOf(3, 2), 2);
assertEq(duplicates.lastIndexOf(3, -7), 4);
assertEq(duplicates.lastIndexOf(1), 8);
assertEq(duplicates.lastIndexOf(1, 0), 0);
assertEq(duplicates.lastIndexOf(1, -5), 5);

// .slice()
var sliced = empty.slice(2);
assertEq(empty, #[]);
assertEq(empty, sliced);
sliced = empty.slice(2, 3);
assertEq(empty, sliced);
sliced = tup.slice(1);
assertEq(tup, tup2);
assertEq(sliced, #[2,3]);
sliced = tup.slice(3);
assertEq(sliced, #[]);
sliced = tup.slice(0, 0);
assertEq(sliced, #[]);
sliced = tup.slice(0, 1);
assertEq(sliced, #[1]);
sliced = tup.slice(0, 3);
assertEq(sliced, tup);

// .toString()
assertEq(tup.toString(), "1,2,3");
assertEq(empty.toString(), "");
assertEq(#[1].toString(), "1");
assertEq(myFish.toString(), "angel,clown,mandarin,sturgeon");

// .toLocaleString() -- TODO more tests
assertEq(tup.toString(), tup.toLocaleString());
assertEq(empty.toString(), empty.toLocaleString());
assertEq(myFish.toString(), myFish.toLocaleString());

// .entries()
var iterator = tup.entries();
var result;
result = iterator.next();
assertEq(result.done, false);
assertEq(result.value[0], 0);
assertEq(result.value[1], 1);
assertEq(result.value.length, 2);

result = iterator.next();
assertEq(result.done, false);
assertEq(result.value[0], 1);
assertEq(result.value[1], 2);
assertEq(result.value.length, 2);

result = iterator.next();
assertEq(result.done, false);
assertEq(result.value[0], 2);
assertEq(result.value[1], 3);
assertEq(result.value.length, 2);

result = iterator.next();
assertEq(result.done, true);
assertEq(result.value, undefined);

iterator = empty.entries();
var result1 = iterator.next();
assertEq(result1.done, true);
assertEq(result1.value, undefined);

// .every()
var everyResult = tup.every(x => x < 10);
assertEq(tup, tup2);
assertEq(everyResult, true);
everyResult = tup.every(x => x < 2);
assertEq(everyResult, false);
assertEq(true, empty.every(x => a > 100));

assertThrowsInstanceOf(() => tup.every(),
                       TypeError,
                       "missing argument 0 when calling function Tuple.prototype.every");

assertThrowsInstanceOf(() => tup.every("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");

// .filter()
var filtered = tup.filter(x => x % 2 == 1);
assertEq(tup, tup2);
assertEq(filtered, #[1,3]);
assertEq(#[].filter(x => x), #[]);

assertThrowsInstanceOf(() => tup.filter(),
                       TypeError,
                       "missing argument 0 when calling function Tuple.prototype.filter");

assertThrowsInstanceOf(() => tup.filter("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");

// .find()
var findResult = tup.find(x => x > 2);
assertEq(tup, tup2);
assertEq(findResult, 3);
assertEq(#[].find(x => true), undefined);

assertThrowsInstanceOf(() => tup.find(),
                       TypeError,
                       "missing argument 0 when calling function Tuple.prototype.find");

assertThrowsInstanceOf(() => tup.find("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");

// .findIndex()
var findIndexResult = tup.findIndex(x => x > 2);
assertEq(tup, tup2);
assertEq(findIndexResult, 2);
assertEq(#[].findIndex(x => true), -1);

assertThrowsInstanceOf(() => tup.findIndex(),
                       TypeError,
                       "missing argument 0 when calling function Tuple.prototype.find");

assertThrowsInstanceOf(() => tup.findIndex("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");


// .forEach()
var a = 0;
var forEachResult = tup.forEach(x => a += x);
assertEq(tup, tup2);
assertEq(forEachResult, undefined);
assertEq(a, 6);

assertEq(undefined, empty.forEach(x => a += x));
assertEq(a, 6);

assertThrowsInstanceOf(() => tup.forEach(),
                       TypeError,
                       "missing argument 0 when calling function Tuple.prototype.forEach");

assertThrowsInstanceOf(() => tup.forEach("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");

// .keys()
var iterator = tup.keys();
var result;
result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 0);

result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 1);

result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 2);

result = iterator.next();
assertEq(result.done, true);
assertEq(result.value, undefined);

iterator = empty.keys();
var result1 = iterator.next();
assertEq(result1.done, true);
assertEq(result1.value, undefined);

// .map()
var mapResult = tup.map(x => x*x);
assertEq(tup, tup2);
assertEq(mapResult, #[1, 4, 9]);
assertEq(empty, empty.map(x => x*x));

assertThrowsInstanceOf(() => tup.map(x => new Object(x)),
                       TypeError,
                       "Record and Tuple can only contain primitive values");

assertThrowsInstanceOf(() => tup.map("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");

// .reduce()
var add = (previousValue, currentValue, currentIndex, O) =>
    previousValue + currentValue;
var reduceResult = tup.reduce(add);
assertEq(tup, tup2);
assertEq(reduceResult, 6);
assertEq(tup.reduce(add, 42), 48);
assertEq(0, empty.reduce(add, 0));

assertThrowsInstanceOf(() => tup.reduce(),
                       TypeError,
                       "Tuple.prototype.reduce");

assertThrowsInstanceOf(() => tup.reduce("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");

assertThrowsInstanceOf(() => empty.reduce(add),
                       TypeError,
                       "reduce of empty tuple with no initial value");

// .reduceRight()
var sub = (previousValue, currentValue, currentIndex, O) =>
    previousValue - currentValue;
var reduceResult = tup.reduceRight(sub);
assertEq(tup, tup2);
assertEq(reduceResult, 0);
assertEq(tup.reduceRight(sub, 42), 36);
assertEq(0, empty.reduceRight(sub, 0));

assertThrowsInstanceOf(() => tup.reduceRight(),
                       TypeError,
                       "Tuple.prototype.reduceRight");

assertThrowsInstanceOf(() => tup.reduceRight("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");

assertThrowsInstanceOf(() => empty.reduceRight(sub),
                       TypeError,
                       "reduce of empty tuple with no initial value");

// .some()
var truePred = x => x % 2 == 0;
var falsePred = x => x > 30;
var trueResult = tup.some(truePred);
assertEq(tup, tup2);
assertEq(trueResult, true);
var falseResult = tup.some(falsePred);
assertEq(falseResult, false);
assertEq(false, empty.some(truePred));

assertThrowsInstanceOf(() => tup.some(),
                       TypeError,
                       "Tuple.prototype.some");

assertThrowsInstanceOf(() => tup.some("monkeys"),
                       TypeError,
                       "\"monkeys\" is not a function");

// .values()
var iterator = tup.values();
var result;
result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 1);

result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 2);

result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 3);

result = iterator.next();
assertEq(result.done, true);
assertEq(result.value, undefined);

iterator = empty.values();
var result1 = iterator.next();
assertEq(result1.done, true);
assertEq(result1.value, undefined);

// @@iterator

var iterator = tup[Symbol.iterator](tup);
var result;
result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 1);

result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 2);

result = iterator.next();
assertEq(result.done, false);
assertEq(result.value, 3);

result = iterator.next();
assertEq(result.done, true);
assertEq(result.value, undefined);

iterator = empty[Symbol.iterator](empty);
var result1 = iterator.next();
assertEq(result1.done, true);
assertEq(result1.value, undefined);

// @@toStringTag

assertEq(tup[Symbol.toStringTag], "Tuple");
assertEq(Object(#[1,2,3])[Symbol.toStringTag], "Tuple");

// length
assertEq(tup.length, 3);
assertEq(Object(#[1,2,3]).length, 3);
assertEq(empty.length, 0);
assertEq(Object(#[]).length, 0);

// .flat()
var toFlatten = #[#[1,2],#[3,#[4,5]],#[6],#[7,8,#[9,#[10,#[11,12]]]]];
var toFlatten2 = #[#[1,2],#[3,#[4,5]],#[6],#[7,8,#[9,#[10,#[11,12]]]]];
assertEq(toFlatten.flat(10), #[1,2,3,4,5,6,7,8,9,10,11,12]);
assertEq(toFlatten, toFlatten2);
assertEq(tup.flat(), tup);
assertEq(empty.flat(), empty);
assertEq(toFlatten.flat(2), #[1,2,3,4,5,6,7,8,9,#[10,#[11,12]]]);
assertEq(toFlatten.flat(), #[1,2,3,#[4,5],6,7,8,#[9,#[10,#[11,12]]]]);

// .flatMap()
var inc = (x, sourceIndex, source) => #[x, x+1];
var toFlatten0 = #[1, 2, 3];
assertEq(toFlatten0.flatMap(inc), #[1, 2, 2, 3, 3, 4]);
assertEq(empty.flatMap(inc), empty);

// miscellaneous

let nullaryMethods = [[Tuple.prototype.toReversed, x => x === #[1,2,3]],
                      [Tuple.prototype.toSorted, x => x === #[1,2,3]],
                      [Tuple.prototype.toString, x => x === "3,2,1"],
                      [Tuple.prototype.toLocaleString, x => x === "3,2,1"],
                      [Tuple.prototype.join, x => x === "3,2,1"],
                      [Tuple.prototype.entries, x => typeof(x) === "object"],
                      [Tuple.prototype.keys, x => typeof(x) === "object"],
                      [Tuple.prototype.values, x => typeof(x) === "object"],
                      [Tuple.prototype.flat, x => x === #[3,2,1]]];

for (p of nullaryMethods) {
    let method = p[0];
    let f = p[1];
    assertEq(f(method.call(Object(#[3,2,1]))), true);
}

function assertTypeError(f) {
    for (thisVal of ["monkeys", [3,2,1], null, undefined, 0]) {
        assertThrowsInstanceOf(f(thisVal), TypeError, "value of TupleObject must be a Tuple");
    }
}

assertTypeError(x => (() => Tuple.prototype.toSorted.call(x)));

assertEq(Tuple.prototype.toSpliced.call(Object(myFish), 2, 0, 'drum'),
         #['angel', 'clown', 'drum', 'mandarin', 'sturgeon']);
assertTypeError(thisVal => (() => Tuple.prototype.toSpliced.call(thisVal, 2, 0, 'drum')));

assertEq(Tuple.prototype.concat.call(Object(#[1,2,3]), 1,2,3,4,5,6), #[1,2,3,1,2,3,4,5,6]);
assertEq(Tuple.prototype.concat.call(Object(#[1,2,3]), 1,2,Object(#[3,4]),5,6), #[1,2,3,1,2,3,4,5,6]);
assertTypeError(thisVal => (() => Tuple.prototype.concat.call(thisVal, 1, 2, 3, 4)));

assertEq(Tuple.prototype.includes.call(Object(#[1,2,3]), 1), true);
assertTypeError(thisVal => (() => Tuple.prototype.concat.includes(thisVal, 1)));

assertEq(Tuple.prototype.indexOf.call(Object(#[1,2,3]), 1), 0);
assertTypeError(thisVal => (() => Tuple.prototype.indexOf.call(thisVal, 0)));

assertEq(Tuple.prototype.lastIndexOf.call(Object(#[1,2,3]), 1), 0);
assertTypeError(thisVal => (() => Tuple.prototype.lastIndexOf.call(thisVal, 0)));

assertEq(Tuple.prototype.slice.call(Object(#[1,2,3]), 1), #[2,3]);
assertTypeError(thisVal => (() => Tuple.prototype.slice.call(thisVal, 0)));

var pred = x => x > 2;

assertEq(Tuple.prototype.every.call(Object(#[1,2,3]), pred), false);
assertTypeError(thisVal => (() => Tuple.prototype.every.call(thisVal, pred)));

assertEq(Tuple.prototype.filter.call(Object(#[1,2,3]), pred), #[3]);
assertTypeError(thisVal => (() => Tuple.prototype.filter.call(thisVal, pred)));

assertEq(Tuple.prototype.find.call(Object(#[1,2,3]), pred), 3);
assertTypeError(thisVal => (() => Tuple.prototype.find.call(thisVal, pred)));

assertEq(Tuple.prototype.findIndex.call(Object(#[1,2,3]), pred), 2);
assertTypeError(thisVal => (() => Tuple.prototype.findIndex.call(thisVal, pred)));

assertEq(Tuple.prototype.some.call(Object(#[1,2,3]), pred), true);
assertTypeError(thisVal => (() => Tuple.prototype.some.call(thisVal, pred)));

var a = 0;
var f = (x => a += x);
assertEq(Tuple.prototype.forEach.call(Object(#[1,2,3]), f), undefined);
assertEq(a, 6);
assertTypeError(thisVal => (() => Tuple.prototype.forEach.call(thisVal, f)));

f = (x => x+1);
assertEq(Tuple.prototype.map.call(Object(#[1,2,3]), f), #[2,3,4]);
assertTypeError(thisVal => (() => Tuple.prototype.map.call(thisVal, f)));

f = (x => #[x,x+1]);
assertEq(Tuple.prototype.flatMap.call(Object(#[1,2,3]), f), #[1,2,2,3,3,4]);
assertTypeError(thisVal => (() => Tuple.prototype.flatMap.call(thisVal, f)));

assertEq(Tuple.prototype.reduce.call(Object(#[1,2,3]), add), 6);
assertTypeError(thisVal => (() => Tuple.prototype.reduce.call(thisVal, add)));

assertEq(Tuple.prototype.reduceRight.call(Object(#[1,2,3]), sub), 0);
assertTypeError(thisVal => (() => Tuple.prototype.reduce.call(thisVal, sub)));

if (typeof reportCompare === "function") reportCompare(0, 0);
