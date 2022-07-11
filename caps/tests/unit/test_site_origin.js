const scriptSecMan = Services.scriptSecurityManager;

// SystemPrincipal checks
let systemPrincipal = scriptSecMan.getSystemPrincipal();
Assert.ok(systemPrincipal.isSystemPrincipal);
Assert.equal(systemPrincipal.origin, "[System Principal]");
Assert.equal(systemPrincipal.originNoSuffix, "[System Principal]");
Assert.equal(systemPrincipal.siteOrigin, "[System Principal]");
Assert.equal(systemPrincipal.siteOriginNoSuffix, "[System Principal]");

// ContentPrincipal checks
let uri1 = Services.io.newURI("http://example.com");
let prinicpal1 = scriptSecMan.createContentPrincipal(uri1, {
  userContextId: 11,
});
Assert.ok(prinicpal1.isContentPrincipal);
Assert.equal(prinicpal1.origin, "http://example.com^userContextId=11");
Assert.equal(prinicpal1.originNoSuffix, "http://example.com");
Assert.equal(prinicpal1.siteOrigin, "http://example.com^userContextId=11");
Assert.equal(prinicpal1.siteOriginNoSuffix, "http://example.com");

let uri2 = Services.io.newURI("http://test1.example.com");
let prinicpal2 = scriptSecMan.createContentPrincipal(uri2, {
  userContextId: 22,
});
Assert.ok(prinicpal2.isContentPrincipal);
Assert.equal(prinicpal2.origin, "http://test1.example.com^userContextId=22");
Assert.equal(prinicpal2.originNoSuffix, "http://test1.example.com");
Assert.equal(prinicpal2.siteOrigin, "http://example.com^userContextId=22");
Assert.equal(prinicpal2.siteOriginNoSuffix, "http://example.com");

let uri3 = Services.io.newURI("https://test1.test2.example.com:5555");
let prinicpal3 = scriptSecMan.createContentPrincipal(uri3, {
  userContextId: 5555,
});
Assert.ok(prinicpal3.isContentPrincipal);
Assert.equal(
  prinicpal3.origin,
  "https://test1.test2.example.com:5555^userContextId=5555"
);
Assert.equal(prinicpal3.originNoSuffix, "https://test1.test2.example.com:5555");
Assert.equal(prinicpal3.siteOrigin, "https://example.com^userContextId=5555");
Assert.equal(prinicpal3.siteOriginNoSuffix, "https://example.com");

let uri4 = Services.io.newURI("https://.example.com:6666");
let prinicpal4 = scriptSecMan.createContentPrincipal(uri4, {
  userContextId: 6666,
});
Assert.ok(prinicpal4.isContentPrincipal);
Assert.equal(prinicpal4.origin, "https://.example.com:6666^userContextId=6666");
Assert.equal(prinicpal4.originNoSuffix, "https://.example.com:6666");
Assert.equal(prinicpal4.siteOrigin, "https://.example.com^userContextId=6666");
Assert.equal(prinicpal4.siteOriginNoSuffix, "https://.example.com");

let aboutURI = Services.io.newURI("about:preferences");
let aboutPrincipal = scriptSecMan.createContentPrincipal(aboutURI, {
  userContextId: 66,
});
Assert.ok(aboutPrincipal.isContentPrincipal);
Assert.equal(aboutPrincipal.origin, "about:preferences^userContextId=66");
Assert.equal(aboutPrincipal.originNoSuffix, "about:preferences");
Assert.equal(aboutPrincipal.siteOrigin, "about:preferences^userContextId=66");
Assert.equal(aboutPrincipal.siteOriginNoSuffix, "about:preferences");

let viewSourceURI = Services.io.newURI(
  "view-source:https://test1.test2.example.com"
);
let viewSourcePrincipal = scriptSecMan.createContentPrincipal(viewSourceURI, {
  userContextId: 101,
});
Assert.ok(viewSourcePrincipal.isContentPrincipal);
Assert.ok(viewSourcePrincipal.schemeIs("view-source"));
Assert.equal(
  viewSourcePrincipal.origin,
  "https://test1.test2.example.com^userContextId=101"
);
Assert.equal(
  viewSourcePrincipal.originNoSuffix,
  "https://test1.test2.example.com"
);
Assert.equal(
  viewSourcePrincipal.siteOrigin,
  "https://example.com^userContextId=101"
);
Assert.equal(viewSourcePrincipal.siteOriginNoSuffix, "https://example.com");

let mozExtensionURI = Services.io.newURI(
  "moz-extension://924f966d-c93b-4fbf-968a-16608461663c/meh.txt"
);
let mozExtensionPrincipal = scriptSecMan.createContentPrincipal(
  mozExtensionURI,
  {
    userContextId: 102,
  }
);
Assert.ok(mozExtensionPrincipal.isContentPrincipal);
Assert.ok(mozExtensionPrincipal.schemeIs("moz-extension"));
Assert.equal(
  mozExtensionPrincipal.origin,
  "moz-extension://924f966d-c93b-4fbf-968a-16608461663c^userContextId=102"
);
Assert.equal(
  mozExtensionPrincipal.originNoSuffix,
  "moz-extension://924f966d-c93b-4fbf-968a-16608461663c"
);
Assert.equal(
  mozExtensionPrincipal.siteOrigin,
  "moz-extension://924f966d-c93b-4fbf-968a-16608461663c^userContextId=102"
);
Assert.equal(
  mozExtensionPrincipal.siteOriginNoSuffix,
  "moz-extension://924f966d-c93b-4fbf-968a-16608461663c"
);

let localhostURI = Services.io.newURI(" http://localhost:44203");
let localhostPrincipal = scriptSecMan.createContentPrincipal(localhostURI, {
  userContextId: 144,
});
Assert.ok(localhostPrincipal.isContentPrincipal);
Assert.ok(localhostPrincipal.schemeIs("http"));
Assert.equal(
  localhostPrincipal.origin,
  "http://localhost:44203^userContextId=144"
);
Assert.equal(localhostPrincipal.originNoSuffix, "http://localhost:44203");
Assert.equal(
  localhostPrincipal.siteOrigin,
  "http://localhost^userContextId=144"
);
Assert.equal(localhostPrincipal.siteOriginNoSuffix, "http://localhost");

// NullPrincipal checks
let nullPrincipal = scriptSecMan.createNullPrincipal({ userContextId: 33 });
Assert.ok(nullPrincipal.isNullPrincipal);
Assert.ok(nullPrincipal.origin.includes("moz-nullprincipal"));
Assert.ok(nullPrincipal.origin.includes("^userContextId=33"));
Assert.ok(nullPrincipal.originNoSuffix.includes("moz-nullprincipal"));
Assert.ok(!nullPrincipal.originNoSuffix.includes("^userContextId=33"));
Assert.ok(nullPrincipal.siteOrigin.includes("moz-nullprincipal"));
Assert.ok(nullPrincipal.siteOrigin.includes("^userContextId=33"));
Assert.ok(nullPrincipal.siteOriginNoSuffix.includes("moz-nullprincipal"));
Assert.ok(!nullPrincipal.siteOriginNoSuffix.includes("^userContextId=33"));

// ExpandedPrincipal checks
let expandedPrincipal = Cu.getObjectPrincipal(Cu.Sandbox([prinicpal2]));
Assert.ok(expandedPrincipal.isExpandedPrincipal);
Assert.equal(
  expandedPrincipal.origin,
  "[Expanded Principal [http://test1.example.com^userContextId=22]]^userContextId=22"
);
Assert.equal(
  expandedPrincipal.originNoSuffix,
  "[Expanded Principal [http://test1.example.com^userContextId=22]]"
);
Assert.equal(
  expandedPrincipal.siteOrigin,
  "[Expanded Principal [http://test1.example.com^userContextId=22]]^userContextId=22"
);
Assert.equal(
  expandedPrincipal.siteOriginNoSuffix,
  "[Expanded Principal [http://test1.example.com^userContextId=22]]"
);

let ipv6URI = Services.io.newURI("https://[2001:db8::ff00:42:8329]:123");
let ipv6Principal = scriptSecMan.createContentPrincipal(ipv6URI, {
  userContextId: 6,
});
Assert.ok(ipv6Principal.isContentPrincipal);
Assert.equal(
  ipv6Principal.origin,
  "https://[2001:db8::ff00:42:8329]:123^userContextId=6"
);
Assert.equal(
  ipv6Principal.originNoSuffix,
  "https://[2001:db8::ff00:42:8329]:123"
);
Assert.equal(
  ipv6Principal.siteOrigin,
  "https://[2001:db8::ff00:42:8329]^userContextId=6"
);
Assert.equal(
  ipv6Principal.siteOriginNoSuffix,
  "https://[2001:db8::ff00:42:8329]"
);
