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

#include "nsCOMPtr.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectData.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"

#include "nsGfxAutoTextControlFrame.h"


static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectDataIID, NS_ISCRIPTGLOBALOBJECTDATA_IID);

extern nsresult NS_NewNativeTextControlFrame(nsIFrame** aNewFrame);


nsresult NS_NewGfxAutoTextControlFrame(nsIFrame** aNewFrame)
{
	NS_PRECONDITION(aNewFrame, "null OUT ptr");
	if (nsnull == aNewFrame)
		return NS_ERROR_NULL_POINTER;

	*aNewFrame = new nsGfxAutoTextControlFrame;
	if (nsnull == aNewFrame)
		return NS_ERROR_OUT_OF_MEMORY;

	nsresult result = ((nsGfxAutoTextControlFrame*)(*aNewFrame))->InitTextControl();
	if (NS_FAILED(result))
	{ // can't properly initialized ender, probably it isn't installed
		delete *aNewFrame;
		result = NS_NewNativeTextControlFrame(aNewFrame);
	}

	return result;
}

char* nsGfxAutoTextControlFrame::eventName[] = {"onstartlookup", "onautocomplete"};


nsGfxAutoTextControlFrame::nsGfxAutoTextControlFrame() :
	mLookupInterval(300),
	mLookupTimer(nsnull),
	mUseBlurr(PR_FALSE)
{
	PRInt32 i;
	for (i = 0; i < LAST_ID; i ++)
	{
		mEvtHdlrContext[i] = nsnull;
		mEvtHdlrScript[i] = nsnull;
	}
}


nsGfxAutoTextControlFrame::~nsGfxAutoTextControlFrame()
{
	KillTimer();
}


nsresult nsGfxAutoTextControlFrame::Init(nsIPresContext&  aPresContext,
				nsIContent*      aContent,
				nsIFrame*        aParent,
				nsIStyleContext* aContext,
				nsIFrame*        aPrevInFlow)
{
	ReadAttributes(aContent);
	return(Inherited::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow));
}

nsresult nsGfxAutoTextControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                              nsGUIEvent* aEvent,
                                              nsEventStatus& aEventStatus)
{
	if (nsEventStatus_eConsumeNoDefault == aEventStatus)
		return NS_OK;

	switch(aEvent->message)
	{
		case NS_KEY_UP: //Set lookup timer only when user release the key and if the key isn't return or enter 
			if (NS_KEY_EVENT == aEvent->eventStructType)
			{
				KillTimer();
				nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
				switch (keyEvent->keyCode)
				{
					case NS_VK_ENTER	: break;
					case NS_VK_RETURN	: break;
					
					case NS_VK_DELETE	:
					case NS_VK_BACK		:
						if (mUseBlurr)
							PrimeTimer();
						break;
					
					default				:
						if (mUseBlurr)
						{
							nsString emptyStr("");
							SetAutoCompleteString(emptyStr);
						}
						PrimeTimer();
						break;
				}
			}
			break;
	
		case NS_KEY_DOWN: //If user press return or enter, fire onautocomplete
      		if (NS_KEY_EVENT == aEvent->eventStructType)
      		{
				nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
				switch (keyEvent->keyCode)
				{
					case NS_VK_ENTER	: ExecuteScriptEventHandler(ONAUTOCOMPLETE_ID);		break;
					case NS_VK_RETURN	: ExecuteScriptEventHandler(ONAUTOCOMPLETE_ID);		break;
				}
        	}
			break;
	}

	return(Inherited::HandleEvent(aPresContext, aEvent, aEventStatus));
}


nsresult nsGfxAutoTextControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  	if (nsHTMLAtoms::autocomplete == aName)
  	{
  		SetAutoCompleteString(aValue);
  		//return NS_OK;
   	}

	return(Inherited::SetProperty(aName, aValue));
}


nsresult nsGfxAutoTextControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  	if (nsHTMLAtoms::autocomplete == aName)
  	{
  		//GetAutoCompleteString(aValue);
  		//return NS_OK;
   	}
	return(Inherited::GetProperty(aName, aValue));
}


void nsGfxAutoTextControlFrame::ReadAttributes(nsIContent* aContent)
{
	nsString val;

	if (NS_SUCCEEDED(aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::autocompletetimeout, val)))
		if (! val.IsEmpty())
		{
			PRInt32 aErrorCode;
			mLookupInterval = val.ToInteger(&aErrorCode);
		}
	
	if (NS_SUCCEEDED(aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::autocompletetype, val)))
	{
		if (! val.IsEmpty())
			mUseBlurr = (val == "blurr");
	}
}


nsresult nsGfxAutoTextControlFrame::SetAutoCompleteString(const nsString& val)
{
	if (mEditor)
	{
		//ducarroz: I need to add the given string at the end of the input field data and select it...
	}
	return NS_OK;
}

 
void nsGfxAutoTextControlFrame::KillTimer()
{
	if (mLookupTimer)
	{
		mLookupTimer->Cancel();
		NS_RELEASE(mLookupTimer);
	}
}


nsresult nsGfxAutoTextControlFrame::PrimeTimer()
{
	KillTimer();
	
	nsresult err = NS_NewTimer(&mLookupTimer);
	if (NS_FAILED(err))
		return err;
		
	mLookupTimer->Init(TimerCallback, this, mLookupInterval);
	return NS_OK;
}


/* static */ void nsGfxAutoTextControlFrame::TimerCallback(nsITimer *aTimer, void *aClosure)
{
	nsGfxAutoTextControlFrame* theAutoText = NS_REINTERPRET_CAST(nsGfxAutoTextControlFrame*, aClosure);
	if (!theAutoText)
		return;
	
	theAutoText->ExecuteScriptEventHandler(ONSTARTLOOKUP_ID);
}


nsresult nsGfxAutoTextControlFrame::SetEventHandlers(PRInt32 handlerID)
{
	if (nsnull == mContent)
		return NS_ERROR_FAILURE;
	
	nsIDOMHTMLInputElement* inputElem = nsnull;
	nsresult result = mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElem);
	if ((NS_OK == result) && inputElem)
	{
		nsString value;

		inputElem->GetAttribute(eventName[handlerID], value);
		if (mEvtHdlrProp[handlerID] != value)
		{
			mEvtHdlrProp[handlerID] = value;
			
  			nsIDocument* doc = nsnull;
			mContent->GetDocument(doc);
			AddScriptEventHandler(handlerID, eventName[handlerID], value, doc);
		}
	
		NS_RELEASE(inputElem);
	}
	return result;
}


nsresult nsGfxAutoTextControlFrame::AddScriptEventHandler(PRInt32 handlerID, const char* handlerName, const nsString& aFunc, nsIDocument* aDocument)
{
	nsresult ret = NS_OK;
	nsIScriptContext* context;
	nsIScriptContextOwner* owner;

	if (nsnull != aDocument)
	{
    	owner = aDocument->GetScriptContextOwner();
		if (owner)
		{
			if (NS_OK == owner->GetScriptContext(&context))
			{
        		nsIScriptObjectOwner* cowner;
        		if (NS_OK == mContent->QueryInterface(kIScriptObjectOwnerIID,
                                              (void**) &cowner))
				{
					JSObject *mScriptObject;
					ret = BuildScriptEventHandler(context, cowner, handlerName, aFunc, &mScriptObject);
					if (NS_SUCCEEDED(ret))
					{
						mEvtHdlrContext[handlerID] = context;
						mEvtHdlrScript[handlerID] = mScriptObject;
					}
          			NS_RELEASE(cowner);
        		}
        		NS_RELEASE(context);
			}
		NS_RELEASE(owner);
		}
	}
  return ret;
}


nsresult nsGfxAutoTextControlFrame::BuildScriptEventHandler(nsIScriptContext* aContext, nsIScriptObjectOwner *aScriptObjectOwner, 
                                const char *aName, const nsString& aFunc, JSObject **mScriptObject)
{
	nsIScriptGlobalObject *global;
	nsIScriptGlobalObjectData *globalData;
	nsIPrincipal * prin = nsnull;
	*mScriptObject = nsnull;
	global = aContext->GetGlobalObject();
	if (global && NS_SUCCEEDED(global->QueryInterface(kIScriptGlobalObjectDataIID, (void**)&globalData)))
	{
		if (NS_FAILED(globalData->GetPrincipal(& prin)))
		{
			NS_RELEASE(global);
			NS_RELEASE(globalData);
			return NS_ERROR_FAILURE;
		}
    
		NS_RELEASE(globalData);
	}
	NS_IF_RELEASE(global);
  JSPrincipals * jsprin;
  prin->ToJSPrincipal(& jsprin);
	if (NS_OK == aScriptObjectOwner->GetScriptObject(aContext, (void**)mScriptObject))
	{
		JSContext* mJSContext = (JSContext*)aContext->GetNativeContext();
		if (nsnull != aName)
		{
			JS_CompileUCFunctionForPrincipals(mJSContext, *mScriptObject, jsprin, aName,
		           0, nsnull, (jschar*)aFunc.GetUnicode(), aFunc.Length(),
		           nsnull, 0);
			return NS_OK;
		}
	}
	
	return NS_ERROR_FAILURE;
}

nsresult nsGfxAutoTextControlFrame::ExecuteScriptEventHandler(PRInt32 handlerID)
{
	jsval funval, result;
	SetEventHandlers(handlerID);
	if (mEvtHdlrContext[handlerID] && mEvtHdlrScript[handlerID])
	{
		JSContext* mJSContext = (JSContext*)mEvtHdlrContext[handlerID]->GetNativeContext();
		if (!JS_LookupProperty(mJSContext, mEvtHdlrScript[handlerID], eventName[handlerID], &funval))
      return NS_ERROR_FAILURE;
    if (JS_TypeOfValue(mJSContext, funval) != JSTYPE_FUNCTION)
      return NS_OK;
		JS_CallFunctionValue(mJSContext, mEvtHdlrScript[handlerID], funval, 0, nsnull, &result);
	}
  return NS_OK;
}
