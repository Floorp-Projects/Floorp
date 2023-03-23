/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the behavior of the tab and the urlbar on new window opened by clicking
// link.

/* import-globals-from common_link_in_tab_title_and_url_prefilled.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/base/content/test/tabs/common_link_in_tab_title_and_url_prefilled.js",
  this
);

add_task(async function normal_page__blank_target() {
  await doTestWithNewWindow({
    link: "wait-a-bit--blank-target",
    expectedSetURICalled: true,
  });
});

add_task(async function normal_page__other_target() {
  await doTestWithNewWindow({
    link: "wait-a-bit--other-target",
    expectedSetURICalled: false,
  });
});

add_task(async function normal_page__by_script() {
  await doTestWithNewWindow({
    link: "wait-a-bit--by-script",
    expectedSetURICalled: false,
  });
});

add_task(async function blank_page__blank_target() {
  await doTestWithNewWindow({
    link: "blank-page--blank-target",
    expectedSetURICalled: false,
  });
});

add_task(async function blank_page__other_target() {
  await doTestWithNewWindow({
    link: "blank-page--other-target",
    expectedSetURICalled: false,
  });
});

add_task(async function blank_page__by_script() {
  await doTestWithNewWindow({
    link: "blank-page--by-script",
    expectedSetURICalled: false,
  });
});
