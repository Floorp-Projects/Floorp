/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "<h1 id=\"h1\">header</h1><p id=\"p\">paragraph</p>";

add_task(async function tabNotHighlighted() {
  await addTab(buildURL(TEST_URI));
  const { toolbox } = await openInspector();
  const isHighlighted = await toolbox.isToolHighlighted("accessibility");

  ok(!isHighlighted, "When accessibility service is not running, accessibility panel " +
                     "should not be highlighted when toolbox opens");

  gBrowser.removeCurrentTab();
});

add_task(async function tabHighlighted() {
  let a11yService = await initA11y();
  ok(a11yService, "Accessibility service was started");
  await addTab(buildURL(TEST_URI));
  const { toolbox } = await openInspector();
  const isHighlighted = await toolbox.isToolHighlighted("accessibility");

  ok(isHighlighted, "When accessibility service is running, accessibility panel should" +
                    "be highlighted when toolbox opens");

  a11yService = null;
  gBrowser.removeCurrentTab();
});
