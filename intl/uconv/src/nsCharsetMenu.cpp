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
#include "rdf.h"
#include "nsIRDFDataSource.h"
#include "nsXPIDLString.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsUConvDll.h"
#include "nsCharsetMenu.h"
#include "nsICharsetConverterManager.h"
#include "nsIStringBundle.h"
#include "nsICollation.h"
#include "nsCollationCID.h"
#include "nsIPref.h"
#include "nsILocaleService.h"
#include "nsLocaleCID.h"
#include "nsObjectArray.h"

static NS_DEFINE_IID(kIRDFServiceIID, NS_IRDFSERVICE_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_IID(kIRDFDataSourceIID, NS_IRDFDATASOURCE_IID);
static NS_DEFINE_CID(kIRDFContainerUtilsIID, NS_IRDFCONTAINERUTILS_IID);
static NS_DEFINE_CID(kRDFContainerUtilsCID, NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kICharsetConverterManagerIID, NS_ICHARSETCONVERTERMANAGER_IID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

static const char * kURINC_BrowserCharsetMenuRoot = "NC:BrowserCharsetMenuRoot";
static const char * kURINC_BrowserMoreCharsetMenuRoot = "NC:BrowserMoreCharsetMenuRoot";
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Checked);

//----------------------------------------------------------------------------
// Class nsMenuItem [declaration]


// XXX elliminate pointers here and include directly nsString objects
class nsMenuItem : public nsObject
{
public: 
  nsString *    mName;
  nsString *    mTitle;

  nsMenuItem();
  ~nsMenuItem();
};

//----------------------------------------------------------------------------
// Class nsCharsetMenu [declaration]

/**
 * The Charset Converter menu.
 *
 * @created         23/Nov/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsCharsetMenu : public nsIRDFDataSource
{
  NS_DECL_ISUPPORTS

private:
  static nsIRDFResource * kNC_BrowserCharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMoreCharsetMenuRoot;
  static nsIRDFResource * kNC_Name;
  static nsIRDFResource * kNC_Checked;

  static nsIRDFDataSource * mInner;

  nsObjectArray mBrowserMenu;
  nsObjectArray mBrowserMoreMenu;
  PRInt32       mStaticCount;

  nsresult Init();
  nsresult Done();
  nsresult SetCharsetCheckmark(nsString * aCharset, PRBool aValue);

  nsresult CreateBrowserMenu();
  nsresult CreateBrowserBasicMenu(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan);
  nsresult CreateBrowserMoreMenu(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan);
  nsresult AddItemToMenu(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsObjectArray * aObjectArray, 
      nsIRDFContainer * aContainer, nsString * aCharset);
  nsresult AddItemToArray(nsICharsetConverterManager * aCCMan, 
      nsObjectArray * aObjectArray, nsString * aCharset, 
      nsMenuItem ** aResult);
  nsresult AddItemToContainer(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsIRDFContainer * aContainer, 
      nsMenuItem * aItem);
  nsresult AddFromPrefsToMenu(nsIPref * aPref, nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsObjectArray * aObjectArray, 
      nsIRDFContainer * aContainer, char * aKey);
  nsresult RemoveFlaggedCharsets(nsString ** aList, PRInt32 aCount, 
      nsICharsetConverterManager * aCCMan, nsString * aProp);
  nsresult RemoveStaticCharsets(nsString ** aList, PRInt32 aCount, 
      nsICharsetConverterManager * aCCMan);
  PRInt32 FindItem(nsObjectArray * aArray, nsString * aCharset, 
      nsMenuItem ** aResult);
  PRInt32 CompareItemTitle(nsMenuItem * aItem1, nsMenuItem * aItem2, 
      nsICollation * aCollation);
  void QuickSort(nsMenuItem ** aArray, PRInt32 aLow, PRInt32 aHigh, 
      nsICollation * aCollation);
  PRInt32 QSPartition(nsMenuItem ** aArray, PRInt32 aLow, PRInt32 aHigh, 
      nsICollation * aCollation);
  nsresult GetCollation(nsICollation ** aCollation);
  nsresult NewRDFContainer(nsIRDFDataSource * aDataSource, 
      nsIRDFResource * aResource, nsIRDFContainer ** aResult);

public:
  nsCharsetMenu();
  virtual ~nsCharsetMenu();

  //--------------------------------------------------------------------------
  // Interface nsIRDFDataSource [declaration]

  NS_IMETHOD GetURI(char ** uri);

  NS_IMETHOD GetSource(nsIRDFResource* property, nsIRDFNode* target, 
      PRBool tv, nsIRDFResource** source);

  NS_IMETHOD GetSources(nsIRDFResource* property, nsIRDFNode* target, 
      PRBool tv, nsISimpleEnumerator** sources);

  NS_IMETHOD GetTarget(nsIRDFResource* source, nsIRDFResource* property, 
      PRBool tv, nsIRDFNode** target);

  NS_IMETHOD GetTargets(nsIRDFResource* source, nsIRDFResource* property, 
      PRBool tv, nsISimpleEnumerator** targets);

  NS_IMETHOD Assert(nsIRDFResource* aSource, nsIRDFResource* aProperty, 
      nsIRDFNode* aTarget, PRBool aTruthValue);

  NS_IMETHOD Unassert(nsIRDFResource* aSource, nsIRDFResource* aProperty, 
      nsIRDFNode* aTarget);

  NS_IMETHOD Change(nsIRDFResource* aSource, nsIRDFResource* aProperty, 
      nsIRDFNode* aOldTarget, nsIRDFNode* aNewTarget);

  NS_IMETHOD Move(nsIRDFResource* aOldSource, nsIRDFResource* aNewSource, 
      nsIRDFResource* aProperty, nsIRDFNode* aTarget);

  NS_IMETHOD HasAssertion(nsIRDFResource* source, nsIRDFResource* property, 
      nsIRDFNode* target, PRBool tv, PRBool* hasAssertion);

  NS_IMETHOD AddObserver(nsIRDFObserver* n);

  NS_IMETHOD RemoveObserver(nsIRDFObserver* n);

  NS_IMETHOD ArcLabelsIn( nsIRDFNode* node, nsISimpleEnumerator** labels);

  NS_IMETHOD ArcLabelsOut(nsIRDFResource* source, 
      nsISimpleEnumerator** labels);

  NS_IMETHOD GetAllResources(nsISimpleEnumerator** aCursor);

  NS_IMETHOD GetAllCommands(nsIRDFResource* source, nsIEnumerator** commands);

  NS_IMETHOD GetAllCmds(nsIRDFResource* source, 
      nsISimpleEnumerator** commands);

  NS_IMETHOD IsCommandEnabled(nsISupportsArray* aSources, 
      nsIRDFResource * aCommand, nsISupportsArray* aArguments, 
      PRBool* aResult);

  NS_IMETHOD DoCommand(nsISupportsArray* aSources, nsIRDFResource*   aCommand, 
      nsISupportsArray* aArguments);
};

//----------------------------------------------------------------------------
// Global functions and data [implementation]

NS_IMETHODIMP NS_NewCharsetMenu(nsISupports * aOuter, const nsIID & aIID, 
                                void ** aResult)
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

//----------------------------------------------------------------------------
// Class nsConverterInfo [implementation]

nsMenuItem::nsMenuItem()
{
  mName = NULL;
  mTitle = NULL;
}

nsMenuItem::~nsMenuItem()
{
  if (mName != NULL) delete mName;
  if (mTitle != NULL) delete mTitle;
}

//----------------------------------------------------------------------------
// Class nsCharsetMenu [implementation]

NS_IMPL_ISUPPORTS(nsCharsetMenu, nsIRDFDataSource::GetIID());

nsIRDFDataSource * nsCharsetMenu::mInner = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMoreCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Name = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Checked = NULL;

nsCharsetMenu::nsCharsetMenu() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);

  Init();
  CreateBrowserMenu();
}

nsCharsetMenu::~nsCharsetMenu() 
{
  Done();

  PR_AtomicDecrement(&g_InstanceCount);
}

nsresult nsCharsetMenu::Init() 
{
  nsresult res = NS_OK;
  nsIRDFService * rdfServ = NULL;
  nsIRDFContainerUtils * rdfUtil = NULL;

  res = nsServiceManager::GetService(kRDFServiceCID, kIRDFServiceIID, 
      (nsISupports **)&rdfServ);
  if (NS_FAILED(res)) goto done;

  rdfServ->GetResource(kURINC_BrowserCharsetMenuRoot, &kNC_BrowserCharsetMenuRoot);
  rdfServ->GetResource(kURINC_BrowserMoreCharsetMenuRoot, &kNC_BrowserMoreCharsetMenuRoot);
  rdfServ->GetResource(kURINC_Name, &kNC_Name);
  rdfServ->GetResource(kURINC_Checked, &kNC_Checked);

  res = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID, nsnull, 
    kIRDFDataSourceIID, (void**) &mInner);
  if (NS_FAILED(res)) goto done;

  res = nsServiceManager::GetService(kRDFContainerUtilsCID, 
      kIRDFContainerUtilsIID, (nsISupports **)&rdfUtil);
  if (NS_FAILED(res)) goto done;

  res = rdfUtil->MakeSeq(mInner, kNC_BrowserCharsetMenuRoot, NULL);
  if (NS_FAILED(res)) goto done;
  res = rdfUtil->MakeSeq(mInner, kNC_BrowserMoreCharsetMenuRoot, NULL);
  if (NS_FAILED(res)) goto done;

  res = rdfServ->RegisterDataSource(this, PR_FALSE);

done:
  if (rdfServ != NULL) nsServiceManager::ReleaseService(kRDFServiceCID, 
      rdfServ);
  if (rdfUtil != NULL) nsServiceManager::ReleaseService(kRDFContainerUtilsCID, 
      rdfUtil);

  return res;
}

nsresult nsCharsetMenu::Done() 
{
  nsresult res = NS_OK;
  nsIRDFService * rdfServ = NULL;

  res = nsServiceManager::GetService(kRDFServiceCID, kIRDFServiceIID, 
      (nsISupports **)&rdfServ);
  if (NS_FAILED(res)) goto done;

  res = rdfServ->UnregisterDataSource(this);

done:
  if (rdfServ != NULL) nsServiceManager::ReleaseService(kRDFServiceCID, 
      rdfServ);
  NS_IF_RELEASE(kNC_BrowserCharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMoreCharsetMenuRoot);
  NS_IF_RELEASE(kNC_Name);
  NS_IF_RELEASE(kNC_Checked);
  NS_IF_RELEASE(mInner);

  return res;
}

nsresult nsCharsetMenu::SetCharsetCheckmark(nsString * aCharset, 
                                            PRBool aValue)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container;
  nsCOMPtr<nsIRDFResource> node;

  NS_WITH_SERVICE(nsIRDFService, rdfServ, kRDFServiceCID, &res);
  if (NS_FAILED(res)) return res;

  res = NewRDFContainer(mInner, kNC_BrowserCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // find RDF node for given charset
  char csID[256];
  aCharset->ToCString(csID, sizeof(csID));
  res = rdfServ->GetResource(csID, getter_AddRefs(node));
  if (NS_FAILED(res)) return res;

  // set checkmark value
  nsCOMPtr<nsIRDFLiteral> checkedLiteral;
  nsAutoString checked((aValue == PR_TRUE) ? "true" : "false");
  res = rdfServ->GetLiteral(checked.GetUnicode(), getter_AddRefs(checkedLiteral));
  if (NS_FAILED(res)) return res;
  res = Assert(node, kNC_Checked, checkedLiteral, PR_TRUE);
  if (NS_FAILED(res)) return res;

  return res;
}

nsresult nsCharsetMenu::CreateBrowserMenu() 
{
  nsresult res = NS_OK;

  NS_WITH_SERVICE(nsIRDFService, rdfServ, kRDFServiceCID, &res);
  if (NS_FAILED(res)) return res;

  NS_WITH_SERVICE(nsICharsetConverterManager, ccMan, kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) return res;

  // even if we fail, the show must go on
  CreateBrowserBasicMenu(rdfServ, ccMan);
  CreateBrowserMoreMenu(rdfServ, ccMan);

  return res;
}

nsresult nsCharsetMenu::CreateBrowserBasicMenu(nsIRDFService * aRDFServ, 
                                               nsICharsetConverterManager * aCCMan) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container;

  res = NewRDFContainer(mInner, kNC_BrowserCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  NS_WITH_SERVICE(nsIPref, pref, NS_PREF_PROGID, &res);
  if (NS_FAILED(res)) return res;

  AddFromPrefsToMenu(pref, aRDFServ, aCCMan, &mBrowserMenu, container, 
      "intl.charset_menu.static");
  mStaticCount = mBrowserMenu.GetUsage();
  AddFromPrefsToMenu(pref, aRDFServ, aCCMan, &mBrowserMenu, container, 
      "intl.charset_menu.cache");

  return res;
}

nsresult nsCharsetMenu::CreateBrowserMoreMenu(nsIRDFService * aRDFServ, 
                                              nsICharsetConverterManager * aCCMan) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container = nsnull;
  PRInt32 i;

  res = NewRDFContainer(mInner, kNC_BrowserMoreCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  nsString ** decs = NULL;
  PRInt32 count;
  res = aCCMan->GetDecoderList(&decs, &count);
  if (NS_FAILED(res)) return res;

  // remove charsets "not for browser"
  nsAutoString prop(".notForBrowser");
  RemoveFlaggedCharsets(decs, count, aCCMan, &prop);

  // remove static charsets
  RemoveStaticCharsets(decs, count, aCCMan);

  // XXX remove charsets in the dynamic menu

  for (i = 0; i < count; i++) if (decs[i] != NULL)
      AddItemToArray(aCCMan, &mBrowserMoreMenu, decs[i], NULL);

  nsMenuItem ** array = (nsMenuItem **)mBrowserMoreMenu.GetArray();
  PRInt32 size = mBrowserMoreMenu.GetUsage();

  // reorder the array
  nsCOMPtr<nsICollation> collation = nsnull;
  res = GetCollation(getter_AddRefs(collation));
  if (NS_SUCCEEDED(res)) QuickSort(array, 0, size - 1, collation);

  for (i=0; i < size; i++) 
      AddItemToContainer(aRDFServ, aCCMan, container, array[i]);

  delete [] decs;

  return res;
}

nsresult nsCharsetMenu::AddItemToMenu(nsIRDFService * aRDFServ, 
                                      nsICharsetConverterManager * aCCMan,
                                      nsObjectArray * aObjectArray,
                                      nsIRDFContainer * aContainer,
                                      nsString * aCharset) 
{
  nsresult res = NS_OK;
  nsMenuItem * item = NULL; 
  
  res = AddItemToArray(aCCMan, aObjectArray, aCharset, &item);
  if (NS_FAILED(res)) goto done;

  res = AddItemToContainer(aRDFServ, aCCMan, aContainer, item);
  if (NS_FAILED(res)) goto done;

  item = NULL;

done:
  if (item != NULL) delete item;

  return res;
}

nsresult nsCharsetMenu::AddItemToArray(nsICharsetConverterManager * aCCMan,
                                       nsObjectArray * aObjectArray,
                                       nsString * aCharset,
                                       nsMenuItem ** aResult) 
{
  nsresult res = NS_OK;
  nsMenuItem * item = NULL; 

  if (aResult != NULL) *aResult = NULL;
  
  item = new nsMenuItem();
  if (item == NULL) {
    res = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  item->mName = new nsString(*aCharset);
  if (item->mName  == NULL) {
    res = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  res = aCCMan->GetCharsetTitle(aCharset, &item->mTitle);
  if (NS_FAILED(res)) item->mTitle = new nsString(*aCharset);
  if (item->mTitle == NULL) {
    res = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  res = aObjectArray->AddObject(item);
  if (NS_FAILED(res)) goto done;

  if (aResult != NULL) *aResult = item;
  item = NULL;

done:
  if (item != NULL) delete item;

  return res;
}

nsresult nsCharsetMenu::AddItemToContainer(nsIRDFService * aRDFServ, 
                                           nsICharsetConverterManager * aCCMan,
                                           nsIRDFContainer * aContainer,
                                           nsMenuItem * aItem) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFResource> node;

  nsString * cs = aItem->mName;

  // Make up a unique ID and create the RDF NODE
  char csID[256];
  cs->ToCString(csID, sizeof(csID));
  res = aRDFServ->GetResource(csID, getter_AddRefs(node));
  if (NS_FAILED(res)) return res;

  const PRUnichar * title = aItem->mTitle->GetUnicode();

    // set node's title
  nsCOMPtr<nsIRDFLiteral> titleLiteral;
  res = aRDFServ->GetLiteral(title, getter_AddRefs(titleLiteral));
  if (NS_FAILED(res)) return res;
  res = Assert(node, kNC_Name, titleLiteral, PR_TRUE);
  if (NS_FAILED(res)) return res;

    // Add the element to the container
  res = aContainer->AppendElement(node);
  if (NS_FAILED(res)) return res;

  return res;
}

nsresult nsCharsetMenu::AddFromPrefsToMenu(nsIPref * aPref, 
                                           nsIRDFService * aRDFServ, 
                                           nsICharsetConverterManager * aCCMan, 
                                           nsObjectArray * aObjectArray, 
                                           nsIRDFContainer * aContainer, 
                                           char * aKey)
{
  nsresult res = NS_OK;

  char * value = NULL;
  res = aPref->CopyCharPref(aKey, &value);
  if (NS_FAILED(res)) return res;

  if (value != NULL) {
    char * p = value;
    char * q = p;
    while (*p != 0) {
      for (; (*q != ',') && (*q != ' ') && (*q != 0); q++);
      char temp = *q;
      *q = 0;

      nsAutoString str(p);
      AddItemToMenu(aRDFServ, aCCMan, aObjectArray, aContainer, &str);

      *q = temp;
      for (; (*q == ',') || (*q == ' '); q++);
      p=q;
    }

    nsAllocator::Free(value);
  }

  return res;
}

nsresult nsCharsetMenu::RemoveFlaggedCharsets(nsString ** aList, PRInt32 aCount,
                                              nsICharsetConverterManager * aCCMan,
                                              nsString * aProp)
{
  nsresult res = NS_OK;

  for (PRInt32 i = 0; i < aCount; i++) {
    nsString * cs = aList[i];
    if (cs == NULL) continue;

    nsString * data = NULL;
    aCCMan->GetCharsetData(cs, aProp, &data);
    if (data == NULL) continue;

    aList[i] = NULL;
    delete data;
  }

  return res;
}

nsresult nsCharsetMenu::RemoveStaticCharsets(nsString ** aList, PRInt32 aCount,
                                             nsICharsetConverterManager * aCCMan)
{
  nsresult res = NS_OK;

  for (PRInt32 i = 0; i < aCount; i++) {
    nsString * cs = aList[i];
    if (cs == NULL) continue;

    nsMenuItem * item;
    if (FindItem(&mBrowserMenu, cs, &item) < 0) continue;

    aList[i] = NULL;
  }

  return res;
}

PRInt32 nsCharsetMenu::FindItem(nsObjectArray * aArray,
                                 nsString * aCharset, nsMenuItem ** aResult)
{
  PRInt32 size = aArray->GetUsage();
  nsMenuItem ** array = (nsMenuItem **)aArray->GetArray();

  for (PRInt32 i=0; i<size; i++) if (aCharset->Equals(*(array[i]->mName))) {
    *aResult = array[i];
    return i;
  };

  aResult = NULL;
  return -1;
}

PRInt32 nsCharsetMenu::CompareItemTitle(nsMenuItem * aItem1, 
                                        nsMenuItem * aItem2,
                                        nsICollation * aCollation)
{
  PRInt32 res; 
  aCollation->CompareString(kCollationStrengthDefault, *aItem1->mTitle, 
      *aItem2->mTitle, &res);

  return res;
}

// XXX use already available QS rutine
void nsCharsetMenu::QuickSort(nsMenuItem ** aArray, PRInt32 aLow, 
                              PRInt32 aHigh, nsICollation * aCollation)
{
  PRInt32 pivot;

  // termination condition
  if (aHigh > aLow) {
    pivot = QSPartition(aArray, aLow, aHigh, aCollation);
    QuickSort(aArray, aLow, pivot-1, aCollation);
    QuickSort(aArray, pivot+1, aHigh, aCollation);
  }
}

// XXX improve performance by generating and storing collation keys
PRInt32 nsCharsetMenu::QSPartition(nsMenuItem ** aArray, PRInt32 aLow, 
                                   PRInt32 aHigh, nsICollation * aCollation)
{
  PRInt32 left, right;
  nsMenuItem * pivot_item;
  pivot_item = aArray[aLow];
  left = aLow;
  right = aHigh;
  while ( left < right ) {
    /* Move left while item < pivot */
    while ((CompareItemTitle(aArray[left], pivot_item, aCollation) <= 0) && (left < right)) left++;
    /* Move right while item > pivot */
    while (CompareItemTitle(aArray[right], pivot_item, aCollation) > 0) right--;
    if (left < right) {
      nsMenuItem * temp = aArray[left];
      aArray[left] = aArray[right];
      aArray[right] = temp;
    }
  }
  /* right is final position for the pivot */
  aArray[aLow] = aArray[right];
  aArray[right] = pivot_item;
  return right;
}

nsresult nsCharsetMenu::GetCollation(nsICollation ** aCollation)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsILocale> locale = nsnull;
  nsICollationFactory * collationFactory = nsnull;
  
  NS_WITH_SERVICE(nsILocaleService, localeServ, kLocaleServiceCID, &res);
  if (NS_FAILED(res)) return res;

  res = localeServ->GetApplicationLocale(getter_AddRefs(locale));
  if (NS_FAILED(res)) return res;

  res = nsComponentManager::CreateInstance(kCollationFactoryCID, NULL, 
      nsICollationFactory::GetIID(), (void**) &collationFactory);
  if (NS_FAILED(res)) return res;

  res = collationFactory->CreateCollation(locale, aCollation);
  NS_RELEASE(collationFactory);
  return res;
}

nsresult nsCharsetMenu::NewRDFContainer(nsIRDFDataSource * aDataSource, 
                                        nsIRDFResource * aResource, 
                                        nsIRDFContainer ** aResult)
{
  nsresult res;

  res = nsComponentManager::CreateInstance(kRDFContainerCID, NULL, 
      nsIRDFContainer::GetIID(), (void**)aResult);
  if (NS_FAILED(res)) return res;

  res = (*aResult)->Init(aDataSource, aResource);
  if (NS_FAILED(res)) {
    NS_RELEASE(*aResult);
  }

  return res;
}

//----------------------------------------------------------------------------
// Interface nsIRDFDataSource [implementation]

NS_IMETHODIMP nsCharsetMenu::GetURI(char ** uri)
{
  if (!uri) return NS_ERROR_NULL_POINTER;

  *uri = nsXPIDLCString::Copy("rdf:charset-menu");
  if (!(*uri)) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsCharsetMenu::GetSource(nsIRDFResource* property,
                                       nsIRDFNode* target,
                                       PRBool tv,
                                       nsIRDFResource** source)
{
  return mInner->GetSource(property, target, tv, source);
}

NS_IMETHODIMP nsCharsetMenu::GetSources(nsIRDFResource* property,
                                        nsIRDFNode* target,
                                        PRBool tv,
                                        nsISimpleEnumerator** sources)
{
  return mInner->GetSources(property, target, tv, sources);
}

NS_IMETHODIMP nsCharsetMenu::GetTarget(nsIRDFResource* source,
                                       nsIRDFResource* property,
                                       PRBool tv,
                                       nsIRDFNode** target)
{
  return mInner->GetTarget(source, property, tv, target);
}

NS_IMETHODIMP nsCharsetMenu::GetTargets(nsIRDFResource* source,
                                        nsIRDFResource* property,
                                        PRBool tv,
                                        nsISimpleEnumerator** targets)
{
  return mInner->GetTargets(source, property, tv, targets);
}

NS_IMETHODIMP nsCharsetMenu::Assert(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    PRBool aTruthValue)
{
  // TODO: filter out asserts we don't care about
  return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP nsCharsetMenu::Unassert(nsIRDFResource* aSource,
                                      nsIRDFResource* aProperty,
                                      nsIRDFNode* aTarget)
{
  // TODO: filter out unasserts we don't care about
  return mInner->Unassert(aSource, aProperty, aTarget);
}


NS_IMETHODIMP nsCharsetMenu::Change(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aOldTarget,
                                    nsIRDFNode* aNewTarget)
{
  // TODO: filter out changes we don't care about
  return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP nsCharsetMenu::Move(nsIRDFResource* aOldSource,
                                  nsIRDFResource* aNewSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
  // TODO: filter out changes we don't care about
  return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
}


NS_IMETHODIMP nsCharsetMenu::HasAssertion(nsIRDFResource* source, 
                                          nsIRDFResource* property, 
                                          nsIRDFNode* target, PRBool tv, 
                                          PRBool* hasAssertion)
{
  return mInner->HasAssertion(source, property, target, tv, hasAssertion);
}

NS_IMETHODIMP nsCharsetMenu::AddObserver(nsIRDFObserver* n)
{
  return mInner->AddObserver(n);
}

NS_IMETHODIMP nsCharsetMenu::RemoveObserver(nsIRDFObserver* n)
{
  return mInner->RemoveObserver(n);
}

NS_IMETHODIMP nsCharsetMenu::ArcLabelsIn(nsIRDFNode* node, 
                                         nsISimpleEnumerator** labels)
{
  return mInner->ArcLabelsIn(node, labels);
}

NS_IMETHODIMP nsCharsetMenu::ArcLabelsOut(nsIRDFResource* source, 
                                          nsISimpleEnumerator** labels)
{
  return mInner->ArcLabelsOut(source, labels);
}

NS_IMETHODIMP nsCharsetMenu::GetAllResources(nsISimpleEnumerator** aCursor)
{
  return mInner->GetAllResources(aCursor);
}

NS_IMETHODIMP nsCharsetMenu::GetAllCommands(
                             nsIRDFResource* source,
                             nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCharsetMenu::GetAllCmds(
                             nsIRDFResource* source,
                             nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCharsetMenu::IsCommandEnabled(
                             nsISupportsArray/*<nsIRDFResource>*/* aSources,
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
