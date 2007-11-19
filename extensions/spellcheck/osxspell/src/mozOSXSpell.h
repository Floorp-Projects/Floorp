/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is Mike Pinkerton.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mike Pinkerton <mikepinkerton@mac.com>
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
 * This spellchecker is based on the built-in spellchecker on Mac OS X. It
 * does not use any Hunspell technology or rely on their dictionaries. It's just
 * a thin wrapper around the Cocoa NSSpellChecker API.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozOSXSpell_h__
#define mozOSXSpell_h__

#include "mozISpellCheckingEngine.h"
#include "mozIPersonalDictionary.h"
#include "nsString.h"
#include "nsCOMPtr.h"

// Use the same contract ID as the Hunspell spellchecker so we get picked up
// instead on Mac OS X but we have our own CID. 
#define MOZ_OSXSPELL_CONTRACTID "@mozilla.org/spellchecker/engine;1"
#define MOZ_OSXSPELL_CID         \
{ /* BAABBAF4-71C3-47F4-A576-E75469E485E2 */  \
0xBAABBAF4, 0x71C3, 0x47F4,                    \
{ 0xA5, 0x76, 0xE7, 0x54, 0x69, 0xE4, 0x85, 0xE2} }

class mozOSXSpell : public mozISpellCheckingEngine
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISPELLCHECKINGENGINE

  mozOSXSpell();

private:
 
  ~mozOSXSpell();

  // NSSpellChecker provides the ability to add words to the local dictionary,
  // but it's much easier to let the rest of Gecko handle that via the personal
  // dictionary given to us and just be ignorant about new words.
  nsCOMPtr<mozIPersonalDictionary> mPersonalDictionary;

  nsString  mLanguage;    // cached to speed up Check()
};

#endif
