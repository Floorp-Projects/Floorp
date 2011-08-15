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
 *      Charley Manske <cmanske@netscape.com>
 *      Neil Deakin <neil@mozdevgroup.com>
 *      Brett Wilson <brettw@gmail.com>
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

#include "nsEditorSpellCheck.h"

#include "nsStyleUtil.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsITextServicesDocument.h"
#include "nsISpellChecker.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"

#include "nsIComponentManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIChromeRegistry.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsITextServicesFilter.h"
#include "nsUnicharUtils.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

class UpdateDictionnaryHolder {
  private:
    nsEditorSpellCheck* mSpellCheck;
  public:
    UpdateDictionnaryHolder(nsEditorSpellCheck* esc): mSpellCheck(esc) {
      if (mSpellCheck) {
        mSpellCheck->BeginUpdateDictionary();
      }
    }
    ~UpdateDictionnaryHolder() {
      if (mSpellCheck) {
        mSpellCheck->EndUpdateDictionary();
      }
    }
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsEditorSpellCheck)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsEditorSpellCheck)

NS_INTERFACE_MAP_BEGIN(nsEditorSpellCheck)
  NS_INTERFACE_MAP_ENTRY(nsIEditorSpellCheck)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditorSpellCheck)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsEditorSpellCheck)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_2(nsEditorSpellCheck,
                           mSpellChecker,
                           mTxtSrvFilter)

nsEditorSpellCheck::nsEditorSpellCheck()
  : mSuggestedWordIndex(0)
  , mDictionaryIndex(0)
  , mUpdateDictionaryRunning(PR_FALSE)
  , mDictWasSetManually(PR_FALSE)
{
}

nsEditorSpellCheck::~nsEditorSpellCheck()
{
  // Make sure we blow the spellchecker away, just in
  // case it hasn't been destroyed already.
  mSpellChecker = nsnull;
}

// The problem is that if the spell checker does not exist, we can not tell
// which dictionaries are installed. This function works around the problem,
// allowing callers to ask if we can spell check without actually doing so (and
// enabling or disabling UI as necessary). This just creates a spellcheck
// object if needed and asks it for the dictionary list.
NS_IMETHODIMP
nsEditorSpellCheck::CanSpellCheck(PRBool* _retval)
{
  nsresult rv;
  nsCOMPtr<nsISpellChecker> spellChecker;
  if (! mSpellChecker) {
    spellChecker = do_CreateInstance(NS_SPELLCHECKER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    spellChecker = mSpellChecker;
  }
  nsTArray<nsString> dictList;
  rv = spellChecker->GetDictionaryList(&dictList);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = (dictList.Length() > 0);
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::InitSpellChecker(nsIEditor* aEditor, PRBool aEnableSelectionChecking)
{
  nsresult rv;

  // We can spell check with any editor type
  nsCOMPtr<nsITextServicesDocument>tsDoc =
     do_CreateInstance("@mozilla.org/textservices/textservicesdocument;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(tsDoc, NS_ERROR_NULL_POINTER);

  tsDoc->SetFilter(mTxtSrvFilter);

  // Pass the editor to the text services document
  rv = tsDoc->InitWithEditor(aEditor);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aEnableSelectionChecking) {
    // Find out if the section is collapsed or not.
    // If it isn't, we want to spellcheck just the selection.

    nsCOMPtr<nsISelection> selection;

    rv = aEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

    PRInt32 count = 0;

    rv = selection->GetRangeCount(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    if (count > 0) {
      nsCOMPtr<nsIDOMRange> range;

      rv = selection->GetRangeAt(0, getter_AddRefs(range));
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool collapsed = PR_FALSE;
      rv = range->GetCollapsed(&collapsed);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!collapsed) {
        // We don't want to touch the range in the selection,
        // so create a new copy of it.

        nsCOMPtr<nsIDOMRange> rangeBounds;
        rv =  range->CloneRange(getter_AddRefs(rangeBounds));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(rangeBounds, NS_ERROR_FAILURE);

        // Make sure the new range spans complete words.

        rv = tsDoc->ExpandRangeToWordBoundaries(rangeBounds);
        NS_ENSURE_SUCCESS(rv, rv);

        // Now tell the text services that you only want
        // to iterate over the text in this range.

        rv = tsDoc->SetExtent(rangeBounds);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  mSpellChecker = do_CreateInstance(NS_SPELLCHECKER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NULL_POINTER);

  rv = mSpellChecker->SetDocument(tsDoc, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // do not fail if UpdateCurrentDictionary fails because this method may
  // succeed later.
  UpdateCurrentDictionary(aEditor);
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetNextMisspelledWord(PRUnichar **aNextMisspelledWord)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  nsAutoString nextMisspelledWord;
  
  DeleteSuggestedWordList();
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  nsresult rv = mSpellChecker->NextMisspelledWord(nextMisspelledWord,
                                                  &mSuggestedWordList);

  *aNextMisspelledWord = ToNewUnicode(nextMisspelledWord);
  return rv;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetSuggestedWord(PRUnichar **aSuggestedWord)
{
  nsAutoString word;
  if ( mSuggestedWordIndex < PRInt32(mSuggestedWordList.Length()))
  {
    *aSuggestedWord = ToNewUnicode(mSuggestedWordList[mSuggestedWordIndex]);
    mSuggestedWordIndex++;
  } else {
    // A blank string signals that there are no more strings
    *aSuggestedWord = ToNewUnicode(EmptyString());
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::CheckCurrentWord(const PRUnichar *aSuggestedWord,
                                     PRBool *aIsMisspelled)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  DeleteSuggestedWordList();
  return mSpellChecker->CheckWord(nsDependentString(aSuggestedWord),
                                  aIsMisspelled, &mSuggestedWordList);
}

NS_IMETHODIMP    
nsEditorSpellCheck::CheckCurrentWordNoSuggest(const PRUnichar *aSuggestedWord,
                                              PRBool *aIsMisspelled)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->CheckWord(nsDependentString(aSuggestedWord),
                                  aIsMisspelled, nsnull);
}

NS_IMETHODIMP    
nsEditorSpellCheck::ReplaceWord(const PRUnichar *aMisspelledWord,
                                const PRUnichar *aReplaceWord,
                                PRBool           allOccurrences)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->Replace(nsDependentString(aMisspelledWord),
                                nsDependentString(aReplaceWord), allOccurrences);
}

NS_IMETHODIMP    
nsEditorSpellCheck::IgnoreWordAllOccurrences(const PRUnichar *aWord)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->IgnoreAll(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetPersonalDictionary()
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

   // We can spell check with any editor type
  mDictionaryList.Clear();
  mDictionaryIndex = 0;
  return mSpellChecker->GetPersonalDictionary(&mDictionaryList);
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetPersonalDictionaryWord(PRUnichar **aDictionaryWord)
{
  if ( mDictionaryIndex < PRInt32( mDictionaryList.Length()))
  {
    *aDictionaryWord = ToNewUnicode(mDictionaryList[mDictionaryIndex]);
    mDictionaryIndex++;
  } else {
    // A blank string signals that there are no more strings
    *aDictionaryWord = ToNewUnicode(EmptyString());
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::AddWordToDictionary(const PRUnichar *aWord)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->AddWordToPersonalDictionary(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::RemoveWordFromDictionary(const PRUnichar *aWord)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  return mSpellChecker->RemoveWordFromPersonalDictionary(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetDictionaryList(PRUnichar ***aDictionaryList, PRUint32 *aCount)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_TRUE(aDictionaryList && aCount, NS_ERROR_NULL_POINTER);

  *aDictionaryList = 0;
  *aCount          = 0;

  nsTArray<nsString> dictList;

  nsresult rv = mSpellChecker->GetDictionaryList(&dictList);

  NS_ENSURE_SUCCESS(rv, rv);

  PRUnichar **tmpPtr = 0;

  if (dictList.Length() < 1)
  {
    // If there are no dictionaries, return an array containing
    // one element and a count of one.

    tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *));

    NS_ENSURE_TRUE(tmpPtr, NS_ERROR_OUT_OF_MEMORY);

    *tmpPtr          = 0;
    *aDictionaryList = tmpPtr;
    *aCount          = 0;

    return NS_OK;
  }

  tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * dictList.Length());

  NS_ENSURE_TRUE(tmpPtr, NS_ERROR_OUT_OF_MEMORY);

  *aDictionaryList = tmpPtr;
  *aCount          = dictList.Length();

  PRUint32 i;

  for (i = 0; i < *aCount; i++)
  {
    tmpPtr[i] = ToNewUnicode(dictList[i]);
  }

  return rv;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetCurrentDictionary(PRUnichar **aDictionary)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_TRUE(aDictionary, NS_ERROR_NULL_POINTER);

  *aDictionary = 0;

  nsAutoString dictStr;
  nsresult rv = mSpellChecker->GetCurrentDictionary(dictStr);
  NS_ENSURE_SUCCESS(rv, rv);

  *aDictionary = ToNewUnicode(dictStr);

  return rv;
}

NS_IMETHODIMP    
nsEditorSpellCheck::SetCurrentDictionary(const PRUnichar *aDictionary)
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_TRUE(aDictionary, NS_ERROR_NULL_POINTER);

  if (!mUpdateDictionaryRunning) {
    mDictWasSetManually = PR_TRUE;
  }
  return mSpellChecker->SetCurrentDictionary(nsDependentString(aDictionary));
}

NS_IMETHODIMP    
nsEditorSpellCheck::UninitSpellChecker()
{
  NS_ENSURE_TRUE(mSpellChecker, NS_ERROR_NOT_INITIALIZED);

  // Cleanup - kill the spell checker
  DeleteSuggestedWordList();
  mDictionaryList.Clear();
  mDictionaryIndex = 0;
  mSpellChecker = 0;
  return NS_OK;
}

// Save the last set dictionary to the user's preferences.
NS_IMETHODIMP
nsEditorSpellCheck::SaveDefaultDictionary()
{
  if (!mDictWasSetManually) {
    return NS_OK;
  }
  PRUnichar *dictName = nsnull;
  nsresult rv = GetCurrentDictionary(&dictName);

  if (NS_SUCCEEDED(rv) && dictName && *dictName) {
    rv = Preferences::SetString("spellchecker.dictionary", dictName);
  }

  if (dictName) {
    nsMemory::Free(dictName);
  }

  return rv;
}


/* void setFilter (in nsITextServicesFilter filter); */
NS_IMETHODIMP 
nsEditorSpellCheck::SetFilter(nsITextServicesFilter *filter)
{
  mTxtSrvFilter = filter;
  return NS_OK;
}

nsresult    
nsEditorSpellCheck::DeleteSuggestedWordList()
{
  mSuggestedWordList.Clear();
  mSuggestedWordIndex = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsEditorSpellCheck::UpdateCurrentDictionary(nsIEditor* aEditor)
{
  if (mDictWasSetManually) { // user has set dictionary manually; we better not change it.
    return NS_OK;
  }

  nsresult rv;

  UpdateDictionnaryHolder holder(this);

  // Tell the spellchecker what dictionary to use:
  nsAutoString dictName;

  // First, try to get language with html5 algorithm
  nsAutoString editorLang;

  nsCOMPtr<nsIContent> rootContent;

  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (htmlEditor) {
    rootContent = htmlEditor->GetActiveEditingHost();
  } else {
    nsCOMPtr<nsIDOMElement> rootElement;
    rv = aEditor->GetRootElement(getter_AddRefs(rootElement));
    NS_ENSURE_SUCCESS(rv, rv);
    rootContent = do_QueryInterface(rootElement);
  }
  NS_ENSURE_TRUE(rootContent, NS_ERROR_FAILURE);

  rootContent->GetLang(editorLang);

  if (editorLang.IsEmpty()) {
    nsCOMPtr<nsIDocument> doc = rootContent->GetCurrentDoc();
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
    doc->GetContentLanguage(editorLang);
  }

  if (!editorLang.IsEmpty()) {
    dictName.Assign(editorLang);
  }

  // otherwise, get language from preferences
  if (dictName.IsEmpty()) {
    dictName.Assign(Preferences::GetLocalizedString("spellchecker.dictionary"));
  }

  if (dictName.IsEmpty())
  {
    // Prefs didn't give us a dictionary name, so just get the current
    // locale and use that as the default dictionary name!

    nsCOMPtr<nsIXULChromeRegistry> packageRegistry =
      mozilla::services::GetXULChromeRegistryService();

    if (packageRegistry) {
      nsCAutoString utf8DictName;
      rv = packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("global"),
                                              utf8DictName);
      AppendUTF8toUTF16(utf8DictName, dictName);
    }
  }

  SetCurrentDictionary(NS_LITERAL_STRING("").get());

  if (NS_SUCCEEDED(rv) && !dictName.IsEmpty()) {
    rv = SetCurrentDictionary(dictName.get());
    if (NS_FAILED(rv)) {
      // required dictionary was not available. Try to get a dictionary
      // matching at least language part of dictName: If required dictionary is
      // "aa-bb", we try "aa", then we try any available dictionary aa-XX
      nsAutoString langCode;
      PRInt32 dashIdx = dictName.FindChar('-');
      if (dashIdx != -1) {
        langCode.Assign(Substring(dictName, 0, dashIdx));
        // try to use langCode
        rv = SetCurrentDictionary(langCode.get());
      } else {
        langCode.Assign(dictName);
      }
      if (NS_FAILED(rv)) {
        // loop over avaible dictionaries; if we find one with required
        // language, use it
        nsTArray<nsString> dictList;
        rv = mSpellChecker->GetDictionaryList(&dictList);
        NS_ENSURE_SUCCESS(rv, rv);
        nsDefaultStringComparator comparator;
        PRInt32 i, count = dictList.Length();
        for (i = 0; i < count; i++) {
          nsAutoString dictStr(dictList.ElementAt(i));
          if (nsStyleUtil::DashMatchCompare(dictStr, langCode, comparator) &&
              NS_SUCCEEDED(SetCurrentDictionary(dictStr.get()))) {
              break;
          }
        }
      }
    }
  }

  // If we have not set dictionary, and the editable element doesn't have a
  // lang attribute, we try to get a dictionary. First try, en-US. If it does
  // not work, pick the first one.
  if (editorLang.IsEmpty()) {
    nsAutoString currentDictionary;
    rv = mSpellChecker->GetCurrentDictionary(currentDictionary);
    if (NS_FAILED(rv) || currentDictionary.IsEmpty()) {
      rv = SetCurrentDictionary(NS_LITERAL_STRING("en-US").get());
      if (NS_FAILED(rv)) {
        nsTArray<nsString> dictList;
        rv = mSpellChecker->GetDictionaryList(&dictList);
        if (NS_SUCCEEDED(rv) && dictList.Length() > 0) {
          SetCurrentDictionary(dictList[0].get());
        }
      }
    }
  }

  // If an error was thrown while setting the dictionary, just
  // fail silently so that the spellchecker dialog is allowed to come
  // up. The user can manually reset the language to their choice on
  // the dialog if it is wrong.

  DeleteSuggestedWordList();

  return NS_OK;
}
