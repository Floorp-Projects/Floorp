function run_test()
{
  try {
    Components.utils.import("resource://test/importer.jsm");
    Assert.ok(false, "import should not succeed.");
  } catch (x) {
    Assert.notEqual(x.fileName.indexOf("syntax_error.jsm"), -1);
    Assert.equal(x.lineNumber, 1);
  }
}