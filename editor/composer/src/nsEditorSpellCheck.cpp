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

#include "nsITextServicesDocument.h"
#include "nsISpellChecker.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIEditor.h"

#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsISupportsPrimitives.h"
#include "nsIServiceManagerUtils.h"
#include "nsIChromeRegistry.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsITextServicesFilter.h"

NS_IMPL_ISUPPORTS1(nsEditorSpellCheck, nsIEditorSpellCheck)

nsEditorSpellCheck::nsEditorSpellCheck()
  : mSuggestedWordIndex(0)
  , mDictionaryIndex(0)
{
}

nsEditorSpellCheck::~nsEditorSpellCheck()
{
  // Make sure we blow the spellchecker away, just in
  // case it hasn't been destroyed already.
  mSpellChecker = nsnull;
}

NS_IMETHODIMP    
nsEditorSpellCheck::InitSpellChecker(nsIEditor* aEditor, PRBool aEnableSelectionChecking)
{
  nsresult rv;

  // We can spell check with any editor type
  nsCOMPtr<nsITextServicesDocument>tsDoc =
     do_CreateInstance("@mozilla.org/textservices/textservicesdocument;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!tsDoc)
    return NS_ERROR_NULL_POINTER;

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

  rv = nsComponentManager::CreateInstance(NS_SPELLCHECKER_CONTRACTID,
                                          nsnull,
                                          NS_GET_IID(nsISpellChecker),
                                       (void **)getter_AddRefs(mSpellChecker));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mSpellChecker)
    return NS_ERROR_NULL_POINTER;

  rv = mSpellChecker->SetDocument(tsDoc, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Tell the spellchecker what dictionary to use:

  nsXPIDLString dictName;

  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);

  if (NS_SUCCEEDED(rv) && prefBranch) {
    nsCOMPtr<nsISupportsString> prefString;
    rv = prefBranch->GetComplexValue("spellchecker.dictionary",
                                     NS_GET_IID(nsISupportsString),
                                     getter_AddRefs(prefString));
    if (prefString) {
      prefString->ToString(getter_Copies(dictName));
    }
  }

  if (NS_FAILED(rv) || dictName.IsEmpty())
  {
    // Prefs didn't give us a dictionary name, so just get the current
    // locale and use that as the default dictionary name!

    nsCOMPtr<nsIXULChromeRegistry> packageRegistry =
      do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);

    if (NS_SUCCEEDED(rv) && packageRegistry) {
      nsCAutoString utf8DictName;
      rv = packageRegistry->GetSelectedLocale(NS_LITERAL_CSTRING("editor"),
                                              utf8DictName);
      AppendUTF8toUTF16(utf8DictName, dictName);
    }
  }

  if (NS_SUCCEEDED(rv) && !dictName.IsEmpty())
    SetCurrentDictionary(dictName.get());

  // If an error was thrown while checking the dictionary pref, just
  // fail silently so that the spellchecker dialog is allowed to come
  // up. The user can manually reset the language to their choice on
  // the dialog if it is wrong.

  DeleteSuggestedWordList();

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetNextMisspelledWord(PRUnichar **aNextMisspelledWord)
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  nsAutoString nextMisspelledWord;
  
  DeleteSuggestedWordList();
  nsresult rv = mSpellChecker->NextMisspelledWord(nextMisspelledWord,
                                                  &mSuggestedWordList);

  *aNextMisspelledWord = ToNewUnicode(nextMisspelledWord);
  return rv;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetSuggestedWord(PRUnichar **aSuggestedWord)
{
  nsAutoString word;
  if ( mSuggestedWordIndex < mSuggestedWordList.Count())
  {
    mSuggestedWordList.StringAt(mSuggestedWordIndex, word);
    mSuggestedWordIndex++;
  } else {
    // A blank string signals that there are no more strings
    word.Truncate();
  }

  *aSuggestedWord = ToNewUnicode(word);
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::CheckCurrentWord(const PRUnichar *aSuggestedWord,
                                     PRBool *aIsMisspelled)
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  DeleteSuggestedWordList();
  return mSpellChecker->CheckWord(nsDependentString(aSuggestedWord),
                                  aIsMisspelled, &mSuggestedWordList);
}

NS_IMETHODIMP    
nsEditorSpellCheck::ReplaceWord(const PRUnichar *aMisspelledWord,
                                const PRUnichar *aReplaceWord,
                                PRBool           allOccurrences)
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  return mSpellChecker->Replace(nsDependentString(aMisspelledWord),
                                nsDependentString(aReplaceWord), allOccurrences);
}

NS_IMETHODIMP    
nsEditorSpellCheck::IgnoreWordAllOccurrences(const PRUnichar *aWord)
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  return mSpellChecker->IgnoreAll(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetPersonalDictionary()
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

   // We can spell check with any editor type
  mDictionaryList.Clear();
  mDictionaryIndex = 0;
  return mSpellChecker->GetPersonalDictionary(&mDictionaryList);
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetPersonalDictionaryWord(PRUnichar **aDictionaryWord)
{
  nsAutoString word;
  if ( mDictionaryIndex < mDictionaryList.Count())
  {
    mDictionaryList.StringAt(mDictionaryIndex, word);
    mDictionaryIndex++;
  } else {
    // A blank string signals that there are no more strings
    word.Truncate();
  }

  *aDictionaryWord = ToNewUnicode(word);
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorSpellCheck::AddWordToDictionary(const PRUnichar *aWord)
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  return mSpellChecker->AddWordToPersonalDictionary(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::RemoveWordFromDictionary(const PRUnichar *aWord)
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  return mSpellChecker->RemoveWordFromPersonalDictionary(nsDependentString(aWord));
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetDictionaryList(PRUnichar ***aDictionaryList, PRUint32 *aCount)
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  if (!aDictionaryList || !aCount)
    return NS_ERROR_NULL_POINTER;

  *aDictionaryList = 0;
  *aCount          = 0;

  nsStringArray dictList;

  nsresult rv = mSpellChecker->GetDictionaryList(&dictList);

  if (NS_FAILED(rv))
    return rv;

  PRUnichar **tmpPtr = 0;

  if (dictList.Count() < 1)
  {
    // If there are no dictionaries, return an array containing
    // one element and a count of one.

    tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *));

    if (!tmpPtr)
      return NS_ERROR_OUT_OF_MEMORY;

    *tmpPtr          = 0;
    *aDictionaryList = tmpPtr;
    *aCount          = 0;

    return NS_OK;
  }

  tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * dictList.Count());

  if (!tmpPtr)
    return NS_ERROR_OUT_OF_MEMORY;

  *aDictionaryList = tmpPtr;
  *aCount          = dictList.Count();

  nsAutoString dictStr;

  PRUint32 i;

  for (i = 0; i < *aCount; i++)
  {
    dictList.StringAt(i, dictStr);
    tmpPtr[i] = ToNewUnicode(dictStr);
  }

  return rv;
}

NS_IMETHODIMP    
nsEditorSpellCheck::GetCurrentDictionary(PRUnichar **aDictionary)
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  if (!aDictionary)
    return NS_ERROR_NULL_POINTER;

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
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  if (!aDictionary)
    return NS_ERROR_NULL_POINTER;

  return mSpellChecker->SetCurrentDictionary(nsDependentString(aDictionary));
}

NS_IMETHODIMP    
nsEditorSpellCheck::UninitSpellChecker()
{
  if (!mSpellChecker)
    return NS_NOINTERFACE;

  // Save the last used dictionary to the user's preferences.
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);

  if (NS_SUCCEEDED(rv) && prefBranch)
  {
    PRUnichar *dictName = nsnull;

    rv = GetCurrentDictionary(&dictName);

    if (NS_SUCCEEDED(rv) && dictName && *dictName) {
      nsCOMPtr<nsISupportsString> prefString =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);

      if (NS_SUCCEEDED(rv) && prefString) {
        prefString->SetData(nsDependentString(dictName));
        rv = prefBranch->SetComplexValue("spellchecker.dictionary",
                                         NS_GET_IID(nsISupportsString),
                                         prefString);
      }
    }

    if (dictName)
      nsMemory::Free(dictName);
  }

  // Cleanup - kill the spell checker
  DeleteSuggestedWordList();
  mDictionaryList.Clear();
  mDictionaryIndex = 0;
  mSpellChecker = 0;
  return NS_OK;
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
