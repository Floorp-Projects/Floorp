// Tests for nsITextToSubURI.unEscapeNonAsciiURI
function run_test() {
  const textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"].getService(Components.interfaces.nsITextToSubURI);

  // Tests whether nsTextToSubURI does UTF-16 unescaping (it shouldn't)
  const testURI = "data:text/html,%FE%FF";
  Assert.equal(textToSubURI.unEscapeNonAsciiURI("UTF-16", testURI), testURI);

  // Tests whether incomplete multibyte sequences throw.
  const tests = [{
    input: "http://example.com/?p=%E9",
    throws: Components.results.NS_ERROR_ILLEGAL_INPUT,
  }, {
    input: "http://example.com/?p=%E9%80",
    throws: Components.results.NS_ERROR_ILLEGAL_INPUT,
  }, {
    input: "http://example.com/?p=%E9%80%80",
    expected: "http://example.com/?p=\u9000",
  }, {
    input: "http://example.com/?p=%E9e",
    throws: Components.results.NS_ERROR_ILLEGAL_INPUT,
  }, {
    input: "http://example.com/?p=%E9%E9",
    throws: Components.results.NS_ERROR_ILLEGAL_INPUT,
  }, {
    input: "http://example.com/?name=M%FCller/",
    throws: Components.results.NS_ERROR_ILLEGAL_INPUT,
  }, {
    input: "http://example.com/?name=M%C3%BCller/",
    expected: "http://example.com/?name=Müller/",
  }];

  for (const t of tests) {
    if (t.throws !== undefined) {
      let thrown = undefined;
      try {
        textToSubURI.unEscapeNonAsciiURI("UTF-8", t.input);
      } catch (e) {
        thrown = e.result;
      }
      Assert.equal(thrown, t.throws);
    } else {
      Assert.equal(textToSubURI.unEscapeNonAsciiURI("UTF-8", t.input), t.expected);
    }
  }
}
