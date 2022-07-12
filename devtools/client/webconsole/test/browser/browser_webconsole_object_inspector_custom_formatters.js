/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check display of custom formatters.
const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-custom-formatters.html";

add_task(async function() {
  // ToDo: This preference can be removed once the custom formatters feature is stable enough
  await pushPref("devtools.custom-formatters", true);
  await pushPref("devtools.custom-formatters.enabled", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  // Reload the browser to ensure the custom formatters are picked up
  await reloadBrowser();

  await testString(hud);
  await testNumber(hud);
  await testObjectWithoutFormatting(hud);
  await testObjectWithFormattedHeader(hud);
  await testObjectWithFormattedHeaderAndBody(hud);
  await testSideEffectsFreeFormatting(hud);
});

async function testString(hud) {
  info("Test for string not being custom formatted");
  await testCustomFormatting(hud, {
    hasCustomFormatter: false,
    messageText: "string",
  });
}

async function testNumber(hud) {
  info("Test for number not being custom formatted");
  await testCustomFormatting(hud, {
    hasCustomFormatter: false,
    messageText: 1337,
  });
}

async function testObjectWithoutFormatting(hud) {
  info("Test for object not being custom formatted");
  await testCustomFormatting(hud, {
    hasCustomFormatter: false,
    messageText: "Object { noFormat: true }",
  });
}

async function testObjectWithFormattedHeader(hud) {
  info("Simple test for custom formatted header");
  await testCustomFormatting(hud, {
    hasCustomFormatter: true,
    messageText: "custom formatted header",
    headerStyles: "font-size: 3rem;",
  });
}

async function testObjectWithFormattedHeaderAndBody(hud) {
  info("Simple test for custom formatted header with body");
  await testCustomFormatting(hud, {
    hasCustomFormatter: true,
    messageText: "custom formatted body",
    headerStyles: "font-style: italic;",
    bodyText: "body",
    bodyStyles: "font-size: 2rem; font-family: serif;",
  });
}

async function testCustomFormatting(
  hud,
  { hasCustomFormatter, messageText, headerStyles, bodyText, bodyStyles }
) {
  const node = await waitFor(() => {
    return findConsoleAPIMessage(hud, messageText);
  });

  let headerSelector = ".objectBox-jsonml";
  if (bodyText) {
    headerSelector = `[aria-level="1"] ${headerSelector}`;
  }
  const headerJsonMlNode = node.querySelector(headerSelector);
  if (hasCustomFormatter) {
    ok(headerJsonMlNode, "The message is custom formatted");

    if (!headerJsonMlNode) {
      return;
    }

    is(
      headerJsonMlNode.getAttribute("style"),
      headerStyles,
      "The custom formatting of the header is correct"
    );

    if (bodyText) {
      const arrow = node.querySelector(".arrow");

      ok(arrow, "There must be a toggle arrow for the header");

      info("Expanding the Object");
      const onMapOiMutation = waitForNodeMutation(node.querySelector(".tree"), {
        childList: true,
      });

      node.querySelector(".arrow").click();
      await onMapOiMutation;

      ok(
        node.querySelector(".arrow").classList.contains("expanded"),
        "The arrow of the node has the expected class after clicking on it"
      );

      const bodyJsonMlNode = node.querySelector(
        '[aria-level="2"] .objectBox-jsonml'
      );
      ok(bodyJsonMlNode, "The body is custom formatted");

      if (!bodyJsonMlNode) {
        return;
      }

      is(bodyJsonMlNode.textContent, bodyText, "The body text is correct");
      is(
        bodyJsonMlNode.getAttribute("style"),
        bodyStyles,
        "The custom formatting of the body is correct"
      );
    }
  } else {
    ok(!headerJsonMlNode, "The message is not custom formatted");
  }
}

async function testSideEffectsFreeFormatting(hud) {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    is(
      content.wrappedJSObject.formatted,
      0,
      "The variable 'formatted' must be unchanged"
    );
  });
}
