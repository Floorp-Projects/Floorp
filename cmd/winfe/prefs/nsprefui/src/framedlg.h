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

#ifndef __FRAMEDLG_H_
#define __FRAMEDLG_H_

class CPPropertyPage;

class CPropertyFrameDialog {
	public:
		CPropertyFrameDialog(HWND 		 hwndOwner,
   							 int		 x,
							 int		 y,
							 LPCSTR		 lpszCaption,
                             NETHELPFUNC lpfnNetHelp);
	   ~CPropertyFrameDialog();

		// Initialization
		HRESULT		CreatePages(ULONG 						  nCategories,
								LPSPECIFYPROPERTYPAGEOBJECTS *lplpProviders,
								ULONG						  nInitialCategory);

		// Modal processing
		int			DoModal();

		// Event processing
		BOOL		InitDialog(HWND hdlg);
		BOOL		OnCommand(int id, HWND hwndCtl, UINT notifyCode);
		void		OnPaint(HDC);
		void		TreeViewSelChanged(LPNM_TREEVIEW);

		// Called by the property page site to give the property frame
		// a chance to translate accelerators
		HRESULT		TranslateAccelerator(LPMSG);

	protected:
		int			RunModalLoop();

	private:
		HWND				 m_hdlg;
		HWND				 m_hwndOwner;
		int					 m_x, m_y;
		LPCSTR				 m_lpszCaption;
		ULONG				 m_nInitialCategory;
		CPropertyCategories	 m_categories;
		CPropertyPage		*m_pCurPage;
		BOOL				 m_bKeepGoing;
		int					 m_nModalResult;
		RECT				 m_pageRect;
		HFONT				 m_hBoldFont;
		LPBITMAPINFOHEADER	 m_lpGradient;
		HBRUSH				 m_hBrush;
        NETHELPFUNC          m_lpfnNetHelp;

		// Helper routines
		void		FillTreeView();
		void		SizeToFit();
		SIZE		GetMaxPageSize();
		HWND		GetPropertyPageWindow();
		BOOL		PreTranslateMessage(LPMSG);
		void		CheckDefPushButton(HWND, HWND);
};

#endif /* __FRAMEDLG_H_ */

