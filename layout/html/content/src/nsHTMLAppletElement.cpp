/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIDOMHTMLAppletElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"

#if defined(OJI) && !defined(XP_UNIX)
#include "nsIServiceManager.h"
#include "nsIJVMManager.h"
#include "nsILiveConnectManager.h"
#include "nsIPluginInstance.h"
#include "nsIJVMPluginInstance.h"
#endif

// XXX this is to get around conflicts with windows.h defines
// introduced through jni.h
#ifdef XP_PC
#undef GetClassName
#undef GetObject
#endif

static NS_DEFINE_IID(kIDOMHTMLAppletElementIID, NS_IDOMHTMLAPPLETELEMENT_IID);

class nsHTMLAppletElement : public nsIDOMHTMLAppletElement,
                            public nsIScriptObjectOwner,
                            public nsIDOMEventReceiver,
                            public nsIHTMLContent
{
public:
  nsHTMLAppletElement(nsIAtom* aTag);
  virtual ~nsHTMLAppletElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLAppletElement
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetAlt(nsString& aAlt);
  NS_IMETHOD SetAlt(const nsString& aAlt);
  NS_IMETHOD GetArchive(nsString& aArchive);
  NS_IMETHOD SetArchive(const nsString& aArchive);
  NS_IMETHOD GetCode(nsString& aCode);
  NS_IMETHOD SetCode(const nsString& aCode);
  NS_IMETHOD GetCodeBase(nsString& aCodeBase);
  NS_IMETHOD SetCodeBase(const nsString& aCodeBase);
  NS_IMETHOD GetHeight(nsString& aHeight);
  NS_IMETHOD SetHeight(const nsString& aHeight);
  NS_IMETHOD GetHspace(nsString& aHspace);
  NS_IMETHOD SetHspace(const nsString& aHspace);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetObject(nsString& aObject);
  NS_IMETHOD SetObject(const nsString& aObject);
  NS_IMETHOD GetVspace(nsString& aVspace);
  NS_IMETHOD SetVspace(const nsString& aVspace);
  NS_IMETHOD GetWidth(nsString& aWidth);
  NS_IMETHOD SetWidth(const nsString& aWidth);

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext,
                             void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
  PRBool mReflectedApplet;
};

nsresult
NS_NewHTMLAppletElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLAppletElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLAppletElement::nsHTMLAppletElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mReflectedApplet = PR_FALSE;
}

nsHTMLAppletElement::~nsHTMLAppletElement()
{
}

NS_IMPL_ADDREF(nsHTMLAppletElement)

NS_IMPL_RELEASE(nsHTMLAppletElement)

nsresult
nsHTMLAppletElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLAppletElementIID)) {
    nsIDOMHTMLAppletElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLAppletElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLAppletElement* it = new nsHTMLAppletElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Archive, archive)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Code, code)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Height, height)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Object, object)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Width, width)

NS_IMETHODIMP
nsHTMLAppletElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (nsGenericHTMLElement::ParseImageAttribute(aAttribute,
                                                     aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLAppletElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::AlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (nsGenericHTMLElement::ImageAttributeToString(aAttribute,
                                                        aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapImageBorderAttributesInto(aAttributes, aContext, aPresContext, nsnull);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLAppletElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}



NS_IMETHODIMP
nsHTMLAppletElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLAppletElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}

#if defined(OJI) && !defined(XP_UNIX)
extern nsresult NS_GetObjectFramePluginInstance(nsIFrame* aFrame, nsIPluginInstance*& aPluginInstance);
#endif

/**
 * For backwards compatibility an applet element's JavaScript object should expose both the public 
 * fields of the applet, and the attributes of the applet tag. The call to nsGenericElement::GetScriptObject
 * takes case of the tag attributes. Here we generate a JavaScript reference to the applet object itself,
 * and set its __proto__ property to the tag object. That way, if the Java applet has public fields that
 * shadow the tag attributes, the applet's fields take precedence.
 */
NS_IMETHODIMP
nsHTMLAppletElement::GetScriptObject(nsIScriptContext* aContext,
                                     void** aScriptObject)
{
#if defined(OJI) && !defined(XP_UNIX)
	nsresult rv = NS_OK;
	if (!mReflectedApplet) {
		// 1. get the script object corresponding to the <APPLET> element itself.
		JSObject* elementObject = nsnull;
		rv = mInner.GetScriptObject(aContext, (void**)&elementObject);
		if (NS_OK != rv)
			return rv;
	
		// 2. get the plugin instance corresponding to this element.
		nsIPresShell* shell = mInner.mDocument->GetShellAt(0);
		nsIFrame* frame = nsnull;
		shell->GetPrimaryFrameFor(mInner.mContent, &frame);

		// 3. get the Java object corresponding to this applet, and reflect it into
		// JavaScript using the LiveConnect manager.
		JSContext* context = (JSContext*)aContext->GetNativeContext();
		JSObject* wrappedAppletObject = nsnull;
		nsIPluginInstance* pluginInstance = nsnull;
		rv = NS_GetObjectFramePluginInstance(frame, pluginInstance);
		if (nsnull != pluginInstance) {
			nsIJVMPluginInstance* javaPluginInstance = nsnull;
			if (pluginInstance->QueryInterface(nsIJVMPluginInstance::GetIID(), (void**)&javaPluginInstance) == NS_OK) {
				jobject appletObject = nsnull;
				rv = javaPluginInstance->GetJavaObject(&appletObject);
				if (NS_OK == rv) {
					nsILiveConnectManager* manager = NULL;
					rv = nsServiceManager::GetService(nsIJVMManager::GetCID(),
					                                  nsILiveConnectManager::GetIID(),
					                                  (nsISupports **)&manager);
					if (rv == NS_OK) {
						rv = manager->WrapJavaObject(context, appletObject, &wrappedAppletObject);
						nsServiceManager::ReleaseService(nsIJVMManager::GetCID(), manager);
					}
				}
				NS_RELEASE(javaPluginInstance);
			}
			NS_RELEASE(pluginInstance);
		}
		
		// 4. set the __proto__ field of the applet object to be the element script object.
		if (nsnull != wrappedAppletObject) {
			JS_SetPrototype(context, wrappedAppletObject, elementObject);
			mInner.SetScriptObject(wrappedAppletObject);
			mReflectedApplet = PR_TRUE;
		}
		*aScriptObject = wrappedAppletObject;
	} else {
		rv = mInner.GetScriptObject(aContext, aScriptObject);
	}
	return rv;
#else
	return mInner.GetScriptObject(aContext, aScriptObject);
#endif
}

// TODO: if this method ever gets called, it will destroy the prototype type chain.

NS_IMETHODIMP
nsHTMLAppletElement::SetScriptObject(void *aScriptObject)
{
	return mInner.SetScriptObject(aScriptObject);
}
