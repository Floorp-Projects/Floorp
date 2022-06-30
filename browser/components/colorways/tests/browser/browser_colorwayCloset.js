/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);
const { ColorwayClosetOpener } = ChromeUtils.import(
  "resource:///modules/ColorwayClosetOpener.jsm"
);

async function testInColorwayClosetModal(testMethod) {
  const { closedPromise, dialog } = ColorwayClosetOpener.openModal();
  await dialog._dialogReady;
  const document = dialog._frame.contentDocument;
  await document.l10n.ready;
  try {
    testMethod(document);
  } finally {
    document.getElementById("cancel").click();
    await closedPromise;
  }
}

add_task(async function setup_tests() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(BuiltInThemes, "findActiveColorwayCollection").returns({
    id: "independent-voices",
    expiry: new Date("3000-01-01"),
    l10nId: {
      title: "colorway-collection-independent-voices",
      description: "colorway-collection-independent-voices-description",
    },
  });
  sandbox.stub(BuiltInThemes, "isColorwayFromCurrentCollection").returns(true);
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_task(async function colorwaycloset_show_colorway() {
  await testInColorwayClosetModal(document => {
    is(
      document.l10n.getAttributes(document.getElementById("collection-title"))
        .id,
      "colorway-collection-independent-voices",
      "Correct collection title should be shown"
    );
    is(
      document.l10n.getAttributes(
        document.querySelector("#collection-expiry-date > span")
      ).args.expiryDate,
      new Date("3000-01-01").getTime(),
      "Correct expiry date should be shown"
    );
    is(
      document.l10n.getAttributes(
        document.querySelector("#collection-expiry-date > span")
      ).id,
      "colorway-collection-expiry-date-span",
      "Correct expiry date format should be shown"
    );
  });
});

add_task(async function colorwaycloset_custom_home_page() {
  info("Set custom home page");
  await HomePage.set("https://www.example.com");
  await testInColorwayClosetModal(document => {
    ok(
      BrowserTestUtils.is_visible(
        document.getElementById("use-fx-home-controls")
      ),
      '"Use Firefox home" controls should be shown'
    );
    ok(
      BrowserTestUtils.is_hidden(
        document.querySelector("#use-fx-home-controls > .success-prompt > span")
      ),
      "Success message should not be shown"
    );
    ok(
      BrowserTestUtils.is_hidden(
        document.querySelector(
          "#use-fx-home-controls > .success-prompt > button"
        )
      ),
      "Undo button should not be shown"
    );
    ok(
      BrowserTestUtils.is_visible(
        document.querySelector("#use-fx-home-controls > .reset-prompt > span")
      ),
      "Reset message should be shown"
    );
    ok(
      BrowserTestUtils.is_visible(
        document.querySelector("#use-fx-home-controls > .reset-prompt > button")
      ),
      "Apply button should be shown"
    );

    document
      .querySelector("#use-fx-home-controls > .reset-prompt > button")
      .click();

    ok(
      BrowserTestUtils.is_visible(
        document.querySelector("#use-fx-home-controls > .success-prompt > span")
      ),
      "Success message should be shown"
    );
    ok(
      BrowserTestUtils.is_visible(
        document.querySelector(
          "#use-fx-home-controls > .success-prompt > button"
        )
      ),
      "Undo button should be shown"
    );
    ok(
      BrowserTestUtils.is_hidden(
        document.querySelector("#use-fx-home-controls > .reset-prompt > span")
      ),
      "Reset message should not be shown"
    );
    ok(
      BrowserTestUtils.is_hidden(
        document.querySelector("#use-fx-home-controls > .reset-prompt > button")
      ),
      "Apply button should not be shown"
    );
  });
});

add_task(async function colorwaycloset_default_home_page() {
  info("Set default home page");
  await HomePage.set(HomePage.getOriginalDefault());
  await testInColorwayClosetModal(document => {
    ok(
      BrowserTestUtils.is_hidden(
        document.getElementById("use-fx-home-controls")
      ),
      '"Use Firefox home" controls should be hidden'
    );
  });
});
