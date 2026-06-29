<!-- SPDX-License-Identifier: MPL-2.0 -->

# Downloaded Firefox Browser Tests

This directory keeps the small repo-side metadata needed to run selected raw
Firefox browser-chrome tests from a downloaded `Floorp-Runtime` collection.

Raw upstream files are not committed here. CI collects them into
`_dist/firefox-tests/files/`, then
`tools/firefox-tests/prepare_firefox_browser_tests.ts` generates ignored
`generated/*.test.js` wrappers that import those raw files through the
`#firefox-tests/` Vite alias.

Add only tests that pass in the Floorp runner without editing the upstream raw
file. If a test needs Floorp-specific changes, keep it under
`../firefox-imported/` instead.
