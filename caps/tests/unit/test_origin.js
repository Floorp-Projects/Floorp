var Cu = Components.utils;
function run_test() {
  do_check_eq(Cu.getObjectPrincipal({}).origin, '[System Principal]');
  var exampleOrg = Cu.getObjectPrincipal(new Cu.Sandbox('http://example.org'));
  do_check_eq(exampleOrg.origin, 'http://example.org');
  var exampleCom = Cu.getObjectPrincipal(new Cu.Sandbox('https://www.example.com:123'));
  do_check_eq(exampleCom.origin, 'https://www.example.com:123');
  var nullPrin = Cu.getObjectPrincipal(new Cu.Sandbox(null));
  do_check_true(/^moz-nullprincipal:\{([0-9]|[a-z]|\-){36}\}$/.test(nullPrin.origin));
  var ep = Cu.getObjectPrincipal(new Cu.Sandbox([exampleCom, nullPrin, exampleOrg]));

  // Origins should be in lexical order.
  do_check_eq(ep.origin, `[Expanded Principal [${exampleOrg.origin}, ${exampleCom.origin}, ${nullPrin.origin}]]`);
}
