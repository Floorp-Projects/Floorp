/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const protocol = require("resource://devtools/shared/protocol.js");
const { RetVal } = protocol;

// Test invalid response specs throw when generating the Actor specification.

// Test top level array response
add_task(async function () {
  Assert.throws(() => {
    protocol.generateActorSpec({
      typeName: "invalidArrayResponse",
      methods: {
        invalidMethod: {
          response: RetVal("array:string"),
        },
      },
    });
  }, /Arrays should be wrapped in objects/);

  protocol.generateActorSpec({
    typeName: "validArrayResponse",
    methods: {
      validMethod: {
        response: {
          someArray: RetVal("array:string"),
        },
      },
    },
  });
  ok(true, "Arrays wrapped in object are valid response packets");
});

// Test response with several placeholders
add_task(async function () {
  Assert.throws(() => {
    protocol.generateActorSpec({
      typeName: "tooManyPlaceholdersResponse",
      methods: {
        invalidMethod: {
          response: {
            prop1: RetVal("json"),
            prop2: RetVal("json"),
          },
        },
      },
    });
  }, /More than one RetVal specified in response/);
});
