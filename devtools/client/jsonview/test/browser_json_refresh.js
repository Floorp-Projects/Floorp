/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_FILE = "simple_json.json";

add_task(async function() {
  info("Test JSON refresh started");

  // generate file:// URI for JSON file and load in new tab
  const dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_JSON_FILE);
  const uri = Services.io.newFileURI(dir);
  const tab = await addJsonViewTab(uri.spec);

  // perform sanity checks for URI and pricnipals in loadInfo
  await ContentTask.spawn(tab.linkedBrowser, {TEST_JSON_FILE}, async function ({TEST_JSON_FILE}) { // eslint-disable-line
    const channel = content.document.docShell.currentDocumentChannel;
    const channelURI = channel.URI.spec;
    ok(channelURI.startsWith("file://") && channelURI.includes(TEST_JSON_FILE),
       "sanity: correct channel uri");
    const contentPolicyType = channel.loadInfo.externalContentPolicyType;
    is(contentPolicyType, Ci.nsIContentPolicy.TYPE_DOCUMENT,
       "sanity: correct contentPolicyType");

    const loadingPrincipal = channel.loadInfo.loadingPrincipal;
    is(loadingPrincipal, null, "sanity: correct loadingPrincipal");
    const triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
    ok(Services.scriptSecurityManager.isSystemPrincipal(triggeringPrincipal),
       "sanity: correct triggeringPrincipal");
    const principalToInherit = channel.loadInfo.principalToInherit;
    ok(principalToInherit.isNullPrincipal, "sanity: correct principalToInherit");
    ok(content.document.nodePrincipal.isNullPrincipal,
       "sanity: correct doc.nodePrincipal");
  });

  // reload the tab
  const loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await loaded;

  // check principals in loadInfo are still correct
  await ContentTask.spawn(tab.linkedBrowser, {TEST_JSON_FILE}, async function ({TEST_JSON_FILE}) { // eslint-disable-line
    const channel = content.document.docShell.currentDocumentChannel;
    const channelURI = channel.URI.spec;
    ok(channelURI.startsWith("file://") && channelURI.includes(TEST_JSON_FILE),
       "reloaded: correct channel uri");
    const contentPolicyType = channel.loadInfo.externalContentPolicyType;
    is(contentPolicyType, Ci.nsIContentPolicy.TYPE_DOCUMENT,
       "reloaded: correct contentPolicyType");

    const loadingPrincipal = channel.loadInfo.loadingPrincipal;
    is(loadingPrincipal, null, "reloaded: correct loadingPrincipal");
    const triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
    ok(Services.scriptSecurityManager.isSystemPrincipal(triggeringPrincipal),
       "reloaded: correct triggeringPrincipal");
    const principalToInherit = channel.loadInfo.principalToInherit;
    ok(principalToInherit.isNullPrincipal, "reloaded: correct principalToInherit");
    ok(content.document.nodePrincipal.isNullPrincipal,
       "reloaded: correct doc.nodePrincipal");
  });
});
