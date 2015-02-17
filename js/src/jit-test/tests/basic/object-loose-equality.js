var obj = {};
var sym = Symbol();

var notEqual = [
    [obj, sym],

    [0, {valueOf() { return null; }}],
    [0, {toString() { return null; }}],
    [null, {valueOf() { return null; }}],
    [null, {toString() { return null; }}],
    [undefined, {valueOf() { return null; }}],
    [undefined, {toString() { return null; }}],
    ["", {valueOf() { return null; }}],
    ["", {toString() { return null; }}],
    ["0", {valueOf() { return null; }}],
    ["0", {toString() { return null; }}],

    [0, {valueOf() { return undefined; }}],
    [0, {toString() { return undefined; }}],
    [null, {valueOf() { return undefined; }}],
    [null, {toString() { return undefined; }}],
    [undefined, {valueOf() { return undefined; }}],
    [undefined, {toString() { return undefined; }}],
    ["", {valueOf() { return undefined; }}],
    ["", {toString() { return undefined; }}],
    ["0", {valueOf() { return undefined; }}],
    ["0", {toString() { return undefined; }}],
]

var equal = [
    [sym, {valueOf() { return sym; }}],
    [sym, {toString() { return sym; }}],

    ["abc", {valueOf() { return "abc"; }}],
    ["abc", {toString() { return "abc"; }}],

    [1, {valueOf() { return 1; }}],
    [1, {toString() { return 1; }}],

    [1, {valueOf() { return true; }}],
    [1, {toString() { return true; }}],

    [true, {valueOf() { return true; }}],
    [true, {toString() { return true; }}],

    [true, {valueOf() { return 1; }}],
    [true, {toString() { return 1; }}],
]

for (var [lhs, rhs] of notEqual) {
    assertEq(lhs == rhs, false);
    assertEq(rhs == lhs, false);
}

for (var [lhs, rhs] of equal) {
    assertEq(lhs == rhs, true);
    assertEq(rhs == lhs, true);
}
