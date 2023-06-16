/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the NetworkCommand API around request blocking

add_task(async function () {
  info("Test NetworkCommand request blocking");
  const tab = await addTab("data:text/html,foo");
  const commands = await CommandsFactory.forTab(tab);
  const networkCommand = commands.networkCommand;
  const resourceCommand = commands.resourceCommand;

  // Usage of request blocking APIs requires to listen to NETWORK_EVENT.
  await resourceCommand.watchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable: () => {},
  });

  let blockedUrls = await networkCommand.getBlockedUrls();
  Assert.deepEqual(
    blockedUrls,
    [],
    "The list of blocked URLs is originaly empty"
  );

  await networkCommand.blockRequestForUrl("https://foo.com");
  blockedUrls = await networkCommand.getBlockedUrls();
  Assert.deepEqual(
    blockedUrls,
    ["https://foo.com"],
    "The freshly added blocked URL is reported as blocked"
  );

  // We pass "url filters" which can be only part of a URL string
  await networkCommand.blockRequestForUrl("bar");
  blockedUrls = await networkCommand.getBlockedUrls();
  Assert.deepEqual(
    blockedUrls,
    ["https://foo.com", "bar"],
    "The second blocked URL is also reported as blocked"
  );

  await networkCommand.setBlockedUrls(["https://mozilla.org"]);
  blockedUrls = await networkCommand.getBlockedUrls();
  Assert.deepEqual(
    blockedUrls,
    ["https://mozilla.org"],
    "setBlockedUrls replace the whole list of blocked URLs"
  );

  await networkCommand.unblockRequestForUrl("https://mozilla.org");
  blockedUrls = await networkCommand.getBlockedUrls();
  Assert.deepEqual(
    blockedUrls,
    [],
    "The unblocked URL disappear from the list of blocked URLs"
  );

  await commands.destroy();
});
