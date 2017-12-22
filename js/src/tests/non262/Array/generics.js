var BUGNUMBER = 1263558;
var summary = "Self-host all Array generics.";

print(BUGNUMBER + ": " + summary);

var arr, arrLike, tmp, f;

function reset() {
  arr = [5, 7, 13];
  arrLike = {
    length: 3,
    0: 5,
    1: 7,
    2: 13,
    toString() {
      return "arrLike";
    }
  };
  tmp = [];
}
function toString() {
  return "G";
}

// Array.join (test this first to use it in remaining tests).
reset();
assertThrowsInstanceOf(() => Array.join(), TypeError);
assertEq(Array.join(arr), "5,7,13");
assertEq(Array.join(arr, "-"), "5-7-13");
assertEq(Array.join(arrLike), "5,7,13");
assertEq(Array.join(arrLike, "-"), "5-7-13");

// Array.concat.
reset();
assertThrowsInstanceOf(() => Array.concat(), TypeError);
assertEq(Array.join(Array.concat(arr), ","), "5,7,13");
assertEq(Array.join(Array.concat(arr, 11), ","), "5,7,13,11");
assertEq(Array.join(Array.concat(arr, 11, 17), ","), "5,7,13,11,17");
assertEq(Array.join(Array.concat(arrLike), ","), "arrLike");
assertEq(Array.join(Array.concat(arrLike, 11), ","), "arrLike,11");
assertEq(Array.join(Array.concat(arrLike, 11, 17), ","), "arrLike,11,17");

// Array.lastIndexOf.
reset();
assertThrowsInstanceOf(() => Array.lastIndexOf(), TypeError);
assertEq(Array.lastIndexOf(arr), -1);
assertEq(Array.lastIndexOf(arr, 1), -1);
assertEq(Array.lastIndexOf(arr, 5), 0);
assertEq(Array.lastIndexOf(arr, 7), 1);
assertEq(Array.lastIndexOf(arr, 13, 1), -1);
assertEq(Array.lastIndexOf(arrLike), -1);
assertEq(Array.lastIndexOf(arrLike, 1), -1);
assertEq(Array.lastIndexOf(arrLike, 5), 0);
assertEq(Array.lastIndexOf(arrLike, 7), 1);
assertEq(Array.lastIndexOf(arrLike, 13, 1), -1);

// Array.indexOf.
reset();
assertThrowsInstanceOf(() => Array.indexOf(), TypeError);
assertEq(Array.indexOf(arr), -1);
assertEq(Array.indexOf(arr, 1), -1);
assertEq(Array.indexOf(arr, 5), 0);
assertEq(Array.indexOf(arr, 7), 1);
assertEq(Array.indexOf(arr, 1, 5), -1);
assertEq(Array.indexOf(arrLike), -1);
assertEq(Array.indexOf(arrLike, 1), -1);
assertEq(Array.indexOf(arrLike, 5), 0);
assertEq(Array.indexOf(arrLike, 7), 1);
assertEq(Array.indexOf(arrLike, 1, 5), -1);

// Array.forEach.
reset();
assertThrowsInstanceOf(() => Array.forEach(), TypeError);
assertThrowsInstanceOf(() => Array.forEach(arr), TypeError);
assertThrowsInstanceOf(() => Array.forEach(arrLike), TypeError);
f = function(...args) {
  tmp.push(this, ...args);
};
tmp = [];
Array.forEach(arr, f);
assertEq(tmp.join(","), "G,5,0,5,7,13," + "G,7,1,5,7,13," + "G,13,2,5,7,13");
tmp = [];
Array.forEach(arr, f, "T");
assertEq(tmp.join(","), "T,5,0,5,7,13," + "T,7,1,5,7,13," + "T,13,2,5,7,13");
tmp = [];
Array.forEach(arrLike, f);
assertEq(tmp.join(","), "G,5,0,arrLike," + "G,7,1,arrLike," + "G,13,2,arrLike");
tmp = [];
Array.forEach(arrLike, f, "T");
assertEq(tmp.join(","), "T,5,0,arrLike," + "T,7,1,arrLike," + "T,13,2,arrLike");

// Array.map.
reset();
assertThrowsInstanceOf(() => Array.map(), TypeError);
assertThrowsInstanceOf(() => Array.map(arr), TypeError);
assertThrowsInstanceOf(() => Array.map(arrLike), TypeError);
f = function(...args) {
  tmp.push(this, ...args);
  return args[0] * 2;
}
tmp = [];
assertEq(Array.join(Array.map(arr, f), ","), "10,14,26");
assertEq(tmp.join(","), "G,5,0,5,7,13," + "G,7,1,5,7,13," + "G,13,2,5,7,13");
tmp = [];
assertEq(Array.join(Array.map(arr, f, "T"), ","), "10,14,26");
assertEq(tmp.join(","), "T,5,0,5,7,13," + "T,7,1,5,7,13," + "T,13,2,5,7,13");
tmp = [];
assertEq(Array.join(Array.map(arrLike, f), ","), "10,14,26");
assertEq(tmp.join(","), "G,5,0,arrLike," + "G,7,1,arrLike," + "G,13,2,arrLike");
tmp = [];
assertEq(Array.join(Array.map(arrLike, f, "T"), ","), "10,14,26");
assertEq(tmp.join(","), "T,5,0,arrLike," + "T,7,1,arrLike," + "T,13,2,arrLike");

// Array.filter.
reset();
assertThrowsInstanceOf(() => Array.filter(), TypeError);
assertThrowsInstanceOf(() => Array.filter(arr), TypeError);
assertThrowsInstanceOf(() => Array.filter(arrLike), TypeError);
f = function(...args) {
  tmp.push(this, ...args);
  return args[0] < 10;
}
tmp = [];
assertEq(Array.join(Array.filter(arr, f), ","), "5,7");
assertEq(tmp.join(","), "G,5,0,5,7,13," + "G,7,1,5,7,13," + "G,13,2,5,7,13");
tmp = [];
assertEq(Array.join(Array.filter(arr, f, "T"), ","), "5,7");
assertEq(tmp.join(","), "T,5,0,5,7,13," + "T,7,1,5,7,13," + "T,13,2,5,7,13");
tmp = [];
assertEq(Array.join(Array.filter(arrLike, f), ","), "5,7");
assertEq(tmp.join(","), "G,5,0,arrLike," + "G,7,1,arrLike," + "G,13,2,arrLike");
tmp = [];
assertEq(Array.join(Array.filter(arrLike, f, "T"), ","), "5,7");
assertEq(tmp.join(","), "T,5,0,arrLike," + "T,7,1,arrLike," + "T,13,2,arrLike");

// Array.every.
reset();
assertThrowsInstanceOf(() => Array.every(), TypeError);
assertThrowsInstanceOf(() => Array.every(arr), TypeError);
assertThrowsInstanceOf(() => Array.every(arrLike), TypeError);
f = function(...args) {
  tmp.push(this, ...args);
  return args[0] < 6;
}
tmp = [];
assertEq(Array.every(arr, f), false);
assertEq(tmp.join(","), "G,5,0,5,7,13," + "G,7,1,5,7,13");
tmp = [];
assertEq(Array.every(arr, f, "T"), false);
assertEq(tmp.join(","), "T,5,0,5,7,13," + "T,7,1,5,7,13");
tmp = [];
assertEq(Array.every(arrLike, f), false);
assertEq(tmp.join(","), "G,5,0,arrLike," + "G,7,1,arrLike");
tmp = [];
assertEq(Array.every(arrLike, f, "T"), false);
assertEq(tmp.join(","), "T,5,0,arrLike," + "T,7,1,arrLike");

// Array.some.
reset();
assertThrowsInstanceOf(() => Array.some(), TypeError);
assertThrowsInstanceOf(() => Array.some(arr), TypeError);
assertThrowsInstanceOf(() => Array.some(arrLike), TypeError);
f = function(...args) {
  tmp.push(this, ...args);
  return args[0] == 7;
}
tmp = [];
assertEq(Array.some(arr, f), true);
assertEq(tmp.join(","), "G,5,0,5,7,13," + "G,7,1,5,7,13");
tmp = [];
assertEq(Array.some(arr, f, "T"), true);
assertEq(tmp.join(","), "T,5,0,5,7,13," + "T,7,1,5,7,13");
tmp = [];
assertEq(Array.some(arrLike, f), true);
assertEq(tmp.join(","), "G,5,0,arrLike," + "G,7,1,arrLike");
tmp = [];
assertEq(Array.some(arrLike, f, "T"), true);
assertEq(tmp.join(","), "T,5,0,arrLike," + "T,7,1,arrLike");

// Array.reduce.
reset();
assertThrowsInstanceOf(() => Array.reduce(), TypeError);
assertThrowsInstanceOf(() => Array.reduce(arr), TypeError);
assertThrowsInstanceOf(() => Array.reduce(arrLike), TypeError);
f = function(...args) {
  tmp.push(...args);
  return args[0] + args[1];
}
tmp = [];
assertEq(Array.reduce(arr, f), 25);
assertEq(tmp.join(","), "5,7,1,5,7,13," + "12,13,2,5,7,13");
tmp = [];
assertEq(Array.reduce(arr, f, 17), 42);
assertEq(tmp.join(","), "17,5,0,5,7,13," + "22,7,1,5,7,13," + "29,13,2,5,7,13");
tmp = [];
assertEq(Array.reduce(arrLike, f), 25);
assertEq(tmp.join(","), "5,7,1,arrLike," + "12,13,2,arrLike");
tmp = [];
assertEq(Array.reduce(arrLike, f, 17), 42);
assertEq(tmp.join(","), "17,5,0,arrLike," + "22,7,1,arrLike," + "29,13,2,arrLike");

// Array.reduceRight.
reset();
assertThrowsInstanceOf(() => Array.reduceRight(), TypeError);
assertThrowsInstanceOf(() => Array.reduceRight(arr), TypeError);
assertThrowsInstanceOf(() => Array.reduceRight(arrLike), TypeError);
f = function(...args) {
  tmp.push(...args);
  return args[0] + args[1];
}
tmp = [];
assertEq(Array.reduceRight(arr, f), 25);
assertEq(tmp.join(","), "13,7,1,5,7,13," + "20,5,0,5,7,13");
tmp = [];
assertEq(Array.reduceRight(arr, f, 17), 42);
assertEq(tmp.join(","), "17,13,2,5,7,13," + "30,7,1,5,7,13," + "37,5,0,5,7,13");
tmp = [];
assertEq(Array.reduceRight(arrLike, f), 25);
assertEq(tmp.join(","), "13,7,1,arrLike," + "20,5,0,arrLike");
tmp = [];
assertEq(Array.reduceRight(arrLike, f, 17), 42);
assertEq(tmp.join(","), "17,13,2,arrLike," + "30,7,1,arrLike," + "37,5,0,arrLike");

// Array.reverse.
reset();
assertThrowsInstanceOf(() => Array.reverse(), TypeError);
assertEq(Array.join(Array.reverse(arr), ","), "13,7,5");
assertEq(Array.join(arr, ","), "13,7,5");
assertEq(Array.join(Array.reverse(arrLike), ","), "13,7,5");
assertEq(Array.join(arrLike, ","), "13,7,5");

// Array.sort.
reset();
assertThrowsInstanceOf(() => Array.sort(), TypeError);
f = function(x, y) {
  return y - x;
}
assertEq(Array.join(Array.sort(arr), ","), "13,5,7");
assertEq(Array.join(Array.sort(arr, f), ","), "13,7,5");
assertEq(Array.join(Array.sort(arrLike), ","), "13,5,7");
assertEq(Array.join(Array.sort(arrLike, f), ","), "13,7,5");

// Array.push.
reset();
assertThrowsInstanceOf(() => Array.push(), TypeError);
assertEq(Array.push(arr), 3);
assertEq(Array.join(arr), "5,7,13");
assertEq(Array.push(arr, 17), 4);
assertEq(Array.join(arr), "5,7,13,17");
assertEq(Array.push(arr, 19, 21), 6);
assertEq(Array.join(arr), "5,7,13,17,19,21");
assertEq(Array.push(arrLike), 3);
assertEq(Array.join(arrLike), "5,7,13");
assertEq(Array.push(arrLike, 17), 4);
assertEq(Array.join(arrLike), "5,7,13,17");
assertEq(Array.push(arrLike, 19, 21), 6);
assertEq(Array.join(arrLike), "5,7,13,17,19,21");

// Array.pop.
reset();
assertThrowsInstanceOf(() => Array.pop(), TypeError);
assertEq(Array.pop(arr), 13);
assertEq(Array.join(arr), "5,7");
assertEq(Array.pop(arr), 7);
assertEq(Array.join(arr), "5");
assertEq(Array.pop(arrLike), 13);
assertEq(Array.join(arrLike), "5,7");
assertEq(Array.pop(arrLike), 7);
assertEq(Array.join(arrLike), "5");

// Array.shift.
reset();
assertThrowsInstanceOf(() => Array.shift(), TypeError);
assertEq(Array.shift(arr), 5);
assertEq(Array.join(arr), "7,13");
assertEq(Array.shift(arr), 7);
assertEq(Array.join(arr), "13");
assertEq(Array.shift(arrLike), 5);
assertEq(Array.join(arrLike), "7,13");
assertEq(Array.shift(arrLike), 7);
assertEq(Array.join(arrLike), "13");

// Array.unshift.
reset();
assertThrowsInstanceOf(() => Array.unshift(), TypeError);
assertEq(Array.unshift(arr), 3);
assertEq(Array.join(arr), "5,7,13");
assertEq(Array.unshift(arr, 17), 4);
assertEq(Array.join(arr), "17,5,7,13");
assertEq(Array.unshift(arr, 19, 21), 6);
assertEq(Array.join(arr), "19,21,17,5,7,13");
assertEq(Array.unshift(arrLike), 3);
assertEq(Array.join(arrLike), "5,7,13");
assertEq(Array.unshift(arrLike, 17), 4);
assertEq(Array.join(arrLike), "17,5,7,13");
assertEq(Array.unshift(arrLike, 19, 21), 6);
assertEq(Array.join(arrLike), "19,21,17,5,7,13");

// Array.splice.
reset();
assertThrowsInstanceOf(() => Array.splice(), TypeError);
assertEq(Array.join(Array.splice(arr)), "");
assertEq(Array.join(arr), "5,7,13");
assertEq(Array.join(Array.splice(arr, 1)), "7,13");
assertEq(Array.join(arr), "5");
reset();
assertEq(Array.join(Array.splice(arr, 1, 1)), "7");
assertEq(Array.join(arr), "5,13");
reset();
assertEq(Array.join(Array.splice(arrLike)), "");
assertEq(Array.join(arrLike), "5,7,13");
assertEq(Array.join(Array.splice(arrLike, 1)), "7,13");
assertEq(Array.join(arrLike), "5");
reset();
assertEq(Array.join(Array.splice(arrLike, 1, 1)), "7");
assertEq(Array.join(arrLike), "5,13");

// Array.slice.
reset();
assertThrowsInstanceOf(() => Array.slice(), TypeError);
assertEq(Array.join(Array.slice(arr)), "5,7,13");
assertEq(Array.join(Array.slice(arr, 1)), "7,13");
assertEq(Array.join(Array.slice(arr, 1, 1)), "");
assertEq(Array.join(Array.slice(arr, 1, 2)), "7");
assertEq(Array.join(Array.slice(arrLike)), "5,7,13");
assertEq(Array.join(Array.slice(arrLike, 1)), "7,13");
assertEq(Array.join(Array.slice(arrLike, 1, 1)), "");
assertEq(Array.join(Array.slice(arrLike, 1, 2)), "7");

if (typeof reportCompare === "function")
  reportCompare(true, true);
