function isSpellingCheckOk(aEditor, aMisspelledWords) {
  var selcon = aEditor.selectionController;
  var sel = selcon.getSelection(selcon.SELECTION_SPELLCHECK);
  var numWords = sel.rangeCount;

  is(numWords, aMisspelledWords.length, "Correct number of misspellings and words.");

  if (numWords != aMisspelledWords.length) {
    return false;
  }

  for (var i = 0; i < numWords; ++i) {
    var word = sel.getRangeAt(i);
    is(word, aMisspelledWords[i], "Misspelling is what we think it is.");
    if (word != aMisspelledWords[i]) {
      return false;
    }
  }
  return true;
}
