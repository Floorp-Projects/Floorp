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
 * The Original Code is Mozilla Communicator client code..
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Kin Blas <kin@netscape.com>
 *      Akkana Peck <akkana@netscape.com>
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

#ifndef nsEditorSpellCheck_h___
#define nsEditorSpellCheck_h___


#include "nsIEditorSpellCheck.h"
#include "nsISpellChecker.h"
#include "nsIURI.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"

#define NS_EDITORSPELLCHECK_CID                     \
{ /* {75656ad9-bd13-4c5d-939a-ec6351eea0cc} */        \
    0x75656ad9, 0xbd13, 0x4c5d,                       \
    { 0x93, 0x9a, 0xec, 0x63, 0x51, 0xee, 0xa0, 0xcc }\
}

class LastDictionary;

class nsEditorSpellCheck : public nsIEditorSpellCheck
{
public:
  nsEditorSpellCheck();
  virtual ~nsEditorSpellCheck();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsEditorSpellCheck)

  /* Declare all methods in the nsIEditorSpellCheck interface */
  NS_DECL_NSIEDITORSPELLCHECK

  static LastDictionary* gDictionaryStore;

  static void ShutDown();

protected:
  nsCOMPtr<nsISpellChecker> mSpellChecker;

  nsTArray<nsString>  mSuggestedWordList;
  PRInt32        mSuggestedWordIndex;

  // these are the words in the current personal dictionary,
  // GetPersonalDictionary must be called to load them.
  nsTArray<nsString>  mDictionaryList;
  PRInt32        mDictionaryIndex;

  nsresult       DeleteSuggestedWordList();

  nsCOMPtr<nsITextServicesFilter> mTxtSrvFilter;
  nsCOMPtr<nsIEditor> mEditor;

  nsString mPreferredLang;

  bool mUpdateDictionaryRunning;

public:
  void BeginUpdateDictionary() { mUpdateDictionaryRunning = PR_TRUE ;}
  void EndUpdateDictionary() { mUpdateDictionaryRunning = PR_FALSE ;}
};

#endif // nsEditorSpellCheck_h___


