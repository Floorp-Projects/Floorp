function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(rootDir + "file_bug234628-8.html", afterOpen);
}

function afterOpen() {
  is(content.document.documentElement.textContent.indexOf("\u0402"), 156, "Parent doc should be windows-1251");

  is(content.frames[0].document.documentElement.textContent.indexOf("\u0402"), 99, "Child doc should be windows-1251");
}

