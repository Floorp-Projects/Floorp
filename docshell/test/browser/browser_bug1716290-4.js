function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1716290-4.sjs",
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
    "UTF-16BE",
    "Doc should report UTF-16BE subsequently (BOM should override detector)"
  );
}
