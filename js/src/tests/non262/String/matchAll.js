// Basic surface tests.

assertEq(typeof String.prototype.matchAll, "function");
assertEq(String.prototype.matchAll.name, "matchAll");
assertEq(String.prototype.matchAll.length, 1);

assertEq(typeof Symbol.matchAll, "symbol");

assertEq(typeof RegExp.prototype[Symbol.matchAll], "function");
assertEq(RegExp.prototype[Symbol.matchAll].name, "[Symbol.matchAll]");
assertEq(RegExp.prototype[Symbol.matchAll].length, 1);

const IteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));
const RegExpStringIteratorPrototype = Object.getPrototypeOf("".matchAll(""));

assertEq(Object.getPrototypeOf(RegExpStringIteratorPrototype), IteratorPrototype);

assertEq(typeof RegExpStringIteratorPrototype.next, "function");
assertEq(RegExpStringIteratorPrototype.next.name, "next");
assertEq(RegExpStringIteratorPrototype.next.length, 0);

assertEq(RegExpStringIteratorPrototype[Symbol.toStringTag], "RegExp String Iterator");


// Basic functional tests.

const RegExp_prototype_exec = RegExp.prototype.exec;
const RegExp_prototype_match = RegExp.prototype[Symbol.match];

function assertEqIterMatchResult(actual, expected) {
    assertEq(actual.done, expected.done);
    if (actual.value === undefined || expected.value === undefined) {
        assertEq(actual.value, expected.value);
    } else {
        assertEqArray(actual.value, expected.value);
        assertEq(actual.value.input, expected.value.input);
        assertEq(actual.value.index, expected.value.index);
    }
}

function assertEqMatchResults(actual, expected) {
    var actualIter = actual[Symbol.iterator]();
    var expectedIter = expected[Symbol.iterator]();
    while (true) {
        var actualResult = actualIter.next();
        var expectedResult = expectedIter.next();
        assertEqIterMatchResult(actualResult, expectedResult);
        if (actualResult.done && expectedResult.done)
            return;
    }
}

function* matchResults(string, regexp, lastIndex = 0) {
    regexp.lastIndex = lastIndex;
    while (true) {
        var match = Reflect.apply(RegExp_prototype_exec, regexp, [string]);
        if (match === null)
            return;
        yield match;
        if (!regexp.global)
            return;
    }
}

assertEqMatchResults(/a/[Symbol.matchAll]("ababcca"), matchResults("ababcca", /a/));
assertEqMatchResults("ababcca".matchAll(/a/g), matchResults("ababcca", /a/g));
assertEqMatchResults("ababcca".matchAll("a"), matchResults("ababcca", /a/g));


// Cross-compartment tests.

{
    let otherGlobal = newGlobal();

    let iterator = otherGlobal.eval(`"ababcca".matchAll(/a/g)`);
    let expected = matchResults("ababcca", /a/g);

    assertEqIterMatchResult(RegExpStringIteratorPrototype.next.call(iterator),
                            expected.next());
}


// Optimization tests.
//
// The optimized path for MatchAllIterator reuses the input RegExp to avoid
// extra RegExp allocations. To make this optimization undetectable from
// user code, we need to ensure that:
// 1. Modifications to the input RegExp through RegExp.prototype.compile are
//    detected and properly handled (= ignored).
// 2. The RegExpStringIterator doesn't modify the input RegExp, for example
//    by updating the lastIndex property.
// 3. Guards against modifications of built-in RegExp.prototype are installed.

// Recompile RegExp (source) before first match.
{
    let regexp = /a+/g;
    let iterator = "aabb".matchAll(regexp);

    regexp.compile("b+", "g");
    assertEqMatchResults(iterator, matchResults("aabb", /a+/g));
}

// Recompile RegExp (flags) before first match.
{
    let regexp = /a+/gi;
    let iterator = "aAbb".matchAll(regexp);

    regexp.compile("a+", "");
    assertEqMatchResults(iterator, matchResults("aAbb", /a+/gi));
}

// Recompile RegExp (source) after first match.
{
    let regexp = /a+/g;
    let iterator = "aabbaa".matchAll(regexp);
    let expected = matchResults("aabbaa", /a+/g);

    assertEqIterMatchResult(iterator.next(), expected.next());
    regexp.compile("b+", "g");
    assertEqIterMatchResult(iterator.next(), expected.next());
}

// Recompile RegExp (flags) after first match.
{
    let regexp = /a+/g;
    let iterator = "aabbAA".matchAll(regexp);
    let expected = matchResults("aabbAA", /a+/g);

    assertEqIterMatchResult(iterator.next(), expected.next());
    regexp.compile("a+", "i");
    assertEqIterMatchResult(iterator.next(), expected.next());
}

// lastIndex property of input RegExp not modified when optimized path used.
{
    let regexp = /a+/g;
    regexp.lastIndex = 1;
    let iterator = "aabbaa".matchAll(regexp);
    let expected = matchResults("aabbaa", /a+/g, 1);

    assertEq(regexp.lastIndex, 1);
    assertEqIterMatchResult(iterator.next(), expected.next());
    assertEq(regexp.lastIndex, 1);
    assertEqIterMatchResult(iterator.next(), expected.next());
    assertEq(regexp.lastIndex, 1);
}

// Modifications to lastIndex property of input RegExp ignored when optimized path used.
{
    let regexp = /a+/g;
    let iterator = "aabbaa".matchAll(regexp);
    regexp.lastIndex = 1;
    let expected = matchResults("aabbaa", /a+/g);

    assertEq(regexp.lastIndex, 1);
    assertEqIterMatchResult(iterator.next(), expected.next());
    assertEq(regexp.lastIndex, 1);
    assertEqIterMatchResult(iterator.next(), expected.next());
    assertEq(regexp.lastIndex, 1);
}

// RegExp.prototype[Symbol.match] is modified to a getter, ensure this getter
// is called exactly twice.
try {
    let regexp = /a+/g;

    let callCount = 0;
    Object.defineProperty(RegExp.prototype, Symbol.match, {
        get() {
            assertEq(this, regexp);
            callCount++;
            return RegExp_prototype_match;
        }
    });
    let iterator = "aabbaa".matchAll(regexp);
    assertEq(callCount, 2);
} finally {
    // Restore optimizable RegExp.prototype shape.
    Object.defineProperty(RegExp.prototype, Symbol.match, {
        value: RegExp_prototype_match,
        writable: true, enumerable: false, configurable: true
    });
}

// RegExp.prototype.exec is changed to a getter.
try {
    let regexp = /a+/g;
    let iterator = "aabbaa".matchAll(regexp);
    let lastIndices = [0, 2, 6][Symbol.iterator]();

    let iteratorRegExp = null;
    let callCount = 0;
    Object.defineProperty(RegExp.prototype, "exec", {
        get() {
            callCount++;

            if (iteratorRegExp === null)
                iteratorRegExp = this;

            assertEq(this === regexp, false);
            assertEq(this, iteratorRegExp);
            assertEq(this.source, regexp.source);
            assertEq(this.flags, regexp.flags);
            assertEq(this.lastIndex, lastIndices.next().value);
            return RegExp_prototype_exec;
        }
    });

    assertEqMatchResults(iterator, matchResults("aabbaa", /a+/g));
    assertEq(callCount, 3);
} finally {
    // Restore optimizable RegExp.prototype shape.
    Object.defineProperty(RegExp.prototype, "exec", {
        value: RegExp_prototype_exec,
        writable: true, enumerable: false, configurable: true
    });
}

// RegExp.prototype.exec is changed to a value property.
try {
    let regexp = /a+/g;
    let iterator = "aabbaa".matchAll(regexp);
    let lastIndices = [0, 2, 6][Symbol.iterator]();

    let iteratorRegExp = null;
    let callCount = 0;
    RegExp.prototype.exec = function(...args) {
        callCount++;

        if (iteratorRegExp === null)
            iteratorRegExp = this;

        assertEq(this === regexp, false);
        assertEq(this, iteratorRegExp);
        assertEq(this.source, regexp.source);
        assertEq(this.flags, regexp.flags);
        assertEq(this.lastIndex, lastIndices.next().value);
        return Reflect.apply(RegExp_prototype_exec, this, args);
    };

    assertEqMatchResults(iterator, matchResults("aabbaa", /a+/g));
    assertEq(callCount, 3);
} finally {
    // Restore optimizable RegExp.prototype shape.
    Object.defineProperty(RegExp.prototype, "exec", {
        value: RegExp_prototype_exec,
        writable: true, enumerable: false, configurable: true
    });
}

// Initial 'lastIndex' is zero if the RegExp is neither global nor sticky (1).
{
    let regexp = /a+/;
    regexp.lastIndex = 2;

    let iterator = regexp[Symbol.matchAll]("aaaaa");
    assertEqMatchResults(iterator, matchResults("aaaaa", /a+/g, 0));
}

// Initial 'lastIndex' is zero if the RegExp is neither global nor sticky (2).
{
    let regexp = /a+/g;
    regexp.lastIndex = 2;

    let iterator = regexp[Symbol.matchAll]("aaaaa");
    assertEqMatchResults(iterator, matchResults("aaaaa", /a+/g, 2));
}

// Initial 'lastIndex' is zero if the RegExp is neither global nor sticky (3).
{
    let regexp = /a+/y;
    regexp.lastIndex = 2;

    let iterator = regexp[Symbol.matchAll]("aaaaa");
    assertEqMatchResults(iterator, matchResults("aaaaa", /a+/g, 2));
}

//Initial 'lastIndex' is zero if the RegExp is neither global nor sticky (4).
{
    let regexp = /a+/gy;
    regexp.lastIndex = 2;

    let iterator = regexp[Symbol.matchAll]("aaaaa");
    assertEqMatchResults(iterator, matchResults("aaaaa", /a+/g, 2));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
