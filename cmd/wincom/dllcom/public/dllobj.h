/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __DllComClassObject_H
#define __DllComClassObject_H

//  ALL COM IMPLEMENTED OBJECTS MUST DERIVE FROM THIS
//      CLASS (or do what it does).

//  All your COM interface exposing classes which you implement
//      in your consumer DLL must derive from this class
//      such that it can perform some minimal DLL maintenence
//      and ensure that you do not forget to do so.
class CComClass {
private:
    CRefDll m_Ref;  //  Ref count dll instance.
public:
    CComClass(IUnknown *pAggregate = NULL);
    virtual ~CComClass();

    //  external IUnknown implementation inside of the class.
private:
    DWORD m_ulCount;
    IUnknown *m_pAggregate;
public:
    HRESULT ObjectQueryInterface(REFIID iid, void **ppObj, BOOL bIUnknown = FALSE);
    virtual HRESULT CustomQueryInterface(REFIID iid, void **ppObj);
    DWORD ObjectAddRef(BOOL bIUnknown = FALSE);
    virtual DWORD CustomAddRef();
    DWORD ObjectRelease(BOOL bIUnknown = FALSE);
    virtual DWORD CustomRelease();

    //  Nested class interface implementation.
private:
    class CImplUnknown : public IUnknown    {
    public:
        CComClass *m_pObject;
        STDMETHODIMP QueryInterface(REFIID iid, void **ppObj);
        STDMETHODIMP_(DWORD) AddRef();
        STDMETHODIMP_(DWORD) Release();
    } m_IUnknown;
};

#endif // __DllComClassObject_H