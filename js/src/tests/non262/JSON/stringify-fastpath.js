// |reftest| skip-if(!xulRuntime.shell) -- not sure why, but invisible errors when running in browser.

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

if (typeof JSONStringify === "undefined") {
    var JSONStringify = SpecialPowers.Cu.getJSTestingFunctions().JSONStringify;
}

var report = this.console?.error || this.printErr;

report("BUGNUMBER: 1837410");

function compare(obj) {
    let slow;
    try { slow = JSONStringify(obj, "SlowOnly"); } catch (e) { slow = "" + e; }
    let maybeFast;
    try { maybeFast = JSONStringify(obj, "Normal"); } catch (e) { maybeFast = "" + e; }
    if (slow !== maybeFast) {
        report(`Slow:\n${slow}\nFast:\n${maybeFast}`);
        return 1;
    }
    return 0;
}

function newBigInt(val = 7n) {
    let result;
    function grabThis() { result = this; }
    grabThis.call(BigInt(val));
    return result;
}

function testBothPaths() {
    let failures = 0;
    failures += compare([, 3, undefined, ,]);
    failures += compare({ a: 1, b: undefined });
    failures += compare({ a: undefined, b: 1 });
    failures += compare({ a: 1, b: Symbol("fnord") });

    const obj = {};
    obj.first = true;
    obj[0] = 'a';
    obj[1] = undefined;
    obj[2] = 'b';
    obj.last = true;
    failures += compare(obj);

    const cyclic = {};
    cyclic.one = 1;
    cyclic.two = { me: cyclic };
    failures += compare(cyclic);

    const sparse = [10, 20, 30];
    sparse[1000] = 40;
    failures += compare(sparse);

    const arr = [10, 20, 30];
    arr.other = true;
    failures += compare(arr);

    const arr2 = new Array(5);
    arr2[1] = 'hello';
    arr2[3] = 'world';
    failures += compare(arr2);

    const big = { p1: 1, p2: 2, p3: 3, p4: 4, p5: 5, p6: 6, p7: 7, p8: 8, p9: 9 };
    failures += compare(big);

    failures += compare(new Number(3));
    failures += compare(new Boolean(true));
    failures += compare(Number.NaN);
    failures += compare(undefined);
    failures += compare({ x: () => 1 });

    failures += compare({ x: newBigInt() });

    const sparse2 = [];
    Object.defineProperty(sparse2, "0", { value: 7, enumerable: false });
    failures += compare(sparse2);

    return failures;
}

function checkFast(value, expectWhySlow) {
    let whySlow;
    let json;
    try {
        json = JSONStringify(value, "FastOnly");
    } catch (e) {
        const m = e.message.match(/failed mandatory fast path: (\S+)/);
        if (!m) {
            report("Expected fast path fail, got " + e);
            return 1;
        }
        whySlow = m[1];
    }

    if (expectWhySlow) {
        if (!whySlow) {
            report("Expected to bail out of fast path but unexpectedly succeeded");
            report((new Error).stack);
            report(json);
            return 1;
        } else if (whySlow != expectWhySlow) {
            report(`Expected to bail out of fast path because ${expectWhySlow} but bailed because ${whySlow}`);
            return 1;
        }
    } else {
        if (whySlow) {
            report("Expected fast path to succeed, bailed because: " + whySlow);
            return 1; // Fail
        }
    }

    return 0;
}

function testFastPath() {
    let failures = 0;
    failures += checkFast({});
    failures += checkFast([]);
    failures += checkFast({ x: true });
    failures += checkFast([, , 10, ,]);
    failures += checkFast({ x: undefined });
    failures += checkFast({ x: Symbol() });
    failures += checkFast({ x: new Set([10,20,30]) });

    failures += checkFast("primitive", "PRIMITIVE");
    failures += checkFast(true, "PRIMITIVE");
    failures += checkFast(7, "PRIMITIVE");

    failures += checkFast({ x: new Uint8Array(3) }, "INELIGIBLE_OBJECT");
    failures += checkFast({ x: new Number(3) }, "INELIGIBLE_OBJECT");
    failures += checkFast({ x: new Boolean(true) }, "INELIGIBLE_OBJECT");
    failures += checkFast({ x: newBigInt(3) }, "INELIGIBLE_OBJECT");
    failures += checkFast(Number.NaN, "PRIMITIVE");
    failures += checkFast(undefined, "PRIMITIVE");

    // Array has enumerated indexed + non-indexed slots.
    const nonElements = [];
    Object.defineProperty(nonElements, 0, { value: "hi", enumerated: true, configurable: true });
    nonElements.named = 7;
    failures += checkFast(nonElements, "INELIGIBLE_OBJECT");

    nonElements.splice(0);
    failures += checkFast(nonElements);

    // Array's prototype has indexed slot and/or inherited element.
    const proto = {};
    Object.defineProperty(proto, "0", { value: 1, enumerable: false });
    const holy = [, , 3];
    Object.setPrototypeOf(holy, proto);
    failures += checkFast(holy, "INELIGIBLE_OBJECT");
    Object.setPrototypeOf(holy, { 1: true });
    failures += checkFast(holy, "INELIGIBLE_OBJECT");

    // This is probably redundant with one of the above, but it was
    // found by a fuzzer at one point.
    const accessorProto = Object.create(Array.prototype);
    Object.defineProperty(accessorProto, "0", {
        get() { return 2; }, set() { }
    });
    const child = [];
    Object.setPrototypeOf(child, accessorProto);
    child.push(1);
    failures += checkFast(child, "INELIGIBLE_OBJECT");

    failures += checkFast({ get x() { return 1; } }, "NON_DATA_PROPERTY");

    const self = {};
    self.self = self;
    failures += checkFast(self, "DEEP_RECURSION");
    const ouroboros = ['head', 'middle', []];
    let p = ouroboros[2];
    let middle;
    for (let i = 0; i < 1000; i++) {
        p.push('middle', 'middle');
        p = p[2] = [];
        if (i == 10) {
            middle = p;
        }
    }
    failures += checkFast(ouroboros, "DEEP_RECURSION"); // Acyclic but deep
    p[2] = ouroboros;
    failures += checkFast(ouroboros, "DEEP_RECURSION"); // Cyclic and deep
    middle[2] = ouroboros;
    failures += checkFast(ouroboros, "DEEP_RECURSION"); // Cyclic after 10 recursions

    failures += checkFast({ 0: true, 1: true, 10000: true }, "INELIGIBLE_OBJECT");
    const arr = [1, 2, 3];
    arr[10000] = 4;
    failures += checkFast(arr, "INELIGIBLE_OBJECT");

    failures += checkFast({ x: 12n }, "BIGINT");

    failures += checkFast({ x: new Date() }, "HAVE_TOJSON");
    failures += checkFast({ toJSON() { return "json"; } }, "HAVE_TOJSON");
    const custom = { toJSON() { return "value"; } };
    failures += checkFast(Object.create(custom), "HAVE_TOJSON");

    return failures;
}

let failures = testBothPaths() + testFastPath();

if (typeof reportCompare === "function") {
    reportCompare(0, failures);
} else {
    assertEq(failures, 0);
}
