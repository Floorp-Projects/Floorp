/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_FILE = "simple_json.json";

add_task(function* () {
  info("Test JSON refresh started");

  // generate file:// URI for JSON file and load in new tab
  let dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_JSON_FILE);
  let uri = Services.io.newFileURI(dir);
  let tab = yield addJsonViewTab(uri.spec);

  // perform sanity checks for URI and pricnipals in loadInfo
  yield ContentTask.spawn(tab.linkedBrowser, {TEST_JSON_FILE}, function*({TEST_JSON_FILE}) { // eslint-disable-line
    let channel = content.document.docShell.currentDocumentChannel;
    let channelURI = channel.URI.spec;
    ok(channelURI.startsWith("file://") && channelURI.includes(TEST_JSON_FILE),
       "sanity: correct channel uri");
    let contentPolicyType = channel.loadInfo.externalContentPolicyType;
    is(contentPolicyType, Ci.nsIContentPolicy.TYPE_DOCUMENT,
       "sanity: correct contentPolicyType");

    let loadingPrincipal = channel.loadInfo.loadingPrincipal;
    is(loadingPrincipal, null, "sanity: correct loadingPrincipal");
    let triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
    ok(Services.scriptSecurityManager.isSystemPrincipal(triggeringPrincipal),
       "sanity: correct triggeringPrincipal");
    let principalToInherit = channel.loadInfo.principalToInherit;
    ok(principalToInherit.isNullPrincipal, "sanity: correct principalToInherit");
    ok(content.document.nodePrincipal.isNullPrincipal,
       "sanity: correct doc.nodePrincipal");
  });

  // reload the tab
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  yield loaded;

  // check principals in loadInfo are still correct
  yield ContentTask.spawn(tab.linkedBrowser, {TEST_JSON_FILE}, function*({TEST_JSON_FILE}) { // eslint-disable-line
    let channel = content.document.docShell.currentDocumentChannel;
    let channelURI = channel.URI.spec;
    ok(channelURI.startsWith("file://") && channelURI.includes(TEST_JSON_FILE),
       "reloaded: correct channel uri");
    let contentPolicyType = channel.loadInfo.externalContentPolicyType;
    is(contentPolicyType, Ci.nsIContentPolicy.TYPE_DOCUMENT,
       "reloaded: correct contentPolicyType");

    let loadingPrincipal = channel.loadInfo.loadingPrincipal;
    is(loadingPrincipal, null, "reloaded: correct loadingPrincipal");
    let triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
    ok(Services.scriptSecurityManager.isSystemPrincipal(triggeringPrincipal),
       "reloaded: correct triggeringPrincipal");
    let principalToInherit = channel.loadInfo.principalToInherit;
    ok(principalToInherit.isNullPrincipal, "reloaded: correct principalToInherit");
    ok(content.document.nodePrincipal.isNullPrincipal,
       "reloaded: correct doc.nodePrincipal");
  });
});
