/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
#ifndef CPMOZILLACONTROL_H
#define CPMOZILLACONTROL_H

//////////////////////////////////////////////////////////////////////////////
// CProxyDWebBrowserEvents
template <class T>
class CProxyDWebBrowserEvents : public IConnectionPointImpl<T, &DIID_DWebBrowserEvents, CComDynamicUnkArray>
{
public:
//methods:
//DWebBrowserEvents : IDispatch
public:
    void Fire_BeforeNavigate(
        BSTR URL,
        long Flags,
        BSTR TargetFrameName,
        VARIANT * PostData,
        BSTR Headers,
        VARIANT_BOOL * Cancel)
    {
        VARIANTARG* pvars = new VARIANTARG[6];
        for (int i = 0; i < 6; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[5].vt = VT_BSTR;
                pvars[5].bstrVal= URL;
                pvars[4].vt = VT_I4;
                pvars[4].lVal= Flags;
                pvars[3].vt = VT_BSTR;
                pvars[3].bstrVal= TargetFrameName;
                pvars[2].vt = VT_VARIANT | VT_BYREF;
                pvars[2].byref= PostData;
                pvars[1].vt = VT_BSTR;
                pvars[1].bstrVal= Headers;
                pvars[0].vt = VT_BOOL | VT_BYREF;
                pvars[0].byref= Cancel;
                DISPPARAMS disp = { pvars, NULL, 6, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x64, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_NavigateComplete(
        BSTR URL)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BSTR;
                pvars[0].bstrVal= URL;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x65, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_StatusTextChange(
        BSTR Text)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BSTR;
                pvars[0].bstrVal= Text;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x66, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_ProgressChange(
        long Progress,
        long ProgressMax)
    {
        VARIANTARG* pvars = new VARIANTARG[2];
        for (int i = 0; i < 2; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[1].vt = VT_I4;
                pvars[1].lVal= Progress;
                pvars[0].vt = VT_I4;
                pvars[0].lVal= ProgressMax;
                DISPPARAMS disp = { pvars, NULL, 2, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x6c, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_DownloadComplete()
    {
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                DISPPARAMS disp = { NULL, NULL, 0, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x68, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
    }
    void Fire_CommandStateChange(
        long Command,
        VARIANT_BOOL Enable)
    {
        VARIANTARG* pvars = new VARIANTARG[2];
        for (int i = 0; i < 2; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[1].vt = VT_I4;
                pvars[1].lVal= Command;
                pvars[0].vt = VT_BOOL;
                pvars[0].boolVal= Enable;
                DISPPARAMS disp = { pvars, NULL, 2, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x69, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_DownloadBegin()
    {
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                DISPPARAMS disp = { NULL, NULL, 0, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x6a, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
    }
    void Fire_NewWindow(
        BSTR URL,
        long Flags,
        BSTR TargetFrameName,
        VARIANT * PostData,
        BSTR Headers,
        VARIANT_BOOL * Processed)
    {
        VARIANTARG* pvars = new VARIANTARG[6];
        for (int i = 0; i < 6; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[5].vt = VT_BSTR;
                pvars[5].bstrVal= URL;
                pvars[4].vt = VT_I4;
                pvars[4].lVal= Flags;
                pvars[3].vt = VT_BSTR;
                pvars[3].bstrVal= TargetFrameName;
                pvars[2].vt = VT_VARIANT | VT_BYREF;
                pvars[2].byref= PostData;
                pvars[1].vt = VT_BSTR;
                pvars[1].bstrVal= Headers;
                pvars[0].vt = VT_BOOL | VT_BYREF;
                pvars[0].byref= Processed;
                DISPPARAMS disp = { pvars, NULL, 6, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x6b, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_TitleChange(
        BSTR Text)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BSTR;
                pvars[0].bstrVal= Text;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x71, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_FrameBeforeNavigate(
        BSTR URL,
        long Flags,
        BSTR TargetFrameName,
        VARIANT * PostData,
        BSTR Headers,
        VARIANT_BOOL * Cancel)
    {
        VARIANTARG* pvars = new VARIANTARG[6];
        for (int i = 0; i < 6; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[5].vt = VT_BSTR;
                pvars[5].bstrVal= URL;
                pvars[4].vt = VT_I4;
                pvars[4].lVal= Flags;
                pvars[3].vt = VT_BSTR;
                pvars[3].bstrVal= TargetFrameName;
                pvars[2].vt = VT_VARIANT | VT_BYREF;
                pvars[2].byref= PostData;
                pvars[1].vt = VT_BSTR;
                pvars[1].bstrVal= Headers;
                pvars[0].vt = VT_BOOL | VT_BYREF;
                pvars[0].byref= Cancel;
                DISPPARAMS disp = { pvars, NULL, 6, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xc8, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_FrameNavigateComplete(
        BSTR URL)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BSTR;
                pvars[0].bstrVal= URL;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xc9, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_FrameNewWindow(
        BSTR URL,
        long Flags,
        BSTR TargetFrameName,
        VARIANT * PostData,
        BSTR Headers,
        VARIANT_BOOL * Processed)
    {
        VARIANTARG* pvars = new VARIANTARG[6];
        for (int i = 0; i < 6; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[5].vt = VT_BSTR;
                pvars[5].bstrVal= URL;
                pvars[4].vt = VT_I4;
                pvars[4].lVal= Flags;
                pvars[3].vt = VT_BSTR;
                pvars[3].bstrVal= TargetFrameName;
                pvars[2].vt = VT_VARIANT | VT_BYREF;
                pvars[2].byref= PostData;
                pvars[1].vt = VT_BSTR;
                pvars[1].bstrVal= Headers;
                pvars[0].vt = VT_BOOL | VT_BYREF;
                pvars[0].byref= Processed;
                DISPPARAMS disp = { pvars, NULL, 6, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xcc, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_Quit(
        VARIANT_BOOL * Cancel)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BOOL | VT_BYREF;
                pvars[0].byref= Cancel;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x67, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_WindowMove()
    {
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                DISPPARAMS disp = { NULL, NULL, 0, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x6d, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
    }
    void Fire_WindowResize()
    {
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                DISPPARAMS disp = { NULL, NULL, 0, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x6e, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
    }
    void Fire_WindowActivate()
    {
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                DISPPARAMS disp = { NULL, NULL, 0, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x6f, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
    }
    void Fire_PropertyChange(
        BSTR Property)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BSTR;
                pvars[0].bstrVal= Property;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x70, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }

};


//////////////////////////////////////////////////////////////////////////////
// CProxyDWebBrowserEvents2
template <class T>
class CProxyDWebBrowserEvents2 : public IConnectionPointImpl<T, &DIID_DWebBrowserEvents2, CComDynamicUnkArray>
{
public:
//methods:
//DWebBrowserEvents2 : IDispatch
public:
    void Fire_StatusTextChange(
        BSTR Text)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BSTR;
                pvars[0].bstrVal= Text;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x66, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_ProgressChange(
        long Progress,
        long ProgressMax)
    {
        VARIANTARG* pvars = new VARIANTARG[2];
        for (int i = 0; i < 2; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[1].vt = VT_I4;
                pvars[1].lVal= Progress;
                pvars[0].vt = VT_I4;
                pvars[0].lVal= ProgressMax;
                DISPPARAMS disp = { pvars, NULL, 2, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x6c, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_CommandStateChange(
        long Command,
        VARIANT_BOOL Enable)
    {
        VARIANTARG* pvars = new VARIANTARG[2];
        for (int i = 0; i < 2; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[1].vt = VT_I4;
                pvars[1].lVal= Command;
                pvars[0].vt = VT_BOOL;
                pvars[0].boolVal= Enable;
                DISPPARAMS disp = { pvars, NULL, 2, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x69, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_DownloadBegin()
    {
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                DISPPARAMS disp = { NULL, NULL, 0, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x6a, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
    }
    void Fire_DownloadComplete()
    {
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                DISPPARAMS disp = { NULL, NULL, 0, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x68, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
    }
    void Fire_TitleChange(
        BSTR Text)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BSTR;
                pvars[0].bstrVal= Text;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x71, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_PropertyChange(
        BSTR szProperty)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BSTR;
                pvars[0].bstrVal= szProperty;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x70, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_BeforeNavigate2(
        IDispatch * pDisp,
        VARIANT * URL,
        VARIANT * Flags,
        VARIANT * TargetFrameName,
        VARIANT * PostData,
        VARIANT * Headers,
        VARIANT_BOOL * Cancel)
    {
        VARIANTARG* pvars = new VARIANTARG[7];
        for (int i = 0; i < 7; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[6].vt = VT_DISPATCH;
                pvars[6].pdispVal= pDisp;
                pvars[5].vt = VT_VARIANT | VT_BYREF;
                pvars[5].byref= URL;
                pvars[4].vt = VT_VARIANT | VT_BYREF;
                pvars[4].byref= Flags;
                pvars[3].vt = VT_VARIANT | VT_BYREF;
                pvars[3].byref= TargetFrameName;
                pvars[2].vt = VT_VARIANT | VT_BYREF;
                pvars[2].byref= PostData;
                pvars[1].vt = VT_VARIANT | VT_BYREF;
                pvars[1].byref= Headers;
                pvars[0].vt = VT_BOOL | VT_BYREF;
                pvars[0].byref= Cancel;
                DISPPARAMS disp = { pvars, NULL, 7, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xfa, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_NewWindow2(
        IDispatch * * ppDisp,
        VARIANT_BOOL * Cancel)
    {
        VARIANTARG* pvars = new VARIANTARG[2];
        for (int i = 0; i < 2; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[1].vt = VT_DISPATCH | VT_BYREF;
                pvars[1].byref= ppDisp;
                pvars[0].vt = VT_BOOL | VT_BYREF;
                pvars[0].byref= Cancel;
                DISPPARAMS disp = { pvars, NULL, 2, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xfb, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_NavigateComplete2(
        IDispatch * pDisp,
        VARIANT * URL)
    {
        VARIANTARG* pvars = new VARIANTARG[2];
        for (int i = 0; i < 2; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[1].vt = VT_DISPATCH;
                pvars[1].pdispVal= pDisp;
                pvars[0].vt = VT_VARIANT | VT_BYREF;
                pvars[0].byref= URL;
                DISPPARAMS disp = { pvars, NULL, 2, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xfc, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_DocumentComplete(
        IDispatch * pDisp,
        VARIANT * URL)
    {
        VARIANTARG* pvars = new VARIANTARG[2];
        for (int i = 0; i < 2; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[1].vt = VT_DISPATCH;
                pvars[1].pdispVal= pDisp;
                pvars[0].vt = VT_VARIANT | VT_BYREF;
                pvars[0].byref= URL;
                DISPPARAMS disp = { pvars, NULL, 2, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x103, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_OnQuit()
    {
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                DISPPARAMS disp = { NULL, NULL, 0, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xfd, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
    }
    void Fire_OnVisible(
        VARIANT_BOOL Visible)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BOOL;
                pvars[0].boolVal= Visible;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xfe, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_OnToolBar(
        VARIANT_BOOL ToolBar)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BOOL;
                pvars[0].boolVal= ToolBar;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0xff, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_OnMenuBar(
        VARIANT_BOOL MenuBar)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BOOL;
                pvars[0].boolVal= MenuBar;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x100, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_OnStatusBar(
        VARIANT_BOOL StatusBar)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BOOL;
                pvars[0].boolVal= StatusBar;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x101, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_OnFullScreen(
        VARIANT_BOOL FullScreen)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BOOL;
                pvars[0].boolVal= FullScreen;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x102, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }
    void Fire_OnTheaterMode(
        VARIANT_BOOL TheaterMode)
    {
        VARIANTARG* pvars = new VARIANTARG[1];
        for (int i = 0; i < 1; i++)
            VariantInit(&pvars[i]);
        T* pT = (T*)this;
        pT->Lock();
        IUnknown** pp = m_vec.begin();
        while (pp < m_vec.end())
        {
            if (*pp != NULL)
            {
                pvars[0].vt = VT_BOOL;
                pvars[0].boolVal= TheaterMode;
                DISPPARAMS disp = { pvars, NULL, 1, 0 };
                IDispatch* pDispatch = reinterpret_cast<IDispatch*>(*pp);
                pDispatch->Invoke(0x104, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &disp, NULL, NULL, NULL);
            }
            pp++;
        }
        pT->Unlock();
        delete[] pvars;
    }

};


#endif