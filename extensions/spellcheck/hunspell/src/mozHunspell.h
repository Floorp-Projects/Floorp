/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developers of the Original Code are Kevin Hendricks (MySpell)
 * and László Németh (Hunspell). Portions created by the Initial Developers
 * are Copyright (C) 2002-2005 the Initial Developers. All Rights Reserved.
 * 
 * Contributor(s): Kevin Hendricks (kevin.hendricks@sympatico.ca)
 *                 David Einstein (deinst@world.std.com)
 *                 Michiel van Leeuwen (mvl@exedo.nl)
 *                 Caolan McNamara (cmc@openoffice.org)
 *                 László Németh (nemethl@gyorsposta.hu)
 *                 Davide Prina
 *                 Giuseppe Modugno
 *                 Gianluca Turconi
 *                 Simon Brouwer
 *                 Noll Janos
 *                 Biro Arpad
 *                 Goldman Eleonora
 *                 Sarlos Tamas
 *                 Bencsath Boldizsar
 *                 Halacsy Peter
 *                 Dvornik Laszlo
 *                 Gefferth Andras
 *                 Nagy Viktor
 *                 Varga Daniel
 *                 Chris Halls
 *                 Rene Engelhard
 *                 Bram Moolenaar
 *                 Dafydd Jones
 *                 Harri Pitkanen
 *                 Andras Timar
 *                 Tor Lillqvist
 *                 Jesper Kristensen (mail@jesperkristensen.dk)
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
 ******* END LICENSE BLOCK *******/

#ifndef mozHunspell_h__
#define mozHunspell_h__

#include <hunspell.hxx>
#include "mozISpellCheckingEngine.h"
#include "mozIPersonalDictionary.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsInterfaceHashtable.h"
#include "nsWeakReference.h"
#include "nsCycleCollectionParticipant.h"
#include "mozHunspellAllocator.h"

#define MOZ_HUNSPELL_CONTRACTID "@mozilla.org/spellchecker/engine;1"
#define MOZ_HUNSPELL_CID         \
/* 56c778e4-1bee-45f3-a689-886692a97fe7 */   \
{ 0x56c778e4, 0x1bee, 0x45f3, \
  { 0xa6, 0x89, 0x88, 0x66, 0x92, 0xa9, 0x7f, 0xe7 } }

class mozHunspell : public mozISpellCheckingEngine,
                    public nsIObserver,
                    public nsSupportsWeakReference,
                    public nsIMemoryReporter
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_MOZISPELLCHECKINGENGINE
  NS_DECL_NSIOBSERVER
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(mozHunspell, mozISpellCheckingEngine)

  mozHunspell();

  nsresult Init();

  void LoadDictionaryList(bool aNotifyChildProcesses);

  // helper method for converting a word to the charset of the dictionary
  nsresult ConvertCharset(const char16_t* aStr, char ** aDst);

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize)
  {
    return MOZ_COLLECT_REPORT(
      "explicit/spell-check", KIND_HEAP, UNITS_BYTES, HunspellAllocator::MemoryAllocated(),
      "Memory used by the spell-checking engine.");
  }

protected:
  virtual ~mozHunspell();

  nsCOMPtr<mozIPersonalDictionary> mPersonalDictionary;
  nsCOMPtr<nsIUnicodeEncoder>      mEncoder;
  nsCOMPtr<nsIUnicodeDecoder>      mDecoder;

  // Hashtable matches dictionary name to .aff file
  nsInterfaceHashtable<nsStringHashKey, nsIFile> mDictionaries;
  nsString  mDictionary;
  nsString  mLanguage;
  nsCString mAffixFileName;

  // dynamic dirs used to search for dictionaries
  nsCOMArray<nsIFile> mDynamicDirectories;

  Hunspell  *mHunspell;
};

#endif
