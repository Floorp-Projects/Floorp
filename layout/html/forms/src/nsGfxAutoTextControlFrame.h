/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

	NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue);
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
