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

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIRegistry.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "rdf.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsUConvDll.h"
#include "nsCharsetMenu.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_IID(kIRDFDataSourceIID, NS_IRDFDATASOURCE_IID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kIRDFServiceIID, NS_IRDFSERVICE_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID, NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);
static NS_DEFINE_IID(kRegistryNodeIID, NS_IREGISTRYNODE_IID);

static const char kURINC_CharsetMenuRoot[] = "NC:CharsetMenuRoot";
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);

static  nsIRDFService* gRDFService = nsnull; // XXX why is this global?!

//----------------------------------------------------------------------------
// Class CharsetInfo [declaration]

class CharsetInfo
{
public: 
  nsString * mName;
  nsCID mCID;

  CharsetInfo();
  ~CharsetInfo();
};

//----------------------------------------------------------------------------
// Class nsCharsetMenu [declaration]

/**
 * The Charset Menu component.
 *
 * @created         17/Sep/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsCharsetMenu : public nsIRDFDataSource
{
  NS_DECL_ISUPPORTS

private:

  static nsIRDFDataSource * mInner;
  static nsIRDFResource * kNC_CharsetMenuRoot;
  static nsIRDFResource * kNC_Name;

  CharsetInfo **    mInfoArray;
  PRInt32           mInfoCapacity;
  PRInt32           mInfoSize;

  nsresult Init();

  void CreateDefaultCharsetList(CharsetInfo *** aArray, PRInt32 * aCapacity, 
      PRInt32 * aSize);

  void InitInfoArray(void);
  void DeleteInfoArray(void);
  void AddToInfoArray(CharsetInfo *** aArray, PRInt32 * aCapacity, 
      PRInt32 * aSize, CharsetInfo * aRecord);

  void CreateCharsetMenu(CharsetInfo ** aArray, PRInt32 aSize);

public:

  /**
   * Class constructor.
   */
  nsCharsetMenu();

  /**
   * Class destructor.
   */
  virtual ~nsCharsetMenu();

  //--------------------------------------------------------------------------
  // Interface nsIRDFDataSource [declaration]

  // XXX format and rename arguments, take implementations outside

  NS_IMETHOD GetURI(char ** uri)
  {
    if (!uri) return NS_ERROR_NULL_POINTER;

    *uri = nsXPIDLCString::Copy("rdf:charset-menu");
    if (!(*uri)) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
  }

  NS_IMETHOD GetSource(nsIRDFResource* property,
                       nsIRDFNode* target,
                       PRBool tv,
                       nsIRDFResource** source)
  {
      return mInner->GetSource(property, target, tv, source);
  }

  NS_IMETHOD GetSources(nsIRDFResource* property,
                        nsIRDFNode* target,
                        PRBool tv,
                        nsISimpleEnumerator** sources)
  {
      return mInner->GetSources(property, target, tv, sources);
  }

  NS_IMETHOD GetTarget(nsIRDFResource* source,
                       nsIRDFResource* property,
                       PRBool tv,
                       nsIRDFNode** target)
  {
    return mInner->GetTarget(source, property, tv, target);
  }

  NS_IMETHOD GetTargets(nsIRDFResource* source,
                        nsIRDFResource* property,
                        PRBool tv,
                        nsISimpleEnumerator** targets)
  {
      return mInner->GetTargets(source, property, tv, targets);
  }

  NS_IMETHOD Assert(nsIRDFResource* aSource, 
                    nsIRDFResource* aProperty, 
                    nsIRDFNode* aTarget,
                    PRBool aTruthValue);

  NS_IMETHOD Unassert(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget);

  NS_IMETHOD Change(nsIRDFResource* aSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aOldTarget,
                    nsIRDFNode* aNewTarget);

  NS_IMETHOD Move(nsIRDFResource* aOldSource,
                  nsIRDFResource* aNewSource,
                  nsIRDFResource* aProperty,
                  nsIRDFNode* aTarget);

  NS_IMETHOD HasAssertion(nsIRDFResource* source,
                          nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          PRBool* hasAssertion)
  {
      return mInner->HasAssertion(source, property, target, tv, hasAssertion);
  }

  NS_IMETHOD AddObserver(nsIRDFObserver* n)
  {
      return mInner->AddObserver(n);
  }

  NS_IMETHOD RemoveObserver(nsIRDFObserver* n)
  {
      return mInner->RemoveObserver(n);
  }

  NS_IMETHOD ArcLabelsIn( nsIRDFNode* node, nsISimpleEnumerator** labels)
  {
      return mInner->ArcLabelsIn(node, labels);
  }

  NS_IMETHOD ArcLabelsOut(nsIRDFResource* source, nsISimpleEnumerator** labels)
  {
      return mInner->ArcLabelsOut(source, labels);
  }

  NS_IMETHOD GetAllResources(nsISimpleEnumerator** aCursor)
  {
      return mInner->GetAllResources(aCursor);
  }

  NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                            nsIEnumerator** commands);
  NS_IMETHOD GetAllCmds(nsIRDFResource* source,
                            nsISimpleEnumerator** commands);

  NS_IMETHOD IsCommandEnabled(nsISupportsArray* aSources,
                              nsIRDFResource*   aCommand,
                              nsISupportsArray* aArguments,
                              PRBool* aResult);

  NS_IMETHOD DoCommand(nsISupportsArray* aSources,
                       nsIRDFResource*   aCommand,
                       nsISupportsArray* aArguments);
};  

//----------------------------------------------------------------------------
// Class nsCharsetInfo [implementation]

CharsetInfo::CharsetInfo()
{
  mName = NULL;
}

CharsetInfo::~CharsetInfo()
{
  if (mName != NULL) delete mName;
}

//----------------------------------------------------------------------------
// Class nsCharsetMenu [implementation]

NS_IMPL_ISUPPORTS(nsCharsetMenu, nsCOMTypeInfo<nsIRDFDataSource>::GetIID());

nsIRDFDataSource * nsCharsetMenu::mInner = NULL;
nsIRDFResource * nsCharsetMenu::kNC_CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Name = NULL;

nsCharsetMenu::nsCharsetMenu() 
{
  InitInfoArray();

  Init();
  CreateDefaultCharsetList(&mInfoArray, &mInfoCapacity, &mInfoSize);
  CreateCharsetMenu(mInfoArray, mInfoSize);

  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsCharsetMenu::~nsCharsetMenu() 
{
  DeleteInfoArray();

  if (gRDFService) {
    gRDFService->UnregisterDataSource(this);
    nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
    gRDFService = nsnull;
  }

  NS_IF_RELEASE(kNC_CharsetMenuRoot);
  NS_IF_RELEASE(kNC_Name);
  NS_IF_RELEASE(mInner);

  PR_AtomicDecrement(&g_InstanceCount);
}

void nsCharsetMenu::CreateDefaultCharsetList(CharsetInfo *** aArray, 
                                             PRInt32 * aCapacity,
                                             PRInt32 * aSize)
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
    CharsetInfo * ci = NULL;

    res = components->CurrentItem(&base);
    if (NS_FAILED(res)) goto done1;

    res = base->QueryInterface(kRegistryNodeIID, (void**)&node);
    if (NS_FAILED(res)) goto done1;

    res = node->GetName(&name);
    if (NS_FAILED(res)) goto done1;

    ci = new CharsetInfo();
    if (ci == NULL) goto done1;
    if (!(ci->mCID.Parse(name))) goto done1;

    res = node->GetKey(&key);
    if (NS_FAILED(res)) goto done1;

    res = registry->GetString(key, "source", &src);
    if (NS_FAILED(res)) goto done1;

    res = registry->GetString(key, "destination", &dest);
    if (NS_FAILED(res)) goto done1;

    // this is hardcoded for decoders; the encoders case is commented out
    /*if (!strcmp(src, "Unicode")) {
      ci->mName = new nsString(dest);
    } else*/ if (!strcmp(dest, "Unicode")) {
      ci->mName = new nsString(src);
    } else goto done1;

    // insert new created Info in the list and then NULL the ptr.
    AddToInfoArray(aArray, aCapacity, aSize, ci);
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

void nsCharsetMenu::InitInfoArray(void)
{
  mInfoArray = NULL;
  mInfoCapacity = 0;
  mInfoSize = 0;
}

void nsCharsetMenu::DeleteInfoArray(void)
{
  for (int i = 0; i < mInfoSize; i++) delete mInfoArray[i];
  if (mInfoArray != NULL) delete [] mInfoArray;
}

void nsCharsetMenu::AddToInfoArray(CharsetInfo *** aArray, 
                                   PRInt32 * aCapacity, 
                                   PRInt32 * aSize, 
                                   CharsetInfo * aRecord)
{
  int i;

  // assure existence of data structures
  if ((*aArray == NULL) || (*aCapacity <= 0)) {
    *aCapacity = 8;
    *aArray = new CharsetInfo * [*aCapacity];
    *aSize = 0;
  }

  // if full, double
  if (*aSize >= *aCapacity) {
    *aCapacity *= 2;
    CharsetInfo ** newArray = new CharsetInfo * [*aCapacity];
    for (i = 0; i < *aSize; i++) newArray[i] = (*aArray)[i];
    delete [] *aArray;
    *aArray = newArray;
  }

  // add element
  (*aArray)[(*aSize)++] = aRecord;
}

void nsCharsetMenu::CreateCharsetMenu(CharsetInfo ** aArray, PRInt32 aSize)
{
  nsCOMPtr<nsIRDFResource> charset;
  nsresult res;

  for (int i = 0; i < aSize; i++) {
    nsString * charsetTitle = aArray[i]->mName;

    // Make up a unique ID and create the RDF NODE
    nsString uniqueID = "";
    uniqueID.Append(*charsetTitle);
    char cID[256];
    uniqueID.ToCString(cID, 256);
    res = gRDFService->GetResource(cID, getter_AddRefs(charset));
    if (NS_FAILED(res)) continue;

    // Get the RDF literal and add it to our node 
    nsCOMPtr<nsIRDFLiteral> charsetTitleLiteral;
    res = gRDFService->GetLiteral(charsetTitle->GetUnicode(), getter_AddRefs(charsetTitleLiteral));
    if (NS_FAILED(res)) continue;

    res = Assert(charset, kNC_Name, charsetTitleLiteral, PR_TRUE);
    if (NS_FAILED(res)) continue;

    // Add the element to the container
    nsCOMPtr<nsIRDFContainer> container;
    res = NS_NewRDFContainer(mInner, kNC_CharsetMenuRoot, getter_AddRefs(container));
    if (NS_FAILED(res)) continue;

    res = container->AppendElement(charset);
    if (NS_FAILED(res)) continue;
  }
}

nsresult NS_NewRDFContainer(nsIRDFDataSource* aDataSource,
                   nsIRDFResource* aResource,
                   nsIRDFContainer** aResult)
{
    nsresult rv;
  
    rv = nsComponentManager::CreateInstance( 
                                        kRDFContainerCID, NULL, nsIRDFContainer::GetIID(),  (void**)aResult );
    if (NS_FAILED(rv))
    	return rv;

    rv = (*aResult)->Init(aDataSource, aResource);
    if (NS_FAILED(rv))
    {
    	NS_RELEASE(*aResult);
    }
    return rv;
}

nsresult nsCharsetMenu::Init() 
{
  nsresult res;

  res = nsServiceManager::GetService(kRDFServiceCID, kIRDFServiceIID, 
    (nsISupports**) &gRDFService );
  if (NS_FAILED(res)) return res;

  gRDFService->GetResource(kURINC_CharsetMenuRoot, &kNC_CharsetMenuRoot);
  gRDFService->GetResource(kURINC_Name, &kNC_Name);

  res = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID, nsnull, 
    kIRDFDataSourceIID, (void**) &mInner);
  if (NS_FAILED(res)) return res;

  NS_WITH_SERVICE(nsIRDFContainerUtils, rdfc, kRDFContainerUtilsCID, &res);
  if (NS_FAILED(res)) return res;

  res = rdfc->MakeSeq(mInner, kNC_CharsetMenuRoot, NULL);
  if (NS_FAILED(res)) return res;

  res = gRDFService->RegisterDataSource(this, PR_FALSE);

  return res;
}

//----------------------------------------------------------------------------
// Interface nsIRDFDataSource [implementation]

// XXX format and rename arguments, take implementations outside

NS_IMETHODIMP nsCharsetMenu::Assert(nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aTarget,
                               PRBool aTruthValue)
{
    // XXX TODO: filter out asserts we don't care about
    return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP nsCharsetMenu::Unassert(nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget)
{
    // XXX TODO: filter out unasserts we don't care about
    return mInner->Unassert(aSource, aProperty, aTarget);
}


NS_IMETHODIMP nsCharsetMenu::Change(nsIRDFResource* aSource,
                                       nsIRDFResource* aProperty,
                                       nsIRDFNode* aOldTarget,
                                       nsIRDFNode* aNewTarget)
{
  // XXX TODO: filter out changes we don't care about
  return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP nsCharsetMenu::Move(nsIRDFResource* aOldSource,
                                     nsIRDFResource* aNewSource,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode* aTarget)
{
  // XXX TODO: filter out changes we don't care about
  return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
}


NS_IMETHODIMP nsCharsetMenu::GetAllCommands(nsIRDFResource* source,
                                       nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCharsetMenu::GetAllCmds(nsIRDFResource* source,
                                       nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCharsetMenu::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                         nsIRDFResource*   aCommand,
                                         nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                         PRBool* aResult)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCharsetMenu::DoCommand(nsISupportsArray* aSources,
                                  nsIRDFResource*   aCommand,
                                  nsISupportsArray* aArguments)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
NS_NewCharsetMenu(nsISupports* aOuter, 
                  const nsIID &aIID,
                  void **aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCharsetMenu* inst = new nsCharsetMenu();
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
