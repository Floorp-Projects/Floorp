function compareToAtom(a) {
    return a == 'test';
}

function compareToAtomNe(a) {
    return a != 'test';
}

var st = 'st';

function compareToRope(a) {
    return a == ('te' + st);
}

function compareToRopeNe(a) {
    var st = 'st';
    return a != ('te' + st);
}

function main() {
    var test = 'test';
    var foobar = 'foobar';

    assertEq(compareToAtom(test), true);
    assertEq(compareToAtom(foobar), false);

    assertEq(compareToAtomNe(test), false);
    assertEq(compareToAtomNe(foobar), true);


    assertEq(compareToRope(test), true);
    assertEq(compareToRope(foobar), false);

    assertEq(compareToRopeNe(test), false);
    assertEq(compareToRopeNe(foobar), true);
}

for (var i = 0; i < 100000; i++) {
    main();
}
