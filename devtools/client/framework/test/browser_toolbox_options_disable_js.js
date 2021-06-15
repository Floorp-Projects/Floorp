/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that disabling JavaScript for a tab works as it should.

const TEST_URI = URL_ROOT + "browser_toolbox_options_disable_js.html";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  // Start on the options panel from where we will toggle the disabling javascript
  // option.
  const toolbox = await gDevTools.showToolboxForTab(tab, { toolId: "options" });

  await testJSEnabled();
  await testJSEnabledIframe();

  // Disable JS.
  await toggleJS(toolbox);

  await testJSDisabled();
  await testJSDisabledIframe();

  // Re-enable JS.
  await toggleJS(toolbox);

  await testJSEnabled();
  await testJSEnabledIframe();

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

async function testJSEnabled() {
  info("Testing that JS is enabled");

  // We use waitForTick here because switching browsingContext.allowJavascript
  // to true takes a while to become live.
  await waitForTick();

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const doc = content.document;
    const output = doc.getElementById("output");
    doc.querySelector("#logJSEnabled").click();
    is(
      output.textContent,
      "JavaScript Enabled",
      'Output is "JavaScript Enabled"'
    );
  });
}

async function testJSEnabledIframe() {
  info("Testing that JS is enabled in the iframe");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const doc = content.document;
    const iframe = doc.querySelector("iframe");
    const iframeDoc = iframe.contentDocument;
    const output = iframeDoc.getElementById("output");
    iframeDoc.querySelector("#logJSEnabled").click();
    is(
      output.textContent,
      "JavaScript Enabled",
      'Output is "JavaScript Enabled" in iframe'
    );
  });
}

async function toggleJS(toolbox) {
  const panel = toolbox.getCurrentPanel();
  const cbx = panel.panelDoc.getElementById("devtools-disable-javascript");

  if (cbx.checked) {
    info("Clearing checkbox to re-enable JS");
  } else {
    info("Checking checkbox to disable JS");
  }

  let javascriptEnabled = await toolbox.commands.targetConfigurationCommand.isJavascriptEnabled();
  is(
    javascriptEnabled,
    !cbx.checked,
    "targetConfigurationCommand.isJavascriptEnabled is correct before the toggle"
  );

  const browserLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser
  );
  cbx.click();
  await browserLoaded;

  javascriptEnabled = await toolbox.commands.targetConfigurationCommand.isJavascriptEnabled();
  is(
    javascriptEnabled,
    !cbx.checked,
    "targetConfigurationCommand.isJavascriptEnabled is correctly updated"
  );
}

async function testJSDisabled() {
  info("Testing that JS is disabled");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const doc = content.document;
    const output = doc.getElementById("output");
    doc.querySelector("#logJSDisabled").click();

    ok(
      output.textContent !== "JavaScript Disabled",
      'output is not "JavaScript Disabled"'
    );
  });
}

async function testJSDisabledIframe() {
  info("Testing that JS is disabled in the iframe");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const doc = content.document;
    const iframe = doc.querySelector("iframe");
    const iframeDoc = iframe.contentDocument;
    const output = iframeDoc.getElementById("output");
    iframeDoc.querySelector("#logJSDisabled").click();
    ok(
      output.textContent !== "JavaScript Disabled",
      'output is not "JavaScript Disabled" in iframe'
    );
  });
}
