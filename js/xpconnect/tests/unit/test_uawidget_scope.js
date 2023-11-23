/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {NetUtil} = ChromeUtils.importESModule("resource://gre/modules/NetUtil.sys.mjs");
const {TestUtils} = ChromeUtils.importESModule("resource://testing-common/TestUtils.sys.mjs");

function getWindowlessBrowser(url) {
  let ssm = Services.scriptSecurityManager;
  let uri = NetUtil.newURI(url);
  let principal = ssm.createContentPrincipal(uri, {});
  let webnav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webnav.docShell;
  docShell.createAboutBlankDocumentViewer(principal, principal);

  let document = webnav.document;
  let video = document.createElement("video");
  document.documentElement.appendChild(video);

  let shadowRoot = video.openOrClosedShadowRoot;
  ok(shadowRoot, "should have shadowRoot");
  ok(shadowRoot.isUAWidget(), "ShadowRoot should be a UAWidget");
  equal(Cu.getGlobalForObject(shadowRoot), Cu.getUAWidgetScope(principal),
        "shadowRoot should be in UAWidget scope");

  return webnav;
}

function StubPolicy(id) {
  return new WebExtensionPolicy({
    id,
    mozExtensionHostname: id,
    baseURL: `file:///{id}`,
    allowedOrigins: new MatchPatternSet([]),
    localizeCallback(string) {},
  });
}

// See https://bugzilla.mozilla.org/show_bug.cgi?id=1588356
add_task(async function() {
  let policy = StubPolicy("foo");
  policy.active = true;

  let webnav = getWindowlessBrowser("moz-extension://foo/a.html");
  webnav.close();

  // Wrappers are nuked asynchronously, so wait for that to happen.
  await TestUtils.topicObserved("inner-window-nuked");

  webnav = getWindowlessBrowser("moz-extension://foo/a.html");
  webnav.close();

  policy.active = false;
});
