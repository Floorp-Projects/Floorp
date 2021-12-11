enableLastWarning();
eval(`
  function blah() {
    return 0;
    if (true) {} /* no newline here */ 400n == "abc";
  }
`);
assertEq(getLastWarning().message, "unreachable code after return statement");
blah();
