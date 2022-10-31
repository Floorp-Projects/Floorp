// |reftest| shell-option(--enable-shadow-realms) skip-if(!xulRuntime.shell)

var sr = new ShadowRealm();

var w = wrapWithProto(sr, null);

var r = ShadowRealm.prototype.evaluate.call(w, `"ok"`);

assertEq(r, "ok");

if (typeof reportCompare === 'function')
  reportCompare(true, true);
