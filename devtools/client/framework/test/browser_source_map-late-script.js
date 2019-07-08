/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that you can subscribe to notifications on a source before it has loaded.

"use strict";

const PAGE_URL = `${URL_ROOT}doc_empty-tab-01.html`;
const JS_URL = URL_ROOT + "code_bundle_late_script.js";

const ORIGINAL_URL = "webpack:///code_late_script.js";

const GENERATED_LINE = 107;
const ORIGINAL_LINE = 11;

add_task(async function() {
  // Start with the empty page, then navigate, so that we can properly
  // listen for new sources arriving.
  const toolbox = await openNewTabAndToolbox(PAGE_URL, "webconsole");
  const service = toolbox.sourceMapURLService;

  const scriptMapped = new Promise(resolve => {
    let count = 0;
    service.subscribe(JS_URL, GENERATED_LINE, undefined, (...args) => {
      if (count === 0) {
        resolve(args);
      }
      count += 1;
    });
  });

  // Inject JS script
  const sourceSeen = waitForSourceLoad(toolbox, JS_URL);
  await createScript(JS_URL);
  await sourceSeen;

  // Ensure that the URL service fired an event about the location loading.
  const [isSourceMapped, url, line] = await scriptMapped;
  is(isSourceMapped, true, "check is mapped");
  is(url, ORIGINAL_URL, "check mapped URL");
  is(line, ORIGINAL_LINE, "check mapped line number");

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
  finish();
});
