/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsMappedAttributes.h"
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

#include "imgIContainer.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "imgIDecoderObserver.h"

#include "nsILoadGroup.h"

#include "nsRuleNode.h"

#include "nsIJSContextStack.h"
#include "nsIView.h"

// XXX nav attrs: suppress

class nsHTMLImageElement : public nsGenericHTMLElement,
                           public nsImageLoadingContent,
                           public nsIDOMHTMLImageElement,
                           public nsIDOMNSHTMLImageElement,
                           public nsIJSNativeInitializer
{
public:
  nsHTMLImageElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLImageElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLImageElement
  NS_DECL_NSIDOMHTMLIMAGEELEMENT

  // nsIDOMNSHTMLImageElement
  NS_DECL_NSIDOMNSHTMLIMAGEELEMENT

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(JSContext* aContext, JSObject *aObj,
                        PRUint32 argc, jsval *argv);

  // nsIContent
  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                    PRInt32 aModType,
                                    nsChangeHint& aHint) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);

  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);

  // XXXbz What about UnsetAttr?  We don't seem to unload images when
  // that happens...

  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);  
  virtual void SetParent(nsIContent* aParent);  

protected:
  void GetImageFrame(nsIImageFrame** aImageFrame);
  nsPoint GetXY();
  nsSize GetWidthHeight();
};

nsIHTMLContent*
NS_NewHTMLImageElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
{
  /*
   * nsHTMLImageElement's will be created without a nsINodeInfo passed in
   * if someone says "var img = new Image();" in JavaScript, in a case like
   * that we request the nsINodeInfo from the document's nodeinfo list.
   */
  nsresult rv;
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());
    NS_ENSURE_TRUE(doc, nsnull);

    rv = doc->NodeInfoManager()->GetNodeInfo(nsHTMLAtoms::img, nsnull,
                                             kNameSpaceID_None,
                                             getter_AddRefs(nodeInfo));
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  return new nsHTMLImageElement(nodeInfo);
}

nsHTMLImageElement::nsHTMLImageElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLImageElement::~nsHTMLImageElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLImageElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLImageElement, nsGenericElement)


// QueryInterface implementation for nsHTMLImageElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLImageElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLImageElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLImageElement)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIImageLoadingContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLImageElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Image)


NS_IMPL_STRING_ATTR(nsHTMLImageElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Border, border)
NS_IMPL_INT_ATTR(nsHTMLImageElement, Hspace, hspace)
NS_IMPL_BOOL_ATTR(nsHTMLImageElement, IsMap, ismap)
NS_IMPL_URI_ATTR(nsHTMLImageElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, Lowsrc, lowsrc)
NS_IMPL_URI_ATTR(nsHTMLImageElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLImageElement, UseMap, usemap)
NS_IMPL_INT_ATTR(nsHTMLImageElement, Vspace, vspace)

void
nsHTMLImageElement::GetImageFrame(nsIImageFrame** aImageFrame)
{
  *aImageFrame = nsnull;
  // If we have no parent, then we won't have a frame yet
  if (!GetParent())
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

nsPoint
nsHTMLImageElement::GetXY()
{
  nsPoint point(0, 0);

  if (!mDocument) {
    return point;
  }

  // Get Presentation shell 0
  nsIPresShell *presShell = mDocument->GetShellAt(0);
  if (!presShell) {
    return point;
  }

  // Get the Presentation Context from the Shell
  nsCOMPtr<nsIPresContext> context;
  presShell->GetPresContext(getter_AddRefs(context));

  if (!context) {
    return point;
  }

  // Flush all pending notifications so that our frames are laid out correctly
  mDocument->FlushPendingNotifications(Flush_Layout);

  // Get the Frame for this image
  nsIFrame* frame = nsnull;
  presShell->GetPrimaryFrameFor(this, &frame);

  if (!frame) {
    return point;
  }

  nsPoint origin(0, 0);
  nsIView* parentView;
  nsresult rv = frame->GetOffsetFromView(context, origin, &parentView);
  if (NS_FAILED(rv)) {
    return point;
  }

  // Get the scale from that Presentation Context
  float scale;
  scale = context->TwipsToPixels();

  // Convert to pixels using that scale
  point.x = NSTwipsToIntPixels(origin.x, scale);
  point.y = NSTwipsToIntPixels(origin.y, scale);

  return point;
}

NS_IMETHODIMP
nsHTMLImageElement::GetX(PRInt32* aX)
{
  *aX = GetXY().x;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::GetY(PRInt32* aY)
{
  *aY = GetXY().y;

  return NS_OK;
}

nsSize
nsHTMLImageElement::GetWidthHeight()
{
  nsSize size(0,0);

  if (mDocument) {
    // Flush all pending notifications so that our frames are up to date.
    // If we're not in a document, we don't have a frame anyway, so we
    // don't care.
    mDocument->FlushPendingNotifications(Flush_Layout);
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
    size = frame->GetSize();

    nsMargin margin;
    frame->CalcBorderPadding(margin);

    size.height -= margin.top + margin.bottom;
    size.width -= margin.left + margin.right;

    nsCOMPtr<nsIPresContext> context;
    GetPresContext(this, getter_AddRefs(context));

    if (context) {
      float t2p;
      t2p = context->TwipsToPixels();

      size.width = NSTwipsToIntPixels(size.width, t2p);
      size.height = NSTwipsToIntPixels(size.height, t2p);
    }
  } else {
    nsHTMLValue value;
    nsCOMPtr<imgIContainer> image;
    if (mCurrentRequest) {
      mCurrentRequest->GetImage(getter_AddRefs(image));
    }

    if (GetHTMLAttribute(nsHTMLAtoms::width,
                         value) == NS_CONTENT_ATTR_HAS_VALUE) {
      size.width = value.GetIntValue();
    } else if (image) {
      image->GetWidth(&size.width);
    }

    if (GetHTMLAttribute(nsHTMLAtoms::height,
                         value) == NS_CONTENT_ATTR_HAS_VALUE) {
      size.height = value.GetIntValue();
    } else if (image) {
      image->GetHeight(&size.height);
    }
  }

  return size;
}

NS_IMETHODIMP
nsHTMLImageElement::GetHeight(PRInt32* aHeight)
{
  *aHeight = GetWidthHeight().height;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::SetHeight(PRInt32 aHeight)
{
  nsAutoString val;
  val.AppendInt(aHeight);

  return nsGenericHTMLElement::SetAttr(kNameSpaceID_None, nsHTMLAtoms::height,
                                       val, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLImageElement::GetWidth(PRInt32* aWidth)
{
  *aWidth = GetWidthHeight().width;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLImageElement::SetWidth(PRInt32 aWidth)
{
  nsAutoString val;
  val.AppendInt(aWidth);

  return nsGenericHTMLElement::SetAttr(kNameSpaceID_None, nsHTMLAtoms::width,
                                       val, PR_TRUE);
}

PRBool
nsHTMLImageElement::ParseAttribute(nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    return ParseAlignValue(aValue, aResult);
  }
  if (aAttribute == nsHTMLAtoms::src) {
    static const char* kWhitespace = " \n\r\t\b";
    aResult.SetTo(nsContentUtils::TrimCharsInSet(kWhitespace, aValue));
    return PR_TRUE;
  }
  if (ParseImageAttribute(aAttribute, aValue, aResult)) {
    return PR_TRUE;
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
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

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
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
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType, aHint);
  if (aAttribute == nsHTMLAtoms::usemap ||
      aAttribute == nsHTMLAtoms::ismap) {
    NS_UpdateHint(aHint, NS_STYLE_HINT_FRAMECHANGE);
  }
  return rv;
}

NS_IMETHODIMP_(PRBool)
nsHTMLImageElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
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


nsresult
nsHTMLImageElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
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

  return nsGenericHTMLElement::HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                              aFlags, aEventStatus);
}

nsresult
nsHTMLImageElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            nsIAtom* aPrefix, const nsAString& aValue,
                            PRBool aNotify)
{
  // If we plan to call ImageURIChanged, we want to do it first so that the
  // image load kicks off _before_ the reflow triggered by the SetAttr.  But if
  // aNotify is false, we are coming from the parser or some such place; we'll
  // get our parent set after all the attributes have been set, so we'll do the
  // image load from SetParent.  Skip the ImageURIChanged call in that case.
  if (aNotify &&
      aNameSpaceID == kNameSpaceID_None && aName == nsHTMLAtoms::src) {

    // If caller is not chrome and dom.disable_image_src_set is true,
    // prevent setting image.src by exiting early
    if (nsContentUtils::GetBoolPref("dom.disable_image_src_set") &&
        !nsContentUtils::IsCallerChrome()) {
      return NS_OK;
    }

    nsCOMPtr<imgIRequest> oldCurrentRequest = mCurrentRequest;

    ImageURIChanged(aValue);

    if (mCurrentRequest && !mPendingRequest &&
        oldCurrentRequest != mCurrentRequest) {
      // We have a current request, and it's not the same one as we used
      // to have, and we have no pending request.  So imglib already had
      // that image.  Reset the animation on it -- see bug 210001
      nsCOMPtr<imgIContainer> container;
      mCurrentRequest->GetImage(getter_AddRefs(container));
      if (container) {
        container->ResetAnimation();
      }
    }
  }
    
  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                       aNotify);
}

void
nsHTMLImageElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                PRBool aCompileEventHandlers)
{
  PRBool documentChanging = aDocument && (aDocument != mDocument);
  
  nsGenericHTMLElement::SetDocument(aDocument, aDeep, aCompileEventHandlers);
  if (documentChanging && GetParent()) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    nsAutoString uri;
    nsresult result = GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, uri);
    if (result == NS_CONTENT_ATTR_HAS_VALUE) {
      ImageURIChanged(uri);
    }
  }
}

void
nsHTMLImageElement::SetParent(nsIContent* aParent)
{
  nsGenericHTMLElement::SetParent(aParent);
  if (aParent && mDocument) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    nsAutoString uri;
    nsresult result = GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, uri);
    if (result == NS_CONTENT_ATTR_HAS_VALUE) {
      ImageURIChanged(uri);
    }
  }
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

  nsresult rv = SetIntAttr(nsHTMLAtoms::width, NS_STATIC_CAST(PRInt32, width));

  if (NS_SUCCEEDED(rv) && (argc > 1)) {
    // The second (optional) argument is the height of the image
    int32 height;
    ret = JS_ValueToInt32(aContext, argv[1], &height);
    NS_ENSURE_TRUE(ret, NS_ERROR_INVALID_ARG);

    rv = SetIntAttr(nsHTMLAtoms::height, NS_STATIC_CAST(PRInt32, height));
  }

  return rv;
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


