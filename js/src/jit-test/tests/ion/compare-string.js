function compareToAtom(a) {
    return a == 'test-test-test-test-test-test-test-test';
}

function compareToAtomStrict(a) {
    return a === 'test-test-test-test-test-test-test-test';
}

function compareToAtomNe(a) {
    return a != 'test-test-test-test-test-test-test-test';
}

function compareToAtomNeStrict(a) {
    return a !== 'test-test-test-test-test-test-test-test';
}

function compareToAtomLessThan(a) {
    return a < 'test-test-test-test-test-test-test-test';
}

function compareToAtomLessThanOrEquals(a) {
    return a <= 'test-test-test-test-test-test-test-test';
}

function compareToAtomGreaterThan(a) {
    return a > 'test-test-test-test-test-test-test-test';
}

function compareToAtomGreaterThanOrEquals(a) {
    return a >= 'test-test-test-test-test-test-test-test';
}

var st = 'st-test-test-test-test-test-test-test';

function compareToRope(a) {
    return a == ('te' + st);
}

function compareToRopeStrict(a) {
    return a === ('te' + st);
}

function compareToRopeNe(a) {
    var st = 'st-test-test-test-test-test-test-test';
    return a != ('te' + st);
}

function compareToRopeNeStrict(a) {
    var st = 'st-test-test-test-test-test-test-test';
    return a !== ('te' + st);
}

function compareToRopeLessThan(a) {
    var st = 'st-test-test-test-test-test-test-test';
    return a < ('te' + st);
}

function compareToRopeLessThanOrEquals(a) {
    var st = 'st-test-test-test-test-test-test-test';
    return a <= ('te' + st);
}

function compareToRopeGreaterThan(a) {
    var st = 'st-test-test-test-test-test-test-test';
    return a > ('te' + st);
}

function compareToRopeGreaterThanOrEquals(a) {
    var st = 'st-test-test-test-test-test-test-test';
    return a >= ('te' + st);
}

function main() {
    // |test| must be longer than |JSFatInlineString::MAX_LENGTH_LATIN1| to
    // ensure the above functions create ropes when concatenating strings.
    var test = 'test-test-test-test-test-test-test-test';
    var foobar = 'foobar';

    assertEq(compareToAtom(test), true);
    assertEq(compareToAtom(foobar), false);

    assertEq(compareToAtomStrict(test), true);
    assertEq(compareToAtomStrict(foobar), false);

    assertEq(compareToAtomNe(test), false);
    assertEq(compareToAtomNe(foobar), true);

    assertEq(compareToAtomNeStrict(test), false);
    assertEq(compareToAtomNeStrict(foobar), true);

    assertEq(compareToAtomLessThan(test), false);
    assertEq(compareToAtomLessThan(foobar), true);

    assertEq(compareToAtomLessThanOrEquals(test), true);
    assertEq(compareToAtomLessThanOrEquals(foobar), true);

    assertEq(compareToAtomGreaterThan(test), false);
    assertEq(compareToAtomGreaterThan(foobar), false);

    assertEq(compareToAtomGreaterThanOrEquals(test), true);
    assertEq(compareToAtomGreaterThanOrEquals(foobar), false);


    assertEq(compareToRope(test), true);
    assertEq(compareToRope(foobar), false);

    assertEq(compareToRopeStrict(test), true);
    assertEq(compareToRopeStrict(foobar), false);

    assertEq(compareToRopeNe(test), false);
    assertEq(compareToRopeNe(foobar), true);

    assertEq(compareToRopeNeStrict(test), false);
    assertEq(compareToRopeNeStrict(foobar), true);

    assertEq(compareToRopeLessThan(test), false);
    assertEq(compareToRopeLessThan(foobar), true);

    assertEq(compareToRopeLessThanOrEquals(test), true);
    assertEq(compareToRopeLessThanOrEquals(foobar), true);

    assertEq(compareToRopeGreaterThan(test), false);
    assertEq(compareToRopeGreaterThan(foobar), false);

    assertEq(compareToRopeGreaterThanOrEquals(test), true);
    assertEq(compareToRopeGreaterThanOrEquals(foobar), false);
}

for (var i = 0; i < 10000; i++) {
    main();
}
