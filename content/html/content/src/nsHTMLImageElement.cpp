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
#include "nsIDOMHTMLImageElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIHTMLAttributes.h"
#include "nsIJSScriptObject.h"
#include "nsIJSNativeInitializer.h"
#include "nsSize.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNeckoUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO

// XXX nav attrs: suppress

static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);
static NS_DEFINE_IID(kIJSNativeInitializerIID, NS_IJSNATIVEINITIALIZER_IID);
static NS_DEFINE_IID(kIDOMWindowIID, NS_IDOMWINDOW_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);

class nsHTMLImageElement : public nsIDOMHTMLImageElement,
                           public nsIScriptObjectOwner,
                           public nsIDOMEventReceiver,
                           public nsIHTMLContent,
                           public nsIJSScriptObject,
                           public nsIJSNativeInitializer
{
public:
  nsHTMLImageElement(nsIAtom* aTag);
  virtual ~nsHTMLImageElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLImageElement
  NS_IMETHOD GetLowSrc(nsString& aLowSrc);
  NS_IMETHOD SetLowSrc(const nsString& aLowSrc);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetAlt(nsString& aAlt);
  NS_IMETHOD SetAlt(const nsString& aAlt);
  NS_IMETHOD GetBorder(nsString& aBorder);
  NS_IMETHOD SetBorder(const nsString& aBorder);
  NS_IMETHOD GetHeight(nsString& aHeight);
  NS_IMETHOD SetHeight(const nsString& aHeight);
  NS_IMETHOD GetHspace(nsString& aHspace);
  NS_IMETHOD SetHspace(const nsString& aHspace);
  NS_IMETHOD GetIsMap(PRBool* aIsMap);
  NS_IMETHOD SetIsMap(PRBool aIsMap);
  NS_IMETHOD GetLongDesc(nsString& aLongDesc);
  NS_IMETHOD SetLongDesc(const nsString& aLongDesc);
  NS_IMETHOD GetSrc(nsString& aSrc);
  NS_IMETHOD SetSrc(const nsString& aSrc);
  NS_IMETHOD GetUseMap(nsString& aUseMap);
  NS_IMETHOD SetUseMap(const nsString& aUseMap);
  NS_IMETHOD GetVspace(nsString& aVspace);
  NS_IMETHOD SetVspace(const nsString& aVspace);
  NS_IMETHOD GetWidth(nsString& aWidth);
  NS_IMETHOD SetWidth(const nsString& aWidth);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_NO_SETDOCUMENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIJSScriptObject
  virtual PRBool    AddProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    GetProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    SetProperty(JSContext *aContext, jsval aID, jsval *aVp);
  virtual PRBool    EnumerateProperty(JSContext *aContext);
  virtual PRBool    Resolve(JSContext *aContext, jsval aID);
  virtual PRBool    Convert(JSContext *aContext, jsval aID);
  virtual void      Finalize(JSContext *aContext);

  // nsIJSNativeInitializer
  NS_IMETHOD        Initialize(JSContext* aContext, PRUint32 argc, jsval *argv);

protected:
  nsGenericHTMLLeafElement mInner;
  nsIDocument* mOwnerDocument;  // Only used if this is a script constructed image
};

nsresult
NS_NewHTMLImageElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLImageElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLImageElement::nsHTMLImageElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mOwnerDocument = nsnull;
}

nsHTMLImageElement::~nsHTMLImageElement()
{
  NS_IF_RELEASE(mOwnerDocument);
}

NS_IMPL_ADDREF(nsHTMLImageElement)

NS_IMPL_RELEASE(nsHTMLImageElement)

nsresult
nsHTMLImageElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  // Note that this has to stay above the generic element
  // QI macro, since it overrides the nsIJSScriptObject implementation
  // from the generic element.
  if (aIID.Equals(kIJSScriptObjectIID)) {
    nsIJSScriptObject* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }                                                             
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLImageElementIID)) {
    nsIDOMHTMLImageElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  if (aIID.Equals(kIJSNativeInitializerIID)) {
    nsIJSNativeInitializer* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLImageElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLImageElement* it = new nsHTMLImageElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLImageElement, LowSrc, lowsrc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Border, border)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Height, height)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Hspace, hspace)
NS_IMPL_BOOL_ATTR(nsHTMLImageElement, IsMap, ismap)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, UseMap, usemap)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Width, width)

NS_IMETHODIMP
nsHTMLImageElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsString& aValue,
                                      nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::ismap) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (nsGenericHTMLElement::ParseImageAttribute(aAttribute,
                                                     aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLImageElement::AttributeToString(nsIAtom* aAttribute,
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
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      PRUint8 align = value.GetIntValue();
      nsStyleDisplay* display = (nsStyleDisplay*)
        aContext->GetMutableStyleData(eStyleStruct_Display);
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      switch (align) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        display->mFloats = NS_STYLE_FLOAT_LEFT;
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        display->mFloats = NS_STYLE_FLOAT_RIGHT;
        break;
      default:
        text->mVerticalAlign.SetIntValue(align, eStyleUnit_Enumerated);
        break;
      }
    }
  }
  nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aContext, aPresContext, nsnull);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLImageElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::usemap) ||
      (aAttribute == nsHTMLAtoms::ismap)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (! nsGenericHTMLElement::GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (! nsGenericHTMLElement::GetImageBorderAttributeImpact(aAttribute, aHint)) {
        aHint = NS_STYLE_HINT_CONTENT;
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLImageElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLImageElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

PRBool    
nsHTMLImageElement::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return mInner.AddProperty(aContext, aID, aVp);
}

PRBool    
nsHTMLImageElement::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return mInner.DeleteProperty(aContext, aID, aVp);
}

PRBool    
nsHTMLImageElement::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return mInner.GetProperty(aContext, aID, aVp);
}

PRBool    
nsHTMLImageElement::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return mInner.SetProperty(aContext, aID, aVp);
}

PRBool    
nsHTMLImageElement::EnumerateProperty(JSContext *aContext)
{
  return mInner.EnumerateProperty(aContext);
}

PRBool    
nsHTMLImageElement::Resolve(JSContext *aContext, jsval aID)
{
  return mInner.Resolve(aContext, aID);
}

PRBool    
nsHTMLImageElement::Convert(JSContext *aContext, jsval aID)
{
  return mInner.Convert(aContext, aID);
}

void      
nsHTMLImageElement::Finalize(JSContext *aContext)
{
  mInner.Finalize(aContext);
}


NS_IMETHODIMP    
nsHTMLImageElement::Initialize(JSContext* aContext, 
                               PRUint32 argc, 
                               jsval *argv)
{
  nsresult result = NS_OK;

  // XXX This element is created unattached to any document.  Later
  // on, it might be used to preload the image cache.  For that, we
  // need a document (actually a pres context).  The only way to get
  // one is to associate the image with one at creation time, through
  // the JSContext passed in.  This mechanism is **UGLY** and makes
  // some hard assumptions about the relationship between JSContexts
  // and documents.

  nsIScriptContext* scriptContext;
  scriptContext = (nsIScriptContext*)JS_GetContextPrivate(aContext);
  if (nsnull != scriptContext) {
    nsIScriptGlobalObject* globalObject = scriptContext->GetGlobalObject();
    if (nsnull != globalObject) {
      nsIDOMWindow* domWindow;
      result = globalObject->QueryInterface(kIDOMWindowIID, (void**)&domWindow);
      if (NS_SUCCEEDED(result)) {
        nsIDOMDocument* domDocument;
        result = domWindow->GetDocument(&domDocument);
        if (NS_SUCCEEDED(result)) {
          // Maintain the reference
          result = domDocument->QueryInterface(kIDocumentIID, 
                                               (void**)&mOwnerDocument);
          NS_RELEASE(domDocument);
        }
        NS_RELEASE(domWindow);
      }
    }
  }

  if (NS_SUCCEEDED(result) && (argc > 0)) {
    // The first (optional) argument is the width of the image
    int32 width;
    JSBool ret = JS_ValueToInt32(aContext, argv[0], &width);
    if (ret) {
      nsHTMLValue widthVal((PRInt32)width, eHTMLUnit_Integer);

      result = mInner.SetHTMLAttribute(nsHTMLAtoms::width,
                                       widthVal, PR_FALSE);
      
      if (NS_SUCCEEDED(result) && (argc > 1)) {
        // The second (optional) argument is the height of the image
        int32 height;
        ret = JS_ValueToInt32(aContext, argv[1], &height);
        if (ret) {
          nsHTMLValue heightVal((PRInt32)height, eHTMLUnit_Integer);
          
          result = mInner.SetHTMLAttribute(nsHTMLAtoms::height,
                                           heightVal, PR_FALSE);
        }
        else {
          result = NS_ERROR_INVALID_ARG;
        }
      }
    }
    else {
      result = NS_ERROR_INVALID_ARG;
    }
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLImageElement::SetDocument(nsIDocument* aDocument, 
                                PRBool aDeep)
{
  // If we've been added to the document, we can get rid of 
  // our owner document reference so as to avoid a circular
  // reference.
  NS_IF_RELEASE(mOwnerDocument);
  return mInner.SetDocument(aDocument, aDeep);
}

NS_IMETHODIMP
nsHTMLImageElement::GetSrc(nsString& aSrc)
{
  mInner.GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, aSrc);
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLImageElement::SetSrc(const nsString& aSrc)
{
  nsresult result = NS_OK;

  if (nsnull != mOwnerDocument) {
    
    PRInt32 i, count = mOwnerDocument->GetNumberOfShells();
    nsIPresShell* shell;

    for (i = 0; i < count; i++) {
      shell = mOwnerDocument->GetShellAt(i);
      if (nsnull != shell) {
        nsIPresContext* context;
        
        result = shell->GetPresContext(&context);
        if (NS_SUCCEEDED(result)) {
          nsSize size;
          nsHTMLValue val;
          float p2t;
          
          context->GetScaledPixelsToTwips(&p2t);
          result = mInner.GetHTMLAttribute(nsHTMLAtoms::width, val);
          if (NS_CONTENT_ATTR_HAS_VALUE == result) {
            size.width = NSIntPixelsToTwips(val.GetIntValue(), p2t);
          }
          else {
            size.width = 0;
          }
          result = mInner.GetHTMLAttribute(nsHTMLAtoms::height, val);
          if (NS_CONTENT_ATTR_HAS_VALUE == result) {
            size.height = NSIntPixelsToTwips(val.GetIntValue(), p2t);
          }
          else {
            size.height = 0;
          }

          nsAutoString url, empty;
          nsIURI* baseURL;

          empty.Truncate();
          result = mOwnerDocument->GetBaseURL(baseURL);
          if (NS_SUCCEEDED(result)) {
#ifndef NECKO
            result = NS_MakeAbsoluteURL(baseURL, empty, aSrc, url);
#else
            result = NS_MakeAbsoluteURI(aSrc, baseURL, url);
#endif // NECKO
            if (NS_FAILED(result)) {
              url = aSrc;
            }
            NS_RELEASE(baseURL);
          }
          else {
            url = aSrc;
          }

          nsSize* specifiedSize = nsnull;
          if ((size.width > 0) || (size.height > 0)) {
            specifiedSize = &size;
          }

          // Start the image loading. We don't care about notification
          // or holding on to the image loader.
          result = context->StartLoadImage(url, nsnull, specifiedSize,
                                           nsnull, nsnull, nsnull,
                                           nsnull);

          NS_RELEASE(context);
        }
        
        NS_RELEASE(shell);
      }
    }

    // Only do this the first time since it's only there for
    // backwards compatability
    NS_RELEASE(mOwnerDocument);
  }

  if (NS_SUCCEEDED(result)) {
    result = mInner.SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, aSrc, PR_TRUE);
  }

  return result;
}
