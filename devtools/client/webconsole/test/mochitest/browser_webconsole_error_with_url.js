/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check if an error with Unicode characters is reported correctly.

"use strict";

const longParam = "0".repeat(200);
const url1 = `https://example.com?v=${longParam}`;
const url2 = `https://example.org?v=${longParam}`;

const TEST_URI = `data:text/html;charset=utf8,<script>
  throw "Visit \u201c${url1}\u201d or \u201c${url2}\u201d to get more " +
        "information on this error.";
</script>`;
const {ELLIPSIS} = require("devtools/shared/l10n");

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  const getCroppedUrl = origin => {
    const cropLimit = 120;
    const half = cropLimit / 2;
    const params =
      `?v=${"0".repeat(half - origin.length - 3)}${ELLIPSIS}${"0".repeat(half)}`;
    return `${origin}${params}`;
  };

  const EXPECTED_MESSAGE = `get more information on this error`;

  const msg = await waitFor(() => findMessage(hud, EXPECTED_MESSAGE));
  ok(msg, `Link in error message are cropped as expected`);

  const [comLink, orgLink] = Array.from(msg.querySelectorAll("a"));
  is(comLink.getAttribute("href"), url1, "First link has expected url");
  is(comLink.getAttribute("title"), url1, "First link has expected tooltip");
  is(comLink.textContent, getCroppedUrl("https://example.com"),
    "First link has expected text");

  is(orgLink.getAttribute("href"), url2, "Second link has expected url");
  is(orgLink.getAttribute("title"), url2, "Second link has expected tooltip");
  is(orgLink.textContent, getCroppedUrl("https://example.org"),
    "Second link has expected text");
});
