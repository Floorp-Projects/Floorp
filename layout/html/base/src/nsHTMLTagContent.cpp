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
#include "nsHTMLTagContent.h"
#include "nsIDocument.h"
#include "nsIHTMLAttributes.h"
#include "nsIStyleRule.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "prprf.h"
#include "nsDOMAttributes.h"
#include "nsICSSParser.h"

static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIScriptObjectOwner, NS_ISCRIPTOBJECTOWNER_IID);

nsHTMLTagContent::nsHTMLTagContent()
{
}

nsHTMLTagContent::nsHTMLTagContent(nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aTag, "null ptr");
  mTag = aTag;
  NS_IF_ADDREF(mTag);
}

nsHTMLTagContent::~nsHTMLTagContent()
{
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mAttributes);
}

nsresult nsHTMLTagContent::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult res = nsHTMLContent::QueryInterface(aIID, aInstancePtr); 
  if (NS_NOINTERFACE == res) {
    if (aIID.Equals(kIDOMElementIID)) {
      *aInstancePtr = (void*)(nsIDOMElement*)this;
      AddRef();
      return NS_OK;
    }
  }

  return res;
}

nsrefcnt nsHTMLTagContent::AddRef(void)
{
  return nsHTMLContent::AddRef(); 
}

nsrefcnt nsHTMLTagContent::Release(void)
{
  return nsHTMLContent::Release(); 
}

nsIAtom* nsHTMLTagContent::GetTag() const
{
  NS_IF_ADDREF(mTag);
  return mTag;
}

void nsHTMLTagContent::ToHTMLString(nsString& aBuf) const
{
  aBuf.Truncate(0);
  aBuf.Append('<');

  nsIAtom* tag = GetTag();
  if (nsnull != tag) {
    nsAutoString tmp;
    tag->ToString(tmp);
    aBuf.Append(tmp);
  } else {
    aBuf.Append("?NULL");
  }
  aBuf.Append('>');
}

void nsHTMLTagContent::SetAttribute(const nsString& aName,
                                    const nsString& aValue)
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  SetAttribute(attr, aValue);
  NS_RELEASE(attr);
}

nsContentAttr nsHTMLTagContent::GetAttribute(const nsString& aName,
                                             nsString& aResult) const
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);

  nsHTMLValue value;
  nsContentAttr result = GetAttribute(attr, value);

  char cbuf[20];
  nscolor color;
  if (eContentAttr_HasValue == result) {
    // Try subclass conversion routine first
    if (eContentAttr_HasValue == AttributeToString(attr, value, aResult)) {
      return result;
    }

    // Provide default conversions for most everything
    switch (value.GetUnit()) {
    case eHTMLUnit_String:
    case eHTMLUnit_Null:
      value.GetStringValue(aResult);
      break;

    case eHTMLUnit_Integer:
      aResult.Truncate();
      aResult.Append(value.GetIntValue(), 10);
      break;

    case eHTMLUnit_Pixel:
      aResult.Truncate();
      aResult.Append(value.GetPixelValue(), 10);
      break;

    case eHTMLUnit_Percent:
      aResult.Truncate(0);
      aResult.Append(PRInt32(value.GetPercentValue() * 100.0f), 10);
      break;

    case eHTMLUnit_Color:
      color = nscolor(value.GetColorValue());
      PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x",
                  NS_GET_R(color), NS_GET_G(color), NS_GET_B(color));
      aResult.Truncate(0);
      aResult.Append(cbuf);
      break;

    case eHTMLUnit_Enumerated:
      NS_NOTREACHED("no default enumerated value to string conversion");
      result = eContentAttr_NotThere;
      break;
    }
  }

  NS_RELEASE(attr);
  return result;
}

nsContentAttr nsHTMLTagContent::AttributeToString(nsIAtom* aAttribute,
                                                  nsHTMLValue& aValue,
                                                  nsString& aResult) const
{
  aResult.Truncate();
  return eContentAttr_NotThere;
}

// Note: Subclasses should override to parse the value string; in
// addition, when they see an unknown attribute they should call this
// so that global attributes are handled (like CLASS, ID, STYLE, etc.)
void nsHTMLTagContent::SetAttribute(nsIAtom* aAttribute,
                                    const nsString& aValue)
{
  if (nsnull == mAttributes) {
    NS_NewHTMLAttributes(&mAttributes, this);
  }
  if (nsnull != mAttributes) {
    if (nsHTMLAtoms::style == aAttribute) {
      // XXX the style sheet language is a document property that
      // should be used to lookup the style sheet parser to parse the
      // attribute.
      nsICSSParser* css;
      nsresult rv = NS_NewCSSParser(&css);
      if (NS_OK != rv) {
        return;
      }
      nsIStyleRule* rule;
      rv = css->ParseDeclarations(aValue, nsnull, rule);
      if ((NS_OK == rv) && (nsnull != rule)) {
        printf("XXX STYLE= discarded: ");
        rule->List();
        NS_RELEASE(rule);
      }
      NS_RELEASE(css);
    }
    else {
      mAttributes->SetAttribute(aAttribute, aValue);
    }
  }
}

void nsHTMLTagContent::SetAttribute(nsIAtom* aAttribute,
                                    const nsHTMLValue& aValue)
{
  if (nsnull == mAttributes) {
    NS_NewHTMLAttributes(&mAttributes, this);
  }
  if (nsnull != mAttributes) {
    mAttributes->SetAttribute(aAttribute, aValue);
  }
}

void nsHTMLTagContent::UnsetAttribute(nsIAtom* aAttribute)
{
  if (nsnull != mAttributes) {
    PRInt32 count = mAttributes->UnsetAttribute(aAttribute);
    if (0 == count) {
      NS_RELEASE(mAttributes);
    }
  }
}

nsContentAttr nsHTMLTagContent::GetAttribute(nsIAtom* aAttribute,
                                             nsHTMLValue& aValue) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAttribute(aAttribute, aValue);
  }
  aValue.Reset();
  return eContentAttr_NotThere;
}

PRInt32 nsHTMLTagContent::GetAllAttributeNames(nsISupportsArray* aArray) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAllAttributeNames(aArray);
  }
  return 0;
}

PRInt32 nsHTMLTagContent::GetAttributeCount(void) const
{
  if (nsnull != mAttributes) {
    return mAttributes->Count();
  }
  return 0;
}

void nsHTMLTagContent::SetID(nsIAtom* aID)
{
  if (nsnull != aID) {
    if (nsnull == mAttributes) {
      NS_NewHTMLAttributes(&mAttributes, this);
    }
    if (nsnull != mAttributes) {
      mAttributes->SetID(aID);
    }
  }
  else {
    if (nsnull != mAttributes) {
      PRInt32 count = mAttributes->SetID(nsnull);
      if (0 == count) {
        NS_RELEASE(mAttributes);
      }
    }
  }
}

nsIAtom* nsHTMLTagContent::GetID(void) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetID();
  }
  return nsnull;
}

void nsHTMLTagContent::SetClass(nsIAtom* aClass)
{
  if (nsnull != aClass) {
    if (nsnull == mAttributes) {
      NS_NewHTMLAttributes(&mAttributes, this);
    }
    if (nsnull != mAttributes) {
      mAttributes->SetClass(aClass);
    }
  }
  else {
    if (nsnull != mAttributes) {
      PRInt32 count = mAttributes->SetClass(nsnull);
      if (0 == count) {
        NS_RELEASE(mAttributes);
      }
    }
  }
}

nsIAtom* nsHTMLTagContent::GetClass(void) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetClass();
  }
  return nsnull;
}


nsIStyleRule* nsHTMLTagContent::GetStyleRule(void)
{
  nsIStyleRule* result = nsnull;

  if (nsnull != mAttributes) {
    mAttributes->QueryInterface(kIStyleRuleIID, (void**)&result);
  }
  return result;
}

nsresult nsHTMLTagContent::GetScriptObject(JSContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    *aScriptObject = nsnull;
    if (nsnull != mParent) {
      nsIScriptObjectOwner *parent;
      if (NS_OK == mParent->QueryInterface(kIScriptObjectOwner, (void**)&parent)) {
        parent->GetScriptObject(aContext, aScriptObject);
        NS_RELEASE(parent);
      }
    }
    res = NS_NewScriptElement(aContext, this, (JSObject*)*aScriptObject, (JSObject**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult nsHTMLTagContent::GetNodeType(PRInt32 *aType)
{
  *aType = nsHTMLContent::ELEMENT;
  return NS_OK;
}

nsresult nsHTMLTagContent::GetParentNode(nsIDOMNode **aNode)
{
  return nsHTMLContent::GetParentNode(aNode);
}

nsresult nsHTMLTagContent::GetChildNodes(nsIDOMNodeIterator **aIterator)
{
  return nsHTMLContent::GetChildNodes(aIterator);
}

nsresult nsHTMLTagContent::HasChildNodes()
{
  return nsHTMLContent::HasChildNodes();
}

nsresult nsHTMLTagContent::GetFirstChild(nsIDOMNode **aNode)
{
  return nsHTMLContent::GetFirstChild(aNode);
}

nsresult nsHTMLTagContent::GetPreviousSibling(nsIDOMNode **aNode)
{
  return nsHTMLContent::GetPreviousSibling(aNode);
}

nsresult nsHTMLTagContent::GetNextSibling(nsIDOMNode **aNode)
{
  return nsHTMLContent::GetNextSibling(aNode);
}

nsresult nsHTMLTagContent::InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild)
{
  return nsHTMLContent::InsertBefore(newChild, refChild);
}

nsresult nsHTMLTagContent::ReplaceChild(nsIDOMNode *newChild, nsIDOMNode *oldChild)
{
  return nsHTMLContent::ReplaceChild(newChild, oldChild);
}

nsresult nsHTMLTagContent::RemoveChild(nsIDOMNode *oldChild)
{
  return nsHTMLContent::RemoveChild(oldChild);
}

nsresult nsHTMLTagContent::GetTagName(nsString &aName)
{
  NS_ASSERTION(nsnull != mTag, "no tag");
  mTag->ToString(aName);
  return NS_OK;
}

nsresult nsHTMLTagContent::GetAttributes(nsIDOMAttributeList **aAttributeList)
{
  NS_PRECONDITION(nsnull != aAttributeList, "null pointer argument");
  if (nsnull != mAttributes) {
    *aAttributeList = new nsDOMAttributeList(*this);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult nsHTMLTagContent::GetDOMAttribute(nsString &aName, nsString &aValue)
{
  GetAttribute(aName, aValue);
  return NS_OK;
}

nsresult nsHTMLTagContent::SetDOMAttribute(nsString &aName, nsString &aValue)
{
  SetAttribute(aName, aValue);
  return NS_OK;
}

nsresult nsHTMLTagContent::RemoveAttribute(nsString &aName)
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  UnsetAttribute(attr);
  return NS_OK;
}

nsresult nsHTMLTagContent::GetAttributeNode(nsString &aName, nsIDOMAttribute **aAttribute)
{
  nsAutoString value;
  if(eContentAttr_NotThere != GetAttribute(aName, value)) {
    *aAttribute = new nsDOMAttribute(aName, value);
  }

  return NS_OK;
}

nsresult nsHTMLTagContent::SetAttributeNode(nsIDOMAttribute *aAttribute)
{
  NS_PRECONDITION(nsnull != aAttribute, "null attribute");
  
  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != aAttribute) {
    nsString name, value;
    res = aAttribute->GetName(name);
    if (NS_OK == res) {
      res = aAttribute->GetValue(value);
      if (NS_OK == res) {
        SetAttribute(name, value);
      }
    }
  }

  return res;
}

nsresult nsHTMLTagContent::RemoveAttributeNode(nsIDOMAttribute *aAttribute)
{
  NS_PRECONDITION(nsnull != aAttribute, "null attribute");
  
  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != aAttribute) {
    nsAutoString name;
    res = aAttribute->GetName(name);
    if (NS_OK == res) {
      nsAutoString upper;
      name.ToUpperCase(upper);
      nsIAtom* attr = NS_NewAtom(upper);
      UnsetAttribute(attr);
    }
  }

  return res;
}

nsresult nsHTMLTagContent::Normalize()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsHTMLTagContent::GetElementsByTagName(nsString &aName,nsIDOMNodeIterator **aIterator)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------

// Attribute parsing utility methods

PRBool nsHTMLTagContent::ParseEnumValue(const nsString& aValue,
                                        EnumTable* aTable,
                                        nsHTMLValue& aResult)
{
  while (nsnull != aTable->tag) {
    if (aValue.EqualsIgnoreCase(aTable->tag)) {
      aResult.SetIntValue(aTable->value, eHTMLUnit_Enumerated);
      return PR_TRUE;
    }
    aTable++;
  }
  return PR_FALSE;
}

PRBool nsHTMLTagContent::EnumValueToString(const nsHTMLValue& aValue,
                                           EnumTable* aTable,
                                           nsString& aResult)
{
  aResult.Truncate(0);
  if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
    PRInt32 v = aValue.GetIntValue();
    while (nsnull != aTable->tag) {
      if (aTable->value == v) {
        aResult.Append(aTable->tag);
        return PR_TRUE;
      }
      aTable++;
    }
  }
  return PR_FALSE;
}

// XXX check all mappings against ebina's usage
static nsHTMLTagContent::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "texttop", NS_STYLE_VERTICAL_ALIGN_TEXT_TOP },
  { "baseline", NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "bottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "top", NS_STYLE_VERTICAL_ALIGN_TOP },
  { "middle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "absbottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "abscenter", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "absmiddle", NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { 0 }
};

PRBool nsHTMLTagContent::ParseAlignParam(const nsString& aString,
                                       nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kAlignTable, aResult);
}

PRBool nsHTMLTagContent::AlignParamToString(const nsHTMLValue& aValue,
                                            nsString& aResult)
{
  return EnumValueToString(aValue, kAlignTable, aResult);
}

static nsHTMLTagContent::EnumTable kDivAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "justify", NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

PRBool nsHTMLTagContent::ParseDivAlignParam(const nsString& aString,
                                            nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kDivAlignTable, aResult);
}

PRBool nsHTMLTagContent::DivAlignParamToString(const nsHTMLValue& aValue,
                                               nsString& aResult)
{
  return EnumValueToString(aValue, kDivAlignTable, aResult);
}

static nsHTMLTagContent::EnumTable kTableAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

PRBool nsHTMLTagContent::ParseTableAlignParam(const nsString& aString,
                                              nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kTableAlignTable, aResult);
}

PRBool nsHTMLTagContent::TableAlignParamToString(const nsHTMLValue& aValue,
                                                 nsString& aResult)
{
  return EnumValueToString(aValue, kTableAlignTable, aResult);
}

void nsHTMLTagContent::ParseValueOrPercent(const nsString& aString,
                                           nsHTMLValue& aResult, 
                                           nsHTMLUnit aValueUnit)
{ // XXX should vave min/max values?
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (tmp.Last() == '%') {/* XXX not 100% compatible with ebina's code */
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    aResult.SetPercentValue(float(val)/100.0f);
  } else {
    if (eHTMLUnit_Pixel == aValueUnit) {
      aResult.SetPixelValue(val);
    }
    else {
      aResult.SetIntValue(val, aValueUnit);
    }
  }
}

/* used to parser attribute values that could be either:
 *   integer  (n), 
 *   percent  (n%),
 *   or proportional (n*)
 */
void nsHTMLTagContent::ParseValueOrPercentOrProportional(const nsString& aString,
    																										 nsHTMLValue& aResult, 
																												 nsHTMLUnit aValueUnit)
{ // XXX should have min/max values?
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (tmp.Last() == '%') {/* XXX not 100% compatible with ebina's code */
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    aResult.SetPercentValue(float(val)/100.0f);
	} else if (tmp.Last() == '*') {
    if (val < 0) val = 0;
    aResult.SetIntValue(val, eHTMLUnit_Proportional);	// proportional values are integers
  } else {
    if (eHTMLUnit_Pixel == aValueUnit) {
      aResult.SetPixelValue(val);
    }
    else {
      aResult.SetIntValue(val, aValueUnit);
    }
  }
}

PRBool nsHTMLTagContent::ValueOrPercentToString(const nsHTMLValue& aValue,
                                                nsString& aResult)
{
  aResult.Truncate(0);
  switch (aValue.GetUnit()) {
  case eHTMLUnit_Integer:
    aResult.Append(aValue.GetIntValue(), 10);
    return PR_TRUE;
  case eHTMLUnit_Pixel:
    aResult.Append(aValue.GetPixelValue(), 10);
    return PR_TRUE;
  case eHTMLUnit_Percent:
    aResult.Append(PRInt32(aValue.GetPercentValue() * 100.0f), 10);
    aResult.Append('%');
    return PR_TRUE;
  }
  return PR_FALSE;
}

void nsHTMLTagContent::ParseValue(const nsString& aString, PRInt32 aMin,
                                  nsHTMLValue& aResult, nsHTMLUnit aValueUnit)
{
  PRInt32 ec, val = aString.ToInteger(&ec);
  if (val < aMin) val = aMin;
  if (eHTMLUnit_Pixel == aValueUnit) {
    aResult.SetPixelValue(val);
  }
  else {
    aResult.SetIntValue(val, aValueUnit);
  }
}

void nsHTMLTagContent::ParseValue(const nsString& aString, PRInt32 aMin,
                                  PRInt32 aMax,
                                  nsHTMLValue& aResult, nsHTMLUnit aValueUnit)
{
  PRInt32 ec, val = aString.ToInteger(&ec);
  if (val < aMin) val = aMin;
  if (val > aMax) val = aMax;
  if (eHTMLUnit_Pixel == aValueUnit) {
    aResult.SetPixelValue(val);
  }
  else {
    aResult.SetIntValue(val, aValueUnit);
  }
}

PRBool nsHTMLTagContent::ParseImageProperty(nsIAtom* aAttribute,
                                            const nsString& aString,
                                            nsHTMLValue& aResult)
{
  if ((aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height)) {
    ParseValueOrPercent(aString, aResult, eHTMLUnit_Pixel);
    return PR_TRUE;
  }
  else if ((aAttribute == nsHTMLAtoms::hspace) ||
           (aAttribute == nsHTMLAtoms::vspace) ||
           (aAttribute == nsHTMLAtoms::border)) {
    ParseValue(aString, 0, aResult, eHTMLUnit_Pixel);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool nsHTMLTagContent::ImagePropertyToString(nsIAtom* aAttribute,
                                               const nsHTMLValue& aValue,
                                               nsString& aResult)
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

void
nsHTMLTagContent::MapImagePropertiesInto(nsIStyleContext* aContext, 
                                         nsIPresContext* aPresContext)
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;

    float p2t = aPresContext->GetPixelsToTwips();
    nsStylePosition* pos = (nsStylePosition*)
      aContext->GetData(eStyleStruct_Position);

    // width: value
    GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = nscoord(p2t * value.GetPixelValue());
      pos->mWidth.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // height: value
    GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = nscoord(p2t * value.GetPixelValue());
      pos->mHeight.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mHeight.SetPercentValue(value.GetPercentValue());
    }
  }
}

void
nsHTMLTagContent::MapImageBorderInto(nsIStyleContext* aContext, 
                                     nsIPresContext* aPresContext,
                                     nscolor aBorderColors[4])
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;

    // border: pixels
    GetAttribute(nsHTMLAtoms::border, value);
    if (value.GetUnit() != eHTMLUnit_Pixel) {
      if (nsnull == aBorderColors) {
        return;
      }
      // If no border is defined and we are forcing a border, force
      // the size to 2 pixels.
      value.SetPixelValue(2);
    }

    float p2t = aPresContext->GetPixelsToTwips();
    nscoord twips = nscoord(p2t * value.GetPixelValue());

    // Fixup border-padding sums: subtract out the old size and then
    // add in the new size.
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetData(eStyleStruct_Spacing);
    nsStyleCoord coord;
    coord.SetCoordValue(twips);
    spacing->mBorder.SetTop(coord);
    spacing->mBorder.SetRight(coord);
    spacing->mBorder.SetBottom(coord);
    spacing->mBorder.SetLeft(coord);
    spacing->mBorderStyle[0] = NS_STYLE_BORDER_STYLE_SOLID;
    spacing->mBorderStyle[1] = NS_STYLE_BORDER_STYLE_SOLID;
    spacing->mBorderStyle[2] = NS_STYLE_BORDER_STYLE_SOLID;
    spacing->mBorderStyle[3] = NS_STYLE_BORDER_STYLE_SOLID;

    // Use supplied colors if provided, otherwise use color for border
    // color
    if (nsnull != aBorderColors) {
      spacing->mBorderColor[0] = aBorderColors[0];
      spacing->mBorderColor[1] = aBorderColors[1];
      spacing->mBorderColor[2] = aBorderColors[2];
      spacing->mBorderColor[3] = aBorderColors[3];
    }
    else {
      // Color is inherited from "color"
      nsStyleColor* styleColor = (nsStyleColor*)
        aContext->GetData(eStyleStruct_Color);
      nscolor color = styleColor->mColor;
      spacing->mBorderColor[0] = color;
      spacing->mBorderColor[1] = color;
      spacing->mBorderColor[2] = color;
      spacing->mBorderColor[3] = color;
    }
  }
}

PRBool nsHTMLTagContent::ParseColor(const nsString& aString,
                                    nsHTMLValue& aResult)
{
  char cbuf[40];
  aString.ToCString(cbuf, sizeof(cbuf));
  if (aString.Length() > 0) {
    nscolor color;
    if (cbuf[0] == '#') {
      if (NS_HexToRGB(cbuf, &color)) {
        aResult.SetColorValue(color);
        return PR_TRUE;
      }
    } else {
      if (NS_ColorNameToRGB(cbuf, &color)) {
        aResult.SetStringValue(aString);
        return PR_TRUE;
      }
    }
  }

  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
}

PRBool nsHTMLTagContent::ColorToString(const nsHTMLValue& aValue,
                                       nsString& aResult)
{
  if (aValue.GetUnit() == eHTMLUnit_Color) {
    nscolor v = aValue.GetColorValue();
    char buf[10];
    PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                NS_GET_R(v), NS_GET_G(v), NS_GET_B(v));
    aResult.Truncate(0);
    aResult.Append(buf);
    return PR_TRUE;
  }
  if (aValue.GetUnit() == eHTMLUnit_String) {
    aValue.GetStringValue(aResult);
    return PR_TRUE;
  }
  if (aValue.GetUnit() == eHTMLUnit_Empty) {  // was illegal
    aResult.Truncate();
    return PR_TRUE;
  }
  return PR_FALSE;
}
