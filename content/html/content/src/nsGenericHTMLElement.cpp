/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
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
#include "nscore.h"
#include "nsGenericHTMLElement.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleRule.h"
#include "nsCSSDeclaration.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIEventListenerManager.h"
#include "nsIFocusController.h"
#include "nsHTMLAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLDocument.h"
#include "nsIHTMLContent.h"
#include "nsILink.h"
#include "nsILinkHandler.h"
#include "nsPIDOMWindow.h"
#include "nsISizeOfHandler.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsStyleConsts.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollableView.h"
#include "nsRange.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"

#include "nsIPresState.h"
#include "nsILayoutHistoryState.h"
#include "nsIFrameManager.h"

#include "nsIHTMLContentContainer.h"
#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsDOMCSSDeclaration.h"
#include "nsICSSOMFactory.h"
#include "prprf.h"
#include "prmem.h"
#include "nsIFormControlFrame.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsILanguageAtomService.h"

#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"

#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"

#include "nsRuleWalker.h"

#include "nsIObjectFrame.h"
#include "nsLayoutAtoms.h"
#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIServiceManager.h"

#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIHTMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"

#include "nsHTMLUtils.h"

#include "nsIDOMText.h"
#include "nsITextContent.h"

static NS_DEFINE_CID(kPresStateCID,  NS_PRESSTATE_CID);
// XXX todo: add in missing out-of-memory checks

//----------------------------------------------------------------------

#ifdef GATHER_ELEMENT_USEAGE_STATISTICS

// static objects that have constructors are kinda bad, but we don't
// care here, this is only debugging code!

static nsHashtable sGEUS_ElementCounts;

void GEUS_ElementCreated(nsINodeInfo *aNodeInfo)
{
  nsAutoString name;
  aNodeInfo->GetLocalName(name);

  nsStringKey key(name);

  PRInt32 count = (PRInt32)sGEUS_ElementCounts.Get(&key);

  count++;

  sGEUS_ElementCounts.Put(&key, (void *)count);
}

PRBool GEUS_enum_func(nsHashKey *aKey, void *aData, void *aClosure)
{
  const PRUnichar *name_chars = ((nsStringKey *)aKey)->GetString();
  NS_ConvertUCS2toUTF8 name(name_chars);

  printf ("%s %d\n", name.get(), aData);

  return PR_TRUE;
}

void GEUS_DumpElementCounts()
{
  printf ("Element count statistics:\n");

  sGEUS_ElementCounts.Enumerate(GEUS_enum_func, nsnull);

  printf ("End of element count statistics:\n");
}

nsresult
nsGenericHTMLElement::Init(nsINodeInfo *aNodeInfo)
{
  GEUS_ElementCreated(aNodeInfo);

  return nsGenericElement::Init(aNodeInfo);
}

#endif


class nsGenericHTMLElementTearoff : public nsIDOMNSHTMLElement,
                                    public nsIDOMElementCSSInlineStyle
{
  NS_DECL_ISUPPORTS

  nsGenericHTMLElementTearoff(nsGenericHTMLElement *aElement)
    : mElement(aElement)
  {
    NS_INIT_ISUPPORTS();
    NS_ADDREF(mElement);
  }

  virtual ~nsGenericHTMLElementTearoff()
  {
    NS_RELEASE(mElement);
  }

  NS_FORWARD_NSIDOMNSHTMLELEMENT(mElement->)
  NS_FORWARD_NSIDOMELEMENTCSSINLINESTYLE(mElement->)

private:
  nsGenericHTMLElement *mElement;
};


NS_IMPL_ADDREF(nsGenericHTMLElementTearoff)
NS_IMPL_RELEASE(nsGenericHTMLElementTearoff)

NS_IMETHODIMP
nsGenericHTMLElementTearoff::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);

  nsISupports *inst = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIDOMNSHTMLElement))) {
    inst = NS_STATIC_CAST(nsIDOMNSHTMLElement *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMElementCSSInlineStyle))) {
    inst = NS_STATIC_CAST(nsIDOMElementCSSInlineStyle *, this);
  } else {
    return mElement->QueryInterface(aIID, aInstancePtr);
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}

static nsICSSOMFactory* gCSSOMFactory = nsnull;
static NS_DEFINE_CID(kCSSOMFactoryCID, NS_CSSOMFACTORY_CID);

nsGenericHTMLElement::nsGenericHTMLElement()
{
  mAttributes = nsnull;
}

nsGenericHTMLElement::~nsGenericHTMLElement()
{
  delete mAttributes;
}

NS_IMETHODIMP
nsGenericHTMLElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NS_SUCCEEDED(nsGenericElement::QueryInterface(aIID, aInstancePtr)))
    return NS_OK;

  nsISupports *inst = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIHTMLContent))) {
    inst = NS_STATIC_CAST(nsIHTMLContent *, this);
  } else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}

nsresult
nsGenericHTMLElement::DOMQueryInterface(nsIDOMHTMLElement *aElement,
                                        REFNSIID aIID, void **aInstancePtr)
{
  nsISupports *inst = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIDOMNode))) {
    inst = NS_STATIC_CAST(nsIDOMNode *, aElement);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMElement))) {
    inst = NS_STATIC_CAST(nsIDOMElement *, aElement);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMHTMLElement))) {
    inst = NS_STATIC_CAST(nsIDOMHTMLElement *, aElement);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMNSHTMLElement))) {
    inst = NS_STATIC_CAST(nsIDOMNSHTMLElement *,
                          new nsGenericHTMLElementTearoff(this));
    NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMElementCSSInlineStyle))) {
    inst = NS_STATIC_CAST(nsIDOMElementCSSInlineStyle *,
                          new nsGenericHTMLElementTearoff(this));
    NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);
  } else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}

/* static */ void
nsGenericHTMLElement::Shutdown()
{
  NS_IF_RELEASE(gCSSOMFactory);
}

nsresult
nsGenericHTMLElement::CopyInnerTo(nsIContent* aSrcContent,
                                  nsGenericHTMLElement* aDst,
                                  PRBool aDeep)
{
  nsresult rv = NS_OK;

  if (mAttributes) {
    PRInt32 index, count;
    GetAttrCount(count);
    nsCOMPtr<nsIAtom> name, prefix;
    PRInt32 namespace_id;
    nsAutoString value;

    for (index = 0; index < count; index++) {
      rv = GetAttrNameAt(index, namespace_id, *getter_AddRefs(name),
                         *getter_AddRefs(prefix));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = GetAttr(namespace_id, name, value);
      NS_ENSURE_SUCCESS(rv, rv);
      if (name == nsHTMLAtoms::style && namespace_id == kNameSpaceID_None) {
        // We can't just set this as a string, because that will fail
        // to reparse the string into style data until the node is
        // inserted into the document.  Clone the HTMLValue instead.
        nsHTMLValue val;
        rv = GetHTMLAttribute(nsHTMLAtoms::style, val);
        if (rv == NS_CONTENT_ATTR_HAS_VALUE &&
            val.GetUnit() == eHTMLUnit_ISupports) {
          nsCOMPtr<nsISupports> supports(dont_AddRef(val.GetISupportsValue()));
          nsCOMPtr<nsICSSStyleRule> rule(do_QueryInterface(supports));

          if (rule) {
            nsCOMPtr<nsICSSRule> ruleClone;

            rv = rule->Clone(*getter_AddRefs(ruleClone));

            val.SetISupportsValue(ruleClone);
            aDst->SetHTMLAttribute(nsHTMLAtoms::style, val, PR_FALSE);
          }
        }
      } else {
        rv = aDst->SetAttr(namespace_id, name, value, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  PRInt32 id;
  if (mDocument) {
    mDocument->GetAndIncrementContentID(&id);
  }

  aDst->SetContentID(id);

  return rv;
}

nsresult
nsGenericHTMLElement::GetTagName(nsAString& aTagName)
{
  return GetNodeName(aTagName);
}

nsresult
nsGenericHTMLElement::GetNodeName(nsAString& aNodeName)
{
  nsresult rv = mNodeInfo->GetQualifiedName(aNodeName);

  if (mNodeInfo->NamespaceEquals(kNameSpaceID_None))
    ToUpperCase(aNodeName);

  return rv;
}

nsresult
nsGenericHTMLElement::GetLocalName(nsAString& aLocalName)
{
  mNodeInfo->GetLocalName(aLocalName);

  if (mNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
    // No namespace, this means we're dealing with a good ol' HTML
    // element, so uppercase the local name.

    ToUpperCase(aLocalName);
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetElementsByTagName(const nsAString& aTagname,
                                           nsIDOMNodeList** aReturn)
{
  nsAutoString tagName(aTagname);

  // Only lowercase the name if this element has no namespace (i.e.
  // it's a HTML element, not an XHTML element).
  if (mNodeInfo && mNodeInfo->NamespaceEquals(kNameSpaceID_None))
    ToLowerCase(tagName);

  return nsGenericElement::GetElementsByTagName(tagName, aReturn);
}

// Implementation for nsIDOMHTMLElement
nsresult
nsGenericHTMLElement::GetId(nsAString& aId)
{
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, aId);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetId(const nsAString& aId)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::id, aId, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetTitle(nsAString& aTitle)
{
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::title, aTitle);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetTitle(const nsAString& aTitle)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::title, aTitle, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetLang(nsAString& aLang)
{
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::lang, aLang);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetLang(const nsAString& aLang)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::lang, aLang, PR_TRUE);
  return NS_OK;
}

static nsGenericHTMLElement::EnumTable kDirTable[] = {
  { "ltr", NS_STYLE_DIRECTION_LTR },
  { "rtl", NS_STYLE_DIRECTION_RTL },
  { 0 }
};

nsresult
nsGenericHTMLElement::GetDir(nsAString& aDir)
{
  nsHTMLValue value;
  nsresult result = GetHTMLAttribute(nsHTMLAtoms::dir, value);

  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    EnumValueToString(value, kDirTable, aDir);
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetDir(const nsAString& aDir)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::dir, aDir, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetClassName(nsAString& aClassName)
{
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::kClass, aClassName);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetClassName(const nsAString& aClassName)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::kClass, aClassName, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  nsresult res = NS_OK;
  nsDOMSlots *slots = GetDOMSlots();

  if (nsnull == slots->mStyle) {
    if (!gCSSOMFactory) {
      res = CallGetService(kCSSOMFactoryCID, &gCSSOMFactory);
      if (NS_FAILED(res))
        return res;
    }

    res = gCSSOMFactory->CreateDOMCSSAttributeDeclaration(this,
                                                          &slots->mStyle);
    if (NS_FAILED(res))
      return res;
  }

  // Why bother with QI?
  NS_IF_ADDREF(*aStyle = slots->mStyle);
  return NS_OK;
}


static inline PRBool
IsBodyTag(nsIAtom *aAtom)
{
  return aAtom == nsHTMLAtoms::body;
}

static inline PRBool
IsOffsetParentTag(nsIAtom *aAtom)
{
  return (aAtom == nsHTMLAtoms::td ||
          aAtom == nsHTMLAtoms::table ||
          aAtom == nsHTMLAtoms::th);
}

nsresult
nsGenericHTMLElement::GetOffsetRect(nsRect& aRect,
                                    nsIContent** aOffsetParent)
{
  *aOffsetParent = nsnull;

  aRect.x = aRect.y = 0;
  aRect.Empty();

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

  // Get the Frame for our content
  nsIFrame* frame = nsnull;
  presShell->GetPrimaryFrameFor(this, &frame);

  if (!frame) {
    return NS_OK;
  }

  // Get the union of all rectangles in this and continuation frames
  nsRect rcFrame;
  nsIFrame* next = frame;

  do {
    nsRect rect;
    next->GetRect(rect);

    rcFrame.UnionRect(rcFrame, rect);

    next->GetNextInFlow(&next);
  } while (next);


  nsCOMPtr<nsIContent> docElement;
  mDocument->GetRootContent(getter_AddRefs(docElement));

  // Find the frame parent whose content's tagName either matches
  // the tagName passed in or is the document element.
  nsCOMPtr<nsIContent> content;
  nsIFrame* parent = nsnull;
  PRBool done = PR_FALSE;
  nsCOMPtr<nsIAtom> tag;

  frame->GetContent(getter_AddRefs(content));

  if (content) {
    content->GetTag(*getter_AddRefs(tag));

    if (IsBodyTag(tag) || content == docElement) {
      done = PR_TRUE;

      parent = frame;
    }
  }

  const nsStyleDisplay* display = nsnull;
  nsPoint origin(0, 0);

  if (!done) {
    PRBool is_absolutely_positioned = PR_FALSE;
    PRBool is_positioned = PR_FALSE;

    frame->GetOrigin(origin);

    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

    if (display && display->IsPositioned()) {
      if (display->IsAbsolutelyPositioned()) {
        // If the primary frame or a parent is absolutely positioned
        // (fixed or absolute) we stop walking up the frame parent
        // chain

        is_absolutely_positioned = PR_TRUE;
      }

      // We need to know if the primary frame is positioned later on.
      is_positioned = PR_TRUE;
    }

    frame->GetParent(&parent);

    while (parent) {
      parent->GetStyleData(eStyleStruct_Display,
                           (const nsStyleStruct*&)display);

      if (display) {
        if (display->IsPositioned()) {
          // Stop at the first *parent* that is positioned (fixed,
          // absolute, or relatiive)

          parent->GetContent(aOffsetParent);

          break;
        }
      }

      // Add the parent's origin to our own to get to the
      // right coordinate system

      if (!is_absolutely_positioned) {
        nsPoint parentOrigin;
        parent->GetOrigin(parentOrigin);
        origin += parentOrigin;
      }

      parent->GetContent(getter_AddRefs(content));

      if (content) {
        // If we've hit the document element, break here
        if (content == docElement) {
          break;
        }

        content->GetTag(*getter_AddRefs(tag));

        // If the tag of this frame is a offset parent tag and this
        // element is *not* positioned, break here. Also break if we
        // hit the body element.
        if ((!is_positioned && IsOffsetParentTag(tag)) || IsBodyTag(tag)) {
          *aOffsetParent = content;
          NS_ADDREF(*aOffsetParent);

          break;
        }
      }

      parent->GetParent(&parent);
    }

    if (is_absolutely_positioned && !*aOffsetParent) {
      // If this element is absolutely positioned, but we don't have
      // an offset parent it means this element is an absolutely
      // positioned child that's not nested inside another positioned
      // element, in this case the element's frame's parent is the
      // frame for the HTML element so we fail to find the body in the
      // parent chain. We want the offset parent in this case to be
      // the body, so we just get the body element from the document.

      nsCOMPtr<nsIDOMHTMLDocument> html_doc(do_QueryInterface(mDocument));

      if (html_doc) {
        nsCOMPtr<nsIDOMHTMLElement> html_element;

        html_doc->GetBody(getter_AddRefs(html_element));

        if (html_element) {
          CallQueryInterface(html_element, aOffsetParent);
        }
      }
    }
  }

  // For the origin, add in the border for the frame
  const nsStyleBorder* border = nsnull;
  nsStyleCoord coord;

#if 0
  // We used to do this to include the border of the frame in the
  // calculations, but I think that's wrong. My tests show that we
  // work more like IE if we don't do this, so lets try this and see
  // if people agree.
  frame->GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border);

  if (border) {
    if (eStyleUnit_Coord == border->mBorder.GetLeftUnit()) {
      origin.x += border->mBorder.GetLeft(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == border->mBorder.GetTopUnit()) {
      origin.y += border->mBorder.GetTop(coord).GetCoordValue();
    }
  }
#endif

  // And subtract out the border for the parent
  if (parent) {
    PRBool includeBorder = PR_TRUE;  // set to false if border-box sizing is used
    const nsStylePosition* position = nsnull;
    parent->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
    if (position && position->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
      includeBorder = PR_FALSE;
    }
    
    if (includeBorder) {
      border = nsnull;

      parent->GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border);
      if (border) {
        if (eStyleUnit_Coord == border->mBorder.GetLeftUnit()) {
          origin.x -= border->mBorder.GetLeft(coord).GetCoordValue();
        }
        if (eStyleUnit_Coord == border->mBorder.GetTopUnit()) {
          origin.y -= border->mBorder.GetTop(coord).GetCoordValue();
        }
      }
    }
  }

  // XXX We should really consider subtracting out padding for
  // content-box sizing, but we should see what IE does....
  
  // Get the scale from that Presentation Context
  float scale;
  context->GetTwipsToPixels(&scale);

  // Convert to pixels using that scale
  aRect.x = NSTwipsToIntPixels(origin.x, scale);
  aRect.y = NSTwipsToIntPixels(origin.y, scale);
  aRect.width = NSTwipsToIntPixels(rcFrame.width, scale);
  aRect.height = NSTwipsToIntPixels(rcFrame.height, scale);

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetOffsetTop(PRInt32* aOffsetTop)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  nsresult res = GetOffsetRect(rcFrame, getter_AddRefs(parent));

  if(NS_SUCCEEDED(res)) {
    *aOffsetTop = rcFrame.y;
  }
  else {
    *aOffsetTop = 0;
  }

  return res;
}

nsresult
nsGenericHTMLElement::GetOffsetLeft(PRInt32* aOffsetLeft)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  nsresult res = GetOffsetRect(rcFrame, getter_AddRefs(parent));

  if(NS_SUCCEEDED(res)) {
    *aOffsetLeft = rcFrame.x;
  }
  else {
    *aOffsetLeft = 0;
  }

  return res;
}

nsresult
nsGenericHTMLElement::GetOffsetWidth(PRInt32* aOffsetWidth)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  nsresult res = GetOffsetRect(rcFrame, getter_AddRefs(parent));

  if(NS_SUCCEEDED(res)) {
    *aOffsetWidth = rcFrame.width;
  }
  else {
    *aOffsetWidth = 0;
  }

  return res;
}

nsresult
nsGenericHTMLElement::GetOffsetHeight(PRInt32* aOffsetHeight)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  nsresult res = GetOffsetRect(rcFrame, getter_AddRefs(parent));

  if(NS_SUCCEEDED(res)) {
    *aOffsetHeight = rcFrame.height;
  }
  else {
    *aOffsetHeight = 0;
  }

  return res;
}

nsresult
nsGenericHTMLElement::GetOffsetParent(nsIDOMElement** aOffsetParent)
{
  NS_ENSURE_ARG_POINTER(aOffsetParent);

  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  nsresult res = GetOffsetRect(rcFrame, getter_AddRefs(parent));
  if (NS_SUCCEEDED(res)) {
    if (parent) {
      res = CallQueryInterface(parent, aOffsetParent);
    } else {
      *aOffsetParent = nsnull;
    }
  }
  return res;
}

nsresult
nsGenericHTMLElement::GetInnerHTML(nsAString& aInnerHTML)
{
  aInnerHTML.Truncate();

  nsCOMPtr<nsIDocument> doc;
  mNodeInfo->GetDocument(*getter_AddRefs(doc));
  if (!doc) {
    return NS_OK; // We rely on the document for doing HTML conversion
  }

  nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *,
                                                                 this)));
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  docEncoder = do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/html");

  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  docEncoder->Init(doc, NS_LITERAL_STRING("text/html"),
                   nsIDocumentEncoder::OutputEncodeEntities);

  nsCOMPtr<nsIDOMRange> range(new nsRange);
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);

  rv = range->SelectNodeContents(thisNode);
  NS_ENSURE_SUCCESS(rv, rv);

  docEncoder->SetRange(range);

  docEncoder->EncodeToString(aInnerHTML);

  return rv;
}

nsresult
nsGenericHTMLElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMRange> range = new nsRange;
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIDOMNSRange> nsrange(do_QueryInterface(range, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *,
                                                                 this)));
  rv = range->SelectNodeContents(thisNode);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = range->DeleteContents();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocumentFragment> df;

  nsCOMPtr<nsIDocument> doc;
  mNodeInfo->GetDocument(*getter_AddRefs(doc));

  nsCOMPtr<nsIScriptContext> scx;
  PRBool scripts_enabled = PR_FALSE;

  if (doc) {
    nsCOMPtr<nsIScriptGlobalObject> sgo;

    doc->GetScriptGlobalObject(getter_AddRefs(sgo));

    if (sgo) {
      sgo->GetContext(getter_AddRefs(scx));

      if (scx) {
        scx->GetScriptsEnabled(&scripts_enabled);
      }
    }
  }

  if (scripts_enabled) {
    // Don't let scripts execute while setting .innerHTML.

    scx->SetScriptsEnabled(PR_FALSE, PR_FALSE);
  }

  rv = nsrange->CreateContextualFragment(aInnerHTML, getter_AddRefs(df));

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMNode> tmpNode;
    rv = thisNode->AppendChild(df, getter_AddRefs(tmpNode));
  }

  if (scripts_enabled) {
    // If we disabled scripts, re-enable them now that we're
    // done. Don't fire JS timeouts when enabling the context here.

    scx->SetScriptsEnabled(PR_TRUE, PR_FALSE);
  }

  return rv;
}

nsresult
nsGenericHTMLElement::GetScrollInfo(nsIScrollableView **aScrollableView,
                                    float *aP2T, float *aT2P)
{
  *aScrollableView = nsnull;
  *aP2T = 0.0f;
  *aT2P = 0.0f;

  // Get the the document
  nsCOMPtr<nsIDocument> doc;
  GetDocument(*getter_AddRefs(doc));
  if (!doc) {
    return NS_OK;
  }

  doc->FlushPendingNotifications(PR_TRUE, PR_FALSE);

  // Get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
  if (!presShell) {
    return NS_OK;
  }

  // Get the primary frame for this element
  nsIFrame *frame = nsnull;
  presShell->GetPrimaryFrameFor(this, &frame);
  if (!frame) {
    return NS_OK;
  }

  // Get the presentation context
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    return NS_OK;
  }

  presContext->GetPixelsToTwips(aP2T);
  presContext->GetTwipsToPixels(aT2P);

  // Get the scrollable frame
  nsIScrollableFrame *scrollFrame = nsnull;
  CallQueryInterface(frame, &scrollFrame);

  if (!scrollFrame) {
    if (mNodeInfo->Equals(nsHTMLAtoms::body)) {
      // The scroll info for the body element should map to the scroll
      // info for the nearest scrollable frame above the body element
      // (i.e. the root scrollable frame).

      do {
        frame->GetParent(&frame);

        if (!frame) {
          break;
        }

        CallQueryInterface(frame, &scrollFrame);
      } while (!scrollFrame);
    }

    if (!scrollFrame) {
      return NS_OK;
    }
  }

  // Get the scrollable view
  scrollFrame->GetScrollableView(presContext, aScrollableView);

  return NS_OK;
}


nsresult
nsGenericHTMLElement::GetScrollTop(PRInt32* aScrollTop)
{
  NS_ENSURE_ARG_POINTER(aScrollTop);
  *aScrollTop = 0;

  nsIScrollableView *view = nsnull;
  nsresult rv = NS_OK;
  float p2t, t2p;

  GetScrollInfo(&view, &p2t, &t2p);

  if (view) {
    nscoord xPos, yPos;
    rv = view->GetScrollPosition(xPos, yPos);

    *aScrollTop = NSTwipsToIntPixels(yPos, t2p);
  }

  return rv;
}

nsresult
nsGenericHTMLElement::SetScrollTop(PRInt32 aScrollTop)
{
  nsIScrollableView *view = nsnull;
  nsresult rv = NS_OK;
  float p2t, t2p;

  GetScrollInfo(&view, &p2t, &t2p);

  if (view) {
    nscoord xPos, yPos;

    rv = view->GetScrollPosition(xPos, yPos);

    if (NS_SUCCEEDED(rv)) {
      rv = view->ScrollTo(xPos, NSIntPixelsToTwips(aScrollTop, p2t),
                          NS_VMREFRESH_IMMEDIATE);
    }
  }

  return rv;
}

nsresult
nsGenericHTMLElement::GetScrollLeft(PRInt32* aScrollLeft)
{
  NS_ENSURE_ARG_POINTER(aScrollLeft);
  *aScrollLeft = 0;

  nsIScrollableView *view = nsnull;
  nsresult rv = NS_OK;
  float p2t, t2p;

  GetScrollInfo(&view, &p2t, &t2p);

  if (view) {
    nscoord xPos, yPos;
    rv = view->GetScrollPosition(xPos, yPos);

    *aScrollLeft = NSTwipsToIntPixels(xPos, t2p);
  }

  return rv;
}

nsresult
nsGenericHTMLElement::SetScrollLeft(PRInt32 aScrollLeft)
{
  nsIScrollableView *view = nsnull;
  nsresult rv = NS_OK;
  float p2t, t2p;

  GetScrollInfo(&view, &p2t, &t2p);

  if (view) {
    nscoord xPos, yPos;
    rv = view->GetScrollPosition(xPos, yPos);

    if (NS_SUCCEEDED(rv)) {
      rv = view->ScrollTo(NSIntPixelsToTwips(aScrollLeft, p2t),
                          yPos, NS_VMREFRESH_IMMEDIATE);
    }
  }

  return rv;
}

nsresult
nsGenericHTMLElement::GetScrollHeight(PRInt32* aScrollHeight)
{
  NS_ENSURE_ARG_POINTER(aScrollHeight);
  *aScrollHeight = 0;

  nsIScrollableView *scrollView = nsnull;
  nsresult rv = NS_OK;
  float p2t, t2p;

  GetScrollInfo(&scrollView, &p2t, &t2p);

  if (!scrollView) {
    return GetOffsetHeight(aScrollHeight);
  }

  // xMax and yMax is the total length of our container
  nscoord xMax, yMax;
  rv = scrollView->GetContainerSize(&xMax, &yMax);

  *aScrollHeight = NSTwipsToIntPixels(yMax, t2p);

  return rv;
}

nsresult
nsGenericHTMLElement::GetScrollWidth(PRInt32* aScrollWidth)
{
  NS_ENSURE_ARG_POINTER(aScrollWidth);
  *aScrollWidth = 0;

  nsIScrollableView *scrollView = nsnull;
  nsresult rv = NS_OK;
  float p2t, t2p;

  GetScrollInfo(&scrollView, &p2t, &t2p);

  if (!scrollView) {
    return GetOffsetWidth(aScrollWidth);
  }

  nscoord xMax, yMax;
  rv = scrollView->GetContainerSize(&xMax, &yMax);

  *aScrollWidth = NSTwipsToIntPixels(xMax, t2p);

  return rv;
}

nsresult
nsGenericHTMLElement::GetClientHeight(PRInt32* aClientHeight)
{
  NS_ENSURE_ARG_POINTER(aClientHeight);
  *aClientHeight = 0;

  nsIScrollableView *scrollView = nsnull;
  nsresult rv = NS_OK;
  float p2t, t2p;

  GetScrollInfo(&scrollView, &p2t, &t2p);

  if (scrollView) {
    const nsIView *view = nsnull;
    nsRect r;

    scrollView->GetClipView(&view);
    view->GetBounds(r);

    *aClientHeight = NSTwipsToIntPixels(r.height, t2p);
  }

  return rv;
}

nsresult
nsGenericHTMLElement::GetClientWidth(PRInt32* aClientWidth)
{
  NS_ENSURE_ARG_POINTER(aClientWidth);
  *aClientWidth = 0;

  nsIScrollableView *scrollView = nsnull;
  nsresult rv = NS_OK;
  float p2t, t2p;

  GetScrollInfo(&scrollView, &p2t, &t2p);

  if (scrollView) {
    const nsIView *view = nsnull;
    nsRect r;

    scrollView->GetClipView(&view);
    view->GetBounds(r);

    *aClientWidth = NSTwipsToIntPixels(r.width, t2p);
  }

  return rv;
}

nsresult
nsGenericHTMLElement::ScrollIntoView(PRBool aTop)
{
  // Get the the document
  nsCOMPtr<nsIDocument> doc;
  GetDocument(*getter_AddRefs(doc));
  if (!doc) {
    return NS_OK;
  }

  // Get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
  if (!presShell) {
    return NS_OK;
  }

  // Get the primary frame for this element
  nsIFrame *frame = nsnull;
  presShell->GetPrimaryFrameFor(this, &frame);
  if (!frame) {
    return NS_OK;
  }

  PRIntn vpercent = aTop ? NS_PRESSHELL_SCROLL_TOP :
    NS_PRESSHELL_SCROLL_ANYWHERE;

  presShell->ScrollFrameIntoView(frame, vpercent,
                                 NS_PRESSHELL_SCROLL_ANYWHERE);

  return NS_OK;
}


static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet* sheet = nsnull;

  if (aDocument) {
    nsCOMPtr<nsIHTMLContentContainer> container(do_QueryInterface(aDocument));

    container->GetAttributeStyleSheet(&sheet);
  }

  return sheet;
}

PRBool
nsGenericHTMLElement::InNavQuirksMode(nsIDocument* aDoc)
{
  nsCOMPtr<nsIHTMLDocument> doc(do_QueryInterface(aDoc));
  if (!doc) {
    return PR_FALSE;
  }

  nsCompatibility mode;
  doc->GetCompatibilityMode(mode);
  return mode == eCompatibility_NavQuirks;
}

nsresult
nsGenericHTMLElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                  PRBool aCompileEventHandlers)
{
  PRBool doNothing = PR_FALSE;
  if (aDocument == mDocument) {
    doNothing = PR_TRUE; // short circuit useless work
  }

  nsresult result = nsGenericElement::SetDocument(aDocument, aDeep,
                                                  aCompileEventHandlers);
  if (NS_FAILED(result)) {
    return result;
  }

  if (!doNothing) {
    if (mDocument && mAttributes) {
      ReparseStyleAttribute();
      nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
      if (sheet) {
        mAttributes->SetStyleSheet(sheet);
        //      sheet->SetAttributesFor(htmlContent, mAttributes); // sync attributes with sheet
        NS_RELEASE(sheet);
      }
    }
  }

  return result;
}

nsresult
nsGenericHTMLElement::FindForm(nsIDOMHTMLFormElement **aForm)
{
  // XXX: Namespaces!!!

  nsCOMPtr<nsIContent> content(this);
  nsCOMPtr<nsIAtom> tag;

  *aForm = nsnull;

  while (content) {
    if (content->IsContentOfType(nsIContent::eHTML)) {
      content->GetTag(*getter_AddRefs(tag));

      // If the current ancestor is a form, return it as our form
      if (tag == nsHTMLAtoms::form) {
        return CallQueryInterface(content, aForm);
      }
    }

    nsIContent *tmp = content;
    tmp->GetParent(*getter_AddRefs(content));

    if (content) {
      PRInt32 i;

      content->IndexOf(tmp, i);

      if (i < 0) {
        // This means 'tmp' is anonymous content, form controls in
        // anonymous content can't refer to the real form, if they do
        // they end up in form.elements n' such, and that's wrong...

        return NS_OK;
      }
    }
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::FindAndSetForm(nsIFormControl *aFormControl)
{
  nsCOMPtr<nsIDOMHTMLFormElement> form;

  FindForm(getter_AddRefs(form));

  if (form) {
    return aFormControl->SetForm(form);
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::HandleDOMEventForAnchors(nsIContent* aOuter,
                                               nsIPresContext* aPresContext,
                                               nsEvent* aEvent,
                                               nsIDOMEvent** aDOMEvent,
                                               PRUint32 aFlags,
                                               nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // Try script event handlers first
  nsresult ret = nsGenericHTMLElement::HandleDOMEvent(aPresContext, aEvent,
                                                      aDOMEvent, aFlags,
                                                      aEventStatus);

  //Need to check if we hit an imagemap area and if so see if we're handling
  //the event on that map or on a link farther up the tree.  If we're on a
  //link farther up, do nothing.
  if (NS_SUCCEEDED(ret)) {
    PRBool targetIsArea = PR_FALSE;

    nsCOMPtr<nsIEventStateManager> esm;
    if (NS_SUCCEEDED(aPresContext->GetEventStateManager(getter_AddRefs(esm))) && esm) {
      nsCOMPtr<nsIContent> target;
      esm->GetEventTargetContent(aEvent, getter_AddRefs(target));
      if (target) {
        nsCOMPtr<nsIAtom> tag;
        target->GetTag(*getter_AddRefs(tag));
        if (tag && tag.get() == nsHTMLAtoms::area) {
          targetIsArea = PR_TRUE;
        }
      }
    }

    if (targetIsArea) {
      //We are over an area.  If our element is not one, then return without
      //running anchor code.
      nsCOMPtr<nsIAtom> tag;
      GetTag(*getter_AddRefs(tag));
      if (tag && tag.get() != nsHTMLAtoms::area) {
        return ret;
      }
    }
  }

  if (NS_FAILED(ret))
    return ret;

  if ((*aEventStatus == nsEventStatus_eIgnore ||
       (*aEventStatus != nsEventStatus_eConsumeNoDefault &&
        (aEvent->message == NS_MOUSE_ENTER_SYNTH ||
         aEvent->message == NS_MOUSE_EXIT_SYNTH))) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE) && !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT)) {

    // If we're here, then aOuter should be an nsILink. We'll use the
    // nsILink interface to get a canonified URL that has been
    // correctly escaped and URL-encoded for the document's charset.

    nsCOMPtr<nsILink> link = do_QueryInterface(aOuter);
    if (!link)
      return NS_ERROR_UNEXPECTED;

    nsXPIDLCString hrefCStr;
    link->GetHrefCString(*getter_Copies(hrefCStr));

    // Only bother to handle the mouse event if there was an href
    // specified.
    if (hrefCStr) {
      NS_ConvertUTF8toUCS2 href(hrefCStr);
      // Strip off any unneeded CF/LF (for Bug 52119)
      // It can't be done in the parser because of Bug 15204
      href.StripChars("\r\n");

      switch (aEvent->message) {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
        {
          // don't make the link grab the focus if there is no link handler
          nsCOMPtr<nsILinkHandler> handler;
          nsresult rv = aPresContext->GetLinkHandler(getter_AddRefs(handler));
          if (NS_SUCCEEDED(rv) && handler && mDocument) {
            // If the window is not active, do not allow the focus to bring the
            // window to the front.  We update the focus controller, but do
            // nothing else.
            nsCOMPtr<nsIFocusController> focusController;
            nsCOMPtr<nsIScriptGlobalObject> globalObj;
            mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
            nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(globalObj));
            win->GetRootFocusController(getter_AddRefs(focusController));
            PRBool isActive = PR_FALSE;
            focusController->GetActive(&isActive);
            if (!isActive) {
              nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(this);
              if(domElement)
                focusController->SetFocusedElement(domElement);
              break;
            }
  
            nsCOMPtr<nsIEventStateManager> stateManager;
            if (NS_OK == aPresContext->GetEventStateManager(getter_AddRefs(stateManager))) {
              stateManager->SetContentState(this, NS_EVENT_STATE_ACTIVE |
                                                  NS_EVENT_STATE_FOCUS);
            }
          }
        }
        break;

      case NS_MOUSE_LEFT_CLICK:
      {
        if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {
          nsInputEvent* inputEvent = NS_STATIC_CAST(nsInputEvent*, aEvent);
          nsAutoString target;
          nsCOMPtr<nsIURI> baseURL;
          GetBaseURL(*getter_AddRefs(baseURL));
          GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, target);
          if (target.IsEmpty()) {
            GetBaseTarget(target);
          }
          if (inputEvent->isControl || inputEvent->isMeta ||
              inputEvent->isAlt ||inputEvent->isShift) {
            break;  // let the click go through so we can handle it in JS/XUL
          }

          ret = TriggerLink(aPresContext, eLinkVerb_Replace, baseURL, href,
                            target, PR_TRUE);

          *aEventStatus = nsEventStatus_eConsumeDoDefault;
        }
      }
      break;

      case NS_KEY_PRESS:
        if (aEvent->eventStructType == NS_KEY_EVENT) {
          nsKeyEvent* keyEvent = NS_STATIC_CAST(nsKeyEvent*, aEvent);
          if (keyEvent->keyCode == NS_VK_RETURN) {
            nsMouseEvent event;
            nsEventStatus status = nsEventStatus_eIgnore;
            nsCOMPtr<nsIContent> mouseContent;

            //fire click
            event.message = NS_MOUSE_LEFT_CLICK;
            event.eventStructType = NS_MOUSE_EVENT;
            nsGUIEvent* guiEvent = NS_STATIC_CAST(nsGUIEvent*, aEvent);
            event.widget = guiEvent->widget;
            event.point = aEvent->point;
            event.refPoint = aEvent->refPoint;
            event.clickCount = 1;
            event.isShift = keyEvent->isShift;
            event.isControl = keyEvent->isControl;
            event.isAlt = keyEvent->isAlt;
            event.isMeta = keyEvent->isMeta;

            nsCOMPtr<nsIPresShell> presShell;
            aPresContext->GetShell(getter_AddRefs(presShell));
            if (presShell) {
              ret = presShell->HandleDOMEventWithTarget(this, &event, &status);
            }
          }
        }
        break;

      // Set the status bar the same for focus and mouseover
      case NS_MOUSE_ENTER_SYNTH:
        *aEventStatus = nsEventStatus_eConsumeNoDefault;
      case NS_FOCUS_CONTENT:
      {
        nsAutoString target;
        nsCOMPtr<nsIURI> baseURL;
        GetBaseURL(*getter_AddRefs(baseURL));
        GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, target);
        if (target.IsEmpty()) {
          GetBaseTarget(target);
        }
        ret = TriggerLink(aPresContext, eLinkVerb_Replace,
                          baseURL, href, target, PR_FALSE);
      }
      break;

      case NS_MOUSE_EXIT_SYNTH:
      {
        *aEventStatus = nsEventStatus_eConsumeNoDefault;
        ret = LeaveLink(aPresContext);
      }
      break;

      default:
        break;
      }
    }
  }
  return ret;
}

NS_IMETHODIMP
nsGenericHTMLElement::GetNameSpaceID(PRInt32& aID) const
{
  // XXX
  // XXX This is incorrect!!!!!!!!!!!!!!!!
  // XXX
  aID = kNameSpaceID_XHTML;
  return NS_OK;
}

nsresult
nsGenericHTMLElement::NormalizeAttrString(const nsAString& aStr,
                                          nsINodeInfo*& aNodeInfo)
{
  // XXX need to validate/strip namespace prefix
  nsAutoString lower(aStr);
  ToLowerCase(lower);

  nsCOMPtr<nsINodeInfoManager> nimgr;
  mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  return nimgr->GetNodeInfo(lower, nsnull, kNameSpaceID_None, aNodeInfo);
}

nsresult
nsGenericHTMLElement::SetAttr(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              PRBool aNotify)
{
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_XHTML,
               "Error, attribute on [X]HTML element set with XHTML namespace, "
               "this is wrong, trust me! Lose the prefix on the attribute!");

  nsresult  result = NS_OK;

  if (aNameSpaceID != kNameSpaceID_None) {
    nsCOMPtr<nsINodeInfoManager> nimgr;
    result = mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_SUCCESS(result, result);

    nsCOMPtr<nsINodeInfo> ni;
    result = nimgr->GetNodeInfo(aAttribute, nsnull, kNameSpaceID_None,
                                *getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(result, result);

    return SetAttr(ni, aValue, aNotify);
  }

  if (aAttribute == nsHTMLAtoms::style) {
    nsHTMLValue parsedValue;
    ParseStyleAttribute(aValue, parsedValue);
    result = SetHTMLAttribute(aAttribute, parsedValue, aNotify);
    return result;
  }
  else {
    // Check for event handlers
    if (IsEventName(aAttribute)) {
      AddScriptEventListener(aAttribute, aValue);
    }
  }

  nsHTMLValue val;

  nsAutoString strValue;
  PRBool modification = PR_TRUE;

  if (StringToAttribute(aAttribute, aValue,
                        val) != NS_CONTENT_ATTR_NOT_THERE) {
    // string value was mapped to nsHTMLValue, set it that way
    return SetHTMLAttribute(aAttribute, val, aNotify);
  }
  else {
    if (ParseCommonAttribute(aAttribute, aValue, val)) {
      // string value was mapped to nsHTMLValue, set it that way
      return SetHTMLAttribute(aAttribute, val, aNotify);
    }

    if (aValue.IsEmpty()) { // if empty string
      val.SetEmptyValue();
      return SetHTMLAttribute(aAttribute, val, aNotify);
    }

    // don't do any update if old == new
    result = GetAttr(aNameSpaceID, aAttribute, strValue);
    if ((NS_CONTENT_ATTR_NOT_THERE != result) && aValue.Equals(strValue)) {
      return NS_OK;
    }

    modification = (result != NS_CONTENT_ATTR_NOT_THERE);

    if (aNotify && (nsnull != mDocument)) {
      mDocument->BeginUpdate();

      mDocument->AttributeWillChange(this, aNameSpaceID, aAttribute);
    }

    // set as string value to avoid another string copy
    nsChangeHint impact = NS_STYLE_HINT_NONE;
    PRInt32 modHint = modification ? PRInt32(nsIDOMMutationEvent::MODIFICATION)
                                   : PRInt32(nsIDOMMutationEvent::ADDITION);
    GetMappedAttributeImpact(aAttribute, modHint, impact);

    nsCOMPtr<nsIHTMLStyleSheet> sheet =
      dont_AddRef(GetAttrStyleSheet(mDocument));

    if (!mAttributes) {
      result = NS_NewHTMLAttributes(&mAttributes);
      NS_ENSURE_SUCCESS(result, result);
    }
    result = mAttributes->SetAttributeFor(aAttribute, aValue,
                                          (impact & ~(nsChangeHint_AttrChange | nsChangeHint_Aural
                                                      | nsChangeHint_Content)) != 0,
                                          this, sheet);
  }

  if (mDocument) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(aAttribute, aNameSpaceID, PR_FALSE, aNotify);

    if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
      nsCOMPtr<nsIDOMEventTarget> node =
        do_QueryInterface(NS_STATIC_CAST(nsIContent *, this));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_ATTRMODIFIED;
      mutation.mTarget = node;

      nsAutoString attrName;
      aAttribute->ToString(attrName);
      nsCOMPtr<nsIDOMAttr> attrNode;
      GetAttributeNode(attrName, getter_AddRefs(attrNode));
      mutation.mRelatedNode = attrNode;

      mutation.mAttrName = aAttribute;
      if (!strValue.IsEmpty())
        mutation.mPrevAttrValue = do_GetAtom(strValue);
      if (!aValue.IsEmpty())
        mutation.mNewAttrValue = do_GetAtom(aValue);
      if (modification)
        mutation.mAttrChange = nsIDOMMutationEvent::MODIFICATION;
      else
        mutation.mAttrChange = nsIDOMMutationEvent::ADDITION;
      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }

    if (aNotify) {
      PRInt32 modHint =
        modification ? PRInt32(nsIDOMMutationEvent::MODIFICATION) :
                       PRInt32(nsIDOMMutationEvent::ADDITION);

      mDocument->AttributeChanged(this, aNameSpaceID, aAttribute, modHint, 
                                  NS_STYLE_HINT_UNKNOWN);
      mDocument->EndUpdate();
    }
  }

  return result;
}

NS_IMETHODIMP
nsGenericHTMLElement::SetAttr(nsINodeInfo* aNodeInfo,
                              const nsAString& aValue,
                              PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsresult rv;
  nsCOMPtr<nsIAtom> localName;
  PRInt32 namespaceID;

  aNodeInfo->GetNameAtom(*getter_AddRefs(localName));
  aNodeInfo->GetNamespaceID(namespaceID);

  NS_ASSERTION(namespaceID != kNameSpaceID_XHTML,
               "Error, attribute on [X]HTML element set with XHTML namespace, "
               "this is wrong, trust me! Lose the prefix on the attribute!");

  if (namespaceID == kNameSpaceID_None) {
    return SetAttr(namespaceID, localName, aValue, aNotify);
  }

  // This code is copied from SetAttr(PRInt32, nsIAtom* ,...
  // It sux that we have it duplicated, but we'll have to live with it
  // until we have better SetAttr signatures in nsIContent

  nsAutoString strValue;

  // don't do any update if old == new
  rv = GetAttr(namespaceID, localName, strValue);
  if (rv != NS_CONTENT_ATTR_NOT_THERE && aValue.Equals(strValue))
    return NS_OK;

  PRBool modification = (rv != NS_CONTENT_ATTR_NOT_THERE);

  if (aNotify && mDocument) {
    mDocument->BeginUpdate();
    mDocument->AttributeWillChange(this, namespaceID, localName);
  }

  if (!mAttributes) {
    rv = NS_NewHTMLAttributes(&mAttributes);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = mAttributes->SetAttributeFor(aNodeInfo, aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mDocument) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(localName, namespaceID, PR_FALSE, aNotify);

    if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
      nsCOMPtr<nsIDOMEventTarget> node =
        do_QueryInterface(NS_STATIC_CAST(nsIContent *, this));

      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_ATTRMODIFIED;
      mutation.mTarget = node;

      nsAutoString attrLocalName, attrNamespace;
      localName->ToString(attrLocalName);
      aNodeInfo->GetNamespaceURI(attrNamespace);
      nsCOMPtr<nsIDOMAttr> attrNode;
      GetAttributeNodeNS(attrNamespace, attrLocalName,
                         getter_AddRefs(attrNode));
      mutation.mRelatedNode = attrNode;

      mutation.mAttrName = localName;
      if (!strValue.IsEmpty())
        mutation.mPrevAttrValue = do_GetAtom(strValue);
      if (!aValue.IsEmpty())
        mutation.mNewAttrValue = do_GetAtom(aValue);
      if (modification)
        mutation.mAttrChange = nsIDOMMutationEvent::MODIFICATION;
      else
        mutation.mAttrChange = nsIDOMMutationEvent::ADDITION;
      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }

    if (aNotify) {
      PRInt32 modHint =
        modification ? PRInt32(nsIDOMMutationEvent::MODIFICATION)
                     : PRInt32(nsIDOMMutationEvent::ADDITION);
      mDocument->AttributeChanged(this, namespaceID, localName, modHint, 
                                  NS_STYLE_HINT_UNKNOWN);
      mDocument->EndUpdate();
    }
  }

  return NS_OK;
}

PRBool nsGenericHTMLElement::IsEventName(nsIAtom* aName)
{
  const PRUnichar *name = nsnull;

  aName->GetUnicode(&name);
  NS_ASSERTION(name, "Null string in atom!");

  if (name[0] != 'o' || name[1] != 'n') {
    return PR_FALSE;
  }

  return (aName == nsLayoutAtoms::onclick                       ||
          aName == nsLayoutAtoms::ondblclick                    ||
          aName == nsLayoutAtoms::onmousedown                   ||
          aName == nsLayoutAtoms::onmouseup                     ||
          aName == nsLayoutAtoms::onmouseover                   ||
          aName == nsLayoutAtoms::onmouseout                    ||
          aName == nsLayoutAtoms::onkeydown                     ||
          aName == nsLayoutAtoms::onkeyup                       ||
          aName == nsLayoutAtoms::onkeypress                    ||
          aName == nsLayoutAtoms::onmousemove                   ||
          aName == nsLayoutAtoms::onload                        ||
          aName == nsLayoutAtoms::onunload                      ||
          aName == nsLayoutAtoms::onabort                       ||
          aName == nsLayoutAtoms::onerror                       ||
          aName == nsLayoutAtoms::onfocus                       ||
          aName == nsLayoutAtoms::onblur                        ||
          aName == nsLayoutAtoms::onsubmit                      ||
          aName == nsLayoutAtoms::onreset                       ||
          aName == nsLayoutAtoms::onchange                      ||
          aName == nsLayoutAtoms::onselect                      || 
          aName == nsLayoutAtoms::onpaint                       ||
          aName == nsLayoutAtoms::onresize                      ||
          aName == nsLayoutAtoms::onscroll                      ||
          aName == nsLayoutAtoms::oninput                       ||
          aName == nsLayoutAtoms::oncontextmenu                 ||
          aName == nsLayoutAtoms::onDOMAttrModified             ||
          aName == nsLayoutAtoms::onDOMCharacterDataModified    || 
          aName == nsLayoutAtoms::onDOMSubtreeModified          ||
          aName == nsLayoutAtoms::onDOMNodeInsertedIntoDocument || 
          aName == nsLayoutAtoms::onDOMNodeRemovedFromDocument  ||
          aName == nsLayoutAtoms::onDOMNodeInserted             || 
          aName == nsLayoutAtoms::onDOMNodeRemoved);
}

static nsChangeHint GetStyleImpactFrom(const nsHTMLValue& aValue)
{
  nsChangeHint hint = NS_STYLE_HINT_NONE;

  if (eHTMLUnit_ISupports == aValue.GetUnit()) {
    nsCOMPtr<nsISupports> supports(dont_AddRef(aValue.GetISupportsValue()));
    nsCOMPtr<nsICSSStyleRule> cssRule(do_QueryInterface(supports));

    if (cssRule) {
      nsCSSDeclaration* declaration = cssRule->GetDeclaration();

      if (declaration) {
        hint = declaration->GetStyleImpact();
      }
    }
  }

  return hint;
}

nsresult
nsGenericHTMLElement::SetHTMLAttribute(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       PRBool aNotify)
{
  nsresult  result = NS_OK;

  nsChangeHint impact = NS_STYLE_HINT_NONE;
  GetMappedAttributeImpact(aAttribute, nsIDOMMutationEvent::MODIFICATION,
                           impact);
  nsCOMPtr<nsIHTMLStyleSheet> sheet;
  if (mDocument) {
    PRBool haveListeners =
      nsGenericElement::HasMutationListeners(this,
                                             NS_EVENT_BITS_MUTATION_ATTRMODIFIED);
    PRBool modification = PR_TRUE;
    nsAutoString oldValueStr;
    if (haveListeners) {
      // save the old attribute so we can set up the mutation event
      // properly
      modification =
        (NS_CONTENT_ATTR_NOT_THERE !=
         GetAttr(kNameSpaceID_None, aAttribute, oldValueStr));
    }
    if (aNotify) {
      mDocument->BeginUpdate();

      mDocument->AttributeWillChange(this, kNameSpaceID_None, aAttribute);

      if (nsHTMLAtoms::style == aAttribute) {
        nsHTMLValue oldValue;
        nsChangeHint oldImpact = NS_STYLE_HINT_NONE;
        // Either we have no listeners or it's a real modification. To
        // cover the former case we need to check the return value of
        // GetHTMLAttribute
        if (modification &&
            NS_CONTENT_ATTR_NOT_THERE != GetHTMLAttribute(aAttribute,
                                                          oldValue)) {
          oldImpact = GetStyleImpactFrom(oldValue);
        }
        impact = GetStyleImpactFrom(aValue);
        NS_UpdateHint(impact, oldImpact);
      }
    }
    sheet = dont_AddRef(GetAttrStyleSheet(mDocument));
    if (sheet) {
      if (!mAttributes) {
        nsresult rv = NS_NewHTMLAttributes(&mAttributes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      PRInt32 count;
      result = mAttributes->SetAttributeFor(aAttribute, aValue,
                                            (impact & ~(nsChangeHint_AttrChange | nsChangeHint_Aural
                                                        | nsChangeHint_Content)) != 0,
                                            this, sheet, count);
      if (0 == count) {
        delete mAttributes;
        mAttributes = nsnull;
      }
    }

    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(aAttribute, kNameSpaceID_None, PR_TRUE,
                                aNotify);

    if (haveListeners) {
      nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_ATTRMODIFIED;
      mutation.mTarget = node;

      nsAutoString attrName;
      aAttribute->ToString(attrName);
      nsCOMPtr<nsIDOMAttr> attrNode;
      GetAttributeNode(attrName, getter_AddRefs(attrNode));
      mutation.mRelatedNode = attrNode;

      mutation.mAttrName = aAttribute;
      nsAutoString newValueStr;
      GetAttr(kNameSpaceID_None, aAttribute, newValueStr);
      if (!newValueStr.IsEmpty())
        mutation.mNewAttrValue = do_GetAtom(newValueStr);
      if (!oldValueStr.IsEmpty())
        mutation.mPrevAttrValue = do_GetAtom(oldValueStr);
      if (modification)
        mutation.mAttrChange = nsIDOMMutationEvent::MODIFICATION;
      else
        mutation.mAttrChange = nsIDOMMutationEvent::ADDITION;
      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }

    if (aNotify) {
      mDocument->AttributeChanged(this, kNameSpaceID_None, aAttribute, nsIDOMMutationEvent::MODIFICATION, impact);
      mDocument->EndUpdate();
    }
  }
  if (!sheet) {  // manage this ourselves and re-sync when we connect to doc
    if (!mAttributes) {
      nsresult rv = NS_NewHTMLAttributes(&mAttributes);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    PRInt32 count;
    result = mAttributes->SetAttributeFor(aAttribute, aValue,
                                          (impact & ~(nsChangeHint_AttrChange | nsChangeHint_Aural
                                                      | nsChangeHint_Content)) != 0,
                                          this, sheet, count);
    if (0 == count) {
      delete mAttributes;
      mAttributes = nsnull;
    }
  }

  return result;
}

nsresult
nsGenericHTMLElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                PRBool aNotify)
{
  nsresult result = NS_OK;

  // Check for event handlers
  if (aNameSpaceID == kNameSpaceID_None &&
      IsEventName(aAttribute)) {
    nsCOMPtr<nsIEventListenerManager> manager;
    GetListenerManager(getter_AddRefs(manager));

    if (manager) {
      result = manager->RemoveScriptEventListener(aAttribute);
    }
  }

  nsChangeHint impact = NS_STYLE_HINT_UNKNOWN;
  if (mDocument) {
    if (aNotify) {
      mDocument->BeginUpdate();

      mDocument->AttributeWillChange(this, aNameSpaceID, aAttribute);

      if (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsHTMLAtoms::style) {
        nsHTMLValue oldValue;
        if (NS_CONTENT_ATTR_NOT_THERE != GetHTMLAttribute(aAttribute,
                                                          oldValue)) {
          impact = GetStyleImpactFrom(oldValue);
        }
        else {
          impact = NS_STYLE_HINT_NONE;
        }
      }
    }

    if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
      nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_ATTRMODIFIED;
      mutation.mTarget = node;

      nsAutoString attrName;
      aAttribute->ToString(attrName);
      nsCOMPtr<nsIDOMAttr> attrNode;
      GetAttributeNode(attrName, getter_AddRefs(attrNode));
      mutation.mRelatedNode = attrNode;

      mutation.mAttrName = aAttribute;

      nsAutoString attr;
      GetAttr(aNameSpaceID, aAttribute, attr);
      if (!attr.IsEmpty())
        mutation.mPrevAttrValue = do_GetAtom(attr);
      mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }
  }

  if (mAttributes) {
    nsCOMPtr<nsIHTMLStyleSheet> sheet
         = dont_AddRef(GetAttrStyleSheet(mDocument));
    PRInt32 count;
    result = mAttributes->UnsetAttributeFor(aAttribute, aNameSpaceID, this,
                                            sheet, count);
    if (0 == count) {
      delete mAttributes;
      mAttributes = nsnull;
    }
  }

  if (mDocument) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(aAttribute, aNameSpaceID, PR_TRUE, aNotify);

    if (aNotify) {
      mDocument->AttributeChanged(this, aNameSpaceID, aAttribute, nsIDOMMutationEvent::REMOVAL, impact);
      mDocument->EndUpdate();
    }
  }

  return result;
}

nsresult
nsGenericHTMLElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                              nsAString& aResult) const
{
  nsCOMPtr<nsIAtom> prefix;
  return GetAttr(aNameSpaceID, aAttribute, *getter_AddRefs(prefix), aResult);
}

nsresult
nsGenericHTMLElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                              nsIAtom*& aPrefix, nsAString& aResult) const
{
  nsresult rv;
  aResult.Truncate();

  aPrefix = nsnull;
  const nsHTMLValue* value;
  if (aNameSpaceID != kNameSpaceID_None) {
    rv = mAttributes ? mAttributes->GetAttribute(aAttribute, aNameSpaceID,
                                                 aPrefix, &value) :
                       NS_CONTENT_ATTR_NOT_THERE;
  } else {
    aNameSpaceID = kNameSpaceID_None;
    rv = mAttributes ? mAttributes->GetAttribute(aAttribute, &value) :
                       NS_CONTENT_ATTR_NOT_THERE;
  }

  aResult.Truncate();
  if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
    // Try subclass conversion routine first
    if (aNameSpaceID == kNameSpaceID_None && 
        AttributeToString(aAttribute, *value, aResult) ==
        NS_CONTENT_ATTR_HAS_VALUE) {
      return rv;
    }

    nscolor color;
    // Provide default conversions for most everything
    switch (value->GetUnit()) {
    case eHTMLUnit_Null:
    case eHTMLUnit_Empty:
      aResult.Truncate();
      break;

    case eHTMLUnit_String:
    case eHTMLUnit_ColorName:
      value->GetStringValue(aResult);
      break;

    case eHTMLUnit_Integer:
      {
        nsAutoString intStr;
        intStr.AppendInt(value->GetIntValue());

        aResult.Assign(intStr);
        break;
      }

    case eHTMLUnit_Pixel:
      {
        nsAutoString intStr;
        intStr.AppendInt(value->GetPixelValue());

        aResult.Assign(intStr);
        break;
      }

    case eHTMLUnit_Percent:
      {
        nsAutoString intStr;
        float percentVal = value->GetPercentValue() * 100.0f;
        intStr.AppendInt(NSToCoordRoundExclusive(percentVal));

        aResult.Assign(intStr);
        aResult.Append(PRUnichar('%'));
        break;
      }

    case eHTMLUnit_Color:
      char cbuf[20];
      color = nscolor(value->GetColorValue());
      PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x",
                  NS_GET_R(color), NS_GET_G(color), NS_GET_B(color));
      aResult.Assign(NS_ConvertASCIItoUCS2(cbuf));
      break;

    default:
    case eHTMLUnit_Enumerated:
      NS_NOTREACHED("no default enumerated value to string conversion");
      rv = NS_CONTENT_ATTR_NOT_THERE;
      break;
    }
  }

  return rv;
}

NS_IMETHODIMP_(PRBool)
nsGenericHTMLElement::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
  return mAttributes && mAttributes->HasAttribute(aName, aNameSpaceID);
}

nsresult
nsGenericHTMLElement::GetHTMLAttribute(nsIAtom* aAttribute,
                                       nsHTMLValue& aValue) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAttribute(aAttribute, aValue);
  }
  aValue.Reset();
  return NS_CONTENT_ATTR_NOT_THERE;
}

nsresult
nsGenericHTMLElement::GetAttrNameAt(PRInt32 aIndex,
                                    PRInt32& aNameSpaceID,
                                    nsIAtom*& aName,
                                    nsIAtom*& aPrefix) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAttributeNameAt(aIndex, aNameSpaceID, aName,
                                           aPrefix);
  }
  aNameSpaceID = kNameSpaceID_None;
  aName = nsnull;
  aPrefix = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult
nsGenericHTMLElement::GetAttrCount(PRInt32& aCount) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAttributeCount(aCount);
  }
  aCount = 0;
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetID(nsIAtom*& aResult) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetID(aResult);
  }
  aResult = nsnull;
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetClasses(nsVoidArray& aArray) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetClasses(aArray);
  }
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsGenericHTMLElement::HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const
{
  if (mAttributes) {
    return mAttributes->HasClass(aClass, aCaseSensitive);
  }
  return PR_FALSE;
}

nsresult
nsGenericHTMLElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  nsresult result = NS_OK;

  if (aRuleWalker) {
    if (mAttributes) {
      result = mAttributes->WalkMappedAttributeStyleRules(aRuleWalker);
    }
  }
  else {
    result = NS_ERROR_NULL_POINTER;
  }
  return result;
}

nsresult
nsGenericHTMLElement::GetInlineStyleRule(nsIStyleRule** aStyleRule)
{
  *aStyleRule = nsnull;
  
  if (mAttributes) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::style, value)) {
      if (eHTMLUnit_ISupports == value.GetUnit()) {
        nsCOMPtr<nsISupports> supports = getter_AddRefs(value.GetISupportsValue());
        if (supports)
          CallQueryInterface(supports, aStyleRule);
      }
    }
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetBaseURL(nsIURI*& aBaseURL) const
{
  nsHTMLValue baseHref;
  if (mAttributes) {
    mAttributes->GetAttribute(nsHTMLAtoms::_baseHref, baseHref);
  }

  nsCOMPtr<nsIDocument> doc(mDocument);

  if (!doc) {
    mNodeInfo->GetDocument(*getter_AddRefs(doc));
  }

  return GetBaseURL(baseHref, doc, &aBaseURL);
}

nsresult
nsGenericHTMLElement::GetBaseURL(const nsHTMLValue& aBaseHref,
                                 nsIDocument* aDocument,
                                 nsIURI** aBaseURL)
{
  nsresult result = NS_OK;

  nsIURI* docBaseURL = nsnull;

  if (aDocument) {
    result = aDocument->GetBaseURL(docBaseURL);
  }

  *aBaseURL = docBaseURL;

  if (eHTMLUnit_String == aBaseHref.GetUnit()) {
    nsAutoString baseHref;
    aBaseHref.GetStringValue(baseHref);
    baseHref.Trim(" \t\n\r");

    nsIURI* url = nsnull;
    result = NS_NewURI(&url, baseHref, nsnull, docBaseURL);

    NS_IF_RELEASE(docBaseURL);
    *aBaseURL = url;
  }

  return result;
}

nsresult
nsGenericHTMLElement::GetBaseTarget(nsAString& aBaseTarget) const
{
  nsresult  result = NS_OK;

  if (nsnull != mAttributes) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::_baseTarget, value)) {
      if (eHTMLUnit_String == value.GetUnit()) {
        value.GetStringValue(aBaseTarget);
        return NS_OK;
      }
    }
  }
  if (nsnull != mDocument) {
    result = mDocument->GetBaseTarget(aBaseTarget);
  }
  else {
    aBaseTarget.Truncate();
  }

  return result;
}

#ifdef DEBUG
void
nsGenericHTMLElement::ListAttributes(FILE* out) const
{
  PRInt32 index, count;
  GetAttrCount(count);
  for (index = 0; index < count; index++) {
    // name
    nsIAtom* attr = nsnull;
    nsIAtom* prefix = nsnull;
    PRInt32 nameSpaceID;
    GetAttrNameAt(index, nameSpaceID, attr, prefix);
    NS_IF_RELEASE(prefix);

    nsAutoString buffer;
    attr->ToString(buffer);

    // value
    nsAutoString value;
    GetAttr(nameSpaceID, attr, value);
    buffer.Append(NS_LITERAL_STRING("=\""));
    for (int i = value.Length(); i >= 0; --i) {
      if (value[i] == PRUnichar('"'))
        value.Insert(PRUnichar('\\'), PRUint32(i));
    }
    buffer.Append(value);
    buffer.Append(NS_LITERAL_STRING("\""));

    fputs(" ", out);
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
    NS_RELEASE(attr);
  }
}

nsresult
nsGenericHTMLElement::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsCOMPtr<nsIAtom> tag;
  GetTag(*getter_AddRefs(tag));
  if (tag) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(NS_LossyConvertUCS2toASCII(buf).get(), out);
  }
  fprintf(out, "@%p", (void*)this);

  ListAttributes(out);

  fprintf(out, " refcount=%d<", mRefCnt.get());

  PRBool canHaveKids;
  CanContainChildren(canHaveKids);
  if (canHaveKids) {
    fputs("\n", out);
    PRInt32 kids;
    ChildCount(kids);
    for (index = 0; index < kids; index++) {
      nsIContent* kid;
      ChildAt(index, kid);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
    for (index = aIndent; --index >= 0; ) fputs("  ", out);
  }
  fputs(">\n", out);

  return NS_OK;
}

nsresult
nsGenericHTMLElement::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const {
   NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buf;
  nsCOMPtr<nsIAtom> tag;
  GetTag(*getter_AddRefs(tag));
  if (tag) {
    tag->ToString(buf);
    fputs("<",out);
    fputs(NS_LossyConvertUCS2toASCII(buf).get(), out);

    if(aDumpAll) ListAttributes(out);

    fputs(">",out);
  }

  PRBool canHaveKids;
  CanContainChildren(canHaveKids);
  if (canHaveKids) {
    if(aIndent) fputs("\n", out);
    PRInt32 kids;
    ChildCount(kids);
    for (index = 0; index < kids; index++) {
      nsIContent* kid;
      ChildAt(index, kid);
      PRInt32 indent=(aIndent)? aIndent+1:0;
      kid->DumpContent(out,indent,aDumpAll);
      NS_RELEASE(kid);
    }
    for (index = aIndent; --index >= 0; ) fputs("  ", out);
    fputs("</",out);
    fputs(NS_LossyConvertUCS2toASCII(buf).get(), out);
    fputs(">",out);

    if(aIndent) fputs("\n", out);
  }

  return NS_OK;
}
#endif


NS_IMETHODIMP_(PRBool)
nsGenericHTMLElement::IsContentOfType(PRUint32 aFlags)
{
  return !(aFlags & ~(eELEMENT | eHTML));
}

#ifdef DEBUG
PRUint32
nsGenericHTMLElement::BaseSizeOf(nsISizeOfHandler* aSizer) const
{
  PRUint32 sum = 0;
  if (mAttributes) {
    PRUint32 attrs = 0;
    mAttributes->SizeOf(aSizer, attrs);
    sum += attrs;
  }
  return sum;
}
#endif


//----------------------------------------------------------------------


nsresult
nsGenericHTMLElement::AttributeToString(nsIAtom* aAttribute,
                                        const nsHTMLValue& aValue,
                                        nsAString& aResult) const
{
  if (nsHTMLAtoms::style == aAttribute) {
    if (eHTMLUnit_ISupports == aValue.GetUnit()) {
      nsCOMPtr<nsISupports> rule = aValue.GetISupportsValue();
      nsCOMPtr<nsICSSStyleRule> cssRule = do_QueryInterface(rule);
      if (cssRule) {
        nsCSSDeclaration* decl = cssRule->GetDeclaration();
        if (decl) {
          decl->ToString(aResult);
        } else {
          aResult.Truncate();
        }
      }
      else {
        aResult.Assign(NS_LITERAL_STRING("Unknown rule type"));
      }
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  } else if (nsHTMLAtoms::dir == aAttribute) {
    nsHTMLValue value;
    nsresult result = GetHTMLAttribute(nsHTMLAtoms::dir, value);

    if (NS_CONTENT_ATTR_HAS_VALUE == result) {
      EnumValueToString(value, kDirTable, aResult);

      return NS_OK;
    }
  }
  aResult.Truncate();
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsGenericHTMLElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                               PRInt32 aModType,
                                               nsChangeHint& aHint) const
{
  if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

#ifdef IBMBIDI
/**
 * Handle attributes on the BDO element
 */
static void
MapBdoAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                     nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_TextReset &&
    aData->mTextData->mUnicodeBidi.GetUnit() == eCSSUnit_Null) {
    aData->mTextData->mUnicodeBidi.SetIntValue(NS_STYLE_UNICODE_BIDI_OVERRIDE, eCSSUnit_Enumerated);
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}
#endif // IBMBIDI

NS_IMETHODIMP
nsGenericHTMLElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
#ifdef IBMBIDI
  if (mNodeInfo->Equals(nsHTMLAtoms::bdo))
    aMapRuleFunc = &MapBdoAttributesInto;
  else
#endif // IBMBIDI
  aMapRuleFunc = &MapCommonAttributesInto;
  return NS_OK;
}

PRBool
nsGenericHTMLElement::ParseEnumValue(const nsAString& aValue,
                                     EnumTable* aTable,
                                     nsHTMLValue& aResult)
{
  nsAutoString val(aValue);
  while (nsnull != aTable->tag) {
    if (val.EqualsIgnoreCase(aTable->tag)) {
      aResult.SetIntValue(aTable->value, eHTMLUnit_Enumerated);
      return PR_TRUE;
    }
    aTable++;
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseCaseSensitiveEnumValue(const nsAString& aValue,
                                                  EnumTable* aTable,
                                                  nsHTMLValue& aResult)
{
  nsAutoString val(aValue);
  while (nsnull != aTable->tag) {
    if (val.EqualsWithConversion(aTable->tag)) {
      aResult.SetIntValue(aTable->value, eHTMLUnit_Enumerated);
      return PR_TRUE;
    }
    aTable++;
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::EnumValueToString(const nsHTMLValue& aValue,
                                        EnumTable* aTable,
                                        nsAString& aResult)
{
  if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
    PRInt32 v = aValue.GetIntValue();
    while (nsnull != aTable->tag) {
      if (aTable->value == v) {
        CopyASCIItoUCS2(nsDependentCString(aTable->tag), aResult);

        return PR_TRUE;
      }
      aTable++;
    }
  }
  aResult.Truncate();
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseValueOrPercent(const nsAString& aString,
                                          nsHTMLValue& aResult,
                                          nsHTMLUnit aValueUnit)
{
  nsAutoString tmp(aString);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (NS_OK == ec) {
    if (val < 0) {
      val = 0;
    }
    if (tmp.RFindChar('%') != kNotFound) {
      if (val > 100) {
        val = 100;
      }
      aResult.SetPercentValue(float(val)/100.0f);
    } else {
      if (eHTMLUnit_Pixel == aValueUnit) {
        aResult.SetPixelValue(val);
      }
      else {
        aResult.SetIntValue(val, aValueUnit);
      }
    }
    return PR_TRUE;
  }

  return PR_FALSE;
}

/* used to parse attribute values that could be either:
 *   integer  (n),
 *   percent  (n%),
 *   or proportional (n*)
 */
PRBool
nsGenericHTMLElement::ParseValueOrPercentOrProportional(const nsAString& aString,
                                                        nsHTMLValue& aResult,
                                                        nsHTMLUnit aValueUnit)
{
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);

  if (NS_OK == ec) {
    if (val < 0) val = 0;
    if (!tmp.IsEmpty() && tmp.RFindChar('%') >= 0) {/* XXX not 100% compatible with ebina's code */
      if (val > 100) val = 100;
      aResult.SetPercentValue(float(val)/100.0f);
    } else if (!tmp.IsEmpty() && tmp.Last() == '*') {
      if (tmp.Length() == 1) {
        // special case: HTML spec says a value '*' == '1*'
        // see http://www.w3.org/TR/html4/types.html#type-multi-length
        // b=29061
        val = 1;
      }
      aResult.SetIntValue(val, eHTMLUnit_Proportional); // proportional values are integers
    } else if (eHTMLUnit_Pixel == aValueUnit) {
        aResult.SetPixelValue(val);
    }
    else {
      aResult.SetIntValue(val, aValueUnit);
    } 
    return PR_TRUE;
  } else if (tmp.Length()==1 && tmp.Last()== '*') {
    aResult.SetIntValue(1, eHTMLUnit_Proportional);
    return PR_TRUE;
  }  
  
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ValueOrPercentToString(const nsHTMLValue& aValue,
                                             nsAString& aResult)
{
  nsAutoString intStr;
  aResult.Truncate(0);
  switch (aValue.GetUnit()) {
    case eHTMLUnit_Integer:
      intStr.AppendInt(aValue.GetIntValue());
      aResult.Append(intStr);
      return PR_TRUE;
    case eHTMLUnit_Pixel:
      intStr.AppendInt(aValue.GetPixelValue());
      aResult.Append(intStr);
      return PR_TRUE;
    case eHTMLUnit_Percent:
      {
      float percentVal = aValue.GetPercentValue() * 100.0f;
      intStr.AppendInt(NSToCoordRoundExclusive(percentVal));
      aResult.Append(intStr);
      aResult.Append(PRUnichar('%'));
      return PR_TRUE;
      }
    default:
      break;
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ValueOrPercentOrProportionalToString(const nsHTMLValue& aValue,
                                                           nsAString& aResult)
{
  nsAutoString intStr;
  aResult.Truncate(0);
  switch (aValue.GetUnit()) {
  case eHTMLUnit_Integer:
    intStr.AppendInt(aValue.GetIntValue());
    aResult.Append(intStr);
    return PR_TRUE;
  case eHTMLUnit_Pixel:
    intStr.AppendInt(aValue.GetPixelValue());
    aResult.Append(intStr);
    return PR_TRUE;
  case eHTMLUnit_Percent:
    {
    float percentVal = aValue.GetPercentValue() * 100.0f;
    intStr.AppendInt(NSToCoordRoundExclusive(percentVal));
    aResult.Append(intStr);
    aResult.Append(NS_LITERAL_STRING("%"));
    return PR_TRUE;
    }
  case eHTMLUnit_Proportional:
    intStr.AppendInt(aValue.GetIntValue());
    aResult.Append(intStr);
    aResult.Append(NS_LITERAL_STRING("*"));
    return PR_TRUE;
  default:
    break;
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseValue(const nsAString& aString, PRInt32 aMin,
                                 PRInt32 aMax,
                                 nsHTMLValue& aResult, nsHTMLUnit aValueUnit)
{
  nsAutoString str(aString);
  PRInt32 ec, val = str.ToInteger(&ec);
  if (NS_OK == ec) {
    if (val < aMin) val = aMin;
    if (val > aMax) val = aMax;
    if (eHTMLUnit_Pixel == aValueUnit) {
      aResult.SetPixelValue(val);
    }
    else {
      aResult.SetIntValue(val, aValueUnit);
    }
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseColor(const nsAString& aString,
                                 nsIDocument* aDocument,
                                 nsHTMLValue& aResult)
{
  if (aString.IsEmpty()) {
    return PR_FALSE;
  }

  // All color strings are one single word so we just strip
  // leading and trailing whitespace before checking.

  // We need a string to remove cruft from
  nsAString::const_iterator iter, end_iter;
  aString.BeginReading(iter);
  aString.EndReading(end_iter);
  PRUnichar the_char;
  // Skip whitespace in the beginning
  while ((iter != end_iter) &&
         (((the_char = *iter) == ' ') ||
          (the_char == '\r') ||
          (the_char == '\t') ||
          (the_char == '\n') ||
          (the_char == '\b')))
    ++iter;
  
  if (iter == end_iter) {
    // Nothing left
    return PR_FALSE;
  }

  --end_iter; // So that it points on a character

  // This will stop at a charater. At very least the same character
  // that stopped the forward iterator.
  while (((the_char = *end_iter)== ' ') ||
         (the_char == '\r') ||
         (the_char == '\t') ||
         (the_char == '\n') ||
         (the_char == '\b'))
    --end_iter;

  nsAutoString colorStr;
  colorStr = Substring(iter, ++end_iter);

  nscolor color;

  // No color names begin with a '#', but numerical colors do so
  // it is a very common first char
  if ((colorStr.CharAt(0) != '#') &&
      NS_ColorNameToRGB(colorStr, &color)) {
    aResult.SetStringValue(colorStr, eHTMLUnit_ColorName);
    return PR_TRUE;
  }

  if (!InNavQuirksMode(aDocument)) {
    if (colorStr.CharAt(0) == '#') {
      colorStr.Cut(0, 1);
      if (NS_HexToRGB(colorStr, &color)) {
        aResult.SetColorValue(color);
        return PR_TRUE;
      }
    }
  }
  else {
    if (NS_LooseHexToRGB(colorStr, &color)) { 
      aResult.SetColorValue(color);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ColorToString(const nsHTMLValue& aValue,
                                    nsAString& aResult)
{
  if (aValue.GetUnit() == eHTMLUnit_Color) {
    nscolor v = aValue.GetColorValue();
    char buf[10];
    PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                NS_GET_R(v), NS_GET_G(v), NS_GET_B(v));
    aResult.Assign(NS_ConvertASCIItoUCS2(buf));
    return PR_TRUE;
  }
  if ((aValue.GetUnit() == eHTMLUnit_ColorName) ||
      (aValue.GetUnit() == eHTMLUnit_String)) {
    aValue.GetStringValue(aResult);
    return PR_TRUE;
  }
  return PR_FALSE;
}

// static
nsIFrame *
nsGenericHTMLElement::GetPrimaryFrameFor(nsIContent* aContent,
                                         nsIDocument* aDocument,
                                         PRBool aFlushContent)
{
  if (aFlushContent) {
    // Cause a flush of content, so we get up-to-date frame
    // information
    aDocument->FlushPendingNotifications(PR_FALSE);
  }

  // Get presentation shell 0
  nsCOMPtr<nsIPresShell> presShell;
  aDocument->GetShellAt(0, getter_AddRefs(presShell));

  if (presShell) {
    nsIFrame *frame = nsnull;
    presShell->GetPrimaryFrameFor(aContent, &frame);
    return frame;
  }

  return nsnull;
}

// static
nsIFormControlFrame*
nsGenericHTMLElement::GetFormControlFrameFor(nsIContent* aContent,
                                             nsIDocument* aDocument,
                                             PRBool aFlushContent)
{
  nsIFrame* frame = GetPrimaryFrameFor(aContent, aDocument, aFlushContent);
  if (frame) {
    nsIFormControlFrame* form_frame = nsnull;
    CallQueryInterface(frame, &form_frame);
    return form_frame;
  }

  return nsnull;
}

nsresult
nsGenericHTMLElement::GetPrimaryPresState(nsIHTMLContent* aContent,
                                          nsIPresState** aPresState)
{
  NS_ENSURE_ARG_POINTER(aPresState);
  *aPresState = nsnull;

  nsresult result = NS_OK;

  nsCOMPtr<nsILayoutHistoryState> history;
  nsCAutoString key;
  GetLayoutHistoryAndKey(aContent, getter_AddRefs(history), key);

  if (history) {
    // Get the pres state for this key, if it doesn't exist, create one
    result = history->GetState(key, aPresState);
    if (!*aPresState) {
      result = CallCreateInstance(kPresStateCID, aPresState);
      if (NS_SUCCEEDED(result)) {
        result = history->AddState(key, *aPresState);
      }
    }
  }

  return result;
}


nsresult
nsGenericHTMLElement::GetLayoutHistoryAndKey(nsIHTMLContent* aContent,
                                             nsILayoutHistoryState** aHistory,
                                             nsACString& aKey)
{
  //
  // Get the pres shell
  //
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = aContent->GetDocument(*getter_AddRefs(doc));
  if (!doc) {
    return rv;
  }

  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  //
  // Get the history (don't bother with the key if the history is not there)
  //
  rv = presShell->GetHistoryState(aHistory);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!*aHistory) {
    return NS_OK;
  }

  //
  // Get the state key
  //
  nsCOMPtr<nsIFrameManager> frameManager;
  presShell->GetFrameManager(getter_AddRefs(frameManager));
  NS_ENSURE_TRUE(frameManager, NS_ERROR_FAILURE);

  rv = frameManager->GenerateStateKey(aContent, nsIStatefulFrame::eNoID, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the state key is blank, this is anonymous content or for
  // whatever reason we are not supposed to save/restore state.
  if (aKey.IsEmpty()) {
    NS_RELEASE(*aHistory);
    return NS_OK;
  }

  // Add something unique to content so layout doesn't muck us up
  aKey += "-C";

  return rv;
}

PRBool
nsGenericHTMLElement::RestoreFormControlState(nsIHTMLContent* aContent,
                                              nsIFormControl* aControl)
{
  nsCOMPtr<nsILayoutHistoryState> history;
  nsCAutoString key;
  nsresult rv = GetLayoutHistoryAndKey(aContent, getter_AddRefs(history), key);
  if (!history) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIPresState> state;
  // Get the pres state for this key
  rv = history->GetState(key, getter_AddRefs(state));
  if (state) {
    rv = aControl->RestoreState(state);
    history->RemoveState(key);
    return NS_SUCCEEDED(rv);
  }

  return PR_FALSE;
}

// XXX This creates a dependency between content and frames
nsresult
nsGenericHTMLElement::GetPresContext(nsIHTMLContent* aContent,
                                     nsIPresContext** aPresContext)
{
  nsIDocument* doc = nsnull;
  nsresult res = NS_NOINTERFACE;
   // Get the document
  if (NS_OK == aContent->GetDocument(doc)) {
    if (nsnull != doc) {
       // Get presentation shell 0
      nsCOMPtr<nsIPresShell> presShell;
      doc->GetShellAt(0, getter_AddRefs(presShell));
      if (nsnull != presShell) {
        res = presShell->GetPresContext(aPresContext);
      }
      NS_RELEASE(doc);
    }
  }

  return res;
}

// XXX check all mappings against ebina's usage
static nsGenericHTMLElement::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },

  { "texttop", NS_STYLE_VERTICAL_ALIGN_TEXT_TOP },// verified
  { "baseline", NS_STYLE_VERTICAL_ALIGN_BASELINE },// verified
  { "center", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "bottom", NS_STYLE_VERTICAL_ALIGN_BASELINE },//verified
  { "top", NS_STYLE_VERTICAL_ALIGN_TOP },//verified
  { "middle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },//verified
  { "absbottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },//verified
  { "abscenter", NS_STYLE_VERTICAL_ALIGN_MIDDLE },/* XXX not the same as ebina */
  { "absmiddle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },/* XXX ditto */
  { 0 }
};

// Elements that should return vertical align values "middle", "bottom", and "top" 
//  instead of "center", "baseline", and "texttop" from GetAttribute() should use this
static nsGenericHTMLElement::EnumTable kVAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "top", NS_STYLE_VERTICAL_ALIGN_TOP },//verified
  { "texttop", NS_STYLE_VERTICAL_ALIGN_TEXT_TOP },// verified
  { "bottom", NS_STYLE_VERTICAL_ALIGN_BASELINE },//verified
  { "baseline", NS_STYLE_VERTICAL_ALIGN_BASELINE },// verified
  { "middle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },//verified
  { "center", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "absbottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },//verified
  { "abscenter", NS_STYLE_VERTICAL_ALIGN_MIDDLE },/* XXX not the same as ebina */
  { "absmiddle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },/* XXX ditto */
  { 0 }
};

static nsGenericHTMLElement::EnumTable kDivAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "justify", NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kFrameborderTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "1", NS_STYLE_FRAME_1 },
  { "0", NS_STYLE_FRAME_0 },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kScrollingTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "on", NS_STYLE_FRAME_ON },
  { "off", NS_STYLE_FRAME_OFF },
  { "scroll", NS_STYLE_FRAME_SCROLL },
  { "noscroll", NS_STYLE_FRAME_NOSCROLL },
  { "auto", NS_STYLE_FRAME_AUTO },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kTableVAlignTable[] = {
  { "top",     NS_STYLE_VERTICAL_ALIGN_TOP },
  { "middle",  NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "bottom",  NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "baseline",NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { 0 }
};

PRBool
nsGenericHTMLElement::ParseCommonAttribute(nsIAtom* aAttribute,
                                           const nsAString& aValue,
                                           nsHTMLValue& aResult)
{
  if (nsHTMLAtoms::dir == aAttribute) {
    return ParseEnumValue(aValue, kDirTable, aResult);
  }
  else if (nsHTMLAtoms::lang == aAttribute) {
    aResult.SetStringValue(aValue);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseAlignValue(const nsAString& aString,
                                      nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kAlignTable, aResult);
}

//----------------------------------------

// Vanilla table as defined by the html4 spec...
static nsGenericHTMLElement::EnumTable kTableHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

// This table is used for TABLE when in compatability mode
static nsGenericHTMLElement::EnumTable kCompatTableHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { "abscenter", NS_STYLE_TEXT_ALIGN_CENTER },
  { "absmiddle", NS_STYLE_TEXT_ALIGN_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

PRBool
nsGenericHTMLElement::ParseTableHAlignValue(const nsAString& aString,
                                            nsHTMLValue& aResult) const
{
  if (InNavQuirksMode(mDocument)) {
    return ParseEnumValue(aString, kCompatTableHAlignTable, aResult);
  }
  return ParseEnumValue(aString, kTableHAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::TableHAlignValueToString(const nsHTMLValue& aValue,
                                               nsAString& aResult) const
{
  if (InNavQuirksMode(mDocument)) {
    return EnumValueToString(aValue, kCompatTableHAlignTable, aResult);
  }
  return EnumValueToString(aValue, kTableHAlignTable, aResult);
}

//----------------------------------------

// These tables are used for TD,TH,TR, etc (but not TABLE)
static nsGenericHTMLElement::EnumTable kTableCellHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kCompatTableCellHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },

  // The following are non-standard but necessary for Nav4 compatibility
  { "middle", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  // allow center and absmiddle to map to NS_STYLE_TEXT_ALIGN_CENTER and
  // NS_STYLE_TEXT_ALIGN_CENTER to map to center by using the following order
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "absmiddle", NS_STYLE_TEXT_ALIGN_CENTER },

  { 0 }
};

PRBool
nsGenericHTMLElement::ParseTableCellHAlignValue(const nsAString& aString,
                                                nsHTMLValue& aResult) const
{
  if (InNavQuirksMode(mDocument)) {
    return ParseEnumValue(aString, kCompatTableCellHAlignTable, aResult);
  }
  return ParseEnumValue(aString, kTableCellHAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::TableCellHAlignValueToString(const nsHTMLValue& aValue,
                                                   nsAString& aResult) const
{
  if (InNavQuirksMode(mDocument)) {
    return EnumValueToString(aValue, kCompatTableCellHAlignTable, aResult);
  }
  return EnumValueToString(aValue, kTableCellHAlignTable, aResult);
}

//----------------------------------------

PRBool
nsGenericHTMLElement::ParseTableVAlignValue(const nsAString& aString,
                                            nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kTableVAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::AlignValueToString(const nsHTMLValue& aValue,
                                         nsAString& aResult)
{
  return EnumValueToString(aValue, kAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::VAlignValueToString(const nsHTMLValue& aValue,
                                         nsAString& aResult)
{
  return EnumValueToString(aValue, kVAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::TableVAlignValueToString(const nsHTMLValue& aValue,
                                               nsAString& aResult)
{
  return EnumValueToString(aValue, kTableVAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::ParseDivAlignValue(const nsAString& aString,
                                         nsHTMLValue& aResult) const
{
  return ParseEnumValue(aString, kDivAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::DivAlignValueToString(const nsHTMLValue& aValue,
                                            nsAString& aResult) const
{
  return EnumValueToString(aValue, kDivAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::ParseImageAttribute(nsIAtom* aAttribute,
                                          const nsAString& aString,
                                          nsHTMLValue& aResult)
{
  if ((aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height)) {
    return ParseValueOrPercent(aString, aResult, eHTMLUnit_Pixel);
  }
  else if ((aAttribute == nsHTMLAtoms::hspace) ||
           (aAttribute == nsHTMLAtoms::vspace) ||
           (aAttribute == nsHTMLAtoms::border)) {
    return ParseValue(aString, 0, aResult, eHTMLUnit_Pixel);
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ImageAttributeToString(nsIAtom* aAttribute,
                                             const nsHTMLValue& aValue,
                                             nsAString& aResult)
{
  if ((aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height) ||
      (aAttribute == nsHTMLAtoms::border) ||
      (aAttribute == nsHTMLAtoms::hspace) ||
      (aAttribute == nsHTMLAtoms::vspace)) {
    return ValueOrPercentToString(aValue, aResult);
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseFrameborderValue(const nsAString& aString,
                                            nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kFrameborderTable, aResult);
}

PRBool
nsGenericHTMLElement::FrameborderValueToString(const nsHTMLValue& aValue,
                                               nsAString& aResult)
{
  return EnumValueToString(aValue, kFrameborderTable, aResult);
}

PRBool
nsGenericHTMLElement::ParseScrollingValue(const nsAString& aString,
                                          nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kScrollingTable, aResult);
}

PRBool
nsGenericHTMLElement::ScrollingValueToString(const nsHTMLValue& aValue,
                                             nsAString& aResult)
{
  return EnumValueToString(aValue, kScrollingTable, aResult);
}

nsresult
nsGenericHTMLElement::ReparseStyleAttribute(void)
{
  nsresult result = NS_OK;
  nsHTMLValue oldValue;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetHTMLAttribute(nsHTMLAtoms::style, oldValue)) {
    if (eHTMLUnit_String == oldValue.GetUnit()) {
      nsHTMLValue parsedValue;
      nsAutoString  stringValue;
      result = ParseStyleAttribute(oldValue.GetStringValue(stringValue), parsedValue);
      if (NS_SUCCEEDED(result) && (eHTMLUnit_String != parsedValue.GetUnit())) {
        result = SetHTMLAttribute(nsHTMLAtoms::style, parsedValue, PR_FALSE);
      }
    }
  }
  return result;
}

nsresult
nsGenericHTMLElement::ParseStyleAttribute(const nsAString& aValue, nsHTMLValue& aResult)
{
  nsresult result = NS_OK;
  NS_ASSERTION(mNodeInfo, "If we don't have a nodeinfo, we are very screwed");

  nsCOMPtr<nsIDocument> doc = mDocument;

  if (!doc) {
    mNodeInfo->GetDocument(*getter_AddRefs(doc));
  }

  if (doc) {
    PRBool isCSS = PR_TRUE; // asume CSS until proven otherwise

    nsAutoString styleType;
    doc->GetHeaderData(nsHTMLAtoms::headerContentStyleType, styleType);
    if (!styleType.IsEmpty()) {
      static const char textCssStr[] = "text/css";
      isCSS = (styleType.EqualsIgnoreCase(textCssStr, sizeof(textCssStr) - 1));
    }

    if (isCSS) {
      nsCOMPtr<nsICSSLoader> cssLoader;
      nsCOMPtr<nsICSSParser> cssParser;
      nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(doc));

      if (htmlContainer) {
        htmlContainer->GetCSSLoader(*getter_AddRefs(cssLoader));
      }
      if (cssLoader) {
        result = cssLoader->GetParserFor(nsnull, getter_AddRefs(cssParser));
      }
      else {
        result = NS_NewCSSParser(getter_AddRefs(cssParser));
        if (cssParser) {
          // look up our namespace.  If we're XHTML, we need to be case-sensitive
          // Otherwise, we should not be.
          cssParser->SetCaseSensitive(mNodeInfo->NamespaceEquals(kNameSpaceID_XHTML));
        }
      }
      if (cssParser) {
        nsCOMPtr<nsIURI> docURL;
        doc->GetBaseURL(*getter_AddRefs(docURL));

        nsCOMPtr<nsIStyleRule> rule;
        result = cssParser->ParseStyleAttribute(aValue, docURL, getter_AddRefs(rule));
        if (cssLoader) {
          cssLoader->RecycleParser(cssParser);
        }

        if (rule) {
          aResult.SetISupportsValue(rule);
          return NS_OK;
        }
      }
    }
  }
  aResult.SetStringValue(aValue);
  return result;
}

/**
 * Handle attributes common to all html elements
 */
void
nsGenericHTMLElement::MapCommonAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                                              nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_TextReset) {
    if (aData->mTextData->mUnicodeBidi.GetUnit() == eCSSUnit_Null) {
      nsHTMLValue value;
      aAttributes->GetAttribute(nsHTMLAtoms::dir, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated)
        aData->mTextData->mUnicodeBidi.SetIntValue(
            NS_STYLE_UNICODE_BIDI_EMBED, eCSSUnit_Enumerated);
    }
  } else if (aData->mSID == eStyleStruct_Visibility) {
    if (aData->mDisplayData->mDirection.GetUnit() == eCSSUnit_Null) {
      nsHTMLValue value;
      aAttributes->GetAttribute(nsHTMLAtoms::dir, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated)
        aData->mDisplayData->mDirection.SetIntValue(value.GetIntValue(),
                                                    eCSSUnit_Enumerated);
    }
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::lang, value);
    if (value.GetUnit() == eHTMLUnit_String) {
      nsAutoString lang;
      value.GetStringValue(lang);
      aData->mDisplayData->mLang.SetStringValue(lang,
                                                eCSSUnit_String);
    }
  }
}

PRBool
nsGenericHTMLElement::GetCommonMappedAttributesImpact(const nsIAtom* aAttribute,
                                                      nsChangeHint& aHint)
{
  if (nsHTMLAtoms::dir == aAttribute) {
    aHint = NS_STYLE_HINT_REFLOW;  // XXX really? possibly FRAMECHANGE?
    return PR_TRUE;
  }
  else if (nsHTMLAtoms::lang == aAttribute) {
    aHint = NS_STYLE_HINT_REFLOW; // LANG attribute affects font selection
    return PR_TRUE;
  }
  /*
     We should not REFRAME for a class change;
     let the resulting style decide the impact
     (bug 21225, mja)
  */
#if 0
  else if (nsHTMLAtoms::kClass == aAttribute) {		// bug 8862
    aHint = NS_STYLE_HINT_FRAMECHANGE;
    return PR_TRUE;
  }
#endif

  else if (nsHTMLAtoms::_baseHref == aAttribute) {
    aHint = NS_STYLE_HINT_VISUAL; // at a minimum, elements may need to override
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::GetImageMappedAttributesImpact(const nsIAtom* aAttribute,
                                                     nsChangeHint& aHint)
{
  if ((nsHTMLAtoms::width == aAttribute) ||
      (nsHTMLAtoms::height == aAttribute) ||
      (nsHTMLAtoms::hspace == aAttribute) ||
      (nsHTMLAtoms::vspace == aAttribute)) {
    aHint = NS_STYLE_HINT_REFLOW;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsGenericHTMLElement::MapAlignAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                            nsRuleData* aRuleData)
{
  if (aRuleData->mSID == eStyleStruct_Display || aRuleData->mSID == eStyleStruct_TextReset) {
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      PRUint8 align = (PRUint8)(value.GetIntValue());
      if (aRuleData->mDisplayData && aRuleData->mDisplayData->mFloat.GetUnit() == eCSSUnit_Null) {
        if (align == NS_STYLE_TEXT_ALIGN_LEFT)
          aRuleData->mDisplayData->mFloat.SetIntValue(NS_STYLE_FLOAT_LEFT, eCSSUnit_Enumerated);
        else if (align == NS_STYLE_TEXT_ALIGN_RIGHT)
          aRuleData->mDisplayData->mFloat.SetIntValue(NS_STYLE_FLOAT_RIGHT, eCSSUnit_Enumerated);
      }
      else if (aRuleData->mTextData && aRuleData->mTextData->mVerticalAlign.GetUnit() == eCSSUnit_Null) {
        switch (align) {
        case NS_STYLE_TEXT_ALIGN_LEFT:
        case NS_STYLE_TEXT_ALIGN_RIGHT:
          break;
        default:
          aRuleData->mTextData->mVerticalAlign.SetIntValue(align, eCSSUnit_Enumerated);
          break;
        }
      }
    }
  }
}

void
nsGenericHTMLElement::MapDivAlignAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                               nsRuleData* aRuleData)
{
  if (aRuleData->mSID == eStyleStruct_Text && aRuleData->mTextData) {
    if (aRuleData->mTextData->mTextAlign.GetUnit() == eCSSUnit_Null) {
      // align: enum
      nsHTMLValue value;
      aAttributes->GetAttribute(nsHTMLAtoms::align, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated)
        aRuleData->mTextData->mTextAlign.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
    }
  }
}

PRBool
nsGenericHTMLElement::GetImageAlignAttributeImpact(const nsIAtom* aAttribute,
                                                   nsChangeHint& aHint)
{
  if ((nsHTMLAtoms::align == aAttribute)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsGenericHTMLElement::MapImageMarginAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  if (aData->mSID != eStyleStruct_Margin || !aData->mMarginData)
    return;

  nsHTMLValue value;

  // hspace: value
  aAttributes->GetAttribute(nsHTMLAtoms::hspace, value);
  nsCSSValue hval;
  if (value.GetUnit() == eHTMLUnit_Pixel)
    hval.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
  else if (value.GetUnit() == eHTMLUnit_Percent)
    hval.SetPercentValue(value.GetPercentValue());

  if (hval.GetUnit() != eCSSUnit_Null) {
    nsCSSRect* margin = aData->mMarginData->mMargin;
    if (margin->mLeft.GetUnit() == eCSSUnit_Null)
      margin->mLeft = hval;
    if (margin->mRight.GetUnit() == eCSSUnit_Null)
      margin->mRight = hval;
  }

  // vspace: value
  aAttributes->GetAttribute(nsHTMLAtoms::vspace, value);
  nsCSSValue vval;
  if (value.GetUnit() == eHTMLUnit_Pixel)
    vval.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
  else if (value.GetUnit() == eHTMLUnit_Percent)
    vval.SetPercentValue(value.GetPercentValue());

  if (vval.GetUnit() != eCSSUnit_Null) {
    nsCSSRect* margin = aData->mMarginData->mMargin;
    if (margin->mTop.GetUnit() == eCSSUnit_Null)
      margin->mTop = vval;
    if (margin->mBottom.GetUnit() == eCSSUnit_Null)
      margin->mBottom = vval;
  }
}

void
nsGenericHTMLElement::MapImagePositionAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                                    nsRuleData* aData)
{
  if (!aAttributes || aData->mSID != eStyleStruct_Position || !aData->mPositionData)
    return;

  nsHTMLValue value;
  
  // width: value
  if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
    aAttributes->GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel)
      aData->mPositionData->mWidth.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
    else if (value.GetUnit() == eHTMLUnit_Percent)
      aData->mPositionData->mWidth.SetPercentValue(value.GetPercentValue());
  }

  // height: value
  if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
    aAttributes->GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel)
      aData->mPositionData->mHeight.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel); 
    else if (value.GetUnit() == eHTMLUnit_Percent)
      aData->mPositionData->mHeight.SetPercentValue(value.GetPercentValue());    
  }
}

void
nsGenericHTMLElement::MapImageBorderAttributeInto(const nsIHTMLMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  if (aData->mSID != eStyleStruct_Border || !aData->mMarginData)
    return;

  nsHTMLValue value;

  // border: pixels
  aAttributes->GetAttribute(nsHTMLAtoms::border, value);
  if (value.GetUnit() == eHTMLUnit_Null)
    return;
  
  if (value.GetUnit() != eHTMLUnit_Pixel)  // something other than pixels
    value.SetPixelValue(0);

  nscoord val = value.GetPixelValue();

  nsCSSRect* borderWidth = aData->mMarginData->mBorderWidth;
  if (borderWidth->mLeft.GetUnit() == eCSSUnit_Null)
    borderWidth->mLeft.SetFloatValue((float)val, eCSSUnit_Pixel);
  if (borderWidth->mTop.GetUnit() == eCSSUnit_Null)
    borderWidth->mTop.SetFloatValue((float)val, eCSSUnit_Pixel);
  if (borderWidth->mRight.GetUnit() == eCSSUnit_Null)
    borderWidth->mRight.SetFloatValue((float)val, eCSSUnit_Pixel);
  if (borderWidth->mBottom.GetUnit() == eCSSUnit_Null)
    borderWidth->mBottom.SetFloatValue((float)val, eCSSUnit_Pixel);

  nsCSSRect* borderStyle = aData->mMarginData->mBorderStyle;
  if (borderStyle->mLeft.GetUnit() == eCSSUnit_Null)
    borderStyle->mLeft.SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  if (borderStyle->mTop.GetUnit() == eCSSUnit_Null)
    borderStyle->mTop.SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  if (borderStyle->mRight.GetUnit() == eCSSUnit_Null)
    borderStyle->mRight.SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  if (borderStyle->mBottom.GetUnit() == eCSSUnit_Null)
    borderStyle->mBottom.SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);

  nsCSSRect* borderColor = aData->mMarginData->mBorderColor;
  if (borderColor->mLeft.GetUnit() == eCSSUnit_Null)
    borderColor->mLeft.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  if (borderColor->mTop.GetUnit() == eCSSUnit_Null)
    borderColor->mTop.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  if (borderColor->mRight.GetUnit() == eCSSUnit_Null)
    borderColor->mRight.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  if (borderColor->mBottom.GetUnit() == eCSSUnit_Null)
    borderColor->mBottom.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
}

PRBool
nsGenericHTMLElement::GetImageBorderAttributeImpact(const nsIAtom* aAttribute,
                                                    nsChangeHint& aHint)
{
  if ((nsHTMLAtoms::border == aAttribute)) {
    aHint = NS_STYLE_HINT_REFLOW;
    return PR_TRUE;
  }
  return PR_FALSE;
}


void
nsGenericHTMLElement::MapBackgroundAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  if (!aData || !aData->mColorData || aData->mSID != eStyleStruct_Background)
    return;

  if (aData->mColorData->mBackImage.GetUnit() == eCSSUnit_Null) {
    // background
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        aAttributes->GetAttribute(nsHTMLAtoms::background, value)) {
      if (eHTMLUnit_String == value.GetUnit()) {
        nsAutoString absURLSpec;
        nsAutoString spec;
        value.GetStringValue(spec);
        if (!spec.IsEmpty()) {
          // Resolve url to an absolute url
          nsCOMPtr<nsIPresShell> shell;
          nsresult rv = aData->mPresContext->GetShell(getter_AddRefs(shell));
          if (NS_SUCCEEDED(rv) && shell) {
            nsCOMPtr<nsIDocument> doc;
            rv = shell->GetDocument(getter_AddRefs(doc));
            if (NS_SUCCEEDED(rv) && doc) {
              nsCOMPtr<nsIURI> docURL;
              nsHTMLValue baseHref;
              aAttributes->GetAttribute(nsHTMLAtoms::_baseHref, baseHref);
              nsGenericHTMLElement::GetBaseURL(baseHref, doc,
                                               getter_AddRefs(docURL));
              rv = NS_MakeAbsoluteURI(absURLSpec, spec, docURL);
              if (NS_SUCCEEDED(rv))
                aData->mColorData->mBackImage.SetStringValue(absURLSpec,
                                                             eCSSUnit_URL);
            }
          }
        }
      } else if (aData->mPresContext) {
        // in NavQuirks mode, allow the empty string to set the
        // background to empty
        nsCompatibility mode;
        aData->mPresContext->GetCompatibilityMode(&mode);
        if (eCompatibility_NavQuirks == mode &&
            eHTMLUnit_Empty == value.GetUnit())
          aData->mColorData->mBackImage.SetStringValue(NS_LITERAL_STRING(""),
                                                       eCSSUnit_URL);
      }
    }
  }

  // bgcolor
  if (aData->mColorData->mBackColor.GetUnit() == eCSSUnit_Null) {
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::bgcolor, value);
    if ((eHTMLUnit_Color == value.GetUnit()) ||
        (eHTMLUnit_ColorName == value.GetUnit()))
      aData->mColorData->mBackColor.SetColorValue(value.GetColorValue());
  }
}

PRBool
nsGenericHTMLElement::GetBackgroundAttributesImpact(const nsIAtom* aAttribute,
                                                    nsChangeHint& aHint)
{
  if ((nsHTMLAtoms::background == aAttribute) ||
      (nsHTMLAtoms::bgcolor == aAttribute)) {
    aHint = NS_STYLE_HINT_VISUAL;
    return PR_TRUE;
  }
  return PR_FALSE;
}


//----------------------------------------------------------------------

nsGenericHTMLLeafElement::nsGenericHTMLLeafElement()
{
}

nsGenericHTMLLeafElement::~nsGenericHTMLLeafElement()
{
}

nsresult
nsGenericHTMLLeafElement::CopyInnerTo(nsIContent* aSrcContent,
                                      nsGenericHTMLLeafElement* aDst,
                                      PRBool aDeep)
{
  nsresult result = nsGenericHTMLElement::CopyInnerTo(aSrcContent,
                                                      aDst,
                                                      aDeep);
  return result;
}

NS_IMETHODIMP
nsGenericHTMLLeafElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  nsDOMSlots* slots = GetDOMSlots();

  if (!slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(nsnull);
    if (!slots->mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mChildNodes);
  }

  return CallQueryInterface(slots->mChildNodes, aChildNodes);
}

//----------------------------------------------------------------------

nsGenericHTMLContainerElement::nsGenericHTMLContainerElement()
{
}

nsGenericHTMLContainerElement::~nsGenericHTMLContainerElement()
{
  PRInt32 n = mChildren.Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsIContent* kid = (nsIContent *)mChildren.ElementAt(i);
    kid->SetParent(nsnull);
    NS_RELEASE(kid);
  }
}

nsresult
nsGenericHTMLContainerElement::CopyInnerTo(nsIContent* aSrcContent,
                                           nsGenericHTMLContainerElement* aDst,
                                           PRBool aDeep)
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aSrcContent, aDst, aDeep);

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aDeep) {
    PRInt32 indx;
    PRInt32 count = mChildren.Count();
    for (indx = 0; indx < count; indx++) {
      nsIContent* child = (nsIContent*)mChildren.ElementAt(indx);

      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(child));

      if (node) {
        nsCOMPtr<nsIDOMNode> newNode;

        rv = node->CloneNode(aDeep, getter_AddRefs(newNode));
        if (node) {
          nsCOMPtr<nsIContent> newContent(do_QueryInterface(newNode));

          if (newContent) {
            rv = aDst->AppendChildTo(newContent, PR_FALSE, PR_FALSE);
            }
          }
        }

      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  nsDOMSlots *slots = GetDOMSlots();

  if (!slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(this);
    // XXX Need to check for out-of-memory
    NS_ADDREF(slots->mChildNodes);
  }

  return CallQueryInterface(slots->mChildNodes, aChildNodes);
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::HasChildNodes(PRBool* aReturn)
{
  if (0 != mChildren.Count()) {
    *aReturn = PR_TRUE;
  }
  else {
    *aReturn = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::GetFirstChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.SafeElementAt(0);
  if (child) {
    nsresult res = CallQueryInterface(child, aNode);
    NS_ASSERTION(NS_SUCCEEDED(res), "Must be a DOM Node"); // must be a DOM Node

    return res;
  }
  *aNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::GetLastChild(nsIDOMNode** aNode)
{
  if (0 != mChildren.Count()) {
    nsIContent *child = (nsIContent *)mChildren.ElementAt(mChildren.Count()-1);
    if (child) {
      nsresult res = CallQueryInterface(child, aNode);
      NS_ASSERTION(NS_SUCCEEDED(res), "Must be a DOM Node"); // must be a DOM Node

      return res;
    }
  }
  *aNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::Compact()
{
  mChildren.Compact();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::CanContainChildren(PRBool& aResult) const
{
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::ChildCount(PRInt32& aCount) const
{
  aCount = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::ChildAt(PRInt32 aIndex,
                                       nsIContent*& aResult) const
{
  // I really prefer NOT to do this test on all ChildAt calls - perhaps we
  // should add FastChildAt().
  nsIContent *child = (nsIContent *)mChildren.SafeElementAt(aIndex);
  NS_IF_ADDREF(child);
  aResult = child;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::IndexOf(nsIContent* aPossibleChild,
                                       PRInt32& aIndex) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");
  aIndex = mChildren.IndexOf(aPossibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::InsertChildAt(nsIContent* aKid,
                                             PRInt32 aIndex,
                                             PRBool aNotify,
                                             PRBool aDeepSetDocument)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsRange::OwnerChildInserted(this, aIndex);
    if (nsnull != doc) {
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
      if (aNotify) {
        doc->ContentInserted(this, aKid, aIndex);
      }

      if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
        nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aKid));
        nsMutationEvent mutation;
        mutation.eventStructType = NS_MUTATION_EVENT;
        mutation.message = NS_MUTATION_NODEINSERTED;
        mutation.mTarget = node;

        nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
        mutation.mRelatedNode = relNode;

        nsEventStatus status = nsEventStatus_eIgnore;
        aKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
      }
    }
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::ReplaceChildAt(nsIContent* aKid,
                                              PRInt32 aIndex,
                                              PRBool aNotify,
                                              PRBool aDeepSetDocument)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIContent* oldKid = NS_STATIC_CAST(nsIContent *,
                                      mChildren.SafeElementAt(aIndex));
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  nsRange::OwnerChildReplaced(this, aIndex, oldKid);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    if (nsnull != doc) {
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
      if (aNotify) {
        doc->ContentReplaced(this, oldKid, aKid, aIndex);
      }
    }
    if (oldKid) {
      oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);
      oldKid->SetParent(nsnull);
      NS_RELEASE(oldKid);
    }
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                             PRBool aDeepSetDocument)
{
  NS_PRECONDITION(nsnull != aKid && this != aKid, "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    // ranges don't need adjustment since new child is at end of list
    if (nsnull != doc) {
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
      if (aNotify) {
        doc->ContentAppended(this, mChildren.Count() - 1);
      }

      if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
        nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aKid));
        nsMutationEvent mutation;
        mutation.eventStructType = NS_MUTATION_EVENT;
        mutation.message = NS_MUTATION_NODEINSERTED;
        mutation.mTarget = node;

        nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
        mutation.mRelatedNode = relNode;

        nsEventStatus status = nsEventStatus_eIgnore;
        aKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
      }
    }
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  nsIContent* oldKid = NS_STATIC_CAST(nsIContent *,
                                      mChildren.SafeElementAt(aIndex));
  if (nsnull != oldKid ) {

    if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
      nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(oldKid));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_NODEREMOVED;
      mutation.mTarget = node;

      nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
      mutation.mRelatedNode = relNode;

      nsEventStatus status = nsEventStatus_eIgnore;
      oldKid->HandleDOMEvent(nsnull, &mutation, nsnull,
                             NS_EVENT_FLAG_INIT, &status);
    }

    nsRange::OwnerChildRemoved(this, aIndex, oldKid);

    mChildren.RemoveElementAt(aIndex);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentRemoved(this, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }

  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::ReplaceContentsWithText(const nsAString& aText,
                                                       PRBool aNotify) {
  PRInt32 children;
  nsresult rv = ChildCount(children);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> firstChild;
  nsCOMPtr<nsIDOMText> textChild;
  // if we already have a DOMText child, reuse it.
  if (children > 0) {
    rv = ChildAt(0, *getter_AddRefs(firstChild));
    NS_ENSURE_SUCCESS(rv, rv);
    textChild = do_QueryInterface(firstChild);
  }
  
  PRInt32 i;
  PRInt32 lastChild = textChild ? 1 : 0;
  for (i = children - 1; i >= lastChild; --i) {
    RemoveChildAt(i, aNotify);
  }

  if (!textChild) {
    nsCOMPtr<nsITextContent> text;
    rv = NS_NewTextNode(getter_AddRefs(text));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = text->SetText(aText, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = InsertChildAt(text, 0, aNotify, PR_FALSE);
  } else {
    rv = textChild->SetData(aText);
  }    
      
  return rv;
}

nsresult
nsGenericHTMLContainerElement::GetContentsAsText(nsAString& aText)
{
  aText.Truncate();
  PRInt32 children;
  nsresult rv = ChildCount(children);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMText> tc;
  nsCOMPtr<nsIContent> child;
  nsAutoString textData;

  PRInt32 i;
  for (i = 0; i < children; ++i) {
    ChildAt(i, *getter_AddRefs(child));
    tc = do_QueryInterface(child);
    if (tc) {
      if (aText.IsEmpty()) {
        tc->GetData(aText);
      } else {
        tc->GetData(textData);
        aText.Append(textData);
      }
    }
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------

nsGenericHTMLContainerFormElement::nsGenericHTMLContainerFormElement()
{
  mForm = nsnull;
}

nsGenericHTMLContainerFormElement::~nsGenericHTMLContainerFormElement()
{
  // Clean up.  Set the form to nsnull so it knows we went away.
  SetForm(nsnull);
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::QueryInterface(REFNSIID aIID,
                                                  void** aInstancePtr)
{
  if (NS_SUCCEEDED(nsGenericHTMLElement::QueryInterface(aIID, aInstancePtr)))
    return NS_OK;

  nsISupports *inst = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIFormControl))) {
    inst = NS_STATIC_CAST(nsIFormControl *, this);
  } else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsGenericHTMLContainerFormElement::IsContentOfType(PRUint32 aFlags)
{
  return !(aFlags & ~(eELEMENT | eHTML | eHTML_FORM_CONTROL));
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::SetForm(nsIDOMHTMLFormElement* aForm,
                                           PRBool aRemoveFromForm)
{
  nsAutoString nameVal, idVal;

  if (aForm || (mForm && aRemoveFromForm)) {
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, idVal);
  }

  if (mForm && aRemoveFromForm) {
    mForm->RemoveElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, idVal);
    }
  }

  if (aForm) {
    // keep a *weak* ref to the form here
    CallQueryInterface(aForm, &mForm);
    mForm->Release();
  } else {
    mForm = nsnull;
  }

  if (mForm) {
    mForm->AddElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->AddElementToTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->AddElementToTable(this, idVal);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  *aForm = nsnull;

  if (mForm) {
    CallQueryInterface(mForm, aForm);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::SetParent(nsIContent* aParent)
{
  nsresult rv = NS_OK;

  if (!aParent && mForm) {
    SetForm(nsnull);
  } else if (mDocument && aParent && (mParent || !mForm)) {
    // If we have a new parent and either we had an old parent or we
    // don't have a form, search for a containing form.  If we didn't
    // have an old parent, but we do have a form, we shouldn't do the
    // search. In this case, someone (possibly the content sink) has
    // already set the form for us.

    rv = FindAndSetForm(this);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = nsGenericElement::SetParent(aParent);
  }

  return rv;
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::SetDocument(nsIDocument* aDocument,
                                               PRBool aDeep,
                                               PRBool aCompileEventHandlers)
{
  nsresult rv = NS_OK;

  // Save state before doing anything if the document is being removed
  if (!aDocument) {
    SaveState();
  }

  if (aDocument && mParent && !mForm) {
    rv = FindAndSetForm(this);
  } else if (!aDocument && mForm) {
    // We got removed from document.  We have a parent form.  Check
    // that the form is still in the document, and if so remove
    // ourselves from the form.  This keeps ghosts from appearing in
    // the form's |elements| array
    nsCOMPtr<nsIContent> formContent(do_QueryInterface(mForm, &rv));
    if (formContent) {
      nsCOMPtr<nsIDocument> doc;
      rv = formContent->GetDocument(*getter_AddRefs(doc));
      NS_ENSURE_SUCCESS(rv, rv);
      if (doc) {
        SetForm(nsnull);
      }
    }
  }

  if (NS_SUCCEEDED(rv)) {
    rv = nsGenericHTMLElement::SetDocument(aDocument, aDeep,
                                           aCompileEventHandlers);
  }

  return rv;
}


nsresult
nsGenericHTMLElement::SetFormControlAttribute(nsIForm* aForm,
                                              PRInt32 aNameSpaceID,
                                              nsIAtom* aName,
                                              const nsAString& aValue,
                                              PRBool aNotify)
{
  nsCOMPtr<nsIFormControl> thisControl;
  nsAutoString tmp;
  nsresult rv = NS_OK;

  QueryInterface(NS_GET_IID(nsIFormControl), getter_AddRefs(thisControl));

  // Add & remove the control to and/or from the hash table
  if (aForm && (aName == nsHTMLAtoms::name || aName == nsHTMLAtoms::id)) {
    GetAttr(kNameSpaceID_None, aName, tmp);

    if (!tmp.IsEmpty()) {
      aForm->RemoveElementFromTable(thisControl, tmp);
    }
  }

  if (aForm && aName == nsHTMLAtoms::type) {
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, tmp);

    if (!tmp.IsEmpty()) {
      aForm->RemoveElementFromTable(thisControl, tmp);
    }

    GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, tmp);

    if (!tmp.IsEmpty()) {
      aForm->RemoveElementFromTable(thisControl, tmp);
    }

    aForm->RemoveElement(thisControl);
  }

  rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aValue, aNotify);

  if (aForm && (aName == nsHTMLAtoms::name || aName == nsHTMLAtoms::id)) {
    GetAttr(kNameSpaceID_None, aName, tmp);

    if (!tmp.IsEmpty()) {
      aForm->AddElementToTable(thisControl, tmp);
    }
  }

  if (aForm && aName == nsHTMLAtoms::type) {
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, tmp);

    if (!tmp.IsEmpty()) {
      aForm->AddElementToTable(thisControl, tmp);
    }

    GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, tmp);

    if (!tmp.IsEmpty()) {
      aForm->AddElementToTable(thisControl, tmp);
    }

    aForm->AddElement(thisControl);
  }

  return rv;
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::SetAttr(PRInt32 aNameSpaceID,
                                           nsIAtom* aName,
                                           const nsAString& aVal,
                                           PRBool aNotify)
{
  return SetFormControlAttribute(mForm, aNameSpaceID, aName, aVal, aNotify);
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::SetAttr(nsINodeInfo* aNodeInfo,
                                           const nsAString& aValue,
                                           PRBool aNotify)
{
  return nsGenericHTMLElement::SetAttr(aNodeInfo, aValue, aNotify);
}

//----------------------------------------------------------------------

nsGenericHTMLLeafFormElement::nsGenericHTMLLeafFormElement()
{
  mForm = nsnull;
}

nsGenericHTMLLeafFormElement::~nsGenericHTMLLeafFormElement()
{
  // Clean up.  Set the form to nsnull so it knows we went away.
  SetForm(nsnull);
}


NS_IMETHODIMP
nsGenericHTMLLeafFormElement::QueryInterface(REFNSIID aIID,
                                             void** aInstancePtr)
{
  if (NS_SUCCEEDED(nsGenericHTMLElement::QueryInterface(aIID, aInstancePtr)))
    return NS_OK;

  nsISupports *inst = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIFormControl))) {
    inst = NS_STATIC_CAST(nsIFormControl *, this);
  } else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsGenericHTMLLeafFormElement::IsContentOfType(PRUint32 aFlags)
{
  return !(aFlags & ~(eELEMENT | eHTML | eHTML_FORM_CONTROL));
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::SetForm(nsIDOMHTMLFormElement* aForm,
                                      PRBool aRemoveFromForm)
{
  nsAutoString nameVal, idVal;

  if (aForm || (mForm && aRemoveFromForm)) {
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, idVal);
  }

  if (mForm && aRemoveFromForm) {
    mForm->RemoveElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, idVal);
    }
  }

  if (aForm) {
    // keep a *weak* ref to the form here
    CallQueryInterface(aForm, &mForm);
    mForm->Release();
  } else {
    mForm = nsnull;
  }

  if (mForm) {
    mForm->AddElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->AddElementToTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->AddElementToTable(this, idVal);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  *aForm = nsnull;

  if (mForm) {
    CallQueryInterface(mForm, aForm);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::SetParent(nsIContent* aParent)
{
  nsresult rv = NS_OK;

  PRBool old_parent = NS_PTR_TO_INT32(mParent);

  if (NS_SUCCEEDED(rv)) {
    rv = nsGenericElement::SetParent(aParent);
  }

  if (!aParent && mForm) {
    SetForm(nsnull);
  }
  // If we have a new parent and either we had an old parent or we
  // don't have a form, search for a containing form.  If we didn't
  // have an old parent, but we do have a form, we shouldn't do the
  // search. In this case, someone (possibly the content sink) has
  // already set the form for us.
  else if (mDocument && aParent && (old_parent || !mForm)) {
    rv = FindAndSetForm(this);
  }

  return rv;
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::SetDocument(nsIDocument* aDocument,
                                          PRBool aDeep,
                                          PRBool aCompileEventHandlers)
{
  nsresult rv = NS_OK;

  // Save state before doing anything if the document is being removed
  if (!aDocument) {
    SaveState();
  }

  if (aDocument && mParent && !mForm) {
    rv = FindAndSetForm(this);
  } else if (!aDocument && mForm) {
    // We got removed from document.  We have a parent form.  Check
    // that the form is still in the document, and if so remove
    // ourselves from the form.  This keeps ghosts from appearing in
    // the form's |elements| array
    nsCOMPtr<nsIContent> formContent(do_QueryInterface(mForm, &rv));
    if (formContent) {
      nsCOMPtr<nsIDocument> doc;
      rv = formContent->GetDocument(*getter_AddRefs(doc));
      NS_ENSURE_SUCCESS(rv, rv);
      if (doc) {
        SetForm(nsnull);
      }
    }
  }

  if (NS_SUCCEEDED(rv)) {
    rv = nsGenericHTMLElement::SetDocument(aDocument, aDeep,
                                           aCompileEventHandlers);
  }

  return rv;
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::DoneCreatingElement()
{
  RestoreFormControlState(this, this);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::SetAttr(PRInt32 aNameSpaceID,
                                      nsIAtom* aName,
                                      const nsAString& aValue,
                                      PRBool aNotify)
{
  return SetFormControlAttribute(mForm, aNameSpaceID, aName, aValue, aNotify);
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::SetAttr(nsINodeInfo* aNodeInfo,
                                      const nsAString& aValue,
                                      PRBool aNotify)
{
  return nsGenericHTMLLeafElement::SetAttr(aNodeInfo, aValue, aNotify);
}

nsresult
nsGenericHTMLElement::SetElementFocus(PRBool aDoFocus)
{
  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(this, getter_AddRefs(presContext));
  if (!presContext) {
    return NS_OK;
  }

  if (aDoFocus) {
    nsresult rv = SetFocus(presContext);
    NS_ENSURE_SUCCESS(rv,rv);
    nsCOMPtr<nsIEventStateManager> esm;
    rv = presContext->GetEventStateManager(getter_AddRefs(esm));
    if (esm) {
      rv = esm->MoveCaretToFocus();
    }
    return rv;
  }

  return RemoveFocus(presContext);
}

nsresult nsGenericHTMLElement::RegUnRegAccessKey(PRBool aDoReg)
{
  // first check to see if we have an access key
  nsAutoString accessKey;
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::accesskey, accessKey);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_CONTENT_ATTR_NOT_THERE == rv || accessKey.IsEmpty()) {
    return NS_OK;
  }

  // We have an access key, so get the ESM from the pres context.
  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(this, getter_AddRefs(presContext));
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nsCOMPtr<nsIEventStateManager> esm;
  presContext->GetEventStateManager(getter_AddRefs(esm));
  NS_ENSURE_TRUE(esm, NS_ERROR_FAILURE);

  // Register or unregister as appropriate.
  if (aDoReg) {
    rv = esm->RegisterAccessKey(this, (PRUint32)accessKey.First());
  } else {
    rv = esm->UnregisterAccessKey(this, (PRUint32)accessKey.First());
  }
  return rv;
}

// static
nsresult
nsGenericHTMLElement::SetProtocolInHrefString(const nsAString &aHref,
                                              const nsAString &aProtocol,
                                              nsAString &aResult)
{
  aResult.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsAString::const_iterator start, end;
  aProtocol.BeginReading(start);
  aProtocol.EndReading(end);
  nsAString::const_iterator iter(start);
  FindCharInReadable(':', iter, end);
  uri->SetScheme(NS_ConvertUCS2toUTF8(Substring(start, iter)));
   
  nsCAutoString newHref;
  uri->GetSpec(newHref);

  aResult.Assign(NS_ConvertUTF8toUCS2(newHref));

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::SetHostnameInHrefString(const nsAString &aHref,
                                              const nsAString &aHostname,
                                              nsAString &aResult)
{
  aResult.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  uri->SetHost(NS_ConvertUCS2toUTF8(aHostname));

  nsCAutoString newHref;
  uri->GetSpec(newHref);

  aResult.Assign(NS_ConvertUTF8toUCS2(newHref));

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::SetPathnameInHrefString(const nsAString &aHref,
                                              const nsAString &aPathname,
                                              nsAString &aResult)
{
  aResult.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri, &rv));
  if (NS_FAILED(rv))
    return rv;

  url->SetFilePath(NS_ConvertUCS2toUTF8(aPathname));

  nsCAutoString newHref;
  uri->GetSpec(newHref);
  aResult.Assign(NS_ConvertUTF8toUCS2(newHref));

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::SetHostInHrefString(const nsAString &aHref,
                                          const nsAString &aHost,
                                          nsAString &aResult)
{
  // Can't simply call nsURI::SetHost, because that would treat the name as an
  // IPv6 address (like http://[server:443]/)

  aResult.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString scheme, userpass, path;
  uri->GetScheme(scheme);
  uri->GetUserPass(userpass);
  uri->GetPath(path);

  if (!userpass.IsEmpty())
    userpass.Append('@');

  aResult.Assign(NS_ConvertUTF8toUCS2(scheme) + NS_LITERAL_STRING("://") +
                 NS_ConvertUTF8toUCS2(userpass) + aHost +
                 NS_ConvertUTF8toUCS2(path));
  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::SetSearchInHrefString(const nsAString &aHref,
                                            const nsAString &aSearch,
                                            nsAString &aResult)
{
  aResult.Truncate();
  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri, &rv));
  if (NS_FAILED(rv))
    return rv;

  url->SetQuery(NS_ConvertUCS2toUTF8(aSearch));

  nsCAutoString newHref;
  uri->GetSpec(newHref);
  aResult.Assign(NS_ConvertUTF8toUCS2(newHref));

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::SetHashInHrefString(const nsAString &aHref,
                                          const nsAString &aHash,
                                          nsAString &aResult)
{
  aResult.Truncate();
  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri, &rv));
  if (NS_FAILED(rv))
    return rv;

  rv = url->SetRef(NS_ConvertUCS2toUTF8(aHash));

  nsCAutoString newHref;
  uri->GetSpec(newHref);
  aResult.Assign(NS_ConvertUTF8toUCS2(newHref));

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::SetPortInHrefString(const nsAString &aHref,
                                          const nsAString &aPort,
                                          nsAString &aResult)
{
  aResult.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);

  if (NS_FAILED(rv))
    return rv;

  PRInt32 port;
  port = nsString(aPort).ToInteger((PRInt32*)&rv);
  if (NS_FAILED(rv))
    return rv;

  uri->SetPort(port);

  nsCAutoString newHref;
  uri->GetSpec(newHref);
  aResult.Assign(NS_ConvertUTF8toUCS2(newHref));

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::GetProtocolFromHrefString(const nsAString& aHref,
                                                nsAString& aProtocol,
                                                nsIDocument *aDocument)
{
  aProtocol.Truncate();

  NS_ENSURE_TRUE(nsHTMLUtils::IOService, NS_ERROR_FAILURE);

  nsCAutoString protocol;

  nsresult rv =
    nsHTMLUtils::IOService->ExtractScheme(NS_ConvertUCS2toUTF8(aHref), protocol);

  if (NS_SUCCEEDED(rv)) {
    aProtocol.Assign(NS_ConvertASCIItoUCS2(protocol) + NS_LITERAL_STRING(":"));
  } else {
    // set the protocol to the protocol of the base URI.

    nsCOMPtr<nsIURI> uri;

    if (aDocument) {
      aDocument->GetBaseURL(*getter_AddRefs(uri));

      if (!uri) {
        aDocument->GetDocumentURL(getter_AddRefs(uri));
      }
    }

    if (uri) {
      uri->GetScheme(protocol);
    }

    if (protocol.IsEmpty()) {
      // set the protocol to http since it is the mostlikely protocol
      // to be used.

      CopyASCIItoUCS2(nsDependentCString("http:"), aProtocol);
    } else {
      CopyASCIItoUCS2(protocol + NS_LITERAL_CSTRING(":"), aProtocol);
    }
  }

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::GetHostFromHrefString(const nsAString& aHref,
                                            nsAString& aHost)
{
  aHost.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString hostport;
  rv = uri->GetHostPort(hostport);

  // Failure to get the hostport from the URI isn't necessarily an
  // error. Some URI's just don't have a hostport.

  if (NS_SUCCEEDED(rv)) {
    aHost.Assign(NS_ConvertUTF8toUCS2(hostport));
  }

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::GetHostnameFromHrefString(const nsAString& aHref,
                                                nsAString& aHostname)
{
  aHostname.Truncate();
  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString host;
  rv = url->GetHost(host);

  if (NS_SUCCEEDED(rv)) {
    // Failure to get the host from the URI isn't necessarily an
    // error. Some URI's just don't have a host.

    aHostname.Assign(NS_ConvertUTF8toUCS2(host));
  }

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::GetPathnameFromHrefString(const nsAString& aHref,
                                                nsAString& aPathname)
{
  aPathname.Truncate();
  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (!url) {
    // If this is not a URL, we can't get the pathname from the URI

    return NS_OK;
  }

  nsCAutoString file;
  rv = url->GetFilePath(file);
  if (NS_FAILED(rv))
    return rv;

  aPathname.Assign(NS_ConvertUTF8toUCS2(file));

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::GetSearchFromHrefString(const nsAString& aHref,
                                              nsAString& aSearch)
{
  aSearch.Truncate();
  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (!url) {
    // If this is not a URL, we can't get the query from the URI

    return NS_OK;
  }

  nsCAutoString search;
  rv = url->GetQuery(search);
  if (NS_FAILED(rv))
    return rv;

  if (!search.IsEmpty()) {
    aSearch.Assign(NS_LITERAL_STRING("?") + NS_ConvertUTF8toUCS2(search));
  }

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::GetPortFromHrefString(const nsAString& aHref,
                                            nsAString& aPort)
{
  aPort.Truncate();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 port;
  rv = uri->GetPort(&port);

  if (NS_SUCCEEDED(rv)) {
    // Failure to get the port from the URI isn't necessarily an
    // error. Some URI's just don't have a port.

    if (port == -1) {
      return NS_OK;
    }

    nsAutoString portStr;
    portStr.AppendInt(port, 10);
    aPort.Assign(portStr);
  }

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::GetHashFromHrefString(const nsAString& aHref,
                                            nsAString& aHash)
{
  aHash.Truncate();
  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (!url) {
    // If this is not a URL, we can't get the hash part from the URI

    return NS_OK;
  }

  nsCAutoString ref;
  rv = url->GetRef(ref);
  if (NS_FAILED(rv))
    return rv;
  NS_UnescapeURL(ref); // XXX may result in random non-ASCII bytes!

  if (!ref.IsEmpty())
    aHash.Assign(NS_LITERAL_STRING("#") + NS_ConvertASCIItoUCS2(ref));
  return NS_OK;
}
