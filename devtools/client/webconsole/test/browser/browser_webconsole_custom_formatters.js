/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check display of custom formatters.
const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-custom-formatters.html";

add_task(async function () {
  await pushPref("devtools.custom-formatters.enabled", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  // Reload the browser to ensure the custom formatters are picked up
  await reloadBrowser();

  await testString(hud);
  await testNumber(hud);
  await testObjectWithoutFormatting(hud);
  await testObjectWithFormattedHeader(hud);
  await testObjectWithFormattedHeaderAndBody(hud);
  await testCustomFormatterWithObjectTag(hud);
  await testProxyObject(hud);
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

async function testCustomFormatterWithObjectTag(hud) {
  info(`Test for custom formatted object with "object" tag in the jsonMl`);
  const node = await waitFor(() => {
    return findConsoleAPIMessage(hud, "object tag");
  });

  const headerJsonMlNode = node.querySelector(".objectBox-jsonml");
  is(
    headerJsonMlNode.getAttribute("style"),
    "color: purple;",
    "The custom formatting of the header is correct"
  );
  const [buttonEl, child1, child2, child3, child4] =
    headerJsonMlNode.childNodes;
  is(child1.textContent, "object tag", "Got expected first item");
  is(
    child2.textContent,
    `~[1,"a"]~`,
    "Got expected second item, the replaced object, custom formatted"
  );
  ok(
    child3.classList.contains("objectBox-null"),
    "Got expected third item, an actual NullRep"
  );
  is(child3.textContent, `null`, "third item has expected content");

  is(
    child4.textContent,
    ` | serialized: 42n undefined null Infinity [object Object]`,
    "Got expected fourth item, serialized values"
  );

  buttonEl.click();
  const bodyLevel1 = await waitFor(() =>
    node.querySelector(".objectBox-jsonml-body-wrapper .objectBox-jsonml")
  );
  const [bodyChild1, bodyChild2] = bodyLevel1.childNodes;
  is(bodyChild1.textContent, "body");

  const bodyCustomFormattedChild = await waitFor(() =>
    bodyChild2.querySelector(".objectBox-jsonml")
  );
  const [subButtonEl, subChild1, subChild2, subChild3] =
    bodyCustomFormattedChild.childNodes;
  ok(!!subButtonEl, "The body child can also be expanded");
  is(subChild1.textContent, "object tag", "Got expected first item");
  is(
    subChild2.textContent,
    `~[2,"b"]~`,
    "Got expected body second item, the replaced object, custom formatted"
  );
  ok(
    subChild3.classList.contains("object-inspector"),
    "Got expected body third item, an actual ObjectInspector"
  );
  is(
    subChild3.textContent,
    `Array [ 2, "b" ]`,
    "body third item has expected content"
  );
}

async function testProxyObject(hud) {
  info("Test that Proxy objects can be handled by custom formatter");
  await testCustomFormatting(hud, {
    hasCustomFormatter: true,
    messageText: "Formatted Proxy",
    headerStyles: "font-weight: bold;",
  });
}

async function testCustomFormatting(
  hud,
  { hasCustomFormatter, messageText, headerStyles, bodyText, bodyStyles }
) {
  const node = await waitFor(() => {
    return findMessageVirtualizedByType({
      hud,
      text: messageText,
      typeSelector: ".console-api",
    });
  });

  const headerJsonMlNode = node.querySelector(".objectBox-jsonml");
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
      const arrow = node.querySelector(".collapse-button");

      ok(arrow, "There must be a toggle arrow for the header");

      info("Expanding the Object");
      const onBodyRendered = waitFor(
        () =>
          !!node.querySelector(
            ".objectBox-jsonml-body-wrapper .objectBox-jsonml"
          )
      );

      arrow.click();
      await onBodyRendered;

      ok(
        node.querySelector(".collapse-button").classList.contains("expanded"),
        "The arrow of the node has the expected class after clicking on it"
      );

      const bodyJsonMlNode = node.querySelector(
        ".objectBox-jsonml-body-wrapper > .objectBox-jsonml"
      );
      ok(bodyJsonMlNode, "The body is custom formatted");

      is(bodyJsonMlNode?.textContent, bodyText, "The body text is correct");
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
