var ssm = Services.scriptSecurityManager;
function makeURI(uri) {
  return Services.io.newURI(uri);
}

function testIPV6Host(aHost, aExpected) {
  var ipv6Host = ssm.createContentPrincipal(makeURI(aHost), {});
  Assert.equal(ipv6Host.origin, aExpected);
}

function run_test() {
  testIPV6Host("http://[::1]/", "http://[::1]");

  testIPV6Host(
    "http://[2001:db8:85a3:8d3:1319:8a2e:370:7348]/",
    "http://[2001:db8:85a3:8d3:1319:8a2e:370:7348]"
  );

  testIPV6Host(
    "http://[2001:db8:85a3:8d3:1319:8a2e:370:7348]:443/",
    "http://[2001:db8:85a3:8d3:1319:8a2e:370:7348]:443"
  );

  testIPV6Host(
    "http://[2001:db8:85a3::1319:8a2e:370:7348]/",
    "http://[2001:db8:85a3:0:1319:8a2e:370:7348]"
  );

  testIPV6Host(
    "http://[20D1:0000:3238:DFE1:63:0000:0000:FEFB]/",
    "http://[20d1:0:3238:dfe1:63::fefb]"
  );

  testIPV6Host(
    "http://[20D1:0:3238:DFE1:63::FEFB]/",
    "http://[20d1:0:3238:dfe1:63::fefb]"
  );
}
