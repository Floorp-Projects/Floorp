function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug673087-2.html",
    afterOpen,
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.documentElement.textContent.indexOf("\uFFFD"),
    0,
    "Doc should decode as replacement initially"
  );

  is(
    content.document.characterSet,
    "replacement",
    "Doc should report replacement initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.documentElement.textContent.indexOf("\uFFFD"),
    0,
    "Doc should decode as replacement subsequently"
  );

  is(
    content.document.characterSet,
    "replacement",
    "Doc should report replacement subsequently"
  );
}
