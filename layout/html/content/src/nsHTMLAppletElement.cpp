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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsIDOMHTMLAppletElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"

#include "nsIServiceManager.h"
#include "nsIJVMManager.h"
#include "nsILiveConnectManager.h"
#include "nsIPluginInstance.h"
#include "nsIJVMPluginInstance.h"
#include "nsLayoutAtoms.h"

// XXX this is to get around conflicts with windows.h defines
// introduced through jni.h
#ifdef XP_PC
#undef GetClassName
#undef GetObject
#endif


class nsHTMLAppletElement : public nsIDOMHTMLAppletElement,
                            public nsIJSScriptObject,
                            public nsIHTMLContent
{
public:
  nsHTMLAppletElement(nsINodeInfo *aNodeInfo);
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
  NS_DECL_IDOMHTMLAPPLETELEMENT

  // nsIJSScriptObject
  virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj,
                        jsval aID, jsval *aVp) {
    return mInner.AddProperty(aContext, aObj, aID, aVp);
  }
  virtual PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj,
                        jsval aID, jsval *aVp) {
    return mInner.DeleteProperty(aContext, aObj, aID, aVp);
  }
  virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj,
                        jsval aID, jsval *aVp) {
    return mInner.GetProperty(aContext, aObj, aID, aVp);
  }
  virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj,
                        jsval aID, jsval *aVp) {
    return mInner.SetProperty(aContext, aObj, aID, aVp);
  }
  virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj) {
    return mInner.EnumerateProperty(aContext, aObj);
  }
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID) {
    return mInner.EnumerateProperty(aContext, aObj);
  }
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID) {
    return mInner.EnumerateProperty(aContext, aObj);
  }
  virtual void      Finalize(JSContext *aContext, JSObject *aObj) {
    mInner.Finalize(aContext, aObj);
  }

  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext,
                             void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
  PRBool mReflectedApplet;
};

nsresult
NS_NewHTMLAppletElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLAppletElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLAppletElement::nsHTMLAppletElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
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
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLAppletElement))) {
    nsIDOMHTMLAppletElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLAppletElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLAppletElement* it = new nsHTMLAppletElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
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
                                       const nsAReadableString& aValue,
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
                                       nsAWritableString& aResult) const
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
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aContext, aPresContext, nsnull);
}

NS_IMETHODIMP
nsHTMLAppletElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint) const
{
  if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (! nsGenericHTMLElement::GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (! nsGenericHTMLElement::GetImageAlignAttributeImpact(aAttribute, aHint)) {
        if (! nsGenericHTMLElement::GetImageBorderAttributeImpact(aAttribute, aHint)) {
          aHint = NS_STYLE_HINT_CONTENT;
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAppletElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                  nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}



NS_IMETHODIMP
nsHTMLAppletElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

extern nsresult NS_GetObjectFramePluginInstance(nsIFrame* aFrame, nsIPluginInstance*& aPluginInstance);

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
 nsresult rv = NS_OK;
NS_WITH_SERVICE(nsIJVMManager, jvm, nsIJVMManager::GetCID(), &rv);
if (NS_SUCCEEDED(rv)) {
	// nsresult rv = NS_OK;
	if (!mReflectedApplet) {
    // 0. Make sure the presentation is up-to-date
    if (mInner.mDocument) {
      mInner.mDocument->FlushPendingNotifications();
    }

		// 1. get the script object corresponding to the <APPLET> element itself.
		JSObject* elementObject = nsnull;
		rv = mInner.GetScriptObject(aContext, (void**)&elementObject);
		if (NS_OK != rv)
			return rv;
	
		// 2. get the plugin instance corresponding to this element.
		nsIPresShell* shell = nsnull;
    if (mInner.mDocument)
      shell = mInner.mDocument->GetShellAt(0);

    nsIFrame* frame = nsnull;

    if (shell) {
      shell->GetPrimaryFrameFor(mInner.mContent, &frame);
  		NS_RELEASE(shell);
    }

    if(frame != nsnull)
    {
      nsIAtom* frameType = nsnull;
      frame->GetFrameType(&frameType);
      if(nsLayoutAtoms::objectFrame != frameType)
      {
        *aScriptObject = elementObject;
        return rv;
      }
    }

    // 3. get the Java object corresponding to this applet, and reflect it into
		// JavaScript using the LiveConnect manager.
		JSContext* context = (JSContext*)aContext->GetNativeContext();
		JSObject* wrappedAppletObject = nsnull;
		nsIPluginInstance* pluginInstance = nsnull;
		rv = NS_GetObjectFramePluginInstance(frame, pluginInstance);
		if ((rv == NS_OK) && (nsnull != pluginInstance)) {
			nsIJVMPluginInstance* javaPluginInstance = nsnull;
			if (pluginInstance->QueryInterface(NS_GET_IID(nsIJVMPluginInstance), (void**)&javaPluginInstance) == NS_OK) {
				jobject appletObject = nsnull;
				rv = javaPluginInstance->GetJavaObject(&appletObject);
				if (NS_OK == rv) {
					nsILiveConnectManager* manager = NULL;
					rv = nsServiceManager::GetService(nsIJVMManager::GetCID(),
					                                  NS_GET_IID(nsILiveConnectManager),
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
}
else    
	return mInner.GetScriptObject(aContext, aScriptObject);
}



// TODO: if this method ever gets called, it will destroy the prototype type chain.

NS_IMETHODIMP
nsHTMLAppletElement::SetScriptObject(void *aScriptObject)
{
	return mInner.SetScriptObject(aScriptObject);
}

NS_IMETHODIMP
nsHTMLAppletElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}
