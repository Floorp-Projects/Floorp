/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { ObjectUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ObjectUtils.sys.mjs"
);

const MESSAGES = [
  {
    trigger: { id: "defaultBrowserCheck" },
    targeting:
      "source == 'startup' && !isMajorUpgrade && !activeNotifications && totalBookmarksCount == 5",
  },
  {
    groups: ["eco"],
    trigger: {
      id: "defaultBrowserCheck",
    },
    targeting:
      "source == 'startup' && !isMajorUpgrade && !activeNotifications && totalBookmarksCount == 5",
  },
];

let EXPERIMENT_VALIDATOR;

add_setup(async function setup() {
  EXPERIMENT_VALIDATOR = await schemaValidatorFor(
    "resource://activity-stream/schemas/MessagingExperiment.schema.json"
  );
});

add_task(function test_reach_experiments_validation() {
  for (const [index, message] of MESSAGES.entries()) {
    assertValidates(
      EXPERIMENT_VALIDATOR,
      message,
      `Message ${index} validates as a MessagingExperiment`
    );
  }
});

function depError(has, missing) {
  return {
    instanceLocation: "#",
    keyword: "dependentRequired",
    keywordLocation: "#/oneOf/1/allOf/0/$ref/dependantRequired",
    error: `Instance has "${has}" but does not have "${missing}".`,
  };
}

function assertContains(haystack, needle) {
  Assert.ok(
    haystack.find(item => ObjectUtils.deepEqual(item, needle)) !== null
  );
}

add_task(function test_reach_experiment_dependentRequired() {
  info(
    "Testing that if id is present then content and template are not required"
  );

  {
    const message = {
      ...MESSAGES[0],
      id: "message-id",
    };

    const result = EXPERIMENT_VALIDATOR.validate(message);
    Assert.ok(result.valid, "message should validate");
  }

  info("Testing that if content is present then id and template are required");
  {
    const message = {
      ...MESSAGES[0],
      content: {},
    };

    const result = EXPERIMENT_VALIDATOR.validate(message);
    Assert.ok(!result.valid, "message should not validate");
    assertContains(result.errors, depError("content", "id"));
    assertContains(result.errors, depError("content", "template"));
  }

  info("Testing that if template is present then id and content are required");
  {
    const message = {
      ...MESSAGES[0],
      template: "cfr",
    };

    const result = EXPERIMENT_VALIDATOR.validate(message);
    Assert.ok(!result.valid, "message should not validate");
    assertContains(result.errors, depError("template", "content"));
    assertContains(result.errors, depError("template", "id"));
  }
});
