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

#ifndef __PAGESITE_H_
#define __PAGESITE_H_

// Class that implements IPropertyPageSite interface
class CPropertyPageSite : public IPropertyPageSite {
	public:
		CPropertyPageSite(CPropertyFrameDialog *);

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
	
		// IPropertyPageSite methods
		STDMETHODIMP OnStatusChange(DWORD flags);
		STDMETHODIMP GetLocaleID(LCID FAR* pLocaleID);
		STDMETHODIMP GetPageContainer(LPUNKNOWN FAR* ppUnk);
		STDMETHODIMP TranslateAccelerator(LPMSG lpMsg);

	private:
		CPropertyFrameDialog   *m_pFrame;
		ULONG					m_uRef;
};

#endif /* __PAGESITE_H_ */

