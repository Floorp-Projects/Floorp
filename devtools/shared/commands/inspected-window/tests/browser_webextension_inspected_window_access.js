/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function run_inspectedWindow_eval({ tab, codeToEval, extension }) {
  const fakeExtCallerInfo = {
    url: `moz-extension://${extension.uuid}/another/fake-caller-script.js`,
    lineNumber: 1,
    addonId: extension.id,
  };
  const commands = await CommandsFactory.forTab(tab, { isWebExtension: true });
  await commands.targetCommand.startListening();
  const result = await commands.inspectedWindowCommand.eval(
    fakeExtCallerInfo,
    codeToEval,
    {}
  );
  await commands.destroy();
  return result;
}

async function openAboutBlankTabWithExtensionOrigin(extension) {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `moz-extension://${extension.uuid}/manifest.json`
  );
  const loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await ContentTask.spawn(tab.linkedBrowser, null, () => {
    // about:blank inherits the principal when opened from content.
    content.wrappedJSObject.location.assign("about:blank");
  });
  await loaded;
  // Sanity checks:
  is(tab.linkedBrowser.currentURI.spec, "about:blank", "expected tab");
  is(
    tab.linkedBrowser.contentPrincipal.originNoSuffix,
    `moz-extension://${extension.uuid}`,
    "about:blank should be at the extension origin"
  );
  return tab;
}

async function checkEvalResult({
  extension,
  description,
  url,
  createTab = () => BrowserTestUtils.openNewForegroundTab(gBrowser, url),
  expectedResult,
}) {
  const tab = await createTab();
  is(tab.linkedBrowser.currentURI.spec, url, "Sanity check: tab URL");
  const result = await run_inspectedWindow_eval({
    tab,
    codeToEval: "'code executed at ' + location.href",
    extension,
  });
  BrowserTestUtils.removeTab(tab);
  SimpleTest.isDeeply(
    result,
    expectedResult,
    `eval result for devtools.inspectedWindow.eval at ${url} (${description})`
  );
}

async function checkEvalAllowed({ extension, description, url, createTab }) {
  info(`checkEvalAllowed: ${description} (at URL: ${url})`);
  await checkEvalResult({
    extension,
    description,
    url,
    createTab,
    expectedResult: { value: `code executed at ${url}` },
  });
}
async function checkEvalDenied({ extension, description, url, createTab }) {
  info(`checkEvalDenied: ${description} (at URL: ${url})`);
  await checkEvalResult({
    extension,
    description,
    url,
    createTab,
    expectedResult: {
      exceptionInfo: {
        isError: true,
        code: "E_PROTOCOLERROR",
        details: [
          "This extension is not allowed on the current inspected window origin",
        ],
        description: "Inspector protocol error: %s",
      },
    },
  });
}

add_task(async function test_eval_at_http() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });

  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const httpUrl = "http://example.com/";

  // When running with --use-http3-server, http:-URLs cannot be loaded.
  try {
    await fetch(httpUrl);
  } catch {
    info("Skipping test_eval_at_http because http:-URL cannot be loaded");
    return;
  }

  const extension = ExtensionTestUtils.loadExtension({});
  await extension.startup();

  await checkEvalAllowed({
    extension,
    description: "http:-URL",
    url: httpUrl,
  });
  await extension.unload();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_eval_at_https() {
  const extension = ExtensionTestUtils.loadExtension({});
  await extension.startup();

  const privilegedExtension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
  });
  await privilegedExtension.startup();

  await checkEvalAllowed({
    extension,
    description: "https:-URL",
    url: "https://example.com/",
  });

  await checkEvalDenied({
    extension,
    description: "a restricted domain",
    // Domain in extensions.webextensions.restrictedDomains by browser.toml.
    url: "https://test2.example.com/",
  });

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.quarantinedDomains.list", "example.com"]],
  });

  await checkEvalDenied({
    extension,
    description: "a quarantined domain",
    url: "https://example.com/",
  });

  await checkEvalAllowed({
    extension: privilegedExtension,
    description: "a quarantined domain",
    url: "https://example.com/",
  });

  await SpecialPowers.popPrefEnv();

  await extension.unload();
  await privilegedExtension.unload();
});

add_task(async function test_eval_at_sandboxed_page() {
  const extension = ExtensionTestUtils.loadExtension({});
  await extension.startup();

  await checkEvalAllowed({
    extension,
    description: "page with CSP sandbox",
    url: "https://example.com/document-builder.sjs?headers=Content-Security-Policy:sandbox&html=x",
  });
  await checkEvalDenied({
    extension,
    description: "restricted domain with CSP sandbox",
    url: "https://test2.example.com/document-builder.sjs?headers=Content-Security-Policy:sandbox&html=x",
  });

  await extension.unload();
});

add_task(async function test_eval_at_own_extension_origin_allowed() {
  const extension = ExtensionTestUtils.loadExtension({
    background() {
      // eslint-disable-next-line no-undef
      browser.test.sendMessage(
        "blob_url",
        URL.createObjectURL(new Blob(["blob: here", { type: "text/html" }]))
      );
    },
    files: {
      "mozext.html": `<!DOCTYPE html>moz-extension: here`,
    },
  });
  await extension.startup();
  const blobUrl = await extension.awaitMessage("blob_url");

  await checkEvalAllowed({
    extension,
    description: "moz-extension:-URL from own extension",
    url: `moz-extension://${extension.uuid}/mozext.html`,
  });
  await checkEvalAllowed({
    extension,
    description: "blob:-URL from own extension",
    url: blobUrl,
  });
  await checkEvalAllowed({
    extension,
    description: "about:blank with origin from own extension",
    url: "about:blank",
    createTab: () => openAboutBlankTabWithExtensionOrigin(extension),
  });

  await extension.unload();
});

add_task(async function test_eval_at_other_extension_denied() {
  // The extension for which we simulate devtools_page, chosen as caller of
  // devtools.inspectedWindow.eval API calls.
  const extension = ExtensionTestUtils.loadExtension({});
  await extension.startup();

  // The other extension, that |extension| should not be able to access:
  const otherExt = ExtensionTestUtils.loadExtension({
    background() {
      // eslint-disable-next-line no-undef
      browser.test.sendMessage(
        "blob_url",
        URL.createObjectURL(new Blob(["blob: here", { type: "text/html" }]))
      );
    },
    files: {
      "mozext.html": `<!DOCTYPE html>moz-extension: here`,
    },
  });
  await otherExt.startup();
  const otherExtBlobUrl = await otherExt.awaitMessage("blob_url");

  await checkEvalDenied({
    extension,
    description: "moz-extension:-URL from another extension",
    url: `moz-extension://${otherExt.uuid}/mozext.html`,
  });
  await checkEvalDenied({
    extension,
    description: "blob:-URL from another extension",
    url: otherExtBlobUrl,
  });
  await checkEvalDenied({
    extension,
    description: "about:blank with origin from another extension",
    url: "about:blank",
    createTab: () => openAboutBlankTabWithExtensionOrigin(otherExt),
  });

  await otherExt.unload();
  await extension.unload();
});

add_task(async function test_eval_at_about() {
  const extension = ExtensionTestUtils.loadExtension({});
  await extension.startup();
  await checkEvalAllowed({
    extension,
    description: "about:blank (null principal)",
    url: "about:blank",
  });
  await checkEvalDenied({
    extension,
    description: "about:addons (system principal)",
    url: "about:addons",
  });
  await checkEvalDenied({
    extension,
    description: "about:robots (about page)",
    url: "about:robots",
  });
  await extension.unload();
});

add_task(async function test_eval_at_file() {
  // FYI: There is also an equivalent test case with a full end-to-end test at:
  // browser/components/extensions/test/browser/browser_ext_devtools_inspectedWindow_eval_file.js

  const extension = ExtensionTestUtils.loadExtension({});
  await extension.startup();

  // A dummy file URL that can be loaded in a tab.
  const fileUrl =
    "file://" +
    getTestFilePath("browser_webextension_inspected_window_access.js");

  // checkEvalAllowed test helper cannot be used, because the file:-URL may
  // redirect elsewhere, so the comparison with the full URL fails.
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, fileUrl);
  const result = await run_inspectedWindow_eval({
    tab,
    codeToEval: "'code executed at ' + location.protocol",
    extension,
  });
  BrowserTestUtils.removeTab(tab);
  SimpleTest.isDeeply(
    result,
    { value: "code executed at file:" },
    `eval result for devtools.inspectedWindow.eval at ${fileUrl}`
  );

  await extension.unload();
});
