load(libdir + 'array-compare.js');

function compareIAndSet(v) {
    var res = 0;
    var c;
    var i = 0;
    c = (v >  1);
    res |= (c << i);
    i++;
    c = (v >= 2);
    res |= (c << i);
    i++;
    c = (v <  3);
    res |= (c << i);
    i++;
    c = (v <= 4);
    res |= (c << i);
    i++;
    c = (v == 5);
    res |= (c << i);
    i++;
    c = (v != 6);
    res |= (c << i);
    i++;
    c = (v === 7);
    res |= (c << i);
    i++;
    c = (v !== 8);
    res |= (c << i);
    i++;
    return res;
}

function compareIAndBranch(v) {
    var res = 0;
    var c = 1;
    var i = 0;
    if (v >  1)
        res |= (c << i);
    i++;
    if (v >= 2)
        res |= (c << i);
    i++;
    if (v <  3)
        res |= (c << i);
    i++;
    if (v <= 4)
        res |= (c << i);
    i++;
    if (v == 5)
        res |= (c << i);
    i++;
    if (v != 6)
        res |= (c << i);
    i++;
    if (v === 7)
        res |= (c << i);
    i++;
    if (v !== 8)
        res |= (c << i);
    i++;
    if (v)
        res |= (c << i);
    i++;
    return res;
}

function compareDAndSet(v) {
    var res = 0;
    var c;
    var i = 0;
    c = (v >  1.5);
    res |= (c << i);
    i++;
    c = (v >= 2.5);
    res |= (c << i);
    i++;
    c = (v <  3.5);
    res |= (c << i);
    i++;
    c = (v <= 4.5);
    res |= (c << i);
    i++;
    c = (v == 5.5);
    res |= (c << i);
    i++;
    c = (v != 6.5);
    res |= (c << i);
    i++;
    c = (v === 7.5);
    res |= (c << i);
    i++;
    c = (v !== 8.5);
    res |= (c << i);
    i++;
    c = (v !== 0.0);
    res |= (c << i);
    i++;
    return res;
}

function compareDAndBranch(v) {
    var res = 0;
    var c = 1;
    var i = 0;
    if (v >  1.5)
        res |= (c << i);
    i++;
    if (v >= 2.5)
        res |= (c << i);
    i++;
    if (v <  3.5)
        res |= (c << i);
    i++;
    if (v <= 4.5)
        res |= (c << i);
    i++;
    if (v == 5.5)
        res |= (c << i);
    i++;
    if (v != 6.5)
        res |= (c << i);
    i++;
    if (v === 7.5)
        res |= (c << i);
    i++;
    if (v !== 8.5)
        res |= (c << i);
    i++;
    if (v)
        res |= (c << i);
    i++;
    return res;
}

function compareSAndSet(v) {
    var res = 0;
    var c;
    var i = 0;
    c = (v >  "a");
    res |= (c << i);
    i++;
    c = (v >= "b");
    res |= (c << i);
    i++;
    c = (v <  "c");
    res |= (c << i);
    i++;
    c = (v <= "d");
    res |= (c << i);
    i++;
    c = (v == "e");
    res |= (c << i);
    i++;
    c = (v != "f");
    res |= (c << i);
    i++;
    c = (v === "g");
    res |= (c << i);
    i++;
    c = (v !== "h");
    res |= (c << i);
    i++;
    return res;
}

function compareSAndBranch(v) {
    var res = 0;
    var c = 1;
    var i = 0;
    if (v >  "a")
        res |= (c << i);
    i++;
    if (v >= "b")
        res |= (c << i);
    i++;
    if (v <  "c")
        res |= (c << i);
    i++;
    if (v <= "d")
        res |= (c << i);
    i++;
    if (v == "e")
        res |= (c << i);
    i++;
    if (v != "f")
        res |= (c << i);
    i++;
    if (v === "g")
        res |= (c << i);
    i++;
    if (v !== "h")
        res |= (c << i);
    i++;
    if (v)
        res |= (c << i);
    i++;
    return res;
}

var expected = [
  // compareIAndSet
  172, 175, 171, 171, 179, 131, 227, 35,
  // compareIAndBranch
  428, 431, 427, 427, 435, 387, 483, 291,
  // compareDAndSet
  428, 428, 431, 427, 427, 435, 387, 483, 291, 416,
  // compareDAndBranch
  428, 428, 431, 427, 427, 435, 387, 483, 291, 160, 172,
  // compareSAndSet
  172, 175, 171, 171, 179, 131, 227, 35, 172,
  // compareSAndBranch
  428, 431, 427, 427, 435, 387, 483, 291, 172
];

var result = [
  compareIAndSet(1),
  compareIAndSet(2),
  compareIAndSet(3),
  compareIAndSet(4),
  compareIAndSet(5),
  compareIAndSet(6),
  compareIAndSet(7),
  compareIAndSet(8),

  compareIAndBranch(1),
  compareIAndBranch(2),
  compareIAndBranch(3),
  compareIAndBranch(4),
  compareIAndBranch(5),
  compareIAndBranch(6),
  compareIAndBranch(7),
  compareIAndBranch(8),

  compareDAndSet(0.5),
  compareDAndSet(1.5),
  compareDAndSet(2.5),
  compareDAndSet(3.5),
  compareDAndSet(4.5),
  compareDAndSet(5.5),
  compareDAndSet(6.5),
  compareDAndSet(7.5),
  compareDAndSet(8.5),
  compareDAndSet(NaN),

  compareDAndBranch(0.5),
  compareDAndBranch(1.5),
  compareDAndBranch(2.5),
  compareDAndBranch(3.5),
  compareDAndBranch(4.5),
  compareDAndBranch(5.5),
  compareDAndBranch(6.5),
  compareDAndBranch(7.5),
  compareDAndBranch(8.5),
  compareDAndBranch(NaN),
  compareDAndBranch(0.0),

  compareSAndSet("a"),
  compareSAndSet("b"),
  compareSAndSet("c"),
  compareSAndSet("d"),
  compareSAndSet("e"),
  compareSAndSet("f"),
  compareSAndSet("g"),
  compareSAndSet("h"),
  compareSAndSet(""),

  compareSAndBranch("a"),
  compareSAndBranch("b"),
  compareSAndBranch("c"),
  compareSAndBranch("d"),
  compareSAndBranch("e"),
  compareSAndBranch("f"),
  compareSAndBranch("g"),
  compareSAndBranch("h"),
  compareSAndBranch("")
];

assertEq(arraysEqual(result, expected), true);
