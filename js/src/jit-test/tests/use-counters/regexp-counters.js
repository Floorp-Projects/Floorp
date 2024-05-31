function test_function_for_use_counter_integration(fn, counter, expected_growth = true) {
    let before = getUseCounterResults();
    assertEq(counter in before, true);

    fn();

    let after = getUseCounterResults();
    if (expected_growth) {
        console.log("Yes Increase: Before ", before[counter], " After", after[counter]);
        assertEq(after[counter] > before[counter], true);
    } else {
        console.log("No Increase: Before ", before[counter], " After", after[counter]);
        assertEq(after[counter] == before[counter], true);
    }
}

class Sentinel extends RegExp { }

function R() { }
Object.setPrototypeOf(R, RegExp);
Object.setPrototypeOf(R.prototype, RegExp.prototype);

// Define a bunch of new getters since the builtin getters throw on
// non-RegExp-branded `this`
Object.defineProperty(R.prototype, "hasIndices", { value: false });
Object.defineProperty(R.prototype, "global", { value: true });
Object.defineProperty(R.prototype, "ignoreCase", { value: false });
Object.defineProperty(R.prototype, "multiline", { value: false });
Object.defineProperty(R.prototype, "dotAll", { value: false });
Object.defineProperty(R.prototype, "unicode", { value: false });
Object.defineProperty(R.prototype, "unicodeSets", { value: false });
Object.defineProperty(R.prototype, "sticky", { value: false });
Object.defineProperty(R.prototype, "source", { value: false });

// Need a Symbol.species getter for Type III Subclassing.
Object.defineProperty(R, Symbol.species, {
    get() {
        return Sentinel;
    },
    configurable: true,
    enumerable: true
});

assertEq(R[Symbol.species], Sentinel);

function test_regexp_matchall() {
    "some".matchAll(/foo/g);
}

function test_regexp_matchall_type_iii() {
    "some string".matchAll(new R("foo"))
}

test_function_for_use_counter_integration(test_regexp_matchall, "SubclassingRegExpTypeIII", false)
test_function_for_use_counter_integration(test_regexp_matchall_type_iii, "SubclassingRegExpTypeIII", true)

function test_regexp_split() {
    let r = /p/;
    let split = r[Symbol.split];
    split.call(r, "split");
}

function test_regexp_split_type_iii() {
    let r = /r/;

    class B extends RegExp {
        static get [Symbol.species]() {
            return Sentinel;
        }
    }

    let b = new B("R");

    let split = r[Symbol.split];

    // This reciever needs to be a class-subclass RegExp,
    // as RegExp[@@split] will call RegExp.exec, which
    // typechecks this.
    split.call(b, "split");
}



test_function_for_use_counter_integration(test_regexp_split, "SubclassingRegExpTypeIII", false)
test_function_for_use_counter_integration(test_regexp_split_type_iii, "SubclassingRegExpTypeIII", true)

function test_regexp_split_type_iii_default_exception() {
    let r = /r/;

    class B extends RegExp {
        static get [Symbol.species]() {
            return this;
        }
    }

    let b = new B("R");

    let split = r[Symbol.split];
    split.call(b, "split");
}

test_function_for_use_counter_integration(test_regexp_split_type_iii_default_exception, "SubclassingRegExpTypeIII", false)

function test_regexp_exec() {
    let r = /r/;
    "s".match(r);
}

function test_regexp_exec_type_iv() {
    class R extends RegExp {
        exec() {
            return {};
        }
    }
    "s".match(new R("s"));
}

test_function_for_use_counter_integration(test_regexp_exec, "SubclassingRegExpTypeIV", false)
test_function_for_use_counter_integration(test_regexp_exec_type_iv, "SubclassingRegExpTypeIV", true)

