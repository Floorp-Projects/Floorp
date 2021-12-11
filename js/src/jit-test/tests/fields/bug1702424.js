load(libdir + "asserts.js");

// Ensure BytecodeGeneration respects stack depth assertions: Private Methods
class a {
    b
    // original test case below.
    #c() {
        d.#c ^= 'x'
    }

    // Operator list:
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Expressions_and_Operators
    //
    // Compound Assignments
    t() {
        d.#c = 'x';
        d.#c += 'x';
        d.#c -= 'x';
        d.#c *= 'x';
        d.#c /= 'x';
        d.#c %= 'x';
        d.#c **= 'x';
        d.#c <<= 'x';
        d.#c >>= 'x';
        d.#c >>>= 'x';

        d.#c &= 'x';
        d.#c ^= 'x';
        d.#c |= 'x';

    }

    // Compound Logical Assignments
    e() {
        d.#c &&= 'x';
        d.#c ??= 'x';
        d.#c ||= 'x';
    }

    // Destructruring assignment.
    ds() {
        [d.#c, b] = [1, 2];
    }

    // Increment/Decrement
    incDec() {
        d.#c++;
        d.#c--;
        ++d.#c;
        --d.#c;
    }
}

class b {
    b
    // original test case below.
    #c = 10;

    // Operator list:
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Expressions_and_Operators
    //
    // Compound Assignments
    t() {
        d.#c = 'x';
        d.#c += 'x';
        d.#c -= 'x';
        d.#c *= 'x';
        d.#c /= 'x';
        d.#c %= 'x';
        d.#c **= 'x';
        d.#c <<= 'x';
        d.#c >>= 'x';
        d.#c >>>= 'x';

        d.#c &= 'x';
        d.#c ^= 'x';
        d.#c |= 'x';

    }

    // Compound Logical Assignments
    e() {
        d.#c &&= 'x';
        d.#c ??= 'x';
        d.#c ||= 'x';
    }

    // Destructruring assignment.
    ds() {
        [d.#c, b] = [1, 2];
    }


    // Increment/Decrement
    incDec() {
        d.#c++;
        d.#c--;
        ++d.#c;
        --d.#c;
    }
}

// Ensure we correctly throw for all compound private method assignments
class LiveTest {
    #c() { }

    test(lambdas) {
        for (let func of lambdas) {
            assertThrowsInstanceOf(() => func(this), Error);
        }
    }

    // Operator list:
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Expressions_and_Operators
    //
    // Compound Assignments
    compound() {
        let tests = [
            (d) => d.#c = 'x',
            (d) => d.#c += 'x',
            (d) => d.#c -= 'x',
            (d) => d.#c *= 'x',
            (d) => d.#c /= 'x',
            (d) => d.#c %= 'x',
            (d) => d.#c **= 'x',
            (d) => d.#c <<= 'x',
            (d) => d.#c >>= 'x',
            (d) => d.#c >>>= 'x',
            (d) => d.#c &= 'x',
            (d) => d.#c ^= 'x',
            (d) => d.#c |= 'x',
        ];

        this.test(tests);
    }

    // Compound Logical Assignments
    compoundLogical() {
        let tests = [
            (d) => d.#c &&= 'x',
        ]
        this.test(tests);
        // These don't throw because they don't
        // process the assignment.
        this.#c ??= 'x';
        this.#c ||= 'x';
    }

    // Destructruring assignment.
    destructuring() {
        let tests = [
            (d) => [d.#c, b] = [1, 2],
        ]
        this.test(tests);
    }


    // Increment/Decrement
    incDec() {
        let tests = [
            (d) => d.#c++,
            (d) => d.#c--,
            (d) => ++d.#c,
            (d) => --d.#c,
        ];
        this.test(tests);
    }
}

var l = new LiveTest;
l.compound();
l.compoundLogical();
l.destructuring();
l.incDec();

// Ensure we correctly throw for all compound private method assignments
class LiveTestField {
    #c = 0;

    // Operator list:
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Expressions_and_Operators
    //
    // Compound Assignments
    compound() {
        assertEq(this.#c = 1, 1);
        assertEq(this.#c += 1, 2);
        assertEq(this.#c -= 1, 1);
        assertEq(this.#c *= 2, 2);
        assertEq(this.#c /= 2, 1);
        assertEq(this.#c %= 1, 0);
        this.#c = 1;
        assertEq(this.#c **= 1, 1);
        assertEq(this.#c <<= 1, 2);
        assertEq(this.#c >>= 1, 1);
        assertEq(this.#c >>>= 1, 0);
        assertEq(this.#c &= 2, 0);
        assertEq(this.#c ^= 2, 2);
        assertEq(this.#c |= 1, 3);
    }

    // Compound Logical Assignments
    compoundLogical() {
        this.#c = undefined;
        this.#c ||= 10;
        assertEq(this.#c, 10);
        this.#c ||= 15;
        assertEq(this.#c, 10);

        this.#c = false;
        this.#c &&= 10;
        assertEq(this.#c, false);
        this.#c = 10;
        this.#c &&= 15;
        assertEq(this.#c, 15);

        this.#c = null;
        this.#c ??= 10;
        assertEq(this.#c, 10);

        this.#c ??= 15
        assertEq(this.#c, 10);
    }

    // Destructruring assignment.
    destructuring() {
        [this.#c, b] = [1, 2];

        assertEq(this.#c, 1);
    }


    // Increment/Decrement
    incDec() {
        this.#c = 0;
        assertEq(++this.#c, 1);
        assertEq(--this.#c, 0);
        this.#c++;
        assertEq(this.#c, 1);
        this.#c--;
        assertEq(this.#c, 0);

    }
}

var k = new LiveTestField;
k.compound();
k.compoundLogical()
k.destructuring()
k.incDec();