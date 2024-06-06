/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { GenAI } = ChromeUtils.importESModule(
  "resource:///modules/GenAI.sys.mjs"
);

add_setup(() => {
  Services.prefs.setStringPref("browser.ml.chat.prompt.prefix", "");
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref("browser.ml.chat.prompt.prefix")
  );
});

/**
 * Check that prompts come from label or value
 */
add_task(function test_basic_prompt() {
  Assert.equal(
    GenAI.buildChatPrompt({ label: "a" }),
    "a",
    "Uses label for prompt"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ value: "b" }),
    "b",
    "Uses value for prompt"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "a", value: "b" }),
    "b",
    "Prefers value for prompt"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "a", value: "" }),
    "a",
    "Falls back to label for prompt"
  );
});

/**
 * Check that placeholders can use context
 */
add_task(function test_prompt_placeholders() {
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a%" }),
    "%a%",
    "Placeholder kept without context"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a%" }, { a: "z" }),
    "z",
    "Placeholder replaced with context"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a%%a%%a%" }, { a: "z" }),
    "zzz",
    "Repeat placeholders replaced with context"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a% %b%" }, { a: "z" }),
    "z %b%",
    "Missing placeholder context not replaced"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a% %b%" }, { a: "z", b: "y" }),
    "z y",
    "Multiple placeholders replaced with context"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a% %b%" }, { a: "%b%", b: "y" }),
    "%b% y",
    "Placeholders from original prompt replaced with context"
  );
});

/**
 * Check that placeholder options are used
 */
add_task(function test_prompt_placeholder_options() {
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a|1%" }, { a: "xyz" }),
    "x",
    "Context reduced to 1"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a|2%" }, { a: "xyz" }),
    "xy",
    "Context reduced to 2"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a|3%" }, { a: "xyz" }),
    "xyz",
    "Context kept to 3"
  );
});

/**
 * Check that prefix pref is added to prompt
 */
add_task(function test_prompt_prefix() {
  Services.prefs.setStringPref("browser.ml.chat.prompt.prefix", "hello ");
  Assert.equal(
    GenAI.buildChatPrompt({ label: "world" }),
    "hello world",
    "Prefix and prompt combined"
  );

  Services.prefs.setStringPref("browser.ml.chat.prompt.prefix", "%a% ");
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a%" }, { a: "hi" }),
    "hi hi",
    "Context used for prefix and prompt"
  );
});
