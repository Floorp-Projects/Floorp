/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The original code is RaptorCanvas
 * 
 * The Initial Developer of the Original Code is Kirk Baker <kbaker@eb.com> and * Ian Wilkinson <iw@ennoble.com
 */

/*
 * nsActions.h
 */
 
#ifndef nsActions_h___
#define nsActions_h___

#ifndef XP_UNIX
//PENDING(mark): Will this work on the Mac????
#include <windows.h>
#endif
#include "nsIWebShell.h"
#include "nsString.h"
#include "plevent.h"


class nsActionEvent {
public:
					nsActionEvent		(PRBool synchronous);
	virtual		   ~nsActionEvent		() {};
	virtual void *	handleEvent			(void) { return NULL;};
			void	destroyEvent		(void) { delete this; };
					operator PLEvent*	() { return &mEvent; };
			PRBool	isSynchronous		(void) { return mSynchronous; };
			PRBool	isComplete			(void);
			void	setComplete			(PRBool complete) { mComplete = complete; };
			void *	getResult			(void) { return mResult; };
			void	setResult			(void * result) { mResult = result; };

protected:
	PLEvent			mEvent;
	void		*	mResult;
	PRBool			mSynchronous;
	PRBool			mComplete;
};


class wsResizeEvent : public nsActionEvent {
public:
					wsResizeEvent		(nsIWebShell* webShell, PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
	PRInt32			mLeft;
	PRInt32			mBottom;
	PRInt32			mWidth;
	PRInt32			mHeight;
};


class wsLoadURLEvent : public nsActionEvent {
public:
					wsLoadURLEvent		(nsIWebShell* webShell, PRUnichar * urlString);
				   ~wsLoadURLEvent		();
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
#ifdef NECKO
	nsString	*	mURL;
#else
	nsString1	*	mURL;
#endif
};


class wsStopEvent : public nsActionEvent {
public:
					wsStopEvent			(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsShowEvent : public nsActionEvent {
public:
					wsShowEvent			(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsHideEvent : public nsActionEvent {
public:
					wsHideEvent			(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsMoveToEvent : public nsActionEvent {
public:
					wsMoveToEvent		(nsIWebShell* webShell, PRInt32 x, PRInt32 y);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
	PRInt32			mX;
	PRInt32			mY;
};



class wsSetFocusEvent : public nsActionEvent {
public:
					wsSetFocusEvent		(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsRemoveFocusEvent : public nsActionEvent {
public:
					wsRemoveFocusEvent	(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsRepaintEvent : public nsActionEvent {
public:
					wsRepaintEvent		(nsIWebShell* webShell, PRBool forceRepaint);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
	PRBool			mForceRepaint;
};



class wsCanBackEvent : public nsActionEvent {
public:
					wsCanBackEvent		(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsCanForwardEvent : public nsActionEvent {
public:
					wsCanForwardEvent	(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsBackEvent : public nsActionEvent {
public:
					wsBackEvent			(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsForwardEvent : public nsActionEvent {
public:
					wsForwardEvent		(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsGoToEvent : public nsActionEvent {
public:
					wsGoToEvent			(nsIWebShell* webShell, PRInt32 historyIndex);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
	PRInt32			mHistoryIndex;
};



class wsGetHistoryLengthEvent : public nsActionEvent {
public:
					wsGetHistoryLengthEvent
										(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsGetHistoryIndexEvent : public nsActionEvent {
public:
					wsGetHistoryIndexEvent
										(nsIWebShell* webShell);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
};



class wsGetURLEvent : public nsActionEvent {
public:
					wsGetURLEvent		(nsIWebShell* webShell, PRInt32 historyIndex);
	void	*		handleEvent			(void);

protected:
	nsIWebShell	*	mWebShell;
	PRInt32			mHistoryIndex;
};


#endif /* nsActions_h___ */

// EOF
