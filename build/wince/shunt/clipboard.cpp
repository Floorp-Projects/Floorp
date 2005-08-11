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
 * The Original Code is WinCE Shunt
 *
 * The Initial Developer of the Original Code is 
 * Douglas F. Turner II  <dougt@meer.net>
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "mozce_internal.h"

extern "C" {
#if 0
}
#endif

static IDataObject* gDataObject = NULL;
static HWND gClipboardWND = NULL; /* we may need to free this */

void oleSetup()
{
  if (gClipboardWND)
    return;
  
  WNDCLASS wndclass;
  ZeroMemory( &wndclass, sizeof(WNDCLASS));
  
  
  wndclass.style          = CS_GLOBALCLASS;
  wndclass.lpfnWndProc    = DefWindowProc;
  wndclass.lpszClassName  = L"OLE_CLIPBOARD";
  
  RegisterClass(&wndclass);
  
  gClipboardWND = CreateWindow(L"OLE_Clipboard",
                         0,
                         0,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         0,
                         0,
                         0,
                         0);
}

class ClipDataObj : public IDataObject
{
public:
  ClipDataObj()
  {
    mRefCnt = 0;
  } 
  
  ~ClipDataObj()
  {  
  }
  
  STDMETHODIMP_(ULONG) AddRef()
  {
    mRefCnt++; 
    return mRefCnt; 
  }
  
  STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject)
  {
    // check to see what interface has been requested
    if(iid == IID_IDataObject || iid == IID_IUnknown)
    {
      AddRef();
      *ppvObject = this;
      return S_OK;
    }
    else
    {
      *ppvObject = 0;
      return E_NOINTERFACE;
    }
  }
  
  STDMETHODIMP_(ULONG) Release()
  {
    mRefCnt--;
    if (mRefCnt == 0)
    {
      delete this;
      return 0;
    }
    
    return mRefCnt;
  }
  
  STDMETHODIMP GetData	(FORMATETC *pFormatEtc, STGMEDIUM *pMedium)
  {
    
    oleSetup();
    
    BOOL b = OpenClipboard(gClipboardWND);
    
    if (!b)
      return E_FAIL;
    
    HANDLE hData = GetClipboardData(pFormatEtc->cfFormat);
    
    LPVOID src = GlobalLock(hData);
    if(src) {
      ULONG  size  = GlobalSize(hData);
      HANDLE hDest = GlobalAlloc(GHND, size);
      LPVOID dest  = GlobalLock(hDest);
      memcpy(dest, src, size);
      
      GlobalUnlock(hDest);
      GlobalUnlock(hData);
      
      hData = hDest;
    }
    
    pMedium->tymed = (hData == 0) ? TYMED_NULL : TYMED_HGLOBAL;
    pMedium->hGlobal = (HGLOBAL)hData;
    pMedium->pUnkForRelease = NULL;
    
    return S_OK;
  }
  
  STDMETHODIMP GetDataHere (LPFORMATETC pFE, LPSTGMEDIUM pSTM)
  {
    return DATA_E_FORMATETC;
  }
  
  STDMETHODIMP QueryGetData (LPFORMATETC pFE)
  {
    return S_OK;
  }
  
  STDMETHODIMP GetCanonicalFormatEtc (LPFORMATETC pFE, LPFORMATETC pCanonFE)
  {
    pFE->ptd = NULL;
    return E_NOTIMPL;
  }
  
  STDMETHODIMP SetData	(LPFORMATETC pFE, LPSTGMEDIUM pSTM, BOOL release)
  {
    return E_NOTIMPL;
  }
  
  STDMETHODIMP EnumFormatEtc	(DWORD dwDirection, LPENUMFORMATETC* ppEnum)
  {
    return E_NOTIMPL;
  }
  
  STDMETHODIMP DAdvise	(LPFORMATETC pFE, DWORD flags, LPADVISESINK pAdvise, DWORD* pConn)
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }
  
  STDMETHODIMP DUnadvise (DWORD pConn)
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }
  
  STDMETHODIMP EnumDAdvise (LPENUMSTATDATA *ppEnum)
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }
private:
  LONG	   mRefCnt;
};



MOZCE_SHUNT_API HRESULT mozce_OleSetClipboard(IDataObject * pDataObj)
{
  MOZCE_PRECHECK
    
	oleSetup();
  
  if (gDataObject)
    gDataObject->Release();
  
  gDataObject = pDataObj;
  
  if (pDataObj) 
  {
    BOOL b = OpenClipboard(gClipboardWND);
    
    if (!b)
      return E_FAIL;
    
    EmptyClipboard();
    
    pDataObj->AddRef();
    
    IEnumFORMATETC* enumerator = NULL;
    pDataObj->EnumFormatEtc(DATADIR_GET, &enumerator);
    if (!enumerator)
      return S_OK;
    
    FORMATETC etc;
    
    while (S_OK == enumerator->Next(1, &etc, NULL))
    {
      if ( etc.tymed == TYMED_HGLOBAL )
      {

		STGMEDIUM medium;
		pDataObj->GetData(&etc, &medium);
        SetClipboardData( etc.cfFormat, medium.hGlobal);
      }
    }
    
    enumerator->Release();
    
    CloseClipboard();
    
  }
  return S_OK;
}

MOZCE_SHUNT_API HRESULT mozce_OleGetClipboard(IDataObject ** pDataObj)
{
  MOZCE_PRECHECK
    oleSetup();
  
  if (pDataObj)
    *pDataObj = gDataObject;
  
  if (!*pDataObj)
  {
    *pDataObj = new ClipDataObj();
    if (!*pDataObj)
      return E_FAIL;

    gDataObject = *pDataObj;
  }
  
  (*pDataObj)->AddRef();
  return S_OK;
}

MOZCE_SHUNT_API HRESULT mozce_OleFlushClipboard()
{
  MOZCE_PRECHECK
    oleSetup();
  
  mozce_OleSetClipboard(NULL);
  return S_OK;
}


MOZCE_SHUNT_API BOOL mozce_IsClipboardFormatAvailable(UINT format)
{
  if (gClipboardWND)
  {
    BOOL b = OpenClipboard(gClipboardWND);
    if (!b)
      return E_FAIL;
    
    IEnumFORMATETC* enumerator = NULL;
    gDataObject->EnumFormatEtc(DATADIR_GET, &enumerator);
    if (!enumerator)
      return S_OK;
    
    FORMATETC etc;
    
    while (S_OK == enumerator->Next(1, &etc, NULL))
    {
      if ( etc.cfFormat == format)
      {
        enumerator->Release();
        CloseClipboard();
        return true;
      }
    }
    enumerator->Release();
    CloseClipboard();
  }
  
  return IsClipboardFormatAvailable(format);
}

#if 0
{
#endif
} /* extern "C" */
