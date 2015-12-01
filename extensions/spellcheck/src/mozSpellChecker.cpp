/* vim: set ts=2 sts=2 sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozSpellChecker.h"
#include "nsIServiceManager.h"
#include "mozISpellI18NManager.h"
#include "nsIStringEnumerator.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/PRemoteSpellcheckEngineChild.h"
#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"

using mozilla::dom::ContentChild;
using mozilla::PRemoteSpellcheckEngineChild;
using mozilla::RemoteSpellcheckEngineChild;

#define DEFAULT_SPELL_CHECKER "@mozilla.org/spellchecker/engine;1"

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozSpellChecker)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozSpellChecker)

NS_INTERFACE_MAP_BEGIN(mozSpellChecker)
  NS_INTERFACE_MAP_ENTRY(nsISpellChecker)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpellChecker)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(mozSpellChecker)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(mozSpellChecker,
                         mTsDoc,
                         mPersonalDictionary)

mozSpellChecker::mozSpellChecker()
  : mEngine(nullptr)
{
}

mozSpellChecker::~mozSpellChecker()
{
  if (mPersonalDictionary) {
    //    mPersonalDictionary->Save();
    mPersonalDictionary->EndSession();
  }
  mSpellCheckingEngine = nullptr;
  mPersonalDictionary = nullptr;

  if (mEngine) {
    MOZ_ASSERT(XRE_IsContentProcess());
    mEngine->Send__delete__(mEngine);
    MOZ_ASSERT(!mEngine);
  }
}

nsresult
mozSpellChecker::Init()
{
  mSpellCheckingEngine = nullptr;
  if (XRE_IsContentProcess()) {
    mozilla::dom::ContentChild* contentChild = mozilla::dom::ContentChild::GetSingleton();
    MOZ_ASSERT(contentChild);
    mEngine = new RemoteSpellcheckEngineChild(this);
    contentChild->SendPRemoteSpellcheckEngineConstructor(mEngine);
  } else {
    mPersonalDictionary = do_GetService("@mozilla.org/spellchecker/personaldictionary;1");
  }

  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::SetDocument(nsITextServicesDocument *aDoc, bool aFromStartofDoc)
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

  int32_t selOffset;
  int32_t begin,end;
  nsresult result;
  result = SetupDoc(&selOffset);
  bool isMisspelled,done;
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
mozSpellChecker::CheckWord(const nsAString &aWord, bool *aIsMisspelled, nsTArray<nsString> *aSuggestions)
{
  nsresult result;
  bool correct;

  if (XRE_IsContentProcess()) {
    nsString wordwrapped = nsString(aWord);
    bool rv;
    if (aSuggestions) {
      rv = mEngine->SendCheckAndSuggest(wordwrapped, aIsMisspelled, aSuggestions);
    } else {
      rv = mEngine->SendCheck(wordwrapped, aIsMisspelled);
    }
    return rv ? NS_OK : NS_ERROR_NOT_AVAILABLE;
  }

  if(!mSpellCheckingEngine) {
    return NS_ERROR_NULL_POINTER;
  }
  *aIsMisspelled = false;
  result = mSpellCheckingEngine->Check(PromiseFlatString(aWord).get(), &correct);
  NS_ENSURE_SUCCESS(result, result);
  if(!correct){
    if(aSuggestions){
      uint32_t count,i;
      char16_t **words;
      
      result = mSpellCheckingEngine->Suggest(PromiseFlatString(aWord).get(), &words, &count);
      NS_ENSURE_SUCCESS(result, result);
      nsString* suggestions = aSuggestions->AppendElements(count);
      for(i=0;i<count;i++){
	suggestions[i].Assign(words[i]);
      }
      
      if (count)
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, words);
    }
    *aIsMisspelled = true;
  }
  return NS_OK;
}

NS_IMETHODIMP 
mozSpellChecker::Replace(const nsAString &aOldWord, const nsAString &aNewWord, bool aAllOccurrences)
{
  if(!mConverter)
    return NS_ERROR_NULL_POINTER;

  nsAutoString newWord(aNewWord); // sigh

  if(aAllOccurrences){
    int32_t selOffset;
    int32_t startBlock,currentBlock,currOffset;
    int32_t begin,end;
    bool done;
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
                  int32_t(aNewWord.Length()) - int32_t(aOldWord.Length());
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
  char16_t empty=0;
  if (!mPersonalDictionary)
    return NS_ERROR_NULL_POINTER;
  res = mPersonalDictionary->AddWord(PromiseFlatString(aWord).get(),&empty);
  return res;
}

NS_IMETHODIMP 
mozSpellChecker::RemoveWordFromPersonalDictionary(const nsAString &aWord)
{
  nsresult res;
  char16_t empty=0;
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
  
  bool hasMore;
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
  if (XRE_IsContentProcess()) {
    ContentChild *child = ContentChild::GetSingleton();
    child->GetAvailableDictionaries(*aDictionaryList);
    return NS_OK;
  }

  nsresult rv;

  // For catching duplicates
  nsTHashtable<nsStringHashKey> dictionaries;

  nsCOMArray<mozISpellCheckingEngine> spellCheckingEngines;
  rv = GetEngineList(&spellCheckingEngines);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int32_t i = 0; i < spellCheckingEngines.Count(); i++) {
    nsCOMPtr<mozISpellCheckingEngine> engine = spellCheckingEngines[i];

    uint32_t count = 0;
    char16_t **words = nullptr;
    engine->GetDictionaryList(&words, &count);
    for (uint32_t k = 0; k < count; k++) {
      nsAutoString dictName;

      dictName.Assign(words[k]);

      // Skip duplicate dictionaries. Only take the first one
      // for each name.
      if (dictionaries.Contains(dictName))
        continue;

      dictionaries.PutEntry(dictName);

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
  if (XRE_IsContentProcess()) {
    aDictionary = mCurrentDictionary;
    return NS_OK;
  }

  if (!mSpellCheckingEngine) {
    aDictionary.Truncate();
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
  if (XRE_IsContentProcess()) {
    nsString wrappedDict = nsString(aDictionary);
    bool isSuccess;
    mEngine->SendSetDictionary(wrappedDict, &isSuccess);
    if (!isSuccess) {
      mCurrentDictionary.Truncate();
      return NS_ERROR_NOT_AVAILABLE;
    }

    mCurrentDictionary = wrappedDict;
    return NS_OK;
  }

  // Calls to mozISpellCheckingEngine::SetDictionary might destroy us
  RefPtr<mozSpellChecker> kungFuDeathGrip = this;

  mSpellCheckingEngine = nullptr;

  if (aDictionary.IsEmpty()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMArray<mozISpellCheckingEngine> spellCheckingEngines;
  rv = GetEngineList(&spellCheckingEngines);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int32_t i = 0; i < spellCheckingEngines.Count(); i++) {
    // We must set mSpellCheckingEngine before we call SetDictionary, since
    // SetDictionary calls back to this spell checker to check if the
    // dictionary was set
    mSpellCheckingEngine = spellCheckingEngines[i];

    rv = mSpellCheckingEngine->SetDictionary(PromiseFlatString(aDictionary).get());

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<mozIPersonalDictionary> personalDictionary = do_GetService("@mozilla.org/spellchecker/personaldictionary;1");
      mSpellCheckingEngine->SetPersonalDictionary(personalDictionary.get());

      nsXPIDLString language;
      nsCOMPtr<mozISpellI18NManager> serv(do_GetService("@mozilla.org/spellchecker/i18nmanager;1", &rv));
      NS_ENSURE_SUCCESS(rv, rv);
      return serv->GetUtil(language.get(),getter_AddRefs(mConverter));
    }
  }

  mSpellCheckingEngine = nullptr;

  // We could not find any engine with the requested dictionary
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult
mozSpellChecker::SetupDoc(int32_t *outBlockOffset)
{
  nsresult  rv;

  nsITextServicesDocument::TSDBlockSelectionStatus blockStatus;
  int32_t selOffset;
  int32_t selLength;
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
    mFromStart = false;
  }
  return rv;
}


// utility method to discover which block we're in. The TSDoc interface doesn't give
// us this, because it can't assume a read-only document.
// shamelessly stolen from nsTextServicesDocument
nsresult
mozSpellChecker::GetCurrentBlockIndex(nsITextServicesDocument *aDoc, int32_t *outBlockIndex)
{
  int32_t  blockIndex = 0;
  bool     isDone = false;
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
  MOZ_ASSERT(!XRE_IsContentProcess());

  nsresult rv;
  bool hasMoreEngines;

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
