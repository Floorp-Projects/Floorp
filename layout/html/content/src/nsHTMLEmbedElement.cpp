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
 */
#include "nsIDOMHTMLElement.h"
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
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsObjectFrame.h"
#include "nsLayoutAtoms.h"
#include "nsObjectFrame.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIPluginInstance.h"
#include "nsIScriptablePlugin.h"
#include "nsIXPConnect.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLEmbedElement.h"

class nsHTMLEmbedElement : public nsGenericHTMLLeafElement,
                           public nsIDOMHTMLEmbedElement
{
public:
  nsHTMLEmbedElement();
  virtual ~nsHTMLEmbedElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLEmbedElement
  NS_DECL_IDOMHTMLEMBEDELEMENT

  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext,
                             void** aScriptObject);
  virtual PRBool GetProperty(JSContext *aContext, JSObject *aObj, 
                             jsval aID, jsval *aVp);
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  nsresult GetPluginInstance(nsIPluginInstance** aPluginInstance);
};

nsresult
NS_NewHTMLEmbedElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLEmbedElement* it = new nsHTMLEmbedElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsHTMLEmbedElement::nsHTMLEmbedElement()
{
}

nsHTMLEmbedElement::~nsHTMLEmbedElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLEmbedElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLEmbedElement, nsGenericElement);

NS_IMPL_HTMLCONTENT_QI(nsHTMLEmbedElement, nsGenericHTMLLeafElement,
                       nsIDOMHTMLEmbedElement);


nsresult
nsHTMLEmbedElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLEmbedElement* it = new nsHTMLEmbedElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEmbedElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsAReadableString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (ParseImageAttribute(aAttribute, aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLEmbedElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      AlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (ImageAttributeToString(aAttribute, aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return nsGenericHTMLLeafElement::AttributeToString(aAttribute, aValue,
                                                     aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aContext,
                                                   aPresContext);
  nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext,
                                               aPresContext);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aContext,
                                                    aPresContext, nsnull);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLEmbedElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (!GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (!GetImageAlignAttributeImpact(aAttribute, aHint)) {
        if (!GetImageBorderAttributeImpact(aAttribute, aHint)) {
          aHint = NS_STYLE_HINT_CONTENT;
        }
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEmbedElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEmbedElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}


/***************************************************************************/

// This is cribbed from nsHTMLImageElement::GetImageFrame.

nsresult
nsHTMLEmbedElement::GetPluginInstance(nsIPluginInstance** aPluginInstance)
{
  NS_ENSURE_ARG_POINTER(aPluginInstance);
  *aPluginInstance = nsnull;

  nsresult result;
  nsCOMPtr<nsIPresContext> context;
  nsCOMPtr<nsIPresShell> shell;
  
  if (mDocument) {
    // Make sure the presentation is up-to-date
    result = mDocument->FlushPendingNotifications();
    if (NS_FAILED(result)) {
      return result;
    }
  }
  
  result = GetPresContext(this, getter_AddRefs(context));
  if (NS_FAILED(result)) {
    return result;
  }
  
  result = context->GetShell(getter_AddRefs(shell));
  if (NS_FAILED(result)) {
    return result;
  }
  
  nsIFrame* frame;
  result = shell->GetPrimaryFrameFor(this, &frame);
  if (NS_FAILED(result)) {
    return result;
  }
  
  if (frame) {
    nsCOMPtr<nsIAtom> type;
    
    frame->GetFrameType(getter_AddRefs(type));
    
    if (type.get() == nsLayoutAtoms::objectFrame) {
      // XXX We could have created an interface for this, but Troy
      // preferred the ugliness of a static cast to the weight of
      // a new interface.
      
      nsObjectFrame* objectFrame = NS_STATIC_CAST(nsObjectFrame*, frame);
      
      return objectFrame->GetPluginInstance(*aPluginInstance);
    }

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

/***************************************************************************/

/*
 * For plugins, we want to expose both attributes of the plugin tag
 * and any scriptable methods that the plugin itself exposes.  To do
 * this, we get the plugin object itself (the XPCOM object) and wrap
 * it as a scriptable object via xpconnect.  We then set the original
 * node element, which exposes the DOM node methods, as the javascript
 * prototype object of that object.  Then we get both sets of methods, and
 * plugin methods can potentially override DOM methods.
 */
NS_IMETHODIMP
nsHTMLEmbedElement::GetScriptObject(nsIScriptContext* aContext,
                                    void** aScriptObject)
{
  if (mDOMSlots && mDOMSlots->mScriptObject)
    return nsGenericHTMLLeafElement::GetScriptObject(aContext, aScriptObject);

  nsresult rv;
  *aScriptObject = nsnull;

  // Get the JS object corresponding to this dom node.  This will become
  // the javascript prototype object of the object we eventually reflect to the
  // DOM.
  JSObject* elementObject = nsnull;
  rv = nsGenericHTMLLeafElement::GetScriptObject(aContext,
                                                 (void**)&elementObject);
  if (NS_FAILED(rv) || !elementObject)
    return rv;

  // Flush pending reflows to ensure the plugin is instansiated, assuming
  // it's visible
  if (mDocument) {
    mDocument->FlushPendingNotifications();
  }

  nsCOMPtr<nsIPluginInstance> pi;
  rv = GetPluginInstance(getter_AddRefs(pi));

  // If GetPluginInstance() fails it means there's no frame for this element
  // yet, in that case we return the script object for the element but we
  // don't cache it so that the next call can get the correct script object
  // if the plugin instance is available at the next call.
  if (NS_FAILED(rv)) {
    if (mDocument) {
      // Since we're resetting the script object to null we'll remove the
      // reference to it so that we won't add the same named reference
      // again the next time someone requests the script object.
      aContext->RemoveReference((void *)&mDOMSlots->mScriptObject,
                                mDOMSlots->mScriptObject);
    }

    SetScriptObject(nsnull);

    *aScriptObject = elementObject;

    return NS_OK;
  }

  // Check if the plugin object has the nsIScriptablePlugin
  // interface, describing how to expose it to JavaScript.  Given this
  // interface, use it to get the scriptable peer object (possibly the
  // plugin object itself) and the scriptable interface to expose it
  // with
  nsIID scriptableInterface;
  nsCOMPtr<nsISupports> scriptablePeer;
  if (NS_SUCCEEDED(rv) && pi) {
    nsCOMPtr<nsIScriptablePlugin> spi(do_QueryInterface(pi, &rv));
    if (NS_SUCCEEDED(rv) && spi) {
      nsIID *scriptableInterfacePtr = nsnull;
      rv = spi->GetScriptableInterface(&scriptableInterfacePtr);

      if (NS_SUCCEEDED(rv) && scriptableInterfacePtr) {
        rv = spi->GetScriptablePeer(getter_AddRefs(scriptablePeer));

        scriptableInterface = *scriptableInterfacePtr;

        nsMemory::Free(scriptableInterfacePtr);
      }
    }
  }

  if (NS_FAILED(rv) || !scriptablePeer) {
    // Fall back to returning the element object.
    *aScriptObject = elementObject;

    return NS_OK;
  }

  // Wrap it.
  JSObject* interfaceObject; // XPConnect-wrapped peer object, when we get it.
  JSContext *cx = (JSContext *)aContext->GetNativeContext();
  nsCOMPtr<nsIXPConnect> xpc =
    do_GetService(nsIXPConnect::GetCID()); 
  if (cx && xpc) {
    JSObject* parentObject = JS_GetParent(cx, elementObject);
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    if (NS_SUCCEEDED(xpc->WrapNative(cx, parentObject,
                                     scriptablePeer, scriptableInterface,
                                     getter_AddRefs(holder))) && holder && 
        NS_SUCCEEDED(holder->GetJSObject(&interfaceObject)) && interfaceObject) {
      *aScriptObject = interfaceObject;
    }
  }

  // If we got an xpconnect-wrapped plugin object, set its' prototype to the
  // element object.
  if (!*aScriptObject || !JS_SetPrototype(cx, interfaceObject,
                                          elementObject)) {
    *aScriptObject = elementObject; // fall back
    return NS_OK;
  }

  // Cache it.
  SetScriptObject(*aScriptObject);

  return NS_OK;
}

// Allow access to arbitrary XPCOM interfaces supported by the plugin
// via a pluginObject.nsISomeInterface notation.
PRBool    
nsHTMLEmbedElement::GetProperty(JSContext *aContext, JSObject *aObj,
                                jsval aID, jsval *aVp)
{
  if (JSVAL_IS_STRING(aID)) {
    PRBool retval = PR_FALSE;
    char* cString = JS_GetStringBytes(JS_ValueToString(aContext, aID));

    nsCOMPtr<nsIInterfaceInfoManager> iim = 
        dont_AddRef(XPTI_GetInterfaceInfoManager());
    nsCOMPtr<nsIXPConnect> xpc =
        do_GetService(nsIXPConnect::GetCID()); 

    if (iim && xpc) {
      nsIID* iid;
      if (NS_SUCCEEDED(iim->GetIIDForName(cString, &iid)) && iid) {
        nsCOMPtr<nsIPluginInstance> pi;
        if (NS_SUCCEEDED(GetPluginInstance(getter_AddRefs(pi))) && pi) {
          nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
          JSObject* ifaceObj;

          if (NS_SUCCEEDED(xpc->WrapNative(aContext, aObj, pi, *iid, 
                                           getter_AddRefs(holder))) && holder && 
              NS_SUCCEEDED(holder->GetJSObject(&ifaceObj)) && ifaceObj) {
              *aVp = OBJECT_TO_JSVAL(ifaceObj);
              retval = PR_TRUE;
          }
        }
        nsMemory::Free(iid);        
        return retval;
      }
    }
  }

  return nsGenericHTMLLeafElement::GetProperty(aContext, aObj, aID, aVp);
}

/////////////////////////////////////////////
// Implement nsIDOMHTMLEmbedElement interface
NS_IMPL_STRING_ATTR(nsHTMLEmbedElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLEmbedElement, Height, height)
NS_IMPL_STRING_ATTR(nsHTMLEmbedElement, Width, width)
NS_IMPL_STRING_ATTR(nsHTMLEmbedElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLEmbedElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLEmbedElement, Src, src)

