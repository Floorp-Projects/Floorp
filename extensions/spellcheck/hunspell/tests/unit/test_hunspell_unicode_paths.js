"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "spellCheck",
  "@mozilla.org/spellchecker/engine;1",
  "mozISpellCheckingEngine"
);

const nsFile = Components.Constructor(
  "@mozilla.org/file/local;1",
  "nsIFile",
  "initWithPath"
);

add_task(async function() {
  let prof = do_get_profile();

  let basePath = PathUtils.join(prof.path, "\u263a", "dictionaries");
  let baseDir = nsFile(basePath);
  await IOUtils.makeDirectory(basePath, { createAncestors: true });

  let dicPath = PathUtils.join(basePath, "dict.dic");
  let affPath = PathUtils.join(basePath, "dict.aff");

  const WORD = "Flehgragh";

  await IOUtils.writeUTF8(dicPath, `1\n${WORD}\n`);
  await IOUtils.writeUTF8(affPath, "");

  spellCheck.loadDictionariesFromDir(baseDir);
  spellCheck.dictionaries = ["dict"];

  ok(
    spellCheck.check(WORD),
    "Dictionary should have been loaded from a unicode path"
  );
});
