/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsGfxAutoTextControlFrame_h___
#define nsGfxAutoTextControlFrame_h___

#include "nsGfxTextControlFrame.h"
#include "jsapi.h"

class nsITimer;
class nsIScriptContext;
class nsIScriptObjectOwner;


/******************************************************************************
 * nsGfxTextControlFrame
 ******************************************************************************/

class nsGfxAutoTextControlFrame : public nsGfxTextControlFrame
{
private:
	typedef nsGfxTextControlFrame Inherited;

public:
	nsGfxAutoTextControlFrame();
	virtual ~nsGfxAutoTextControlFrame();

	NS_IMETHOD  Init(nsIPresContext&  aPresContext,
					nsIContent*      aContent,
					nsIFrame*        aParent,
					nsIStyleContext* aContext,
					nsIFrame*        aPrevInFlow);

	NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
					nsGUIEvent* aEvent,
					nsEventStatus& aEventStatus);

	NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
	NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

	void ReadAttributes(nsIContent* aContent);
	nsresult SetAutoCompleteString(const nsString& val);

	static void TimerCallback(nsITimer *aTimer, void *aClosure);
	nsresult PrimeTimer();
	void KillTimer();

	enum{	ONSTARTLOOKUP_ID = 0,
			ONAUTOCOMPLETE_ID,
			LAST_ID
		};
	static char* eventName[LAST_ID];
	nsresult SetEventHandlers(PRInt32 handlerID);
	nsresult AddScriptEventHandler(PRInt32 handlerID, const char* handlerName,
								const nsString& aValue, nsIDocument* aDocument);
	nsresult BuildScriptEventHandler(nsIScriptContext* aContext, nsIScriptObjectOwner *aScriptObjectOwner, 
                                const char *aName, const nsString& aFunc, JSObject **mScriptObject);
	nsresult ExecuteScriptEventHandler(PRInt32 handlerID);

protected:
	PRBool					mUseBlurr;
	nsITimer*				mLookupTimer;
	PRInt32					mLookupInterval;
	nsString				mEvtHdlrProp[LAST_ID];
	nsIScriptContext*		mEvtHdlrContext[LAST_ID];
	JSObject*				mEvtHdlrScript[LAST_ID];
};

#endif
