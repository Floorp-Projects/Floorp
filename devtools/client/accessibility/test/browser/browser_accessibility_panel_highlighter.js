/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = '<h1 id="h1">header</h1><p id="p">paragraph</p>';

add_task(async function tabNotHighlighted() {
  Services.prefs.setBoolPref("devtools.accessibility.auto-init.enabled", false);
  await addTab(buildURL(TEST_URI));
  const { toolbox } = await openInspector();
  const isHighlighted = await toolbox.isToolHighlighted("accessibility");

  ok(
    !isHighlighted,
    "When accessibility service is not running, accessibility panel " +
      "should not be highlighted when toolbox opens"
  );

  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref("devtools.accessibility.auto-init.enabled");
});

add_task(async function tabHighlighted() {
  Services.prefs.setBoolPref("devtools.accessibility.auto-init.enabled", false);
  let a11yService = await initA11y();
  ok(a11yService, "Accessibility service was started");
  await addTab(buildURL(TEST_URI));
  const { toolbox } = await openInspector();
  await checkHighlighted(toolbox, true);

  a11yService = null;
  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref("devtools.accessibility.auto-init.enabled");
});
