/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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
#include "nscore.h"
#include "nsGenericHTMLElement.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIContentViewer.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleRule.h"
#include "nsCSSStruct.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIEventListenerManager.h"
#include "nsMappedAttributes.h"
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLDocument.h"
#include "nsILink.h"
#include "nsPIDOMWindow.h"
#include "nsIStyleRule.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollableView.h"
#include "nsIScrollableViewProvider.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsRange.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocShell.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
#include "nsScriptLoader.h"
#include "nsRuleData.h"

#include "nsPresState.h"
#include "nsILayoutHistoryState.h"

#include "nsHTMLParts.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsGkAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsDOMCSSDeclaration.h"
#include "nsICSSOMFactory.h"
#include "nsITextControlFrame.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"

#include "nsMutationEvent.h"

#include "nsContentCID.h"

#include "nsIDOMText.h"

#include "nsIEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsEventDispatcher.h"
#include "nsLayoutUtils.h"
#include "nsContentCreatorFunctions.h"
#include "mozAutoDocUpdate.h"
#include "nsIFocusController.h"

class nsINodeInfo;
class nsIDOMNodeList;
class nsRuleWalker;

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
  NS_ConvertUTF16toUTF8 name(name_chars);

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

  return nsGenericHTMLElementBase::Init(aNodeInfo);
}

#endif


class nsGenericHTMLElementTearoff : public nsIDOMNSHTMLElement,
                                    public nsIDOMElementCSSInlineStyle
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  nsGenericHTMLElementTearoff(nsGenericHTMLElement *aElement)
    : mElement(aElement)
  {
  }

  virtual ~nsGenericHTMLElementTearoff()
  {
  }

  NS_FORWARD_NSIDOMNSHTMLELEMENT(mElement->)
  NS_FORWARD_NSIDOMELEMENTCSSINLINESTYLE(mElement->)

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsGenericHTMLElementTearoff,
                                           nsIDOMNSHTMLElement)

private:
  nsCOMPtr<nsGenericHTMLElement> mElement;
};

NS_IMPL_CYCLE_COLLECTION_1(nsGenericHTMLElementTearoff, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsGenericHTMLElementTearoff,
                                          nsIDOMNSHTMLElement)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsGenericHTMLElementTearoff,
                                           nsIDOMNSHTMLElement)

NS_INTERFACE_TABLE_HEAD(nsGenericHTMLElementTearoff)
  NS_INTERFACE_TABLE_INHERITED2(nsGenericHTMLElementTearoff,
                                nsIDOMNSHTMLElement,
                                nsIDOMElementCSSInlineStyle)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsGenericHTMLElementTearoff)
NS_INTERFACE_MAP_END_AGGREGATED(mElement)


NS_IMPL_INT_ATTR(nsGenericHTMLElement, TabIndex, tabindex)

nsresult
nsGenericHTMLElement::DOMQueryInterface(nsIDOMHTMLElement *aElement,
                                        REFNSIID aIID, void **aInstancePtr)
{
  NS_PRECONDITION(aInstancePtr, "null out param");

  nsresult rv = NS_ERROR_FAILURE;

  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY(nsIDOMHTMLElement, nsIDOMNode)
    NS_INTERFACE_TABLE_ENTRY(nsIDOMHTMLElement, nsIDOMElement)
    NS_INTERFACE_TABLE_ENTRY(nsIDOMHTMLElement, nsIDOMHTMLElement)
  NS_INTERFACE_TABLE_END_WITH_PTR(aElement)

  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMNSHTMLElement,
                                 new nsGenericHTMLElementTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMElementCSSInlineStyle,
                                 new nsGenericHTMLElementTearoff(this))
  NS_INTERFACE_MAP_END

// No closing bracket, becuase NS_INTERFACE_MAP_END does that for us.
    
nsresult
nsGenericHTMLElement::CopyInnerTo(nsGenericElement* aDst) const
{
  nsresult rv;
  PRInt32 i, count = GetAttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);
    const nsAttrValue *value = mAttrsAndChildren.AttrAt(i);
    if (name->Equals(nsGkAtoms::style, kNameSpaceID_None) &&
        value->Type() == nsAttrValue::eCSSStyleRule) {
      // We can't just set this as a string, because that will fail
      // to reparse the string into style data until the node is
      // inserted into the document.  Clone the HTMLValue instead.
      nsCOMPtr<nsICSSRule> ruleClone;
      rv = value->GetCSSStyleRuleValue()->Clone(*getter_AddRefs(ruleClone));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsICSSStyleRule> styleRule = do_QueryInterface(ruleClone);
      NS_ENSURE_TRUE(styleRule, NS_ERROR_UNEXPECTED);

      rv = aDst->SetInlineStyleRule(styleRule, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      continue;
    }

    nsAutoString valStr;
    value->ToString(valStr);
    rv = aDst->SetAttr(name->NamespaceID(), name->LocalName(),
                       name->GetPrefix(), valStr, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Copy the baseuri and basetarget
  void* prop;
  if ((prop = GetProperty(nsGkAtoms::htmlBaseHref))) {
    rv = aDst->SetProperty(nsGkAtoms::htmlBaseHref, prop,
                           nsPropertyTable::SupportsDtorFunc, PR_TRUE);
    if (NS_SUCCEEDED(rv)) {
      NS_ADDREF(static_cast<nsIURI*>(prop));
    }
  }
  if ((prop = GetProperty(nsGkAtoms::htmlBaseTarget))) {
    rv = aDst->SetProperty(nsGkAtoms::htmlBaseTarget, prop,
                           nsPropertyTable::SupportsDtorFunc, PR_TRUE);
    if (NS_SUCCEEDED(rv)) {
      NS_ADDREF(static_cast<nsIAtom*>(prop));
    }
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetTagName(nsAString& aTagName)
{
  return GetNodeName(aTagName);
}

NS_IMETHODIMP
nsGenericHTMLElement::SetAttribute(const nsAString& aName,
                                   const nsAString& aValue)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

  if (!name) {
    nsresult rv = nsContentUtils::CheckQName(aName, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAtom> nameAtom;
    if (mNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
      nsAutoString lower;
      ToLowerCase(aName, lower);
      nameAtom = do_GetAtom(lower);
    }
    else {
      nameAtom = do_GetAtom(aName);
    }
    NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

    return SetAttr(kNameSpaceID_None, nameAtom, aValue, PR_TRUE);
  }

  return SetAttr(name->NamespaceID(), name->LocalName(), name->GetPrefix(),
                 aValue, PR_TRUE);
}

nsresult
nsGenericHTMLElement::GetNodeName(nsAString& aNodeName)
{
  mNodeInfo->GetQualifiedName(aNodeName);

  if (mNodeInfo->NamespaceEquals(kNameSpaceID_None))
    ToUpperCase(aNodeName);

  return NS_OK;
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

  return nsGenericHTMLElementBase::GetElementsByTagName(tagName, aReturn);
}

nsresult
nsGenericHTMLElement::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                             const nsAString& aLocalName,
                                             nsIDOMNodeList** aReturn)
{
  nsAutoString localName(aLocalName);

  // Only lowercase the name if this element has no namespace (i.e.
  // it's a HTML element, not an XHTML element).
  if (mNodeInfo && mNodeInfo->NamespaceEquals(kNameSpaceID_None))
    ToLowerCase(localName);

  return nsGenericHTMLElementBase::GetElementsByTagNameNS(aNamespaceURI,
                                                          localName,
                                                          aReturn);
}

// Implementation for nsIDOMHTMLElement
nsresult
nsGenericHTMLElement::GetId(nsAString& aId)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::id, aId);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetId(const nsAString& aId)
{
  SetAttr(kNameSpaceID_None, nsGkAtoms::id, aId, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetTitle(nsAString& aTitle)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, aTitle);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetTitle(const nsAString& aTitle)
{
  SetAttr(kNameSpaceID_None, nsGkAtoms::title, aTitle, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetLang(nsAString& aLang)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::lang, aLang);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetLang(const nsAString& aLang)
{
  SetAttr(kNameSpaceID_None, nsGkAtoms::lang, aLang, PR_TRUE);
  return NS_OK;
}

static const nsAttrValue::EnumTable kDirTable[] = {
  { "ltr", NS_STYLE_DIRECTION_LTR },
  { "rtl", NS_STYLE_DIRECTION_RTL },
  { 0 }
};

nsresult
nsGenericHTMLElement::GetDir(nsAString& aDir)
{
  const nsAttrValue* attr = mAttrsAndChildren.GetAttr(nsGkAtoms::dir);

  if (attr && attr->Type() == nsAttrValue::eEnum) {
    attr->ToString(aDir);
  }
  else {
    aDir.Truncate();
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetDir(const nsAString& aDir)
{
  SetAttr(kNameSpaceID_None, nsGkAtoms::dir, aDir, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetClassName(nsAString& aClassName)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::_class, aClassName);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetClassName(const nsAString& aClassName)
{
  SetAttr(kNameSpaceID_None, nsGkAtoms::_class, aClassName, PR_TRUE);
  return NS_OK;
}

static PRBool
IsBody(nsIContent *aContent)
{
  return (aContent->NodeInfo()->Equals(nsGkAtoms::body) &&
          aContent->IsNodeOfType(nsINode::eHTML));
}

static PRBool IS_TABLE_CELL(nsIAtom* frameType) {
  return nsGkAtoms::tableCellFrame == frameType ||
    nsGkAtoms::bcTableCellFrame == frameType;
}

static PRBool
IsOffsetParent(nsIFrame* aFrame)
{
  nsIAtom* frameType = aFrame->GetType();
  return (IS_TABLE_CELL(frameType) ||
          frameType == nsGkAtoms::tableFrame);
}

void
nsGenericHTMLElement::GetOffsetRect(nsRect& aRect, nsIContent** aOffsetParent)
{
  *aOffsetParent = nsnull;
  aRect = nsRect();

  nsIFrame* frame = GetStyledFrame();
  if (!frame) {
    return;
  }

  nsIFrame* parent = frame->GetParent();
  nsPoint origin(0, 0);

  if (parent && parent->GetType() == nsGkAtoms::tableOuterFrame) {
    origin = parent->GetPositionIgnoringScrolling();
    parent = parent->GetParent();
  }

  nsIContent* docElement = GetCurrentDoc()->GetRootContent();
  nsIContent* content = frame->GetContent();

  if (content && (IsBody(content) || content == docElement)) {
    parent = frame;
  }
  else {
    const PRBool isPositioned = frame->GetStyleDisplay()->IsPositioned();
    const PRBool isAbsolutelyPositioned =
      frame->GetStyleDisplay()->IsAbsolutelyPositioned();
    origin += frame->GetPositionIgnoringScrolling();

    for ( ; parent ; parent = parent->GetParent()) {
      content = parent->GetContent();

      // Stop at the first ancestor that is positioned.
      if (parent->GetStyleDisplay()->IsPositioned()) {
        *aOffsetParent = content;
        NS_IF_ADDREF(*aOffsetParent);
        break;
      }

      // Add the parent's origin to our own to get to the
      // right coordinate system.
      const PRBool isOffsetParent = !isPositioned && IsOffsetParent(parent);
      if (!isAbsolutelyPositioned && !isOffsetParent) {
        origin += parent->GetPositionIgnoringScrolling();
      }

      if (content) {
        // If we've hit the document element, break here.
        if (content == docElement) {
          break;
        }

        // Break if the ancestor frame type makes it suitable as offset parent
        // and this element is *not* positioned or if we found the body element.
        if (isOffsetParent || IsBody(content)) {
          *aOffsetParent = content;
          NS_ADDREF(*aOffsetParent);
          break;
        }
      }
    }

    if (isAbsolutelyPositioned && !*aOffsetParent) {
      // If this element is absolutely positioned, but we don't have
      // an offset parent it means this element is an absolutely
      // positioned child that's not nested inside another positioned
      // element, in this case the element's frame's parent is the
      // frame for the HTML element so we fail to find the body in the
      // parent chain. We want the offset parent in this case to be
      // the body, so we just get the body element from the document.

      nsCOMPtr<nsIDOMHTMLDocument> html_doc(do_QueryInterface(GetCurrentDoc()));

      if (html_doc) {
        nsCOMPtr<nsIDOMHTMLElement> html_element;
        html_doc->GetBody(getter_AddRefs(html_element));
        if (html_element) {
          CallQueryInterface(html_element, aOffsetParent);
        }
      }
    }
  }

  // Subtract the parent border unless it uses border-box sizing.
  if (parent &&
      parent->GetStylePosition()->mBoxSizing != NS_STYLE_BOX_SIZING_BORDER) {
    const nsStyleBorder* border = parent->GetStyleBorder();
    origin.x -= border->GetActualBorderWidth(NS_SIDE_LEFT);
    origin.y -= border->GetActualBorderWidth(NS_SIDE_TOP);
  }

  // XXX We should really consider subtracting out padding for
  // content-box sizing, but we should see what IE does....

  // Convert to pixels.
  aRect.x = nsPresContext::AppUnitsToIntCSSPixels(origin.x);
  aRect.y = nsPresContext::AppUnitsToIntCSSPixels(origin.y);

  // Get the union of all rectangles in this and continuation frames.
  // It doesn't really matter what we use as aRelativeTo here, since
  // we only care about the size. We just have to use something non-null.
  nsRect rcFrame = nsLayoutUtils::GetAllInFlowRectsUnion(frame, frame);
  aRect.width = nsPresContext::AppUnitsToIntCSSPixels(rcFrame.width);
  aRect.height = nsPresContext::AppUnitsToIntCSSPixels(rcFrame.height);
}

nsresult
nsGenericHTMLElement::GetOffsetTop(PRInt32* aOffsetTop)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  GetOffsetRect(rcFrame, getter_AddRefs(parent));

  *aOffsetTop = rcFrame.y;

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetOffsetLeft(PRInt32* aOffsetLeft)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  GetOffsetRect(rcFrame, getter_AddRefs(parent));

  *aOffsetLeft = rcFrame.x;

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetOffsetWidth(PRInt32* aOffsetWidth)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  GetOffsetRect(rcFrame, getter_AddRefs(parent));

  *aOffsetWidth = rcFrame.width;

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetOffsetHeight(PRInt32* aOffsetHeight)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  GetOffsetRect(rcFrame, getter_AddRefs(parent));

  *aOffsetHeight = rcFrame.height;

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetOffsetParent(nsIDOMElement** aOffsetParent)
{
  nsRect rcFrame;
  nsCOMPtr<nsIContent> parent;
  GetOffsetRect(rcFrame, getter_AddRefs(parent));

  if (parent) {
    CallQueryInterface(parent, aOffsetParent);
  } else {
    *aOffsetParent = nsnull;
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetInnerHTML(nsAString& aInnerHTML)
{
  aInnerHTML.Truncate();

  nsCOMPtr<nsIDocument> doc = GetOwnerDoc();
  if (!doc) {
    return NS_OK; // We rely on the document for doing HTML conversion
  }

  nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(static_cast<nsIContent *>
                                                             (this)));
  nsresult rv = NS_OK;

  nsAutoString contentType;
  if (!doc->IsCaseSensitive()) {
    // All case-insensitive documents are HTML as far as we're concerned
    contentType.AssignLiteral("text/html");
  } else {
    doc->GetContentType(contentType);
  }
  
  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  docEncoder =
    do_CreateInstance(PromiseFlatCString(
        nsDependentCString(NS_DOC_ENCODER_CONTRACTID_BASE) +
        NS_ConvertUTF16toUTF8(contentType)
      ).get());
  if (!docEncoder && doc->IsCaseSensitive()) {
    // This could be some type for which we create a synthetic document.  Try
    // again as XML
    contentType.AssignLiteral("application/xml");
    docEncoder = do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "application/xml");
  }

  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
  rv = docEncoder->Init(domDoc, contentType,
                        nsIDocumentEncoder::OutputEncodeBasicEntities |
                        // Output DOM-standard newlines
                        nsIDocumentEncoder::OutputLFLineBreak |
                        // Don't do linebreaking that's not present in the source
                        nsIDocumentEncoder::OutputRaw);
  NS_ENSURE_SUCCESS(rv, rv);

  docEncoder->SetContainerNode(thisNode);
  return docEncoder->EncodeToString(aInnerHTML);
}

nsresult
nsGenericHTMLElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  // This BeginUpdate/EndUpdate pair is important to make us reenable the
  // scriptloader before the last EndUpdate call.
  mozAutoDocUpdate updateBatch(GetCurrentDoc(), UPDATE_CONTENT_MODEL, PR_TRUE);

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(GetOwnerDoc(), nsnull);

  // Remove childnodes
  nsContentUtils::SetNodeTextContent(this, EmptyString(), PR_FALSE);

  nsCOMPtr<nsIDOMDocumentFragment> df;

  nsCOMPtr<nsIDocument> doc = GetOwnerDoc();

  // Strong ref since appendChild can fire events
  nsRefPtr<nsScriptLoader> loader;
  PRBool scripts_enabled = PR_FALSE;

  if (doc) {
    loader = doc->ScriptLoader();
    scripts_enabled = loader->GetEnabled();
    loader->SetEnabled(PR_FALSE);
  }

  nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(static_cast<nsIContent *>
                                                             (this)));
  nsresult rv = nsContentUtils::CreateContextualFragment(thisNode, aInnerHTML,
                                                         PR_FALSE,
                                                         getter_AddRefs(df));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMNode> tmpNode;
    rv = thisNode->AppendChild(df, getter_AddRefs(tmpNode));
  }

  if (scripts_enabled) {
    // If we disabled scripts, re-enable them now that we're
    // done. Don't fire JS timeouts when enabling the context here.

    loader->SetEnabled(PR_TRUE);
  }

  return rv;
}

nsresult
nsGenericHTMLElement::ScrollIntoView(PRBool aTop)
{
  nsIDocument *document = GetCurrentDoc();

  if (!document) {
    return NS_OK;
  }

  // Get the presentation shell
  nsCOMPtr<nsIPresShell> presShell = document->GetPrimaryShell();
  if (!presShell) {
    return NS_OK;
  }

  PRIntn vpercent = aTop ? NS_PRESSHELL_SCROLL_TOP :
    NS_PRESSHELL_SCROLL_BOTTOM;

  presShell->ScrollContentIntoView(this, vpercent,
                                   NS_PRESSHELL_SCROLL_ANYWHERE);

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLElement::GetSpellcheck(PRBool* aSpellcheck)
{
  NS_ENSURE_ARG_POINTER(aSpellcheck);
  *aSpellcheck = PR_FALSE;              // Default answer is to not spellcheck

  // Has the state has been explicitly set?
  nsINode* node;
  for (node = this; node; node = node->GetNodeParent()) {
    if (node->IsNodeOfType(nsINode::eHTML)) {
      static nsIContent::AttrValuesArray strings[] =
        {&nsGkAtoms::_true, &nsGkAtoms::_false, nsnull};
      switch (static_cast<nsIContent*>(node)->
              FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::spellcheck,
                              strings, eCaseMatters)) {
        case 0:                         // spellcheck = "true"
          *aSpellcheck = PR_TRUE;
          // Fall through
        case 1:                         // spellcheck = "false"
          return NS_OK;
      }
    }
  }

  // Is this a chrome element?
  if (nsContentUtils::IsChromeDoc(GetOwnerDoc())) {
    return NS_OK;                       // Not spellchecked by default
  }

  if (IsCurrentBodyElement()) {
    nsCOMPtr<nsIHTMLDocument> doc = do_QueryInterface(GetCurrentDoc());
    if (doc) {
      *aSpellcheck = doc->IsEditingOn();
    }

    return NS_OK;
  }

  // Is this element editable?
  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(this);
  if (!formControl) {
    return NS_OK;                       // Not spellchecked by default
  }

  // Is this a multiline plaintext input?
  PRInt32 controlType = formControl->GetType();
  if (controlType == NS_FORM_TEXTAREA) {
    *aSpellcheck = PR_TRUE;             // Spellchecked by default
    return NS_OK;
  }

  // Is this anything other than a single-line plaintext input?
  if (controlType != NS_FORM_INPUT_TEXT) {
    return NS_OK;                       // Not spellchecked by default
  }

  // Does the user want single-line inputs spellchecked by default?
  // NOTE: Do not reflect a pref value of 0 back to the DOM getter.
  // The web page should not know if the user has disabled spellchecking.
  // We'll catch this in the editor itself.
  PRInt32 spellcheckLevel =
    nsContentUtils::GetIntPref("layout.spellcheckDefault", 1);
  if (spellcheckLevel == 2) {           // "Spellcheck multi- and single-line"
    *aSpellcheck = PR_TRUE;             // Spellchecked by default
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLElement::SetSpellcheck(PRBool aSpellcheck)
{
  if (aSpellcheck) {
    return SetAttrHelper(nsGkAtoms::spellcheck, NS_LITERAL_STRING("true"));
  }

  return SetAttrHelper(nsGkAtoms::spellcheck, NS_LITERAL_STRING("false"));
}

NS_IMETHODIMP
nsGenericHTMLElement::GetDraggable(PRBool* aDraggable)
{
  *aDraggable = AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                             nsGkAtoms::_true, eIgnoreCase);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLElement::SetDraggable(PRBool aDraggable)
{
  return SetAttrHelper(nsGkAtoms::draggable,
                       aDraggable ? NS_LITERAL_STRING("true") :
                                    NS_LITERAL_STRING("false"));
}

PRBool
nsGenericHTMLElement::InNavQuirksMode(nsIDocument* aDoc)
{
  return aDoc && aDoc->GetCompatibilityMode() == eCompatibility_NavQuirks;
}

void
nsGenericHTMLElement::UpdateEditableState()
{
  // XXX Should we do this only when in a document?
  ContentEditableTristate value = GetContentEditableValue();
  if (value != eInherit) {
    SetEditableFlag(!!value);

    return;
  }

  nsGenericElement::UpdateEditableState();
}

nsresult
nsGenericHTMLElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                 nsIContent* aBindingParent,
                                 PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElementBase::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    if (HasFlag(NODE_IS_EDITABLE) && GetContentEditableValue() == eTrue) {
      nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(aDocument);
      if (htmlDocument) {
        htmlDocument->ChangeContentEditableCount(this, +1);
      }
    }
  }

  return rv;
}

void
nsGenericHTMLElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  if (GetContentEditableValue() == eTrue) {
    nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(GetCurrentDoc());
    if (htmlDocument) {
      htmlDocument->ChangeContentEditableCount(this, -1);
    }
  }

  nsGenericElement::UnbindFromTree(aDeep, aNullParent);
}

already_AddRefed<nsIDOMHTMLFormElement>
nsGenericHTMLElement::FindForm(nsIForm* aCurrentForm)
{
  // Make sure we don't end up finding a form that's anonymous from
  // our point of view.
  nsIContent* bindingParent = GetBindingParent();

  nsIContent* content = this;
  while (content != bindingParent && content) {
    // If the current ancestor is a form, return it as our form
    if (content->Tag() == nsGkAtoms::form &&
        content->IsNodeOfType(nsINode::eHTML)) {
#ifdef DEBUG
      if (!nsContentUtils::IsInSameAnonymousTree(this, content)) {
        // It's possible that we started unbinding at |content| or
        // some ancestor of it, and |content| and |this| used to all be
        // anonymous.  Check for this the hard way.
        for (nsIContent* child = this; child != content;
             child = child->GetParent()) {
          NS_ASSERTION(child->GetParent()->IndexOf(child) != -1,
                       "Walked too far?");
        }
      }
#endif
      nsIDOMHTMLFormElement* form;
      CallQueryInterface(content, &form);

      return form;
    }

    nsIContent *prevContent = content;
    content = prevContent->GetParent();

    if (!content && aCurrentForm) {
      // We got to the root of the subtree we're in, and we're being removed
      // from the DOM (the only time we get into this method with a non-null
      // aCurrentForm).  Check whether aCurrentForm is in the same subtree.  If
      // it is, we want to return aCurrentForm, since this case means that
      // we're one of those inputs-in-a-table that have a hacked mForm pointer
      // and a subtree containing both us and the form got removed from the
      // DOM.
      nsCOMPtr<nsIContent> formCOMPtr = do_QueryInterface(aCurrentForm);
      NS_ASSERTION(formCOMPtr, "aCurrentForm isn't an nsIContent?");
      // Use an nsIContent temporary to reduce addref/releasing as we go up the
      // tree
      nsINode* iter = formCOMPtr;
      do {
        iter = iter->GetNodeParent();
        if (iter == prevContent) {
          nsIDOMHTMLFormElement* form;
          CallQueryInterface(aCurrentForm, &form);
          return form;
        }
      } while (iter);
    }
  }

  return nsnull;
}

static PRBool
IsArea(nsIContent *aContent)
{
  return (aContent->Tag() == nsGkAtoms::area &&
          aContent->IsNodeOfType(nsINode::eHTML));
}

PRBool
nsGenericHTMLElement::CheckHandleEventForAnchorsPreconditions(nsEventChainVisitor& aVisitor)
{
  NS_PRECONDITION(nsCOMPtr<nsILink>(do_QueryInterface(this)),
                  "should be called only when |this| implements |nsILink|");

  if (!aVisitor.mPresContext) {
    // We need a pres context to do link stuff. Some events (e.g. mutation
    // events) don't have one.
    // XXX: ideally, shouldn't we be able to do what we need without one?
    return PR_FALSE; 
  }

  //Need to check if we hit an imagemap area and if so see if we're handling
  //the event on that map or on a link farther up the tree.  If we're on a
  //link farther up, do nothing.
  nsCOMPtr<nsIContent> target;
  aVisitor.mPresContext->EventStateManager()->
    GetEventTargetContent(aVisitor.mEvent, getter_AddRefs(target));

  return !target || !IsArea(target) || IsArea(this);
}

nsresult
nsGenericHTMLElement::PreHandleEventForAnchors(nsEventChainPreVisitor& aVisitor)
{
  nsresult rv = nsGenericHTMLElementBase::PreHandleEvent(aVisitor);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!CheckHandleEventForAnchorsPreconditions(aVisitor)) {
    return NS_OK;
  }

  return PreHandleEventForLinks(aVisitor);
}

nsresult
nsGenericHTMLElement::PostHandleEventForAnchors(nsEventChainPostVisitor& aVisitor)
{
  if (!CheckHandleEventForAnchorsPreconditions(aVisitor)) {
    return NS_OK;
  }

  return PostHandleEventForLinks(aVisitor);
}

PRBool
nsGenericHTMLElement::IsHTMLLink(nsIURI** aURI) const
{
  NS_PRECONDITION(aURI, "Must provide aURI out param");

  GetHrefURIForAnchors(aURI);
  // We promise out param is non-null if we return true, so base rv on it
  return *aURI != nsnull;
}

nsresult
nsGenericHTMLElement::GetHrefURIForAnchors(nsIURI** aURI) const
{
  // This is used by the three nsILink implementations and
  // nsHTMLStyleElement.

  // Get href= attribute (relative URI).

  // We use the nsAttrValue's copy of the URI string to avoid copying.
  const nsAttrValue* attr = mAttrsAndChildren.GetAttr(nsGkAtoms::href);
  if (attr) {
    // Get base URI.
    nsCOMPtr<nsIURI> baseURI = GetBaseURI();

    // Get absolute URI.
    nsresult rv = nsContentUtils::NewURIWithDocumentCharset(aURI,
                                                            attr->GetStringValue(),
                                                            GetOwnerDoc(),
                                                            baseURI);
    if (NS_FAILED(rv)) {
      *aURI = nsnull;
    }
  }
  else {
    // Absolute URI is null to say we have no HREF.
    *aURI = nsnull;
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                   const nsAString* aValue, PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (nsContentUtils::IsEventAttributeName(aName, EventNameType_HTML) && aValue) {
      nsresult rv = AddScriptEventListener(aName, *aValue);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aNotify && aName == nsGkAtoms::spellcheck) {
      SyncEditorsOnSubtree(this);
    }
  }

  return nsGenericHTMLElementBase::AfterSetAttr(aNamespaceID, aName,
                                                aValue, aNotify);
}

nsresult
nsGenericHTMLElement::GetEventListenerManagerForAttr(nsIEventListenerManager** aManager,
                                                     nsISupports** aTarget,
                                                     PRBool* aDefer)
{
  // Attributes on the body and frameset tags get set on the global object
  if (mNodeInfo->Equals(nsGkAtoms::body) ||
      mNodeInfo->Equals(nsGkAtoms::frameset)) {
    nsPIDOMWindow *win;

    // If we have a document, and it has a window, add the event
    // listener on the window (the inner window). If not, proceed as
    // normal.
    // XXXbz sXBL/XBL2 issue: should we instead use GetCurrentDoc() here,
    // override BindToTree for those classes and munge event listeners there?
    nsIDocument *document = GetOwnerDoc();
    nsresult rv = NS_OK;

    // FIXME (https://bugzilla.mozilla.org/show_bug.cgi?id=431767)
    // nsDocument::GetInnerWindow can return an outer window in some cases,
    // we don't want to stick an event listener on an outer window, so
    // bail if it does.
    if (document &&
        (win = document->GetInnerWindow()) && win->IsInnerWindow()) {
      nsCOMPtr<nsPIDOMEventTarget> piTarget(do_QueryInterface(win));
      NS_ENSURE_TRUE(piTarget, NS_ERROR_FAILURE);

      rv = piTarget->GetListenerManager(PR_TRUE, aManager);

      if (NS_SUCCEEDED(rv)) {
        NS_ADDREF(*aTarget = win);
      }
      *aDefer = PR_FALSE;
    } else {
      *aManager = nsnull;
      *aTarget = nsnull;
      *aDefer = PR_FALSE;
    }

    return rv;
  }

  return nsGenericHTMLElementBase::GetEventListenerManagerForAttr(aManager,
                                                                  aTarget,
                                                                  aDefer);
}

nsresult
nsGenericHTMLElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                              nsIAtom* aPrefix, const nsAString& aValue,
                              PRBool aNotify)
{
  PRBool contentEditable = aNameSpaceID == kNameSpaceID_None &&
                           aName == nsGkAtoms::contenteditable;
  PRInt32 change;
  if (contentEditable) {
    change = GetContentEditableValue() == eTrue ? -1 : 0;
  }

  nsresult rv = nsGenericElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                          aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (contentEditable) {
    if (aValue.IsEmpty() || aValue.LowerCaseEqualsLiteral("true")) {
      change += 1;
    }

    ChangeEditableState(change);
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                PRBool aNotify)
{
  PRBool contentEditable = PR_FALSE;
  PRInt32 contentEditableChange;

  if (aNameSpaceID == kNameSpaceID_None) {
    contentEditable = PR_TRUE;
    contentEditableChange = GetContentEditableValue() == eTrue ? -1 : 0;
  }

  // Check for event handlers
  if (aNameSpaceID == kNameSpaceID_None) {
    if (nsContentUtils::IsEventAttributeName(aAttribute, EventNameType_HTML)) {
      nsCOMPtr<nsIEventListenerManager> manager;
      GetListenerManager(PR_FALSE, getter_AddRefs(manager));

      if (manager) {
        manager->RemoveScriptEventListener(aAttribute);
      }
    }
  }

  nsresult rv = nsGenericHTMLElementBase::UnsetAttr(aNameSpaceID, aAttribute,
                                                    aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (contentEditable) {
    ChangeEditableState(contentEditableChange);
  }

  return NS_OK;
}

already_AddRefed<nsIURI>
nsGenericHTMLElement::GetBaseURI() const
{
  nsIDocument* doc = GetOwnerDoc();

  void* prop;
  if (HasFlag(NODE_HAS_PROPERTIES) && (prop = GetProperty(nsGkAtoms::htmlBaseHref))) {
    nsIURI* uri = static_cast<nsIURI*>(prop);
    NS_ADDREF(uri);
    
    return uri;
  }

  // If we are a plain old HTML element (not XHTML), don't bother asking the
  // base class -- our base URI is determined solely by the document base.
  if (mNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
    if (doc) {
      nsIURI *uri = doc->GetBaseURI();
      NS_IF_ADDREF(uri);

      return uri;
    }

    return nsnull;
  }

  return nsGenericHTMLElementBase::GetBaseURI();
}

void
nsGenericHTMLElement::GetBaseTarget(nsAString& aBaseTarget) const
{
  void* prop;
  if (HasFlag(NODE_HAS_PROPERTIES) && (prop = GetProperty(nsGkAtoms::htmlBaseTarget))) {
    static_cast<nsIAtom*>(prop)->ToString(aBaseTarget);
    
    return;
  }

  nsIDocument* ownerDoc = GetOwnerDoc();
  if (ownerDoc) {
    ownerDoc->GetBaseTarget(aBaseTarget);
  } else {
    aBaseTarget.Truncate();
  }
}

PRBool
nsGenericHTMLElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eELEMENT | eHTML));
}

//----------------------------------------------------------------------


PRBool
nsGenericHTMLElement::ParseAttribute(PRInt32 aNamespaceID,
                                     nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::dir) {
      return aResult.ParseEnumValue(aValue, kDirTable);
    }
  
    if (aAttribute == nsGkAtoms::tabindex) {
      return aResult.ParseIntWithBounds(aValue, -32768, 32767);
    }

    if (aAttribute == nsGkAtoms::name && !aValue.IsEmpty()) {
      // Store name as an atom.  name="" means that the element has no name,
      // not that it has an emptystring as the name.
      aResult.ParseAtom(aValue);
      return PR_TRUE;
    }

    if (aAttribute == nsGkAtoms::contenteditable) {
      aResult.ParseAtom(aValue);
      return PR_TRUE;
    }
  }

  return nsGenericHTMLElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
}

PRBool
nsGenericHTMLElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap
  };
  
  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

nsMapRuleToAttributesFunc
nsGenericHTMLElement::GetAttributeMappingFunction() const
{
  return &MapCommonAttributesInto;
}

// static
nsIFormControlFrame*
nsGenericHTMLElement::GetFormControlFrameFor(nsIContent* aContent,
                                             nsIDocument* aDocument,
                                             PRBool aFlushContent)
{
  if (aFlushContent) {
    // Cause a flush of content, so we get up-to-date frame
    // information
    aDocument->FlushPendingNotifications(Flush_Layout);
  }
  nsIFrame* frame = GetPrimaryFrameFor(aContent, aDocument);
  if (frame) {
    nsIFormControlFrame* form_frame = do_QueryFrame(frame);
    if (form_frame) {
      return form_frame;
    }

    // If we have generated content, the primary frame will be a
    // wrapper frame..  out real frame will be in its child list.
    for (frame = frame->GetFirstChild(nsnull);
         frame;
         frame = frame->GetNextSibling()) {
      form_frame = do_QueryFrame(frame);
      if (form_frame) {
        return form_frame;
      }
    }
  }

  return nsnull;
}

/* static */ nsresult
nsGenericHTMLElement::GetPrimaryPresState(nsGenericHTMLElement* aContent,
                                          nsPresState** aPresState)
{
  NS_ENSURE_ARG_POINTER(aPresState);
  *aPresState = nsnull;

  nsresult result = NS_OK;

  nsCOMPtr<nsILayoutHistoryState> history;
  nsCAutoString key;
  GetLayoutHistoryAndKey(aContent, PR_FALSE, getter_AddRefs(history), key);

  if (history) {
    // Get the pres state for this key, if it doesn't exist, create one
    result = history->GetState(key, aPresState);
    if (!*aPresState) {
      *aPresState = new nsPresState();
      if (!*aPresState) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
        
      result = history->AddState(key, *aPresState);
    }
  }

  return result;
}


nsresult
nsGenericHTMLElement::GetLayoutHistoryAndKey(nsGenericHTMLElement* aContent,
                                             PRBool aRead,
                                             nsILayoutHistoryState** aHistory,
                                             nsACString& aKey)
{
  //
  // Get the pres shell
  //
  nsCOMPtr<nsIDocument> doc = aContent->GetDocument();
  if (!doc) {
    return NS_OK;
  }

  //
  // Get the history (don't bother with the key if the history is not there)
  //
  *aHistory = doc->GetLayoutHistoryState().get();
  if (!*aHistory) {
    return NS_OK;
  }

  if (aRead && !(*aHistory)->HasStates()) {
    NS_RELEASE(*aHistory);
    return NS_OK;
  }

  //
  // Get the state key
  //
  nsresult rv = nsContentUtils::GenerateStateKey(aContent, doc,
                                                 nsIStatefulFrame::eNoID,
                                                 aKey);
  if (NS_FAILED(rv)) {
    NS_RELEASE(*aHistory);
    return rv;
  }

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
nsGenericHTMLElement::RestoreFormControlState(nsGenericHTMLElement* aContent,
                                              nsIFormControl* aControl)
{
  nsCOMPtr<nsILayoutHistoryState> history;
  nsCAutoString key;
  nsresult rv = GetLayoutHistoryAndKey(aContent, PR_TRUE,
                                       getter_AddRefs(history), key);
  if (!history) {
    return PR_FALSE;
  }

  nsPresState *state;
  // Get the pres state for this key
  rv = history->GetState(key, &state);
  if (state) {
    PRBool result = aControl->RestoreState(state);
    history->RemoveState(key);
    return result;
  }

  return PR_FALSE;
}

// XXX This creates a dependency between content and frames
nsPresContext*
nsGenericHTMLElement::GetPresContext()
{
  // Get the document
  nsIDocument* doc = GetDocument();
  if (doc) {
    // Get presentation shell 0
    nsIPresShell *presShell = doc->GetPrimaryShell();
    if (presShell) {
      return presShell->GetPresContext();
    }
  }

  return nsnull;
}

static const nsAttrValue::EnumTable kAlignTable[] = {
  { "left",      NS_STYLE_TEXT_ALIGN_LEFT },
  { "right",     NS_STYLE_TEXT_ALIGN_RIGHT },

  { "top",       NS_STYLE_VERTICAL_ALIGN_TOP },
  { "middle",    NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE },
  { "bottom",    NS_STYLE_VERTICAL_ALIGN_BASELINE },

  { "center",    NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE },
  { "baseline",  NS_STYLE_VERTICAL_ALIGN_BASELINE },

  { "texttop",   NS_STYLE_VERTICAL_ALIGN_TEXT_TOP },
  { "absmiddle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "abscenter", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "absbottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { 0 }
};

static const nsAttrValue::EnumTable kDivAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_MOZ_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "justify", NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

static const nsAttrValue::EnumTable kFrameborderTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "1", NS_STYLE_FRAME_1 },
  { "0", NS_STYLE_FRAME_0 },
  { 0 }
};

static const nsAttrValue::EnumTable kScrollingTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "on", NS_STYLE_FRAME_ON },
  { "off", NS_STYLE_FRAME_OFF },
  { "scroll", NS_STYLE_FRAME_SCROLL },
  { "noscroll", NS_STYLE_FRAME_NOSCROLL },
  { "auto", NS_STYLE_FRAME_AUTO },
  { 0 }
};

static const nsAttrValue::EnumTable kTableVAlignTable[] = {
  { "top",     NS_STYLE_VERTICAL_ALIGN_TOP },
  { "middle",  NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "bottom",  NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "baseline",NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { 0 }
};

PRBool
nsGenericHTMLElement::ParseAlignValue(const nsAString& aString,
                                      nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kAlignTable);
}

//----------------------------------------

// Vanilla table as defined by the html4 spec...
static const nsAttrValue::EnumTable kTableHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

// This table is used for TABLE when in compatability mode
static const nsAttrValue::EnumTable kCompatTableHAlignTable[] = {
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
                                            nsAttrValue& aResult) const
{
  if (InNavQuirksMode(GetOwnerDoc())) {
    return aResult.ParseEnumValue(aString, kCompatTableHAlignTable);
  }
  return aResult.ParseEnumValue(aString, kTableHAlignTable);
}

//----------------------------------------

// These tables are used for TD,TH,TR, etc (but not TABLE)
static const nsAttrValue::EnumTable kTableCellHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_MOZ_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

static const nsAttrValue::EnumTable kCompatTableCellHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_MOZ_LEFT },
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
                                                nsAttrValue& aResult) const
{
  if (InNavQuirksMode(GetOwnerDoc())) {
    return aResult.ParseEnumValue(aString, kCompatTableCellHAlignTable);
  }
  return aResult.ParseEnumValue(aString, kTableCellHAlignTable);
}

//----------------------------------------

PRBool
nsGenericHTMLElement::ParseTableVAlignValue(const nsAString& aString,
                                            nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kTableVAlignTable);
}

PRBool
nsGenericHTMLElement::ParseDivAlignValue(const nsAString& aString,
                                         nsAttrValue& aResult) const
{
  return aResult.ParseEnumValue(aString, kDivAlignTable);
}

PRBool
nsGenericHTMLElement::ParseImageAttribute(nsIAtom* aAttribute,
                                          const nsAString& aString,
                                          nsAttrValue& aResult)
{
  if ((aAttribute == nsGkAtoms::width) ||
      (aAttribute == nsGkAtoms::height)) {
    return aResult.ParseSpecialIntValue(aString, PR_TRUE);
  }
  else if ((aAttribute == nsGkAtoms::hspace) ||
           (aAttribute == nsGkAtoms::vspace) ||
           (aAttribute == nsGkAtoms::border)) {
    return aResult.ParseIntWithBounds(aString, 0);
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseFrameborderValue(const nsAString& aString,
                                            nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kFrameborderTable);
}

PRBool
nsGenericHTMLElement::ParseScrollingValue(const nsAString& aString,
                                          nsAttrValue& aResult)
{
  return aResult.ParseEnumValue(aString, kScrollingTable);
}

/**
 * Handle attributes common to all html elements
 */
void
nsGenericHTMLElement::MapCommonAttributesInto(const nsMappedAttributes* aAttributes,
                                              nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(UserInterface)) {
    nsRuleDataUserInterface *ui = aData->mUserInterfaceData;
    if (ui->mUserModify.GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value =
        aAttributes->GetAttr(nsGkAtoms::contenteditable);
      if (value) {
        if (value->Equals(nsGkAtoms::_empty, eCaseMatters) ||
            value->Equals(nsGkAtoms::_true, eIgnoreCase)) {
          ui->mUserModify.SetIntValue(NS_STYLE_USER_MODIFY_READ_WRITE,
                                      eCSSUnit_Enumerated);
        }
        else if (value->Equals(nsGkAtoms::_false, eIgnoreCase)) {
            ui->mUserModify.SetIntValue(NS_STYLE_USER_MODIFY_READ_ONLY,
                                        eCSSUnit_Enumerated);
        }
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Visibility)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::lang);
    if (value && value->Type() == nsAttrValue::eString) {
      aData->mDisplayData->mLang.SetStringValue(value->GetStringValue(),
                                                eCSSUnit_String);
    }
  }
}

void
nsGenericHTMLFormElement::UpdateEditableFormControlState()
{
  ContentEditableTristate value = GetContentEditableValue();
  if (value != eInherit) {
    SetEditableFlag(!!value);

    return;
  }

  nsIContent *parent = GetParent();

  if (parent && parent->HasFlag(NODE_IS_EDITABLE)) {
    SetEditableFlag(PR_TRUE);
    return;
  }

  PRInt32 formType = GetType();
  if (formType != NS_FORM_INPUT_PASSWORD && formType != NS_FORM_INPUT_TEXT &&
      formType != NS_FORM_TEXTAREA) {
    SetEditableFlag(PR_FALSE);
    return;
  }

  // If not contentEditable we still need to check the readonly attribute.
  PRBool roState;
  GetBoolAttr(nsGkAtoms::readonly, &roState);

  SetEditableFlag(!roState);
}


/* static */ const nsGenericHTMLElement::MappedAttributeEntry
nsGenericHTMLElement::sCommonAttributeMap[] = {
  { &nsGkAtoms::contenteditable },
  { &nsGkAtoms::lang },
  { nsnull }
};

/* static */ const nsGenericElement::MappedAttributeEntry
nsGenericHTMLElement::sImageMarginSizeAttributeMap[] = {
  { &nsGkAtoms::width },
  { &nsGkAtoms::height },
  { &nsGkAtoms::hspace },
  { &nsGkAtoms::vspace },
  { nsnull }
};

/* static */ const nsGenericElement::MappedAttributeEntry
nsGenericHTMLElement::sImageAlignAttributeMap[] = {
  { &nsGkAtoms::align },
  { nsnull }
};

/* static */ const nsGenericElement::MappedAttributeEntry
nsGenericHTMLElement::sDivAlignAttributeMap[] = {
  { &nsGkAtoms::align },
  { nsnull }
};

/* static */ const nsGenericElement::MappedAttributeEntry
nsGenericHTMLElement::sImageBorderAttributeMap[] = {
  { &nsGkAtoms::border },
  { nsnull }
};

/* static */ const nsGenericElement::MappedAttributeEntry
nsGenericHTMLElement::sBackgroundAttributeMap[] = {
  { &nsGkAtoms::background },
  { &nsGkAtoms::bgcolor },
  { nsnull }
};

/* static */ const nsGenericElement::MappedAttributeEntry
nsGenericHTMLElement::sBackgroundColorAttributeMap[] = {
  { &nsGkAtoms::bgcolor },
  { nsnull }
};

/* static */ const nsGenericElement::MappedAttributeEntry
nsGenericHTMLElement::sScrollingAttributeMap[] = {
  { &nsGkAtoms::scrolling },
  { nsnull }
};

void
nsGenericHTMLElement::MapImageAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                                 nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & (NS_STYLE_INHERIT_BIT(Display) |
                          NS_STYLE_INHERIT_BIT(TextReset))) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
    if (value && value->Type() == nsAttrValue::eEnum) {
      PRInt32 align = value->GetEnumValue();
      if ((aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Display)) &&
          aRuleData->mDisplayData->mFloat.GetUnit() == eCSSUnit_Null) {
        if (align == NS_STYLE_TEXT_ALIGN_LEFT)
          aRuleData->mDisplayData->mFloat.SetIntValue(NS_STYLE_FLOAT_LEFT, eCSSUnit_Enumerated);
        else if (align == NS_STYLE_TEXT_ALIGN_RIGHT)
          aRuleData->mDisplayData->mFloat.SetIntValue(NS_STYLE_FLOAT_RIGHT, eCSSUnit_Enumerated);
      }
      if ((aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(TextReset)) &&
          aRuleData->mTextData->mVerticalAlign.GetUnit() == eCSSUnit_Null) {
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
nsGenericHTMLElement::MapDivAlignAttributeInto(const nsMappedAttributes* aAttributes,
                                               nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Text)) {
    if (aRuleData->mTextData->mTextAlign.GetUnit() == eCSSUnit_Null) {
      // align: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
      if (value && value->Type() == nsAttrValue::eEnum)
        aRuleData->mTextData->mTextAlign.SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }
}


void
nsGenericHTMLElement::MapImageMarginAttributeInto(const nsMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Margin)))
    return;

  const nsAttrValue* value;

  // hspace: value
  value = aAttributes->GetAttr(nsGkAtoms::hspace);
  if (value) {
    nsCSSValue hval;
    if (value->Type() == nsAttrValue::eInteger)
      hval.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
    else if (value->Type() == nsAttrValue::ePercent)
      hval.SetPercentValue(value->GetPercentValue());

    if (hval.GetUnit() != eCSSUnit_Null) {
      nsCSSRect& margin = aData->mMarginData->mMargin;
      if (margin.mLeft.GetUnit() == eCSSUnit_Null)
        margin.mLeft = hval;
      if (margin.mRight.GetUnit() == eCSSUnit_Null)
        margin.mRight = hval;
    }
  }

  // vspace: value
  value = aAttributes->GetAttr(nsGkAtoms::vspace);
  if (value) {
    nsCSSValue vval;
    if (value->Type() == nsAttrValue::eInteger)
      vval.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
    else if (value->Type() == nsAttrValue::ePercent)
      vval.SetPercentValue(value->GetPercentValue());
  
    if (vval.GetUnit() != eCSSUnit_Null) {
      nsCSSRect& margin = aData->mMarginData->mMargin;
      if (margin.mTop.GetUnit() == eCSSUnit_Null)
        margin.mTop = vval;
      if (margin.mBottom.GetUnit() == eCSSUnit_Null)
        margin.mBottom = vval;
    }
  }
}

void
nsGenericHTMLElement::MapImageSizeAttributesInto(const nsMappedAttributes* aAttributes,
                                                 nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)))
    return;

  // width: value
  if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
    if (value && value->Type() == nsAttrValue::eInteger)
      aData->mPositionData->mWidth.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
    else if (value && value->Type() == nsAttrValue::ePercent)
      aData->mPositionData->mWidth.SetPercentValue(value->GetPercentValue());
  }

  // height: value
  if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
    if (value && value->Type() == nsAttrValue::eInteger)
      aData->mPositionData->mHeight.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
    else if (value && value->Type() == nsAttrValue::ePercent)
      aData->mPositionData->mHeight.SetPercentValue(value->GetPercentValue());    
  }
}

void
nsGenericHTMLElement::MapImageBorderAttributeInto(const nsMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Border)))
    return;

  // border: pixels
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::border);
  if (!value)
    return;
  
  nscoord val = 0;
  if (value->Type() == nsAttrValue::eInteger)
    val = value->GetIntegerValue();

  nsCSSRect& borderWidth = aData->mMarginData->mBorderWidth;
  if (borderWidth.mLeft.GetUnit() == eCSSUnit_Null)
    borderWidth.mLeft.SetFloatValue((float)val, eCSSUnit_Pixel);
  if (borderWidth.mTop.GetUnit() == eCSSUnit_Null)
    borderWidth.mTop.SetFloatValue((float)val, eCSSUnit_Pixel);
  if (borderWidth.mRight.GetUnit() == eCSSUnit_Null)
    borderWidth.mRight.SetFloatValue((float)val, eCSSUnit_Pixel);
  if (borderWidth.mBottom.GetUnit() == eCSSUnit_Null)
    borderWidth.mBottom.SetFloatValue((float)val, eCSSUnit_Pixel);

  nsCSSRect& borderStyle = aData->mMarginData->mBorderStyle;
  if (borderStyle.mLeft.GetUnit() == eCSSUnit_Null)
    borderStyle.mLeft.SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  if (borderStyle.mTop.GetUnit() == eCSSUnit_Null)
    borderStyle.mTop.SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  if (borderStyle.mRight.GetUnit() == eCSSUnit_Null)
    borderStyle.mRight.SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);
  if (borderStyle.mBottom.GetUnit() == eCSSUnit_Null)
    borderStyle.mBottom.SetIntValue(NS_STYLE_BORDER_STYLE_SOLID, eCSSUnit_Enumerated);

  nsCSSRect& borderColor = aData->mMarginData->mBorderColor;
  if (borderColor.mLeft.GetUnit() == eCSSUnit_Null)
    borderColor.mLeft.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  if (borderColor.mTop.GetUnit() == eCSSUnit_Null)
    borderColor.mTop.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  if (borderColor.mRight.GetUnit() == eCSSUnit_Null)
    borderColor.mRight.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
  if (borderColor.mBottom.GetUnit() == eCSSUnit_Null)
    borderColor.mBottom.SetIntValue(NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR, eCSSUnit_Enumerated);
}

void
nsGenericHTMLElement::MapBackgroundInto(const nsMappedAttributes* aAttributes,
                                        nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Background)))
    return;

  nsPresContext* presContext = aData->mPresContext;
  if (aData->mColorData->mBackImage.GetUnit() == eCSSUnit_Null &&
      presContext->UseDocumentColors()) {
    // background
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::background);
    if (value && value->Type() == nsAttrValue::eString) {
      const nsString& spec = value->GetStringValue();
      if (!spec.IsEmpty()) {
        // Resolve url to an absolute url
        // XXX this breaks if the HTML element has an xml:base
        // attribute (the xml:base will not be taken into account)
        // as well as elements with _baseHref set. We need to be able
        // to get to the element somehow, or store the base URI in the
        // attributes.
        nsIDocument* doc = presContext->Document();
        nsCOMPtr<nsIURI> uri;
        nsresult rv = nsContentUtils::NewURIWithDocumentCharset(
            getter_AddRefs(uri), spec, doc, doc->GetBaseURI());
        if (NS_SUCCEEDED(rv)) {
          // Note that this should generally succeed here, due to the way
          // |spec| is created.  Maybe we should just add an nsStringBuffer
          // accessor on nsAttrValue?
          nsStringBuffer* buffer = nsCSSValue::BufferFromString(spec);
          if (NS_LIKELY(buffer != 0)) {
            // XXXbz it would be nice to assert that doc->NodePrincipal() is
            // the same as the principal of the node (which we'd need to store
            // in the mapped attrs or something?)
            nsCSSValue::Image *img =
              new nsCSSValue::Image(uri, buffer, doc->GetDocumentURI(),
                                    doc->NodePrincipal(), doc);
            buffer->Release();
            if (NS_LIKELY(img != 0)) {
              aData->mColorData->mBackImage.SetImageValue(img);
            }
          }
        }
      }
      else if (presContext->CompatibilityMode() == eCompatibility_NavQuirks) {
        // in NavQuirks mode, allow the empty string to set the
        // background to empty
        aData->mColorData->mBackImage.SetNoneValue();
      }
    }
  }
}

void
nsGenericHTMLElement::MapBGColorInto(const nsMappedAttributes* aAttributes,
                                     nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Background)))
    return;

  if (aData->mColorData->mBackColor.GetUnit() == eCSSUnit_Null &&
      aData->mPresContext->UseDocumentColors()) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::bgcolor);
    nscolor color;
    if (value && value->GetColorValue(color)) {
      aData->mColorData->mBackColor.SetColorValue(color);
    }
  }
}

void
nsGenericHTMLElement::MapBackgroundAttributesInto(const nsMappedAttributes* aAttributes,
                                                  nsRuleData* aData)
{
  MapBackgroundInto(aAttributes, aData);
  MapBGColorInto(aAttributes, aData);
}

void
nsGenericHTMLElement::MapScrollingAttributeInto(const nsMappedAttributes* aAttributes,
                                                nsRuleData* aData)
{
  if (!(aData->mSIDs & NS_STYLE_INHERIT_BIT(Display)))
    return;

  // scrolling
  nsCSSValue* overflowValues[2] = {
    &aData->mDisplayData->mOverflowX,
    &aData->mDisplayData->mOverflowY,
  };
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(overflowValues); ++i) {
    if (overflowValues[i]->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::scrolling);
      if (value && value->Type() == nsAttrValue::eEnum) {
        PRInt32 mappedValue;
        switch (value->GetEnumValue()) {
          case NS_STYLE_FRAME_ON:
          case NS_STYLE_FRAME_SCROLL:
          case NS_STYLE_FRAME_YES:
            mappedValue = NS_STYLE_OVERFLOW_SCROLL;
            break;

          case NS_STYLE_FRAME_OFF:
          case NS_STYLE_FRAME_NOSCROLL:
          case NS_STYLE_FRAME_NO:
            mappedValue = NS_STYLE_OVERFLOW_HIDDEN;
            break;
        
          case NS_STYLE_FRAME_AUTO:
            mappedValue = NS_STYLE_OVERFLOW_AUTO;
            break;

          default:
            NS_NOTREACHED("unexpected value");
            mappedValue = NS_STYLE_OVERFLOW_AUTO;
            break;
        }
        overflowValues[i]->SetIntValue(mappedValue, eCSSUnit_Enumerated);
      }
    }
  }
}

//----------------------------------------------------------------------

nsresult
nsGenericHTMLElement::GetAttrHelper(nsIAtom* aAttr, nsAString& aValue)
{
  GetAttr(kNameSpaceID_None, aAttr, aValue);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetAttrHelper(nsIAtom* aAttr, const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, aAttr, aValue, PR_TRUE);
}

nsresult
nsGenericHTMLElement::GetStringAttrWithDefault(nsIAtom* aAttr,
                                               const char* aDefault,
                                               nsAString& aResult)
{
  if (!GetAttr(kNameSpaceID_None, aAttr, aResult)) {
    CopyASCIItoUTF16(aDefault, aResult);
  }
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetBoolAttr(nsIAtom* aAttr, PRBool aValue)
{
  if (aValue) {
    return SetAttr(kNameSpaceID_None, aAttr, EmptyString(), PR_TRUE);
  }

  return UnsetAttr(kNameSpaceID_None, aAttr, PR_TRUE);
}

nsresult
nsGenericHTMLElement::GetBoolAttr(nsIAtom* aAttr, PRBool* aValue) const
{
  *aValue = HasAttr(kNameSpaceID_None, aAttr);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetIntAttr(nsIAtom* aAttr, PRInt32 aDefault, PRInt32* aResult)
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(aAttr);
  if (attrVal && attrVal->Type() == nsAttrValue::eInteger) {
    *aResult = attrVal->GetIntegerValue();
  }
  else {
    *aResult = aDefault;
  }
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetIntAttr(nsIAtom* aAttr, PRInt32 aValue)
{
  nsAutoString value;
  value.AppendInt(aValue);

  return SetAttr(kNameSpaceID_None, aAttr, value, PR_TRUE);
}

nsresult
nsGenericHTMLElement::GetFloatAttr(nsIAtom* aAttr, float aDefault, float* aResult)
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(aAttr);
  if (attrVal && attrVal->Type() == nsAttrValue::eFloatValue) {
    *aResult = attrVal->GetFloatValue();
  }
  else {
    *aResult = aDefault;
  }
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetFloatAttr(nsIAtom* aAttr, float aValue)
{
  nsAutoString value;
  value.AppendFloat(aValue);

  return SetAttr(kNameSpaceID_None, aAttr, value, PR_TRUE);
}

nsresult
nsGenericHTMLElement::GetURIAttr(nsIAtom* aAttr, nsIAtom* aBaseAttr, nsAString& aResult)
{
  nsAutoString attrValue;
  if (!GetAttr(kNameSpaceID_None, aAttr, attrValue)) {
    aResult.Truncate();

    return NS_OK;
  }

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsresult rv;

  if (aBaseAttr) {
    nsAutoString baseAttrValue;
    if (GetAttr(kNameSpaceID_None, aBaseAttr, baseAttrValue)) {
      nsCOMPtr<nsIURI> baseAttrURI;
      rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(baseAttrURI),
                                                     baseAttrValue, GetOwnerDoc(),
                                                     baseURI);
      if (NS_FAILED(rv)) {
        // Just use the attr value as the result...
        aResult = attrValue;

        return NS_OK;
      }
      baseURI.swap(baseAttrURI);
    }
  }

  nsCOMPtr<nsIURI> attrURI;
  rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(attrURI),
                                                 attrValue, GetOwnerDoc(),
                                                 baseURI);
  if (NS_FAILED(rv)) {
    // Just use the attr value as the result...
    aResult = attrValue;

    return NS_OK;
  }

  NS_ASSERTION(attrURI,
               "nsContentUtils::NewURIWithDocumentCharset return value lied");

  nsCAutoString spec;
  attrURI->GetSpec(spec);
  CopyUTF8toUTF16(spec, aResult);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetURIListAttr(nsIAtom* aAttr, nsAString& aResult)
{
  aResult.Truncate();

  nsAutoString value;
  if (!GetAttr(kNameSpaceID_None, aAttr, value))
    return NS_OK;

  nsIDocument* doc = GetOwnerDoc(); 
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  // Value contains relative URIs split on spaces (U+0020)
  const PRUnichar *start = value.BeginReading();
  const PRUnichar *end   = value.EndReading();
  const PRUnichar *iter  = start;
  for (;;) {
    if (iter < end && *iter != ' ') {
      ++iter;
    } else {  // iter is pointing at either end or a space
      while (*start == ' ' && start < iter)
        ++start;
      if (iter != start) {
        if (!aResult.IsEmpty())
          aResult.Append(PRUnichar(' '));
        const nsSubstring& uriPart = Substring(start, iter);
        nsCOMPtr<nsIURI> attrURI;
        nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(attrURI),
                                                  uriPart, doc, baseURI);
        if (attrURI) {
          nsCAutoString spec;
          attrURI->GetSpec(spec);
          AppendUTF8toUTF16(spec, aResult);
        } else {
          aResult.Append(uriPart);
        }
      }
      start = iter = iter + 1;
      if (iter >= end)
        break;
    }
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetContentEditable(nsAString& aContentEditable)
{
  ContentEditableTristate value = GetContentEditableValue();

  if (value == eTrue) {
    aContentEditable.AssignLiteral("true");
  }
  else if (value == eFalse) {
    aContentEditable.AssignLiteral("false");
  }
  else {
    aContentEditable.AssignLiteral("inherit");
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetContentEditable(const nsAString& aContentEditable)
{
  nsString contentEditable;
  ToLowerCase(aContentEditable, contentEditable);

  if (contentEditable.EqualsLiteral("inherit")) {
    UnsetAttr(kNameSpaceID_None, nsGkAtoms::contenteditable, PR_TRUE);

    return NS_OK;
  }

  if (!contentEditable.EqualsLiteral("true") &&
      !contentEditable.EqualsLiteral("false")) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  SetAttr(kNameSpaceID_None, nsGkAtoms::contenteditable, contentEditable,
          PR_TRUE);

  return NS_OK;
}

//----------------------------------------------------------------------

NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsGenericHTMLFrameElement, TabIndex, tabindex, 0)

nsGenericHTMLFormElement::nsGenericHTMLFormElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  mForm = nsnull;
}

nsGenericHTMLFormElement::~nsGenericHTMLFormElement()
{
  // Check that this element is still not the default content
  // of its parent form.
  NS_ASSERTION(!mForm || mForm->GetDefaultSubmitElement() != this,
               "Content being destroyed is the default content");

  // Clean up.  Set the form to nsnull so it knows we went away.
  // Do not notify as the content is being destroyed.
  ClearForm(PR_TRUE, PR_FALSE);
}

NS_IMPL_QUERY_INTERFACE_INHERITED1(nsGenericHTMLFormElement,
                                   nsGenericHTMLElement,
                                   nsIFormControl)

PRBool
nsGenericHTMLFormElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eELEMENT | eHTML | eHTML_FORM_CONTROL));
}

void
nsGenericHTMLFormElement::SaveSubtreeState()
{
  SaveState();

  nsGenericHTMLElement::SaveSubtreeState();
}

void
nsGenericHTMLFormElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  NS_PRECONDITION(aForm, "Don't pass null here");
  NS_ASSERTION(!mForm,
               "We don't support switching from one non-null form to another.");

  // keep a *weak* ref to the form here
  CallQueryInterface(aForm, &mForm);
  mForm->Release();
}

void
nsGenericHTMLFormElement::ClearForm(PRBool aRemoveFromForm,
                                    PRBool aNotify)
{
  NS_ASSERTION((mForm != nsnull) == HasFlag(ADDED_TO_FORM),
               "Form control should have had flag set correctly");

  if (!mForm) {
    return;
  }
  
  if (aRemoveFromForm) {
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

    mForm->RemoveElement(this, aNotify);

    if (!nameVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->RemoveElementFromTable(this, idVal);
    }
  }

  UnsetFlags(ADDED_TO_FORM);
  mForm = nsnull;
}

NS_IMETHODIMP
nsGenericHTMLFormElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  *aForm = nsnull;

  if (mForm) {
    CallQueryInterface(mForm, aForm);
  }

  return NS_OK;
}

PRUint32
nsGenericHTMLFormElement::GetDesiredIMEState()
{
  nsCOMPtr<nsIEditor> editor = nsnull;
  nsresult rv = GetEditorInternal(getter_AddRefs(editor));
  if (NS_FAILED(rv) || !editor)
    return nsGenericHTMLElement::GetDesiredIMEState();
  nsCOMPtr<nsIEditorIMESupport> imeEditor = do_QueryInterface(editor);
  if (!imeEditor)
    return nsGenericHTMLElement::GetDesiredIMEState();
  PRUint32 state;
  rv = imeEditor->GetPreferredIMEState(&state);
  if (NS_FAILED(rv))
    return nsGenericHTMLElement::GetDesiredIMEState();
  return state;
}

PRBool
nsGenericHTMLFrameElement::IsHTMLFocusable(PRBool *aIsFocusable,
                                           PRInt32 *aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aIsFocusable, aTabIndex)) {
    return PR_TRUE;
  }

  // If there is no subdocument, docshell or content viewer, it's not tabbable
  PRBool isFocusable = PR_FALSE;
  nsIDocument *doc = GetCurrentDoc();
  if (doc) {
    // XXXbz should this use GetOwnerDoc() for GetSubDocumentFor?
    // sXBL/XBL2 issue!
    nsIDocument *subDoc = doc->GetSubDocumentFor(this);
    if (subDoc) {
      nsCOMPtr<nsISupports> container = subDoc->GetContainer();
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
      if (docShell) {
        nsCOMPtr<nsIContentViewer> contentViewer;
        docShell->GetContentViewer(getter_AddRefs(contentViewer));
        if (contentViewer) {
          isFocusable = PR_TRUE;
          nsCOMPtr<nsIContentViewer> zombieViewer;
          contentViewer->GetPreviousViewer(getter_AddRefs(zombieViewer));
          if (zombieViewer) {
            // If there are 2 viewers for the current docshell, that 
            // means the current document is a zombie document.
            // Only navigate into the frame/iframe if it's not a zombie.
            isFocusable = PR_FALSE;
          }
        }
      }
    }
  }

  *aIsFocusable = isFocusable;
  if (!isFocusable && aTabIndex) {
    *aTabIndex = -1;
  }

  return PR_FALSE;
}

nsresult
nsGenericHTMLFormElement::BindToTree(nsIDocument* aDocument,
                                     nsIContent* aParent,
                                     nsIContent* aBindingParent,
                                     PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!aParent) {
    return NS_OK;
  }

  PRBool hadForm = (mForm != nsnull);
  
  if (!mForm) {
    // We now have a parent, so we may have picked up an ancestor form.  Search
    // for it.  Note that if mForm is already set we don't want to do this,
    // because that means someone (probably the content sink) has already set
    // it to the right value.  Also note that even if being bound here didn't
    // change our parent, we still need to search, since our parent chain
    // probably changed _somewhere_.
    nsCOMPtr<nsIDOMHTMLFormElement> form = FindForm();
    if (form) {
      SetForm(form);
    }
  }

  if (mForm && !HasFlag(ADDED_TO_FORM)) {
    // Now we need to add ourselves to the form
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);
    
    SetFlags(ADDED_TO_FORM);

    // Notify only if we just found this mForm.
    mForm->AddElement(this, !hadForm);
    
    if (!nameVal.IsEmpty()) {
      mForm->AddElementToTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->AddElementToTable(this, idVal);
    }
  }

  return NS_OK;
}

void
nsGenericHTMLFormElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  // Save state before doing anything
  SaveState();

  if (mForm) {
    // Might need to unset mForm
    if (aNullParent) {
      // No more parent means no more form
      ClearForm(PR_TRUE, PR_TRUE);
    } else {
      // Recheck whether we should still have an mForm.
      nsCOMPtr<nsIDOMHTMLFormElement> form = FindForm(mForm);
      if (!form) {
        ClearForm(PR_TRUE, PR_TRUE);
      } else {
        UnsetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
      }
    }
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
nsGenericHTMLFormElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                        const nsAString* aValue, PRBool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    nsAutoString tmp;

    // remove the control from the hashtable as needed

    if (mForm && (aName == nsGkAtoms::name || aName == nsGkAtoms::id)) {
      GetAttr(kNameSpaceID_None, aName, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp);
      }
    }

    if (mForm && aName == nsGkAtoms::type) {
      nsIDocument* doc = GetCurrentDoc();
      MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, aNotify);
      
      GetAttr(kNameSpaceID_None, nsGkAtoms::name, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp);
      }

      GetAttr(kNameSpaceID_None, nsGkAtoms::id, tmp);

      if (!tmp.IsEmpty()) {
        mForm->RemoveElementFromTable(this, tmp);
      }

      mForm->RemoveElement(this, aNotify);

      // Removing the element from the form can make it not be the default
      // control anymore.  Go ahead and notify on that change, though we might
      // end up readding and becoming the default control again in
      // AfterSetAttr.
      if (doc && aNotify) {
        doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_DEFAULT);
      }
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName,
                                             aValue, aNotify);
}

nsresult
nsGenericHTMLFormElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                       const nsAString* aValue, PRBool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    // add the control to the hashtable as needed

    if (mForm && (aName == nsGkAtoms::name || aName == nsGkAtoms::id) &&
        aValue) {
      if (!aValue->IsEmpty()) {
        mForm->AddElementToTable(this, *aValue);
      }
    }

    if (mForm && aName == nsGkAtoms::type) {
      nsIDocument* doc = GetDocument();
      MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, aNotify);
      
      nsAutoString tmp;

      GetAttr(kNameSpaceID_None, nsGkAtoms::name, tmp);

      if (!tmp.IsEmpty()) {
        mForm->AddElementToTable(this, tmp);
      }

      GetAttr(kNameSpaceID_None, nsGkAtoms::id, tmp);

      if (!tmp.IsEmpty()) {
        mForm->AddElementToTable(this, tmp);
      }

      mForm->AddElement(this, aNotify);

      // Adding the element to the form can make it be the default control .
      // Go ahead and notify on that change.
      // Note: no need to notify on CanBeDisabled(), since type attr
      // changes can't affect that.
      if (doc && aNotify) {
        doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_DEFAULT);
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}

PRBool
nsGenericHTMLFormElement::CanBeDisabled() const
{
  PRInt32 type = GetType();
  // It's easier to test the types that _cannot_ be disabled
  return
    type != NS_FORM_LABEL &&
    type != NS_FORM_LEGEND &&
    type != NS_FORM_FIELDSET &&
    type != NS_FORM_OBJECT;
}

PRBool
nsGenericHTMLFormElement::IsSubmitControl() const
{
  PRInt32 type = GetType();
  return type == NS_FORM_INPUT_SUBMIT ||
         type == NS_FORM_BUTTON_SUBMIT ||
         type == NS_FORM_INPUT_IMAGE;
}

PRInt32
nsGenericHTMLFormElement::IntrinsicState() const
{
  // If you add attribute-dependent states here, you need to add them them to
  // AfterSetAttr too.  And add them to AfterSetAttr for all subclasses that
  // implement IntrinsicState() and are affected by that attribute.
  PRInt32 state = nsGenericHTMLElement::IntrinsicState();

  if (CanBeDisabled()) {
    // :enabled/:disabled
    PRBool disabled;
    GetBoolAttr(nsGkAtoms::disabled, &disabled);
    if (disabled) {
      state |= NS_EVENT_STATE_DISABLED;
      state &= ~NS_EVENT_STATE_ENABLED;
    } else {
      state &= ~NS_EVENT_STATE_DISABLED;
      state |= NS_EVENT_STATE_ENABLED;
    }
  }
  
  if (mForm &&
      // XXXbz Need the cast to make VC++6 happy.
      static_cast<const nsIFormControl*>
                 (mForm->GetDefaultSubmitElement()) == this) {
      NS_ASSERTION(IsSubmitControl(),
                   "Default submit element that isn't a submit control.");
      // We are the default submit element (:default)
      state |= NS_EVENT_STATE_DEFAULT;
  }

  return state;
}

void
nsGenericHTMLFormElement::SetFocusAndScrollIntoView(nsPresContext* aPresContext)
{
  nsIEventStateManager *esm = aPresContext->EventStateManager();
  if (esm->SetContentState(this, NS_EVENT_STATE_FOCUS)) {
    nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);
    if (formControlFrame) {
      formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
      nsCOMPtr<nsIPresShell> presShell = aPresContext->GetPresShell();
      if (presShell) {
        presShell->ScrollContentIntoView(this, NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                         NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
      }
    }
  }
}

void
nsGenericHTMLFormElement::DoSetFocus(nsPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  if (FocusState() == eActiveWindow) {
    SetFocusAndScrollIntoView(aPresContext);
  }
}

nsGenericHTMLFormElement::FocusTristate
nsGenericHTMLFormElement::FocusState()
{
  // We can't be focused if we aren't in a document
  nsIDocument* doc = GetCurrentDoc();
  if (!doc)
    return eUnfocusable;

  // first see if we are disabled or not. If disabled then do nothing.
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return eUnfocusable;
  }

  // If the window is not active, do not allow the focus to bring the
  // window to the front.  We update the focus controller, but do
  // nothing else.
  nsCOMPtr<nsPIDOMWindow> win = doc->GetWindow();
  if (win) {
    nsIFocusController *focusController = win->GetRootFocusController();
    if (focusController) {
      PRBool isActive = PR_FALSE;
      focusController->GetActive(&isActive);
      if (!isActive) {
        focusController->SetFocusedWindow(win);
        nsCOMPtr<nsIDOMElement> el =
          do_QueryInterface(static_cast<nsGenericHTMLElement*>(this));
        focusController->SetFocusedElement(el);

        return eInactiveWindow;
      }
    }
  }

  return eActiveWindow;
}

//----------------------------------------------------------------------

nsGenericHTMLFrameElement::~nsGenericHTMLFrameElement()
{
  if (mFrameLoader) {
    mFrameLoader->Destroy();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGenericHTMLFrameElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGenericHTMLFrameElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFrameLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_TABLE_HEAD(nsGenericHTMLFrameElement)
  NS_INTERFACE_TABLE_INHERITED2(nsGenericHTMLFrameElement,
                                nsIDOMNSHTMLFrameElement,
                                nsIFrameLoaderOwner)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsGenericHTMLFrameElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

nsresult
nsGenericHTMLFrameElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  NS_PRECONDITION(aContentDocument, "Null out param");
  *aContentDocument = nsnull;

  nsCOMPtr<nsIDOMWindow> win;
  GetContentWindow(getter_AddRefs(win));

  if (!win) {
    return NS_OK;
  }

  return win->GetDocument(aContentDocument);
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::GetContentWindow(nsIDOMWindow** aContentWindow)
{
  NS_PRECONDITION(aContentWindow, "Null out param");
  *aContentWindow = nsnull;

  nsresult rv = EnsureFrameLoader();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mFrameLoader) {
    return NS_OK;
  }

  PRBool depthTooGreat = PR_FALSE;
  mFrameLoader->GetDepthTooGreat(&depthTooGreat);
  if (depthTooGreat) {
    // Claim to have no contentWindow
    return NS_OK;
  }
  
  nsCOMPtr<nsIDocShell> doc_shell;
  mFrameLoader->GetDocShell(getter_AddRefs(doc_shell));

  nsCOMPtr<nsPIDOMWindow> win(do_GetInterface(doc_shell));

  if (!win) {
    return NS_OK;
  }

  NS_ASSERTION(win->IsOuterWindow(),
               "Uh, this window should always be an outer window!");

  return CallQueryInterface(win, aContentWindow);
}

nsresult
nsGenericHTMLFrameElement::EnsureFrameLoader()
{
  if (!GetParent() || !IsInDoc() || mFrameLoader) {
    // If frame loader is there, we just keep it around, cached
    return NS_OK;
  }

  mFrameLoader = new nsFrameLoader(this);
  if (!mFrameLoader)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::GetFrameLoader(nsIFrameLoader **aFrameLoader)
{
  NS_IF_ADDREF(*aFrameLoader = mFrameLoader);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::SwapFrameLoaders(nsIFrameLoaderOwner* aOtherOwner)
{
  // We don't support this yet
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsGenericHTMLFrameElement::LoadSrc()
{
  nsresult rv = EnsureFrameLoader();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mFrameLoader) {
    return NS_OK;
  }

  rv = mFrameLoader->LoadFrame();
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to load URL");
  }
#endif

  return rv;
}

nsresult
nsGenericHTMLFrameElement::BindToTree(nsIDocument* aDocument,
                                      nsIContent* aParent,
                                      nsIContent* aBindingParent,
                                      PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "Missing a script blocker!");
    // We're in a document now.  Kick off the frame load.
    LoadSrc();
  }
  
  return rv;
}

void
nsGenericHTMLFrameElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  if (mFrameLoader) {
    // This iframe is being taken out of the document, destroy the
    // iframe's frame loader (doing that will tear down the window in
    // this iframe).
    // XXXbz we really want to only partially destroy the frame
    // loader... we don't want to tear down the docshell.  Food for
    // later bug.
    mFrameLoader->Destroy();
    mFrameLoader = nsnull;
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
nsGenericHTMLFrameElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                   nsIAtom* aPrefix, const nsAString& aValue,
                                   PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  
  if (NS_SUCCEEDED(rv) && aNameSpaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::src) {
    return LoadSrc();
  }

  return rv;
}

void
nsGenericHTMLFrameElement::DestroyContent()
{
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nsnull;
  }

  nsGenericHTMLElement::DestroyContent();
}

//----------------------------------------------------------------------

void
nsGenericHTMLElement::SetElementFocus(PRBool aDoFocus)
{
  nsCOMPtr<nsPresContext> presContext = GetPresContext();
  if (!presContext)
    return;

  if (aDoFocus) {
    if (IsInDoc()) {
      // Make sure that our frames are up to date so we focus the right thing.
      GetCurrentDoc()->FlushPendingNotifications(Flush_Frames);
    }

    SetFocus(presContext);

    presContext->EventStateManager()->MoveCaretToFocus();
    return;
  }

  RemoveFocus(presContext);
}

nsresult
nsGenericHTMLElement::Blur()
{
  if (ShouldBlur(this)) {
    SetElementFocus(PR_FALSE);
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::Focus()
{
  // Generic HTML elements are focusable only if tabindex explicitly set.
  // SetFocus() will check to see if we're focusable and then
  // call into esm to do the work of focusing.
  if (ShouldFocus(this)) {
    SetElementFocus(PR_TRUE);
  }

  return NS_OK;
}

void
nsGenericHTMLElement::RemoveFocus(nsPresContext *aPresContext)
{
  if (!aPresContext) 
    return;

  if (IsNodeOfType(eHTML_FORM_CONTROL)) {
    nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);
    if (formControlFrame) {
      formControlFrame->SetFocus(PR_FALSE, PR_FALSE);
    }
  }
  
  if (IsInDoc()) {
    aPresContext->EventStateManager()->SetContentState(nsnull,
                                                       NS_EVENT_STATE_FOCUS);
  }
}

PRBool
nsGenericHTMLElement::IsHTMLFocusable(PRBool *aIsFocusable, PRInt32 *aTabIndex)
{
  nsIDocument *doc = GetCurrentDoc();
  if (!doc || doc->HasFlag(NODE_IS_EDITABLE)) {
    // In designMode documents we only allow focusing the document.
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = PR_FALSE;

    return PR_TRUE;
  }

  PRInt32 tabIndex = 0;   // Default value for non HTML elements with -moz-user-focus
  GetTabIndex(&tabIndex);

  PRBool override, disabled;
  if (IsEditableRoot()) {
    // Editable roots should always be focusable.
    override = PR_TRUE;

    // Ignore the disabled attribute in editable contentEditable/designMode
    // roots.
    disabled = PR_FALSE;
    if (!HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
      // The default value for tabindex should be 0 for editable
      // contentEditable roots.
      tabIndex = 0;
    }
  }
  else {
    override = PR_FALSE;

    // Just check for disabled attribute on all HTML elements
    disabled = HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
    if (disabled) {
      tabIndex = -1;
    }
  }

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }

  // If a tabindex is specified at all, or the default tabindex is 0, we're focusable
  *aIsFocusable = tabIndex >= 0 ||
                  (!disabled && HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex));

  return override;
}

void
nsGenericHTMLElement::RegUnRegAccessKey(PRBool aDoReg)
{
  // first check to see if we have an access key
  nsAutoString accessKey;
  GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);
  if (accessKey.IsEmpty()) {
    return;
  }

  // We have an access key, so get the ESM from the pres context.
  nsPresContext *presContext = GetPresContext();

  if (presContext) {
    nsIEventStateManager *esm = presContext->EventStateManager();

    // Register or unregister as appropriate.
    if (aDoReg) {
      esm->RegisterAccessKey(this, (PRUint32)accessKey.First());
    } else {
      esm->UnregisterAccessKey(this, (PRUint32)accessKey.First());
    }
  }
}

void
nsGenericHTMLElement::PerformAccesskey(PRBool aKeyCausesActivation,
                                       PRBool aIsTrustedEvent)
{
  nsPresContext *presContext = GetPresContext();
  if (!presContext)
    return;

  nsIEventStateManager *esm = presContext->EventStateManager();
  if (!esm)
    return;

  // It's hard to say what HTML4 wants us to do in all cases.
  esm->ChangeFocusWith(this, nsIEventStateManager::eEventFocusedByKey);

  if (aKeyCausesActivation) {
    // Click on it if the users prefs indicate to do so.
    nsMouseEvent event(aIsTrustedEvent, NS_MOUSE_CLICK,
                       nsnull, nsMouseEvent::eReal);

    nsAutoPopupStatePusher popupStatePusher(aIsTrustedEvent ?
                                            openAllowed : openAbused);

    nsEventDispatcher::Dispatch(this, presContext, &event);
  }
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
  uri->SetScheme(NS_ConvertUTF16toUTF8(Substring(start, iter)));
   
  nsCAutoString newHref;
  uri->GetSpec(newHref);

  CopyUTF8toUTF16(newHref, aResult);

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

  uri->SetHost(NS_ConvertUTF16toUTF8(aHostname));

  nsCAutoString newHref;
  uri->GetSpec(newHref);

  CopyUTF8toUTF16(newHref, aResult);

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

  url->SetFilePath(NS_ConvertUTF16toUTF8(aPathname));

  nsCAutoString newHref;
  uri->GetSpec(newHref);

  CopyUTF8toUTF16(newHref, aResult);

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

  CopyASCIItoUTF16(scheme, aResult);
  aResult.AppendLiteral("://");
  if (!userpass.IsEmpty()) {
    AppendUTF8toUTF16(userpass, aResult);
    aResult.Append(PRUnichar('@'));
  }
  aResult.Append(aHost);
  AppendUTF8toUTF16(path, aResult);

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

  url->SetQuery(NS_ConvertUTF16toUTF8(aSearch));

  nsCAutoString newHref;
  uri->GetSpec(newHref);

  CopyUTF8toUTF16(newHref, aResult);

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

  rv = url->SetRef(NS_ConvertUTF16toUTF8(aHash));

  nsCAutoString newHref;
  uri->GetSpec(newHref);

  CopyUTF8toUTF16(newHref, aResult);

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

  CopyUTF8toUTF16(newHref, aResult);

  return NS_OK;
}

// static
nsresult
nsGenericHTMLElement::GetProtocolFromHrefString(const nsAString& aHref,
                                                nsAString& aProtocol,
                                                nsIDocument *aDocument)
{
  aProtocol.Truncate();

  nsIIOService* ioService = nsContentUtils::GetIOService();
  NS_ENSURE_TRUE(ioService, NS_ERROR_FAILURE);

  nsCAutoString protocol;

  nsresult rv =
    ioService->ExtractScheme(NS_ConvertUTF16toUTF8(aHref), protocol);

  if (NS_SUCCEEDED(rv)) {
    CopyASCIItoUTF16(protocol, aProtocol);
  } else {
    // set the protocol to the protocol of the base URI.

    if (aDocument) {
      nsIURI *uri = aDocument->GetBaseURI();
      if (uri) {
        uri->GetScheme(protocol);
      }
    }

    if (protocol.IsEmpty()) {
      // set the protocol to http since it is the most likely protocol
      // to be used.
      aProtocol.AssignLiteral("http");
    } else {
      CopyASCIItoUTF16(protocol, aProtocol);
    }
  }
  aProtocol.Append(PRUnichar(':'));

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
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      // Don't throw from these methods!  Not a valid URI means return
      // empty string.
      rv = NS_OK;
    }
    return rv;
  }

  nsCAutoString hostport;
  rv = uri->GetHostPort(hostport);

  // Failure to get the hostport from the URI isn't necessarily an
  // error. Some URI's just don't have a hostport.

  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(hostport, aHost);
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
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      // Don't throw from these methods!  Not a valid URI means return
      // empty string.
      rv = NS_OK;
    }
    return rv;
  }

  nsCAutoString host;
  rv = url->GetHost(host);

  if (NS_SUCCEEDED(rv)) {
    // Failure to get the host from the URI isn't necessarily an
    // error. Some URI's just don't have a host.

    CopyUTF8toUTF16(host, aHostname);
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
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      rv = NS_OK;
    }
    return rv;
  }

  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));

  if (!url) {
    // If this is not a URL, we can't get the pathname from the URI

    return NS_OK;
  }

  nsCAutoString file;
  rv = url->GetFilePath(file);
  if (NS_FAILED(rv))
    return rv;

  CopyUTF8toUTF16(file, aPathname);

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
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      rv = NS_OK;
    }
    return rv;
  }

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
    CopyUTF8toUTF16(NS_LITERAL_CSTRING("?") + search, aSearch);
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
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      rv = NS_OK;
    }
    return rv;
  }

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
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      rv = NS_OK;
    }
    return rv;
  }

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

  if (!ref.IsEmpty()) {
    aHash.Assign(PRUnichar('#'));
    AppendASCIItoUTF16(ref, aHash);
  }
  return NS_OK;
}

const nsAttrName*
nsGenericHTMLElement::InternalGetExistingAttrNameFromQName(const nsAString& aStr) const
{
  if (mNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
    nsAutoString lower;
    ToLowerCase(aStr, lower);
    return mAttrsAndChildren.GetExistingAttrNameFromQName(
      NS_ConvertUTF16toUTF8(lower));
  }

  return mAttrsAndChildren.GetExistingAttrNameFromQName(
    NS_ConvertUTF16toUTF8(aStr));
}

nsresult
nsGenericHTMLElement::GetEditor(nsIEditor** aEditor)
{
  *aEditor = nsnull;

  if (!nsContentUtils::IsCallerTrustedForWrite())
    return NS_ERROR_DOM_SECURITY_ERR;

  return GetEditorInternal(aEditor);
}

nsresult
nsGenericHTMLElement::GetEditorInternal(nsIEditor** aEditor)
{
  *aEditor = nsnull;

  nsIFormControlFrame *fcFrame = GetFormControlFrame(PR_FALSE);
  if (fcFrame) {
    nsITextControlFrame *textFrame = do_QueryFrame(fcFrame);
    if (textFrame) {
      return textFrame->GetEditor(aEditor);
    }
  }

  return NS_OK;
}

already_AddRefed<nsIEditor>
nsGenericHTMLElement::GetAssociatedEditor()
{
  // If contenteditable is ever implemented, it might need to do something different here?

  nsIEditor* editor = nsnull;
  GetEditorInternal(&editor);
  return editor;
}

PRBool
nsGenericHTMLElement::IsCurrentBodyElement()
{
  nsCOMPtr<nsIDOMHTMLBodyElement> bodyElement = do_QueryInterface(this);
  if (!bodyElement) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMHTMLDocument> htmlDocument =
    do_QueryInterface(GetCurrentDoc());
  if (!htmlDocument) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMHTMLElement> htmlElement;
  htmlDocument->GetBody(getter_AddRefs(htmlElement));
  return htmlElement == bodyElement;
}

// static
void
nsGenericHTMLElement::SyncEditorsOnSubtree(nsIContent* content)
{
  /* Sync this node */
  nsGenericHTMLElement* element = FromContent(content);
  if (element) {
    nsCOMPtr<nsIEditor> editor = element->GetAssociatedEditor();
    if (editor) {
      editor->SyncRealTimeSpell();
    }
  }

  /* Sync all children */
  PRUint32 childCount = content->GetChildCount();
  for (PRUint32 i = 0; i < childCount; ++i) {
    nsIContent* childContent = content->GetChildAt(i);
    NS_ASSERTION(childContent,
                 "DOM mutated unexpectedly while syncing editors!");
    if (childContent) {
      SyncEditorsOnSubtree(childContent);
    }
  }
}

void
nsGenericHTMLElement::RecompileScriptEventListeners()
{
    PRInt32 i, count = mAttrsAndChildren.AttrCount();
    for (i = 0; i < count; ++i) {
        const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);

        // Eventlistenener-attributes are always in the null namespace
        if (!name->IsAtom()) {
            continue;
        }

        nsIAtom *attr = name->Atom();
        if (!nsContentUtils::IsEventAttributeName(attr, EventNameType_HTML)) {
            continue;
        }

        nsAutoString value;
        GetAttr(kNameSpaceID_None, attr, value);
        AddScriptEventListener(attr, value, PR_TRUE);
    }
}

PRBool
nsGenericHTMLElement::IsEditableRoot() const
{
  nsIDocument *document = GetCurrentDoc();
  if (!document) {
    return PR_FALSE;
  }

  if (document->HasFlag(NODE_IS_EDITABLE)) {
    return PR_FALSE;
  }

  if (GetContentEditableValue() != eTrue) {
    return PR_FALSE;
  }

  nsIContent *parent = GetParent();

  return !parent || !parent->HasFlag(NODE_IS_EDITABLE);
}

static void
MakeContentDescendantsEditable(nsIContent *aContent, nsIDocument *aDocument)
{
  PRInt32 stateBefore = aContent->IntrinsicState();

  aContent->UpdateEditableState();

  if (aDocument && stateBefore != aContent->IntrinsicState()) {
    aDocument->ContentStatesChanged(aContent, nsnull,
                                    NS_EVENT_STATE_MOZ_READONLY |
                                    NS_EVENT_STATE_MOZ_READWRITE);
  }

  PRUint32 i, n = aContent->GetChildCount();
  for (i = 0; i < n; ++i) {
    nsIContent *child = aContent->GetChildAt(i);
    if (!child->HasAttr(kNameSpaceID_None, nsGkAtoms::contenteditable)) {
      MakeContentDescendantsEditable(child, aDocument);
    }
  }
}

void
nsGenericHTMLElement::ChangeEditableState(PRInt32 aChange)
{
  nsIDocument* document = GetCurrentDoc();
  if (!document) {
    return;
  }

  if (aChange != 0) {
    nsCOMPtr<nsIHTMLDocument> htmlDocument =
      do_QueryInterface(document);
    if (htmlDocument) {
      htmlDocument->ChangeContentEditableCount(this, aChange);
    }
  }

  if (document->HasFlag(NODE_IS_EDITABLE)) {
    document = nsnull;
  }

  MakeContentDescendantsEditable(this, document);
}
