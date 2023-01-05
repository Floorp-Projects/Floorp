/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check display of custom formatters.
const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-custom-formatters-errors.html";

add_task(async function() {
  // ToDo: This preference can be removed once the custom formatters feature is stable enough
  await pushPref("devtools.custom-formatters", true);
  await pushPref("devtools.custom-formatters.enabled", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  // Reload the browser to ensure the custom formatters are picked up
  await reloadBrowser();

  await testHeaderNotAFunction(hud);
  await testHeaderNotReturningJsonMl(hud);
  await testHeaderNotReturningElementType(hud);
  await testHeaderThrowing(hud);
  await testHeaderHavingSideEffects(hud);
  await testHasBodyNotAFunction(hud);
  await testHasBodyThrowing(hud);
  await testHasBodyHavingSideEffects(hud);
  await testBodyNotAFunction(hud);
  await testBodyReturningNull(hud);
  await testBodyNotReturningJsonMl(hud);
  await testBodyNotReturningElementType(hud);
  await testBodyThrowing(hud);
  await testBodyHavingSideEffects(hud);
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
    source: "test-console-custom-formatters-errors.html:19:18",
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

async function testHeaderHavingSideEffects(hud) {
  info(`Test for "header" function having side effects`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[4].header was not run because it has side effects`,
  });
}

async function testHasBodyNotAFunction(hud) {
  info(`Test for "hasBody" not being a function`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[5].hasBody should be a function, got number`,
  });
}

async function testHasBodyThrowing(hud) {
  info(`Test for "hasBody" function throwing`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[6].hasBody threw: ERROR`,
  });
}

async function testHasBodyHavingSideEffects(hud) {
  info(`Test for "hasBody" function having side effects`);
  await testCustomFormatting(hud, {
    messageText: `Custom formatter failed: devtoolsFormatters[7].hasBody was not run because it has side effects`,
  });
}

async function testBodyNotAFunction(hud) {
  info(`Test for "body" not being a function`);
  await testCustomFormatting(hud, {
    messageText: "body not a function",
    bodyText: `Custom formatter failed: devtoolsFormatters[8].body should be a function, got number`,
  });
}

async function testBodyReturningNull(hud) {
  info(`Test for "body" returning null`);
  await testCustomFormatting(hud, {
    messageText: "body returns null",
    bodyText: `Custom formatter failed: devtoolsFormatters[9].body should return an array, got null`,
  });
}

async function testBodyNotReturningJsonMl(hud) {
  info(`Test for "body" not returning JsonML`);
  await testCustomFormatting(hud, {
    messageText: "body doesn't return JsonML",
    bodyText: `Custom formatter failed: devtoolsFormatters[10].body should return an array, got number`,
  });
}

async function testBodyNotReturningElementType(hud) {
  info(`Test for "body" function returning array without element type`);
  await testCustomFormatting(hud, {
    messageText: "body array misses element type",
    bodyText: `Custom formatter failed: devtoolsFormatters[11].body returned an empty array`,
  });
}

async function testBodyThrowing(hud) {
  info(`Test for "body" function throwing`);
  await testCustomFormatting(hud, {
    messageText: "body throws",
    bodyText: `Custom formatter failed: devtoolsFormatters[12].body threw: ERROR`,
  });
}

async function testBodyHavingSideEffects(hud) {
  info(`Test for "body" function having side effects`);
  await testCustomFormatting(hud, {
    messageText: "body has side effects",
    bodyText: `Custom formatter failed: devtoolsFormatters[13].body was not run because it has side effects`,
  });
}

async function testErrorsLoggedOnce(hud) {
  const messages = findMessagesByType(hud, "custom formatter failed", ".error");

  messages.forEach(async message => {
    await checkUniqueMessageExists(hud, message.textContent, ".error");
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
