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
#include "nsIDocument.h"
#include "nsIAtom.h"
#include "nsIArena.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsBlockFrame.h"
#include "nsInlineFrame.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsDOMIterator.h"
#include "nsUnitConversion.h"
#include "nsIURL.h"
#include "prprf.h"

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
}

nsHTMLContainer::nsHTMLContainer(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
}

nsHTMLContainer::~nsHTMLContainer()
{
  PRInt32 n = mChildren.Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsIContent* kid = (nsIContent*) mChildren.ElementAt(i);
    NS_RELEASE(kid);
  }
}

PRBool nsHTMLContainer::CanContainChildren() const
{
  return PR_TRUE;
}

PRInt32 nsHTMLContainer::ChildCount() const
{
  return mChildren.Count();
}

nsIContent* nsHTMLContainer::ChildAt(PRInt32 aIndex) const
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(aIndex);
  if (nsnull != child) {
    NS_ADDREF(child);
  }
  return child;
}

PRInt32 nsHTMLContainer::IndexOf(nsIContent* aPossibleChild) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");
  return mChildren.IndexOf(aPossibleChild);
}

PRBool nsHTMLContainer::InsertChildAt(nsIContent* aKid, PRInt32 aIndex)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      doc->ContentInserted(this, aKid, aIndex);
    }
  }
  return rv;
}

PRBool nsHTMLContainer::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex)
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
      doc->ContentReplaced(this, oldKid, aKid, aIndex);
    }
    oldKid->SetDocument(nsnull);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  return rv;
}

PRBool nsHTMLContainer::AppendChild(nsIContent* aKid)
{
  NS_PRECONDITION((nsnull != aKid) && (aKid != this), "null ptr");
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      doc->ContentAppended(this);
    }
  }
  return rv;
}

PRBool nsHTMLContainer::RemoveChildAt(PRInt32 aIndex)
{
  nsIContent* oldKid = (nsIContent*) mChildren.ElementAt(aIndex);
  if (nsnull != oldKid ) {
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      doc->ContentWillBeRemoved(this, oldKid, aIndex);
    }
    PRBool rv = mChildren.RemoveElementAt(aIndex);
    if (nsnull != doc) {
      doc->ContentHasBeenRemoved(this, oldKid, aIndex);
    }
    oldKid->SetDocument(nsnull);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
    return rv;
  }
  return PR_FALSE;
}

void nsHTMLContainer::Compact()
{
  //XXX I'll turn this on in a bit... mChildren.Compact();
}

nsresult
nsHTMLContainer::CreateFrame(nsIPresContext* aPresContext,
                             nsIFrame* aParentFrame,
                             nsIStyleContext* aStyleContext,
                             nsIFrame*& aResult)
{
  nsStyleDisplay* styleDisplay =
    (nsStyleDisplay*) aStyleContext->GetData(eStyleStruct_Display);

  // Use style to choose what kind of frame to create
  nsIFrame* frame = nsnull;
  nsresult rv;
  switch (styleDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
    rv = nsBlockFrame::NewFrame(&frame, this, aParentFrame);
    break;

  case NS_STYLE_DISPLAY_INLINE:
    rv = nsInlineFrame::NewFrame(&frame, this, aParentFrame);
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

void nsHTMLContainer::SetAttribute(nsIAtom* aAttribute,
                                   const nsString& aValue)
{
  // Special handling code for various html container attributes; note
  // that if an attribute doesn't require special handling then we
  // fall through and use the default base class implementation.
  nsHTMLValue val;

  // Check for attributes common to most html containers
  if (aAttribute == nsHTMLAtoms::dir) {
    if (ParseEnumValue(aValue, kDirTable, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
    return;
  }
  if (aAttribute == nsHTMLAtoms::lang) {
    nsHTMLTagContent::SetAttribute(aAttribute, aValue);
    return;
  }

  if (mTag == nsHTMLAtoms::p) {
    if ((aAttribute == nsHTMLAtoms::align) &&
        ParseDivAlignParam(aValue, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }
  else if (mTag == nsHTMLAtoms::a) {
    if (aAttribute == nsHTMLAtoms::href) {
      nsAutoString href(aValue);
      href.StripWhitespace();
      nsHTMLTagContent::SetAttribute(aAttribute, href);
      return;
    }
    if (aAttribute == nsHTMLAtoms::suppress) {
      if (aValue.EqualsIgnoreCase("true")) {
        nsHTMLValue val;
        val.SetEmptyValue();
        nsHTMLTagContent::SetAttribute(aAttribute, val);
        return;
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
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
    if (aAttribute == nsHTMLAtoms::color) {
      ParseColor(aValue, val);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }
  else if ((mTag == nsHTMLAtoms::div) || (mTag == nsHTMLAtoms::multicol)) {
    if ((mTag == nsHTMLAtoms::div) && (aAttribute == nsHTMLAtoms::align) &&
        ParseDivAlignParam(aValue, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
    if (aAttribute == nsHTMLAtoms::cols) {
      ParseValue(aValue, 0, val, eHTMLUnit_Integer);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }

    // Note: These attributes only apply when cols > 1
    if (aAttribute == nsHTMLAtoms::gutter) {
      ParseValue(aValue, 1, val, eHTMLUnit_Pixel);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
    if (aAttribute == nsHTMLAtoms::width) {
      ParseValueOrPercent(aValue, val, eHTMLUnit_Pixel);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }
  else if ((mTag == nsHTMLAtoms::h1) || (mTag == nsHTMLAtoms::h2) ||
           (mTag == nsHTMLAtoms::h3) || (mTag == nsHTMLAtoms::h4) ||
           (mTag == nsHTMLAtoms::h5) || (mTag == nsHTMLAtoms::h6)) {
    if ((aAttribute == nsHTMLAtoms::align) &&
        ParseDivAlignParam(aValue, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }
  else if (mTag == nsHTMLAtoms::pre) {
    if ((aAttribute == nsHTMLAtoms::wrap) ||
        (aAttribute == nsHTMLAtoms::variable)) {
      val.SetEmptyValue();
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
    if (aAttribute == nsHTMLAtoms::cols) {
      ParseValue(aValue, 0, val, eHTMLUnit_Integer);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
    if (aAttribute == nsHTMLAtoms::tabstop) {
      PRInt32 ec, tabstop = aValue.ToInteger(&ec);
      if (tabstop <= 0) {
        tabstop = 8;
      }
      val.SetIntValue(tabstop, eHTMLUnit_Integer);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }
  else if (mTag == nsHTMLAtoms::li) {
    if (aAttribute == nsHTMLAtoms::type) {
      if (ParseEnumValue(aValue, kListItemTypeTable, val)) {
        nsHTMLTagContent::SetAttribute(aAttribute, val);
        return;
      }
      // Illegal type values are left as is for the dom
    }
    if (aAttribute == nsHTMLAtoms::value) {
      ParseValue(aValue, 1, val, eHTMLUnit_Integer);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }
  else if ((mTag == nsHTMLAtoms::ul) || (mTag == nsHTMLAtoms::ol) ||
           (mTag == nsHTMLAtoms::menu) || (mTag == nsHTMLAtoms::dir)) {
    if (aAttribute == nsHTMLAtoms::type) {
      if (!ParseEnumValue(aValue, kListTypeTable, val)) {
        val.SetIntValue(NS_STYLE_LIST_STYLE_BASIC, eHTMLUnit_Enumerated);
      }
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
    if (aAttribute == nsHTMLAtoms::start) {
      ParseValue(aValue, 1, val, eHTMLUnit_Integer);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
    if (aAttribute == nsHTMLAtoms::compact) {
      val.SetEmptyValue();
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }
  else if (mTag == nsHTMLAtoms::dl) {
    if (aAttribute == nsHTMLAtoms::compact) {
      val.SetEmptyValue();
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }
  else if (mTag == nsHTMLAtoms::body) {
    if (aAttribute == nsHTMLAtoms::background) {
      nsAutoString href(aValue);
      href.StripWhitespace();
      nsHTMLTagContent::SetAttribute(aAttribute, href);
      return;
    }
    if (aAttribute == nsHTMLAtoms::bgcolor) {
      ParseColor(aValue, val);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
      return;
    }
  }

  // Use default attribute catching code
  nsHTMLTagContent::SetAttribute(aAttribute, aValue);
}

nsContentAttr nsHTMLContainer::AttributeToString(nsIAtom* aAttribute,
                                                 nsHTMLValue& aValue,
                                                 nsString& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
    if (aAttribute == nsHTMLAtoms::align) {
      DivAlignParamToString(aValue, aResult);
      ca = eContentAttr_HasValue;
    }
    else if (mTag == nsHTMLAtoms::li) {
      if (aAttribute == nsHTMLAtoms::type) {
        EnumValueToString(aValue, kListItemTypeTable, aResult);
        ca = eContentAttr_HasValue;
      }
    }
    else if ((mTag == nsHTMLAtoms::ul) || (mTag == nsHTMLAtoms::ol) ||
             (mTag == nsHTMLAtoms::menu) || (mTag == nsHTMLAtoms::dir)) {
      if (aAttribute == nsHTMLAtoms::type) {
        EnumValueToString(aValue, kListTypeTable, aResult);
        ca = eContentAttr_HasValue;
      }
    }
    else if (mTag == nsHTMLAtoms::font) {
      if ((aAttribute == nsHTMLAtoms::size) ||
          (aAttribute == nsHTMLAtoms::pointSize) ||
          (aAttribute == nsHTMLAtoms::fontWeight)) {
        aResult.Truncate();
        aResult.Append(aValue.GetIntValue(), 10);
        ca = eContentAttr_HasValue;
      }
    }
  }
  return ca;
}

void nsHTMLContainer::MapAttributesInto(nsIStyleContext* aContext, 
                                        nsIPresContext* aPresContext)
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;

    // Check for attributes common to most html containers
    GetAttribute(nsHTMLAtoms::dir, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      nsStyleDisplay* display = (nsStyleDisplay*)
        aContext->GetData(eStyleStruct_Display);
      display->mDirection = value.GetIntValue();
    }

    if (mTag == nsHTMLAtoms::p) {
      // align: enum
      GetAttribute(nsHTMLAtoms::align, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) {
        nsStyleText* text = (nsStyleText*)aContext->GetData(eStyleStruct_Text);
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
      nsStyleFont* font = (nsStyleFont*)aContext->GetData(eStyleStruct_Font);
      nsStyleFont* parentFont = font;
      nsIStyleContext* parentContext = aContext->GetParent();
      if (nsnull != parentContext) {
        parentFont = (nsStyleFont*)parentContext->GetData(eStyleStruct_Font);
      }

      // face: string list
      GetAttribute(nsHTMLAtoms::face, value);
      if (value.GetUnit() == eHTMLUnit_String) {
        nsAutoString familyList;
        value.GetStringValue(familyList);
        // XXX needs font support to determine usable fonts
        // parse up the string & remove the quotes
        // XXX only does first until we can tell what are installed fonts
        nsAutoString family;
        PRInt32 index = familyList.Find(PRUnichar(','));
        if (-1 < index) {
          familyList.Left(family, index);
        }
        else {
          family.Append(familyList);
        }
        family.StripChars("\"");
        family.StripWhitespace();

        font->mFont.name = family;
      }

      // pointSize: int, enum
      GetAttribute(nsHTMLAtoms::pointSize, value);
      if (value.GetUnit() == eHTMLUnit_Integer) {
        // XXX should probably sanitize value
        font->mFont.size = parentFont->mFont.size + NS_POINTS_TO_TWIPS_INT(value.GetIntValue());
      }
      else if (value.GetUnit() == eHTMLUnit_Enumerated) {
        font->mFont.size = NS_POINTS_TO_TWIPS_INT(value.GetIntValue());
      }
      else {
        // size: int, enum
        GetAttribute(nsHTMLAtoms::size, value);
        if ((value.GetUnit() == eHTMLUnit_Integer) || (value.GetUnit() == eHTMLUnit_Enumerated)) { 
          static float kFontScale[7] = {
            0.7f,
            0.8f,
            1.0f,
            1.2f,
            1.5f,
            2.0f,
            3.0f
          };
          PRInt32 size = value.GetIntValue();
        
          const nsFont& normal = aPresContext->GetDefaultFont();  // XXX should be BASEFONT

          if (value.GetUnit() == eHTMLUnit_Enumerated) {
            size = ((0 < size) ? ((size < 8) ? size : 7) : 0); 
            font->mFont.size = (nscoord)((float)normal.size * kFontScale[size - 1]);
          }
          else {  // int (+/-)
            if ((0 < size) && (size <= 7)) { // +
              PRInt32 index;
              for (index = 0; index < 6; index++)
                if (parentFont->mFont.size < (nscoord)((float)normal.size * kFontScale[index]))
                  break;
              size = ((index - 1) + size);
              if (7 < size) size = 7;
              font->mFont.size = (nscoord)((float)normal.size * kFontScale[size]);
            }
            else if ((-7 <= size) && (size < 0)) {
              PRInt32 index;
              for (index = 6; index > 0; index--)
                if (parentFont->mFont.size > (nscoord)((float)normal.size * kFontScale[index]))
                  break;
              size = ((index + 1) + size);
              if (size < 0) size = 0;
              font->mFont.size = (nscoord)((float)normal.size * kFontScale[size]);
            }
            else if (0 == size) {
              font->mFont.size = parentFont->mFont.size;
            }
          }
        }
      }

      // fontWeight: int, enum
      GetAttribute(nsHTMLAtoms::fontWeight, value);
      if (value.GetUnit() == eHTMLUnit_Integer) { // +/-
        PRInt32 weight = parentFont->mFont.weight + value.GetIntValue();
        font->mFont.weight = ((100 < weight) ? ((weight < 700) ? weight : 700) : 100);
      }
      else if (value.GetUnit() == eHTMLUnit_Enumerated) {
        PRInt32 weight = value.GetIntValue();
        weight = ((100 < weight) ? ((weight < 700) ? weight : 700) : 100);
        font->mFont.weight = weight;
      }

      // color: color
      GetAttribute(nsHTMLAtoms::color, value);
      if (value.GetUnit() == eHTMLUnit_Color) {
        nsStyleColor* color = (nsStyleColor*)aContext->GetData(eStyleStruct_Color);
        color->mColor = value.GetColorValue();
      }
      else if (value.GetUnit() == eHTMLUnit_String) {
        nsAutoString buffer;
        value.GetStringValue(buffer);
        char cbuf[40];
        buffer.ToCString(cbuf, sizeof(cbuf));

        nsStyleColor* color = (nsStyleColor*)aContext->GetData(eStyleStruct_Color);
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
        // XXX set
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
      nsStyleList* list = (nsStyleList*)aContext->GetData(eStyleStruct_List);

      // type: enum
      GetAttribute(nsHTMLAtoms::type, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) {
        list->mListStyleType = value.GetIntValue();
      }
    }
    else if ((mTag == nsHTMLAtoms::ul) || (mTag == nsHTMLAtoms::ol) ||
             (mTag == nsHTMLAtoms::menu) || (mTag == nsHTMLAtoms::dir)) {
      nsStyleList* list = (nsStyleList*)aContext->GetData(eStyleStruct_List);

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
    }
  }
}


void
nsHTMLContainer::MapBackgroundAttributesInto(nsIStyleContext* aContext,
                                             nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  // background
  if (eContentAttr_HasValue == GetAttribute(nsHTMLAtoms::background, value)) {
    if (eHTMLUnit_String == value.GetUnit()) {
      // Resolve url to an absolute url
      nsIURL* docURL = nsnull;
      nsIDocument* doc = mDocument;
      if (nsnull != doc) {
        docURL = doc->GetDocumentURL();
      }

      nsAutoString absURLSpec;
      nsAutoString spec;
      value.GetStringValue(spec);
      nsresult rv = NS_MakeAbsoluteURL(docURL, "", spec, absURLSpec);
      if (nsnull != docURL) {
        NS_RELEASE(docURL);
      }
      nsStyleColor* color = (nsStyleColor*)aContext->GetData(eStyleStruct_Color);
      color->mBackgroundImage = absURLSpec;
      color->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
      color->mBackgroundRepeat = NS_STYLE_BG_REPEAT_XY;
    }
  }

  // bgcolor
  if (eContentAttr_HasValue == GetAttribute(nsHTMLAtoms::bgcolor, value)) {
    if (eHTMLUnit_Color == value.GetUnit()) {
      nsStyleColor* color = (nsStyleColor*)aContext->GetData(eStyleStruct_Color);
      color->mBackgroundColor = value.GetColorValue();
      color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
    else if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString buffer;
      value.GetStringValue(buffer);
      char cbuf[40];
      buffer.ToCString(cbuf, sizeof(cbuf));

      nsStyleColor* color = (nsStyleColor*)aContext->GetData(eStyleStruct_Color);
      NS_ColorNameToRGB(cbuf, &(color->mBackgroundColor));
      color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
  }
}


// nsIDOMNode interface
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);

nsresult nsHTMLContainer::GetChildNodes(nsIDOMNodeIterator **aIterator)
{
  NS_PRECONDITION(nsnull != aIterator, "null pointer");
  *aIterator = new nsDOMIterator(*this);
  return NS_OK;
}

nsresult nsHTMLContainer::HasChildNodes()
{
  if (0 != mChildren.Count()) {
    return NS_OK;
  } 
  else {
    return NS_ERROR_FAILURE;
  }
}

nsresult nsHTMLContainer::GetFirstChild(nsIDOMNode **aNode)
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(0);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node

    return res;
  }

  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContainer::InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild)
{
  NS_PRECONDITION(nsnull != newChild, "Null new child");

  // Get the nsIContent interface for the new content
  nsIContent* newContent = nsnull;
  nsresult    res = newChild->QueryInterface(kIContentIID, (void**)&newContent);
  NS_ASSERTION(NS_OK == res, "New child must be an nsIContent");

  if (NS_OK == res) {
    if (nsnull == refChild) {
      // Append the new child to the end
      if (PR_FALSE == AppendChild(newContent)) {
        res = NS_ERROR_FAILURE;
      }
    } else {
      nsIContent* content = nsnull;

      // Get the index of where to insert the new child
      res = refChild->QueryInterface(kIContentIID, (void**)&content);
      NS_ASSERTION(NS_OK == res, "Ref child must be an nsIContent");
      if (NS_OK == res) {
        PRInt32 pos = IndexOf(content);
        if (pos >= 0) {
          if (PR_FALSE == InsertChildAt(newContent, pos)) {
            res = NS_ERROR_FAILURE;
          }
        }
        NS_RELEASE(content);
      }
    }

    NS_RELEASE(newContent);
  }

  return res;
}

nsresult nsHTMLContainer::ReplaceChild(nsIDOMNode *newChild, 
                                        nsIDOMNode *oldChild)
{
  nsIContent* content = nsnull;
  nsresult res = oldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos = IndexOf(content);
    if (pos >= 0) {
      nsIContent* newContent = nsnull;
      nsresult res = newChild->QueryInterface(kIContentIID, (void**)&newContent);
      NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
      if (NS_OK == res) {
        if (PR_FALSE == ReplaceChildAt(newContent, pos)) {
          res = NS_ERROR_FAILURE;
        }
        NS_RELEASE(newContent);
      }
    }
    NS_RELEASE(content);
  }

  return res;
}

nsresult nsHTMLContainer::RemoveChild(nsIDOMNode *oldChild)
{
  nsIContent* content = nsnull;
  nsresult res = oldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos = IndexOf(content);
    if (pos >= 0) {
      if (PR_TRUE == RemoveChildAt(pos)) {
        res = NS_ERROR_FAILURE;
      }
    }
    NS_RELEASE(content);
  }

  return res;
}

