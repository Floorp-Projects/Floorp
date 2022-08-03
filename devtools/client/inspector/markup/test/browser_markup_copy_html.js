/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the copy inner and outer html menu options.

// The nicely formatted HTML code.
const FORMATTED_HTML = `<body>
  <style>
    div {
      color: red;
    }

    span {
      text-decoration: underline;
    }
  </style>
  <div><span><em>Hello</em></span></div>
  <script>
    console.log("Hello!");
  </script>
</body>`;

// The inner HTML of the body node from the code above.
const FORMATTED_INNER_HTML = FORMATTED_HTML.replace(/<\/*body>/g, "")
  .trim()
  .replace(/^ {2}/gm, "");

// The formatted outer HTML, using tabs rather than spaces.
const TABS_FORMATTED_HTML = FORMATTED_HTML.replace(/[ ]{2}/g, "\t");

// The formatted outer HTML, using 3 spaces instead of 2.
const THREE_SPACES_FORMATTED_HTML = FORMATTED_HTML.replace(/[ ]{2}/g, "   ");

// Uglify the formatted code by removing all spaces and line breaks.
const UGLY_HTML = FORMATTED_HTML.replace(/[\r\n\s]+/g, "");

// And here is the inner html of the body node from the ugly code above.
const UGLY_INNER_HTML = UGLY_HTML.replace(/<\/*body>/g, "");

add_task(async function() {
  // Load the ugly code in a new tab and open the inspector.
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(UGLY_HTML)
  );

  info("Get the inner and outer html copy menu items");
  const allMenuItems = openContextMenuAndGetAllItems(inspector);
  const outerHtmlMenu = allMenuItems.find(
    ({ id }) => id === "node-menu-copyouter"
  );
  const innerHtmlMenu = allMenuItems.find(
    ({ id }) => id === "node-menu-copyinner"
  );

  info("Try to copy the outer html");
  await waitForClipboardPromise(() => outerHtmlMenu.click(), UGLY_HTML);

  info("Try to copy the inner html");
  await waitForClipboardPromise(() => innerHtmlMenu.click(), UGLY_INNER_HTML);

  info("Set the pref for beautifying html on copy");
  await pushPref("devtools.markup.beautifyOnCopy", true);

  info("Try to copy the beautified outer html");
  await waitForClipboardPromise(() => outerHtmlMenu.click(), FORMATTED_HTML);

  info("Try to copy the beautified inner html");
  await waitForClipboardPromise(
    () => innerHtmlMenu.click(),
    FORMATTED_INNER_HTML
  );

  info("Set the pref to stop expanding tabs into spaces");
  await pushPref("devtools.editor.expandtab", false);

  info("Check that the beautified outer html uses tabs");
  await waitForClipboardPromise(
    () => outerHtmlMenu.click(),
    TABS_FORMATTED_HTML
  );

  info("Set the pref to expand tabs to 3 spaces");
  await pushPref("devtools.editor.expandtab", true);
  await pushPref("devtools.editor.tabsize", 3);

  info("Try to copy the beautified outer html");
  await waitForClipboardPromise(
    () => outerHtmlMenu.click(),
    THREE_SPACES_FORMATTED_HTML
  );
});
