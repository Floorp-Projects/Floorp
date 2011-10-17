// visibility of updates to RegExp.multiline

function foo(value) {
  for (var i = 0; i < 50; i++) {
    var re = /erwe/;
    assertEq(re.multiline, value);
  }
}

foo(false);
RegExp.multiline = true;
foo(true);
