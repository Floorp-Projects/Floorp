const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var ssm = Services.scriptSecurityManager;

function makeURI(uri) {
  return Services.io.newURI(uri);
}

function createPrincipal(aURI) {
  try {
    var uri = makeURI(aURI);
    var principal = ssm.createContentPrincipal(uri, {});
    return principal;
  } catch (e) {
    return null;
  }
}

function run_test() {
  Assert.equal(createPrincipal("http://test^test/foo^bar#x^y"), null);

  Assert.equal(createPrincipal("http://test^test/foo\\bar"), null);

  Assert.equal(createPrincipal("http://test:2^3/foo\\bar"), null);

  Assert.equal(
    createPrincipal("http://test/foo^bar").exposableSpec,
    "http://test/foo%5Ebar"
  );
}
