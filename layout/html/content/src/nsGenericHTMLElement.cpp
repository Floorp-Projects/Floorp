/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsGenericHTMLElement.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsICSSParser.h"
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsIDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIEventListenerManager.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLDocument.h"
#include "nsIHTMLContent.h"
#include "nsILinkHandler.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsISizeOfHandler.h"
#include "nsIStyleContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsStyleConsts.h"
#include "nsXIFConverter.h"
#include "nsFrame.h"
#include "nsRange.h"
#include "nsIPresShell.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsINameSpaceManager.h"

#include "nsIHTMLContentContainer.h"
#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsHTMLAtoms.h"
#include "nsDOMEventsIIDs.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSDeclaration.h"
#include "prprf.h"
#include "prmem.h"
#include "nsIFormControlFrame.h"  

// XXX todo: add in missing out-of-memory checks
NS_DEFINE_IID(kIDOMHTMLElementIID, NS_IDOMHTMLELEMENT_IID);

static NS_DEFINE_IID(kIDOMAttrIID, NS_IDOMATTR_IID);
static NS_DEFINE_IID(kIDOMNamedNodeMapIID, NS_IDOMNAMEDNODEMAP_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kICSSStyleRuleIID, NS_ICSS_STYLE_RULE_IID);
static NS_DEFINE_IID(kICSSDeclarationIID, NS_ICSS_DECLARATION_IID);
static NS_DEFINE_IID(kIDOMNodeListIID, NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDOMCSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMDocumentFragmentIID, NS_IDOMDOCUMENTFRAGMENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);

//----------------------------------------------------------------------

class nsDOMCSSAttributeDeclaration : public nsDOMCSSDeclaration 
{
public:
  nsDOMCSSAttributeDeclaration(nsIHTMLContent *aContent);
  ~nsDOMCSSAttributeDeclaration();

  virtual void DropReference();
  virtual nsresult GetCSSDeclaration(nsICSSDeclaration **aDecl,
                                     PRBool aAllocate);
  virtual nsresult StylePropertyChanged(const nsString& aPropertyName,
                                        PRInt32 aHint);
  virtual nsresult GetParent(nsISupports **aParent);
  virtual nsresult GetBaseURL(nsIURL** aURL);

protected:
  nsIHTMLContent *mContent;  
};

nsDOMCSSAttributeDeclaration::nsDOMCSSAttributeDeclaration(nsIHTMLContent *aContent)
{
  // This reference is not reference-counted. The content
  // object tells us when its about to go away.
  mContent = aContent;
}

nsDOMCSSAttributeDeclaration::~nsDOMCSSAttributeDeclaration()
{
}

void 
nsDOMCSSAttributeDeclaration::DropReference()
{
  mContent = nsnull;
}

nsresult
nsDOMCSSAttributeDeclaration::GetCSSDeclaration(nsICSSDeclaration **aDecl,
                                                  PRBool aAllocate)
{
  nsHTMLValue val;
  nsIStyleRule* rule;
  nsICSSStyleRule*  cssRule;
  nsresult result = NS_OK;

  *aDecl = nsnull;
  if (nsnull != mContent) {
    mContent->GetHTMLAttribute(nsHTMLAtoms::style, val);
    if (eHTMLUnit_ISupports == val.GetUnit()) {
      rule = (nsIStyleRule*) val.GetISupportsValue();
      result = rule->QueryInterface(kICSSStyleRuleIID, (void**)&cssRule);
      if (NS_OK == result) {
        *aDecl = cssRule->GetDeclaration();
        NS_RELEASE(cssRule);
      }      
      NS_RELEASE(rule);
    }
    else if (PR_TRUE == aAllocate) {
      result = NS_NewCSSDeclaration(aDecl);
      if (NS_OK == result) {
        result = NS_NewCSSStyleRule(&cssRule, nsCSSSelector());
        if (NS_OK == result) {
          cssRule->SetDeclaration(*aDecl);
          cssRule->SetWeight(0x7fffffff);
          rule = (nsIStyleRule *)cssRule;
          result = mContent->SetHTMLAttribute(nsHTMLAtoms::style, 
                                              nsHTMLValue(cssRule), 
                                              PR_FALSE);
          NS_RELEASE(cssRule);
        }
        else {
          NS_RELEASE(*aDecl);
        }
      }
    }
  }

  return result;
}

nsresult 
nsDOMCSSAttributeDeclaration::StylePropertyChanged(const nsString& aPropertyName,
                                                   PRInt32 aHint)
{
  nsresult result = NS_OK;
  if (nsnull != mContent) {
    nsIDocument *doc;
    result = mContent->GetDocument(doc);
    if (NS_SUCCEEDED(result) && (nsnull != doc)) {
      result = doc->AttributeChanged(mContent, nsHTMLAtoms::style, aHint);
      NS_RELEASE(doc);
    }
  }
  
  return result;
}

nsresult 
nsDOMCSSAttributeDeclaration::GetParent(nsISupports **aParent)
{
  if (nsnull != mContent) {
    return mContent->QueryInterface(kISupportsIID, (void **)aParent);
  }

  return NS_OK;
}

nsresult 
nsDOMCSSAttributeDeclaration::GetBaseURL(nsIURL** aURL)
{
  NS_ASSERTION(nsnull != aURL, "null pointer");

  nsresult result = NS_ERROR_NULL_POINTER;

  if (nsnull != aURL) {
    *aURL = nsnull;
    if (nsnull != mContent) {
      nsIDocument *doc;
      result = mContent->GetDocument(doc);
      if (NS_SUCCEEDED(result) && (nsnull != doc)) {
        doc->GetBaseURL(*aURL);
        NS_RELEASE(doc);
      }
    }
  }
  return result;
}

//----------------------------------------------------------------------

static nsresult EnsureWritableAttributes(nsIHTMLContent* aContent,
                                         nsIHTMLAttributes*& aAttributes, PRBool aCreate)
{
  nsresult  result = NS_OK;

  if (nsnull == aAttributes) {
    if (PR_TRUE == aCreate) {
      nsMapAttributesFunc mapFunc;
      result = aContent->GetAttributeMappingFunction(mapFunc);
      if (NS_OK == result) {
        result = NS_NewHTMLAttributes(&aAttributes, nsnull, mapFunc);
        if (NS_OK == result) {
          aAttributes->AddContentRef();
        }
      }
    }
  }
  else {
    PRInt32 contentRefCount;
    aAttributes->GetContentRefCount(contentRefCount);
    if (1 < contentRefCount) {
      nsIHTMLAttributes*  attrs;
      result = aAttributes->Clone(&attrs);
      if (NS_OK == result) {
        aAttributes->ReleaseContentRef();
        NS_RELEASE(aAttributes);
        aAttributes = attrs;
        aAttributes->AddContentRef();
      }
    }
  }
  return result;
}

static void ReleaseAttributes(nsIHTMLAttributes*& aAttributes)
{
  aAttributes->ReleaseContentRef();
  NS_RELEASE(aAttributes);
}

nsGenericHTMLElement::nsGenericHTMLElement()
{
  mAttributes = nsnull;
}

nsGenericHTMLElement::~nsGenericHTMLElement()
{
  if (nsnull != mAttributes) {
    ReleaseAttributes(mAttributes);
  }
}

nsresult 
nsGenericHTMLElement::CopyInnerTo(nsIContent* aSrcContent,
                                  nsGenericHTMLElement* aDst,
                                  PRBool aDeep)
{
  nsresult result = NS_OK;
  
  if (nsnull != mAttributes) {
    aDst->mAttributes = mAttributes;
    NS_ADDREF(mAttributes);
    mAttributes->AddContentRef();
  }

  return result;
}

nsresult
nsGenericHTMLElement::GetTagName(nsString& aTagName)
{
  nsGenericElement::GetTagName(aTagName);
  aTagName.ToUpperCase();
  return NS_OK;
}

// Implementation for nsIDOMHTMLElement
nsresult
nsGenericHTMLElement::GetId(nsString& aId)
{
  GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::id, aId);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetId(const nsString& aId)
{
  SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::id, aId, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetTitle(nsString& aTitle)
{
  GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::title, aTitle);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetTitle(const nsString& aTitle)
{
  SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::title, aTitle, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetLang(nsString& aLang)
{
  GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::lang, aLang);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetLang(const nsString& aLang)
{
  SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::lang, aLang, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetDir(nsString& aDir)
{
  GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::dir, aDir);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetDir(const nsString& aDir)
{
  SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::dir, aDir, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetClassName(nsString& aClassName)
{
  GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, aClassName);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetClassName(const nsString& aClassName)
{
  SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, aClassName, PR_TRUE);
  return NS_OK;
}

nsresult    
nsGenericHTMLElement::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  nsresult res = NS_OK;
  nsDOMSlots *slots = GetDOMSlots();

  if (nsnull == slots->mStyle) {
    nsIHTMLContent* htmlContent;
    mContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent);
    slots->mStyle = new nsDOMCSSAttributeDeclaration(htmlContent);
    NS_RELEASE(htmlContent);
    if (nsnull == slots->mStyle) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mStyle);
  }
  
  res = slots->mStyle->QueryInterface(kIDOMCSSStyleDeclarationIID,
                                      (void **)aStyle);

  return res;
}

static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet*  sheet = nsnull;
  nsIHTMLContentContainer*  htmlContainer;
  
  if (nsnull != aDocument) {
    if (NS_OK == aDocument->QueryInterface(kIHTMLContentContainerIID, (void**)&htmlContainer)) {
      htmlContainer->GetAttributeStyleSheet(&sheet);
      NS_RELEASE(htmlContainer);
    }
  }
  NS_ASSERTION(nsnull != sheet, "can't get attribute style sheet");
  return sheet;
}

nsresult
nsGenericHTMLElement::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
  nsresult result = nsGenericElement::SetDocument(aDocument, aDeep);
  
  if (NS_OK != result) {
    return result;
  }

  nsIHTMLContent* htmlContent;

  result = mContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent);
  if (NS_OK != result) {
    return result;
  }

  if ((nsnull != mDocument) && (nsnull != mAttributes)) {
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    if (nsnull != sheet) {
      mAttributes->SetStyleSheet(sheet);
      sheet->SetAttributesFor(htmlContent, mAttributes); // sync attributes with sheet
      NS_RELEASE(sheet);
    }
  }

  NS_RELEASE(htmlContent);
  return result;
}

nsresult
nsGenericHTMLElement::GetNameSpaceID(PRInt32& aID) const
{
  aID = kNameSpaceID_HTML;
  return NS_OK;
}


//void
//nsHTMLTagContent::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
//{
//  if (!aHandler->HaveSeen(mTag)) {
//    mTag->SizeOf(aHandler);
//  }
//  if (!aHandler->HaveSeen(mAttributes)) {
//    mAttributes->SizeOf(aHandler);
//  }
//}

#if 0
static nsGenericHTMLElement::EnumTable kDirTable[] = {
  { "ltr", NS_STYLE_DIRECTION_LTR },
  { "rtl", NS_STYLE_DIRECTION_RTL },
  { 0 }
};
#endif

nsresult 
nsGenericHTMLElement::ParseAttributeString(const nsString& aStr, 
                                           nsIAtom*& aName,
                                           PRInt32& aNameSpaceID)
{
  // XXX need to validate/strip namespace prefix
  nsAutoString  lower;
  aStr.ToLowerCase(lower);  
  aName = NS_NewAtom(lower);
  aNameSpaceID = kNameSpaceID_HTML;
  
  return NS_OK;
}

nsresult 
nsGenericHTMLElement::GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                               nsIAtom*& aPrefix)
{
  aPrefix = nsnull;
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetAttribute(PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   const nsString& aValue,
                                   PRBool aNotify)
{
  nsresult  result = NS_OK;
  NS_ASSERTION((kNameSpaceID_HTML == aNameSpaceID) || 
               (kNameSpaceID_None == aNameSpaceID) || 
               (kNameSpaceID_Unknown == aNameSpaceID), 
               "html content only holds HTML attributes");

  if ((kNameSpaceID_HTML != aNameSpaceID) && 
      (kNameSpaceID_None != aNameSpaceID) &&
      (kNameSpaceID_Unknown != aNameSpaceID)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (nsHTMLAtoms::style == aAttribute) {
    PRBool isCSS = PR_TRUE;
    if (nsnull != mDocument) {
      nsAutoString  styleType;
      mDocument->GetHeaderData(nsHTMLAtoms::headerContentStyleType, styleType);
      if (0 < styleType.Length()) {
        isCSS = styleType.EqualsIgnoreCase("text/css");
      }
    }
    if (isCSS) {
      nsICSSParser* css;
      result = NS_NewCSSParser(&css);
      if (NS_OK != result) {
        return result;
      }

      nsIURL* docURL = nsnull;
      if (nsnull != mDocument) {
        mDocument->GetBaseURL(docURL);
      }

      nsIStyleRule* rule;
      result = css->ParseDeclarations(aValue, docURL, rule);
      NS_IF_RELEASE(docURL);
      if ((NS_OK == result) && (nsnull != rule)) {
        result = SetHTMLAttribute(aAttribute, nsHTMLValue(rule), aNotify);
        NS_RELEASE(rule);
      }
      else {
        result = SetHTMLAttribute(aAttribute, nsHTMLValue(aValue), aNotify);
      }
      NS_RELEASE(css);
      return NS_OK;
    }
  }
  else {
    // Check for event handlers
    if ((nsHTMLAtoms::onclick == aAttribute) || 
             (nsHTMLAtoms::ondblclick == aAttribute) ||
             (nsHTMLAtoms::onmousedown == aAttribute) ||
             (nsHTMLAtoms::onmouseup == aAttribute) ||
             (nsHTMLAtoms::onmouseover == aAttribute) ||
             (nsHTMLAtoms::onmouseout == aAttribute))
      AddScriptEventListener(aAttribute, aValue, kIDOMMouseListenerIID); 
    else if ((nsHTMLAtoms::onkeydown == aAttribute) ||
             (nsHTMLAtoms::onkeyup == aAttribute) ||
             (nsHTMLAtoms::onkeypress == aAttribute))
      AddScriptEventListener(aAttribute, aValue, kIDOMKeyListenerIID); 
    else if (nsHTMLAtoms::onmousemove == aAttribute)
      AddScriptEventListener(aAttribute, aValue, kIDOMMouseMotionListenerIID); 
    else if (nsHTMLAtoms::onload == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onload, aValue, kIDOMLoadListenerIID); 
    else if ((nsHTMLAtoms::onunload == aAttribute) ||
             (nsHTMLAtoms::onabort == aAttribute) ||
             (nsHTMLAtoms::onerror == aAttribute))
      AddScriptEventListener(aAttribute, aValue, kIDOMLoadListenerIID); 
    else if ((nsHTMLAtoms::onfocus == aAttribute) ||
             (nsHTMLAtoms::onblur == aAttribute))
      AddScriptEventListener(aAttribute, aValue, kIDOMFocusListenerIID); 
    else if ((nsHTMLAtoms::onsubmit == aAttribute) ||
             (nsHTMLAtoms::onreset == aAttribute) ||
             (nsHTMLAtoms::onchange == aAttribute))
      AddScriptEventListener(aAttribute, aValue, kIDOMFormListenerIID); 
    else if (nsHTMLAtoms::onpaint == aAttribute)
      AddScriptEventListener(aAttribute, aValue, kIDOMPaintListenerIID); 
  }
 
  nsHTMLValue val;
  nsIHTMLContent* htmlContent;
  
  result = mContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent);
  if (NS_OK != result) {
    return result;
  }
  if (NS_CONTENT_ATTR_NOT_THERE !=
      htmlContent->StringToAttribute(aAttribute, aValue, val)) {
    // string value was mapped to nsHTMLValue, set it that way
    result = SetHTMLAttribute(aAttribute, val, aNotify);
    NS_RELEASE(htmlContent);
    return result;
  }
  else {
    // set as string value to avoid another string copy
    if (nsnull != mDocument) {  // set attr via style sheet
      nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
      if (nsnull != sheet) {
        result = sheet->SetAttributeFor(aAttribute, aValue, htmlContent, mAttributes);
        NS_RELEASE(sheet);
      }
    }
    else {  // manage this ourselves and re-sync when we connect to doc
      result = EnsureWritableAttributes(htmlContent, mAttributes, PR_TRUE);
      if (nsnull != mAttributes) {
        PRInt32   count;
        result = mAttributes->SetAttribute(aAttribute, aValue, count);
        if (0 == count) {
          ReleaseAttributes(mAttributes);
        }
      }
    }
  }
  NS_RELEASE(htmlContent);

  if (aNotify && (nsnull != mDocument)) {
    mDocument->AttributeChanged(mContent, aAttribute, NS_STYLE_HINT_UNKNOWN);
  }

  return result;
}

static PRInt32 GetStyleImpactFrom(const nsHTMLValue& aValue)
{
  PRInt32 hint = NS_STYLE_HINT_NONE;

  if (eHTMLUnit_ISupports == aValue.GetUnit()) {
    nsISupports* supports = aValue.GetISupportsValue();
    if (nsnull != supports) {
      nsICSSStyleRule* cssRule;
      if (NS_SUCCEEDED(supports->QueryInterface(kICSSStyleRuleIID, (void**)&cssRule))) {
        nsICSSDeclaration* declaration = cssRule->GetDeclaration();
        if (nsnull != declaration) {
          declaration->GetStyleImpact(&hint);
          NS_RELEASE(declaration);
        }
        NS_RELEASE(cssRule);
      }
      NS_RELEASE(supports);
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
  nsIHTMLContent* htmlContent;

  result = mContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent);
  if (NS_OK != result) {
    return result;
  }
  if (nsnull != mDocument) {  // set attr via style sheet
    PRInt32 hint = NS_STYLE_HINT_UNKNOWN;
    if (aNotify && (nsHTMLAtoms::style == aAttribute)) {
      nsHTMLValue oldValue;
      PRInt32 oldHint = NS_STYLE_HINT_NONE;
      if (NS_CONTENT_ATTR_NOT_THERE != GetHTMLAttribute(aAttribute, oldValue)) {
        oldHint = GetStyleImpactFrom(oldValue);
      }
      hint = GetStyleImpactFrom(aValue);
      if (hint < oldHint) {
        hint = oldHint;
      }
    }
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    if (nsnull != sheet) {
        result = sheet->SetAttributeFor(aAttribute, aValue, htmlContent,
                                        mAttributes);
        NS_RELEASE(sheet);
    }
    if (aNotify) {
      mDocument->AttributeChanged(mContent, aAttribute, hint);
    }
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    result = EnsureWritableAttributes(htmlContent, mAttributes, PR_TRUE);
    if (nsnull != mAttributes) {
      PRInt32   count;
      result = mAttributes->SetAttribute(aAttribute, aValue, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  NS_RELEASE(htmlContent);
  return result;
}

/**
 * Handle attributes common to all html elements
 */
void
nsGenericHTMLElement::MapCommonAttributesInto(nsIHTMLAttributes* aAttributes, 
                                              nsIStyleContext* aStyleContext,
                                              nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::dir, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      nsStyleDisplay* display = (nsStyleDisplay*)
        aStyleContext->GetMutableStyleData(eStyleStruct_Display);
      display->mDirection = value.GetIntValue();
    }
  }
}

nsresult
nsGenericHTMLElement::UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRBool aNotify)
{
  nsresult result = NS_OK;

  NS_ASSERTION((kNameSpaceID_HTML == aNameSpaceID) || 
               (kNameSpaceID_None == aNameSpaceID) || 
               (kNameSpaceID_Unknown == aNameSpaceID), 
               "html content only holds HTML attributes");

  if ((kNameSpaceID_HTML != aNameSpaceID) && 
      (kNameSpaceID_None != aNameSpaceID) &&
      (kNameSpaceID_Unknown != aNameSpaceID)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsIHTMLContent* htmlContent;

  result = mContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent);
  if (NS_OK != result) {
    return result;
  }
  if (nsnull != mDocument) {  // set attr via style sheet
    PRInt32 hint = NS_STYLE_HINT_UNKNOWN;
    if (aNotify && (nsHTMLAtoms::style == aAttribute)) {
      nsHTMLValue oldValue;
      if (NS_CONTENT_ATTR_NOT_THERE != GetHTMLAttribute(aAttribute, oldValue)) {
        hint = GetStyleImpactFrom(oldValue);
      }
      else {
        hint = NS_STYLE_HINT_NONE;
      }
    }
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    if (nsnull != sheet) {
      result = sheet->UnsetAttributeFor(aAttribute, htmlContent, mAttributes);
      NS_RELEASE(sheet);
    }
    if (aNotify) {
      mDocument->AttributeChanged(mContent, aAttribute, hint);
    }
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    result = EnsureWritableAttributes(htmlContent, mAttributes, PR_FALSE);
    if (nsnull != mAttributes) {
      PRInt32 count;
      result = mAttributes->UnsetAttribute(aAttribute, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  NS_RELEASE(htmlContent);
  return result;
}

nsresult
nsGenericHTMLElement::GetAttribute(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                                   nsString &aResult) const
{
  NS_ASSERTION((kNameSpaceID_HTML == aNameSpaceID) || 
               (kNameSpaceID_None == aNameSpaceID) || 
               (kNameSpaceID_Unknown == aNameSpaceID), 
               "html content only holds HTML attributes");

  if ((kNameSpaceID_HTML != aNameSpaceID) && 
      (kNameSpaceID_None != aNameSpaceID) &&
      (kNameSpaceID_Unknown != aNameSpaceID)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsHTMLValue value;
  nsresult result = GetHTMLAttribute(aAttribute, value);

  char cbuf[20];
  nscolor color;
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    nsIHTMLContent* htmlContent;
    result = mContent->QueryInterface(kIHTMLContentIID, (void **)&htmlContent);
    if (NS_OK != result) {
      return result;
    }
    // Try subclass conversion routine first
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        htmlContent->AttributeToString(aAttribute, value, aResult)) {
      NS_RELEASE(htmlContent);
      return result;
    }
    NS_RELEASE(htmlContent);

    // Provide default conversions for most everything
    switch (value.GetUnit()) {
    case eHTMLUnit_Empty:
      aResult.Truncate();
      break;

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
      aResult.Append('%');
      break;

    case eHTMLUnit_Color:
      color = nscolor(value.GetColorValue());
      PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x",
                  NS_GET_R(color), NS_GET_G(color), NS_GET_B(color));
      aResult.Truncate(0);
      aResult.Append(cbuf);
      break;

    default:
    case eHTMLUnit_Enumerated:
      NS_NOTREACHED("no default enumerated value to string conversion");
      result = NS_CONTENT_ATTR_NOT_THERE;
      break;
    }
  }
  return result;
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
nsGenericHTMLElement::GetAttributeNameAt(PRInt32 aIndex,
                                         PRInt32& aNameSpaceID, 
                                         nsIAtom*& aName) const
{
  aNameSpaceID = kNameSpaceID_HTML;
  if (nsnull != mAttributes) {
    return mAttributes->GetAttributeNameAt(aIndex, aName);
  }
  aName = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult
nsGenericHTMLElement::GetAttributeCount(PRInt32& aCount) const
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

nsresult
nsGenericHTMLElement::HasClass(nsIAtom* aClass) const
{
  if (nsnull != mAttributes) {
    return mAttributes->HasClass(aClass);
  }
  return NS_COMFALSE;
}

nsresult
nsGenericHTMLElement::GetContentStyleRule(nsIStyleRule*& aResult)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  nsIStyleRule* rule = nsnull;

  if (nsnull != mAttributes) {
    result = mAttributes->QueryInterface(kIStyleRuleIID, (void**)&rule);
  }
  aResult = rule;
  return result;
}

nsresult
nsGenericHTMLElement::GetInlineStyleRule(nsIStyleRule*& aResult)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  nsIStyleRule* rule = nsnull;

  if (nsnull != mAttributes) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::style, value)) {
      if (eHTMLUnit_ISupports == value.GetUnit()) {
        nsISupports* supports = value.GetISupportsValue();
        result = supports->QueryInterface(kIStyleRuleIID, (void**)&rule);
        NS_RELEASE(supports);
      }
    }
  }
  aResult = rule;
  return result;
}

nsresult
nsGenericHTMLElement::GetBaseURL(nsIURL*& aBaseURL) const
{
  return GetBaseURL(mAttributes, mDocument, &aBaseURL);
}

nsresult
nsGenericHTMLElement::GetBaseURL(nsIHTMLAttributes* aAttributes,
                                 nsIDocument* aDocument,
                                 nsIURL** aBaseURL)
{
  nsresult result = NS_OK;

  nsIURL* docBaseURL = nsnull;
  if (nsnull != aDocument) {
    result = aDocument->GetBaseURL(docBaseURL);
  }
  *aBaseURL = docBaseURL;
//  NS_IF_RELEASE(docBaseURL);

  if (nsnull != aAttributes) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == aAttributes->GetAttribute(nsHTMLAtoms::_baseHref, value)) {
      if (eHTMLUnit_String == value.GetUnit()) {
        nsAutoString baseHref;
        value.GetStringValue(baseHref);

        nsIURL* url = nsnull;
        nsIURLGroup* urlGroup = nsnull;
        docBaseURL->GetURLGroup(&urlGroup);
        if (urlGroup) {
          result = urlGroup->CreateURL(&url, docBaseURL, baseHref, nsnull);
          NS_RELEASE(urlGroup);
        }
        else {
          result = NS_NewURL(&url, baseHref, docBaseURL);
        }
        *aBaseURL = url;
      }
    }
  }
  return result;
}

nsresult
nsGenericHTMLElement::GetBaseTarget(nsString& aBaseTarget) const
{
  nsresult  result = NS_OK;
  PRBool    hasLocal = PR_FALSE;

  if (nsnull != mAttributes) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::_baseTarget, value)) {
      if (eHTMLUnit_String == value.GetUnit()) {
        value.GetStringValue(aBaseTarget);
        hasLocal = PR_TRUE;
      }
    }
  }
  if ((PR_FALSE == hasLocal) && (nsnull != mDocument)) {
    nsIHTMLDocument* htmlDoc;
    result = mDocument->QueryInterface(kIHTMLDocumentIID, (void**)&htmlDoc);
    if (NS_SUCCEEDED(result)) {
      result = htmlDoc->GetBaseTarget(aBaseTarget);
      NS_RELEASE(htmlDoc);
    }
  }
  else {
    aBaseTarget.Truncate();
  }
  return result;
}

void
nsGenericHTMLElement::ListAttributes(FILE* out) const
{
  PRInt32 index, count;
  GetAttributeCount(count);
  for (index = 0; index < count; index++) {
    // name
    nsIAtom* attr = nsnull;
    PRInt32 nameSpaceID;
    GetAttributeNameAt(index, nameSpaceID, attr);
    nsAutoString buffer;
    attr->ToString(buffer);

    // value
    nsAutoString value;
    GetAttribute(nameSpaceID, attr, value);
    buffer.Append("=");
    buffer.Append(value);

    fputs(" ", out);
    fputs(buffer, out);
    NS_RELEASE(attr);
  }
}

nsresult
nsGenericHTMLElement::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsIAtom* tag;
  GetTag(tag);
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }

  ListAttributes(out);

  nsIContent* hc = mContent;  
  nsrefcnt r = NS_ADDREF(hc) - 1;
  NS_RELEASE(hc);
  fprintf(out, " refcount=%d<", r);

  PRBool canHaveKids;
  mContent->CanContainChildren(canHaveKids);
  if (canHaveKids) {
    fputs("\n", out);
    PRInt32 kids;
    mContent->ChildCount(kids);
    for (index = 0; index < kids; index++) {
      nsIContent* kid;
      mContent->ChildAt(index, kid);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
    for (index = aIndent; --index >= 0; ) fputs("  ", out);
  }
  fputs(">\n", out);

  return NS_OK;
}

nsresult
nsGenericHTMLElement::ToHTML(FILE* out) const
{
  nsAutoString tmp;
  nsresult rv = ToHTMLString(tmp);
  fputs(tmp, out);
  return rv;
}

// XXX i18n: this is wrong (?) because we need to know the outgoing
// character set (I think)
void NS_QuoteForHTML(const nsString& aValue, nsString& aResult);
void
NS_QuoteForHTML(const nsString& aValue, nsString& aResult)
{
  aResult.Truncate();
  const PRUnichar* cp = aValue.GetUnicode();
  const PRUnichar* end = aValue.GetUnicode() + aValue.Length();
  aResult.Append('"');
  while (cp < end) {
    PRUnichar ch = *cp++;
    if ((ch >= 0x20) && (ch <= 0x7f)) {
      if (ch == '\"') {
        aResult.Append("&quot;");
      }
      else {
        aResult.Append(ch);
      }
    }
    else {
      aResult.Append("&#");
      aResult.Append((PRInt32) ch, 10);
      aResult.Append(';');
    }
  }
  aResult.Append('"');
}

nsresult
nsGenericHTMLElement::ToHTMLString(nsString& aBuf) const
{
  aBuf.Truncate(0);
  aBuf.Append('<');

  if (nsnull != mTag) {
    nsAutoString tmp;
    mTag->ToString(tmp);
    aBuf.Append(tmp);
  } else {
    aBuf.Append("?NULL");
  }

  if (nsnull != mAttributes) {
    PRInt32 index, count;
    mAttributes->GetAttributeCount(count);
    nsAutoString name, value, quotedValue;
    for (index = 0; index < count; index++) {
      nsIAtom* atom = nsnull;
      mAttributes->GetAttributeNameAt(index, atom);
      atom->ToString(name);
      aBuf.Append(' ');
      aBuf.Append(name);
      value.Truncate();
      GetAttribute(kNameSpaceID_HTML, atom, value);
      NS_RELEASE(atom);
      if (value.Length() > 0) {
        aBuf.Append('=');
        NS_QuoteForHTML(value, quotedValue);
        aBuf.Append(quotedValue);
      }
    }
  }

  aBuf.Append('>');
  return NS_OK;
}

//----------------------------------------------------------------------


nsresult
nsGenericHTMLElement::AttributeToString(nsIAtom* aAttribute,
                                        const nsHTMLValue& aValue,
                                        nsString& aResult) const
{
  if (nsHTMLAtoms::style == aAttribute) {
    if (eHTMLUnit_ISupports == aValue.GetUnit()) {
      nsIStyleRule* rule = (nsIStyleRule*) aValue.GetISupportsValue();
      nsICSSStyleRule*  cssRule;
      if (NS_OK == rule->QueryInterface(kICSSStyleRuleIID, (void**)&cssRule)) {
        nsICSSDeclaration* decl = cssRule->GetDeclaration();
        if (nsnull != decl) {
          decl->ToString(aResult);
        }
        NS_RELEASE(cssRule);
      }
      else {
        aResult = "Unknown rule type";
      }
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  aResult.Truncate();
  return NS_CONTENT_ATTR_NOT_THERE;
}

PRBool
nsGenericHTMLElement::ParseEnumValue(const nsString& aValue,
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

PRBool
nsGenericHTMLElement::ParseCaseSensitiveEnumValue(const nsString& aValue,
                                                  EnumTable* aTable,
                                                  nsHTMLValue& aResult)
{
  while (nsnull != aTable->tag) {
    if (aValue.Equals(aTable->tag)) {
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

PRBool
nsGenericHTMLElement::ParseValueOrPercent(const nsString& aString,
                                          nsHTMLValue& aResult, 
                                          nsHTMLUnit aValueUnit)
{
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (NS_OK == ec) {
    if (val < 0) val = 0; 
    if (tmp.Last() == '%') {/* XXX not 100% compatible with ebina's code */
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
    return PR_TRUE;
  }

  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
}

/* used to parse attribute values that could be either:
 *   integer  (n), 
 *   percent  (n%),
 *   or proportional (n*)
 */
void
nsGenericHTMLElement::ParseValueOrPercentOrProportional(const nsString& aString,
                                                        nsHTMLValue& aResult, 
                                                        nsHTMLUnit aValueUnit)
{
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (NS_OK == ec) {
    if (val < 0) val = 0;
    if (tmp.Last() == '%') {/* XXX not 100% compatible with ebina's code */
      if (val > 100) val = 100;
      aResult.SetPercentValue(float(val)/100.0f);
	  } else if (tmp.Last() == '*') {
      aResult.SetIntValue(val, eHTMLUnit_Proportional); // proportional values are integers
    } else {
      if (eHTMLUnit_Pixel == aValueUnit) {
        aResult.SetPixelValue(val);
      }
      else {
        aResult.SetIntValue(val, aValueUnit);
      }
    }
  }
}

PRBool
nsGenericHTMLElement::ValueOrPercentToString(const nsHTMLValue& aValue,
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
    default:
      break;
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ValueOrPercentOrProportionalToString(const nsHTMLValue& aValue,
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
  case eHTMLUnit_Proportional:
    aResult.Append(aValue.GetIntValue(), 10);
    aResult.Append('*');
    return PR_TRUE;
  default:
    break;
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseValue(const nsString& aString, PRInt32 aMin,
                                 nsHTMLValue& aResult, nsHTMLUnit aValueUnit)
{
  PRInt32 ec, val = aString.ToInteger(&ec);
  if (NS_OK == ec) {
    if (val < aMin) val = aMin;
    if (eHTMLUnit_Pixel == aValueUnit) {
      aResult.SetPixelValue(val);
    }
    else {
      aResult.SetIntValue(val, aValueUnit);
    }
    return PR_TRUE;
  }

  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseValue(const nsString& aString, PRInt32 aMin,
                                 PRInt32 aMax,
                                 nsHTMLValue& aResult, nsHTMLUnit aValueUnit)
{
  PRInt32 ec, val = aString.ToInteger(&ec);
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

  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseColor(const nsString& aString,
                                 nsHTMLValue& aResult)
{
  if (aString.Length() > 0) {
    nsAutoString  colorStr (aString);
    colorStr.CompressWhitespace();
    char cbuf[40];
    colorStr.ToCString(cbuf, sizeof(cbuf));
    nscolor color = 0;
    if (NS_ColorNameToRGB(cbuf, &color)) {
      aResult.SetStringValue(colorStr);
      return PR_TRUE;
    }
    if (NS_LooseHexToRGB(cbuf, &color)) {
      aResult.SetColorValue(color);
      return PR_TRUE;
    }
  }

#if XXX_no_nav_compat
  // Illegal values are mapped to empty
  aResult.SetEmptyValue();
  return PR_FALSE;
#else
  // Illegal values in nav are mapped to zero
  aResult.SetColorValue(0);
  return PR_TRUE;
#endif
}

PRBool
nsGenericHTMLElement::ColorToString(const nsHTMLValue& aValue,
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

// XXX This creates a dependency between content and frames
nsresult 
nsGenericHTMLElement::GetPrimaryFrame(nsIHTMLContent* aContent,
                                      nsIFormControlFrame *&aFormControlFrame)
{
  nsIDocument* doc = nsnull;
  nsresult res = NS_NOINTERFACE;
   // Get the document
  if (NS_OK == aContent->GetDocument(doc)) {
    if (nsnull != doc) {
       // Get presentation shell 0
      nsIPresShell* presShell = doc->GetShellAt(0);
      if (nsnull != presShell) {
        nsIFrame *frame = nsnull;
        presShell->GetPrimaryFrameFor(aContent, &frame);
        if (nsnull != frame) {
          res = frame->QueryInterface(kIFormControlFrameIID, (void**)&aFormControlFrame);
        }
        NS_RELEASE(presShell);
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

static nsGenericHTMLElement::EnumTable kCompatDivAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "justify", NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kDivAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_CENTER },
  { "justify", NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kFrameborderQuirksTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "1", NS_STYLE_FRAME_1 },
  { "0", NS_STYLE_FRAME_0 },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kFrameborderStandardTable[] = {
  { "1", NS_STYLE_FRAME_1 },
  { "0", NS_STYLE_FRAME_0 },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kScrollingQuirksTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "on", NS_STYLE_FRAME_ON },
  { "off", NS_STYLE_FRAME_OFF },
  { "scroll", NS_STYLE_FRAME_SCROLL },
  { "noscroll", NS_STYLE_FRAME_NOSCROLL },
  { "auto", NS_STYLE_FRAME_AUTO },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kScrollingStandardTable[] = {
  { "yes", NS_STYLE_FRAME_YES },
  { "no", NS_STYLE_FRAME_NO },
  { "auto", NS_STYLE_FRAME_AUTO },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kTableHAlignTable[] = {
  { "left",   NS_STYLE_TEXT_ALIGN_LEFT },
  { "right",  NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "char",   NS_STYLE_TEXT_ALIGN_CHAR },
  { "justify",NS_STYLE_TEXT_ALIGN_JUSTIFY },

  // The following are non-standard but necessary for Nav4 compatibility
  { "middle", NS_STYLE_TEXT_ALIGN_CENTER },
  { "absmiddle", NS_STYLE_TEXT_ALIGN_CENTER },

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
nsGenericHTMLElement::ParseAlignValue(const nsString& aString,
                                      nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::ParseTableHAlignValue(const nsString& aString,
                                            nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kTableHAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::ParseTableVAlignValue(const nsString& aString,
                                            nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kTableVAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::AlignValueToString(const nsHTMLValue& aValue,
                                         nsString& aResult)
{
  return EnumValueToString(aValue, kAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::TableHAlignValueToString(const nsHTMLValue& aValue,
                                               nsString& aResult)
{
  return EnumValueToString(aValue, kTableHAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::TableVAlignValueToString(const nsHTMLValue& aValue,
                                               nsString& aResult)
{
  return EnumValueToString(aValue, kTableVAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::ParseDivAlignValue(const nsString& aString,
                                         nsHTMLValue& aResult)
{
#if XXX_no_nav_compat
  return ParseEnumValue(aString, kDivAlignTable, aResult);
#else
  return ParseEnumValue(aString, kCompatDivAlignTable, aResult);
#endif
}

PRBool
nsGenericHTMLElement::DivAlignValueToString(const nsHTMLValue& aValue,
                                            nsString& aResult)
{
#if XXX_no_nav_compat
  return EnumValueToString(aValue, kDivAlignTable, aResult);
#else
  return EnumValueToString(aValue, kCompatDivAlignTable, aResult);
#endif
}

PRBool
nsGenericHTMLElement::ParseImageAttribute(nsIAtom* aAttribute,
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

PRBool
nsGenericHTMLElement::ImageAttributeToString(nsIAtom* aAttribute,
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

PRBool
nsGenericHTMLElement::ParseFrameborderValue(PRBool aStandardMode,
                                            const nsString& aString,
                                            nsHTMLValue& aResult)
{
  if (aStandardMode) {
    return ParseEnumValue(aString, kFrameborderStandardTable, aResult);
  } else {
    return ParseEnumValue(aString, kFrameborderQuirksTable, aResult);
  }
}

PRBool
nsGenericHTMLElement::FrameborderValueToString(PRBool aStandardMode,
                                               const nsHTMLValue& aValue,
                                               nsString& aResult)
{
  if (aStandardMode) {
    return EnumValueToString(aValue, kFrameborderStandardTable, aResult);
  } else {
    return EnumValueToString(aValue, kFrameborderQuirksTable, aResult);
  }
}

PRBool
nsGenericHTMLElement::ParseScrollingValue(PRBool aStandardMode,
                                          const nsString& aString,
                                          nsHTMLValue& aResult)
{
  if (aStandardMode) {
    return ParseEnumValue(aString, kScrollingStandardTable, aResult);
  } else {
    return ParseEnumValue(aString, kScrollingQuirksTable, aResult);
  }
}

PRBool
nsGenericHTMLElement::ScrollingValueToString(PRBool aStandardMode,
                                             const nsHTMLValue& aValue,
                                             nsString& aResult)
{
  if (aStandardMode) {
    return EnumValueToString(aValue, kScrollingStandardTable, aResult);
  } else {
    return EnumValueToString(aValue, kScrollingQuirksTable, aResult);
  }
}

void
nsGenericHTMLElement::MapImageAttributesInto(nsIHTMLAttributes* aAttributes, 
                                             nsIStyleContext* aContext, 
                                             nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;

    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nsStylePosition* pos = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);

    // width: value
    aAttributes->GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mWidth.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // height: value
    aAttributes->GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mHeight.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mHeight.SetPercentValue(value.GetPercentValue());
    }

    // hspace: value
    aAttributes->GetAttribute(nsHTMLAtoms::hspace, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      nsStyleCoord c(twips);
      spacing->mMargin.SetLeft(c);
      spacing->mMargin.SetRight(c);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      nsStyleCoord c(value.GetPercentValue(), eStyleUnit_Coord);
      spacing->mMargin.SetLeft(c);
      spacing->mMargin.SetRight(c);
    }

    // vspace: value
    aAttributes->GetAttribute(nsHTMLAtoms::vspace, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      nsStyleCoord c(twips);
      spacing->mMargin.SetTop(c);
      spacing->mMargin.SetBottom(c);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      nsStyleCoord c(value.GetPercentValue(), eStyleUnit_Coord);
      spacing->mMargin.SetTop(c);
      spacing->mMargin.SetBottom(c);
    }
  }
}

void
nsGenericHTMLElement::MapImageAlignAttributeInto(nsIHTMLAttributes* aAttributes,
                                                 nsIStyleContext* aContext,
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
      nsStyleSpacing* spacing = (nsStyleSpacing*)
        aContext->GetMutableStyleData(eStyleStruct_Spacing);
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      nsStyleCoord three(NSIntPixelsToTwips(3, p2t));
      switch (align) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        display->mFloats = NS_STYLE_FLOAT_LEFT;
        spacing->mMargin.SetLeft(three);
        spacing->mMargin.SetRight(three);
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        display->mFloats = NS_STYLE_FLOAT_RIGHT;
        spacing->mMargin.SetLeft(three);
        spacing->mMargin.SetRight(three);
        break;
      default:
        text->mVerticalAlign.SetIntValue(align, eStyleUnit_Enumerated);
        break;
      }
    }
  }
}

void
nsGenericHTMLElement::MapImageBorderAttributesInto(nsIHTMLAttributes* aAttributes, 
                                                   nsIStyleContext* aContext, 
                                                   nsIPresContext* aPresContext,
                                                   nscolor aBorderColors[4])
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;

    // border: pixels
    aAttributes->GetAttribute(nsHTMLAtoms::border, value);
    if (value.GetUnit() != eHTMLUnit_Pixel) {
      if (nsnull == aBorderColors) {
        return;
      }
      // If no border is defined and we are forcing a border, force
      // the size to 2 pixels.
      value.SetPixelValue(2);
    }

    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);

    // Fixup border-padding sums: subtract out the old size and then
    // add in the new size.
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);
    nsStyleCoord coord;
    coord.SetCoordValue(twips);
    spacing->mBorder.SetTop(coord);
    spacing->mBorder.SetRight(coord);
    spacing->mBorder.SetBottom(coord);
    spacing->mBorder.SetLeft(coord);
    
	spacing->SetBorderStyle(0,NS_STYLE_BORDER_STYLE_SOLID);
	spacing->SetBorderStyle(1,NS_STYLE_BORDER_STYLE_SOLID);
	spacing->SetBorderStyle(2,NS_STYLE_BORDER_STYLE_SOLID);
	spacing->SetBorderStyle(3,NS_STYLE_BORDER_STYLE_SOLID);


    // Use supplied colors if provided, otherwise use color for border
    // color
    if (nsnull != aBorderColors) {
        spacing->SetBorderColor(0, aBorderColors[0]);
		spacing->SetBorderColor(1, aBorderColors[1]);
		spacing->SetBorderColor(2, aBorderColors[2]);
		spacing->SetBorderColor(3, aBorderColors[3]);
    }
    else {
      // Color is inherited from "color"
      const nsStyleColor* styleColor = (const nsStyleColor*)
        aContext->GetStyleData(eStyleStruct_Color);
      nscolor color = styleColor->mColor;
      spacing->SetBorderColor(0, color);
      spacing->SetBorderColor(1, color);
      spacing->SetBorderColor(2, color);
      spacing->SetBorderColor(3, color);
    }
  }
}

void
nsGenericHTMLElement::MapBackgroundAttributesInto(nsIHTMLAttributes* aAttributes, 
                                                  nsIStyleContext* aContext,
                                                  nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  // background
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      aAttributes->GetAttribute(nsHTMLAtoms::background, value)) {
    if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString absURLSpec;
      nsAutoString spec;
      value.GetStringValue(spec);
      if (spec.Length() > 0) {
        // Resolve url to an absolute url
        nsCOMPtr<nsIPresShell> shell;
        nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv) && shell) {
          nsCOMPtr<nsIDocument> doc;
          rv = shell->GetDocument(getter_AddRefs(doc));
          if (NS_SUCCEEDED(rv) && doc) {
            nsCOMPtr<nsIURL> docURL;
            nsGenericHTMLElement::GetBaseURL(aAttributes, doc,
                                             getter_AddRefs(docURL));
            rv = NS_MakeAbsoluteURL(docURL, "", spec, absURLSpec);
            if (NS_SUCCEEDED(rv)) {
              nsStyleColor* color = (nsStyleColor*)
                aContext->GetMutableStyleData(eStyleStruct_Color);
              color->mBackgroundImage = absURLSpec;
              color->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
              color->mBackgroundRepeat = NS_STYLE_BG_REPEAT_XY;
            }
          }
        }
      }
    }
  }

  // bgcolor
  if (NS_CONTENT_ATTR_HAS_VALUE == aAttributes->GetAttribute(nsHTMLAtoms::bgcolor, value)) {
    if (eHTMLUnit_Color == value.GetUnit()) {
      nsStyleColor* color = (nsStyleColor*)
        aContext->GetMutableStyleData(eStyleStruct_Color);
      color->mBackgroundColor = value.GetColorValue();
      color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
    else if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString buffer;
      value.GetStringValue(buffer);
      char cbuf[40];
      buffer.ToCString(cbuf, sizeof(cbuf));

      nsStyleColor* color = (nsStyleColor*)
        aContext->GetMutableStyleData(eStyleStruct_Color);
      NS_ColorNameToRGB(cbuf, &(color->mBackgroundColor));
      color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
  }
}

static PRBool AttributeChangeRequiresRepaint(const nsIAtom* aAttribute)
{
  // these are attributes that always require a restyle and a repaint, but not a reflow
  // regardless of the tag they are associated with
  return (PRBool)
    (aAttribute==nsHTMLAtoms::bgcolor ||
     aAttribute==nsHTMLAtoms::color);
}

static PRBool AttributeChangeRequiresReflow(const nsIAtom* aAttribute)
{
    // these are attributes that always require a restyle and reflow
    // regardless of the tag they are associated with
  return (PRBool)
    (aAttribute==nsHTMLAtoms::align        ||
     aAttribute==nsHTMLAtoms::border       ||
     aAttribute==nsHTMLAtoms::cellpadding  ||
     aAttribute==nsHTMLAtoms::cellspacing  ||
     aAttribute==nsHTMLAtoms::ch           ||
     aAttribute==nsHTMLAtoms::choff        ||
     aAttribute==nsHTMLAtoms::colspan      ||
     aAttribute==nsHTMLAtoms::face         ||
     aAttribute==nsHTMLAtoms::frame        ||
     aAttribute==nsHTMLAtoms::height       ||
     aAttribute==nsHTMLAtoms::nowrap       ||
     aAttribute==nsHTMLAtoms::rowspan      ||
     aAttribute==nsHTMLAtoms::rules        ||
     aAttribute==nsHTMLAtoms::span         ||
     aAttribute==nsHTMLAtoms::valign       ||
     aAttribute==nsHTMLAtoms::width);
}

static PRBool AttributeChangeRequiresReframe(const nsIAtom* aAttribute)
{
  return (PRBool)
    (aAttribute==nsHTMLAtoms::id           ||
     aAttribute==nsHTMLAtoms::kClass       ||
     aAttribute==nsHTMLAtoms::dir);
}

PRBool
nsGenericHTMLElement::GetStyleHintForCommonAttributes(const nsIContent* aNode,
                                                      const nsIAtom* aAttribute,
                                                      PRInt32* aHint)
{
  PRBool setHint = PR_TRUE;
  if (PR_TRUE == AttributeChangeRequiresReframe(aAttribute)) {
    *aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (PR_TRUE == AttributeChangeRequiresReflow(aAttribute)) {
    *aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (PR_TRUE == AttributeChangeRequiresRepaint(aAttribute)) {
    *aHint = NS_STYLE_HINT_VISUAL;
  }
  else {
    *aHint = NS_STYLE_HINT_CONTENT; // only frames will get notified...
    setHint = PR_FALSE;
  }
  return setHint;
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

nsresult
nsGenericHTMLLeafElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  nsDOMSlots* slots = GetDOMSlots();

  if (nsnull == slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(nsnull);
    if (nsnull == slots->mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mChildNodes);
  }

  return slots->mChildNodes->QueryInterface(kIDOMNodeListIID, (void **)aChildNodes);
}

nsresult
nsGenericHTMLLeafElement::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  nsresult rv = NS_OK;
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.BeginLeaf(name);    
  }

  // Add all attributes to the convert
  if (nsnull != mAttributes) 
  {
    PRInt32 index, count;
    mAttributes->GetAttributeCount(count);
    nsAutoString name, value;
    for (index = 0; index < count; index++) 
    {
      nsIAtom* atom = nsnull;
      mAttributes->GetAttributeNameAt(index, atom);
      atom->ToString(name);

      value.Truncate();
      GetAttribute(kNameSpaceID_HTML, atom, value);
      NS_RELEASE(atom);
      
      aConverter.AddHTMLAttribute(name,value);
    }
  }
  return rv;
}

nsresult
nsGenericHTMLLeafElement::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
  return NS_OK;
}

nsresult
nsGenericHTMLLeafElement::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.EndLeaf(name);    
  }
  return NS_OK;
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
    NS_RELEASE(kid);
  }
}

nsresult
nsGenericHTMLContainerElement::CopyInnerTo(nsIContent* aSrcContent,
                                           nsGenericHTMLContainerElement* aDst,
                                           PRBool aDeep)
{
  nsresult result = nsGenericHTMLElement::CopyInnerTo(aSrcContent,
                                                      aDst,
                                                      aDeep);
  if (NS_OK != result) {
    return result;
  }

  if (aDeep) {
    PRInt32 index;
    PRInt32 count = mChildren.Count();
    for (index = 0; index < count; index++) {
      nsIContent* child = (nsIContent*)mChildren.ElementAt(index);
      if (nsnull != child) {
        nsIDOMNode* node;
        result = child->QueryInterface(kIDOMNodeIID, (void**)&node);
        if (NS_OK == result) {
          nsIDOMNode* newNode;
          
          result = node->CloneNode(aDeep, &newNode);
          if (NS_OK == result) {
            nsIContent* newContent;
            
            result = newNode->QueryInterface(kIContentIID, (void**)&newContent);
            if (NS_OK == result) {
              result = aDst->AppendChildTo(newContent, PR_FALSE);
              NS_RELEASE(newContent);
            }
            NS_RELEASE(newNode);
          }
          NS_RELEASE(node);
        }

        if (NS_OK != result) {
          return result;
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  nsDOMSlots *slots = GetDOMSlots();

  if (nsnull == slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(mContent);
    NS_ADDREF(slots->mChildNodes);
  }

  return slots->mChildNodes->QueryInterface(kIDOMNodeListIID, (void **)aChildNodes);
}

nsresult
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

nsresult
nsGenericHTMLContainerElement::GetFirstChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(0);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  *aNode = nsnull;
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::GetLastChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(mChildren.Count()-1);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  *aNode = nsnull;
  return NS_OK;
}

// XXX It's possible that newChild has already been inserted in the
// tree; if this is the case then we need to remove it from where it
// was before placing it in it's new home

nsresult
nsGenericHTMLContainerElement::InsertBefore(nsIDOMNode* aNewChild,
                                            nsIDOMNode* aRefChild,
                                            nsIDOMNode** aReturn)
{
  nsresult res;

  *aReturn = nsnull;
  if (nsnull == aNewChild) {
    return NS_OK;/* XXX wrong error value */
  }

  // Check if this is a document fragment. If it is, we need
  // to remove the children of the document fragment and add them
  // individually (i.e. we don't add the actual document fragment).
  nsIDOMDocumentFragment* docFrag = nsnull;
  if (NS_OK == aNewChild->QueryInterface(kIDOMDocumentFragmentIID,
                                         (void **)&docFrag)) {
    
    nsIContent* docFragContent;
    res = aNewChild->QueryInterface(kIContentIID, (void **)&docFragContent);

    if (NS_OK == res) {
      nsIContent* refContent = nsnull;
      nsIContent* childContent = nsnull;
      PRInt32 refPos = 0;
      PRInt32 i, count;

      if (nsnull != aRefChild) {
        res = aRefChild->QueryInterface(kIContentIID, (void **)&refContent);
        NS_ASSERTION(NS_OK == res, "Ref child must be an nsIContent");
        IndexOf(refContent, refPos);
      }

      docFragContent->ChildCount(count);
      // Iterate through the fragments children, removing each from
      // the fragment and inserting it into the child list of its
      // new parent.
      for (i = 0; i < count; i++) {
        // Always get and remove the first child, since the child indexes
        // change as we go along.
        res = docFragContent->ChildAt(0, childContent);
        if (NS_OK == res) {
          res = docFragContent->RemoveChildAt(0, PR_FALSE);
          if (NS_OK == res) {
            SetDocumentInChildrenOf(childContent, mDocument);
            if (nsnull == refContent) {
              // Append the new child to the end
              res = AppendChildTo(childContent, PR_TRUE);
            }
            else {
              // Insert the child and increment the insertion position
              res = InsertChildAt(childContent, refPos++, PR_TRUE);
            }
            if (NS_OK != res) {
              // Stop inserting and indicate failure
              break;
            }
          }
          else {
            // Stop inserting and indicate failure
            break;
          }
        }
        else {
          // Stop inserting and indicate failure
          break;
        }
      }
      NS_RELEASE(docFragContent);
      
      // XXX Should really batch notification till the end
      // rather than doing it for each element added
      if (NS_OK == res) {
        *aReturn = aNewChild;
        NS_ADDREF(aNewChild);
      }
      NS_IF_RELEASE(refContent);
    }
    NS_RELEASE(docFrag);
  }
  else {
    // Get the nsIContent interface for the new content
    nsIContent* newContent = nsnull;
    res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
    NS_ASSERTION(NS_OK == res, "New child must be an nsIContent");
    if (NS_OK == res) {
      nsIContent* oldParent;
      res = newContent->GetParent(oldParent);
      if (NS_OK == res) {
        // Remove the element from the old parent if one exists
        if (nsnull != oldParent) {
          PRInt32 index;
          oldParent->IndexOf(newContent, index);
          if (-1 != index) {
            oldParent->RemoveChildAt(index, PR_TRUE);
          }
          NS_RELEASE(oldParent);
        }

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
      }
      NS_RELEASE(newContent);

      *aReturn = aNewChild;
      NS_ADDREF(aNewChild);
    }
  }

  return res;
}

nsresult
nsGenericHTMLContainerElement::ReplaceChild(nsIDOMNode* aNewChild,
                                            nsIDOMNode* aOldChild,
                                            nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  if (nsnull == aOldChild) {
    return NS_OK;
  }
  nsIContent* content = nsnull;
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
        // Check if this is a document fragment. If it is, we need
        // to remove the children of the document fragment and add them
        // individually (i.e. we don't add the actual document fragment).
        nsIDOMDocumentFragment* docFrag = nsnull;
        if (NS_OK == aNewChild->QueryInterface(kIDOMDocumentFragmentIID,
                                               (void **)&docFrag)) {
    
          nsIContent* docFragContent;
          res = aNewChild->QueryInterface(kIContentIID, (void **)&docFragContent);
          if (NS_OK == res) {
            PRInt32 count;

            docFragContent->ChildCount(count);
            // If there are children of the document
            if (count > 0) {
              nsIContent* childContent;
              // Remove the last child of the document fragment
              // and do a replace with it
              res = docFragContent->ChildAt(count-1, childContent);
              if (NS_OK == res) {
                res = docFragContent->RemoveChildAt(count-1, PR_FALSE);
                if (NS_OK == res) {
                  SetDocumentInChildrenOf(childContent, mDocument);
                  res = ReplaceChildAt(childContent, pos, PR_TRUE);
                  // If there are more children, then insert them before
                  // the newly replaced child
                  if ((NS_OK == res) && (count > 1)) {
                    nsIDOMNode* childNode = nsnull;

                    res = childContent->QueryInterface(kIDOMNodeIID,
                                                       (void **)&childNode);
                    if (NS_OK == res) {
                      nsIDOMNode* rv;

                      res = InsertBefore(aNewChild, childNode, &rv);
                      if (NS_OK == res) {
                        NS_IF_RELEASE(rv);
                      }
                      NS_RELEASE(childNode);
                    }
                  }
                }
                NS_RELEASE(childContent);
              }
            }
            NS_RELEASE(docFragContent);
          }
          NS_RELEASE(docFrag);
        }
        else {
          nsIContent* oldParent;
          res = newContent->GetParent(oldParent);
          if (NS_OK == res) {
            // Remove the element from the old parent if one exists
            if (nsnull != oldParent) {
              PRInt32 index;
              oldParent->IndexOf(newContent, index);
              if (-1 != index) {
                oldParent->RemoveChildAt(index, PR_TRUE);
              }
              NS_RELEASE(oldParent);
            }

            SetDocumentInChildrenOf(newContent, mDocument);
            res = ReplaceChildAt(newContent, pos, PR_TRUE);
          }
        }
        NS_RELEASE(newContent);
      }
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }
  
  return res;
}

nsresult
nsGenericHTMLContainerElement::RemoveChild(nsIDOMNode* aOldChild, 
                                           nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  if (nsnull == aOldChild) {
    return NS_OK;
  }

  nsIContent* content = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_SUCCEEDED(res)) {
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

nsresult
nsGenericHTMLContainerElement::AppendChild(nsIDOMNode* aNewChild,
                                           nsIDOMNode** aReturn)
{
  return InsertBefore(aNewChild, nsnull, aReturn);
}

nsresult
nsGenericHTMLContainerElement::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  nsresult rv = NS_OK;
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.BeginContainer(name);
  }

  // Add all attributes to the convert
  if (nsnull != mAttributes) 
  {
    PRInt32 index, count;
    mAttributes->GetAttributeCount(count);
    nsAutoString name, value;
    for (index = 0; index < count; index++) 
    {
      nsIAtom* atom = nsnull;
      mAttributes->GetAttributeNameAt(index, atom);
      atom->ToString(name);

      value.Truncate();
      GetAttribute(kNameSpaceID_HTML, atom, value);
      NS_RELEASE(atom);
      
      aConverter.AddHTMLAttribute(name,value);
    }
  }
  return rv;
}

nsresult
nsGenericHTMLContainerElement::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.EndContainer(name);
  }
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::Compact()
{
  mChildren.Compact();
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::CanContainChildren(PRBool& aResult) const
{
  aResult = PR_TRUE;
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::ChildCount(PRInt32& aCount) const
{
  aCount = mChildren.Count();
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::ChildAt(PRInt32 aIndex,
                                       nsIContent*& aResult) const
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(aIndex);
  if (nsnull != child) {
    NS_ADDREF(child);
  }
  aResult = child;
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::IndexOf(nsIContent* aPossibleChild,
                                       PRInt32& aIndex) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");
  aIndex = mChildren.IndexOf(aPossibleChild);
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::InsertChildAt(nsIContent* aKid,
                                             PRInt32 aIndex,
                                             PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    nsRange::OwnerChildInserted(mContent, aIndex);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE);
      if (aNotify) {
        doc->ContentInserted(mContent, aKid, aIndex);
      }
    }
  }
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::ReplaceChildAt(nsIContent* aKid,
                                              PRInt32 aIndex,
                                              PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    nsRange::OwnerChildReplaced(mContent, aIndex, oldKid);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE);
      if (aNotify) {
        doc->ContentReplaced(mContent, oldKid, aKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  NS_PRECONDITION((nsnull != aKid) && (aKid != mContent), "null ptr");
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    // ranges don't need adjustment since new child is at end of list
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE);
      if (aNotify) {
        doc->ContentAppended(mContent, mChildren.Count() - 1);
      }
    }
  }
  return NS_OK;
}

nsresult
nsGenericHTMLContainerElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
  if (nsnull != oldKid ) {
    nsIDocument* doc = mDocument;
    mChildren.RemoveElementAt(aIndex);
    nsRange::OwnerChildRemoved(mContent, aIndex, oldKid);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentRemoved(mContent, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }

  return NS_OK;
}

