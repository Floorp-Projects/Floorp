var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

var uris = [
  {
    uri: "blob:https://example.com/230d5d50-35f9-9745-a64a-15e47b731a81",
    local: true,
  },
  {
    uri: "rstp://1.2.3.4/some_path?param=a",
    local: false,
  },
];

function run_test() {
  for (let i = 0; i < uris.length; i++) {
    let uri = ios.newURI(uris[i].uri);
    let flags = ios.getDynamicProtocolFlags(uri);

    Assert.equal(
      Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE & flags,
      uris[i].local ? Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE : 0
    );
  }
}
