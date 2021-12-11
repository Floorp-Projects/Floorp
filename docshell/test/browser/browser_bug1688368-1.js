function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1688368-1.sjs",
    afterOpen,
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.body.textContent.indexOf("â"),
    0,
    "Doc should be windows-1252 initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.body.textContent.indexOf("☃"),
    0,
    "Doc should be UTF-8 subsequently"
  );
}
