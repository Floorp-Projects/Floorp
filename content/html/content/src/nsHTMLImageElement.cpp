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
#include "nsImageLoadingContent.h"
#include "nsHTMLAtoms.h"
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
                           public nsImageLoadingContent,
                           public nsIDOMHTMLImageElement,
                           public nsIDOMNSHTMLImageElement,
                           public nsIJSNativeInitializer
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
  NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                    PRInt32 aModType,
                                    nsChangeHint& aHint) const;
  NS_IMETHOD_(PRBool) HasAttributeDependentStyle(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);

  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                     const nsAString& aValue, PRBool aNotify);
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo, const nsAString& aValue,
                     PRBool aNotify);

  // XXXbz What about UnsetAttr?  We don't seem to unload images when
  // that happens...

  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);  
  NS_IMETHOD SetParent(nsIContent* aParent);  

protected:
  void GetImageFrame(nsIImageFrame** aImageFrame);
  nsresult GetXY(PRInt32* aX, PRInt32* aY);
  nsresult GetWidthHeight(PRInt32* aWidth, PRInt32* aHeight);
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

    nsINodeInfoManager *nodeInfoManager = doc->GetNodeInfoManager();
    NS_ENSURE_TRUE(nodeInfoManager, NS_ERROR_UNEXPECTED);

    rv = nodeInfoManager->GetNodeInfo(nsHTMLAtoms::img, nsnull,
                                      kNameSpaceID_None,
                                      getter_AddRefs(nodeInfo));
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
  NS_INTERFACE_MAP_ENTRY(nsIImageLoadingContent)
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
NS_IMPL_URI_ATTR(nsHTMLImageElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Lowsrc, lowsrc)
NS_IMPL_URI_ATTR_GETTER(nsHTMLImageElement, Src, src)
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
  NS_PRECONDITION(aComplete, "Null out param!");
  *aComplete = PR_TRUE;

  if (!mCurrentRequest) {
    return NS_OK;
  }

  PRUint32 status;
  mCurrentRequest->GetImageStatus(&status);
  *aComplete =
    (status &
     (imgIRequest::STATUS_LOAD_COMPLETE | imgIRequest::STATUS_ERROR)) != 0;

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
  nsIPresShell *presShell = mDocument->GetShellAt(0);
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

  if (mDocument) {
    // Flush all pending notifications so that our frames are up to date.
    // If we're not in a document, we don't have a frame anyway, so we
    // don't care.
    mDocument->FlushPendingNotifications();
  }

  nsIImageFrame* imageFrame;
  GetImageFrame(&imageFrame);

  nsIFrame* frame = nsnull;

  if (imageFrame) {
    CallQueryInterface(imageFrame, &frame);
    NS_WARN_IF_FALSE(frame,"Should not happen - image frame is not frame");
  }

  if (frame) {
    // XXX we could put an accessor on nsIImageFrame to return its
    // mComputedSize.....
    nsSize size = frame->GetSize();

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
    nsCOMPtr<imgIContainer> image;
    if (mCurrentRequest) {
      mCurrentRequest->GetImage(getter_AddRefs(image));
    }

    if (aWidth) {
      nsresult rv = GetHTMLAttribute(nsHTMLAtoms::width, value);

      if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        *aWidth = value.GetPixelValue();
      } else if (image) {
        image->GetWidth(aWidth);
      }
    }

    if (aHeight) {
      nsresult rv = GetHTMLAttribute(nsHTMLAtoms::height, value);

      if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        *aHeight = value.GetPixelValue();
      } else if (image) {
        image->GetHeight(aHeight);
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
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLImageElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                           PRInt32 aModType,
                                           nsChangeHint& aHint) const
{
  nsresult rv =
    nsGenericHTMLLeafElement::GetAttributeChangeHint(aAttribute,
                                                     aModType, aHint);
  if (aAttribute == nsHTMLAtoms::usemap ||
      aAttribute == nsHTMLAtoms::ismap) {
    NS_UpdateHint(aHint, NS_STYLE_HINT_FRAMECHANGE);
  }
  return rv;
}

NS_IMETHODIMP_(PRBool)
nsHTMLImageElement::HasAttributeDependentStyle(const nsIAtom* aAttribute) const
{
  static const AttributeDependenceEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
    sImageAlignAttributeMap
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
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
nsHTMLImageElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            const nsAString& aValue, PRBool aNotify)
{
  // If we plan to call ImageURIChanged, we want to do it first so that the
  // image load kicks off _before_ the reflow triggered by the SetAttr.  But if
  // aNotify is false, we are coming from the parser or some such place; we'll
  // get our parent set after all the attributes have been set, so we'll do the
  // image load from SetParent.  Skip the ImageURIChanged call in that case.
  if (aNotify &&
      aNameSpaceID == kNameSpaceID_None && aName == nsHTMLAtoms::src) {
    ImageURIChanged(aValue);
  }
    
  return nsGenericHTMLLeafElement::SetAttr(aNameSpaceID, aName,
                                           aValue, aNotify);
}

NS_IMETHODIMP
nsHTMLImageElement::SetAttr(nsINodeInfo* aNodeInfo, const nsAString& aValue,
                            PRBool aNotify)
{
  return nsGenericHTMLLeafElement::SetAttr(aNodeInfo, aValue, aNotify);
}

NS_IMETHODIMP
nsHTMLImageElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                PRBool aCompileEventHandlers)
{
  PRBool documentChanging = aDocument && (aDocument != mDocument);
  
  nsresult rv = nsGenericHTMLLeafElement::SetDocument(aDocument, aDeep,
                                                      aCompileEventHandlers);
  if (documentChanging && mParent) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    nsAutoString uri;
    nsresult result = GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, uri);
    if (result == NS_CONTENT_ATTR_HAS_VALUE) {
      ImageURIChanged(uri);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLImageElement::SetParent(nsIContent* aParent)
{
  nsresult rv = nsGenericHTMLLeafElement::SetParent(aParent);
  if (aParent && mDocument) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    nsAutoString uri;
    nsresult result = GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, uri);
    if (result == NS_CONTENT_ATTR_HAS_VALUE) {
      ImageURIChanged(uri);
    }
  }
  return rv;
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

  // Call ImageURIChanged first so that our image load will kick off
  // before the SetAttr triggers a reflow
  ImageURIChanged(aSrc);

  return nsGenericHTMLLeafElement::SetAttr(kNameSpaceID_None,
                                           nsHTMLAtoms::src, aSrc, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLImageElement::GetNaturalHeight(PRInt32* aNaturalHeight)
{
  NS_ENSURE_ARG_POINTER(aNaturalHeight);

  *aNaturalHeight = 0;

  if (!mCurrentRequest) {
    return NS_OK;
  }
  
  nsCOMPtr<imgIContainer> image;
  mCurrentRequest->GetImage(getter_AddRefs(image));
  if (!image) {
    return NS_OK;
  }

  image->GetHeight(aNaturalHeight);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::GetNaturalWidth(PRInt32* aNaturalWidth)
{
  NS_ENSURE_ARG_POINTER(aNaturalWidth);

  *aNaturalWidth = 0;

  if (!mCurrentRequest) {
    return NS_OK;
  }
  
  nsCOMPtr<imgIContainer> image;
  mCurrentRequest->GetImage(getter_AddRefs(image));
  if (!image) {
    return NS_OK;
  }

  image->GetWidth(aNaturalWidth);
  return NS_OK;
}


