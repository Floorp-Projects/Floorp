function sbTest() {
  var threw = false;
  try {
    for (var x in Components) { }
    ok(false, "Shouldn't be able to enumerate Components");
  } catch(e) {
    ok(true, "Threw appropriately");
    threw = true;
  }
  ok(threw, "Shouldn't have thrown uncatchable exception");
}

function run_test() {
  var sb = Components.utils.Sandbox('http://www.example.com', { wantComponents: true });
  sb.ok = ok;
  Components.utils.evalInSandbox(sbTest.toSource(), sb);
  Components.utils.evalInSandbox('sbTest();', sb);
}
