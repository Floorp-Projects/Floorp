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
      ukrainianLabel,
      ukrainianDownload,
      ukrainianDelete,
    },
  } = await setupAboutPreferences(LANGUAGE_PAIRS);

  is(
    downloadAllLabel.getAttribute("data-l10n-id"),
    "translations-manage-install-description",
    "The first row is all of the languages."
  );
  is(frenchLabel.textContent, "French", "There is a French row.");
  is(spanishLabel.textContent, "Spanish", "There is a Spanish row.");
  is(ukrainianLabel.textContent, "Ukrainian", "There is a Ukrainian row.");

  await assertVisibility({
    message: "Everything starts out as available to download",
    visible: {
      downloadAll,
      frenchDownload,
      spanishDownload,
      ukrainianDownload,
    },
    hidden: { deleteAll, frenchDelete, spanishDelete, ukrainianDelete },
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
    visible: {
      downloadAll,
      deleteAll,
      frenchDelete,
      spanishDownload,
      ukrainianDownload,
    },
    hidden: { frenchDownload, spanishDelete, ukrainianDelete },
  });

  click(frenchDelete, "Deleting French");

  await assertVisibility({
    message: "Everything can be downloaded.",
    visible: {
      downloadAll,
      frenchDownload,
      spanishDownload,
      ukrainianDownload,
    },
    hidden: { deleteAll, frenchDelete, spanishDelete, ukrainianDelete },
  });

  click(downloadAll, "Downloading all languages.");

  const allModels = [
    "lex.50.50.enes.s2t.bin",
    "lex.50.50.enfr.s2t.bin",
    "lex.50.50.enuk.s2t.bin",
    "lex.50.50.esen.s2t.bin",
    "lex.50.50.fren.s2t.bin",
    "lex.50.50.uken.s2t.bin",
    "model.enes.intgemm.alphas.bin",
    "model.enfr.intgemm.alphas.bin",
    "model.enuk.intgemm.alphas.bin",
    "model.esen.intgemm.alphas.bin",
    "model.fren.intgemm.alphas.bin",
    "model.uken.intgemm.alphas.bin",
    "vocab.enes.spm",
    "vocab.enfr.spm",
    "vocab.enuk.spm",
    "vocab.esen.spm",
    "vocab.fren.spm",
    "vocab.uken.spm",
  ];
  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      allModels.length
    ),
    allModels,
    "All models were downloaded."
  );
  Assert.deepEqual(
    await remoteClients.translationsWasm.resolvePendingDownloads(1),
    ["bergamot-translator"],
    "Wasm was downloaded."
  );

  await assertVisibility({
    message: "Everything can be deleted.",
    visible: { deleteAll, frenchDelete, spanishDelete, ukrainianDelete },
    hidden: { downloadAll, frenchDownload, spanishDownload, ukrainianDownload },
  });

  click(deleteAll, "Deleting all languages.");

  await assertVisibility({
    message: "Everything can be downloaded again",
    visible: {
      downloadAll,
      frenchDownload,
      spanishDownload,
      ukrainianDownload,
    },
    hidden: { deleteAll, frenchDelete, spanishDelete, ukrainianDelete },
  });

  click(frenchDownload, "Downloading French.");
  click(spanishDownload, "Downloading Spanish.");
  click(ukrainianDownload, "Downloading Ukrainian.");

  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      allModels.length
    ),
    allModels,
    "All models were downloaded again."
  );

  remoteClients.translationsWasm.assertNoNewDownloads();

  await assertVisibility({
    message: "Everything is downloaded again.",
    visible: { deleteAll, frenchDelete, spanishDelete, ukrainianDelete },
    hidden: { downloadAll, frenchDownload, spanishDownload, ukrainianDownload },
  });

  await cleanup();
});

add_task(async function test_about_preferences_download_reject() {
  const {
    cleanup,
    remoteClients,
    elements: { document, frenchDownload },
  } = await setupAboutPreferences(LANGUAGE_PAIRS);

  click(frenchDownload, "Downloading French");

  is(
    maybeGetByL10nId("translations-manage-error-install", document),
    null,
    "No error messages are present."
  );

  const failureErrors = await captureTranslationsError(() =>
    remoteClients.translationModels.rejectPendingDownloads(frenchModels.length)
  );

  ok(
    !!failureErrors.length,
    `The errors for download should have been reported, found ${failureErrors.length} errors`
  );
  for (const { error } of failureErrors) {
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

  const successErrors = await captureTranslationsError(() =>
    remoteClients.translationModels.resolvePendingDownloads(frenchModels.length)
  );

  is(
    successErrors.length,
    0,
    "Expected no errors downloading French the second time"
  );

  await cleanup();
});
