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
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLAttributes.h"
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
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include "imgIContainer.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "imgIDecoderObserver.h"

#include "nsILoadGroup.h"

#include "nsRuleNode.h"

#include "nsIJSContextStack.h"
#include "nsIView.h"

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
  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32 aModType,
                                      nsChangeHint& aHint) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  nsresult SetSrcInner(nsIURI* aBaseURL, const nsAString& aSrc);

  void GetImageFrame(nsIImageFrame** aImageFrame);
  nsresult GetXY(PRInt32* aX, PRInt32* aY);
  nsresult GetWidthHeight(PRInt32* aWidth, PRInt32* aHeight);

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
   * that we request the nsINodeInfo from the document's nodeinfo list.
   */
  nsresult rv;
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsIDOMDocument> dom_doc;
    nsContentUtils::GetDocumentFromCaller(getter_AddRefs(dom_doc));

    nsCOMPtr<nsIDocument> doc(do_QueryInterface(dom_doc));
    NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
    doc->GetNodeInfoManager(*getter_AddRefs(nodeInfoManager));
    NS_ENSURE_TRUE(nodeInfoManager, NS_ERROR_UNEXPECTED);

    rv = nodeInfoManager->GetNodeInfo(nsHTMLAtoms::img, nsnull,
                                      kNameSpaceID_None,
                                      *getter_AddRefs(nodeInfo));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsHTMLImageElement* it = new nsHTMLImageElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = it->Init(nodeInfo);

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
}

nsHTMLImageElement::~nsHTMLImageElement()
{
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


NS_IMPL_STRING_ATTR(nsHTMLImageElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Border, border)
NS_IMPL_PIXEL_ATTR(nsHTMLImageElement, Hspace, hspace)
NS_IMPL_BOOL_ATTR(nsHTMLImageElement, IsMap, ismap)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, UseMap, usemap)
NS_IMPL_PIXEL_ATTR(nsHTMLImageElement, Vspace, vspace)

void
nsHTMLImageElement::GetImageFrame(nsIImageFrame** aImageFrame)
{
  *aImageFrame = nsnull;
  // If we have no parent, then we won't have a frame yet
  if (!mParent)
    return;

  nsIFrame* frame = GetPrimaryFrame(PR_TRUE);

  if (frame) {
    CallQueryInterface(frame, aImageFrame);
  }
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

nsresult
nsHTMLImageElement::GetXY(PRInt32* aX, PRInt32* aY)
{
  if (aX) {
    *aX = 0;
  }

  if (aY) {
    *aY = 0;
  }

  if (!mDocument) {
    return NS_OK;
  }

  // Get Presentation shell 0
  nsCOMPtr<nsIPresShell> presShell;
  mDocument->GetShellAt(0, getter_AddRefs(presShell));

  if (!presShell) {
    return NS_OK;
  }

  // Get the Presentation Context from the Shell
  nsCOMPtr<nsIPresContext> context;
  presShell->GetPresContext(getter_AddRefs(context));

  if (!context) {
    return NS_OK;
  }

  // Flush all pending notifications so that our frames are uptodate
  mDocument->FlushPendingNotifications();

  // Get the Frame for this image
  nsIFrame* frame = nsnull;
  presShell->GetPrimaryFrameFor(this, &frame);

  if (!frame) {
    return NS_OK;
  }

  nsPoint origin(0, 0);
  nsIView* parentView;
  nsresult rv = frame->GetOffsetFromView(context, origin, &parentView);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the scale from that Presentation Context
  float scale;
  context->GetTwipsToPixels(&scale);

  // Convert to pixels using that scale
  if (aX) {
    *aX = NSTwipsToIntPixels(origin.x, scale);
  }

  if (aY) {
    *aY = NSTwipsToIntPixels(origin.y, scale);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::GetX(PRInt32* aX)
{
  return GetXY(aX, nsnull);
}

NS_IMETHODIMP
nsHTMLImageElement::GetY(PRInt32* aY)
{
  return GetXY(nsnull, aY);
}

nsresult
nsHTMLImageElement::GetWidthHeight(PRInt32* aWidth, PRInt32* aHeight)
{
  if (aHeight) {
    *aHeight = 0;
  }

  if (aWidth) {
    *aWidth = 0;
  }

  nsIImageFrame* imageFrame;
  GetImageFrame(&imageFrame);

  nsIFrame* frame = nsnull;

  if (imageFrame) {
    CallQueryInterface(imageFrame, &frame);
    NS_WARN_IF_FALSE(frame,"Should not happen - image frame is not frame");
  }

  if (frame) {
    nsSize size;
    frame->GetSize(size);

    nsMargin margin;
    frame->CalcBorderPadding(margin);

    size.height -= margin.top + margin.bottom;
    size.width -= margin.left + margin.right;

    nsCOMPtr<nsIPresContext> context;
    GetPresContext(this, getter_AddRefs(context));

    if (context) {
      float t2p;
      context->GetTwipsToPixels(&t2p);

      if (aWidth) {
        *aWidth = NSTwipsToIntPixels(size.width, t2p);
      }

      if (aHeight) {
        *aHeight = NSTwipsToIntPixels(size.height, t2p);
      }
    }
  } else {
    nsHTMLValue value;

    if (aWidth) {
      nsresult rv = GetHTMLAttribute(nsHTMLAtoms::width, value);

      if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        *aWidth = value.GetPixelValue();
      }
    }

    if (aHeight) {
      nsresult rv = GetHTMLAttribute(nsHTMLAtoms::height, value);

      if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        *aHeight = value.GetPixelValue();
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::GetHeight(PRInt32* aHeight)
{
  return GetWidthHeight(nsnull, aHeight);
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
  return GetWidthHeight(aWidth, nsnull);
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
                                      const nsAString& aValue,
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
    static const char* kWhitespace = " \n\r\t\b";
    aResult.SetStringValue(nsContentUtils::TrimCharsInSet(kWhitespace, aValue));
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
                                      nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      VAlignValueToString(aValue, aResult);
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
nsHTMLImageElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32 aModType,
                                             nsChangeHint& aHint) const
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

NS_IMETHODIMP
nsHTMLImageElement::Initialize(JSContext* aContext, JSObject *aObj,
                               PRUint32 argc, jsval *argv)
{
  if (argc <= 0) {
    // Nothing to do here if we don't get any arguments.

    return NS_OK;
  }

  // The first (optional) argument is the width of the image
  int32 width;
  JSBool ret = JS_ValueToInt32(aContext, argv[0], &width);
  NS_ENSURE_TRUE(ret, NS_ERROR_INVALID_ARG);

  nsHTMLValue widthVal((PRInt32)width, eHTMLUnit_Pixel);

  nsresult rv = SetHTMLAttribute(nsHTMLAtoms::width, widthVal, PR_FALSE);

  if (NS_SUCCEEDED(rv) && (argc > 1)) {
    // The second (optional) argument is the height of the image
    int32 height;
    ret = JS_ValueToInt32(aContext, argv[1], &height);
    NS_ENSURE_TRUE(ret, NS_ERROR_INVALID_ARG);

    nsHTMLValue heightVal((PRInt32)height, eHTMLUnit_Pixel);

    rv = SetHTMLAttribute(nsHTMLAtoms::height, heightVal, PR_FALSE);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLImageElement::GetSrc(nsAString& aSrc)
{
  // Resolve url to an absolute url
  nsresult rv = NS_OK;
  nsAutoString relURLSpec;
  nsCOMPtr<nsIURI> baseURL;

  // Get base URL.
  GetBaseURL(*getter_AddRefs(baseURL));

  // Get href= attribute (relative URL).
  nsGenericHTMLLeafElement::GetAttr(kNameSpaceID_None, nsHTMLAtoms::src,
                                    relURLSpec);
  relURLSpec.Trim(" \t\n\r");

  if (baseURL && !relURLSpec.IsEmpty()) {
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
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::width, tmpStr, PR_FALSE);

  tmpStr.Truncate();
  tmpStr.AppendInt(size.height);
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::height, tmpStr, PR_FALSE);

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

  HandleDOMEvent(pc, &event, nsnull, NS_EVENT_FLAG_INIT, &estatus);

  return NS_OK;
}

NS_IMETHODIMP nsHTMLImageElement::FrameChanged(imgIContainer *container, nsISupports *cx, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  return NS_OK;
}

nsresult
nsHTMLImageElement::SetSrcInner(nsIURI* aBaseURL,
                                const nsAString& aSrc)
{
  nsresult result = SetAttr(kNameSpaceID_None, nsHTMLAtoms::src, aSrc,
                            PR_TRUE);
  NS_ENSURE_SUCCESS(result, result);

  if (!mDocument) {
    nsCOMPtr<nsIDocument> doc;
    mNodeInfo->GetDocument(*getter_AddRefs(doc));

    nsCOMPtr<nsIPresShell> shell;

    doc->GetShellAt(0, getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIPresContext> context;

      result = shell->GetPresContext(getter_AddRefs(context));
      if (context) {
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

        nsCOMPtr<nsIURI> uri;
        result = NS_NewURI(getter_AddRefs(uri), aSrc, nsnull, aBaseURL);
        if (NS_FAILED(result)) return result;

        nsCOMPtr<nsIDocument> document;
        result = shell->GetDocument(getter_AddRefs(document));
        if (NS_FAILED(result)) return result;

        nsCOMPtr<nsIScriptGlobalObject> globalObject;
        result = document->GetScriptGlobalObject(getter_AddRefs(globalObject));
        if (NS_FAILED(result)) return result;

        nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(globalObject));

        PRBool shouldLoad = PR_TRUE;
        result =
          NS_CheckContentLoadPolicy(nsIContentPolicy::IMAGE,
                                    uri,
                                    NS_STATIC_CAST(nsIDOMHTMLImageElement *,
                                                   this),
                                    domWin, &shouldLoad);
        if (NS_SUCCEEDED(result) && !shouldLoad) {
          return NS_OK;
        }

        // If we have a loader we're in the middle of loading a image,
        // we'll cancel that load and start a new one.

        // if (mRequest) {
        //   mRequest->Cancel() ?? cancel the load?
        // }

        nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
        if (!il) {
          return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsIDocument> doc;
        nsCOMPtr<nsILoadGroup> loadGroup;
        nsCOMPtr<nsIURI> documentURI;
        shell->GetDocument(getter_AddRefs(doc));
        if (doc) {
          doc->GetDocumentLoadGroup(getter_AddRefs(loadGroup));

          // Get the documment URI for the referrer.
          doc->GetDocumentURL(getter_AddRefs(documentURI));
        }

        // XXX: initialDocumentURI is NULL!
        il->LoadImage(uri, nsnull, documentURI, loadGroup, this, context, nsIRequest::LOAD_NORMAL,
                      nsnull, nsnull, getter_AddRefs(mRequest));
      }
    }
  } else {
    nsIImageFrame* imageFrame;
    GetImageFrame(&imageFrame);
    if (!imageFrame) {
      // If we don't have an image frame, reconstruct the frame
      // so that the new image can load.
      nsCOMPtr<nsIPresShell> shell;
      mDocument->GetShellAt(0, getter_AddRefs(shell));
      if (shell) {
        shell->RecreateFramesFor(this);
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsHTMLImageElement::SetSrc(const nsAString& aSrc)
{
  /*
   * If caller is not chrome and dom.disable_image_src_set is true,
   * prevent setting image.src by exiting early
   */

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    PRBool disableImageSrcSet = PR_FALSE;
    prefBranch->GetBoolPref("dom.disable_image_src_set", &disableImageSrcSet);

    if (disableImageSrcSet && !nsContentUtils::IsCallerChrome()) {
      return NS_OK;
    }
  }

  nsCOMPtr<nsIURI> baseURL;
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMDocument> domDoc;
  nsContentUtils::GetDocumentFromCaller(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (doc) {
    // XXX GetBaseURL should do the GetDocumentURL for us
    rv = doc->GetBaseURL(*getter_AddRefs(baseURL));
    if (!baseURL) {
      rv = doc->GetDocumentURL(getter_AddRefs(baseURL));
    }
  }

  if (!baseURL) {
    mNodeInfo->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      rv = doc->GetBaseURL(*getter_AddRefs(baseURL));
    }
  }

  if (NS_SUCCEEDED(rv)) {
    rv = SetSrcInner(baseURL, aSrc);
  }

  return rv;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLImageElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif

NS_IMETHODIMP
nsHTMLImageElement::GetNaturalHeight(PRInt32* aNaturalHeight)
{
  NS_ENSURE_ARG_POINTER(aNaturalHeight);

  *aNaturalHeight = 0;

  nsIImageFrame* imageFrame;
  GetImageFrame(&imageFrame);

  if (!imageFrame)
    return NS_OK; // don't throw JS exceptions in this case

  PRUint32 width, height;

  nsresult rv = imageFrame->GetNaturalImageSize(&width, &height);

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
  GetImageFrame(&imageFrame);

  if (!imageFrame)
    return NS_OK; // don't throw JS exceptions in this case

  PRUint32 width, height;

  nsresult rv = imageFrame->GetNaturalImageSize(&width, &height);

  if (NS_FAILED(rv))
    return NS_OK;

  *aNaturalWidth = (PRInt32)width;

  return NS_OK;
}


