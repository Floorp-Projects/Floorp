/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that you can subscribe to notifications on a source before it has loaded.

"use strict";

const PAGE_URL = `${URL_ROOT_SSL}doc_empty-tab-01.html`;
const JS_URL = URL_ROOT_SSL + "code_bundle_late_script.js";

const ORIGINAL_URL = "webpack:///code_late_script.js";

const GENERATED_LINE = 107;
const ORIGINAL_LINE = 11;

add_task(async function () {
  // Start with the empty page, then navigate, so that we can properly
  // listen for new sources arriving.
  const toolbox = await openNewTabAndToolbox(PAGE_URL, "webconsole");
  const service = toolbox.sourceMapURLService;

  const scriptMapped = new Promise(resolve => {
    let count = 0;
    service.subscribeByURL(
      JS_URL,
      GENERATED_LINE,
      undefined,
      originalLocation => {
        if (count === 0) {
          resolve(originalLocation);
        }
        count += 1;

        return () => {};
      }
    );
  });

  // Inject JS script
  const sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  await createScript(JS_URL);
  await sourceSeen;

  // Ensure that the URL service fired an event about the location loading.
  const { url, line } = await scriptMapped;
  is(url, ORIGINAL_URL, "check mapped URL");
  is(line, ORIGINAL_LINE, "check mapped line number");

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
  finish();
});
