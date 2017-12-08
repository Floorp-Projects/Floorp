for (let method of [undefined, null]) {
    let obj = {
        [Symbol.toPrimitive]: method,
        toString: () => "pass",
    };
    assertEq("" + obj, "pass");
}

for (let method of [true, false, 0, 123, "", "abc", Symbol(), {}]) {
    let obj = {
        [Symbol.toPrimitive]: method,
        toString: () => "pass",
    };
    assertThrowsInstanceOf(() => "" + obj, TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
