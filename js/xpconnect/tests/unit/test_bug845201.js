function sbTest() {
  var threw = false;
  try {
    for (var x in Components) { }
    do_check_true(false, "Shouldn't be able to enumerate Components");
  } catch(e) {
    do_check_true(true, "Threw appropriately");
    threw = true;
  }
  do_check_true(threw, "Shouldn't have thrown uncatchable exception");
}

function run_test() {
  var sb = Components.utils.Sandbox('http://www.example.com', { wantComponents: true });
  sb.do_check_true = do_check_true;
  Components.utils.evalInSandbox(sbTest.toSource(), sb);
  Components.utils.evalInSandbox('sbTest();', sb);
}
