"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");

Cu.importGlobalProperties(["TextEncoder"]);

XPCOMUtils.defineLazyServiceGetter(this, "spellCheck",
                                   "@mozilla.org/spellchecker/engine;1", "mozISpellCheckingEngine");

const nsFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                      "initWithPath");

add_task(async function() {
  let prof = do_get_profile();

  let basePath = OS.Path.join(prof.path, "\u263a", "dictionaries");
  let baseDir = nsFile(basePath);
  await OS.File.makeDir(basePath, {from: prof.path});

  let dicPath = OS.Path.join(basePath, "dict.dic");
  let affPath = OS.Path.join(basePath, "dict.aff");

  const WORD = "Flehgragh";

  await OS.File.writeAtomic(dicPath, new TextEncoder().encode(`1\n${WORD}\n`));
  await OS.File.writeAtomic(affPath, new TextEncoder().encode(""));

  spellCheck.loadDictionariesFromDir(baseDir);
  spellCheck.dictionary = "dict";

  ok(spellCheck.check(WORD), "Dictionary should have been loaded from a unicode path");
});

