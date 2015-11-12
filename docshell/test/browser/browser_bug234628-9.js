function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(rootDir + "file_bug234628-9.html", afterOpen);
}

function afterOpen() {
  is(content.document.documentElement.textContent.indexOf('\u20AC'), 145, "Parent doc should be UTF-16");

  is(content.frames[0].document.documentElement.textContent.indexOf('\u20AC'), 96, "Child doc should be windows-1252");
}

