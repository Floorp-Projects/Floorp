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

#ifndef __ISPPAGEO_H_
#define __ISPPAGEO_H_

/////////////////////////////////////////////////////////////////////////////
// ISpecifyPropertyPageObjects interface

#ifdef __cplusplus
interface ISpecifyPropertyPageObjects;
#else
typedef interface ISpecifyPropertyPageObjects ISpecifyPropertyPageObjects;
#endif

typedef ISpecifyPropertyPageObjects FAR* LPSPECIFYPROPERTYPAGEOBJECTS;

#undef  INTERFACE
#define INTERFACE ISpecifyPropertyPageObjects

typedef struct tagCAPPAGE
{
	ULONG 		    cElems;
	LPPROPERTYPAGE *pElems;
} CAPPAGE;

// ISpecifyPropertyPageObjects is similar to ISpecifyPropertyPages except
// that method GetPageObjects returns the IPropertyPage * pointers
//
// The caller must call IUnknown::Release() for each of the page objects
DECLARE_INTERFACE_(ISpecifyPropertyPageObjects, IUnknown)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// ISpecifyPropertyPageObjects methods
	STDMETHOD(GetPageObjects)(THIS_ CAPPAGE FAR* pPages) PURE;
};

#endif /* __ISPPAGEO_H_ */

