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

#ifndef __PREFPRIV_H_
#define __PREFPRIV_H_

#include "isppageo.h"

extern HINSTANCE	g_hInstance;

struct CPropertyCategory;

// Structure containing information for each property page
struct CPropertyPage {
	CPropertyCategory  *m_pCategory;
	LPPROPERTYPAGE		m_pPage;
	SIZE				m_size;
	LPCSTR				m_lpszTitle;
	LPCSTR				m_lpszDescription;
	LPCSTR				m_lpszHelpTopic;
	HTREEITEM			m_hitem;

	// Releases the reference to the interface for the property page,
	// and frees any memory
   ~CPropertyPage();

    // Initializes the property page by setting the page site
	// and getting the page info
	HRESULT		Initialize(LPPROPERTYPAGE pPage, LPPROPERTYPAGESITE pSite);
};

// Counted array of property pages
struct CPropertyCategory {
	LPSPECIFYPROPERTYPAGEOBJECTS m_pProvider;
	ULONG				   		 m_nPages;
	CPropertyPage  		   		*m_pPages;

	// Releases the reference to the interface for the property page provider,
	// and destroys each of the property pages objects
   ~CPropertyCategory();

    // Gets the list of property page objects and initializes each of the pages
	HRESULT		CreatePages(LPSPECIFYPROPERTYPAGEOBJECTS, LPPROPERTYPAGESITE);

	// Subscripting operator
	CPropertyPage& operator[](ULONG nIndex) {return m_pPages[nIndex];}
};

// Counted array of CPropertyCategory structures
struct CPropertyCategories {
	ULONG			   m_nCategories;
	CPropertyCategory *m_pCategories;

   ~CPropertyCategories();

    // Asks each category to create its property pages
	HRESULT		Initialize(ULONG nCategories,
						   LPSPECIFYPROPERTYPAGEOBJECTS *,
						   LPPROPERTYPAGESITE);

	// Subscripting operator
	CPropertyCategory& operator[](ULONG nIndex) {return m_pCategories[nIndex];}
};

#endif /* __PREFPRIV_H_ */

