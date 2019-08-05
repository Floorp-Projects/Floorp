function isSpellingCheckOk(aEditor, aMisspelledWords, aTodo = false) {
  var selcon = aEditor.selectionController;
  var sel = selcon.getSelection(selcon.SELECTION_SPELLCHECK);
  var numWords = sel.rangeCount;

  if (aTodo) {
    todo_is(
      numWords,
      aMisspelledWords.length,
      "Correct number of misspellings and words."
    );
  } else {
    is(
      numWords,
      aMisspelledWords.length,
      "Correct number of misspellings and words."
    );
  }

  if (numWords !== aMisspelledWords.length) {
    return false;
  }

  for (var i = 0; i < numWords; ++i) {
    var word = String(sel.getRangeAt(i));
    if (aTodo) {
      todo_is(word, aMisspelledWords[i], "Misspelling is what we think it is.");
    } else {
      is(word, aMisspelledWords[i], "Misspelling is what we think it is.");
    }
    if (word !== aMisspelledWords[i]) {
      return false;
    }
  }
  return true;
}
