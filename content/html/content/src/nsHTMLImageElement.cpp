/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMNSHTMLImageElement.h"
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
#include "nsContentUtils.h"
#include "nsIWebShell.h"
#include "nsIFrame.h"
#include "nsIImageFrame.h"
#include "nsLayoutAtoms.h"
#include "nsNodeInfoManager.h"
#include "nsGUIEvent.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"

#include "imgIContainer.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "imgIDecoderObserver.h"

#include "nsILoadGroup.h"

#include "nsIRuleNode.h"

#include "nsIJSContextStack.h"

// XXX nav attrs: suppress

class nsHTMLImageElement : public nsGenericHTMLLeafElement,
                           public nsIDOMHTMLImageElement,
                           public nsIDOMNSHTMLImageElement,
                           public nsIJSNativeInitializer,
                           public imgIDecoderObserver
{
public:
  nsHTMLImageElement();
  virtual ~nsHTMLImageElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLLeafElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLLeafElement::)

  // nsIDOMHTMLImageElement
  NS_DECL_NSIDOMHTMLIMAGEELEMENT

  // nsIDOMNSHTMLImageElement
  NS_DECL_NSIDOMNSHTMLIMAGEELEMENT

  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(JSContext* aContext, JSObject *aObj,
                        PRUint32 argc, jsval *argv);

  // nsIContent
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  nsresult SetSrcInner(nsIURI* aBaseURL, const nsAReadableString& aSrc);
  static nsresult GetCallerSourceURL(nsIURI** sourceURL);

  nsresult GetImageFrame(nsIImageFrame** aImageFrame);

  // Only used if this is a script constructed image
  nsIDocument* mOwnerDocument;


  nsCOMPtr<imgIRequest> mRequest;
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

#ifndef USE_IMG2
  if (mLoader)
    mLoader->RemoveFrame(this);
#endif
}


NS_IMPL_ADDREF_INHERITED(nsHTMLImageElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLImageElement, nsGenericElement)


// QueryInterface implementation for nsHTMLImageElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLImageElement,
                                    nsGenericHTMLLeafElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLImageElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLImageElement)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLImageElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


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
NS_IMPL_INT_ATTR(nsHTMLImageElement, Border, border)
NS_IMPL_INT_ATTR(nsHTMLImageElement, Hspace, hspace)
NS_IMPL_BOOL_ATTR(nsHTMLImageElement, IsMap, ismap)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Lowsrc, lowsrc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, UseMap, usemap)
NS_IMPL_INT_ATTR(nsHTMLImageElement, Vspace, vspace)

nsresult
nsHTMLImageElement::GetImageFrame(nsIImageFrame** aImageFrame)
{
  NS_ENSURE_ARG_POINTER(aImageFrame);
  *aImageFrame = nsnull;

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
      nsIImageFrame* imageFrame;
      result = frame->QueryInterface(NS_GET_IID(nsIImageFrame),
                                     (void**)&imageFrame);
      if (NS_FAILED(result)) {
        NS_WARNING("Should not happen - frame is not image frame even though "
                   "type is nsLayoutAtoms::imageFrame");
        return result;
      }
      *aImageFrame = imageFrame;

      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::GetComplete(PRBool* aComplete)
{
  NS_ENSURE_ARG_POINTER(aComplete);
  *aComplete = PR_FALSE;

  nsresult result;
  nsIImageFrame* imageFrame;

  GetImageFrame(&imageFrame);
  if (imageFrame) {
    result = imageFrame->IsImageComplete(aComplete);
  } else {
    result = NS_OK;

    *aComplete = !mRequest;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::GetX(PRInt32* aX)
{
  return GetOffsetLeft(aX);
}

NS_IMETHODIMP
nsHTMLImageElement::GetY(PRInt32* aY)
{
  return GetOffsetTop(aY);
}

NS_IMETHODIMP
nsHTMLImageElement::GetHeight(PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  nsIImageFrame* imageFrame = nsnull;

  nsresult rv = GetImageFrame(&imageFrame);

  nsIFrame* frame = nsnull;
  if (imageFrame) {
    imageFrame->QueryInterface(NS_GET_IID(nsIFrame), (void**)&frame);
    NS_WARN_IF_FALSE(frame,"Should not happen - image frame is not frame");
  }

  if (frame) {
    nsSize size;
    frame->GetSize(size);

    nsMargin margin;
    frame->CalcBorderPadding(margin);

    size.height -= margin.top + margin.bottom;

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

  return nsGenericHTMLLeafElement::SetAttr(kNameSpaceID_None,
                                           nsHTMLAtoms::height,
                                           val, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLImageElement::GetWidth(PRInt32* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;

  nsIImageFrame* imageFrame;

  nsresult rv = GetImageFrame(&imageFrame);

  nsIFrame* frame = nsnull;
  if (imageFrame) {
    imageFrame->QueryInterface(NS_GET_IID(nsIFrame), (void**)&frame);
    NS_WARN_IF_FALSE(frame,"Should not happen - image frame is not frame");
  }

  if (NS_SUCCEEDED(rv) && frame) {
    nsSize size;
    frame->GetSize(size);

    nsMargin margin;
    frame->CalcBorderPadding(margin);

    size.width -= margin.left + margin.right;

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

  return nsGenericHTMLLeafElement::SetAttr(kNameSpaceID_None,
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
  else if (aAttribute == nsHTMLAtoms::src) {
    aResult.SetStringValue(aValue);
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
MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (!aData || !aAttributes)
    return;
  nsGenericHTMLElement::MapAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImagePositionAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}


NS_IMETHODIMP
nsHTMLImageElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
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
nsHTMLImageElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
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

nsresult
nsHTMLImageElement::GetCallerSourceURL(nsIURI** sourceURL)
{
  // XXX Code duplicated from nsHTMLDocument
  // XXX Question, why does this return NS_OK on failure?
  nsresult result = NS_OK;

  // We need to use the dynamically scoped global and assume that the
  // current JSContext is a DOM context with a nsIScriptGlobalObject so
  // that we can get the url of the caller.
  // XXX This will fail on non-DOM contexts :(

  // Get JSContext from stack.




  // XXX: This service should be cached.
  nsCOMPtr<nsIJSContextStack>
    stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", &result));

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  JSContext *cx = nsnull;

  // it's possible that there is not a JSContext on the stack.
  // specifically this can happen when the DOM is being manipulated
  // from native (non-JS) code.
  if (NS_FAILED(stack->Peek(&cx)) || !cx)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptGlobalObject> global;
  nsContentUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(global));
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
            doc->GetDocumentURL(sourceURL);
          }
        }
      }
    }
  }

  return result;
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
  nsContentUtils::GetStaticScriptGlobal(aContext, aObj,
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
  nsGenericHTMLLeafElement::GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::src,
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


NS_IMETHODIMP
nsHTMLImageElement::OnStartDecode(imgIRequest *request, nsISupports *cx)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::OnStartContainer(imgIRequest *request, nsISupports *cx,
                                     imgIContainer *image)
{
  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(cx));

  NS_ASSERTION(pc, "not a prescontext!");

  float t2p;
  pc->GetTwipsToPixels(&t2p);

  nsSize size;
  image->GetWidth(&size.width);
  image->GetHeight(&size.height);

  nsAutoString tmpStr;
  tmpStr.AppendInt(size.width);
  NS_STATIC_CAST(nsIContent *, this)->SetAttr(kNameSpaceID_None,
                                              nsHTMLAtoms::width,
                                              tmpStr, PR_FALSE);

  tmpStr.Truncate();
  tmpStr.AppendInt(size.height);
  NS_STATIC_CAST(nsIContent *, this)->SetAttr(kNameSpaceID_None,
                                              nsHTMLAtoms::height,
                                              tmpStr, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::OnStartFrame(imgIRequest *request, nsISupports *cx,
                                 gfxIImageFrame *frame)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::OnDataAvailable(imgIRequest *request, nsISupports *cx,
                                    gfxIImageFrame *frame, const nsRect * rect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::OnStopFrame(imgIRequest *request, nsISupports *cx,
                                gfxIImageFrame *frame)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::OnStopContainer(imgIRequest *request, nsISupports *cx,
                                    imgIContainer *image)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::OnStopDecode(imgIRequest *request, nsISupports *cx,
                                 nsresult status, const PRUnichar *statusArg)
{
  nsCOMPtr<nsIPresContext> pc(do_QueryInterface(cx));

  NS_ASSERTION(pc, "not a prescontext!");

  // We set mRequest = nsnull to indicate that we're complete.
  mRequest = nsnull;

  // Fire the onload event.
  nsEventStatus estatus = nsEventStatus_eIgnore;
  nsEvent event;
  event.eventStructType = NS_EVENT;

  if (NS_SUCCEEDED(status))
    event.message = NS_IMAGE_LOAD;
  else
    event.message = NS_IMAGE_ERROR;

  this->HandleDOMEvent(pc, &event, nsnull, NS_EVENT_FLAG_INIT,
                      &estatus);

  return NS_OK;
}

NS_IMETHODIMP nsHTMLImageElement::FrameChanged(imgIContainer *container, nsISupports *cx, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  return NS_OK;
}

nsresult
nsHTMLImageElement::SetSrcInner(nsIURI* aBaseURL,
                                const nsAReadableString& aSrc)
{
  nsresult result = NS_OK;

  result = nsGenericHTMLLeafElement::SetAttr(kNameSpaceID_HTML,
                                             nsHTMLAtoms::src, aSrc,
                                             PR_TRUE);

  if (NS_SUCCEEDED(result) && mOwnerDocument) {
    nsCOMPtr<nsIPresShell> shell;

    mOwnerDocument->GetShellAt(0, getter_AddRefs(shell));
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

        nsCOMPtr<nsIURI> uri;
        result = NS_NewURI(getter_AddRefs(uri), aSrc, aBaseURL);
        if (NS_FAILED(result)) return result;

        nsCOMPtr<nsIDocument> document;
        result = shell->GetDocument(getter_AddRefs(document));
        if (NS_FAILED(result)) return result;

        nsCOMPtr<nsIScriptGlobalObject> globalObject;
        result = document->GetScriptGlobalObject(getter_AddRefs(globalObject));
        if (NS_FAILED(result)) return result;

        nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(globalObject));

        PRBool shouldLoad = PR_TRUE;
        result = NS_CheckContentLoadPolicy(nsIContentPolicy::IMAGE,
                                       uri,
                                       NS_STATIC_CAST(nsISupports *,
                                                      (nsIDOMHTMLImageElement*)this),
                                       domWin, &shouldLoad);
        if (NS_SUCCEEDED(result) && !shouldLoad)
            return NS_OK;

        // If we have a loader we're in the middle of loading a image,
        // we'll cancel that load and start a new one.
        if (mRequest) {
          // mRequest->Cancel() ?? cancel the load?
        }

        nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
        if (!il) {
          return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsISupports> sup(do_QueryInterface(context));

        nsCOMPtr<nsIDocument> doc;
        nsCOMPtr<nsILoadGroup> loadGroup;
        shell->GetDocument(getter_AddRefs(doc));
        if (doc) {
          doc->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
        }

        il->LoadImage(uri, loadGroup, this, sup, nsIRequest::LOAD_NORMAL, nsnull, getter_AddRefs(mRequest));
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

  (void) GetCallerSourceURL(getter_AddRefs(baseURL));

  if (mDocument && !baseURL) {
    result = mDocument->GetBaseURL(*getter_AddRefs(baseURL));
  }

  if (mOwnerDocument && !baseURL) {
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

  nsIImageFrame* imageFrame = nsnull;
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

  nsIImageFrame* imageFrame;
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


