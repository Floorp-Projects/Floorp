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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "stdafx.h"

#include "resource.h"
#include "Cbrowse.h"
#include "CBrowseDlg.h"
#include "TestScriptHelper.h"

#define BEGIN_TESTSET() \
    TestSet aTestSets[] = {
#define DECL_SET(name, descr) \
    { _T(#name), _T(descr), sizeof(a##name)/sizeof(a##name[0]), a##name, NULL },
#define DECL_SET_DYNAMIC(name, descr, pop) \
    { _T(#name), _T(descr), 0, NULL, pop },
#define END_TESTSET() \
    { NULL, NULL, 0, NULL } }; \
    int nTestSets = sizeof(aTestSets) / sizeof(aTestSets[0]) - 1;

///////////////////////////////////////////////////////////////////////////////

void BrowserInfo::OutputString(const TCHAR *szMessage, ...)
{
	TCHAR szBuffer[256];

	va_list cArgs;
	va_start(cArgs, szMessage);
	_vstprintf(szBuffer, szMessage, cArgs);
	va_end(cArgs);

	CString szOutput;
	szOutput.Format(_T("  Test: %s"), szBuffer);

	pBrowseDlg->OutputString(szOutput);
}


HRESULT BrowserInfo::GetWebBrowser(IWebBrowserApp **pWebBrowser)
{
	if (pIUnknown == NULL)
	{
		return E_FAIL;
	}
	return pIUnknown->QueryInterface(IID_IWebBrowserApp, (void **) pWebBrowser);
}


HRESULT BrowserInfo::GetDocument(IHTMLDocument2 **pDocument)
{
	CIPtr(IWebBrowserApp) cpWebBrowser;
	if (FAILED(GetWebBrowser(&cpWebBrowser)))
	{
		return E_FAIL;
	}

	CIPtr(IDispatch) cpDispDocument;
	cpWebBrowser->get_Document(&cpDispDocument);
	if (cpDispDocument == NULL)
	{
		return E_FAIL;
	}

	return cpDispDocument->QueryInterface(IID_IHTMLDocument2, (void **) pDocument);
}

///////////////////////////////////////////////////////////////////////////////

struct InterfaceInfo
{
	const IID *piid;
	const TCHAR *szName;
};


static InterfaceInfo aDocIIDs[] = 
{
	{ &IID_IOleCommandTarget,		_T("IOleCommandTarget") },
	{ &IID_IHTMLDocument,			_T("IHTMLDocument") },
	{ &IID_IHTMLDocument2,			_T("IHTMLDocument2") },
	{ &IID_IHTMLElementCollection,	_T("IHTMLElementCollection") },
	{ &IID_IHTMLElement,			_T("IHTMLElement") }
};


TestResult __cdecl tstDocument(BrowserInfo &cInfo)
{
	CIPtr(IHTMLDocument2) cpDocElement;
	cInfo.GetDocument(&cpDocElement);
	if (cpDocElement == NULL)
	{
		cInfo.OutputString(_T("Error: No document"));
		return trFailed;
	}

	// Dump out all the interfaces supported by the document element
	for (int i = 0; i < sizeof(aDocIIDs) / sizeof(aDocIIDs[0]); i++)
	{
		IUnknown *pUnkI = NULL;
		if (SUCCEEDED(cpDocElement->QueryInterface(*(aDocIIDs[i].piid), (void **) &pUnkI)))
		{
			cInfo.OutputString(_T("Info: Document supports interface %s"), aDocIIDs[i].szName);
			pUnkI->Release();
		}
		else
		{
			cInfo.OutputString(_T("Info: Document doesn't support interface %s"), aDocIIDs[i].szName);
		}
	}

	return trPassed;
}


TestResult __cdecl tstCollectionEnum(BrowserInfo &cInfo)
{
	CIPtr(IHTMLDocument2) cpDocElement;
	cInfo.GetDocument(&cpDocElement);
	if (cpDocElement == NULL)
	{
		cInfo.OutputString(_T("Error: No document"));
		return trFailed;
	}
	
	CIPtr(IHTMLElementCollection) cpColl;
	HRESULT hr = cpDocElement->get_all( &cpColl );
	if (hr == S_OK)
	{
		CComPtr<IUnknown> cpUnkEnum;
		cpColl->get__newEnum(&cpUnkEnum);
		if (cpUnkEnum == NULL)
		{
			cInfo.OutputString(_T("Error: No collection"));
			return trFailed;
		}

		CIPtr(IEnumVARIANT) cpEnumVARIANT = cpUnkEnum;
		if (cpEnumVARIANT)
		{
			cInfo.OutputString(_T("Collection has IEnumVARIANT"));
		}
		CIPtr(IEnumUnknown) cpEnumUnknown = cpUnkEnum;
		if (cpEnumUnknown)
		{
			cInfo.OutputString(_T("Collection has IEnumUnknown"));
		}
	}
	else
	{
		cInfo.OutputString(_T("Error: No collection from document"));
		return trFailed;
	}

	return trPassed;
}


void tstDrillerLevel(BrowserInfo &cInfo, IHTMLElementCollection *pCollection, int nLevel)
{
	if (pCollection == NULL)
	{
		return;
	}

	LONG nElements = 0;;
	HRESULT hr = pCollection->get_length( &nElements );
	if (FAILED(hr))
	{
		cInfo.OutputString(_T("Error: Collection failed to return number of elements!"));
		return;
	}

	USES_CONVERSION;
	char szLevel[256];
	memset(szLevel, 0, sizeof(szLevel));
	memset(szLevel, ' ', nLevel);
	TCHAR *szIndent = A2T(szLevel);

	cInfo.OutputString(_T("%sParsing collection..."), szIndent);
	cInfo.OutputString(_T("%sCollection with %d elements"), szIndent, (int) nElements);

	for ( int i=0; i< nElements; i++ )
	{
		VARIANT varIndex;
		varIndex.vt = VT_UINT;
		varIndex.lVal = i;
		
		VARIANT var2;
		VariantInit( &var2 );
		CIPtr(IDispatch) cpDisp; 

		hr = pCollection->item( varIndex, var2, &cpDisp );
		if ( hr != S_OK )
		{
			continue;
		}

		CIPtr(IHTMLElement) cpElem;

		hr = cpDisp->QueryInterface( IID_IHTMLElement, (void **)&cpElem );
		if ( hr == S_OK )
		{

			BSTR bstrTagName = NULL;
			hr = cpElem->get_tagName(&bstrTagName);
			CString szTagName = bstrTagName;
			SysFreeString(bstrTagName);

			BSTR bstrID = NULL;
			hr = cpElem->get_id(&bstrID);
			CString szID = bstrID;
			SysFreeString(bstrID);

			BSTR bstrClassName = NULL;
			hr = cpElem->get_className(&bstrClassName);
			CString szClassName = bstrClassName;
			SysFreeString(bstrClassName);
			
			cInfo.OutputString(_T("%sElement at %d is %s"), szIndent, i, szTagName);
			cInfo.OutputString(_T("%s  id=%s"), szIndent, szID);
			cInfo.OutputString(_T("%s  classname=%s"), szIndent, szClassName);

			CIPtr(IHTMLImgElement) cpImgElem;
			hr = cpDisp->QueryInterface( IID_IHTMLImgElement, (void **)&cpImgElem );
			if ( hr == S_OK )
			{
//				cpImgElem->get_href(&bstr);
			}
			else
			{
				CIPtr(IHTMLAnchorElement) cpAnchElem;
				hr = cpDisp->QueryInterface( IID_IHTMLAnchorElement, (void **)&cpAnchElem );
				if ( hr == S_OK )
				{
//					cpAnchElem->get_href(&bstr);
				}
			}

			CIPtr(IDispatch) cpDispColl;
			hr = cpElem->get_children(&cpDispColl);
			if (hr == S_OK)
			{
				CIPtr(IHTMLElementCollection) cpColl = cpDispColl;
				tstDrillerLevel(cInfo, cpColl, nLevel + 1);
			}
		}
	}

	cInfo.OutputString(_T("%sEnd collection"), szIndent);
}


TestResult __cdecl tstDriller(BrowserInfo &cInfo)
{
	CIPtr(IHTMLDocument2) cpDocElement;
	cInfo.GetDocument(&cpDocElement);
	if (cpDocElement == NULL)
	{
		cInfo.OutputString(_T("Error: No document"));
		return trFailed;
	}
	
	CIPtr(IHTMLElementCollection) cpColl;
	HRESULT hr = cpDocElement->get_all( &cpColl );
	if (hr == S_OK)
	{
		tstDrillerLevel(cInfo, cpColl, 0);
	}

	return trPassed;
}


TestResult __cdecl tstInnerHTML(BrowserInfo &cInfo)
{
	CIPtr(IHTMLDocument2) cpDocElement;
	cInfo.GetDocument(&cpDocElement);
	if (cpDocElement == NULL)
	{
		cInfo.OutputString(_T("Error: No document"));
		return trFailed;
	}
	
	CIPtr(IHTMLElement) cpBody;
	HRESULT hr = cpDocElement->get_body( &cpBody );
	if (hr == S_OK)
	{
        BSTR html = NULL;
        cpBody->get_innerHTML(&html);
        SysFreeString(html);
	}

	return trPassed;
}


TestResult __cdecl tstTesters(BrowserInfo &cInfo)
{
	cInfo.OutputString("Test architecture is reasonably sane!");
	return trPassed;
}


TestResult __cdecl tstControlActive(BrowserInfo &cInfo)
{
	CBrowserCtlSiteInstance *pControlSite = cInfo.pControlSite;
	if (pControlSite == NULL)
	{
		cInfo.OutputString(_T("Error: No control site"));
		return trFailed;
	}

	if (!pControlSite->IsInPlaceActive())
	{
		cInfo.OutputString(_T("Error: Control is not in-place active"));
		return trFailed;
	}

	return trPassed;
}


TestResult __cdecl tstIWebBrowser(BrowserInfo &cInfo)
{
	if (cInfo.pIUnknown == NULL)
	{
		cInfo.OutputString(_T("Error: No control"));
		return trFailed;
	}

	CIPtr(IWebBrowser) cpIWebBrowser = cInfo.pIUnknown;
	if (cpIWebBrowser)
	{
		return trPassed;
	}

	cInfo.OutputString(_T("Error: No IWebBrowser"));
	return trFailed;
}


TestResult __cdecl tstIWebBrowser2(BrowserInfo &cInfo)
{
	if (cInfo.pIUnknown == NULL)
	{
		cInfo.OutputString(_T("Error: No control"));
		return trFailed;
	}
	CIPtr(IWebBrowser2) cpIWebBrowser = cInfo.pIUnknown;
	if (cpIWebBrowser)
	{
		return trPassed;
	}

	cInfo.OutputString(_T("Error: No IWebBrowser2"));
	return trFailed;
}


TestResult __cdecl tstIWebBrowserApp(BrowserInfo &cInfo)
{
	if (cInfo.pIUnknown == NULL)
	{
		cInfo.OutputString(_T("Error: No control"));
		return trFailed;
	}

	CIPtr(IWebBrowserApp) cpIWebBrowser = cInfo.pIUnknown;
	if (cpIWebBrowser)
	{
		return trPassed;
	}

	cInfo.OutputString(_T("Error: No IWebBrowserApp"));
	return trFailed;
}


TestResult __cdecl tstNavigate2(BrowserInfo &cInfo)
{
	return trFailed;
}


TestResult __cdecl tstScriptTest(BrowserInfo &cInfo)
{
	cInfo.nResult = trFailed;

	CTestScriptHelperInstance *pHelper = NULL;
	CTestScriptHelperInstance::CreateInstance(&pHelper);
	if (pHelper)
	{
		pHelper->m_pBrowserInfo = &cInfo;

		CActiveScriptSiteInstance *pSite = NULL;
		CActiveScriptSiteInstance::CreateInstance(&pSite);
		if (pSite)
		{
			// TODO read from registry
			CString szScript;
			szScript.Format(_T("Scripts\\%s"), cInfo.pTest->szName);

			pSite->AddRef();
			pSite->AttachVBScript();
			pSite->AddNamedObject(_T("BrowserInfo"), pHelper, TRUE);
			pSite->ParseScriptFile(szScript);
			pSite->PlayScript();
			pSite->Detach();
			pSite->Release();
		}
	}

	return cInfo.nResult;
}


Test aScripts[] =
{
	{ _T("Script test"), _T("Test that the scripting engine is sane"), tstScriptTest }
};


void __cdecl ScriptSetPopulator(TestSet *pTestSet)
{
	// TODO read from registry
	CString szTestDir(_T("Scripts"));

	CStringList cStringList;
	CFileFind cFinder;
	CString szPattern;


	szPattern.Format(_T("%s\\*.vbs"), szTestDir);
	BOOL bWorking = cFinder.FindFile(szPattern);
	while (bWorking)
	{
		bWorking = cFinder.FindNextFile();
		cStringList.AddTail(cFinder.GetFileName());
	}
	
	szPattern.Format(_T("%s\\*.js"), szTestDir);
	bWorking = cFinder.FindFile(szPattern);
	while (bWorking)
	{
		bWorking = cFinder.FindNextFile();
		cStringList.AddTail(cFinder.GetFileName());
	}

	// Create a set of tests from the scripts found
	Test *pTests = (Test *) malloc(sizeof(Test) * cStringList.GetCount());
	for (int i = 0; i < cStringList.GetCount(); i++)
	{
		CString szScript = cStringList.GetAt(cStringList.FindIndex(i));
		_tcscpy(pTests[i].szName, szScript);
		_tcscpy(pTests[i].szDesc, _T("Run the specified script"));
		pTests[i].pfn = tstScriptTest;
	}

	pTestSet->nTests = cStringList.GetCount();
	pTestSet->aTests = pTests;
}

///////////////////////////////////////////////////////////////////////////////

Test aBasic[] =
{
	{ _T("Test Tester"), _T("Ensure that the testing architecture is working"), tstTesters, trNotRun },
	{ _T("Control basics"), _T("Ensure that the browser control is active"), tstControlActive, trNotRun },
	{ _T("IWebBrowser"), _T("Test if control has an IWebBrowser interface"), tstIWebBrowser, trNotRun },
	{ _T("IWebBrowser2"), _T("Test if control has an IWebBrowser2 interface"), tstIWebBrowser2, trNotRun },
	{ _T("IWebBrowserApp"), _T("Test if control has an IWebBrowserApp interface"), tstIWebBrowserApp, trNotRun }
};


Test aBrowsing[] =
{
	{ _T("IWebBrowser2::Navigate2"), _T("Test if browser can navigate to the test URL"), NULL }
};


Test aDHTML[] =
{
	{ _T("IWebBrowser::get_Document"), _T("Test if browser has a top level element"), tstDocument },
	{ _T("IHTMLElementCollection::get__newEnum"), _T("Test if element collections return enumerations"), tstCollectionEnum },
	{ _T("Parse DOM"), _T("Parse the document DOM"), tstDriller },
    { _T("innerHTML"), _T("Dump the innerHTML for the BODY element"), tstInnerHTML }
};


Test aOther[] =
{
	{ _T("Print Page"), _T("Print the test URL page"), NULL }
};

BEGIN_TESTSET()
    DECL_SET(Basic, "Basic sanity tests")
    DECL_SET(Browsing, "Browsing and navigation tests")
    DECL_SET(DHTML, "Test the DOM")
    DECL_SET(Other, "Other tests")
    DECL_SET_DYNAMIC(Scripts, "Script tests", ScriptSetPopulator)
END_TESTSET()

