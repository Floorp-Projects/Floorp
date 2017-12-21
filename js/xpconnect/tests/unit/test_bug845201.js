function sbTest() {
  var threw = false;
  try {
    for (var x in Components) { }
    Assert.ok(false, "Shouldn't be able to enumerate Components");
  } catch(e) {
    Assert.ok(true, "Threw appropriately");
    threw = true;
  }
  Assert.ok(threw, "Shouldn't have thrown uncatchable exception");
}

function run_test() {
  var sb = Components.utils.Sandbox('http://www.example.com', { wantComponents: true });
  sb.do_check_true = do_check_true;
  Components.utils.evalInSandbox(sbTest.toSource(), sb);
  Components.utils.evalInSandbox('sbTest();', sb);
}
