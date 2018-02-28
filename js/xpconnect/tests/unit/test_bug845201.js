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
  var sb = Cu.Sandbox('http://www.example.com', { wantComponents: true });
  sb.ok = ok;
  Cu.evalInSandbox(sbTest.toSource(), sb);
  Cu.evalInSandbox('sbTest();', sb);
}
