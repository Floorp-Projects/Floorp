function run_test()
{
  try {
    Components.utils.import("resource://test/importer.jsm");
    do_check_true(false, "import should not succeed.");
  } catch (x) {
    do_check_neq(x.fileName.indexOf("syntax_error.jsm"), -1);
    do_check_eq(x.lineNumber, 1);
  }
}