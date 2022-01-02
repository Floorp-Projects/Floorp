function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1716290-3.sjs",
    afterOpen,
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.characterSet,
    "Shift_JIS",
    "Doc should report Shift_JIS initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.characterSet,
    "replacement",
    "Doc should report replacement subsequently (non-ASCII-compatible HTTP header should override detector)"
  );
}
