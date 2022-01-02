// RegExp.prototype.exec: Test lastIndex changes for ES2017.

// Test various combinations of:
// - Pattern matches or doesn't match
// - Global and/or sticky flag is set.
// - lastIndex exceeds the input string length
// - lastIndex is +-0
const testCases = [
    { regExp: /a/,  lastIndex: 0, input: "a", result: 0 },
    { regExp: /a/g, lastIndex: 0, input: "a", result: 1 },
    { regExp: /a/y, lastIndex: 0, input: "a", result: 1 },

    { regExp: /a/,  lastIndex: 0, input: "b", result: 0 },
    { regExp: /a/g, lastIndex: 0, input: "b", result: 0 },
    { regExp: /a/y, lastIndex: 0, input: "b", result: 0 },

    { regExp: /a/,  lastIndex: -0, input: "a", result: -0 },
    { regExp: /a/g, lastIndex: -0, input: "a", result: 1 },
    { regExp: /a/y, lastIndex: -0, input: "a", result: 1 },

    { regExp: /a/,  lastIndex: -0, input: "b", result: -0 },
    { regExp: /a/g, lastIndex: -0, input: "b", result: 0 },
    { regExp: /a/y, lastIndex: -0, input: "b", result: 0 },

    { regExp: /a/,  lastIndex: -1, input: "a", result: -1 },
    { regExp: /a/g, lastIndex: -1, input: "a", result: 1 },
    { regExp: /a/y, lastIndex: -1, input: "a", result: 1 },

    { regExp: /a/,  lastIndex: -1, input: "b", result: -1 },
    { regExp: /a/g, lastIndex: -1, input: "b", result: 0 },
    { regExp: /a/y, lastIndex: -1, input: "b", result: 0 },

    { regExp: /a/,  lastIndex: 100, input: "a", result: 100 },
    { regExp: /a/g, lastIndex: 100, input: "a", result: 0 },
    { regExp: /a/y, lastIndex: 100, input: "a", result: 0 },
];

// Basic test.
for (let {regExp, lastIndex, input, result} of testCases) {
    let re = new RegExp(regExp);
    re.lastIndex = lastIndex;
    re.exec(input);
    assertEq(re.lastIndex, result);
}

// Test when lastIndex is non-writable.
for (let {regExp, lastIndex, input} of testCases) {
    let re = new RegExp(regExp);
    Object.defineProperty(re, "lastIndex", { value: lastIndex, writable: false });
    if (re.global || re.sticky) {
        assertThrowsInstanceOf(() => re.exec(input), TypeError);
    } else {
        re.exec(input);
    }
    assertEq(re.lastIndex, lastIndex);
}

// Test when lastIndex is changed to non-writable as a side-effect.
for (let {regExp, lastIndex, input} of testCases) {
    let re = new RegExp(regExp);
    let called = false;
    re.lastIndex = {
        valueOf() {
            assertEq(called, false);
            called = true;
            Object.defineProperty(re, "lastIndex", { value: 9000, writable: false });
            return lastIndex;
        }
    };
    if (re.global || re.sticky) {
        assertThrowsInstanceOf(() => re.exec(input), TypeError);
    } else {
        re.exec(input);
    }
    assertEq(re.lastIndex, 9000);
    assertEq(called, true);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
