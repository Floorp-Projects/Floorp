/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteL10n } = ChromeUtils.import(
  "resource://activity-stream/lib/RemoteL10n.jsm"
);

add_task(async function test_TODO() {
  const CONTENT = "remote_l10n_test_string = RemoteL10n string";
  const dirPath = OS.Path.join(
    OS.Constants.Path.localProfileDir,
    ...["settings", "main", "ms-language-packs", "browser", "newtab"]
  );
  const filePath = OS.Path.join(dirPath, "asrouter.ftl");

  await IOUtils.makeDirectory(dirPath, {
    ignoreExisting: true,
    from: OS.Constants.Path.localProfileDir,
  });
  await IOUtils.writeUTF8(filePath, CONTENT, {
    tmpPath: `${filePath}.tmp`,
  });

  RemoteL10n.reloadL10n();

  let [{ value }] = await RemoteL10n.l10n.formatMessages([
    { id: "remote_l10n_test_string" },
  ]);

  Assert.equal(
    value,
    "RemoteL10n string",
    "Got back the string we wrote to disk"
  );
});
