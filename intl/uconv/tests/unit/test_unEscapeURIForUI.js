// Tests for nsITextToSubURI.unEscapeURIForUI
function run_test() {
  // Tests whether incomplete multibyte sequences throw.
  const tests = [
    {
      input: "http://example.com/?p=%E3%80%82",
      //TODO: should be the same as input, bug 1248812
      expected: "http://example.com/?p=%u3002",
    },
    {
      input: "http://example.com/?name=%E3%80%82",
      dontEscape: true,
      expected: "http://example.com/?name=\u3002",
    },
  ];

  for (const t of tests) {
    Assert.equal(
      Services.textToSubURI.unEscapeURIForUI(t.input, t.dontEscape),
      t.expected
    );
  }
}
