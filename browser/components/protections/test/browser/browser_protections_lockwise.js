/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);

const TEST_LOGIN1 = new nsLoginInfo(
  "https://example.com/",
  "https://example.com/",
  null,
  "user1",
  "pass1",
  "username",
  "password"
);

const TEST_LOGIN2 = new nsLoginInfo(
  "https://2.example.com/",
  "https://2.example.com/",
  null,
  "user2",
  "pass2",
  "username",
  "password"
);

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Check that the correct content is displayed for non-logged in users.");
  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    const noLoginsContent = content.document.querySelector(
      "#lockwise-body-content .no-logins"
    );
    const hasLoginsContent = content.document.querySelector(
      "#lockwise-body-content .has-logins"
    );

    ok(
      ContentTaskUtils.is_visible(noLoginsContent),
      "Content for user with no logins is shown."
    );
    ok(
      ContentTaskUtils.is_hidden(hasLoginsContent),
      "Content for user with logins is hidden."
    );
  });

  info("Add a login and check that content for a logged in user is displayed.");
  Services.logins.addLogin(TEST_LOGIN1);
  await reloadTab(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    const noLoginsContent = content.document.querySelector(
      "#lockwise-body-content .no-logins"
    );
    const hasLoginsContent = content.document.querySelector(
      "#lockwise-body-content .has-logins"
    );
    const numberOfLogins = hasLoginsContent.querySelector(
      ".number-of-logins.block"
    );

    ok(
      ContentTaskUtils.is_hidden(noLoginsContent),
      "Content for user with no logins is hidden."
    );
    ok(
      ContentTaskUtils.is_visible(hasLoginsContent),
      "Content for user with logins is shown."
    );
    is(numberOfLogins.textContent, 1, "One stored login should be displayed");
  });

  info(
    "Add another login and check the number of stored logins is updated after reload."
  );
  Services.logins.addLogin(TEST_LOGIN2);
  await reloadTab(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    const numberOfLogins = content.document.querySelector(
      "#lockwise-body-content .has-logins .number-of-logins.block"
    );

    is(numberOfLogins.textContent, 2, "Two stored logins should be displayed");
  });

  // remove logins
  Services.logins.removeLogin(TEST_LOGIN1);
  Services.logins.removeLogin(TEST_LOGIN2);
  await BrowserTestUtils.removeTab(tab);
});
