/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Inline Spellchecker.
 *
 * The Initial Developer of the Original Code is
 * Mozdev Group, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Deakin (neil@mozdevgroup.com)
 *   Scott MacGregor (mscott@mozilla.org)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const kSpellMaxNumSuggestions = 7; // Maximum number of suggested words to fill into the context menu. Most
                                   // applications (like Open Office) set this value to 15.

const kSpellNoMispelling = -1;
const kSpellNoSuggestionsFound = 0;

var InlineSpellChecker =
{
  editor : null,
  inlineSpellChecker: null,

  Init : function (editor, enable)
  {
    this.editor = editor;
    this.inlineSpellChecker = editor.inlineSpellChecker;
    this.inlineSpellChecker.enableRealTimeSpell = enable;
  },

  checkDocument : function(doc)
  {
    if (!this.inlineSpellChecker || !this.inlineSpellChecker.enableRealTimeSpell) 
      return;

    var range = doc.createRange();
    range.selectNodeContents(doc.body);
    this.inlineSpellChecker.spellCheckRange(range);
  },

  getMispelledWord : function()
  {
    if (!this.inlineSpellChecker || !this.inlineSpellChecker.enableRealTimeSpell) 
      return null;

    var selection = this.editor.selection;
    return this.inlineSpellChecker.getMispelledWord(selection.anchorNode, selection.anchorOffset);
  },

  // returns kSpellNoMispelling if the word is spelled correctly
  // For mispelled words, returns kSpellNoSuggestionsFound when there are no suggestions otherwise the
  // number of suggestions is returned.
  // firstNonWordMenuItem is the first element in the menu popup that isn't a dynamically added word
  // added by updateSuggestionsMenu.
  updateSuggestionsMenu : function (menupopup, firstNonWordMenuItem, word)
  {
    if (!this.inlineSpellChecker || !this.inlineSpellChecker.enableRealTimeSpell) 
      return kSpellNoMispelling;

    var child = menupopup.firstChild;
    while (child != firstNonWordMenuItem) 
    {
      var next = child.nextSibling;
      menupopup.removeChild(child);
      child = next;
    }

    if (!word)
    {
      word = this.getMispelledWord();
      if (!word) 
        return kSpellNoMispelling;
    }

    var spellChecker = this.inlineSpellChecker.spellChecker;
    if (!spellChecker) 
      return kSpellNoMispelling;

    var numSuggestedWords = 0; 

    var isIncorrect = spellChecker.CheckCurrentWord(word.toString());
    if (isIncorrect)
    {
      do {
        var suggestion = spellChecker.GetSuggestedWord();
        if (!suggestion) 
          break;

        var item = document.createElement("menuitem");
        item.setAttribute("label", suggestion);
        item.setAttribute("value", suggestion);

        item.setAttribute("oncommand", "InlineSpellChecker.selectSuggestion(event.target.value, null, null);");
        menupopup.insertBefore(item, firstNonWordMenuItem);
        numSuggestedWords++;
      } while (numSuggestedWords < kSpellMaxNumSuggestions);
    }
    else 
      numSuggestedWords = kSpellNoMispelling;

    return numSuggestedWords;
  },

  selectSuggestion : function (newword, node, offset)
  {
    if (!this.inlineSpellChecker || !this.inlineSpellChecker.enableRealTimeSpell) 
      return;

    if (!node) 
    {
      var selection = this.editor.selection;
      node = selection.anchorNode;
      offset = selection.anchorOffset;
    }

    this.inlineSpellChecker.replaceWord(node, offset, newword);
  },

  addToDictionary : function (node, offset)
  {
    if (!this.inlineSpellChecker || !this.inlineSpellChecker.enableRealTimeSpell) 
      return;

    if (!node)
    {
      var selection = this.editor.selection;
      node = selection.anchorNode;
      offset = selection.anchorOffset;
    }

    var word = this.inlineSpellChecker.getMispelledWord(node,offset);
    if (word) this.inlineSpellChecker.addWordToDictionary(word);
  },

  ignoreWord : function (node, offset)
  {
    if (!this.inlineSpellChecker || !this.inlineSpellChecker.enableRealTimeSpell) 
      return;

    if (!node)
    {
      var selection = this.editor.selection;
      node = selection.anchorNode;
      offset = selection.anchorOffset;
    }

    var word = this.inlineSpellChecker.getMispelledWord(node, offset);
    if (word) 
      this.inlineSpellChecker.ignoreWord(word);
  }
}
