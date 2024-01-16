function test(flags) {
  // RegExp with a simple atom matcher.
  // - Global flag to enable setting 'lastIndex'.
  let s = "\u{10000}";
  let re = RegExp(s, flags + "g");

  for (let i = 0; i < 200; ++i) {
    // Set lastIndex in the middle of the surrogate pair.
    re.lastIndex = 1;

    // |exec| will reset lastIndex to the start of the surrogate pair.
    let r = re.exec(s);

    // Atom match should succeed.
    assertEq(r[0], s);
    assertEq(r.index, 0);
    assertEq(re.lastIndex, 2);
  }
}

// Unicode flag to enable surrogate pairs support.
test("u");

// Unicode-Sets flag to enable surrogate pairs support.
test("v");
