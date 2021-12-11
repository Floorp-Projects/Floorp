function exists() {
  var a = {'null': 0, 'undefined': 0};
  for (var i = 0; i < 100; i++) {
    a[null] = i;
    a[undefined] = i * 2;
    assertEq(a['null'], i);
    assertEq(a['undefined'], i * 2);
  }
}

function adding() {
  for (var i = 0; i < 100; i++) {
    var a = {};
    a[null] = i;
    a[undefined] = i * 2;
    assertEq(a['null'], i);
    assertEq(a['undefined'], i * 2);
  }
}

function setter() {
  var test = 0;
  var a = {
    set null(v) {
      test = v;
    },
    set undefined(v) {
      test = v * 2;
    }
  }
  for (var i = 0; i < 100; i++) {
    a[null] = i;
    assertEq(test, i);
    a[undefined] = i;
    assertEq(test, i * 2);
  }
}

function mixed() {
  var a = {'null': void 0, 'undefined': void 0};
  for (var i = 0; i < 100; i++) {
    a[i % 2 ? null : undefined] = i;
    assertEq(a[i % 2 ? 'null' : 'undefined'], i)
  }
}

exists();
adding()
setter();
mixed();
