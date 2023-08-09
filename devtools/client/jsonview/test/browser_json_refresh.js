/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_FILE = "simple_json.json";

add_task(async function () {
  info("Test JSON refresh started");

  // generate file:// URI for JSON file and load in new tab
  const dir = getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_JSON_FILE);
  dir.normalize();
  const uri = Services.io.newFileURI(dir);
  const tab = await addJsonViewTab(uri.spec);

  // perform sanity checks for URI and principals in loadInfo
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ TEST_JSON_FILE }],
    // eslint-disable-next-line no-shadow
    async function ({ TEST_JSON_FILE }) {
      const channel = content.docShell.currentDocumentChannel;
      const channelURI = channel.URI.spec;
      ok(
        channelURI.startsWith("file://") && channelURI.includes(TEST_JSON_FILE),
        "sanity: correct channel uri"
      );
      const contentPolicyType = channel.loadInfo.externalContentPolicyType;
      is(
        contentPolicyType,
        Ci.nsIContentPolicy.TYPE_DOCUMENT,
        "sanity: correct contentPolicyType"
      );

      const loadingPrincipal = channel.loadInfo.loadingPrincipal;
      is(loadingPrincipal, null, "sanity: correct loadingPrincipal");
      const triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
      ok(
        triggeringPrincipal.isSystemPrincipal,
        "sanity: correct triggeringPrincipal"
      );
      const principalToInherit = channel.loadInfo.principalToInherit;
      ok(!principalToInherit, "sanity: no principalToInherit");
      is(
        content.document.nodePrincipal.origin,
        "resource://devtools",
        "sanity: correct doc.nodePrincipal"
      );
    }
  );

  // reload the tab
  await reloadBrowser();

  // check principals in loadInfo are still correct
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ TEST_JSON_FILE }],
    // eslint-disable-next-line no-shadow
    async function ({ TEST_JSON_FILE }) {
      // eslint-disable-line
      const channel = content.docShell.currentDocumentChannel;
      const channelURI = channel.URI.spec;
      ok(
        channelURI.startsWith("file://") && channelURI.includes(TEST_JSON_FILE),
        "reloaded: correct channel uri"
      );
      const contentPolicyType = channel.loadInfo.externalContentPolicyType;
      is(
        contentPolicyType,
        Ci.nsIContentPolicy.TYPE_DOCUMENT,
        "reloaded: correct contentPolicyType"
      );

      const loadingPrincipal = channel.loadInfo.loadingPrincipal;
      is(loadingPrincipal, null, "reloaded: correct loadingPrincipal");
      const triggeringPrincipal = channel.loadInfo.triggeringPrincipal;
      ok(
        triggeringPrincipal.isSystemPrincipal,
        "reloaded: correct triggeringPrincipal"
      );
      const principalToInherit = channel.loadInfo.principalToInherit;
      ok(!principalToInherit, "reloaded: no principalToInherit");
      is(
        content.document.nodePrincipal.origin,
        "resource://devtools",
        "reloaded: correct doc.nodePrincipal"
      );
    }
  );
});
