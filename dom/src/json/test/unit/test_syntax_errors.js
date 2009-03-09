function checkForSyntaxErrors(s) {
  var crockfordErrored = false;
  try {
    crockfordJSON.parse(s)
  } catch (e) {
    var crockfordErrored = true;
    do_check_true(e instanceof crockfordJSON.window.SyntaxError);
  }
  do_check_true(crockfordErrored);
  
  var JSONErrored = false;
  try {
    JSON.parse(s)
  } catch (e) {
    var JSONErrored = true;
    do_check_true(e instanceof SyntaxError);
  }
  do_check_true(JSONErrored);
}

function run_test() {
  checkForSyntaxErrors("{}...");
  checkForSyntaxErrors('{"foo": truBBBB}');
  checkForSyntaxErrors('{foo: truBBBB}');
  checkForSyntaxErrors('{"foo": undefined}');
  checkForSyntaxErrors('{"foo": ]');
  checkForSyntaxErrors('{"foo');
}