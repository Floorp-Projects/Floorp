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


class nsHTMLAppletElement : public nsGenericHTMLContainerElement,
                            public nsIDOMHTMLAppletElement
{
public:
  nsHTMLAppletElement();
  virtual ~nsHTMLAppletElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLAppletElement
  NS_DECL_IDOMHTMLAPPLETELEMENT

  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext,
                             void** aScriptObject);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  PRBool mReflectedApplet;
};

nsresult
NS_NewHTMLAppletElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLAppletElement* it = new nsHTMLAppletElement();

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


nsHTMLAppletElement::nsHTMLAppletElement()
{
  mReflectedApplet = PR_FALSE;
}

nsHTMLAppletElement::~nsHTMLAppletElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLAppletElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLAppletElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI(nsHTMLAppletElement, nsGenericHTMLContainerElement,
                       nsIDOMHTMLAppletElement)


nsresult
nsHTMLAppletElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLAppletElement* it = new nsHTMLAppletElement();

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
  return nsGenericHTMLContainerElement::AttributeToString(aAttribute, aValue,
                                                          aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
  nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext,
                                               aPresContext);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aContext,
                                                   aPresContext);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aContext,
                                                    aPresContext, nsnull);
}

NS_IMETHODIMP
nsHTMLAppletElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
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
nsHTMLAppletElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                  nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}



extern nsresult
NS_GetObjectFramePluginInstance(nsIFrame* aFrame,
                                nsIPluginInstance*& aPluginInstance);

/**
 * For backwards compatibility an applet element's JavaScript object
 * should expose both the public fields of the applet, and the
 * attributes of the applet tag. The call to
 * nsGenericElement::GetScriptObject takes case of the tag
 * attributes. Here we generate a JavaScript reference to the applet
 * object itself, and set its __proto__ property to the tag
 * object. That way, if the Java applet has public fields that shadow
 * the tag attributes, the applet's fields take precedence.
 */
NS_IMETHODIMP
nsHTMLAppletElement::GetScriptObject(nsIScriptContext* aContext,
                                     void** aScriptObject)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIJVMManager> jvm(do_GetService(nsIJVMManager::GetCID(), &rv));

  if (NS_SUCCEEDED(rv)) {
    if (!mReflectedApplet) {
      // 0. Make sure the presentation is up-to-date
      if (mDocument) {
        mDocument->FlushPendingNotifications();
      }

      // 1. get the script object corresponding to the <APPLET> element itself.
      JSObject* elementObject = nsnull;
      rv = nsGenericHTMLContainerElement::GetScriptObject(aContext,
                                                          (void**)&elementObject);
      if (NS_FAILED(rv))
        return rv;

      // 2. get the plugin instance corresponding to this element.
      nsCOMPtr<nsIPresShell> shell;
      if (mDocument)
        shell = dont_AddRef(mDocument->GetShellAt(0));

      nsIFrame* frame = nsnull;

      if (shell) {
        shell->GetPrimaryFrameFor(this, &frame);
      }

      if (frame) {
        nsCOMPtr<nsIAtom> frameType;
        frame->GetFrameType(getter_AddRefs(frameType));
        if(nsLayoutAtoms::objectFrame != frameType.get()) {
          *aScriptObject = elementObject;
          return rv;
        }
      }

      // 3. get the Java object corresponding to this applet, and
      // reflect it into JavaScript using the LiveConnect manager.
      JSContext* context = (JSContext*)aContext->GetNativeContext();
      JSObject* wrappedAppletObject = nsnull;
      nsCOMPtr<nsIPluginInstance> pluginInstance;

      NS_GetObjectFramePluginInstance(frame, *getter_AddRefs(pluginInstance));

      if (pluginInstance) {
        nsCOMPtr<nsIJVMPluginInstance> javaPluginInstance;

        javaPluginInstance = do_QueryInterface(pluginInstance);

        if (javaPluginInstance) {
          jobject appletObject = nsnull;
          rv = javaPluginInstance->GetJavaObject(&appletObject);

          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsILiveConnectManager> manager;

            manager = do_GetService(nsIJVMManager::GetCID());

            if (manager) {
              rv = manager->WrapJavaObject(context, appletObject,
                                           &wrappedAppletObject);
            }
          }
        }
      }

      // 4. set the __proto__ field of the applet object to be the
      // element script object.
      if (wrappedAppletObject) {
        JS_SetPrototype(context, wrappedAppletObject, elementObject);

        // Cache the wrapped applet object as our script object.
        SetScriptObject(wrappedAppletObject);

        mReflectedApplet = PR_TRUE;

        *aScriptObject = wrappedAppletObject;
      } else {
        // We didn't wrap the applet object so we'll fall back and use
        // the plain DOM script object.

        *aScriptObject = elementObject;
      }
    }
    else {
      rv = nsGenericHTMLContainerElement::GetScriptObject(aContext,
                                                          aScriptObject);
    }
  }
  else {
    rv = nsGenericHTMLContainerElement::GetScriptObject(aContext,
                                                        aScriptObject);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLAppletElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
