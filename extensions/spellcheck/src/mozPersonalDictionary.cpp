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
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein <Deinst@world.std.com>
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

#include "mozPersonalDictionary.h"
#include "nsIUnicharInputStream.h"
#include "nsReadableUtils.h"
#include "nsIFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsAVLTree.h"
#include "nsIObserverService.h"
#include "nsIPref.h"
#include "nsCRT.h"
#include "nsNetUtil.h"

#define MOZ_PERSONAL_DICT_NAME "persdict.dat"
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

const int kMaxWordLen=256;
#define spellchecker_savePref "spellchecker.savePDEverySession"
static PRBool SessionSave=PR_FALSE;

/**
 * This is the most braindead implementation of a personal dictionary possible.
 * There is not much complexity needed, though.  It could be made much faster,
 *  and probably should, but I don't see much need for more in terms of interface.
 *
 * Allowing personal words to be associated with only certain dictionaries maybe.
 *
 * TODO:
 * Implement the suggestion record.
 */


NS_IMPL_ISUPPORTS3(mozPersonalDictionary, mozIPersonalDictionary, nsIObserver, nsSupportsWeakReference);

/* AVL node functors */

/*
 * String comparitor for Unicode tree
 **/
class StringNodeComparitor: public nsAVLNodeComparitor {
public:
  StringNodeComparitor(){}
  virtual ~StringNodeComparitor(){}
  virtual PRInt32 operator() (void *item1,void *item2){
    return nsCRT::strcmp((PRUnichar *)item1,(PRUnichar *)item2);
  }
};

/*
 * String comparitor for charset tree
 **/
class CStringNodeComparitor: public nsAVLNodeComparitor {
public:
  CStringNodeComparitor(){}
  virtual ~CStringNodeComparitor(){}
  virtual PRInt32 operator() (void *item1,void *item2){
    return nsCRT::strcmp((char *)item1,(char *)item2);
  }
};
/*
 * the generic deallocator
 **/
class DeallocatorFunctor: public nsAVLNodeFunctor {
public:
  DeallocatorFunctor(){}
  virtual ~DeallocatorFunctor(){}
  virtual void* operator() (void * anItem){
    nsMemory::Free(anItem);
    return nsnull;
  }
};

static StringNodeComparitor *gStringNodeComparitor;
static CStringNodeComparitor *gCStringNodeComparitor;
static DeallocatorFunctor *gDeallocatorFunctor;

/*
 * Functor for copying the unicode tree to the charset tree with conversion
 **/
class ConvertedCopyFunctor: public nsAVLNodeFunctor {
public:
  ConvertedCopyFunctor(nsIUnicodeEncoder* anEncoder, nsAVLTree *aNewTree):newTree(aNewTree),encoder(anEncoder){
    res = NS_OK;
  }
  virtual ~ConvertedCopyFunctor(){}
  nsresult GetResult(){return res;}
  virtual void* operator() (void * anItem){
    if(NS_SUCCEEDED(res)){
      PRUnichar * word=(PRUnichar *) anItem;
      PRInt32 inLength,outLength;
      inLength = nsCRT::strlen(word);
      res = encoder->GetMaxLength(word,inLength,&outLength);
      if(NS_FAILED(res))
        return nsnull;
      char * tmp = (char *) nsMemory::Alloc(sizeof(PRUnichar *) * (outLength+1));
      res = encoder->Convert(word,&inLength,tmp,&outLength);
      if(res == NS_ERROR_UENC_NOMAPPING){
        res = NS_OK;   // word just doesnt fit in our charset -- assume that it is the wrong language.
        nsMemory::Free(tmp);
      }
      else{
        tmp[outLength]='\0';
        newTree->AddItem(tmp);
      }
    }
    return nsnull;
  }
protected:
  nsresult res;
  nsAVLTree *newTree;
  nsCOMPtr<nsIUnicodeEncoder> encoder;
};

/*
 * Copy the tree to a waiiting aray of Unichar pointers.
 * All the strings are newly created.  It is the callers responsibility to free them
 **/
class CopyToArrayFunctor: public nsAVLNodeFunctor {
public:
  CopyToArrayFunctor(PRUnichar **tabulaRasa){
    data = tabulaRasa;
    count =0;
    res = NS_OK;
  }
  virtual ~CopyToArrayFunctor(){}
  nsresult GetResult(){return res;}
  virtual void* operator() (void * anItem){
    PRUnichar * word=(PRUnichar *) anItem;
    if(NS_SUCCEEDED(res)){
      data[count]=ToNewUnicode(nsDependentString(word));
      if(!data[count]) res = NS_ERROR_OUT_OF_MEMORY;
      return (void*) (data[count++]);
    }
    return nsnull;
  }
protected:
  nsresult res;
  PRUnichar **data;
  PRUint32 count;
};

/*
 * Copy the tree to an open file.
 **/
class CopyToStreamFunctor: public nsAVLNodeFunctor {
public:
  CopyToStreamFunctor(nsIOutputStream *aStream):mStream(aStream){
    res = NS_OK;
  }
  virtual ~CopyToStreamFunctor(){}
  nsresult GetResult(){return res;}
  virtual void* operator() (void * anItem){
    nsString word((PRUnichar *) anItem);
    if(NS_SUCCEEDED(res)){
      PRUint32 bytesWritten;
      word.Append(NS_LITERAL_STRING("\n"));
      NS_ConvertUCS2toUTF8 UTF8word(word);
      res = mStream->Write(UTF8word.get(),UTF8word.Length(),&bytesWritten);
    }
    return nsnull;
  }
protected:
  nsresult res;
  nsIOutputStream* mStream;
};

mozPersonalDictionary::mozPersonalDictionary():mUnicodeTree(nsnull),mCharsetTree(nsnull),mUnicodeIgnoreTree(nsnull),mCharsetIgnoreTree(nsnull)
{
  NS_INIT_ISUPPORTS();

  NS_ASSERTION(!gStringNodeComparitor,"Someone's been writing in my statics! I'm a Singleton Bear!");
  if(!gStringNodeComparitor){
    gStringNodeComparitor = new StringNodeComparitor;
    gCStringNodeComparitor = new CStringNodeComparitor;
    gDeallocatorFunctor = new DeallocatorFunctor;
  }
}

mozPersonalDictionary::~mozPersonalDictionary()
{
  delete mUnicodeTree;
  delete mCharsetTree;
  delete mUnicodeIgnoreTree;
  delete mCharsetIgnoreTree;
}

int PR_CALLBACK
SpellcheckerSavePrefChanged(const char * newpref, void * data) 
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if(NS_SUCCEEDED(rv)&&prefs) {
    if(NS_FAILED(prefs->GetBoolPref(spellchecker_savePref,&SessionSave))){
      SessionSave = PR_TRUE;
    }
  }
  else{
    SessionSave = PR_TRUE;
  }
  return 0;
}

NS_IMETHODIMP mozPersonalDictionary::Init()
{
  nsresult rv;

  nsCOMPtr<nsIObserverService> svc = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv) && svc) {
    // Register as an oserver of shutdown
    rv = svc->AddObserver(this,   NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    // Register as an observer of profile changes
    if (NS_SUCCEEDED(rv))
      rv = svc->AddObserver(this, "profile-before-change", PR_TRUE);
    if (NS_SUCCEEDED(rv))
      rv = svc->AddObserver(this, "profile-after-change", PR_TRUE);
  }
  if(NS_FAILED(rv)) return rv;
   
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if(NS_SUCCEEDED(rv)&&prefs) {
    if(NS_FAILED(prefs->GetBoolPref(spellchecker_savePref, &SessionSave))){
      SessionSave = PR_TRUE;
    }
    prefs->RegisterCallback(spellchecker_savePref,SpellcheckerSavePrefChanged,nsnull);
  }
  else{
    SessionSave = PR_FALSE;
  }

  if(NS_FAILED(rv)) return rv;

  return Load();
}

/* attribute wstring language; */
NS_IMETHODIMP mozPersonalDictionary::GetLanguage(PRUnichar * *aLanguage)
{
  nsresult res=NS_OK;
  NS_PRECONDITION(aLanguage != nsnull, "null ptr");
  if(!aLanguage){
    res = NS_ERROR_NULL_POINTER;
  }
  else{
    *aLanguage = ToNewUnicode(mLanguage);
    if(!aLanguage) res = NS_ERROR_OUT_OF_MEMORY;
  }
  return res;
}

NS_IMETHODIMP mozPersonalDictionary::SetLanguage(const PRUnichar * aLanguage)
{
  mLanguage = aLanguage;
  return NS_OK;
}

/* attribute wstring charset; */
NS_IMETHODIMP mozPersonalDictionary::GetCharset(PRUnichar * *aCharset)
{
  nsresult res=NS_OK;
  NS_PRECONDITION(aCharset != nsnull, "null ptr");
  if(!aCharset){
    res = NS_ERROR_NULL_POINTER;
  }
  else{
    *aCharset = ToNewUnicode(mLanguage);
    if(!aCharset) res = NS_ERROR_OUT_OF_MEMORY;
  }
  return res;
}

NS_IMETHODIMP mozPersonalDictionary::SetCharset(const PRUnichar * aCharset)
{
  nsresult res;
  mCharset = aCharset;

  nsCAutoString convCharset;
  convCharset.AssignWithConversion(mCharset); 

  nsCOMPtr<nsICharsetConverterManager> ccm = do_GetService(kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) return res;
  if(!ccm) return NS_ERROR_FAILURE;

  res=ccm->GetUnicodeEncoder(convCharset.get(),getter_AddRefs(mEncoder));

  if (NS_FAILED(res)) return res;
  if(!mEncoder) return NS_ERROR_FAILURE;

  if(mEncoder && NS_SUCCEEDED(res)){
    res=mEncoder->SetOutputErrorBehavior(mEncoder->kOnError_Signal,nsnull,'?');
  }

  if(mEncoder && mUnicodeTree){
    delete mCharsetTree;
    mCharsetTree = new nsAVLTree(*gCStringNodeComparitor,gDeallocatorFunctor);
    ConvertedCopyFunctor converter(mEncoder,mCharsetTree);
    mUnicodeTree->ForEachDepthFirst(converter);
  }
  if(mEncoder && mUnicodeIgnoreTree){
    delete mCharsetIgnoreTree;
    mCharsetIgnoreTree = new nsAVLTree(*gCStringNodeComparitor,gDeallocatorFunctor);
    ConvertedCopyFunctor converter(mEncoder,mCharsetIgnoreTree);
    mUnicodeIgnoreTree->ForEachDepthFirst(converter);
  }
  return res;
}

/* void Load (); */
NS_IMETHODIMP mozPersonalDictionary::Load()
{
  //FIXME Deinst  -- get dictionary name from prefs;
  nsresult res;
  nsCOMPtr<nsIFile> theFile;
  PRBool dictExists;

  res = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(theFile));
  if(NS_FAILED(res)) return res;
  if(!theFile)return NS_ERROR_FAILURE;
  res = theFile->Append(NS_LITERAL_STRING(MOZ_PERSONAL_DICT_NAME));
  if(NS_FAILED(res)) return res;
  res = theFile->Exists(&dictExists);
  if(NS_FAILED(res)) return res;

  if(!dictExists) {
    //create new user dictionary
    nsCOMPtr<nsIOutputStream> outStream;
    NS_NewLocalFileOutputStream(getter_AddRefs(outStream), theFile, PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE ,0664);

    CopyToStreamFunctor writer(outStream);
    if(NS_FAILED(res)) return res;
    if(!outStream)return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIInputStream> inStream;
  NS_NewLocalFileInputStream(getter_AddRefs(inStream), theFile);
  nsCOMPtr<nsIUnicharInputStream> convStream;
  res = NS_NewUTF8ConverterStream(getter_AddRefs(convStream), inStream, 0);
  if(NS_FAILED(res)) return res;
  
  // we're rereading get rid of the old data  -- we shouldn't have any, but...
  delete mUnicodeTree;
  mUnicodeTree = new nsAVLTree(*gStringNodeComparitor,gDeallocatorFunctor);

  PRUnichar c;
  PRUint32 nRead;
  PRBool done = PR_FALSE;
  do{  // read each line of text into the string array.
    if( (NS_OK != convStream->Read(&c, 1, &nRead)) || (nRead != 1)) break;
    while(!done && ((c == '\n') || (c == '\r'))){
      if( (NS_OK != convStream->Read(&c, 1, &nRead)) || (nRead != 1)) done = PR_TRUE;
    }
    if (!done){ 
      nsAutoString word;
      while((c != '\n') && (c != '\r') && !done){
        word.Append(c);
        if( (NS_OK != convStream->Read(&c, 1, &nRead)) || (nRead != 1)) done = PR_TRUE;
      }
      mUnicodeTree->AddItem((void *)ToNewUnicode(word));
    }
  }while(!done);
  mDirty = PR_FALSE;
  
  return res;
}

/* void Save (); */
NS_IMETHODIMP mozPersonalDictionary::Save()
{
  nsCOMPtr<nsIFile> theFile;
  nsresult res;

  if(!mDirty) return NS_OK;
  //FIXME Deinst  -- get dictionary name from prefs;

  res = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(theFile));
  if(NS_FAILED(res)) return res;
  if(!theFile)return NS_ERROR_FAILURE;
  res = theFile->Append(NS_LITERAL_STRING(MOZ_PERSONAL_DICT_NAME));
  if(NS_FAILED(res)) return res;

  nsCOMPtr<nsIOutputStream> outStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(outStream), theFile, PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE ,0664);

  CopyToStreamFunctor writer(outStream);
  if(NS_FAILED(res)) return res;
  if(!outStream)return NS_ERROR_FAILURE;
  if (mUnicodeTree) mUnicodeTree->ForEach(writer);
  mDirty = PR_FALSE;
  return NS_OK;
}

/* void GetWordList ([array, size_is (count)] out wstring words, out PRUint32 count); */
NS_IMETHODIMP mozPersonalDictionary::GetWordList(PRUnichar ***words, PRUint32 *count)
{
  if(!words || !count) 
    return NS_ERROR_NULL_POINTER;
  *words=0;
  *count=0;
  PRUnichar **tmpPtr = 0;
  if(!mUnicodeTree){
    return NS_OK;
  }
  tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * (mUnicodeTree->GetCount()));
  if (!tmpPtr)
    return NS_ERROR_OUT_OF_MEMORY;
  CopyToArrayFunctor pitneyBowes(tmpPtr);
  mUnicodeTree->ForEach(pitneyBowes);

  nsresult res = pitneyBowes.GetResult();
  if(NS_SUCCEEDED(res)){
    *count = mUnicodeTree->GetCount();
    *words = tmpPtr;
  }
  return res;
}

/* boolean CheckUnicode (in wstring word); */
NS_IMETHODIMP mozPersonalDictionary::CheckUnicode(const PRUnichar *word, PRBool *_retval)
{
  if(!word || !_retval || !mUnicodeTree)
    return NS_ERROR_NULL_POINTER;
  if(mUnicodeTree->FindItem((void *)word)){
    *_retval = PR_TRUE;
  }
  else{
    if(mUnicodeIgnoreTree&&mUnicodeIgnoreTree->FindItem((void *)word)){
      *_retval = PR_TRUE;
    }
    else{
      *_retval = PR_FALSE;
    }
  }
  return NS_OK;
}

/* boolean Check (in string word); */
NS_IMETHODIMP mozPersonalDictionary::Check(const char *word, PRBool *_retval)
{
  if(!word || !_retval || !mCharsetTree)
    return NS_ERROR_NULL_POINTER;
  if(mCharsetTree->FindItem((void *)word)){
    *_retval = PR_TRUE;
  }
  else{
    if(mCharsetIgnoreTree&&mCharsetIgnoreTree->FindItem((void *)word)){
      *_retval = PR_TRUE;
    }
    else{
      *_retval = PR_FALSE;
    }
  }
  return NS_OK;
}

/* void AddWord (in wstring word); */
NS_IMETHODIMP mozPersonalDictionary::AddWord(const PRUnichar *word, const PRUnichar *lang)
{
  nsAutoString temp(word);
  if(mUnicodeTree) mUnicodeTree->AddItem(ToNewUnicode(nsDependentString(word)));
  mDirty=PR_TRUE;

  nsresult res=NS_OK;
  if(mCharsetTree&&mEncoder){
    PRInt32 inLength,outLength;
    inLength = nsCRT::strlen(word);
    res = mEncoder->GetMaxLength(word,inLength,&outLength);
    if(NS_FAILED(res))
      return res;
    char * tmp = (char *) nsMemory::Alloc(sizeof(PRUnichar *) * (outLength+1));
    res = mEncoder->Convert(word,&inLength,tmp,&outLength);
    if(NS_FAILED(res))
      return res;
    tmp[outLength]='\0';
    mCharsetTree->AddItem(tmp);
  }

  return res;
}

/* void RemoveWord (in wstring word); */
NS_IMETHODIMP mozPersonalDictionary::RemoveWord(const PRUnichar *word, const PRUnichar *lang)
{
  nsAutoString temp(word);
  if(mUnicodeTree) mUnicodeTree->RemoveItem((void *)word);
  mDirty=PR_TRUE;

  nsresult res=NS_OK;
  if(mCharsetTree&&mEncoder){
    PRInt32 inLength,outLength;
    inLength = nsCRT::strlen(word);
    res = mEncoder->GetMaxLength(word,inLength,&outLength);
    if(NS_FAILED(res))
      return res;
    char * tmp = (char *) nsMemory::Alloc(sizeof(PRUnichar *) * (outLength+1));
    res = mEncoder->Convert(word,&inLength,tmp,&outLength);
    if(NS_FAILED(res))
      return res;
    tmp[outLength]='\0';
    mCharsetTree->AddItem(tmp);
  }

  return res;
}
/* void IgnoreWord (in wstring word); */
NS_IMETHODIMP mozPersonalDictionary::IgnoreWord(const PRUnichar *word)
{
  if(!mUnicodeIgnoreTree){
    mUnicodeIgnoreTree=new nsAVLTree(*gStringNodeComparitor,gDeallocatorFunctor);
  }
  if(!mUnicodeIgnoreTree){
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mUnicodeIgnoreTree->AddItem(ToNewUnicode(nsDependentString(word)));
  if(!mCharsetIgnoreTree){
    mCharsetIgnoreTree=new nsAVLTree(*gCStringNodeComparitor,gDeallocatorFunctor);
  }
  if(!mCharsetIgnoreTree){
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if(mCharsetIgnoreTree&&mEncoder){
    PRInt32 inLength,outLength;
    nsresult res;
    inLength = nsCRT::strlen(word);
    res = mEncoder->GetMaxLength(word,inLength,&outLength);
    if(NS_FAILED(res))
      return res;
    char * tmp = (char *) nsMemory::Alloc(sizeof(PRUnichar *) * (outLength+1));
    res = mEncoder->Convert(word,&inLength,tmp,&outLength);
    if(NS_FAILED(res))
      return res;
    tmp[outLength]='\0';
    mCharsetIgnoreTree->AddItem(tmp);
  }
  
  return NS_OK;
}

/* void EndSession (); */
NS_IMETHODIMP mozPersonalDictionary::EndSession()
{
  if(SessionSave)Save();
  delete mUnicodeIgnoreTree;
  delete mCharsetIgnoreTree;
  mUnicodeIgnoreTree=nsnull;
  mCharsetIgnoreTree=nsnull;
    return NS_OK;
}

/* void AddCorrection (in wstring word, in wstring correction); */
NS_IMETHODIMP mozPersonalDictionary::AddCorrection(const PRUnichar *word, const PRUnichar *correction, const PRUnichar *lang)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void RemoveCorrection (in wstring word, in wstring correction); */
NS_IMETHODIMP mozPersonalDictionary::RemoveCorrection(const PRUnichar *word, const PRUnichar *correction, const PRUnichar *lang)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GetCorrection (in wstring word, [array, size_is (count)] out wstring words, out PRUint32 count); */
NS_IMETHODIMP mozPersonalDictionary::GetCorrection(const PRUnichar *word, PRUnichar ***words, PRUint32 *count)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void observe (in nsISupports aSubject, in string aTopic, in wstring aData); */
NS_IMETHODIMP mozPersonalDictionary::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    Save();
    delete mUnicodeTree;
    delete mCharsetTree;
    delete mUnicodeIgnoreTree;
    delete mCharsetIgnoreTree;
    mUnicodeTree=nsnull;
    mCharsetTree=nsnull;
    mUnicodeIgnoreTree=nsnull;
    mCharsetIgnoreTree=nsnull;
  }
  else if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Save();
    delete mUnicodeTree;
    delete mCharsetTree;
    delete mUnicodeIgnoreTree;
    delete mCharsetIgnoreTree;
    mUnicodeTree=nsnull;
    mCharsetTree=nsnull;
    mUnicodeIgnoreTree=nsnull;
    mCharsetIgnoreTree=nsnull;
  }
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    Load();
  }
  return NS_OK;
}

