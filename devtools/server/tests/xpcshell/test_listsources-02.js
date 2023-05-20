/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check getting sources before there are any.
 */

var gNumTimesSourcesSent = 0;

add_task(
  threadFrontTest(async ({ threadFront, client }) => {
    client.request = (function (origRequest) {
      return function (request, onResponse) {
        if (request.type === "sources") {
          ++gNumTimesSourcesSent;
        }
        return origRequest.call(this, request, onResponse);
      };
    })(client.request);

    // Test listing zero sources
    const packet = await threadFront.getSources();

    Assert.ok(!packet.error);
    Assert.ok(!!packet.sources);
    Assert.equal(packet.sources.length, 0);

    Assert.ok(
      gNumTimesSourcesSent <= 1,
      "Should only send one sources request at most, even though we" +
        " might have had to send one to determine feature support."
    );
  })
);
