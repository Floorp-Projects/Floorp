/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsIHTMLDocument.h"
#include "nsIDocument.h"
#include "nsIAtom.h"
#include "nsIArena.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsCSSBlockFrame.h"
#include "nsIPresContext.h"
#include "nsIDeviceContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsDOMNodeList.h"
#include "nsUnitConversion.h"
#include "nsStyleUtil.h"
#include "nsIURL.h"
#include "prprf.h"
#include "nsISizeOfHandler.h"

#include "nsCSSInlineFrame.h"
#include "nsIWebShell.h"


static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

nsresult
NS_NewHTMLContainer(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag)
{
  nsHTMLContainer* it = new nsHTMLContainer(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

nsresult
NS_NewHTMLContainer(nsIHTMLContent** aInstancePtrResult,
                    nsIArena* aArena, nsIAtom* aTag)
{
  nsHTMLContainer* it = new(aArena) nsHTMLContainer(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

nsHTMLContainer::nsHTMLContainer()
{
  mChildNodes = nsnull;
}

nsHTMLContainer::nsHTMLContainer(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
  mChildNodes = nsnull;
}

nsHTMLContainer::~nsHTMLContainer()
{
  PRInt32 n = mChildren.Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsIContent* kid = (nsIContent*) mChildren.ElementAt(i);
    NS_RELEASE(kid);
  }
  
  if (nsnull != mChildNodes) {
    mChildNodes->ReleaseContent();
    NS_RELEASE(mChildNodes);
  }
}

NS_IMETHODIMP
nsHTMLContainer::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  nsHTMLContainer::SizeOfWithoutThis(aHandler);
  return NS_OK;
}

void
nsHTMLContainer::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
{
  aHandler->Add((size_t) (- (PRInt32)sizeof(mChildren) ) );
  mChildren.SizeOf(aHandler);

  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIContent* child = (nsIContent*) mChildren[i];
    if (!aHandler->HaveSeen(child)) {
      child->SizeOf(aHandler);
    }
  }
}

NS_IMETHODIMP
nsHTMLContainer::CanContainChildren(PRBool& aResult) const
{
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::ChildCount(PRInt32& aCount) const
{
  aCount = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(aIndex);
  if (nsnull != child) {
    NS_ADDREF(child);
  }
  aResult = child;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");
  aIndex = mChildren.IndexOf(aPossibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                               PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentInserted(this, aKid, aIndex);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                                PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIContent* oldKid = (nsIContent*) mChildren.ElementAt(aIndex);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentReplaced(this, oldKid, aKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  NS_PRECONDITION((nsnull != aKid) && (aKid != this), "null ptr");
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentAppended(this);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsIContent* oldKid = (nsIContent*) mChildren.ElementAt(aIndex);
  if (nsnull != oldKid ) {
    nsIDocument* doc = mDocument;
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentWillBeRemoved(this, oldKid, aIndex);
      }
    }
    PRBool rv = mChildren.RemoveElementAt(aIndex);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentHasBeenRemoved(this, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::Compact()
{
  //XXX I'll turn this on in a bit... mChildren.Compact();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::CreateFrame(nsIPresContext* aPresContext,
                             nsIFrame* aParentFrame,
                             nsIStyleContext* aStyleContext,
                             nsIFrame*& aResult)
{
  const nsStyleDisplay* styleDisplay =
    (const nsStyleDisplay*) aStyleContext->GetStyleData(eStyleStruct_Display);

  // Use style to choose what kind of frame to create
  nsIFrame* frame = nsnull;
  nsresult rv;
  switch (styleDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
    rv = NS_NewCSSBlockFrame(&frame, this, aParentFrame);
    break;

  case NS_STYLE_DISPLAY_INLINE:
    rv = NS_NewCSSInlineFrame(&frame, this, aParentFrame);
    break;

  default:
    // Create an empty frame for holding content that is not being
    // reflowed.
    rv = nsFrame::NewFrame(&frame, this, aParentFrame);
    break;
  }
  if (NS_OK == rv) {
    frame->SetStyleContext(aPresContext, aStyleContext);
  }
  aResult = frame;
  return rv;
}

//----------------------------------------------------------------------

static nsHTMLTagContent::EnumTable kListTypeTable[] = {
  { "none", NS_STYLE_LIST_STYLE_NONE },
  { "disc", NS_STYLE_LIST_STYLE_DISC },
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { "decimal", NS_STYLE_LIST_STYLE_DECIMAL },
  { "lower-roman", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { "upper-roman", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "lower-alpha", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "upper-alpha", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { 0 }
};

static nsHTMLTagContent::EnumTable kListItemTypeTable[] = {
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { 0 }
};

static nsHTMLTagContent::EnumTable kDirTable[] = {
  { "ltr", NS_STYLE_DIRECTION_LTR },
  { "rtl", NS_STYLE_DIRECTION_RTL },
  { 0 }
};

NS_IMETHODIMP
nsHTMLContainer::SetAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              PRBool aNotify)
{
  // Special handling code for various html container attributes; note
  // that if an attribute doesn't require special handling then we
  // fall through and use the default base class implementation.
  nsHTMLValue val;

  // Check for attributes common to most html containers
  if (aAttribute == nsHTMLAtoms::dir) {
    if (ParseEnumValue(aValue, kDirTable, val)) {
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if (aAttribute == nsHTMLAtoms::lang) {
    return nsHTMLTagContent::SetAttribute(aAttribute, aValue, aNotify);
  }

  if (mTag == nsHTMLAtoms::p) {
    if ((aAttribute == nsHTMLAtoms::align) &&
        ParseDivAlignParam(aValue, val)) {
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if (mTag == nsHTMLAtoms::a) {
    if (aAttribute == nsHTMLAtoms::href) {
      nsAutoString href(aValue);
      href.StripWhitespace();
      return nsHTMLTagContent::SetAttribute(aAttribute, href, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::suppress) {
      if (aValue.EqualsIgnoreCase("true")) {
        nsHTMLValue val;
        val.SetEmptyValue();
        return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
      }
    }
    // XXX PRE?
  }
  else if (mTag == nsHTMLAtoms::font) {
    if ((aAttribute == nsHTMLAtoms::size) ||
        (aAttribute == nsHTMLAtoms::pointSize) ||
        (aAttribute == nsHTMLAtoms::fontWeight)) {
      nsAutoString tmp(aValue);
      PRInt32 ec, v = tmp.ToInteger(&ec);
      tmp.CompressWhitespace(PR_TRUE, PR_FALSE);
      PRUnichar ch = tmp.First();
      val.SetIntValue(v, ((ch == '+') || (ch == '-')) ?
                      eHTMLUnit_Integer : eHTMLUnit_Enumerated);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::color) {
      ParseColor(aValue, val);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if ((mTag == nsHTMLAtoms::div) || (mTag == nsHTMLAtoms::multicol)) {
    if ((mTag == nsHTMLAtoms::div) && (aAttribute == nsHTMLAtoms::align) &&
        ParseDivAlignParam(aValue, val)) {
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::cols) {
      ParseValue(aValue, 0, val, eHTMLUnit_Integer);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }

    // Note: These attributes only apply when cols > 1
    if (aAttribute == nsHTMLAtoms::gutter) {
      ParseValue(aValue, 1, val, eHTMLUnit_Pixel);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::width) {
      ParseValueOrPercent(aValue, val, eHTMLUnit_Pixel);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if ((mTag == nsHTMLAtoms::h1) || (mTag == nsHTMLAtoms::h2) ||
           (mTag == nsHTMLAtoms::h3) || (mTag == nsHTMLAtoms::h4) ||
           (mTag == nsHTMLAtoms::h5) || (mTag == nsHTMLAtoms::h6)) {
    if ((aAttribute == nsHTMLAtoms::align) &&
        ParseDivAlignParam(aValue, val)) {
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if (mTag == nsHTMLAtoms::pre) {
    if ((aAttribute == nsHTMLAtoms::wrap) ||
        (aAttribute == nsHTMLAtoms::variable)) {
      val.SetEmptyValue();
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::cols) {
      ParseValue(aValue, 0, val, eHTMLUnit_Integer);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::tabstop) {
      PRInt32 ec, tabstop = aValue.ToInteger(&ec);
      if (tabstop <= 0) {
        tabstop = 8;
      }
      val.SetIntValue(tabstop, eHTMLUnit_Integer);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if (mTag == nsHTMLAtoms::li) {
    if (aAttribute == nsHTMLAtoms::type) {
      if (ParseEnumValue(aValue, kListItemTypeTable, val)) {
        return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
      }
      // Illegal type values are left as is for the dom
    }
    if (aAttribute == nsHTMLAtoms::value) {
      ParseValue(aValue, 1, val, eHTMLUnit_Integer);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if ((mTag == nsHTMLAtoms::ul) || (mTag == nsHTMLAtoms::ol) ||
           (mTag == nsHTMLAtoms::menu) || (mTag == nsHTMLAtoms::dir)) {
    if (aAttribute == nsHTMLAtoms::type) {
      if (!ParseEnumValue(aValue, kListTypeTable, val)) {
        val.SetIntValue(NS_STYLE_LIST_STYLE_BASIC, eHTMLUnit_Enumerated);
      }
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::start) {
      ParseValue(aValue, 1, val, eHTMLUnit_Integer);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::compact) {
      val.SetEmptyValue();
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if (mTag == nsHTMLAtoms::dl) {
    if (aAttribute == nsHTMLAtoms::compact) {
      val.SetEmptyValue();
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  else if (mTag == nsHTMLAtoms::body) {
    if (aAttribute == nsHTMLAtoms::background) {
      nsAutoString href(aValue);
      href.StripWhitespace();
      return nsHTMLTagContent::SetAttribute(aAttribute, href, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::bgcolor) {
      ParseColor(aValue, val);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::text) {
      ParseColor(aValue, val);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::link) {
      ParseColor(aValue, val);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::alink) {
      ParseColor(aValue, val);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::vlink) {
      ParseColor(aValue, val);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::marginwidth) {
      ParseValue(aValue, 0, val, eHTMLUnit_Pixel);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
    if (aAttribute == nsHTMLAtoms::marginheight) {
      ParseValue(aValue, 0, val, eHTMLUnit_Pixel);
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }

  // Use default attribute catching code
  return nsHTMLTagContent::SetAttribute(aAttribute, aValue, aNotify);
}

NS_IMETHODIMP
nsHTMLContainer::AttributeToString(nsIAtom* aAttribute,
                                   nsHTMLValue& aValue,
                                   nsString& aResult) const
{
  nsresult ca = NS_CONTENT_ATTR_NOT_THERE;
  if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
    if (aAttribute == nsHTMLAtoms::align) {
      DivAlignParamToString(aValue, aResult);
      ca = NS_CONTENT_ATTR_HAS_VALUE;
    }
    else if (mTag == nsHTMLAtoms::li) {
      if (aAttribute == nsHTMLAtoms::type) {
        EnumValueToString(aValue, kListItemTypeTable, aResult);
        ca = NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
    else if ((mTag == nsHTMLAtoms::ul) || (mTag == nsHTMLAtoms::ol) ||
             (mTag == nsHTMLAtoms::menu) || (mTag == nsHTMLAtoms::dir)) {
      if (aAttribute == nsHTMLAtoms::type) {
        EnumValueToString(aValue, kListTypeTable, aResult);
        ca = NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
    else if (mTag == nsHTMLAtoms::font) {
      if ((aAttribute == nsHTMLAtoms::size) ||
          (aAttribute == nsHTMLAtoms::pointSize) ||
          (aAttribute == nsHTMLAtoms::fontWeight)) {
        aResult.Truncate();
        aResult.Append(aValue.GetIntValue(), 10);
        ca = NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  }
  else if (aValue.GetUnit() == eHTMLUnit_Integer) {
    if (mTag == nsHTMLAtoms::font) {
      if ((aAttribute == nsHTMLAtoms::size) ||
          (aAttribute == nsHTMLAtoms::pointSize) ||
          (aAttribute == nsHTMLAtoms::fontWeight)) {
        aResult.Truncate();
        PRInt32 value = aValue.GetIntValue();
        if (value >= 0) {
          aResult.Append('+');
        }
        aResult.Append(value, 10);
        ca = NS_CONTENT_ATTR_HAS_VALUE;
      }
    }
  }
  else {
    ca = nsHTMLTagContent::AttributeToString(aAttribute, aValue, aResult);
  }
  return ca;
}

NS_IMETHODIMP
nsHTMLContainer::MapAttributesInto(nsIStyleContext* aContext, 
                                   nsIPresContext* aPresContext)
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;

    // Check for attributes common to most html containers
    GetAttribute(nsHTMLAtoms::dir, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      nsStyleDisplay* display = (nsStyleDisplay*)
        aContext->GetMutableStyleData(eStyleStruct_Display);
      display->mDirection = value.GetIntValue();
    }

    if (mTag == nsHTMLAtoms::p) {
      // align: enum
      GetAttribute(nsHTMLAtoms::align, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) {
        nsStyleText* text = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
        text->mTextAlign = value.GetIntValue();
      }
    }
    else if (mTag == nsHTMLAtoms::a) {
      // suppress: bool (absolute)
      GetAttribute(nsHTMLAtoms::suppress, value);
      if (value.GetUnit() == eHTMLUnit_Empty) {
        // XXX set suppress
      }
    }
    else if (mTag == nsHTMLAtoms::font) {
      nsStyleFont* font = (nsStyleFont*)aContext->GetMutableStyleData(eStyleStruct_Font);
      const nsStyleFont* parentFont = font;
      nsIStyleContext* parentContext = aContext->GetParent();
      if (nsnull != parentContext) {
        parentFont = (const nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
      }
      const nsFont& defaultFont = aPresContext->GetDefaultFont(); 
      const nsFont& defaultFixedFont = aPresContext->GetDefaultFixedFont(); 

      // face: string list
      GetAttribute(nsHTMLAtoms::face, value);
      if (value.GetUnit() == eHTMLUnit_String) {

        nsIDeviceContext* dc = aPresContext->GetDeviceContext();
        if (nsnull != dc) {
          nsAutoString  familyList;

          value.GetStringValue(familyList);
          
          font->mFont.name = familyList;
          nsAutoString face;
          if (NS_OK == dc->FirstExistingFont(font->mFont, face)) {
            if (face.EqualsIgnoreCase("monospace")) {
              font->mFont = font->mFixedFont;
            }
            else {
              font->mFixedFont.name = familyList;
            }
          }
          else {
            font->mFont.name = defaultFont.name;
            font->mFixedFont.name= defaultFixedFont.name;
          }
          font->mFlags |= NS_STYLE_FONT_FACE_EXPLICIT;
          NS_RELEASE(dc);
        }
      }

      // pointSize: int, enum
      GetAttribute(nsHTMLAtoms::pointSize, value);
      if (value.GetUnit() == eHTMLUnit_Integer) {
        // XXX should probably sanitize value
        font->mFont.size = parentFont->mFont.size + NSIntPointsToTwips(value.GetIntValue());
        font->mFixedFont.size = parentFont->mFixedFont.size + NSIntPointsToTwips(value.GetIntValue());
        font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
      }
      else if (value.GetUnit() == eHTMLUnit_Enumerated) {
        font->mFont.size = NSIntPointsToTwips(value.GetIntValue());
        font->mFixedFont.size = NSIntPointsToTwips(value.GetIntValue());
        font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
      }
      else {
        // size: int, enum , NOTE: this does not count as an explicit size
        // also this has no effect if font is already explicit
        if (0 == (font->mFlags & NS_STYLE_FONT_SIZE_EXPLICIT)) {
          GetAttribute(nsHTMLAtoms::size, value);
          if ((value.GetUnit() == eHTMLUnit_Integer) || (value.GetUnit() == eHTMLUnit_Enumerated)) { 
            PRInt32 size = value.GetIntValue();
        
            if (value.GetUnit() == eHTMLUnit_Integer) { // int (+/-)
              size = 3 + size;  // XXX should be BASEFONT, not three
            }
            size = ((0 < size) ? ((size < 8) ? size : 7) : 1); 
            PRInt32 scaler = aPresContext->GetFontScaler();
            float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
            font->mFont.size = nsStyleUtil::CalcFontPointSize(size, (PRInt32)defaultFont.size, scaleFactor);
            font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(size, (PRInt32)defaultFixedFont.size, scaleFactor);
          }
        }
      }

      // fontWeight: int, enum
      GetAttribute(nsHTMLAtoms::fontWeight, value);
      if (value.GetUnit() == eHTMLUnit_Integer) { // +/-
        PRInt32 weight = parentFont->mFont.weight + value.GetIntValue();
        font->mFont.weight = ((100 < weight) ? ((weight < 700) ? weight : 700) : 100);
        font->mFixedFont.weight = ((100 < weight) ? ((weight < 700) ? weight : 700) : 100);
      }
      else if (value.GetUnit() == eHTMLUnit_Enumerated) {
        PRInt32 weight = value.GetIntValue();
        weight = ((100 < weight) ? ((weight < 700) ? weight : 700) : 100);
        font->mFont.weight = weight;
        font->mFixedFont.weight = weight;
      }

      // color: color
      GetAttribute(nsHTMLAtoms::color, value);
      if (value.GetUnit() == eHTMLUnit_Color) {
        nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
        color->mColor = value.GetColorValue();
      }
      else if (value.GetUnit() == eHTMLUnit_String) {
        nsAutoString buffer;
        value.GetStringValue(buffer);
        char cbuf[40];
        buffer.ToCString(cbuf, sizeof(cbuf));

        nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
        NS_ColorNameToRGB(cbuf, &(color->mColor));
      }
      
      NS_IF_RELEASE(parentContext);
    }
    else if ((mTag == nsHTMLAtoms::div) || (mTag == nsHTMLAtoms::multicol)) {
      if (mTag == nsHTMLAtoms::div) {
        // align: enum
        GetAttribute(nsHTMLAtoms::align, value);
        if (value.GetUnit() == eHTMLUnit_Enumerated) {
          // XXX set align
        }
      }

      PRInt32 numCols = 1;
      // cols: int
      GetAttribute(nsHTMLAtoms::cols, value);
      if (value.GetUnit() == eHTMLUnit_Integer) {
        numCols = value.GetIntValue();
        // XXX
      }

      // Note: These attributes only apply when cols > 1
      if (1 < numCols) {
        // gutter: int
        GetAttribute(nsHTMLAtoms::gutter, value);
        if (value.GetUnit() == eHTMLUnit_Pixel) {
          // XXX set
        }

        // width: pixel, %
        GetAttribute(nsHTMLAtoms::width, value);
        if (value.GetUnit() == eHTMLUnit_Pixel) {
          // XXX set
        }
        else if (value.GetUnit() == eHTMLUnit_Percent) {
          // XXX set
        }
      }
    }
    else if ((mTag == nsHTMLAtoms::h1) || (mTag == nsHTMLAtoms::h2) ||
             (mTag == nsHTMLAtoms::h3) || (mTag == nsHTMLAtoms::h4) ||
             (mTag == nsHTMLAtoms::h5) || (mTag == nsHTMLAtoms::h6)) {
      // align: enum
      GetAttribute(nsHTMLAtoms::align, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) {
        // XXX set
      }
    }
    else if (mTag == nsHTMLAtoms::pre) {
      // wrap: empty
      GetAttribute(nsHTMLAtoms::wrap, value);
      if (value.GetUnit() == eHTMLUnit_Empty) {
        // XXX set
      }
      
      // variable: empty
      GetAttribute(nsHTMLAtoms::variable, value);
      if (value.GetUnit() == eHTMLUnit_Empty) {
        nsStyleFont* font = (nsStyleFont*)
          aContext->GetMutableStyleData(eStyleStruct_Font);
        font->mFont.name = "serif";
      }

      // cols: int
      GetAttribute(nsHTMLAtoms::cols, value);
      if (value.GetUnit() == eHTMLUnit_Integer) {
        // XXX set
      }

      // tabstop: int
      if (value.GetUnit() == eHTMLUnit_Integer) {
        // XXX set
      }
    }
    else if (mTag == nsHTMLAtoms::li) {
      nsStyleList* list = (nsStyleList*)aContext->GetMutableStyleData(eStyleStruct_List);

      // type: enum
      GetAttribute(nsHTMLAtoms::type, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) {
        list->mListStyleType = value.GetIntValue();
      }
    }
    else if ((mTag == nsHTMLAtoms::ul) || (mTag == nsHTMLAtoms::ol) ||
             (mTag == nsHTMLAtoms::menu) || (mTag == nsHTMLAtoms::dir)) {
      nsStyleList* list = (nsStyleList*)aContext->GetMutableStyleData(eStyleStruct_List);

      // type: enum
      GetAttribute(nsHTMLAtoms::type, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) {
        list->mListStyleType = value.GetIntValue();
      }

      // compact: empty
      GetAttribute(nsHTMLAtoms::compact, value);
      if (value.GetUnit() == eHTMLUnit_Empty) {
        // XXX set
      }
    }
    else if (mTag == nsHTMLAtoms::dl) {
      // compact: flag
      GetAttribute(nsHTMLAtoms::compact, value);
      if (value.GetUnit() == eHTMLUnit_Empty) {
        // XXX set
      }
    }
    else if (mTag == nsHTMLAtoms::body) {
      MapBackgroundAttributesInto(aContext, aPresContext);
      GetAttribute(nsHTMLAtoms::text, value);
      if (eHTMLUnit_Color == value.GetUnit()) {
        nsStyleColor* color = (nsStyleColor*)
          aContext->GetMutableStyleData(eStyleStruct_Color);
        color->mColor = value.GetColorValue();
        aPresContext->SetDefaultColor(color->mColor);
      }

      nsIHTMLDocument*  htmlDoc;
      if (NS_OK == mDocument->QueryInterface(kIHTMLDocumentIID, (void**)&htmlDoc)) {
        nsIHTMLStyleSheet* styleSheet;
        if (NS_OK == htmlDoc->GetAttributeStyleSheet(&styleSheet)) {
          GetAttribute(nsHTMLAtoms::link, value);
          if (eHTMLUnit_Color == value.GetUnit()) {
            styleSheet->SetLinkColor(value.GetColorValue());
          }

          GetAttribute(nsHTMLAtoms::alink, value);
          if (eHTMLUnit_Color == value.GetUnit()) {
            styleSheet->SetActiveLinkColor(value.GetColorValue());
          }

          GetAttribute(nsHTMLAtoms::vlink, value);
          if (eHTMLUnit_Color == value.GetUnit()) {
            styleSheet->SetVisitedLinkColor(value.GetColorValue());
          }
        }
        NS_RELEASE(htmlDoc);
      }

      // marginwidth/height get set by a special style rule

      // set up the basefont (defaults to 3)
      nsStyleFont* font = (nsStyleFont*)aContext->GetMutableStyleData(eStyleStruct_Font);
      const nsFont& defaultFont = aPresContext->GetDefaultFont(); 
      const nsFont& defaultFixedFont = aPresContext->GetDefaultFixedFont(); 
      PRInt32 scaler = aPresContext->GetFontScaler();
      float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
      font->mFont.size = nsStyleUtil::CalcFontPointSize(3, (PRInt32)defaultFont.size, scaleFactor);
      font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(3, (PRInt32)defaultFixedFont.size, scaleFactor);
    }
  }
  return NS_OK;
}


void
nsHTMLContainer::MapBackgroundAttributesInto(nsIStyleContext* aContext,
                                             nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  // background
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(nsHTMLAtoms::background, value)) {
    if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString absURLSpec;
      nsAutoString spec;
      value.GetStringValue(spec);
      if (spec.Length() > 0) {
        // Resolve url to an absolute url
        nsIURL* docURL = nsnull;
        nsIDocument* doc = mDocument;
        if (nsnull != doc) {
          docURL = doc->GetDocumentURL();
        }

        nsresult rv = NS_MakeAbsoluteURL(docURL, "", spec, absURLSpec);
        if (nsnull != docURL) {
          NS_RELEASE(docURL);
        }
        nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
        color->mBackgroundImage = absURLSpec;
        color->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
        color->mBackgroundRepeat = NS_STYLE_BG_REPEAT_XY;
      }
    }
  }

  // bgcolor
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(nsHTMLAtoms::bgcolor, value)) {
    if (eHTMLUnit_Color == value.GetUnit()) {
      nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
      color->mBackgroundColor = value.GetColorValue();
      color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
    else if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString buffer;
      value.GetStringValue(buffer);
      char cbuf[40];
      buffer.ToCString(cbuf, sizeof(cbuf));

      nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
      NS_ColorNameToRGB(cbuf, &(color->mBackgroundColor));
      color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
  }
}


// nsIDOMNode interface
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);

NS_IMETHODIMP    
nsHTMLContainer::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  NS_PRECONDITION(nsnull != aChildNodes, "null pointer");
  if (nsnull == mChildNodes) {
    mChildNodes = new nsDOMNodeList(this);
    NS_ADDREF(mChildNodes);
  }
  *aChildNodes = mChildNodes;
  NS_ADDREF(mChildNodes);
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContainer::GetHasChildNodes(PRBool* aReturn)
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
nsHTMLContainer::GetFirstChild(nsIDOMNode **aNode)
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(0);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node

    return res;
  }
  else {
    aNode = nsnull;
    return NS_OK;
  }
}

NS_IMETHODIMP    
nsHTMLContainer::GetLastChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(mChildren.Count()-1);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node

    return res;
  }
  else {
    aNode = nsnull;
    return NS_OK;
  }
}

static void
SetDocumentInChildrenOf(nsIContent* aContent, nsIDocument* aDocument)
{
  PRInt32 i, n;
  aContent->ChildCount(n);
  for (i = 0; i < n; i++) {
    nsIContent* child;
    aContent->ChildAt(i, child);
    if (nsnull != child) {
      child->SetDocument(aDocument);
      SetDocumentInChildrenOf(child, aDocument);
    }
  }
}

// XXX It's possible that newChild has already been inserted in the
// tree; if this is the case then we need to remove it from where it
// was before placing it in it's new home

NS_IMETHODIMP    
nsHTMLContainer::InsertBefore(nsIDOMNode* aNewChild, 
                              nsIDOMNode* aRefChild, 
                              nsIDOMNode** aReturn)
{
  if (nsnull == aNewChild) {
    *aReturn = nsnull;
    return NS_OK;/* XXX wrong error value */
  }

  // Get the nsIContent interface for the new content
  nsIContent* newContent = nsnull;
  nsresult res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
  NS_ASSERTION(NS_OK == res, "New child must be an nsIContent");
  if (NS_OK == res) {
    if (nsnull == aRefChild) {
      // Append the new child to the end
      SetDocumentInChildrenOf(newContent, mDocument);
      res = AppendChildTo(newContent, PR_TRUE);
    }
    else {
      // Get the index of where to insert the new child
      nsIContent* refContent = nsnull;
      res = aRefChild->QueryInterface(kIContentIID, (void**)&refContent);
      NS_ASSERTION(NS_OK == res, "Ref child must be an nsIContent");
      if (NS_OK == res) {
        PRInt32 pos;
        IndexOf(refContent, pos);
        if (pos >= 0) {
          SetDocumentInChildrenOf(newContent, mDocument);
          res = InsertChildAt(newContent, pos, PR_TRUE);
        }
        NS_RELEASE(refContent);
      }
    }
    NS_RELEASE(newContent);

    *aReturn = aNewChild;
    NS_ADDREF(aNewChild);
  }
  else {
    *aReturn = nsnull;
  }

  return res;
}

NS_IMETHODIMP    
nsHTMLContainer::ReplaceChild(nsIDOMNode* aNewChild, 
                              nsIDOMNode* aOldChild, 
                              nsIDOMNode** aReturn)
{
  nsIContent* content = nsnull;
  *aReturn = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos;
    IndexOf(content, pos);
    if (pos >= 0) {
      nsIContent* newContent = nsnull;
      nsresult res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
      NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
      if (NS_OK == res) {
        res = ReplaceChildAt(newContent, pos, PR_TRUE);
        NS_RELEASE(newContent);
      }
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }
  
  return res;
}

NS_IMETHODIMP    
nsHTMLContainer::RemoveChild(nsIDOMNode* aOldChild, 
                             nsIDOMNode** aReturn)
{
  nsIContent* content = nsnull;
  *aReturn = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos;
    IndexOf(content, pos);
    if (pos >= 0) {
      res = RemoveChildAt(pos, PR_TRUE);
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }

  return res;
}

NS_IMETHODIMP    
nsHTMLContainer::AppendChild(nsIDOMNode* aNewChild, 
                             nsIDOMNode** aReturn)
{
  return InsertBefore(aNewChild, nsnull, aReturn);
}
