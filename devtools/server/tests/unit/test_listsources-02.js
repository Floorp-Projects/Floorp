/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check getting sources before there are any.
 */

var gThreadFront;

var gNumTimesSourcesSent = 0;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, client }) => {
      gThreadFront = threadFront;
      client.request = (function(origRequest) {
        return function(request, onResponse) {
          if (request.type === "sources") {
            ++gNumTimesSourcesSent;
          }
          return origRequest.call(this, request, onResponse);
        };
      })(client.request);
      test_listing_zero_sources();
    },
    { waitForFinish: true }
  )
);

function test_listing_zero_sources() {
  gThreadFront.getSources().then(function(packet) {
    Assert.ok(!packet.error);
    Assert.ok(!!packet.sources);
    Assert.equal(packet.sources.length, 0);

    Assert.ok(
      gNumTimesSourcesSent <= 1,
      "Should only send one sources request at most, even though we" +
        " might have had to send one to determine feature support."
    );

    threadFrontTestFinished();
  });
}
