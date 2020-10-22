function stringToNumber() {
  function f(s) {
    return ~~s;
  }

  var q = 0;
  for (var i = 0; i < 200; ++i) {
    q += f("1");
    q += f("0x2");
    q += f("0b11");
    q += f("0o4");

    // Invalid inputs: ~~Nan == 0
    q += f("z");
    q += f("0x2.3");
    q += f("0x1.fp4");
  }
  assertEq(q, (1 + 2 + 3 + 4) * 200);
}
stringToNumber();

function stringToInt32() {
  function f(s) {
    return s - 0;
  }

  var q = 0;
  for (var i = 0; i < 200; ++i) {
    q += f("1");
    q += f("0x2");
    q += f("0b11");
    q += f("0o4");
  }
  assertEq(q, (1 + 2 + 3 + 4) * 200);
}
stringToInt32();
