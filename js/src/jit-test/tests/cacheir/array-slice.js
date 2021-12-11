function packed() {
  var a = [0, 1, 2, 3];
  for (var i = 0; i <= 100; ++i) {
    var r = a.slice(0);
    assertEq(r.length, 4);
  }
}

for (var i = 0; i < 2; ++i) {
  packed();
}

function packedThenUnpacked() {
  var a = [0, 1, 2, 3];
  var q = 0;
  for (var i = 0; i <= 100; ++i) {
    if (i === 100) a[10] = 0;

    var r = a.slice(0);
    assertEq(r.length, i < 100 ? 4 : 11);
  }
}

for (var i = 0; i < 2; ++i) {
  packedThenUnpacked();
}

function unpacked() {
  var a = [0, 1, /* hole */ , 3];
  for (var i = 0; i <= 100; ++i) {
    var r = a.slice(0);
    assertEq(r.length, 4);
    assertEq(2 in r, false);
  }
}

for (var i = 0; i < 2; ++i) {
  unpacked();
}
