/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const frenchModels = [
  "lex.50.50.enfr.s2t.bin",
  "lex.50.50.fren.s2t.bin",
  "model.enfr.intgemm.alphas.bin",
  "model.fren.intgemm.alphas.bin",
  "vocab.enfr.spm",
  "vocab.fren.spm",
];

add_task(async function test_about_preferences_manage_languages() {
  const {
    cleanup,
    remoteClients,
    elements: {
      downloadAllLabel,
      downloadAll,
      deleteAll,
      frenchLabel,
      frenchDownload,
      frenchDelete,
      spanishLabel,
      spanishDownload,
      spanishDelete,
    },
  } = await setupAboutPreferences([
    { fromLang: "en", toLang: "fr" },
    { fromLang: "fr", toLang: "en" },
    { fromLang: "en", toLang: "es" },
    { fromLang: "es", toLang: "en" },
  ]);

  is(
    downloadAllLabel.getAttribute("data-l10n-id"),
    "translations-manage-install-description",
    "The first row is all of the languages."
  );
  is(frenchLabel.textContent, "French", "There is a French row.");
  is(spanishLabel.textContent, "Spanish", "There is a Spanish row.");

  await assertVisibility({
    message: "Everything starts out as available to download",
    visible: { downloadAll, frenchDownload, spanishDownload },
    hidden: { deleteAll, frenchDelete, spanishDelete },
  });

  click(frenchDownload, "Downloading French");

  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      frenchModels.length
    ),
    frenchModels,
    "French models were downloaded."
  );

  await assertVisibility({
    message: "French can now be deleted, and delete all is available.",
    visible: { downloadAll, deleteAll, frenchDelete, spanishDownload },
    hidden: { frenchDownload, spanishDelete },
  });

  click(frenchDelete, "Deleting French");

  await assertVisibility({
    message: "Everything can be downloaded.",
    visible: { downloadAll, frenchDownload, spanishDownload },
    hidden: { deleteAll, frenchDelete, spanishDelete },
  });

  click(downloadAll, "Downloading all languages.");

  const allModels = [
    "lex.50.50.enes.s2t.bin",
    "lex.50.50.enfr.s2t.bin",
    "lex.50.50.esen.s2t.bin",
    "lex.50.50.fren.s2t.bin",
    "model.enes.intgemm.alphas.bin",
    "model.enfr.intgemm.alphas.bin",
    "model.esen.intgemm.alphas.bin",
    "model.fren.intgemm.alphas.bin",
    "vocab.enes.spm",
    "vocab.enfr.spm",
    "vocab.esen.spm",
    "vocab.fren.spm",
  ];
  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      allModels.length
    ),
    allModels,
    "All models were downloaded."
  );
  Assert.deepEqual(
    await remoteClients.languageIdModels.resolvePendingDownloads(1),
    ["lid.176.ftz"],
    "Language ID model was downloaded."
  );
  Assert.deepEqual(
    await remoteClients.translationsWasm.resolvePendingDownloads(2),
    ["bergamot-translator", "fasttext-wasm"],
    "Wasm was downloaded."
  );

  await assertVisibility({
    message: "Everything can be deleted.",
    visible: { deleteAll, frenchDelete, spanishDelete },
    hidden: { downloadAll, frenchDownload, spanishDownload },
  });

  click(deleteAll, "Deleting all languages.");

  await assertVisibility({
    message: "Everything can be downloaded again",
    visible: { downloadAll, frenchDownload, spanishDownload },
    hidden: { deleteAll, frenchDelete, spanishDelete },
  });

  click(frenchDownload, "Downloading French.");
  click(spanishDownload, "Downloading Spanish.");

  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      allModels.length
    ),
    allModels,
    "All models were downloaded again."
  );

  remoteClients.translationsWasm.assertNoNewDownloads();
  remoteClients.languageIdModels.assertNoNewDownloads();

  await assertVisibility({
    message: "Everything is downloaded again.",
    visible: { deleteAll, frenchDelete, spanishDelete },
    hidden: { downloadAll, frenchDownload, spanishDownload },
  });

  return cleanup();
});

add_task(async function test_about_preferences_download_reject() {
  const {
    cleanup,
    remoteClients,
    elements: { document, frenchDownload },
  } = await setupAboutPreferences([
    { fromLang: "en", toLang: "fr" },
    { fromLang: "fr", toLang: "en" },
    { fromLang: "en", toLang: "es" },
    { fromLang: "es", toLang: "en" },
  ]);

  click(frenchDownload, "Downloading French");

  is(
    maybeGetByL10nId("translations-manage-error-install", document),
    null,
    "No error messages are present."
  );

  const errors = await captureTranslationsError(() =>
    remoteClients.translationModels.rejectPendingDownloads(frenchModels.length)
  );

  ok(
    !!errors.length,
    `The errors for download should have been reported, found ${errors.length} errors`
  );
  for (const { error } of errors) {
    is(
      error?.message,
      "Failed to download file.",
      "The error reported was a download error."
    );
  }

  await waitForCondition(
    () => maybeGetByL10nId("translations-manage-error-install", document),
    "The error message is now visible."
  );

  click(frenchDownload, "Attempting to download French again", document);
  is(
    maybeGetByL10nId("translations-manage-error-install", document),
    null,
    "The error message is hidden again."
  );

  return cleanup();
});
