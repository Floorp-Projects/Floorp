/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Chak Nanga <chak@netscape.com> 
 */

// This interface acts as a glue between the required/optional 
// Gecko embedding interfaces and the actual platform specific
// way of doing things - such as updating a statusbar etc.
//
// For ex, in the mfcembed sample the required interfaces such as 
// IWebBrowserChrome etc. are implemented in a XP way in the
// BrowserImp*.cpp files. However, when they get called to update the
// statusbar etc. they call on this interface to get the actual job
// done. During the BrowserFrame creation some object must implement
// this interface and pass the pointer to it via the Init() member of
// the CBrowserImpl class

#ifndef _IBROWSERFRAMEGLUE_H
#define _IBROWSERFRAMEGLUE_H

struct IBrowserFrameGlue {
	// Progress Related Methods
	virtual void UpdateStatusBarText(const PRUnichar *aMessage) = 0;
	virtual void UpdateProgress(PRInt32 aCurrent, PRInt32 aMax) = 0;
	virtual void UpdateBusyState(PRBool aBusy) = 0;
	virtual void UpdateCurrentURI(nsIURI *aLocation) = 0;

	// BrowserFrame Related Methods
	virtual PRBool CreateNewBrowserFrame(PRUint32 chromeMask, 
							PRInt32 x, PRInt32 y, 
							PRInt32 cx, PRInt32 cy,
							nsIWebBrowser ** aWebBrowser) = 0;
	virtual void DestroyBrowserFrame() = 0;
	virtual void GetBrowserFrameTitle(PRUnichar **aTitle) = 0;
	virtual void SetBrowserFrameTitle(const PRUnichar *aTitle) = 0;
	virtual void GetBrowserFramePosition(PRInt32 *aX, PRInt32 *aY) = 0;
	virtual void SetBrowserFramePosition(PRInt32 aX, PRInt32 aY) = 0;
	virtual void GetBrowserFrameSize(PRInt32 *aCX, PRInt32 *aCY) = 0;
	virtual void SetBrowserFrameSize(PRInt32 aCX, PRInt32 aCY) = 0;
	virtual void GetBrowserFramePositionAndSize(PRInt32 *aX, PRInt32 *aY, PRInt32 *aCX, PRInt32 *aCY) = 0;
	virtual void SetBrowserFramePositionAndSize(PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, PRBool fRepaint) = 0;
	virtual void ShowBrowserFrame(PRBool aShow) = 0;
	virtual void SetFocus() = 0;
	virtual void FocusAvailable(PRBool *aFocusAvail) = 0;
	virtual void GetBrowserFrameVisibility(PRBool *aVisible) = 0;
	virtual nsresult FindNamedBrowserItem(const PRUnichar *aName, nsIWebBrowserChrome *aWebBrowserChrome, nsIDocShellTreeItem ** aBrowserItem) = 0;

	// ContextMenu Related Methods
	virtual void ShowContextMenu(PRUint32 aContextFlags, nsIDOMNode *aNode) = 0;

	//Prompt Related Methods
	virtual void Alert(const PRUnichar *dialogTitle, const PRUnichar *text) = 0;
	virtual void Confirm(const PRUnichar *dialogTitle, const PRUnichar *text, PRBool *_retval) = 0;
	virtual void Prompt(const PRUnichar *dialogTitle, const PRUnichar *text,
					const PRUnichar *defaultEditText, PRUnichar **result, 
					PRBool *retval) = 0;
	virtual void PromptPassword(const PRUnichar *dialogTitle, const PRUnichar *text,
					const PRUnichar *checkboxMsg, PRBool *checkboxState,
					PRUnichar **result, PRBool *retval) = 0;
	virtual void PromptUserNamePassword(const PRUnichar *dialogTitle, const PRUnichar *text,
								   const PRUnichar *userNameLabel, const PRUnichar *passwordLabel, 
								   const PRUnichar *checkboxMsg, PRBool *checkboxState,
								   PRUnichar **username, PRUnichar **password,
								   PRBool *retval) = 0;

};

#define	NS_DECL_BROWSERFRAMEGLUE	\
	public:	\
		virtual void UpdateStatusBarText(const PRUnichar *aMessage);	\
		virtual void UpdateProgress(PRInt32 aCurrent, PRInt32 aMax);	\
		virtual void UpdateBusyState(PRBool aBusy);						\
		virtual void UpdateCurrentURI(nsIURI *aLocation);				\
		virtual PRBool CreateNewBrowserFrame(PRUint32 chromeMask, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, nsIWebBrowser** aWebBrowser);	\
		virtual void DestroyBrowserFrame();							\
		virtual void GetBrowserFrameTitle(PRUnichar **aTitle);	\
		virtual void SetBrowserFrameTitle(const PRUnichar *aTitle);	\
		virtual void GetBrowserFramePosition(PRInt32 *aX, PRInt32 *aY);	\
		virtual void SetBrowserFramePosition(PRInt32 aX, PRInt32 aY);	\
		virtual void GetBrowserFrameSize(PRInt32 *aCX, PRInt32 *aCY);	\
		virtual void SetBrowserFrameSize(PRInt32 aCX, PRInt32 aCY);		\
		virtual void GetBrowserFramePositionAndSize(PRInt32 *aX, PRInt32 *aY, PRInt32 *aCX, PRInt32 *aCY);	\
		virtual void SetBrowserFramePositionAndSize(PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, PRBool fRepaint);	\
		virtual void ShowBrowserFrame(PRBool aShow);					\
		virtual void SetFocus();										\
		virtual void FocusAvailable(PRBool *aFocusAvail);				\
		virtual void GetBrowserFrameVisibility(PRBool *aVisible);		\
		virtual nsresult FindNamedBrowserItem(const PRUnichar *aName, nsIWebBrowserChrome *aWebBrowserChrome, nsIDocShellTreeItem ** aBrowserItem);	\
		virtual void ShowContextMenu(PRUint32 aContextFlags, nsIDOMNode *aNode);	\
		virtual void Alert(const PRUnichar *dialogTitle, const PRUnichar *text);	\
		virtual void Confirm(const PRUnichar *dialogTitle, const PRUnichar *text, PRBool *_retval);	\
		virtual void Prompt(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *defaultEditText, PRUnichar **result, PRBool *retval);	\
		virtual void PromptPassword(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *checkboxMsg, PRBool *checkboxState, PRUnichar **result, PRBool *retval);	\
		virtual void PromptUserNamePassword(const PRUnichar *dialogTitle, const PRUnichar *text, const PRUnichar *userNameLabel, const PRUnichar *passwordLabel, const PRUnichar *checkboxMsg, PRBool *checkboxState, PRUnichar **username, PRUnichar **password, PRBool *retval);
		
typedef IBrowserFrameGlue *PBROWSERFRAMEGLUE;

#endif //_IBROWSERFRAMEGLUE_H
