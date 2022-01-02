function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1716290-1.sjs",
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
    "windows-1252",
    "Doc should report windows-1252 subsequently (detector should override header)"
  );
}
