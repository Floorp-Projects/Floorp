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
 * The Initial Developer of the Original Code is David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein Deinst@world.std.com
 *   Jesper Kristensen <mail@jesperkristensen.dk>
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


#include "mozSpellChecker.h"
#include "nsIServiceManager.h"
#include "mozISpellI18NManager.h"
#include "nsIStringEnumerator.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"

#define DEFAULT_SPELL_CHECKER "@mozilla.org/spellchecker/engine;1"

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozSpellChecker)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozSpellChecker)

NS_INTERFACE_MAP_BEGIN(mozSpellChecker)
  NS_INTERFACE_MAP_ENTRY(nsISpellChecker)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpellChecker)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(mozSpellChecker)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_3(mozSpellChecker,
                           mConverter,
                           mTsDoc,
                           mPersonalDictionary)

mozSpellChecker::mozSpellChecker()
{
}

mozSpellChecker::~mozSpellChecker()
{
  if(mPersonalDictionary){
    //    mPersonalDictionary->Save();
    mPersonalDictionary->EndSession();
  }
  mSpellCheckingEngine = nsnull;
  mPersonalDictionary = nsnull;
}

nsresult 
mozSpellChecker::Init()
{
  mPersonalDictionary = do_GetService("@mozilla.org/spellchecker/personaldictionary;1");
  
  mSpellCheckingEngine = nsnull;

  return NS_OK;
} 

NS_IMETHODIMP 
mozSpellChecker::SetDocument(nsITextServicesDocument *aDoc, PRBool aFromStartofDoc)
{
  mTsDoc = aDoc;
  mFromStart = aFromStartofDoc;
  return NS_OK;
}


NS_IMETHODIMP 
mozSpellChecker::NextMisspelledWord(nsAString &aWord, nsTArray<nsString> *aSuggestions)
{
  if(!aSuggestions||!mConverter)
    return NS_ERROR_NULL_POINTER;

  PRInt32 selOffset;
  PRInt32 begin,end;
  nsresult result;
  result = SetupDoc(&selOffset);
  PRBool isMisspelled,done;
  if (NS_FAILED(result))
    return result;

  while( NS_SUCCEEDED(mTsDoc->IsDone(&done)) && !done )
    {
      nsString str;
      result = mTsDoc->GetCurrentTextBlock(&str);
  
      if (NS_FAILED(result))
        return result;
      do{
        result = mConverter->FindNextWord(str.get(),str.Length(),selOffset,&begin,&end);
        if(NS_SUCCEEDED(result)&&(begin != -1)){
          const nsAString &currWord = Substring(str, begin, end - begin);
          result = CheckWord(currWord, &isMisspelled, aSuggestions);
          if(isMisspelled){
            aWord = currWord;
            mTsDoc->SetSelection(begin, end-begin);
            // After ScrollSelectionIntoView(), the pending notifications might
            // be flushed and PresShell/PresContext/Frames may be dead.
            // See bug 418470.
            mTsDoc->ScrollSelectionIntoView();
            return NS_OK;
          }
        }
        selOffset = end;
      }while(end != -1);
      mTsDoc->NextBlock();
      selOffset=0;
    }
  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::CheckWord(const nsAString &aWord, PRBool *aIsMisspelled, nsTArray<nsString> *aSuggestions)
{
  nsresult result;
  PRBool correct;
  if(!mSpellCheckingEngine)
    return NS_ERROR_NULL_POINTER;

  *aIsMisspelled = PR_FALSE;
  result = mSpellCheckingEngine->Check(PromiseFlatString(aWord).get(), &correct);
  NS_ENSURE_SUCCESS(result, result);
  if(!correct){
    if(aSuggestions){
      PRUint32 count,i;
      PRUnichar **words;
      
      result = mSpellCheckingEngine->Suggest(PromiseFlatString(aWord).get(), &words, &count);
      NS_ENSURE_SUCCESS(result, result); 
      for(i=0;i<count;i++){
        aSuggestions->AppendElement(nsDependentString(words[i]));
      }
      
      if (count)
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, words);
    }
    *aIsMisspelled = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::Replace(const nsAString &aOldWord, const nsAString &aNewWord, PRBool aAllOccurrences)
{
  if(!mConverter)
    return NS_ERROR_NULL_POINTER;

  nsAutoString newWord(aNewWord); // sigh

  if(aAllOccurrences){
    PRInt32 selOffset;
    PRInt32 startBlock,currentBlock,currOffset;
    PRInt32 begin,end;
    PRBool done;
    nsresult result;
    nsAutoString str;

    // find out where we are
    result = SetupDoc(&selOffset);
    if(NS_FAILED(result))
      return result;
    result = GetCurrentBlockIndex(mTsDoc,&startBlock);
    if(NS_FAILED(result))
      return result;

    //start at the beginning
    result = mTsDoc->FirstBlock();
    currOffset=0;
    currentBlock = 0;
    while( NS_SUCCEEDED(mTsDoc->IsDone(&done)) && !done )
      {
        result = mTsDoc->GetCurrentTextBlock(&str);
        do{
          result = mConverter->FindNextWord(str.get(),str.Length(),currOffset,&begin,&end);
          if(NS_SUCCEEDED(result)&&(begin != -1)){
            if (aOldWord.Equals(Substring(str, begin, end-begin))) {
              // if we are before the current selection point but in the same block
              // move the selection point forwards
              if((currentBlock == startBlock)&&(begin < selOffset)){
                selOffset +=
                  PRInt32(aNewWord.Length()) - PRInt32(aOldWord.Length());
                if(selOffset < begin) selOffset=begin;
              }
              mTsDoc->SetSelection(begin, end-begin);
              mTsDoc->InsertText(&newWord);
              mTsDoc->GetCurrentTextBlock(&str);
              end += (aNewWord.Length() - aOldWord.Length());  // recursion was cute in GEB, not here.
            }
          }
          currOffset = end;
        }while(currOffset != -1);
        mTsDoc->NextBlock();
        currentBlock++;
        currOffset=0;          
      }

    // We are done replacing.  Put the selection point back where we found  it (or equivalent);
    result = mTsDoc->FirstBlock();
    currentBlock = 0;
    while(( NS_SUCCEEDED(mTsDoc->IsDone(&done)) && !done ) &&(currentBlock < startBlock)){
      mTsDoc->NextBlock();
    }

//After we have moved to the block where the first occurrence of replace was done, put the 
//selection to the next word following it. In case there is no word following it i.e if it happens
//to be the last word in that block, then move to the next block and put the selection to the 
//first word in that block, otherwise when the Setupdoc() is called, it queries the LastSelectedBlock()
//and the selection offset of the last occurrence of the replaced word is taken instead of the first 
//occurrence and things get messed up as reported in the bug 244969

    if( NS_SUCCEEDED(mTsDoc->IsDone(&done)) && !done ){
      nsString str;                                
      result = mTsDoc->GetCurrentTextBlock(&str);  
      result = mConverter->FindNextWord(str.get(),str.Length(),selOffset,&begin,&end);
            if(end == -1)
             {
                mTsDoc->NextBlock();
                selOffset=0;
                result = mTsDoc->GetCurrentTextBlock(&str); 
                result = mConverter->FindNextWord(str.get(),str.Length(),selOffset,&begin,&end);
                mTsDoc->SetSelection(begin, 0);
             }
         else
                mTsDoc->SetSelection(begin, 0);
    }
 }
  else{
    mTsDoc->InsertText(&newWord);
  }
  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::IgnoreAll(const nsAString &aWord)
{
  if(mPersonalDictionary){
    mPersonalDictionary->IgnoreWord(PromiseFlatString(aWord).get());
  }
  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::AddWordToPersonalDictionary(const nsAString &aWord)
{
  nsresult res;
  PRUnichar empty=0;
  if (!mPersonalDictionary)
    return NS_ERROR_NULL_POINTER;
  res = mPersonalDictionary->AddWord(PromiseFlatString(aWord).get(),&empty);
  return res;
}

NS_IMETHODIMP 
mozSpellChecker::RemoveWordFromPersonalDictionary(const nsAString &aWord)
{
  nsresult res;
  PRUnichar empty=0;
  if (!mPersonalDictionary)
    return NS_ERROR_NULL_POINTER;
  res = mPersonalDictionary->RemoveWord(PromiseFlatString(aWord).get(),&empty);
  return res;
}

NS_IMETHODIMP 
mozSpellChecker::GetPersonalDictionary(nsTArray<nsString> *aWordList)
{
  if(!aWordList || !mPersonalDictionary)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIStringEnumerator> words;
  mPersonalDictionary->GetWordList(getter_AddRefs(words));
  
  PRBool hasMore;
  nsAutoString word;
  while (NS_SUCCEEDED(words->HasMore(&hasMore)) && hasMore) {
    words->GetNext(word);
    aWordList->AppendElement(word);
  }
  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::GetDictionaryList(nsTArray<nsString> *aDictionaryList)
{
  nsresult rv;

  // For catching duplicates
  nsClassHashtable<nsStringHashKey, nsCString> dictionaries;
  dictionaries.Init();

  nsCOMArray<mozISpellCheckingEngine> spellCheckingEngines;
  rv = GetEngineList(&spellCheckingEngines);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < spellCheckingEngines.Count(); i++) {
    nsCOMPtr<mozISpellCheckingEngine> engine = spellCheckingEngines[i];

    PRUint32 count = 0;
    PRUnichar **words = NULL;
    engine->GetDictionaryList(&words, &count);
    for (PRUint32 k = 0; k < count; k++) {
      nsAutoString dictName;

      dictName.Assign(words[k]);

      // Skip duplicate dictionaries. Only take the first one
      // for each name.
      if (dictionaries.Get(dictName, NULL))
        continue;

      dictionaries.Put(dictName, NULL);

      if (!aDictionaryList->AppendElement(dictName)) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, words);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, words);
  }

  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::GetCurrentDictionary(nsAString &aDictionary)
{
  if (!mSpellCheckingEngine) {
    aDictionary.AssignLiteral("");
    return NS_OK;
  }

  nsXPIDLString dictname;
  mSpellCheckingEngine->GetDictionary(getter_Copies(dictname));
  aDictionary = dictname;
  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::SetCurrentDictionary(const nsAString &aDictionary)
{
  mSpellCheckingEngine = nsnull;

  if (aDictionary.IsEmpty()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMArray<mozISpellCheckingEngine> spellCheckingEngines;
  rv = GetEngineList(&spellCheckingEngines);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < spellCheckingEngines.Count(); i++) {
    // We must set mSpellCheckingEngine before we call SetDictionary, since
    // SetDictionary calls back to this spell checker to check if the
    // dictionary was set
    mSpellCheckingEngine = spellCheckingEngines[i];

    rv = mSpellCheckingEngine->SetDictionary(PromiseFlatString(aDictionary).get());

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<mozIPersonalDictionary> personalDictionary = do_GetService("@mozilla.org/spellchecker/personaldictionary;1");
      mSpellCheckingEngine->SetPersonalDictionary(personalDictionary.get());

      return NS_OK;
    }
  }

  mSpellCheckingEngine = NULL;
  
  // We could not find any engine with the requested dictionary
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
mozSpellChecker::CheckCurrentDictionary()
{
  // If the current dictionary has been uninstalled, we need to stop using it.
  // This happens when there is a current engine, but that engine has no
  // current dictionary.

  if (!mSpellCheckingEngine) {
    // We didn't have a current dictionary
    return NS_OK;
  }

  nsXPIDLString dictname;
  mSpellCheckingEngine->GetDictionary(getter_Copies(dictname));

  if (!dictname.IsEmpty()) {
    // We still have a current dictionary
    return NS_OK;
  }

  // We had a current dictionary, but it has gone, so we cannot use it anymore.
  mSpellCheckingEngine = nsnull;
  return NS_OK;
}

nsresult
mozSpellChecker::SetupDoc(PRInt32 *outBlockOffset)
{
  nsresult  rv;

  nsITextServicesDocument::TSDBlockSelectionStatus blockStatus;
  PRInt32 selOffset;
  PRInt32 selLength;
  *outBlockOffset = 0;

  if (!mFromStart) 
  {
    rv = mTsDoc->LastSelectedBlock(&blockStatus, &selOffset, &selLength);
    if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
    {
      switch (blockStatus)
      {
        case nsITextServicesDocument::eBlockOutside:  // No TB in S, but found one before/after S.
        case nsITextServicesDocument::eBlockPartial:  // S begins or ends in TB but extends outside of TB.
          // the TS doc points to the block we want.
          *outBlockOffset = selOffset + selLength;
          break;
                    
        case nsITextServicesDocument::eBlockInside:  // S extends beyond the start and end of TB.
          // we want the block after this one.
          rv = mTsDoc->NextBlock();
          *outBlockOffset = 0;
          break;
                
        case nsITextServicesDocument::eBlockContains: // TB contains entire S.
          *outBlockOffset = selOffset + selLength;
          break;
        
        case nsITextServicesDocument::eBlockNotFound: // There is no text block (TB) in or before the selection (S).
        default:
          NS_NOTREACHED("Shouldn't ever get this status");
      }
    }
    else  //failed to get last sel block. Just start at beginning
    {
      rv = mTsDoc->FirstBlock();
      *outBlockOffset = 0;
    }
  
  }
  else // we want the first block
  {
    rv = mTsDoc->FirstBlock();
    mFromStart = PR_FALSE;
  }
  return rv;
}


// utility method to discover which block we're in. The TSDoc interface doesn't give
// us this, because it can't assume a read-only document.
// shamelessly stolen from nsTextServicesDocument
nsresult
mozSpellChecker::GetCurrentBlockIndex(nsITextServicesDocument *aDoc, PRInt32 *outBlockIndex)
{
  PRInt32  blockIndex = 0;
  PRBool   isDone = PR_FALSE;
  nsresult result = NS_OK;

  do
  {
    aDoc->PrevBlock();

    result = aDoc->IsDone(&isDone);

    if (!isDone)
      blockIndex ++;

  } while (NS_SUCCEEDED(result) && !isDone);
  
  *outBlockIndex = blockIndex;

  return result;
}

nsresult
mozSpellChecker::GetEngineList(nsCOMArray<mozISpellCheckingEngine>* aSpellCheckingEngines)
{
  nsresult rv;
  PRBool hasMoreEngines;

  nsCOMPtr<nsICategoryManager> catMgr = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMgr)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsISimpleEnumerator> catEntries;

  // Get contract IDs of registrated external spell-check engines and
  // append one of HunSpell at the end.
  rv = catMgr->EnumerateCategory("spell-check-engine", getter_AddRefs(catEntries));
  if (NS_FAILED(rv))
    return rv;

  while (catEntries->HasMoreElements(&hasMoreEngines), hasMoreEngines){
    nsCOMPtr<nsISupports> elem;
    rv = catEntries->GetNext(getter_AddRefs(elem));

    nsCOMPtr<nsISupportsCString> entry = do_QueryInterface(elem, &rv);
    if (NS_FAILED(rv))
      return rv;

    nsCString contractId;
    rv = entry->GetData(contractId);
    if (NS_FAILED(rv))
      return rv;

    // Try to load spellchecker engine. Ignore errors silently
    // except for the last one (HunSpell).
    nsCOMPtr<mozISpellCheckingEngine> engine =
      do_GetService(contractId.get(), &rv);
    if (NS_SUCCEEDED(rv)) {
      aSpellCheckingEngines->AppendObject(engine);
    }
  }

  // Try to load HunSpell spellchecker engine.
  nsCOMPtr<mozISpellCheckingEngine> engine =
    do_GetService(DEFAULT_SPELL_CHECKER, &rv);
  if (NS_FAILED(rv)) {
    // Fail if not succeeded to load HunSpell. Ignore errors
    // for external spellcheck engines.
    return rv;
  }
  aSpellCheckingEngines->AppendObject(engine);

  return NS_OK;
}
