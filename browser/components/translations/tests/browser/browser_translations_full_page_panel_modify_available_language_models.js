/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test verifies that when language models are added or removed from Remote Settings,
 * then the list of available languages is immediately reflected in the FullPageTranslationsPanel's
 * dropdown menu lists upon the panel's next open.
 */
add_task(
  async function test_full_page_translations_panel_modify_available_language_models() {
    const { runInPage, remoteClients, cleanup } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      autoDownloadFromRemoteSettings: true,
    });

    const { fromMenuList, toMenuList } = FullPageTranslationsPanel.elements;

    await FullPageTranslationsTestUtils.assertTranslationsButton(
      { button: true },
      "The button is available."
    );

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await FullPageTranslationsTestUtils.openPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });
    ok(
      !fromMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel has no selection for Japanese in the from-menu-list."
    );
    ok(
      !toMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel has no selection for Japanese in the to-menu-list."
    );

    await FullPageTranslationsTestUtils.clickCancelButton();

    const recordsForEnJa = createRecordsForLanguagePair("en", "ja");
    const recordsForJaEn = createRecordsForLanguagePair("ja", "en");

    info("Publishing Japanese as a source language in Remote Settings.");
    await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
      recordsToCreate: recordsForJaEn,
      expectedCreatedRecordsCount: FILES_PER_LANGUAGE_PAIR,
    });

    await FullPageTranslationsTestUtils.openPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });
    ok(
      fromMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel now has an item for Japanese in the from-menu-list."
    );
    ok(
      !toMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel still has no selection for Japanese in the to-menu-list."
    );

    await FullPageTranslationsTestUtils.clickCancelButton();

    info("Removing Japanese as a source language from Remote Settings.");
    await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
      recordsToDelete: recordsForJaEn,
      expectedDeletedRecordsCount: FILES_PER_LANGUAGE_PAIR,
    });

    await FullPageTranslationsTestUtils.openPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });
    ok(
      !fromMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel no longer has an item for Japanese in the from-menu-list."
    );
    ok(
      !toMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel still has no selection for Japanese in the to-menu-list."
    );

    await FullPageTranslationsTestUtils.clickCancelButton();

    info("Publishing Japanese as a target language in Remote Settings.");
    await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
      recordsToCreate: recordsForEnJa,
      expectedCreatedRecordsCount: FILES_PER_LANGUAGE_PAIR,
    });

    await FullPageTranslationsTestUtils.openPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });
    ok(
      !fromMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel still has no selection for Japanese in the from-menu-list."
    );
    ok(
      toMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel now has an item for Japanese in the to-menu-list."
    );

    await FullPageTranslationsTestUtils.clickCancelButton();

    info("Republishing Japanese as a source language in Remote Settings.");
    await modifyRemoteSettingsRecords(remoteClients.translationModels.client, {
      recordsToCreate: recordsForJaEn,
      expectedCreatedRecordsCount: FILES_PER_LANGUAGE_PAIR,
    });

    await FullPageTranslationsTestUtils.openPanel({
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });
    ok(
      fromMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel now has an item for Japanese in the from-menu-list."
    );
    ok(
      toMenuList.querySelector('[value="ja"]'),
      "The FullPageTranslationsPanel still has an item for Japanese in the to-menu-list."
    );

    await FullPageTranslationsTestUtils.changeSelectedToLanguage("ja");
    await FullPageTranslationsTestUtils.clickTranslateButton();

    await FullPageTranslationsTestUtils.assertPageIsTranslated(
      "es",
      "ja",
      runInPage
    );

    await cleanup();
  }
);
