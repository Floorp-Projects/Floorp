/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check display of custom formatters.
const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-custom-formatters-errors.html";

add_task(async function () {
  await pushPref("devtools.custom-formatters.enabled", true);

  // enable "can't access property "y", x is undefined" error message
  await pushPref("javascript.options.property_error_message_fix", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  // Reload the browser to ensure the custom formatters are picked up
  await reloadBrowser();

  await testHeaderNotAFunction(hud);
  await testHeaderNotReturningJsonMl(hud);
  await testHeaderNotReturningElementType(hud);
  await testHeaderThrowing(hud);
  await testHasBodyNotAFunction(hud);
  await testHasBodyThrowing(hud);
  await testBodyNotAFunction(hud);
  await testBodyReturningNull(hud);
  await testBodyNotReturningJsonMl(hud);
  await testBodyNotReturningElementType(hud);
  await testBodyThrowing(hud);
  await testIncorrectObjectTag(hud);
  await testInvalidTagname(hud);
  await testNoPrivilegedAccess(hud);
  await testErrorsLoggedOnce(hud);
});

async function testHeaderNotAFunction(hud) {
  info(`Test for "header" not being a function`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[0].header should be a function, got number`,
  });
}

async function testHeaderNotReturningJsonMl(hud) {
  info(`Test for "header" not returning JsonML`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[1].header should return an array, got number`,
    source: "test-console-custom-formatters-errors.html:19:19",
  });
}

async function testHeaderNotReturningElementType(hud) {
  info(`Test for "header" function returning array without element type`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[2].header returned an empty array`,
  });
}

async function testHeaderThrowing(hud) {
  info(`Test for "header" function throwing`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[3].header threw: ERROR`,
  });
}

async function testHasBodyNotAFunction(hud) {
  info(`Test for "hasBody" not being a function`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[4].hasBody should be a function, got number`,
  });
}

async function testHasBodyThrowing(hud) {
  info(`Test for "hasBody" function throwing`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[5].hasBody threw: ERROR`,
  });
}

async function testBodyNotAFunction(hud) {
  info(`Test for "body" not being a function`);
  await testCustomFormatting(hud, {
    messageText: "body not a function",
    bodyText: `Custom formatter failed: devtoolsFormatters[6].body should be a function, got number`,
  });
}

async function testBodyReturningNull(hud) {
  info(`Test for "body" returning null`);
  await testCustomFormatting(hud, {
    messageText: "body returns null",
    bodyText: `Custom formatter failed: devtoolsFormatters[7].body should return an array, got null`,
  });
}

async function testBodyNotReturningJsonMl(hud) {
  info(`Test for "body" not returning JsonML`);
  await testCustomFormatting(hud, {
    messageText: "body doesn't return JsonML",
    bodyText: `Custom formatter failed: devtoolsFormatters[8].body should return an array, got number`,
  });
}

async function testBodyNotReturningElementType(hud) {
  info(`Test for "body" function returning array without element type`);
  await testCustomFormatting(hud, {
    messageText: "body array misses element type",
    bodyText: `Custom formatter failed: devtoolsFormatters[9].body returned an empty array`,
  });
}

async function testBodyThrowing(hud) {
  info(`Test for "body" function throwing`);
  await testCustomFormatting(hud, {
    messageText: "body throws",
    bodyText: `Custom formatter failed: devtoolsFormatters[10].body threw: ERROR`,
  });
}

async function testErrorsLoggedOnce(hud) {
  const messages = findMessagesByType(hud, "custom formatter failed", ".error");

  messages.forEach(async message => {
    await checkUniqueMessageExists(hud, message.textContent, ".error");
  });
}

async function testIncorrectObjectTag(hud) {
  info(`Test for "object" tag without attribute`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[11] couldn't be run: "object" tag should have attributes`,
  });

  info(`Test for "object" tag without "object" attribute`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[12] couldn't be run: attribute of "object" tag should have an "object" property`,
  });

  info(`Test for infinite "object" tag`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: Too deep hierarchy of inlined custom previews`,
  });
}

async function testInvalidTagname(hud) {
  info(`Test invalid tagname in the returned JsonML`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[14] couldn't be run: tagName should be a string, got number`,
  });
}

async function testNoPrivilegedAccess(hud) {
  info(`Test for denied access to windowUtils from hook`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[17].header threw: can't access property "garbageCollect", window.windowUtils is undefined`,
  });
}

async function testCustomFormatting(hud, { messageText, source, bodyText }) {
  const headerNode = bodyText
    ? await waitFor(() => {
        return findConsoleAPIMessage(hud, messageText);
      })
    : await waitFor(() => {
        return findErrorMessage(hud, messageText);
      });

  ok(true, `Got expected message: ${messageText}`);

  if (source) {
    const sourceLink = headerNode.querySelector(".message-location");
    is(sourceLink?.textContent, source, "Source location is correct");
  }

  if (bodyText) {
    const arrow = headerNode.querySelector(".collapse-button");

    ok(arrow, "There must be a toggle arrow for the header");

    info("Expanding the Object");
    const bodyErrorNode = waitFor(() => {
      return findErrorMessage(hud, bodyText);
    });

    arrow.click();
    await bodyErrorNode;

    ok(
      headerNode
        .querySelector(".collapse-button")
        .classList.contains("expanded"),
      "The arrow of the node has the expected class after clicking on it"
    );
  }
}
