/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteL10n } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/RemoteL10n.sys.mjs"
);

const ID = "remote_l10n_test_string";
const VALUE = "RemoteL10n string";
const CONTENT = `${ID} = ${VALUE}`;

add_setup(async () => {
  const l10nRegistryInstance = L10nRegistry.getInstance();
  const localProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
  const dirPath = PathUtils.join(
    localProfileDir,
    ...["settings", "main", "ms-language-packs", "browser", "newtab"]
  );
  const filePath = PathUtils.join(dirPath, "asrouter.ftl");

  await IOUtils.makeDirectory(dirPath, {
    ignoreExisting: true,
    from: localProfileDir,
  });
  await IOUtils.writeUTF8(filePath, CONTENT, {
    tmpPath: `${filePath}.tmp`,
  });

  // Remove any cached l10n resources, "cfr" is the cache key
  // used for strings from the remote `asrouter.ftl` see RemoteL10n.sys.mjs
  RemoteL10n.reloadL10n();
  if (l10nRegistryInstance.hasSource("cfr")) {
    l10nRegistryInstance.removeSources(["cfr"]);
  }
});

add_task(async function test_TODO() {
  let [{ value }] = await RemoteL10n.l10n.formatMessages([{ id: ID }]);

  Assert.equal(value, VALUE, "Got back the string we wrote to disk");
});

// Test that the formatting helper works.  This helper is lower-level than the
// DOM localization apparatus, and as such doesn't require the weight of the
// `browser` test framework, but it's nice to co-locate related tests.
add_task(async function test_formatLocalizableText() {
  let value = await RemoteL10n.formatLocalizableText({ string_id: ID });

  Assert.equal(value, VALUE, "Got back the string we wrote to disk");

  value = await RemoteL10n.formatLocalizableText("unchanged");

  Assert.equal(value, "unchanged", "Got back the string provided");
});
