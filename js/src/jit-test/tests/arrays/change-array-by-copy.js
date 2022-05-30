// |jit-test| --enable-change-array-by-copy; skip-if: !getBuildConfiguration()['change-array-by-copy']

load(libdir + 'array-compare.js');
load(libdir + "asserts.js");

var sequence = [1, 2, 3];
let reversedSequence = sequence.withReversed();
assertEq(arraysEqual(sequence, [1, 2, 3]), true);
assertEq(arraysEqual(reversedSequence, [3, 2, 1]), true);

sequence = [87, 3, 5, 888, 321, 42];
var sortedSequence = sequence.withSorted((x, y) => (x >= y));
assertEq(arraysEqual(sequence, [87, 3, 5, 888, 321, 42]), true);
assertEq(arraysEqual(sortedSequence, [3, 5, 42, 87, 321, 888]), true);

sequence = ["the", "quick", "fox", "jumped", "over", "the", "lazy", "dog"];
sortedSequence = sequence.withSorted();
assertEq(arraysEqual(sequence, ["the", "quick", "fox", "jumped", "over", "the", "lazy", "dog"]), true);
assertEq(arraysEqual(sortedSequence, ["dog", "fox", "jumped", "lazy", "over", "quick", "the", "the"]), true);

/* Test that the correct exception is thrown with a
   non-function comparefn argument */
assertThrowsInstanceOf(() => sequence.withSorted([1, 2, 3]), TypeError);

/* withSpliced */
/* examples from:
  https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/splice */

function unchanged(a) {
    assertEq(arraysEqual(a, ['angel', 'clown', 'mandarin', 'sturgeon']), true);
}

// Remove no elements before index 2, insert "drum"
let myFish = ['angel', 'clown', 'mandarin', 'sturgeon']
var myFishSpliced = myFish.withSpliced(2, 0, 'drum')
unchanged(myFish);
assertEq(arraysEqual(myFishSpliced, ['angel', 'clown', 'drum', 'mandarin', 'sturgeon']), true);


// Remove no elements before index 2, insert "drum" and "guitar"
var myFishSpliced = myFish.withSpliced(2, 0, 'drum', 'guitar');
unchanged(myFish);
assertEq(arraysEqual(myFishSpliced, ['angel', 'clown', 'drum', 'guitar', 'mandarin', 'sturgeon']), true);

// Remove 1 element at index 3
let myFish1 = ['angel', 'clown', 'drum', 'mandarin', 'sturgeon']
myFishSpliced = myFish1.withSpliced(3, 1);
assertEq(arraysEqual(myFish1, ['angel', 'clown', 'drum', 'mandarin', 'sturgeon']), true);
assertEq(arraysEqual(myFishSpliced, ['angel', 'clown', 'drum', 'sturgeon']), true);

// Remove 1 element at index 2, and insert 'trumpet'
let myFish2 = ['angel', 'clown', 'drum', 'sturgeon']
myFishSpliced = myFish2.withSpliced(2, 1, 'trumpet');
assertEq(arraysEqual(myFish2, ['angel', 'clown', 'drum', 'sturgeon']), true);
assertEq(arraysEqual(myFishSpliced, ['angel', 'clown', 'trumpet', 'sturgeon']), true);

// Remove 2 elements at index 0, and insert 'parrot', 'anemone', and 'blue'
let myFish3 = ['angel', 'clown', 'trumpet', 'sturgeon']
myFishSpliced = myFish3.withSpliced(0, 2, 'parrot', 'anemone', 'blue');
assertEq(arraysEqual(myFish3, ['angel', 'clown', 'trumpet', 'sturgeon']), true);
assertEq(arraysEqual(myFishSpliced, ['parrot', 'anemone', 'blue', 'trumpet', 'sturgeon']), true);

// Remove 2 elements, starting at index 2
let myFish4 = ['parrot', 'anemone', 'blue', 'trumpet', 'sturgeon']
myFishSpliced = myFish4.withSpliced(2, 2);
assertEq(arraysEqual(myFish4, ['parrot', 'anemone', 'blue', 'trumpet', 'sturgeon']), true);
assertEq(arraysEqual(myFishSpliced, ['parrot', 'anemone', 'sturgeon']), true);

// Remove 1 element from index -2
myFishSpliced = myFish.withSpliced(-2, 1);
unchanged(myFish);
assertEq(arraysEqual(myFishSpliced, ['angel', 'clown', 'sturgeon']), true);

// Remove all elements, starting from index 2
myFishSpliced = myFish.withSpliced(2);
unchanged(myFish);
assertEq(arraysEqual(myFishSpliced, ['angel', 'clown']), true);

/* withAt */
sequence = [1, 2, 3];
let seq_withAt = sequence.withAt(1, 42);
assertEq(arraysEqual(sequence, [1, 2, 3]), true);
assertEq(arraysEqual(seq_withAt, [1, 42, 3]), true);
assertEq(arraysEqual(sequence.withAt(-0, 42), [42, 2, 3]), true);

/* false/true => index 0/1 */
assertEq(arraysEqual(sequence.withAt(false, 42), [42, 2, 3]), true);
assertEq(arraysEqual(sequence.withAt(true, 42), [1, 42, 3]), true);

/* null => 0 */
assertEq(arraysEqual(sequence.withAt(null, 42), [42, 2, 3]), true);
/* [] => 0 */
assertEq(arraysEqual(sequence.withAt([], 42), [42, 2, 3]), true);

assertEq(arraysEqual(sequence.withAt("2", 42), [1, 2, 42]), true);

/* Non-numeric indices => 0 */
assertEq(arraysEqual(sequence.withAt("monkeys", 42), [42, 2, 3]), true);
assertEq(arraysEqual(sequence.withAt(undefined, 42), [42, 2, 3]), true);
assertEq(arraysEqual(sequence.withAt(function() {}, 42), [42, 2, 3]), true);

assertThrowsInstanceOf(() => sequence.withAt(3, 42), RangeError);
assertThrowsInstanceOf(() => sequence.withAt(5, 42), RangeError);
assertThrowsInstanceOf(() => sequence.withAt(-10, 42), RangeError);
assertThrowsInstanceOf(() => sequence.withAt(Infinity, 42), RangeError);

