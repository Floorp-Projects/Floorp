ChromeUtils.defineModuleGetter(this, "ASRouterTargeting",
"resource://activity-stream/lib/ASRouterTargeting.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");

// ASRouterTargeting.isMatch
add_task(async function should_do_correct_targeting() {
  is(await ASRouterTargeting.isMatch("FOO", {FOO: true}), true, "should return true for a matching value");
  is(await ASRouterTargeting.isMatch("!FOO", {FOO: true}), false, "should return false for a non-matching value");
});

add_task(async function should_handle_async_getters() {
  const context = {get FOO() { return Promise.resolve(true); }};
  is(await ASRouterTargeting.isMatch("FOO", context), true, "should return true for a matching async value");
});

// ASRouterTargeting.findMatchingMessage
add_task(async function find_matching_message() {
  const messages = [
    {id: "foo", targeting: "FOO"},
    {id: "bar", targeting: "!FOO"}
  ];
  const context = {FOO: true};

  const match = await ASRouterTargeting.findMatchingMessage(messages, context);

  is(match, messages[0], "should match and return the correct message");
});

add_task(async function return_nothing_for_no_matching_message() {
  const messages = [{id: "bar", targeting: "!FOO"}];
  const context = {FOO: true};

  const match = await ASRouterTargeting.findMatchingMessage(messages, context);

  is(match, undefined, "should return nothing since no matching message exists");
});

// ASRouterTargeting.Environment
add_task(async function checkProfileAgeCreated() {
  let profileAccessor = new ProfileAge();
  is(await ASRouterTargeting.Environment.profileAgeCreated, await profileAccessor.created,
    "should return correct profile age creation date");

  const message = {id: "foo", targeting: `profileAgeCreated > ${await profileAccessor.created - 100}`};
  is(await ASRouterTargeting.findMatchingMessage([message]), message,
    "should select correct item by profile age created");
});

add_task(async function checkProfileAgeReset() {
  let profileAccessor = new ProfileAge();
  is(await ASRouterTargeting.Environment.profileAgeReset, await profileAccessor.reset,
    "should return correct profile age reset");

  const message = {id: "foo", targeting: `profileAgeReset == ${await profileAccessor.reset}`};
  is(await ASRouterTargeting.findMatchingMessage([message]), message,
    "should select correct item by profile age reset");
});

add_task(async function checkhasFxAccount() {
  await pushPrefs(["services.sync.username", "someone@foo.com"]);
  is(await ASRouterTargeting.Environment.hasFxAccount, true,
    "should return true if a fx account is set");

  const message = {id: "foo", targeting: "hasFxAccount"};
  is(await ASRouterTargeting.findMatchingMessage([message]), message,
    "should select correct item by hasFxAccount");
});
