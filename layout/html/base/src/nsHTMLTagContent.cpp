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
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleRule.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "prprf.h"
#include "nsDOMAttributes.h"
#include "nsIDOMDocument.h"
#include "nsILinkHandler.h"
#include "nsIPresContext.h"
#include "nsIURL.h"
#include "nsICSSParser.h"
#include "nsISupportsArray.h"
#include "nsXIFConverter.h"
#include "nsISizeOfHandler.h"
#include "nsIEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsIScriptEventListener.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMEvent.h"
#include "nsDOMEventsIIDs.h"
#include "jsapi.h"

#include "nsXIFConverter.h"

static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIScriptEventListenerIID, NS_ISCRIPTEVENTLISTENER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);


/**
 * Translate the content object into the (XIF) XML Interchange Format
 * XIF is an intermediate form of the content model, the buffer
 * will then be parsed into any number of formats including HTML, TXT, etc.
 * These methods must be called in the following order:
   
      BeginConvertToXIF
      ConvertContentToXIF
      EndConvertToXIF
 */

NS_IMETHODIMP
nsHTMLTagContent::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);


    PRBool    isContainer;
    CanContainChildren(isContainer);
    if (!isContainer)
      aConverter.BeginLeaf(name);    
    else
      aConverter.BeginContainer(name);
  }

  // Add all attributes to the convert
  if (nsnull != mAttributes) 
  {
    nsISupportsArray* attrs;
    nsresult rv = NS_NewISupportsArray(&attrs);
    if (NS_OK == rv) 
    {
      PRInt32 i, n;
      mAttributes->GetAllAttributeNames(attrs, n);
      nsAutoString name, value;
      for (i = 0; i < n; i++) 
      {
        nsIAtom* atom = (nsIAtom*) attrs->ElementAt(i);
        atom->ToString(name);

        value.Truncate();
        GetAttribute(name, value);
        
        aConverter.AddHTMLAttribute(name,value);
        NS_RELEASE(atom);
      }
      NS_RELEASE(attrs);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTagContent::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    PRBool  isContainer;
    CanContainChildren(isContainer);
  
    nsAutoString name;
    mTag->ToString(name);

    if (!isContainer)
      aConverter.EndLeaf(name);    
    else
      aConverter.EndContainer(name);
  }
  return NS_OK;
}

/**
 * Translate the content object into the (XIF) XML Interchange Format
 * XIF is an intermediate form of the content model, the buffer
 * will then be parsed into any number of formats including HTML, TXT, etc.
 */
NS_IMETHODIMP
nsHTMLTagContent::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
  // Do nothing, all conversion is handled in the StartConvertToXIF and FinishConvertToXIF 
  return NS_OK;
}



static nsresult EnsureWritableAttributes(nsIHTMLContent* aContent, 
                                         nsIHTMLAttributes*& aAttributes, PRBool aCreate)
{
  nsresult  result = NS_OK;

  if (nsnull == aAttributes) {
    if (PR_TRUE == aCreate) {
      nsMapAttributesFunc mapFunc;
      result = aContent->GetAttributeMappingFunction(mapFunc);
      if (NS_OK == result) {
        result = NS_NewHTMLAttributes(&aAttributes, mapFunc);
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
  if(nsnull != mAttributes) {
    ReleaseAttributes(mAttributes);
  }
}

nsresult nsHTMLTagContent::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult res = nsHTMLContent::QueryInterface(aIID, aInstancePtr); 
  if (NS_NOINTERFACE == res) {
    if (aIID.Equals(kIDOMElementIID)) {
      *aInstancePtr = (void*)(nsIDOMElement*)(nsIDOMHTMLElement*)this;
      AddRef();
      return NS_OK;
    }
    if (aIID.Equals(kIDOMHTMLElementIID)) {
      *aInstancePtr = (void*)(nsIDOMHTMLElement*)this;
      AddRef();
      return NS_OK;
    }
    if (aIID.Equals(kIJSScriptObjectIID)) {
      *aInstancePtr = (void*)(nsIJSScriptObject*)this;
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

NS_IMETHODIMP nsHTMLTagContent::GetTag(nsIAtom*& aResult) const
{
  NS_IF_ADDREF(mTag);
  aResult = mTag;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTagContent::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  nsHTMLTagContent::SizeOfWithoutThis(aHandler);
  return NS_OK;
}

void
nsHTMLTagContent::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
{
  if (!aHandler->HaveSeen(mTag)) {
    mTag->SizeOf(aHandler);
  }
  if (!aHandler->HaveSeen(mAttributes)) {
    mAttributes->SizeOf(aHandler);
  }
}

NS_IMETHODIMP
nsHTMLTagContent::ToHTMLString(nsString& aBuf) const
{
  aBuf.Truncate(0);
  aBuf.Append('<');

  nsIAtom* tag;
  GetTag(tag);
  if (nsnull != tag) {
    nsAutoString tmp;
    tag->ToString(tmp);
    aBuf.Append(tmp);
  } else {
    aBuf.Append("?NULL");
  }

  if (nsnull != mAttributes) {
    nsISupportsArray* attrs;
    nsresult rv = NS_NewISupportsArray(&attrs);
    if (NS_OK == rv) {
      PRInt32 i, n;
      mAttributes->GetAllAttributeNames(attrs, n);
      nsAutoString name, value, quotedValue;
      for (i = 0; i < n; i++) {
        nsIAtom* atom = (nsIAtom*) attrs->ElementAt(i);
        atom->ToString(name);
        aBuf.Append(' ');
        aBuf.Append(name);
        value.Truncate();
        GetAttribute(name, value);
        if (value.Length() > 0) {
          aBuf.Append('=');
          QuoteForHTML(value, quotedValue);
          aBuf.Append(quotedValue);
        }
        NS_RELEASE(atom);
      }
      NS_RELEASE(attrs);
    }
  }

  aBuf.Append('>');
  return NS_OK;
}

static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet*  sheet = nsnull;
  nsIHTMLDocument*  htmlDoc;

  if (nsnull != aDocument) {
    if (NS_OK == aDocument->QueryInterface(kIHTMLDocumentIID, (void**)&htmlDoc)) {
      htmlDoc->GetAttributeStyleSheet(&sheet);
      NS_RELEASE(htmlDoc);
    }
  }
  NS_ASSERTION(nsnull != sheet, "can't get attribute style sheet");
  return sheet;
}

NS_IMETHODIMP
nsHTMLTagContent::SetDocument(nsIDocument* aDocument)
{
  nsresult rv = nsHTMLContent::SetDocument(aDocument);
  if (NS_OK != rv) {
    return rv;
  }

  if (nsnull != mAttributes) {
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    if (nsnull != sheet) {
      sheet->SetAttributesFor(this, mAttributes); // sync attributes with sheet
      NS_RELEASE(sheet);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTagContent::SetAttribute(const nsString& aName,
                               const nsString& aValue,
                               PRBool aNotify)
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  nsresult rv = SetAttribute(attr, aValue, aNotify);
  NS_RELEASE(attr);
  return rv;
}

NS_IMETHODIMP
nsHTMLTagContent::GetAttribute(nsIAtom *aAttribute,
                               nsString &aResult) const
{
  nsHTMLValue value;
  nsresult result = GetAttribute(aAttribute, value);

  char cbuf[20];
  nscolor color;
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    // Try subclass conversion routine first
    if (NS_CONTENT_ATTR_HAS_VALUE == AttributeToString(aAttribute, value, aResult)) {
      return result;
    }

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

NS_IMETHODIMP
nsHTMLTagContent::GetAttribute(const nsString& aName,
                               nsString& aResult) const
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  nsresult result = GetAttribute(attr, aResult);
  NS_RELEASE(attr);
  return result;
}

NS_IMETHODIMP
nsHTMLTagContent::AttributeToString(nsIAtom* aAttribute,
                                    nsHTMLValue& aValue,
                                    nsString& aResult) const
{
  if (nsHTMLAtoms::style == aAttribute) {
    if (eHTMLUnit_ISupports == aValue.GetUnit()) {
      nsIStyleRule* rule = (nsIStyleRule*) aValue.GetISupportsValue();
      // rule->ToString(str);
      aResult = "XXX style rule ToString goes here";
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  aResult.Truncate();
  return NS_CONTENT_ATTR_NOT_THERE;
}

// XXX temporary stub until this class is nuked
NS_IMETHODIMP
nsHTMLTagContent::StringToAttribute(nsIAtom* aAttribute,
                                    const nsString& aValue,
                                    nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

nsresult
nsHTMLTagContent::AddScriptEventListener(nsIAtom* aAttribute, const nsString& aValue, REFNSIID aIID)
{
  nsresult mRet = NS_OK;  
  nsIScriptContext* mContext;
  nsIScriptContextOwner* mOwner;

  NS_PRECONDITION(nsnull != mDocument, "Element not yet added to doc");

  if (nsnull != mDocument) {
    mOwner = mDocument->GetScriptContextOwner();
    if (NS_OK == mOwner->GetScriptContext(&mContext)) {
      if (nsHTMLAtoms::body == mTag || nsHTMLAtoms::frameset == mTag) {
        nsIDOMEventReceiver *mReceiver;
        nsIScriptGlobalObject *mGlobal = mContext->GetGlobalObject();

        if (nsnull != mGlobal && NS_OK == mGlobal->QueryInterface(kIDOMEventReceiverIID, (void**)&mReceiver)) {
          nsIEventListenerManager *mManager;
          if (NS_OK == mReceiver->GetListenerManager(&mManager)) {
            nsIScriptObjectOwner *mObjectOwner;
            if (NS_OK == mGlobal->QueryInterface(kIScriptObjectOwnerIID, (void**)&mObjectOwner)) {
              mRet = mManager->AddScriptEventListener(mContext, mObjectOwner, aAttribute, aValue, aIID); 
              NS_RELEASE(mObjectOwner);
            }
            NS_RELEASE(mManager);
          }
          NS_RELEASE(mReceiver);
        }
        NS_IF_RELEASE(mGlobal);
      }
      else {
        nsIEventListenerManager *mManager;
        if (NS_OK == GetListenerManager(&mManager)) {
          mRet = mManager->AddScriptEventListener(mContext, this, aAttribute, aValue, aIID);
          NS_RELEASE(mManager);
        }
      }
      NS_RELEASE(mContext);
    }
    NS_RELEASE(mOwner);
  }
  return mRet;
}

// Note: Subclasses should override to parse the value string; in
// addition, when they see an unknown attribute they should call this
// so that global attributes are handled (like CLASS, ID, STYLE, etc.)
NS_IMETHODIMP
nsHTMLTagContent::SetAttribute(nsIAtom* aAttribute,
                               const nsString& aValue,
                               PRBool aNotify)
{
  nsresult  result = NS_OK;
  if (nsHTMLAtoms::style == aAttribute) {
    // XXX the style sheet language is a document property that
    // should be used to lookup the style sheet parser to parse the
    // attribute.
    nsICSSParser* css;
    result = NS_NewCSSParser(&css);
    if (NS_OK != result) {
      return result;
    }
    nsIStyleRule* rule;
    result = css->ParseDeclarations(aValue, nsnull, rule);
    if ((NS_OK == result) && (nsnull != rule)) {
      result = SetAttribute(aAttribute, nsHTMLValue(rule), aNotify);
      NS_RELEASE(rule);
    }
    NS_RELEASE(css);
  }
  else {
    // Check for event handlers
    if (nsHTMLAtoms::onclick == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onclick, aValue, kIDOMMouseListenerIID); 
    if (nsHTMLAtoms::ondblclick == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onclick, aValue, kIDOMMouseListenerIID); 
    if (nsHTMLAtoms::onmousedown == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onmousedown, aValue, kIDOMMouseListenerIID); 
    if (nsHTMLAtoms::onmouseup == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onmouseup, aValue, kIDOMMouseListenerIID); 
    if (nsHTMLAtoms::onmouseover == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onmouseover, aValue, kIDOMMouseListenerIID); 
    if (nsHTMLAtoms::onmouseout == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onmouseout, aValue, kIDOMMouseListenerIID); 
    if (nsHTMLAtoms::onkeydown == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onkeydown, aValue, kIDOMKeyListenerIID); 
    if (nsHTMLAtoms::onkeyup == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onkeyup, aValue, kIDOMKeyListenerIID); 
    if (nsHTMLAtoms::onkeypress == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onkeypress, aValue, kIDOMKeyListenerIID); 
    if (nsHTMLAtoms::onmousemove == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onmousemove, aValue, kIDOMMouseMotionListenerIID); 
    if (nsHTMLAtoms::onload == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onload, aValue, kIDOMLoadListenerIID); 
    if (nsHTMLAtoms::onunload == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onunload, aValue, kIDOMLoadListenerIID); 
    if (nsHTMLAtoms::onabort == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onabort, aValue, kIDOMLoadListenerIID); 
    if (nsHTMLAtoms::onerror == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onerror, aValue, kIDOMLoadListenerIID); 
    if (nsHTMLAtoms::onfocus == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onfocus, aValue, kIDOMFocusListenerIID); 
    if (nsHTMLAtoms::onblur == aAttribute)
      AddScriptEventListener(nsHTMLAtoms::onblur, aValue, kIDOMFocusListenerIID); 

    if (nsnull != mDocument) {  // set attr via style sheet
      nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
      if (nsnull != sheet) {
        result = sheet->SetAttributeFor(aAttribute, aValue, this, mAttributes);
        NS_RELEASE(sheet);
      }
    }
    else {  // manage this ourselves and re-sync when we connect to doc
      result = EnsureWritableAttributes(this, mAttributes, PR_TRUE);
      if (nsnull != mAttributes) {
        PRInt32   count;
        result = mAttributes->SetAttribute(aAttribute, aValue, count);
        if (0 == count) {
          ReleaseAttributes(mAttributes);
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLTagContent::SetAttribute(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               PRBool aNotify)
{
  nsresult  result = NS_OK;
  if (nsnull != mDocument) {  // set attr via style sheet
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    if (nsnull != sheet) {
      result = sheet->SetAttributeFor(aAttribute, aValue, this, mAttributes);
      NS_RELEASE(sheet);
    }
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    result = EnsureWritableAttributes(this, mAttributes, PR_TRUE);
    if (nsnull != mAttributes) {
      PRInt32   count;
      result = mAttributes->SetAttribute(aAttribute, aValue, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLTagContent::UnsetAttribute(nsIAtom* aAttribute)
{
  nsresult result = NS_OK;
  if (nsnull != mDocument) {  // set attr via style sheet
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    if (nsnull != sheet) {
      result = sheet->UnsetAttributeFor(aAttribute, this, mAttributes);
      NS_RELEASE(sheet);
    }
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    result = EnsureWritableAttributes(this, mAttributes, PR_FALSE);
    if (nsnull != mAttributes) {
      PRInt32 count;
      result = mAttributes->UnsetAttribute(aAttribute, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLTagContent::GetAttribute(nsIAtom* aAttribute,
                               nsHTMLValue& aValue) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAttribute(aAttribute, aValue);
  }
  aValue.Reset();
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTagContent::GetAllAttributeNames(nsISupportsArray* aArray,
                                       PRInt32& aCount) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAllAttributeNames(aArray, aCount);
  }
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTagContent::GetAttributeCount(PRInt32& aCount) const
{
  if (nsnull != mAttributes) {
    return mAttributes->Count(aCount);
  }
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTagContent::SetID(nsIAtom* aID)
{
  nsresult result = NS_OK;
  if (nsnull != mDocument) {  // set attr via style sheet
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    if (nsnull != sheet) {
      result = sheet->SetIDFor(aID, this, mAttributes);
      NS_RELEASE(sheet);
    }
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    EnsureWritableAttributes(this, mAttributes, PRBool(nsnull != aID));
    if (nsnull != mAttributes) {
      PRInt32 count;
      result = mAttributes->SetID(aID, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLTagContent::GetID(nsIAtom*& aResult) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetID(aResult);
  }
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTagContent::SetClass(nsIAtom* aClass)
{
  nsresult result = NS_OK;
  if (nsnull != mDocument) {  // set attr via style sheet
    nsIHTMLStyleSheet*  sheet = GetAttrStyleSheet(mDocument);
    if (nsnull != sheet) {
      result = sheet->SetClassFor(aClass, this, mAttributes);
      NS_RELEASE(sheet);
    }
  }
  else {  // manage this ourselves and re-sync when we connect to doc
    EnsureWritableAttributes(this, mAttributes, PRBool(nsnull != aClass));
    if (nsnull != mAttributes) {
      PRInt32 count;
      result = mAttributes->SetClass(aClass, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLTagContent::GetClass(nsIAtom*& aResult) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetClass(aResult);
  }
  aResult = nsnull;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTagContent::GetStyleRule(nsIStyleRule*& aResult)
{
  nsIStyleRule* result = nsnull;

  if (nsnull != mAttributes) {
    mAttributes->QueryInterface(kIStyleRuleIID, (void**)&result);
  }
  aResult = result;
  return NS_OK;
}

//
// Implementation of nsIScriptObjectOwner interface
//
nsresult nsHTMLTagContent::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptElement(aContext, (nsISupports *)(nsIDOMHTMLElement *)this, mParent, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

PRBool    nsHTMLTagContent::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    nsHTMLTagContent::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    nsHTMLTagContent::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    nsHTMLTagContent::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsAutoString mPropName, mPrefix;
    mPropName.SetString(JS_GetStringChars(JS_ValueToString(aContext, aID)));
    mPrefix.SetString(mPropName, 2);
    if (mPrefix == "on") {
      nsIEventListenerManager *mManager = nsnull;

      if (mPropName == "onmousedown" || mPropName == "onmouseup" || mPropName ==  "onclick" ||
         mPropName == "onmouseover" || mPropName == "onmouseout") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMMouseListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onkeydown" || mPropName == "onkeyup" || mPropName == "onkeypress") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMKeyListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onmousemove") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMMouseMotionListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onfocus" || mPropName == "onblur") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMFocusListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onsubmit" || mPropName == "onreset") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMFormListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onload" || mPropName == "onunload" || mPropName == "onabort" ||
               mPropName == "onerror") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMLoadListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      NS_IF_RELEASE(mManager);
    }
  }
  return PR_TRUE;
}

PRBool    nsHTMLTagContent::EnumerateProperty(JSContext *aContext)
{
  return PR_TRUE;
}

PRBool    nsHTMLTagContent::Resolve(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

PRBool    nsHTMLTagContent::Convert(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

void      nsHTMLTagContent::Finalize(JSContext *aContext)
{
}

PRBool    
nsHTMLTagContent::Construct(JSContext *cx, JSObject *obj,  uintN argc, 
                            jsval *argv, jsval *rval)
{
  return PR_FALSE;
}

//
// Implementation of nsIDOMNode interface
//
NS_IMETHODIMP    
nsHTMLTagContent::GetNodeName(nsString& aNodeName)
{
  return GetTagName(aNodeName);
}

NS_IMETHODIMP    
nsHTMLTagContent::GetNodeValue(nsString& aNodeValue)
{
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::SetNodeValue(const nsString& aNodeValue)
{
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetNodeType(PRInt32* aNodeType)
{
  *aNodeType = nsHTMLContent::ELEMENT;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_PRECONDITION(nsnull != aAttributes, "null pointer argument");
  if (nsnull != mAttributes) {
    // XXX Should we create a new one every time or should we
    // cache one after we create it? If we find that this is
    // something that's called often, we might need to do the
    // latter.
    *aAttributes = new nsDOMAttributeMap(*this);
  }
  else {
    *aAttributes = nsnull;
  }
  return NS_OK;
}

// XXX Currently implemented as a call to document.CreateElement().
// This requires that the content actually have a document, which
// might not be the case if it isn't yet part of the tree.
NS_IMETHODIMP    
nsHTMLTagContent::CloneNode(nsIDOMNode** aReturn)
{
  nsIDOMDocument *doc;
  nsresult res = NS_OK; 
  nsAutoString tag_name;
  nsIDOMNamedNodeMap *attr_map;

  if ((nsnull == mDocument) || (nsnull == mTag)) {
    return NS_ERROR_FAILURE;
  }

  res = mDocument->QueryInterface(kIDOMDocumentIID, (void **)&doc);
  if (NS_OK != res) {
    return res;
  }

  mTag->ToString(tag_name);
  // XXX Probably not the most efficient way to pass along attribute
  // information.
  GetAttributes(&attr_map);

  res = doc->CreateElement(tag_name, attr_map, (nsIDOMElement **)aReturn);

  NS_RELEASE(doc);

  return res;
}

NS_IMETHODIMP    
nsHTMLTagContent::Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetParentNode(nsIDOMNode** aParentNode)
{
  return nsHTMLContent::GetParentNode(aParentNode);
}

NS_IMETHODIMP    
nsHTMLTagContent::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  return nsHTMLContent::GetChildNodes(aChildNodes);
}

NS_IMETHODIMP    
nsHTMLTagContent::GetHasChildNodes(PRBool* aHasChildNodes)
{
  return nsHTMLContent::GetHasChildNodes(aHasChildNodes);
}

NS_IMETHODIMP    
nsHTMLTagContent::GetFirstChild(nsIDOMNode** aFirstChild)
{
  return nsHTMLContent::GetFirstChild(aFirstChild);
}

NS_IMETHODIMP    
nsHTMLTagContent::GetLastChild(nsIDOMNode** aLastChild)
{
  return nsHTMLContent::GetLastChild(aLastChild);
}

NS_IMETHODIMP    
nsHTMLTagContent::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  return nsHTMLContent::GetPreviousSibling(aPreviousSibling);
}

NS_IMETHODIMP    
nsHTMLTagContent::GetNextSibling(nsIDOMNode** aNextSibling)
{
  return nsHTMLContent::GetNextSibling(aNextSibling);
}

NS_IMETHODIMP    
nsHTMLTagContent::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return nsHTMLContent::InsertBefore(aNewChild, aRefChild, aReturn);
}

NS_IMETHODIMP    
nsHTMLTagContent::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsHTMLContent::ReplaceChild(aNewChild, aOldChild, aReturn);
}

NS_IMETHODIMP    
nsHTMLTagContent::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsHTMLContent::RemoveChild(aOldChild, aReturn);
}

NS_IMETHODIMP    
nsHTMLTagContent::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return nsHTMLContent::AppendChild(aNewChild, aReturn);
}


//
// Implementation of nsIDOMElement interface
//
NS_IMETHODIMP    
nsHTMLTagContent::GetTagName(nsString& aTagName)
{
  if (nsnull != mTag) {
    mTag->ToString(aTagName);
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetDOMAttribute(const nsString& aName, nsString& aReturn)
{
  GetAttribute(aName, aReturn);
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::SetDOMAttribute(const nsString& aName, const nsString& aValue)
{
  SetAttribute(aName, aValue, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::RemoveAttribute(const nsString& aName)
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  UnsetAttribute(attr);
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetAttributeNode(const nsString& aName, nsIDOMAttribute** aReturn)
{
  nsAutoString value;
  if(NS_CONTENT_ATTR_NOT_THERE != GetAttribute(aName, value)) {
    *aReturn = new nsDOMAttribute(aName, value);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::SetAttributeNode(nsIDOMAttribute* aAttribute)
{
  NS_PRECONDITION(nsnull != aAttribute, "null attribute");
  
  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != aAttribute) {
    // XXX Very suspicious code. Why aren't these nsAutoString?
    nsString name, value;
    res = aAttribute->GetName(name);
    if (NS_OK == res) {
      res = aAttribute->GetValue(value);
      if (NS_OK == res) {
        SetAttribute(name, value, PR_TRUE);
      }
    }
  }

  return res;
}

NS_IMETHODIMP    
nsHTMLTagContent::RemoveAttributeNode(nsIDOMAttribute* aAttribute)
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

NS_IMETHODIMP    
nsHTMLTagContent::GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsHTMLTagContent::Normalize()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//
// Implementation of nsIDOMHTMLElement interface
//
NS_IMETHODIMP    
nsHTMLTagContent::GetId(nsString& aId)
{
  GetAttribute(nsHTMLAtoms::id, aId);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::SetId(const nsString& aId)
{
  SetAttribute(nsHTMLAtoms::id, aId, PR_TRUE);
  
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetTitle(nsString& aTitle)
{
  GetAttribute(nsHTMLAtoms::title, aTitle);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::SetTitle(const nsString& aTitle)
{
  SetAttribute(nsHTMLAtoms::title, aTitle, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetLang(nsString& aLang)
{
  GetAttribute(nsHTMLAtoms::lang, aLang);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::SetLang(const nsString& aLang)
{
  SetAttribute(nsHTMLAtoms::lang, aLang, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetDir(nsString& aDir)
{
  GetAttribute(nsHTMLAtoms::dir, aDir);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::SetDir(const nsString& aDir)
{
  SetAttribute(nsHTMLAtoms::dir, aDir, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::GetClassName(nsString& aClassName)
{
  GetAttribute(nsHTMLAtoms::kClass, aClassName);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLTagContent::SetClassName(const nsString& aClassName)
{
  SetAttribute(nsHTMLAtoms::kClass, aClassName, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTagContent::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  // XXX NYI
  return NS_OK;
}

void nsHTMLTagContent::TriggerLink(nsIPresContext& aPresContext,
                                       const nsString& aBase,
                                       const nsString& aURLSpec,
                                       const nsString& aTargetSpec,
                                       PRBool aClick)
{
  nsILinkHandler* handler;
  if (NS_OK == aPresContext.GetLinkHandler(&handler) && (nsnull != handler)) {
    // Resolve url to an absolute url
    nsIURL* docURL = nsnull;
    nsIDocument* doc;
    if (NS_OK == GetDocument(doc)) {
      docURL = doc->GetDocumentURL();
      NS_RELEASE(doc);
    }

    nsAutoString absURLSpec;
    if (aURLSpec.Length() > 0) {
      nsresult rv = NS_MakeAbsoluteURL(docURL, aBase, aURLSpec, absURLSpec);
    }
    else {
      absURLSpec = aURLSpec;
    }

    if (nsnull != docURL) {
      NS_RELEASE(docURL);
    }

    // Now pass on absolute url to the click handler
    if (aClick) {
      handler->OnLinkClick(nsnull, absURLSpec, aTargetSpec);
    }
    else {
      handler->OnOverLink(nsnull, absURLSpec, aTargetSpec);
    }
    NS_RELEASE(handler);
  }
}

nsresult nsHTMLTagContent::HandleDOMEvent(nsIPresContext& aPresContext,
                                            nsEvent* aEvent,
                                            nsIDOMEvent** aDOMEvent,
                                            PRUint32 aFlags,
                                            nsEventStatus& aEventStatus)
{
  nsresult ret = NS_OK;
  
  ret = nsHTMLContent::HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);

  if (NS_OK == ret && nsEventStatus_eIgnore == aEventStatus) {
    switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      if (mTag == nsHTMLAtoms::a) {
        nsIEventStateManager *mStateManager;
        if (NS_OK == aPresContext.GetEventStateManager(&mStateManager)) {
          mStateManager->SetActiveLink(this);
          NS_RELEASE(mStateManager);
        }
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      if (mTag == nsHTMLAtoms::a) {
        nsIEventStateManager *mStateManager;
        nsIContent *mActiveLink;
        if (NS_OK == aPresContext.GetEventStateManager(&mStateManager)) {
          mStateManager->GetActiveLink(&mActiveLink);
          NS_RELEASE(mStateManager);
        }

        if (mActiveLink == this) {
          nsEventStatus mStatus;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = NS_MOUSE_LEFT_CLICK;
          HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, mStatus);

          if (nsEventStatus_eConsumeNoDefault != mStatus) {
            nsAutoString base, href, target;
            GetAttribute(nsString(NS_HTML_BASE_HREF), base);
            GetAttribute(nsString("href"), href);
            GetAttribute(nsString("target"), target);
            if (target.Length() == 0) {
              GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
            }
            TriggerLink(aPresContext, base, href, target, PR_TRUE);
            aEventStatus = nsEventStatus_eConsumeNoDefault; 
          }
        }
      }
      break;

    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      // XXX Bring up a contextual menu provided by the application
      break;

    case NS_MOUSE_ENTER:
    //mouse enter doesn't work yet.  Use move until then.
      if (mTag == nsHTMLAtoms::a) {
        nsAutoString base, href, target;
        GetAttribute(nsString(NS_HTML_BASE_HREF), base);
        GetAttribute(nsString("href"), href);
        GetAttribute(nsString("target"), target);
        if (target.Length() == 0) {
          GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
        }
        TriggerLink(aPresContext, base, href, target, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

      // XXX this doesn't seem to do anything yet
    case NS_MOUSE_EXIT:
      if (mTag == nsHTMLAtoms::a) {
        nsAutoString empty;
        TriggerLink(aPresContext, empty, empty, empty, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeDoDefault; 
      }
      break;

    default:
      break;
    }
  }
  return ret;
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
  { "middle", NS_STYLE_TEXT_ALIGN_CENTER },
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

/* ----- table specific attribute code ----- */

static nsHTMLTagContent::EnumTable kTableAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_CENTER },
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


static nsHTMLTagContent::EnumTable kTableCaptionAlignTable[] = {
  { "left",  NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "top",   NS_STYLE_VERTICAL_ALIGN_TOP},
  { "bottom",NS_STYLE_VERTICAL_ALIGN_BOTTOM},
  { 0 }
};

PRBool nsHTMLTagContent::ParseTableCaptionAlignParam(const nsString& aString,
                                                     nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kTableCaptionAlignTable, aResult);
}

PRBool nsHTMLTagContent::TableCaptionAlignParamToString(const nsHTMLValue& aValue,
                                                        nsString& aResult)
{
  return EnumValueToString(aValue, kTableCaptionAlignTable, aResult);
}

/* ----- end table specific attribute code ----- */


PRBool
nsHTMLTagContent::ParseValueOrPercent(const nsString& aString,
                                      nsHTMLValue& aResult, 
                                      nsHTMLUnit aValueUnit)
{ // XXX should vave min/max values?
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (NS_OK == ec) {
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
nsHTMLTagContent::ParseValueOrPercentOrProportional(const nsString& aString,
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

PRBool
nsHTMLTagContent::ValueOrPercentToString(const nsHTMLValue& aValue,
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

PRBool
nsHTMLTagContent::ParseValue(const nsString& aString, PRInt32 aMin,
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
nsHTMLTagContent::ParseValue(const nsString& aString, PRInt32 aMin,
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
      aContext->GetMutableStyleData(eStyleStruct_Position);
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);

    // width: value
    GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mWidth.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // height: value
    GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mHeight.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mHeight.SetPercentValue(value.GetPercentValue());
    }

    // hspace: value
    GetAttribute(nsHTMLAtoms::hspace, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      spacing->mMargin.SetRight(nsStyleCoord(twips));
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      spacing->mMargin.SetRight(nsStyleCoord(value.GetPercentValue(),
                                             eStyleUnit_Coord));
    }

    // vspace: value
    GetAttribute(nsHTMLAtoms::vspace, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      spacing->mMargin.SetBottom(nsStyleCoord(twips));
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      spacing->mMargin.SetBottom(nsStyleCoord(value.GetPercentValue(),
                                              eStyleUnit_Coord));
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
      const nsStyleColor* styleColor = (const nsStyleColor*)
        aContext->GetStyleData(eStyleStruct_Color);
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
  if (aString.Length() > 0) {
    nsAutoString  colorStr (aString);
    colorStr.CompressWhitespace();
    char cbuf[40];
    colorStr.ToCString(cbuf, sizeof(cbuf));
    nscolor color;
    if (NS_ColorNameToRGB(cbuf, &color)) {
      aResult.SetStringValue(colorStr);
      return PR_TRUE;
    }
    if (NS_HexToRGB(cbuf, &color)) {
      aResult.SetColorValue(color);
      return PR_TRUE;
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
