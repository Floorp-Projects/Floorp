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
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMImage.h"
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
#include "nsIHTMLAttributes.h"
#include "nsIJSScriptObject.h"
#include "nsIJSNativeInitializer.h"
#include "nsSize.h"
#include "nsIDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsLayoutUtils.h"
#include "nsIWebShell.h"
#include "nsIFrame.h"
#include "nsImageFrame.h"
#include "nsFrameImageLoader.h"
#include "nsLayoutAtoms.h"
#include "nsNodeInfoManager.h"
#include "nsIFrameImageLoader.h"


// XXX nav attrs: suppress

class nsHTMLImageElement : public nsGenericHTMLLeafElement,
                           public nsIDOMHTMLImageElement,
                           public nsIDOMImage,
                           public nsIJSNativeInitializer
{
public:
  nsHTMLImageElement();
  virtual ~nsHTMLImageElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLImageElement
  NS_DECL_IDOMHTMLIMAGEELEMENT

  // nsIDOMImage
  NS_DECL_IDOMIMAGE
  
  // nsIJSScriptObject
  virtual PRBool GetProperty(JSContext *aContext, JSObject *aObj, 
                             jsval aID, jsval *aVp);
  virtual PRBool SetProperty(JSContext *aContext, JSObject *aObj, 
                             jsval aID, jsval *aVp);
  virtual PRBool Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                         PRBool *aDidDefineProperty);

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(JSContext* aContext, JSObject *aObj, 
                        PRUint32 argc, jsval *argv);

  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
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
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  nsresult SetSrcInner(nsIURI* aBaseURL, const nsAReadableString& aSrc);
  static nsresult GetCallerSourceURL(JSContext* cx, nsIURI** sourceURL);

  nsresult GetImageFrame(nsImageFrame** aImageFrame);

  // Only used if this is a script constructed image
  nsIDocument* mOwnerDocument;

  static nsresult ImageLibCallBack(nsIPresContext* aPresContext,
                                   nsIFrameImageLoader* aLoader,
                                   nsIFrame* aFrame,
                                   void* aClosure,
                                   PRUint32 aStatus);

  nsCOMPtr<nsIFrameImageLoader> mLoader;
};

nsresult
NS_NewHTMLImageElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  /*
   * nsHTMLImageElement's will be created without a nsINodeInfo passed in
   * if someone says "var img = new Image();" in JavaScript, in a case like
   * that we request the nsINodeInfo from the anonymous nodeinfo list.
   */
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
    nsresult rv;
    rv = nsNodeInfoManager::GetAnonymousManager(*getter_AddRefs(nodeInfoManager));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nodeInfoManager->GetNodeInfo(nsHTMLAtoms::img, nsnull,
                                      kNameSpaceID_None,
                                      *getter_AddRefs(nodeInfo));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsHTMLImageElement* it = new nsHTMLImageElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(nodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLImageElement::nsHTMLImageElement()
{
  mOwnerDocument = nsnull;
}

nsHTMLImageElement::~nsHTMLImageElement()
{
  NS_IF_RELEASE(mOwnerDocument);

  if (mLoader)
    mLoader->RemoveFrame(this);
}


NS_IMPL_ADDREF_INHERITED(nsHTMLImageElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLImageElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI3(nsHTMLImageElement, nsGenericHTMLLeafElement,
                        nsIDOMHTMLImageElement, nsIDOMImage,
                        nsIJSNativeInitializer);


nsresult
nsHTMLImageElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLImageElement* it = new nsHTMLImageElement();

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


NS_IMPL_STRING_ATTR(nsHTMLImageElement, LowSrc, lowsrc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Border, border)
NS_IMPL_INT_ATTR(nsHTMLImageElement, Border, border)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Hspace, hspace)
NS_IMPL_INT_ATTR(nsHTMLImageElement, Hspace, hspace)
NS_IMPL_BOOL_ATTR(nsHTMLImageElement, IsMap, ismap)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Lowsrc, lowsrc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, UseMap, usemap)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Vspace, vspace)
NS_IMPL_INT_ATTR(nsHTMLImageElement, Vspace, vspace)

nsresult
nsHTMLImageElement::GetImageFrame(nsImageFrame** aImageFrame)
{
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
    
    if (type.get() == nsLayoutAtoms::imageFrame) {
      // XXX We could have created an interface for this, but Troy
      // preferred the ugliness of a static cast to the weight of
      // a new interface.
      nsImageFrame* imageFrame = NS_STATIC_CAST(nsImageFrame*, frame);
      *aImageFrame = imageFrame;
      
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsHTMLImageElement::GetComplete(PRBool* aComplete)
{
  NS_ENSURE_ARG_POINTER(aComplete);
  *aComplete = PR_FALSE;

  nsresult result;
  nsImageFrame* imageFrame;

  result = GetImageFrame(&imageFrame);
  if (NS_SUCCEEDED(result) && imageFrame) {
    result = imageFrame->IsImageComplete(aComplete);
  } else {
    result = NS_OK;

    *aComplete = !mLoader;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::GetHeight(nsAWritableString& aValue)
{
  nsresult rv = nsGenericHTMLLeafElement::GetAttribute(kNameSpaceID_None,
                                                       nsHTMLAtoms::height,
                                                       aValue);

  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    PRInt32 height = 0;

    aValue.Truncate(); 

    // A zero height most likely means that the image is not loaded yet.
    if (NS_SUCCEEDED(GetHeight(&height)) && height) {
      nsAutoString heightStr;
      heightStr.AppendInt(height);
      aValue.Append(heightStr);
      aValue.Append(NS_LITERAL_STRING("px"));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::SetHeight(const nsAReadableString& aValue)
{
  return nsGenericHTMLLeafElement::SetAttribute(kNameSpaceID_None,
                                                nsHTMLAtoms::height,
                                                aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLImageElement::GetHeight(PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  nsImageFrame* imageFrame;

  nsresult rv = GetImageFrame(&imageFrame);

  if (NS_SUCCEEDED(rv) && imageFrame) {
    nsSize size;
    imageFrame->GetSize(size);

    nsCOMPtr<nsIPresContext> context;
    rv = GetPresContext(this, getter_AddRefs(context));

    if (NS_SUCCEEDED(rv) && context) {
      float t2p;
      context->GetTwipsToPixels(&t2p);

      *aHeight = NSTwipsToIntPixels(size.height, t2p);
    }
  } else {
    nsHTMLValue value;
    rv = GetHTMLAttribute(nsHTMLAtoms::height, value);

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      *aHeight = value.GetPixelValue();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::SetHeight(PRInt32 aHeight)
{
  nsAutoString val;

  val.AppendInt(aHeight);

  return nsGenericHTMLLeafElement::SetAttribute(kNameSpaceID_None,
                                                nsHTMLAtoms::height,
                                                val, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLImageElement::GetWidth(nsAWritableString& aValue)
{
  nsresult rv = nsGenericHTMLLeafElement::GetAttribute(kNameSpaceID_None,
                                                       nsHTMLAtoms::width,
                                                       aValue);

  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    PRInt32 width = 0;

    aValue.Truncate(); 

    // A zero width most likely means that the image is not loaded yet.
    if (NS_SUCCEEDED(GetWidth(&width)) && width) {
      nsAutoString widthStr;
      widthStr.AppendInt(width);
      aValue.Append(widthStr);
      aValue.Append(NS_LITERAL_STRING("px"));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::SetWidth(const nsAReadableString& aValue)
{
  return nsGenericHTMLLeafElement::SetAttribute(kNameSpaceID_None,
                                                nsHTMLAtoms::width, aValue,
                                                PR_TRUE);
}

NS_IMETHODIMP
nsHTMLImageElement::GetWidth(PRInt32* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;

  nsImageFrame* imageFrame;

  nsresult rv = GetImageFrame(&imageFrame);

  if (NS_SUCCEEDED(rv) && imageFrame) {
    nsSize size;
    imageFrame->GetSize(size);

    nsCOMPtr<nsIPresContext> context;
    rv = GetPresContext(this, getter_AddRefs(context));

    if (NS_SUCCEEDED(rv) && context) {
      float t2p;
      context->GetTwipsToPixels(&t2p);

      *aWidth = NSTwipsToIntPixels(size.width, t2p);
    }
  } else {
    nsHTMLValue value;
    rv = GetHTMLAttribute(nsHTMLAtoms::width, value);

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      *aWidth = value.GetPixelValue();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::SetWidth(PRInt32 aWidth)
{
  nsAutoString val;

  val.AppendInt(aWidth);

  return nsGenericHTMLLeafElement::SetAttribute(kNameSpaceID_None,
                                                nsHTMLAtoms::width,
                                                val, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLImageElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsAReadableString& aValue,
                                      nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::ismap) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (ParseImageAttribute(aAttribute, aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLImageElement::AttributeToString(nsIAtom* aAttribute,
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

  nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext,
                                               aPresContext);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aContext,
                                                    aPresContext, nsnull);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLImageElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::usemap) ||
      (aAttribute == nsHTMLAtoms::ismap)  ||
      (aAttribute == nsHTMLAtoms::align)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (!GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (!GetImageBorderAttributeImpact(aAttribute, aHint)) {
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
nsHTMLImageElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  // If we are a map and get a mouse click, don't let it be handled by
  // the Generic Element as this could cause a click event to fire
  // twice, once by the image frame for the map and once by the Anchor
  // element. (bug 39723)
  if (NS_MOUSE_LEFT_CLICK == aEvent->message) {
    PRBool isMap = PR_FALSE;
    GetIsMap(&isMap);
    if (isMap) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
    }
  }

  return nsGenericHTMLLeafElement::HandleDOMEvent(aPresContext, aEvent,
                                                  aDOMEvent, aFlags,
                                                  aEventStatus);
}

PRBool    
nsHTMLImageElement::GetProperty(JSContext *aContext, JSObject *aObj,
                                jsval aID, jsval *aVp)
{
  // XXX Security manager needs to be called
  if (JSVAL_IS_STRING(aID)) {
    PRUnichar* ustr =
      NS_REINTERPRET_CAST(PRUnichar *,
                          JS_GetStringChars(JS_ValueToString(aContext, aID)));

    if (NS_LITERAL_STRING("src").Equals(ustr)) {
      nsAutoString src;
      if (NS_SUCCEEDED(GetSrc(src))) {
        const PRUnichar* bytes = src.GetUnicode();
        JSString* str = JS_NewUCStringCopyZ(aContext, (const jschar*)bytes);
        if (str) {
          *aVp = STRING_TO_JSVAL(str);
          return PR_TRUE;
        }
        else {
          return PR_FALSE;
        }
      }
      else {
        return PR_FALSE;
      }
    }
  }

  return nsGenericHTMLLeafElement::GetProperty(aContext, aObj, aID, aVp);
}

nsresult
nsHTMLImageElement::GetCallerSourceURL(JSContext* cx,
                                       nsIURI** sourceURL)
{
  // XXX Code duplicated from nsHTMLDocument
  // XXX Question, why does this return NS_OK on failure?
  nsresult result = NS_OK;

  // We need to use the dynamically scoped global and assume that the 
  // current JSContext is a DOM context with a nsIScriptGlobalObject so
  // that we can get the url of the caller.
  // XXX This will fail on non-DOM contexts :(

  nsCOMPtr<nsIScriptGlobalObject> global;
  nsLayoutUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(global));
  if (global) {
    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(global));

    if (window) {
      nsCOMPtr<nsIDOMDocument> domDoc;

      result = window->GetDocument(getter_AddRefs(domDoc));
      if (NS_SUCCEEDED(result)) {
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));

        if (doc) {
          result = doc->GetBaseURL(*sourceURL);
          if (!*sourceURL) {
            *sourceURL = doc->GetDocumentURL();
          }
        }
      }
    }
  }

  return result;
}

PRBool    
nsHTMLImageElement::SetProperty(JSContext *aContext, JSObject *aObj,
                                jsval aID, jsval *aVp)
{
  nsresult result = NS_OK;

  // XXX Security manager needs to be called
  if (JSVAL_IS_STRING(aID)) {
    PRUnichar* ustr =
      NS_REINTERPRET_CAST(PRUnichar *,
                          JS_GetStringChars(JS_ValueToString(aContext, aID)));

    if (NS_LITERAL_STRING("src").Equals(ustr)) {
      nsCOMPtr<nsIURI> base;
      nsAutoString src, url;

      // Get the parameter passed in
      JSString *jsstring;
      if ((jsstring = JS_ValueToString(aContext, *aVp))) {
        src.Assign(NS_REINTERPRET_CAST(const PRUnichar*,
                                       JS_GetStringChars(jsstring)));
        src.Trim(" \t\n\r");
      }

      // Get the source of the caller
      result = GetCallerSourceURL(aContext, getter_AddRefs(base));

      if (NS_SUCCEEDED(result)) {
        if (base) {
          result = NS_MakeAbsoluteURI(url, src, base);
        } else {
          url.Assign(src);
        }

        if (NS_SUCCEEDED(result)) {
          result = SetSrcInner(base, url);
        }
      }
    }
    else {
      result = nsGenericHTMLLeafElement::SetProperty(aContext, aObj, aID, aVp);
    }
  }
  else {
    result = nsGenericHTMLLeafElement::SetProperty(aContext, aObj, aID, aVp);
  }

  if (NS_FAILED(result)) {
    return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool    
nsHTMLImageElement::Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                            PRBool *aDidDefineProperty)
{
  if (JSVAL_IS_STRING(aID) && mDOMSlots) {
    JSString *str;

    str = JSVAL_TO_STRING(aID);

    const jschar *chars = ::JS_GetStringChars(str);
    const PRUnichar *unichars = NS_REINTERPRET_CAST(const PRUnichar*, chars);

    if (!nsCRT::strcmp(unichars, NS_LITERAL_STRING("src").get())) {
      // Someone is trying to resolve "src" so we deifine it on the
      // object with a JSVAL_VOID value, the real value will be returned
      // when the caller calls GetProperty().
      ::JS_DefineUCProperty(aContext,
                            (JSObject *)mDOMSlots->mScriptObject,
                            chars, ::JS_GetStringLength(str),
                            JSVAL_VOID, nsnull, nsnull, 0);

      *aDidDefineProperty = PR_TRUE;

      return PR_TRUE;
    }
  }

  return nsGenericHTMLLeafElement::Resolve(aContext, aObj, aID,
                                           aDidDefineProperty);
}

NS_IMETHODIMP    
nsHTMLImageElement::Initialize(JSContext* aContext,
                               JSObject *aObj,
                               PRUint32 argc, 
                               jsval *argv)
{
  nsresult result = NS_OK;

  // XXX This element is created unattached to any document.  Later
  // on, it might be used to preload the image cache.  For that, we
  // need a document (actually a pres context).  The only way to get
  // one is to associate the image with one at creation time.
  // This is safer than it used to be since we get the global object
  // from the static scope chain rather than through the JSCOntext.

  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  nsLayoutUtils::GetStaticScriptGlobal(aContext, aObj,
                                       getter_AddRefs(globalObject));;
  if (globalObject) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(globalObject));

    if (domWindow) {
      nsCOMPtr<nsIDOMDocument> domDocument;
      result = domWindow->GetDocument(getter_AddRefs(domDocument));
      if (NS_SUCCEEDED(result)) {
        // Maintain the reference
        result = domDocument->QueryInterface(NS_GET_IID(nsIDocument), 
                                             (void**)&mOwnerDocument);
      }
    }
  }

  if (NS_SUCCEEDED(result) && (argc > 0)) {
    // The first (optional) argument is the width of the image
    int32 width;
    JSBool ret = JS_ValueToInt32(aContext, argv[0], &width);

    if (ret) {
      nsHTMLValue widthVal((PRInt32)width, eHTMLUnit_Integer);

      result = SetHTMLAttribute(nsHTMLAtoms::width, widthVal, PR_FALSE);
      
      if (NS_SUCCEEDED(result) && (argc > 1)) {
        // The second (optional) argument is the height of the image
        int32 height;
        ret = JS_ValueToInt32(aContext, argv[1], &height);

        if (ret) {
          nsHTMLValue heightVal((PRInt32)height, eHTMLUnit_Integer);
          
          result = SetHTMLAttribute(nsHTMLAtoms::height, heightVal, PR_FALSE);
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
                                PRBool aDeep, PRBool aCompileEventHandlers)
{
  // If we've been added to the document, we can get rid of 
  // our owner document reference so as to avoid a circular
  // reference.
  NS_IF_RELEASE(mOwnerDocument);
  return nsGenericHTMLLeafElement::SetDocument(aDocument, aDeep,
                                               aCompileEventHandlers);
}

NS_IMETHODIMP
nsHTMLImageElement::GetSrc(nsAWritableString& aSrc)
{
  // Resolve url to an absolute url
  nsresult rv = NS_OK;
  nsAutoString relURLSpec;
  nsCOMPtr<nsIURI> baseURL;

  // Get base URL.
  GetBaseURL(*getter_AddRefs(baseURL));

  // Get href= attribute (relative URL).
  nsGenericHTMLLeafElement::GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src,
                                         relURLSpec);
  relURLSpec.Trim(" \t\n\r");

  if (baseURL && relURLSpec.Length() > 0) {
    // Get absolute URL.
    rv = NS_MakeAbsoluteURI(aSrc, relURLSpec, baseURL);
  }
  else {
    // Absolute URL is same as relative URL.
    aSrc = relURLSpec;
  }

  return rv;
}

nsresult nsHTMLImageElement::ImageLibCallBack(nsIPresContext* aPresContext,
                                              nsIFrameImageLoader* aLoader,
                                              nsIFrame* aFrame,
                                              void* aClosure,
                                              PRUint32 aStatus)
{
  nsHTMLImageElement *img = (nsHTMLImageElement *)aClosure;

  if (!img || !img->mLoader)
    return NS_OK;

  if ((aStatus & NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE) &&
      !(aStatus & NS_IMAGE_LOAD_STATUS_ERROR)) {
    nsCOMPtr<nsIPresContext> cx;
    aLoader->GetPresContext(getter_AddRefs(cx));

    float t2p;
    cx->GetTwipsToPixels(&t2p);

    nsSize size;
    aLoader->GetSize(size);

    nsAutoString tmpStr;
    tmpStr.AppendInt(NSTwipsToIntPixels(size.width, t2p));
    NS_STATIC_CAST(nsIContent *, img)->SetAttribute(kNameSpaceID_None,
                                                    nsHTMLAtoms::width,
                                                    tmpStr, PR_FALSE);

    tmpStr.Truncate();
    tmpStr.AppendInt(NSTwipsToIntPixels(size.height, t2p));
    NS_STATIC_CAST(nsIContent *, img)->SetAttribute(kNameSpaceID_None,
                                                    nsHTMLAtoms::height,
                                                    tmpStr, PR_FALSE);
  }

  if (aStatus & (NS_IMAGE_LOAD_STATUS_IMAGE_READY |
                 NS_IMAGE_LOAD_STATUS_ERROR)) {
    // We set mLoader = nsnull to indicate that we're complete.
    img->mLoader->RemoveFrame(img);
    img->mLoader = nsnull;

    // Fire the onload event.
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;

    if (aStatus & NS_IMAGE_LOAD_STATUS_IMAGE_READY) {
      event.message = NS_IMAGE_LOAD;
    } else {
      event.message = NS_IMAGE_ERROR;
    }

    img->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, 
                        &status);
  }

  return NS_OK;
}


nsresult
nsHTMLImageElement::SetSrcInner(nsIURI* aBaseURL,
                                const nsAReadableString& aSrc)
{
  nsresult result = NS_OK;

  result = nsGenericHTMLLeafElement::SetAttribute(kNameSpaceID_HTML,
                                                  nsHTMLAtoms::src, aSrc,
                                                  PR_TRUE);

  if (NS_SUCCEEDED(result) && mOwnerDocument) {
    nsCOMPtr<nsIPresShell> shell;

    shell = dont_AddRef(mOwnerDocument->GetShellAt(0));
    if (shell) {
      nsCOMPtr<nsIPresContext> context;

      result = shell->GetPresContext(getter_AddRefs(context));
      if (context) {
        nsSize size;
        nsHTMLValue val;
        float p2t;

        context->GetScaledPixelsToTwips(&p2t);
        result = GetHTMLAttribute(nsHTMLAtoms::width, val);

        if (NS_CONTENT_ATTR_HAS_VALUE == result) {
          size.width = NSIntPixelsToTwips(val.GetIntValue(), p2t);
        }
        else {
          size.width = 0;
        }

        result = GetHTMLAttribute(nsHTMLAtoms::height, val);

        if (NS_CONTENT_ATTR_HAS_VALUE == result) {
          size.height = NSIntPixelsToTwips(val.GetIntValue(), p2t);
        }
        else {
          size.height = 0;
        }

        nsAutoString url;
        if (aBaseURL) {
          result = NS_MakeAbsoluteURI(url, aSrc, aBaseURL);
          if (NS_FAILED(result)) {
            url.Assign(aSrc);
          }
        }
        else {
          url.Assign(aSrc);
        }

        nsSize* specifiedSize = nsnull;
        if ((size.width > 0) || (size.height > 0)) {
          specifiedSize = &size;
        }

        // If we have a loader we're in the middle of loading a image,
        // we'll cancel that load and start a new one.
        if (mLoader) {
          mLoader->RemoveFrame(this);
        }

        // Start the image loading.
        result = context->StartLoadImage(url, nsnull, specifiedSize,
                                         nsnull, ImageLibCallBack, this,
                                         this, getter_AddRefs(mLoader));
      }
    }

    // Only do this the first time since it's only there for
    // backwards compatability
    NS_RELEASE(mOwnerDocument);
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLImageElement::SetSrc(const nsAReadableString& aSrc)
{
  nsCOMPtr<nsIURI> baseURL;
  nsresult result = NS_OK;

  if (mOwnerDocument) {
    result = mOwnerDocument->GetBaseURL(*getter_AddRefs(baseURL));
  }
  
  if (NS_SUCCEEDED(result)) {
    result = SetSrcInner(baseURL, aSrc);
  }

  return result;
}

NS_IMETHODIMP
nsHTMLImageElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::GetNaturalHeight(PRInt32* aNaturalHeight)
{
  NS_ENSURE_ARG_POINTER(aNaturalHeight);
  
  *aNaturalHeight = 0;
  
  nsImageFrame* imageFrame;
  nsresult rv = GetImageFrame(&imageFrame);

  if (NS_FAILED(rv) || !imageFrame)
    return NS_OK; // don't throw JS exceptions in this case

  PRUint32 width, height;

  rv = imageFrame->GetNaturalImageSize(&width, &height);

  if (NS_FAILED(rv))
    return NS_OK;

  *aNaturalHeight = (PRInt32)height;

  return NS_OK;  
}

NS_IMETHODIMP
nsHTMLImageElement::GetNaturalWidth(PRInt32* aNaturalWidth)
{
  NS_ENSURE_ARG_POINTER(aNaturalWidth);
  
  *aNaturalWidth = 0;
    
  nsImageFrame* imageFrame;
  nsresult rv = GetImageFrame(&imageFrame);

  if (NS_FAILED(rv) || !imageFrame)
    return NS_OK; // don't throw JS exceptions in this case

  PRUint32 width, height;

  rv = imageFrame->GetNaturalImageSize(&width, &height);

  if (NS_FAILED(rv))
    return NS_OK;

  *aNaturalWidth = (PRInt32)width;

  return NS_OK;  
}


