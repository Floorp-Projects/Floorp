/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#define NS_IMPL_IDS

#include "nsString.h"
#include "nsIRegistry.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsUConvDll.h"

// just for CIDs
#include "nsIUnicodeDecodeHelper.h"
#include "nsIUnicodeEncodeHelper.h"

static NS_DEFINE_IID(kRegistryNodeIID, NS_IREGISTRYNODE_IID);

//----------------------------------------------------------------------------
// Class nsObject [declaration]

class nsObject
{
public: 
  virtual ~nsObject();
};

//----------------------------------------------------------------------------
// Class nsObjectArray [declaration]

class nsObjectArray
{
private:

  nsObject **   mArray;
  PRInt32       mCapacity;
  PRInt32       mUsage;

  void Init(PRInt32 aCapacity);

public: 
  nsObjectArray();
  nsObjectArray(PRInt32 aCapacity);
  ~nsObjectArray();

  nsObject ** GetArray();
  PRInt32 GetUsage();
  nsresult InsureCapacity(PRInt32 aCapacity);
  nsresult AddObject(nsObject * aObject);
};

//----------------------------------------------------------------------------
// Class nsConverterInfo [declaration]

class nsConverterInfo : public nsObject
{
public: 
  nsString *    mName;
  nsCID         mCID;
  PRInt32       mFlags;

  nsConverterInfo();
  ~nsConverterInfo();
};

//----------------------------------------------------------------------------
// Class nsCharsetConverterManager [declaration]

/**
 * The actual implementation of the nsICharsetConverterManager interface.
 *
 * @created         15/Nov/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsCharsetConverterManager : public nsICharsetConverterManager
{
  NS_DECL_ISUPPORTS

private:

  nsObjectArray mDecoderArray;
  nsObjectArray mEncoderArray;

  /**
   * Takes charset information from Registry and puts it into those arrays.
   */
  void FillInfoArrays();
  void FillConverterProperties(nsObjectArray * aArray);

  /**
   * Returns NULL if charset could not be found.
   */
  nsConverterInfo * GetConverterInfo(nsObjectArray * aArray, 
      nsString * aName);

  nsresult GetConverterList(nsObjectArray * aArray, nsString *** aResult, 
      PRInt32 * aCount);

  static nsresult RegisterConverterPresenceData(char * aRegistryPath, 
      PRBool aPresence);

public:

  nsCharsetConverterManager();
  virtual ~nsCharsetConverterManager();
  static nsresult RegisterConverterManagerData();

  //--------------------------------------------------------------------------
  // Interface nsICharsetConverterManager [declaration]

  NS_IMETHOD GetUnicodeEncoder(const nsString * aDest, 
      nsIUnicodeEncoder ** aResult);
  NS_IMETHOD GetUnicodeDecoder(const nsString * aSrc, 
      nsIUnicodeDecoder ** aResult);
  NS_IMETHOD GetDecoderList(nsString *** aResult, PRInt32 * aCount);
  NS_IMETHOD GetEncoderList(nsString *** aResult, PRInt32 * aCount);
  NS_IMETHOD GetDecoderFlags(nsString * aName, PRInt32 * aFlags);
  NS_IMETHOD GetEncoderFlags(nsString * aName, PRInt32 * aFlags);
};

//----------------------------------------------------------------------------
// Global functions and data [implementation]

NS_IMETHODIMP NS_NewCharsetConverterManager(nsISupports* aOuter, 
                                            const nsIID& aIID,
                                            void** aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCharsetConverterManager * inst = new nsCharsetConverterManager();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) {
    *aResult = nsnull;
    delete inst;
  }
  return res;
}

//----------------------------------------------------------------------------
// Class nsObject [implementation]

// XXX clarify why I can't have this method as a pure virtual one?!
nsObject::~nsObject()
{
}

//----------------------------------------------------------------------------
// Class nsObjectsArray [implementation]

nsObjectArray::nsObjectArray(PRInt32 aCapacity)
{
  Init(aCapacity);
}

nsObjectArray::nsObjectArray()
{
  Init(64);
}

nsObjectArray::~nsObjectArray()
{
  for (PRInt32 i = 0; i < mUsage; i++) delete mArray[i];
  if (mArray != NULL) delete [] mArray;
}

nsObject ** nsObjectArray::GetArray()
{
  return mArray;
}

PRInt32 nsObjectArray::GetUsage()
{
  return mUsage;
}

void nsObjectArray::Init(PRInt32 aCapacity)
{
  mCapacity = aCapacity;
  mUsage = 0;
  mArray = NULL;
}

nsresult nsObjectArray::InsureCapacity(PRInt32 aCapacity)
{
  PRInt32 i;

  if (mArray == NULL) {
    mArray = new nsObject * [mCapacity];
    if (mArray == NULL) return NS_ERROR_OUT_OF_MEMORY;
  }

  if (aCapacity > mCapacity) {
    while (aCapacity > mCapacity) mCapacity *= 2;
    nsObject ** newArray = new nsObject * [mCapacity];
    if (newArray == NULL) return NS_ERROR_OUT_OF_MEMORY;
    for (i = 0; i < mUsage; i++) newArray[i] = mArray[i];
    delete [] mArray;
    mArray = newArray;
  }

  return NS_OK;
}

nsresult nsObjectArray::AddObject(nsObject * aObject)
{
  nsresult res;

  res = InsureCapacity(mUsage + 1);
  if (NS_FAILED(res)) return res;

  (mArray)[mUsage++] = aObject;
  return NS_OK;
}

//----------------------------------------------------------------------------
// Class nsConverterInfo [implementation]

nsConverterInfo::nsConverterInfo()
{
  mName = NULL;
}

nsConverterInfo::~nsConverterInfo()
{
  if (mName != NULL) delete mName;
}

//----------------------------------------------------------------------------
// Class nsCharsetConverterManager [implementation]

NS_IMPL_ISUPPORTS(nsCharsetConverterManager, nsICharsetConverterManager::GetIID());

nsCharsetConverterManager::nsCharsetConverterManager() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);

  FillInfoArrays();
  FillConverterProperties(&mDecoderArray);
  FillConverterProperties(&mEncoderArray);
}

nsCharsetConverterManager::~nsCharsetConverterManager() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

nsresult nsCharsetConverterManager::RegisterConverterManagerData()
{
  // XXX take these konstants out of here
  // XXX change "xuconv" to "uconv" when the new enc&dec trees are in place
  RegisterConverterPresenceData("software/netscape/intl/xuconv/not-for-browser", PR_FALSE);

  return NS_OK;
}

nsresult nsCharsetConverterManager::RegisterConverterPresenceData(
                                    char * aRegistryPath, 
                                    PRBool aPresence)
{
  nsresult res;
  nsIRegistry * registry = NULL;
  nsRegistryKey key;

  // get the registry
  res = nsServiceManager::GetService(NS_REGISTRY_PROGID, 
    nsIRegistry::GetIID(), (nsISupports**)&registry);
  if (NS_FAILED(res)) goto done;

  // open the registry
  res = registry->OpenWellKnownRegistry(
    nsIRegistry::ApplicationComponentRegistry);
  if (NS_FAILED(res)) goto done;

  res = registry -> AddSubtree(nsIRegistry::Common, aRegistryPath, &key);
  if (NS_FAILED(res)) goto done;

  // XXX instead of these hardcoded values, I should read from a property file
  res = registry -> SetInt(key, "x-fake-1999", 1);
  if (NS_FAILED(res)) goto done;
  res = registry -> SetInt(key, "x-fake-2000", 1);
  if (NS_FAILED(res)) goto done;
  res = registry -> SetInt(key, "x-euc-tw", 1);
  if (NS_FAILED(res)) goto done;

done:
  if (registry != NULL) {
    registry -> Close();
    nsServiceManager::ReleaseService(NS_REGISTRY_PROGID, registry);
  }

  return res;
}

// XXX rethink the registry structure(tree) for these converters
// The idea is to have two trees:
// .../uconv/decoder/(CID/name)
// .../uconv/encoder/(CID/name)
// XXX take the registry strings out and make them macros
void nsCharsetConverterManager::FillInfoArrays() 
{
  nsresult res = NS_OK;
  nsIEnumerator * components = NULL;
  nsIRegistry * registry = NULL;
  nsRegistryKey uconvKey, key;

  // get the registry
  res = nsServiceManager::GetService(NS_REGISTRY_PROGID, 
    nsIRegistry::GetIID(), (nsISupports**)&registry);
  if (NS_FAILED(res)) goto done;

  // open the registry
  res = registry->OpenWellKnownRegistry(
    nsIRegistry::ApplicationComponentRegistry);
  if (NS_FAILED(res)) goto done;

  // get subtree
  res = registry->GetSubtree(nsIRegistry::Common,  
    "software/netscape/intl/uconv", &uconvKey);
  if (NS_FAILED(res)) goto done;

  // enumerate subtrees
  res = registry->EnumerateSubtrees(uconvKey, &components);
  if (NS_FAILED(res)) goto done;
  res = components->First();
  if (NS_FAILED(res)) goto done;

  while (NS_OK != components->IsDone()) {
    nsISupports * base = NULL;
    nsIRegistryNode * node = NULL;
    char * name = NULL;
    char * src = NULL;
    char * dest = NULL;
    nsConverterInfo * ci = NULL;

    res = components->CurrentItem(&base);
    if (NS_FAILED(res)) goto done1;

    res = base->QueryInterface(kRegistryNodeIID, (void**)&node);
    if (NS_FAILED(res)) goto done1;

    res = node->GetName(&name);
    if (NS_FAILED(res)) goto done1;

    ci = new nsConverterInfo();
    if (ci == NULL) goto done1;
    if (!(ci->mCID.Parse(name))) goto done1;

    res = node->GetKey(&key);
    if (NS_FAILED(res)) goto done1;

    res = registry->GetString(key, "source", &src);
    if (NS_FAILED(res)) goto done1;

    res = registry->GetString(key, "destination", &dest);
    if (NS_FAILED(res)) goto done1;

    if (!strcmp(src, "Unicode")) {
      ci->mName = new nsString(dest);
      mEncoderArray.AddObject(ci);
    } else if (!strcmp(dest, "Unicode")) {
      ci->mName = new nsString(src);
      mDecoderArray.AddObject(ci);
    } else goto done1;

    ci = NULL;

done1:
    NS_IF_RELEASE(base);
    NS_IF_RELEASE(node);

    if (name != NULL) nsCRT::free(name);
    if (src != NULL) nsCRT::free(src);
    if (dest != NULL) nsCRT::free(dest);
    if (ci != NULL) delete ci;

    res = components->Next();
    if (NS_FAILED(res)) break; // this is NOT supposed to fail!
  }

  // finish and clean up
done:
  if (registry != NULL) {
    registry->Close();
    nsServiceManager::ReleaseService(NS_REGISTRY_PROGID, registry);
  }

  NS_IF_RELEASE(components);
}

void nsCharsetConverterManager::FillConverterProperties(
                                nsObjectArray * aArray)
{
  PRInt32 size = aArray->GetUsage();
  nsConverterInfo ** array = (nsConverterInfo **)aArray->GetArray();
  if ((size == 0) || (array == NULL)) return;

  for (PRInt32 i = 0; i < size; i++) {
    int val = 0;
    SET_FOR_BROWSER(val);
    SET_FOR_MAILNEWSEDITOR(val);
    array[i]->mFlags = val;
  }

  // XXX get these properties from registry, not from your stomach
}

// XXX optimise this method - use some hash tables for God's sake!
// That could also mean taking the name out of the data structure. Maybe.
nsConverterInfo * nsCharsetConverterManager::GetConverterInfo(
                                             nsObjectArray * aArray, 
                                             nsString * aName) 
{
  PRInt32 size = aArray->GetUsage();
  nsConverterInfo ** array = (nsConverterInfo **)aArray->GetArray();
  if ((size == 0) || (array == NULL)) return NULL;

  for (PRInt32 i = 0; i < size; i++) if (aName->Equals(*(array[i]->mName))) 
    return array[i];

  return NULL;
}

nsresult nsCharsetConverterManager::GetConverterList(
                                    nsObjectArray * aArray,
                                    nsString *** aResult,
                                    PRInt32 * aCount)
{
  *aResult = NULL;
  *aCount = 0;

  PRInt32 size = aArray->GetUsage();
  nsConverterInfo ** array = (nsConverterInfo **)aArray->GetArray();
  if ((size == 0) || (array == NULL)) return NS_OK;

  *aResult = new nsString * [size];
  if (*aResult == NULL) return NS_ERROR_OUT_OF_MEMORY;

  *aCount = size;
  for (PRInt32 i=0;i<size;i++) (*aResult)[i] = array[i]->mName;

  return NS_OK;

  // XXX also create new Strings here, as opposed to just providing pointers 
  // to the existing ones
}

//----------------------------------------------------------------------------
// Interface nsICharsetConverterManager [implementation]

NS_IMETHODIMP nsCharsetConverterManager::GetUnicodeEncoder(
                                         const nsString * aDest, 
                                         nsIUnicodeEncoder ** aResult)
{
  *aResult= nsnull;
  nsIComponentManager* comMgr;
  nsresult res;
  res = NS_GetGlobalComponentManager(&comMgr);
  if(NS_FAILED(res))
     return res;
  PRInt32 baselen = nsCRT::strlen(NS_UNICODEENCODER_PROGID_BASE);
  char progid[256];
  PL_strncpy(progid, NS_UNICODEENCODER_PROGID_BASE, 256);
  aDest->ToCString(progid + baselen, 256 - baselen);
  res = comMgr->CreateInstanceByProgID(progid,NULL,
                 kIUnicodeEncoderIID ,(void**)aResult);
  if(NS_FAILED(res))
    res = NS_ERROR_UCONV_NOCONV;
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetUnicodeDecoder(
                                         const nsString * aSrc, 
                                         nsIUnicodeDecoder ** aResult)
{
  *aResult= nsnull;
  nsIComponentManager* comMgr;
  nsresult res;
  res = NS_GetGlobalComponentManager(&comMgr);
  if(NS_FAILED(res))
     return res;
  PRInt32 baselen = nsCRT::strlen(NS_UNICODEDECODER_PROGID_BASE);
  char progid[256];
  PL_strncpy(progid, NS_UNICODEDECODER_PROGID_BASE, 256);
  aSrc->ToCString(progid + baselen, 256 - baselen);
  res = comMgr->CreateInstanceByProgID(progid,NULL,
                 kIUnicodeDecoderIID,(void**)aResult);
  if(NS_FAILED(res))
    res = NS_ERROR_UCONV_NOCONV;
  return res;
}

NS_IMETHODIMP nsCharsetConverterManager::GetDecoderList(nsString *** aResult, 
                                                        PRInt32 * aCount)
{
  return GetConverterList(&mDecoderArray, aResult, aCount);
}

NS_IMETHODIMP nsCharsetConverterManager::GetEncoderList(nsString *** aResult, 
                                                        PRInt32 * aCount)
{
  return GetConverterList(&mEncoderArray, aResult, aCount);
}

NS_IMETHODIMP nsCharsetConverterManager::GetDecoderFlags(nsString * aName, 
                                                         PRInt32 * aFlags)
{
  *aFlags = 0;

  nsConverterInfo * info = GetConverterInfo(&mDecoderArray, aName);
  if (info == NULL) return NS_ERROR_UCONV_NOCONV;

  *aFlags = info->mFlags;
  return NS_OK;
}

NS_IMETHODIMP nsCharsetConverterManager::GetEncoderFlags(nsString * aName, 
                                                         PRInt32 * aFlags)
{
  *aFlags = 0;

  nsConverterInfo * info = GetConverterInfo(&mEncoderArray, aName);
  if (info == NULL) return NS_ERROR_UCONV_NOCONV;

  *aFlags = info->mFlags;
  return NS_OK;
}
