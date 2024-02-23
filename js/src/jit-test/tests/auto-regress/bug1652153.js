x = "x";

lFile(x);

function lFile() {
  return oomTest(function() {
    let error = new Error("foobar");
    let report = createErrorReport(error);
  });
}
