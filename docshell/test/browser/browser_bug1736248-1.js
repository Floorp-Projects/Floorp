function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1736248-1.html",
    afterOpen,
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.documentElement.textContent.indexOf("\u00C3"),
    1064,
    "Doc should be windows-1252 initially"
  );
  is(
    content.document.characterSet,
    "windows-1252",
    "Doc should report windows-1252 initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.documentElement.textContent.indexOf("\u00E4"),
    1064,
    "Doc should be UTF-8 subsequently"
  );
  is(
    content.document.characterSet,
    "UTF-8",
    "Doc should report UTF-8 subsequently"
  );
}
