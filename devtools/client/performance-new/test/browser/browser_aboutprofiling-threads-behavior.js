/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test() {
  info(
    "Test the behavior of thread toggling and the text summary works as expected."
  );

  // This test assumes that the Web Developer preset is set by default, which is
  // not the case on Nightly and custom builds.
  BackgroundJSM.changePreset(
    "aboutprofiling",
    "web-developer",
    Services.profiler.GetFeatures()
  );

  await withAboutProfiling(async document => {
    const threadTextEl = await getNearestInputFromText(
      document,
      "Add custom threads by name:"
    );

    is(
      getActiveConfiguration().threads.join(","),
      "GeckoMain,Compositor,Renderer,DOM Worker",
      "The threads starts out with the default"
    );
    is(
      threadTextEl.value,
      "GeckoMain,Compositor,Renderer,DOM Worker",
      "The threads starts out with the default in the thread text input"
    );

    await clickThreadCheckbox(document, "Compositor", "Toggle off");

    is(
      getActiveConfiguration().threads.join(","),
      "GeckoMain,Renderer,DOM Worker",
      "The threads have been updated"
    );
    is(
      threadTextEl.value,
      "GeckoMain,Renderer,DOM Worker",
      "The threads have been updated in the thread text input"
    );

    await clickThreadCheckbox(document, "DNS Resolver", "Toggle on");

    is(
      getActiveConfiguration().threads.join(","),
      "GeckoMain,Renderer,DOM Worker,DNS Resolver",
      "Another thread was added"
    );
    is(
      threadTextEl.value,
      "GeckoMain,Renderer,DOM Worker,DNS Resolver",
      "Another thread was in the thread text input"
    );

    const styleThreadCheckbox = await getNearestInputFromText(
      document,
      "StyleThread"
    );
    ok(!styleThreadCheckbox.checked, "The style thread is not checked.");

    // Set the input box directly
    setReactFriendlyInputValue(
      threadTextEl,
      "GeckoMain,DOM Worker,DNS Resolver,StyleThread"
    );
    threadTextEl.dispatchEvent(new Event("blur", { bubbles: true }));

    ok(styleThreadCheckbox.checked, "The style thread is now checked.");
    is(
      getActiveConfiguration().threads.join(","),
      "GeckoMain,DOM Worker,DNS Resolver,StyleThread",
      "Another thread was added"
    );
    is(
      threadTextEl.value,
      "GeckoMain,DOM Worker,DNS Resolver,StyleThread",
      "Another thread was in the thread text input"
    );

    // The all threads checkbox has nested text elements, so it's not easy to select
    // by its label value. Select it by ID.
    const allThreadsCheckbox = document.querySelector(
      "#perf-settings-thread-checkbox-all-threads"
    );
    info(`Turning on "All Threads" by clicking it."`);
    allThreadsCheckbox.click();

    is(
      getActiveConfiguration().threads.join(","),
      "GeckoMain,DOM Worker,DNS Resolver,StyleThread,*",
      "Asterisk was added"
    );
    is(
      threadTextEl.value,
      "GeckoMain,DOM Worker,DNS Resolver,StyleThread,*",
      "Asterisk was in the thread text input"
    );

    // Remove the asterisk
    setReactFriendlyInputValue(
      threadTextEl,
      "GeckoMain,DOM Worker,DNS Resolver,StyleThread"
    );
    threadTextEl.dispatchEvent(new Event("blur", { bubbles: true }));

    ok(!allThreadsCheckbox.checked, "The all threads checkbox is not checked.");
  });
});

/**
 * @param {Document} document
 * @param {string} threadName
 * @param {string} action - This is the intent of the click.
 */
async function clickThreadCheckbox(document, threadName, action) {
  info(`${action} "${threadName}" by clicking it.`);
  const checkbox = await getNearestInputFromText(document, threadName);
  checkbox.click();
}
