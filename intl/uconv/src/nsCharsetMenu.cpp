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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsICurrentCharsetListener.h"
#include "nsLocaleCID.h"
#include "nsQuickSort.h"
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
static const char * kURINC_BrowserMore1CharsetMenuRoot = "NC:BrowserMore1CharsetMenuRoot";
static const char * kURINC_BrowserMore2CharsetMenuRoot = "NC:BrowserMore2CharsetMenuRoot";
static const char * kURINC_BrowserMore3CharsetMenuRoot = "NC:BrowserMore3CharsetMenuRoot";
static const char * kURINC_BrowserMore4CharsetMenuRoot = "NC:BrowserMore4CharsetMenuRoot";
static const char * kURINC_BrowserMore5CharsetMenuRoot = "NC:BrowserMore5CharsetMenuRoot";
static const char * kURINC_BrowserMore6CharsetMenuRoot = "NC:BrowserMore6CharsetMenuRoot";
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Checked);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, BookmarkSeparator);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, CharsetDetector);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, NC, type);

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
class nsCharsetMenu : public nsIRDFDataSource, nsICurrentCharsetListener
{
  NS_DECL_ISUPPORTS

private:
  static nsIRDFResource * kNC_BrowserCharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMoreCharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore1CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore2CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore3CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore4CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore5CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore6CharsetMenuRoot;
  static nsIRDFResource * kNC_Name;
  static nsIRDFResource * kNC_Checked;
  static nsIRDFResource * kNC_CharsetDetector;
  static nsIRDFResource * kNC_BookmarkSeparator;
  static nsIRDFResource * kRDF_type;

  static nsIRDFDataSource * mInner;

  nsObjectArray mBrowserMenu;
  nsObjectArray mBrowserMoreMenu;
  PRInt32       mStaticCount;

  nsresult Init();
  nsresult Done();
  nsresult SetCharsetCheckmark(nsString * aCharset, PRBool aValue);

  nsresult InitBrowserMenu();
  nsresult InitBrowserStaticMenu(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsString ** aDecs, 
      PRInt32 aCount);
  nsresult InitBrowserMoreXMenu(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsString ** aDecs, PRInt32 aCount);
  nsresult InitBrowserMoreMenu(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsString ** aDecs, PRInt32 aCount);
  nsresult InitBrowserAutodetectMenu(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsIRDFContainer * aContainer);
  nsresult AddItemToMenu(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsObjectArray * aObjectArray, 
      nsIRDFContainer * aContainer, nsString * aCharset, 
      nsIRDFResource * aType = NULL);
  nsresult AddItemToArray(nsICharsetConverterManager * aCCMan, 
      nsObjectArray * aObjectArray, nsString * aCharset, 
      nsMenuItem ** aResult, PRInt32 aPosition = -1);
  nsresult AddItemToContainer(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsIRDFContainer * aContainer, 
      nsMenuItem * aItem, nsIRDFResource * aType = NULL);
  nsresult AddSeparatorToContainer(nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsIRDFContainer * aContainer);
  nsresult AddFromStringToMenu(char * aCharsetList, 
      nsIRDFService * aRDFServ, nsICharsetConverterManager * aCCMan, 
      nsObjectArray * aObjectArray, nsIRDFContainer * aContainer,
      nsString ** aDecs, PRInt32 aCount);
  nsresult AddFromPrefsToMenu(nsIPref * aPref, nsIRDFService * aRDFServ, 
      nsICharsetConverterManager * aCCMan, nsObjectArray * aObjectArray, 
      nsIRDFContainer * aContainer, char * aKey, nsString ** aDecs, 
      PRInt32 aCount);
  nsresult AddCharsetToCache(nsObjectArray * aArray, nsString * aCharset);
  nsresult RemoveFlaggedCharsets(nsString ** aList, PRInt32 aCount, 
      nsICharsetConverterManager * aCCMan, nsString * aProp);
  nsresult RemoveStaticCharsets(nsString ** aList, PRInt32 aCount, 
      nsICharsetConverterManager * aCCMan);
  PRInt32 FindItem(nsObjectArray * aArray, nsString * aCharset, 
      nsMenuItem ** aResult);
  nsresult GetCollation(nsICollation ** aCollation);
  nsresult NewRDFContainer(nsIRDFDataSource * aDataSource, 
      nsIRDFResource * aResource, nsIRDFContainer ** aResult);

public:
  nsCharsetMenu();
  virtual ~nsCharsetMenu();

  //--------------------------------------------------------------------------
  // Interface nsICurrentCharsetListener [declaration]

  NS_IMETHOD SetCurrentCharset(const PRUnichar * aCharset);

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

static int CompareItemTitle(const void* aArg1, const void* aArg2, void *data);

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

// NS_IMPL_ISUPPORTS(nsCharsetMenu, NS_GET_IID(nsIRDFDataSource));
NS_IMPL_ISUPPORTS2(nsCharsetMenu, nsIRDFDataSource, nsICurrentCharsetListener)

nsIRDFDataSource * nsCharsetMenu::mInner = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMoreCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore1CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore2CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore3CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore4CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore5CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore6CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Name = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Checked = NULL;
nsIRDFResource * nsCharsetMenu::kNC_CharsetDetector = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BookmarkSeparator = NULL;
nsIRDFResource * nsCharsetMenu::kRDF_type = NULL;

nsCharsetMenu::nsCharsetMenu() 
:mStaticCount(0)
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);

  Init();
  InitBrowserMenu();
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
  rdfServ->GetResource(kURINC_BrowserMore1CharsetMenuRoot, &kNC_BrowserMore1CharsetMenuRoot);
  rdfServ->GetResource(kURINC_BrowserMore2CharsetMenuRoot, &kNC_BrowserMore2CharsetMenuRoot);
  rdfServ->GetResource(kURINC_BrowserMore3CharsetMenuRoot, &kNC_BrowserMore3CharsetMenuRoot);
  rdfServ->GetResource(kURINC_BrowserMore4CharsetMenuRoot, &kNC_BrowserMore4CharsetMenuRoot);
  rdfServ->GetResource(kURINC_BrowserMore5CharsetMenuRoot, &kNC_BrowserMore5CharsetMenuRoot);
  rdfServ->GetResource(kURINC_BrowserMore6CharsetMenuRoot, &kNC_BrowserMore6CharsetMenuRoot);
  rdfServ->GetResource(kURINC_Name, &kNC_Name);
  rdfServ->GetResource(kURINC_Checked, &kNC_Checked);
  rdfServ->GetResource(kURINC_CharsetDetector, &kNC_CharsetDetector);
  rdfServ->GetResource(kURINC_BookmarkSeparator, &kNC_BookmarkSeparator);
  rdfServ->GetResource(kURINC_type, &kRDF_type);

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
  res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore1CharsetMenuRoot, NULL);
  if (NS_FAILED(res)) goto done;
  res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore2CharsetMenuRoot, NULL);
  if (NS_FAILED(res)) goto done;
  res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore3CharsetMenuRoot, NULL);
  if (NS_FAILED(res)) goto done;
  res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore4CharsetMenuRoot, NULL);
  if (NS_FAILED(res)) goto done;
  res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore5CharsetMenuRoot, NULL);
  if (NS_FAILED(res)) goto done;
  res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore6CharsetMenuRoot, NULL);
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
  NS_IF_RELEASE(kNC_BrowserMore1CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore2CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore3CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore4CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore5CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore6CharsetMenuRoot);
  NS_IF_RELEASE(kNC_Name);
  NS_IF_RELEASE(kNC_Checked);
  NS_IF_RELEASE(kNC_CharsetDetector);
  NS_IF_RELEASE(kNC_BookmarkSeparator);
  NS_IF_RELEASE(kRDF_type);
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
  nsAutoString checked; checked.AssignWithConversion((aValue == PR_TRUE) ? "true" : "false");
  res = rdfServ->GetLiteral(checked.GetUnicode(), getter_AddRefs(checkedLiteral));
  if (NS_FAILED(res)) return res;
  res = Assert(node, kNC_Checked, checkedLiteral, PR_TRUE);
  if (NS_FAILED(res)) return res;

  return res;
}

nsresult nsCharsetMenu::InitBrowserMenu() 
{
  nsresult res = NS_OK;

  NS_WITH_SERVICE(nsIRDFService, rdfServ, kRDFServiceCID, &res);
  if (NS_FAILED(res)) return res;

  NS_WITH_SERVICE(nsICharsetConverterManager, ccMan, kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) return res;

  nsString ** decs = NULL;
  PRInt32 count;
  res = ccMan->GetDecoderList(&decs, &count);
  if (NS_FAILED(res)) return res;

  // even if we fail, the show must go on
  InitBrowserStaticMenu(rdfServ, ccMan, decs, count);
  InitBrowserMoreXMenu(rdfServ, ccMan, decs, count);
  InitBrowserMoreMenu(rdfServ, ccMan, decs, count);

  delete [] decs;

  return res;
}

nsresult nsCharsetMenu::InitBrowserStaticMenu(nsIRDFService * aRDFServ, 
                                              nsICharsetConverterManager * aCCMan, 
                                              nsString ** aDecs, PRInt32 aCount) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container;

  res = NewRDFContainer(mInner, kNC_BrowserCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  NS_WITH_SERVICE(nsIPref, pref, NS_PREF_PROGID, &res);
  if (NS_FAILED(res)) return res;

  // XXX work around bug that causes the submenus to be first instead of last
  AddSeparatorToContainer(aRDFServ, aCCMan, container);

  InitBrowserAutodetectMenu(aRDFServ, aCCMan, container);

  AddSeparatorToContainer(aRDFServ, aCCMan, container);

  AddFromPrefsToMenu(pref, aRDFServ, aCCMan, &mBrowserMenu, container, 
      "intl.charset_menu.static", aDecs, aCount);
  mStaticCount = mBrowserMenu.GetUsage();

  return res;
}

nsresult nsCharsetMenu::InitBrowserMoreXMenu(nsIRDFService * aRDFServ, 
                                             nsICharsetConverterManager * aCCMan, 
                                             nsString ** aDecs, PRInt32 aCount) 
{
  nsresult res = NS_OK;

  nsCOMPtr<nsIRDFContainer> container1;
  nsCOMPtr<nsIRDFContainer> container2;
  nsCOMPtr<nsIRDFContainer> container3;
  nsCOMPtr<nsIRDFContainer> container4;
  nsCOMPtr<nsIRDFContainer> container5;
  nsCOMPtr<nsIRDFContainer> container6;
  char cs1[] = { "iso-8859-1, iso-8859-15, iso-8859-2, iso-8859-3, iso-8859-4, iso-8859-7, iso-8859-9, iso-8859-10, iso-8859-13, iso-8859-14" };
  char cs2[] = { "iso-2022-jp, shift_jis, euc-jp, big5, x-euc-tw, gb2312, x-gbk, hz-gb-2312, iso-2022-cn, euc-kr, utf-7, utf-8" };
  char cs3[] = { "koi8-r, iso-8859-5, windows-1251, iso-ir-111, ibm866, x-mac-cyrillic, koi8-u, x-mac-ukrainian" };
  char cs4[] = { "windows-1258, x-viet-tcvn5712, viscii, x-viet-vps, tis-620, armscii-8" };
  char cs5[] = { "x-mac-roman, x-mac-ce, x-mac-turkish, x-mac-croatian, x-mac-romanian, x-mac-icelandic, x-mac-greek" };
  char cs6[] = { "windows-1252, windows-1250, windows-1254, windows-1257, windows-1253" };
  nsObjectArray mBrowserMore1Menu;
  nsObjectArray mBrowserMore2Menu;
  nsObjectArray mBrowserMore3Menu;
  nsObjectArray mBrowserMore4Menu;
  nsObjectArray mBrowserMore5Menu;
  nsObjectArray mBrowserMore6Menu;

  res = NewRDFContainer(mInner, kNC_BrowserMore1CharsetMenuRoot, 
      getter_AddRefs(container1));
  if (NS_FAILED(res)) return res;
  AddFromStringToMenu(cs1, aRDFServ, aCCMan, &mBrowserMore1Menu, container1, 
      aDecs, aCount);

  res = NewRDFContainer(mInner, kNC_BrowserMore2CharsetMenuRoot, 
      getter_AddRefs(container2));
  if (NS_FAILED(res)) return res;
  AddFromStringToMenu(cs2, aRDFServ, aCCMan, &mBrowserMore2Menu, container2, 
      aDecs, aCount);

  res = NewRDFContainer(mInner, kNC_BrowserMore3CharsetMenuRoot, 
      getter_AddRefs(container3));
  if (NS_FAILED(res)) return res;
  AddFromStringToMenu(cs3, aRDFServ, aCCMan, &mBrowserMore3Menu, container3, 
      aDecs, aCount);

  res = NewRDFContainer(mInner, kNC_BrowserMore4CharsetMenuRoot, 
      getter_AddRefs(container4));
  if (NS_FAILED(res)) return res;
  AddFromStringToMenu(cs4, aRDFServ, aCCMan, &mBrowserMore4Menu, container4, 
      aDecs, aCount);

  res = NewRDFContainer(mInner, kNC_BrowserMore5CharsetMenuRoot, 
      getter_AddRefs(container5));
  if (NS_FAILED(res)) return res;
  AddFromStringToMenu(cs5, aRDFServ, aCCMan, &mBrowserMore5Menu, container5, 
      aDecs, aCount);

  res = NewRDFContainer(mInner, kNC_BrowserMore6CharsetMenuRoot, 
      getter_AddRefs(container6));
  if (NS_FAILED(res)) return res;
  AddFromStringToMenu(cs6, aRDFServ, aCCMan, &mBrowserMore6Menu, container6, 
      aDecs, aCount);

  return res;
}

nsresult nsCharsetMenu::InitBrowserMoreMenu(nsIRDFService * aRDFServ, 
                                            nsICharsetConverterManager * aCCMan,
                                            nsString ** aDecs, PRInt32 aCount) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container = nsnull;
  PRInt32 i;

  res = NewRDFContainer(mInner, kNC_BrowserMoreCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // remove charsets "not for browser"
  nsAutoString prop; prop.AssignWithConversion(".notForBrowser");
  RemoveFlaggedCharsets(aDecs, aCount, aCCMan, &prop);

  for (i = 0; i < aCount; i++) if (aDecs[i] != NULL)
      AddItemToArray(aCCMan, &mBrowserMoreMenu, aDecs[i], NULL);

  nsMenuItem ** array = (nsMenuItem **)mBrowserMoreMenu.GetArray();
  PRInt32 size = mBrowserMoreMenu.GetUsage();

  // reorder the array
  nsCOMPtr<nsICollation> collation = nsnull;
  res = GetCollation(getter_AddRefs(collation));
  if (NS_SUCCEEDED(res)) 
      NS_QuickSort(array, size, sizeof(*array), CompareItemTitle, collation);

  for (i=0; i < size; i++) 
      AddItemToContainer(aRDFServ, aCCMan, container, array[i]);

  return res;
}

nsresult nsCharsetMenu::InitBrowserAutodetectMenu(nsIRDFService * aRDFServ, 
                                                  nsICharsetConverterManager * aCCMan,
                                                  nsIRDFContainer * aContainer) 
{
  nsresult res = NS_OK;
  nsObjectArray mBrowserAutodetectMenu;
  nsStringArray array;

  res = aCCMan->GetCharsetDetectorList(&array);
  if (NS_FAILED(res)) return res;

  for (PRInt32 i = 0; i < array.Count(); i++) {
    res = AddItemToMenu(aRDFServ, aCCMan, &mBrowserAutodetectMenu, aContainer, 
        array.StringAt(i), kNC_CharsetDetector);
    if (NS_FAILED(res)) return res;
  }

  // XXX at some point add reordering here

  return res;
}

nsresult nsCharsetMenu::AddItemToMenu(nsIRDFService * aRDFServ, 
                                      nsICharsetConverterManager * aCCMan,
                                      nsObjectArray * aObjectArray,
                                      nsIRDFContainer * aContainer,
                                      nsString * aCharset, 
                                      nsIRDFResource * aType) 
{
  nsresult res = NS_OK;
  nsMenuItem * item = NULL; 
  
  res = AddItemToArray(aCCMan, aObjectArray, aCharset, &item);
  if (NS_FAILED(res)) goto done;

  res = AddItemToContainer(aRDFServ, aCCMan, aContainer, item, aType);
  if (NS_FAILED(res)) goto done;

  item = NULL;

done:
  if (item != NULL) delete item;

  return res;
}

nsresult nsCharsetMenu::AddItemToArray(nsICharsetConverterManager * aCCMan,
                                       nsObjectArray * aObjectArray,
                                       nsString * aCharset,
                                       nsMenuItem ** aResult, 
                                       PRInt32 aPosition) 
{
  nsresult res = NS_OK;
  nsMenuItem * item = NULL; 

  if (aResult != NULL) *aResult = NULL;
  
  item = new nsMenuItem();
  if (item == NULL) {
    res = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  // XXX positively hacky
  if (aObjectArray == &mBrowserMenu) {
    item->mName = new nsString;
    item->mName->AssignWithConversion("charset.");
    item->mName->Append(*aCharset);
  } else {
    item->mName = new nsString(*aCharset);
    if (item->mName  == NULL) {
      res = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
  }

  res = aCCMan->GetCharsetTitle(aCharset, &item->mTitle);
  if (NS_FAILED(res)) item->mTitle = new nsString(*aCharset);
  if (item->mTitle == NULL) {
    res = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  res = aObjectArray->AddObject(item, aPosition);
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
                                           nsMenuItem * aItem,
                                           nsIRDFResource * aType) 
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

  if (aType != NULL) {
    res = Assert(node, kRDF_type, aType, PR_TRUE);
    if (NS_FAILED(res)) return res;
  }

    // Add the element to the container
  res = aContainer->AppendElement(node);
  if (NS_FAILED(res)) return res;

  return res;
}

// XXX unify with AddItemToContainer
nsresult nsCharsetMenu::AddSeparatorToContainer(nsIRDFService * aRDFServ, 
                                                nsICharsetConverterManager * aCCMan,
                                                nsIRDFContainer * aContainer) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFResource> node;

  // Make up a unique ID and create the RDF NODE
  char csID[] = "-----";
  res = aRDFServ->GetResource(csID, getter_AddRefs(node));
  if (NS_FAILED(res)) return res;

  nsAutoString str; str.AssignWithConversion(csID);

  // set node's title
  nsCOMPtr<nsIRDFLiteral> titleLiteral;
  res = aRDFServ->GetLiteral(str.GetUnicode(), getter_AddRefs(titleLiteral));
  if (NS_FAILED(res)) return res;
  res = Assert(node, kNC_Name, titleLiteral, PR_TRUE);
  if (NS_FAILED(res)) return res;
  res = Assert(node, kRDF_type, kNC_BookmarkSeparator, PR_TRUE);
  if (NS_FAILED(res)) return res;

    // Add the element to the container
  res = aContainer->AppendElement(node);
  if (NS_FAILED(res)) return res;

  return res;
}

nsresult nsCharsetMenu::AddFromStringToMenu(char * aCharsetList,
                                            nsIRDFService * aRDFServ, 
                                            nsICharsetConverterManager * aCCMan, 
                                            nsObjectArray * aObjectArray, 
                                            nsIRDFContainer * aContainer,
                                            nsString ** aDecs, PRInt32 aCount)
{
  char * p = aCharsetList;
  char * q = p;
  while (*p != 0) {
    for (; (*q != ',') && (*q != ' ') && (*q != 0); q++);
    char temp = *q;
    *q = 0;

    nsAutoString str; str.AssignWithConversion(p);

    // if this charset is not on accepted list of charsets, ignore it
    PRInt32 i;
    for (i = 0; i < aCount; i++) {
      if ((aDecs[i] != NULL) && str.Equals(*aDecs[i])) break;
    }

    // else, remove it from there
    if (i != aCount) {
      nsresult res = AddItemToMenu(aRDFServ, aCCMan, aObjectArray, aContainer, &str);
      NS_ASSERTION(NS_SUCCEEDED(res), "failed to add item to menu");
      aDecs[i] = NULL;
    } 

    *q = temp;
    for (; (*q == ',') || (*q == ' '); q++);
    p=q;
  }

  return NS_OK;
}

nsresult nsCharsetMenu::AddFromPrefsToMenu(nsIPref * aPref, 
                                           nsIRDFService * aRDFServ, 
                                           nsICharsetConverterManager * aCCMan, 
                                           nsObjectArray * aObjectArray, 
                                           nsIRDFContainer * aContainer, 
                                           char * aKey,
                                           nsString ** aDecs, PRInt32 aCount)
{
  nsresult res = NS_OK;

  char * value = NULL;
  res = aPref->CopyCharPref(aKey, &value);
  if (NS_FAILED(res)) return res;

  if (value != NULL) {
    res = AddFromStringToMenu(value, aRDFServ, aCCMan, aObjectArray, 
        aContainer, aDecs, aCount);
    nsAllocator::Free(value);
  }

  return res;
}

nsresult nsCharsetMenu::AddCharsetToCache(nsObjectArray * aArray, 
                                          nsString * aCharset)
{
  PRInt32 i;
  nsresult res = NS_OK;

  nsAutoString charset; charset.AssignWithConversion("charset.");
  charset.Append(*aCharset);
  i = FindItem(aArray, &charset, NULL);
  if (i >= 0) return res;

  nsCOMPtr<nsIRDFContainer> container;

  NS_WITH_SERVICE(nsIRDFService, rdfServ, kRDFServiceCID, &res);
  if (NS_FAILED(res)) return res;

  NS_WITH_SERVICE(nsICharsetConverterManager, ccMan, kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) return res;

  res = NewRDFContainer(mInner, kNC_BrowserCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;


  // XXX insert into first cache position, not at the end
  // XXX iff too many items, remove last one.
  res = AddItemToMenu(rdfServ, ccMan, aArray, container, aCharset);

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
    if (aResult != NULL) *aResult = array[i];
    return i;
  };

  if (aResult != NULL) *aResult = NULL;
  return -1;
}

static int CompareItemTitle(const void* aArg1, const void* aArg2, void *data)
{
  PRInt32 res; 
  nsMenuItem * aItem1 = *((nsMenuItem **) aArg1);
  nsMenuItem * aItem2 = *((nsMenuItem **) aArg2);
  nsICollation * aCollation = (nsICollation *) data;

  aCollation->CompareString(kCollationCaseInSensitive, *aItem1->mTitle, 
      *aItem2->mTitle, &res);

  return res;
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
      NS_GET_IID(nsICollationFactory), (void**) &collationFactory);
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
      NS_GET_IID(nsIRDFContainer), (void**)aResult);
  if (NS_FAILED(res)) return res;

  res = (*aResult)->Init(aDataSource, aResource);
  if (NS_FAILED(res)) {
    NS_RELEASE(*aResult);
  }

  return res;
}

//----------------------------------------------------------------------------
// Interface nsICurrentCharsetListener [implementation]

NS_IMETHODIMP nsCharsetMenu::SetCurrentCharset(const PRUnichar * aCharset)
{
  // iff necessary, add new charset item, freeing a position if needed
  nsAutoString charset(aCharset);
  return AddCharsetToCache(&mBrowserMenu, &charset);
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
