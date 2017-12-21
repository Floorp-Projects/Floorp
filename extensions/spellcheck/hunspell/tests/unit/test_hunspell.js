/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var Cc = Components.classes;
var Ci = Components.interfaces;

const tests = [
    ["affixes", "iso-8859-1"],
    ["condition", "iso-8859-1"],
    ["condition-utf", "UTF-8"],
    ["base", "iso-8859-1"],
    ["base-utf", "UTF-8"],
    ["allcaps", "iso-8859-1"],
    ["allcaps-utf", "UTF-8"],
    ["allcaps2", "iso-8859-1"],
    ["allcaps3", "iso-8859-1"],
    ["keepcase", "iso-8859-1"],
    ["i58202", "iso-8859-1"],
    ["map", "iso-8859-1"],
    ["rep", "iso-8859-1"],
    ["sug", "iso-8859-1"],
    ["sugutf", "UTF-8"],
    ["phone", "iso-8859-1"],
    ["flag", "iso-8859-1"],
    ["flaglong", "iso-8859-1"],
    ["flagnum", "iso-8859-1"],
    ["flagutf8", "UTF-8"],
    ["slash", "iso-8859-1"],
    ["forbiddenword", "iso-8859-1"],
    ["nosuggest", "iso-8859-1"],
    ["alias", "iso-8859-1"],
    ["alias2", "iso-8859-1"],
    ["alias3", "iso-8859-1"],
    ["breakdefault", "iso-8859-1"],
    ["break", "UTF-8"],
    ["needaffix", "iso-8859-1"],
    ["needaffix2", "iso-8859-1"],
    ["needaffix3", "iso-8859-1"],
    ["needaffix4", "iso-8859-1"],
    ["needaffix5", "iso-8859-1"],
    ["circumfix", "iso-8859-1"],
    ["fogemorpheme", "iso-8859-1"],
    ["onlyincompound", "iso-8859-1"],
    ["complexprefixes", "iso-8859-1"],
    ["complexprefixes2", "iso-8859-1"],
    ["complexprefixesutf", "UTF-8"],
    ["conditionalprefix", "iso-8859-1"],
    ["zeroaffix", "iso-8859-1"],
    ["utf8", "UTF-8"],
    ["utf8-bom", "UTF-8", {1: "todo"}],
    ["utf8-bom2", "UTF-8", {1: "todo"}],
    ["utf8-nonbmp", "UTF-8", {1: "todo", 2: "todo", 3: "todo", 4: "todo"}],
    ["compoundflag", "iso-8859-1"],
    ["compoundrule", "iso-8859-1"],
    ["compoundrule2", "iso-8859-1"],
    ["compoundrule3", "iso-8859-1"],
    ["compoundrule4", "iso-8859-1"],
    ["compoundrule5", "UTF-8"],
    ["compoundrule6", "iso-8859-1"],
    ["compoundrule7", "iso-8859-1"],
    ["compoundrule8", "iso-8859-1"],
    ["compoundaffix", "iso-8859-1"],
    ["compoundaffix2", "iso-8859-1"],
    ["compoundaffix3", "iso-8859-1"],
    ["checkcompounddup", "iso-8859-1"],
    ["checkcompoundtriple", "iso-8859-1"],
    ["simplifiedtriple", "iso-8859-1"],
    ["checkcompoundrep", "iso-8859-1"],
    ["checkcompoundcase2", "iso-8859-1"],
    ["checkcompoundcaseutf", "UTF-8"],
    ["checkcompoundpattern", "iso-8859-1"],
    ["checkcompoundpattern2", "iso-8859-1"],
    ["checkcompoundpattern3", "iso-8859-1"],
    ["checkcompoundpattern4", "iso-8859-1"],
    ["utfcompound", "UTF-8"],
    ["checksharps", "iso-8859-1"],
    ["checksharpsutf", "UTF-8"],
    ["germancompounding", "iso-8859-1"],
    ["germancompoundingold", "iso-8859-1"],
    ["i35725", "iso-8859-1"],
    ["i53643", "iso-8859-1"],
    ["i54633", "iso-8859-1"],
    ["i54980", "iso-8859-1", {1: "todo", 3: "todo"}],
    ["maputf", "UTF-8"],
    ["reputf", "UTF-8"],
    ["ignore", "iso-8859-1"],
    ["ignoreutf", "UTF-8",
     {1: "todo", 2: "todo", 3: "todo", 4: "todo", 5: "todo", 6: "todo",
      7: "todo", 8: "todo"}],
    ["1592880", "iso-8859-1"],
    ["1695964", "iso-8859-1"],
    ["1463589", "iso-8859-1"],
    ["1463589-utf", "UTF-8"],
    ["IJ", "iso-8859-1"],
    ["i68568", "iso-8859-1"],
    ["i68568utf", "UTF-8"],
    ["1706659", "iso-8859-1"],
    ["digits-in-words", "iso-8859-1"],
//    ["colons-in-words", "iso-8859-1"], Suggestion test only
    ["ngram-utf-fix", "UTF-8"],
    ["morph", "us-ascii",
     {11: "todo", 12: "todo", 13: "todo", 14: "todo", 15: "todo", 16: "todo",
      17: "todo", 18: "todo", 19: "todo", 20: "todo", 21: "todo", 22: "todo",
      23: "todo", 24: "todo", 25: "todo", 26: "todo", 27: "todo"}],
    ["1975530", "UTF-8"],
    ["fullstrip", "iso-8859-1"],
    ["iconv", "UTF-8"],
    ["oconv", "UTF-8"],
    ["encoding", "iso-8859-1", {1: "todo", 3: "todo"}],
    ["korean", "UTF-8"],
    ["opentaal-forbiddenword1", "UTF-8"],
    ["opentaal-forbiddenword2", "UTF-8"],
    ["opentaal-keepcase", "UTF-8"],
    ["arabic", "UTF-8"],
    ["2970240", "iso-8859-1"],
    ["2970242", "iso-8859-1"],
    ["breakoff", "iso-8859-1"],
    ["opentaal-cpdpat", "iso-8859-1"],
    ["opentaal-cpdpat2", "iso-8859-1"],
    ["2999225", "iso-8859-1"],
    ["onlyincompound2", "iso-8859-1"],
    ["forceucase", "iso-8859-1"],
    ["warn", "iso-8859-1"]
];

function* do_get_file_by_line(file, charset) {
  dump("getting file by line for file " + file.path + "\n");
  dump("using charset " + charset +"\n");
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].
              createInstance(Ci.nsIFileInputStream);
  fis.init(file, 0x1 /* READONLY */,
           0o444, Ci.nsIFileInputStream.CLOSE_ON_EOF);

  let lis = Cc["@mozilla.org/intl/converter-input-stream;1"].
              createInstance(Ci.nsIConverterInputStream);
  lis.init(fis, charset, 1024, 0);
  lis.QueryInterface(Ci.nsIUnicharLineInputStream);

  var val = {};
  while (lis.readLine(val)) {
    yield val.value;
    val = {};
  }
}

function do_run_test(checker, name, charset, todo_good, todo_bad) {
  dump("\n\n\n\n");
  dump("running test for " + name + "\n");
  if (!checker) {
    do_throw("Need spell checker here!");
  }

  let good = do_get_file("data/" + name + ".good", true);
  let bad = do_get_file("data/" + name + ".wrong", true);
  let sug = do_get_file("data/" + name + ".sug", true);

  dump("Need some expected output\n")
  Assert.ok(good.exists() || bad.exists() || sug.exists());

  dump("Setting dictionary to " + name + "\n");
  checker.dictionary = name;

  if (good.exists()) {
    var good_counter = 0;
    for (val of do_get_file_by_line(good, charset)) {
      let todo = false;
      good_counter++;
      if (todo_good && todo_good[good_counter]) {
        todo = true;
        dump("TODO\n");
      }

      dump("Expect word " + val + " is spelled correctly\n");
      if (todo) {
        todo_check_true(checker.check(val));
      } else {
        Assert.ok(checker.check(val));
      }
    }
  }

  if (bad.exists()) {
    var bad_counter = 0;
    for (val of do_get_file_by_line(bad, charset)) {
      let todo = false;
      bad_counter++;
      if (todo_bad && todo_bad[bad_counter]) {
        todo = true;
        dump("TODO\n");
      }

      dump("Expect word " + val + " is spelled wrong\n");
      if (todo) {
        todo_check_false(checker.check(val));
      } else {
        Assert.ok(!checker.check(val));
      }
    }
  }

    // XXXkhuey test suggestions
}

function run_test() {
  let spellChecker = Cc["@mozilla.org/spellchecker/engine;1"].
                     getService(Ci.mozISpellCheckingEngine);

  Assert.ok(!!spellChecker, "Should have a spell checker");
  spellChecker.QueryInterface(Ci.mozISpellCheckingEngine);
  let testdir = do_get_file("data/", false);
  spellChecker.loadDictionariesFromDir(testdir);

  function do_run_test_closure(test) {
    [name, charset, todo_good, todo_bad] = test;
    do_run_test(spellChecker, name, charset, todo_good, todo_bad);
  }

  tests.forEach(do_run_test_closure);
}
