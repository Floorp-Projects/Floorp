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
#include "nscore.h"
#include "nsGenericHTMLElement.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"
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
#include "nsIHTMLAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLDocument.h"
#include "nsIHTMLContent.h"
#include "nsILink.h"
#include "nsILinkHandler.h"
#include "nsPIDOMWindow.h"
#include "nsISizeOfHandler.h"
#include "nsIStyleContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsStyleConsts.h"
#include "nsIFrame.h"
#include "nsRange.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"

#include "nsIPresState.h"
#include "nsILayoutHistoryState.h"
#include "nsIFrameManager.h"

#include "nsIHTMLContentContainer.h"
#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSDeclaration.h"
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

#include "nsIRuleWalker.h"

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

static NS_DEFINE_CID(kPresStateCID,  NS_PRESSTATE_CID);
// XXX todo: add in missing out-of-memory checks

#include "nsIPref.h" // Used by the temp pref, should be removed!

//----------------------------------------------------------------------

class nsDOMCSSAttributeDeclaration : public nsDOMCSSDeclaration
{
public:
  nsDOMCSSAttributeDeclaration(nsIHTMLContent *aContent);
  ~nsDOMCSSAttributeDeclaration();

  NS_IMETHOD RemoveProperty(const nsAReadableString& aPropertyName,
                            nsAWritableString& aReturn);

  virtual void DropReference();
  virtual nsresult GetCSSDeclaration(nsICSSDeclaration **aDecl,
                                     PRBool aAllocate);
  virtual nsresult SetCSSDeclaration(nsICSSDeclaration *aDecl);
  virtual nsresult ParsePropertyValue(const nsAReadableString& aPropName,
                                      const nsAReadableString& aPropValue);
  virtual nsresult ParseDeclaration(const nsAReadableString& aDecl,
                                    PRBool aParseOnlyOneDecl,
                                    PRBool aClearOldDecl);
  virtual nsresult GetParent(nsISupports **aParent);

protected:
  nsIHTMLContent *mContent;
};

MOZ_DECL_CTOR_COUNTER(nsDOMCSSAttributeDeclaration)

nsDOMCSSAttributeDeclaration::nsDOMCSSAttributeDeclaration(nsIHTMLContent *aContent)
{
  MOZ_COUNT_CTOR(nsDOMCSSAttributeDeclaration);

  // This reference is not reference-counted. The content
  // object tells us when its about to go away.
  mContent = aContent;
}

nsDOMCSSAttributeDeclaration::~nsDOMCSSAttributeDeclaration()
{
  MOZ_COUNT_DTOR(nsDOMCSSAttributeDeclaration);
}

NS_IMETHODIMP
nsDOMCSSAttributeDeclaration::RemoveProperty(const nsAReadableString& aPropertyName,
                                             nsAWritableString& aReturn)
{
  nsCOMPtr<nsICSSDeclaration> decl;
  nsresult rv = GetCSSDeclaration(getter_AddRefs(decl), PR_TRUE);

  if (NS_SUCCEEDED(rv) && decl && mContent) {
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));

    if (doc) {
      doc->BeginUpdate();

      doc->AttributeWillChange(mContent, kNameSpaceID_None,
                               nsHTMLAtoms::style);
    }

    PRInt32 hint;
    decl->GetStyleImpact(&hint);

    nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);
    nsCSSValue val;

    rv = decl->RemoveProperty(prop, val);

    if (NS_SUCCEEDED(rv)) {
      // We pass in eCSSProperty_UNKNOWN here so that we don't get the
      // property name in the return string.
      val.ToString(aReturn, eCSSProperty_UNKNOWN);
    } else {
      // If we tried to remove an invalid property or a property that wasn't
      //  set we simply return success and an empty string
      rv = NS_OK;
    }

    if (doc) {
      doc->AttributeChanged(mContent, kNameSpaceID_None, nsHTMLAtoms::style,
                            nsIDOMMutationEvent::MODIFICATION, 
                            hint);
      doc->EndUpdate();
    }
  }

  return rv;
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
      result = rule->QueryInterface(NS_GET_IID(nsICSSStyleRule), (void**)&cssRule);
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
nsDOMCSSAttributeDeclaration::SetCSSDeclaration(nsICSSDeclaration *aDecl)
{
  nsHTMLValue val;
  nsIStyleRule* rule;
  nsICSSStyleRule*  cssRule;
  nsresult result = NS_OK;

  if (nsnull != mContent) {
    mContent->GetHTMLAttribute(nsHTMLAtoms::style, val);
    if (eHTMLUnit_ISupports == val.GetUnit()) {
      rule = (nsIStyleRule*) val.GetISupportsValue();
      result = rule->QueryInterface(NS_GET_IID(nsICSSStyleRule), (void**)&cssRule);
      if (NS_OK == result) {
        cssRule->SetDeclaration(aDecl);
        NS_RELEASE(cssRule);
      }
      NS_RELEASE(rule);
    }
  }

  return result;
}

nsresult
nsDOMCSSAttributeDeclaration::ParsePropertyValue(const nsAReadableString& aPropName,
                                                 const nsAReadableString& aPropValue)
{
  nsCOMPtr<nsICSSDeclaration> decl;
  nsresult result = GetCSSDeclaration(getter_AddRefs(decl), PR_TRUE);

  if (!decl) {
    return result;
  }
  
  nsCOMPtr<nsICSSLoader> cssLoader;
  nsCOMPtr<nsICSSParser> cssParser;
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIDocument> doc;

  result = mContent->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(result)) {
    return result;
  }
  
  if (doc) {
    doc->GetBaseURL(*getter_AddRefs(baseURI));

    nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(doc));
    if (htmlContainer) {
      htmlContainer->GetCSSLoader(*getter_AddRefs(cssLoader));
    }
  }

  if (cssLoader) {
    result = cssLoader->GetParserFor(nsnull, getter_AddRefs(cssParser));
  }
  else {
    result = NS_NewCSSParser(getter_AddRefs(cssParser));
  }
  if (NS_FAILED(result)) {
    return result;
  }

  PRInt32 hint;
  if (doc) {
    doc->BeginUpdate();
    doc->AttributeWillChange(mContent, kNameSpaceID_None, nsHTMLAtoms::style);
  }
  
  nsCOMPtr<nsINodeInfo> nodeInfo;
  mContent->GetNodeInfo(*getter_AddRefs(nodeInfo));
  if (nodeInfo) {
    nsCOMPtr<nsIDocument> doc;
    nodeInfo->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(doc));
      if (htmlDoc) {
        nsDTDMode mode;
        htmlDoc->GetDTDMode(mode);
        cssParser->SetQuirkMode(eDTDMode_strict != mode);
      }
    }
  }

  result = cssParser->ParseProperty(aPropName, aPropValue, baseURI, decl, &hint);
  if (doc) {
    doc->AttributeChanged(mContent, kNameSpaceID_None,
                                   nsHTMLAtoms::style,
                                   nsIDOMMutationEvent::MODIFICATION, hint);
    doc->EndUpdate();
  }

  if (cssLoader) {
    cssLoader->RecycleParser(cssParser);
  }

  return NS_OK;
}

nsresult
nsDOMCSSAttributeDeclaration::ParseDeclaration(const nsAReadableString& aDecl,
                                               PRBool aParseOnlyOneDecl,
                                               PRBool aClearOldDecl)
{
  nsCOMPtr<nsICSSDeclaration> decl;
  nsresult result = GetCSSDeclaration(getter_AddRefs(decl), PR_TRUE);

  if (decl) {
    nsCOMPtr<nsICSSLoader> cssLoader;
    nsCOMPtr<nsICSSParser> cssParser;
    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIDocument> doc;

    result = mContent->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      doc->GetBaseURL(*getter_AddRefs(baseURI));

      nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(doc));
      if (htmlContainer) {
        htmlContainer->GetCSSLoader(*getter_AddRefs(cssLoader));
      }
    }

    if (cssLoader) {
      result = cssLoader->GetParserFor(nsnull, getter_AddRefs(cssParser));
    }
    else {
      result = NS_NewCSSParser(getter_AddRefs(cssParser));
    }

    if (NS_SUCCEEDED(result)) {
      PRInt32 hint;
      if (doc) {
        doc->BeginUpdate();

        doc->AttributeWillChange(mContent, kNameSpaceID_None,
                                 nsHTMLAtoms::style);
      }
      nsCOMPtr<nsICSSDeclaration> declClone;
      decl->Clone(*getter_AddRefs(declClone));

      if (aClearOldDecl) {
        // This should be done with decl->Clear() once such a method exists.
        nsAutoString propName;
        PRUint32 count, i;

        decl->Count(&count);

        for (i = 0; i < count; i++) {
          decl->GetNthProperty(0, propName);

          nsCSSProperty prop = nsCSSProps::LookupProperty(propName);
          nsCSSValue val;

          decl->RemoveProperty(prop, val);
        }
      }
  
      nsCOMPtr<nsINodeInfo> nodeInfo;
      mContent->GetNodeInfo(*getter_AddRefs(nodeInfo));
      if (nodeInfo) {
        nsCOMPtr<nsIDocument> doc;
        nodeInfo->GetDocument(*getter_AddRefs(doc));
        if (doc) {
          nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(doc));
          if (htmlDoc) {
            nsDTDMode mode;
            htmlDoc->GetDTDMode(mode);
            cssParser->SetQuirkMode(eDTDMode_strict != mode);
          }
        }
      }

      result = cssParser->ParseAndAppendDeclaration(aDecl, baseURI, decl,
                                                    aParseOnlyOneDecl, &hint);
      if (result == NS_CSS_PARSER_DROP_DECLARATION) {
        SetCSSDeclaration(declClone);
        result = NS_OK;
      }
      if (doc) {
        if (NS_SUCCEEDED(result) && result != NS_CSS_PARSER_DROP_DECLARATION) {
          doc->AttributeChanged(mContent, kNameSpaceID_None,
                                nsHTMLAtoms::style, nsIDOMMutationEvent::MODIFICATION, hint);
        }
        doc->EndUpdate();
      }
      if (cssLoader) {
        cssLoader->RecycleParser(cssParser);
      }
    }
  }

  return result;
}

nsresult
nsDOMCSSAttributeDeclaration::GetParent(nsISupports **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nsnull;

  if (nsnull != mContent) {
    return mContent->QueryInterface(NS_GET_IID(nsISupports), (void **)aParent);
  }

  return NS_OK;
}

//----------------------------------------------------------------------

// this function is a holdover from when attributes were shared
// leaving it in place in case we need to go back to that model
static nsresult EnsureWritableAttributes(nsIHTMLContent* aContent,
                                         nsIHTMLAttributes*& aAttributes, PRBool aCreate)
{
  nsresult  result = NS_OK;

  if (nsnull == aAttributes) {
    if (PR_TRUE == aCreate) {
      result = NS_NewHTMLAttributes(&aAttributes);
    }
  }
  return result;
}

static void ReleaseAttributes(nsIHTMLAttributes*& aAttributes)
{
//  aAttributes->ReleaseContentRef();
  NS_RELEASE(aAttributes);
}

static int gGenericHTMLElementCount = 0;
static nsILanguageAtomService* gLangService = nsnull;


class nsGenericHTMLElementTearoff : public nsIDOMNSHTMLElement,
                                    public nsIDOMElementCSSInlineStyle
{
  NS_DECL_ISUPPORTS

  nsGenericHTMLElementTearoff(nsGenericHTMLElement *aElement)
    : mElement(aElement)
  {
    NS_INIT_REFCNT();
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


nsGenericHTMLElement::nsGenericHTMLElement()
{
  mAttributes = nsnull;
  gGenericHTMLElementCount++;
}

nsGenericHTMLElement::~nsGenericHTMLElement()
{
  if (nsnull != mAttributes) {
    ReleaseAttributes(mAttributes);
  }
  if (!--gGenericHTMLElementCount) {
    NS_IF_RELEASE(gLangService);
  }
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

nsresult
nsGenericHTMLElement::CopyInnerTo(nsIContent* aSrcContent,
                                  nsGenericHTMLElement* aDst,
                                  PRBool aDeep)
{
  nsresult result = NS_OK;

  if (nsnull != mAttributes) {
    result = mAttributes->Clone(&(aDst->mAttributes));

    if (NS_SUCCEEDED(result)) {
      nsHTMLValue val;
      nsresult rv = aDst->GetHTMLAttribute(nsHTMLAtoms::style, val);

      if (rv == NS_CONTENT_ATTR_HAS_VALUE &&
          val.GetUnit() == eHTMLUnit_ISupports) {
        nsCOMPtr<nsISupports> supports(dont_AddRef(val.GetISupportsValue()));
        nsCOMPtr<nsICSSStyleRule> rule(do_QueryInterface(supports));

        if (rule) {
          nsCOMPtr<nsICSSRule> ruleClone;

          result = rule->Clone(*getter_AddRefs(ruleClone));

          val.SetISupportsValue(ruleClone);
          aDst->SetHTMLAttribute(nsHTMLAtoms::style, val, PR_FALSE);
        }
      }
    }
  }

  PRInt32 id;
  if (mDocument) {
    mDocument->GetAndIncrementContentID(&id);
  }

  aDst->SetContentID(id);

  return result;
}

nsresult
nsGenericHTMLElement::GetTagName(nsAWritableString& aTagName)
{
  return GetNodeName(aTagName);
}

nsresult
nsGenericHTMLElement::GetNodeName(nsAWritableString& aNodeName)
{
  nsresult rv = mNodeInfo->GetQualifiedName(aNodeName);

  if (mNodeInfo->NamespaceEquals(kNameSpaceID_None))
    ToUpperCase(aNodeName);

  return rv;
}

nsresult
nsGenericHTMLElement::GetLocalName(nsAWritableString& aLocalName)
{
  mNodeInfo->GetLocalName(aLocalName);

  // This doesn't work for XHTML
  ToUpperCase(aLocalName);

  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetElementsByTagName(const nsAReadableString& aTagname,
                                           nsIDOMNodeList** aReturn)
{
  nsAutoString tagName(aTagname);

  // Only lowercase the name if this element has no namespace (i.e.
  // it's a HTML element, not an XHTML element).
  if (mNodeInfo && mNodeInfo->NamespaceEquals(kNameSpaceID_None))
    tagName.ToLowerCase();

  return nsGenericElement::GetElementsByTagName(tagName, aReturn);
}

// Implementation for nsIDOMHTMLElement
nsresult
nsGenericHTMLElement::GetId(nsAWritableString& aId)
{
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, aId);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetId(const nsAReadableString& aId)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::id, aId, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetTitle(nsAWritableString& aTitle)
{
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::title, aTitle);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetTitle(const nsAReadableString& aTitle)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::title, aTitle, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetLang(nsAWritableString& aLang)
{
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::lang, aLang);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetLang(const nsAReadableString& aLang)
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
nsGenericHTMLElement::GetDir(nsAWritableString& aDir)
{
  nsHTMLValue value;
  nsresult result = GetHTMLAttribute(nsHTMLAtoms::dir, value);

  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    EnumValueToString(value, kDirTable, aDir);
  }

  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetDir(const nsAReadableString& aDir)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::dir, aDir, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::GetClassName(nsAWritableString& aClassName)
{
  GetAttr(kNameSpaceID_None, nsHTMLAtoms::kClass, aClassName);
  return NS_OK;
}

nsresult
nsGenericHTMLElement::SetClassName(const nsAReadableString& aClassName)
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
    slots->mStyle = new nsDOMCSSAttributeDeclaration(this);

    if (nsnull == slots->mStyle) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mStyle);
  }

  res = slots->mStyle->QueryInterface(NS_GET_IID(nsIDOMCSSStyleDeclaration),
                                      (void **)aStyle);

  return res;
}

nsresult
nsGenericHTMLElement::GetOffsetRect(nsRect& aRect,
                                    nsIAtom* aOffsetParentTag,
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

    if (tag.get() == aOffsetParentTag || content == docElement) {
      done = PR_TRUE;

      parent = frame;
    }
  }

  const nsStyleDisplay* display = nsnull;
  nsPoint origin(0, 0);

  if (!done) {
    PRBool is_absolutely_positioned = PR_FALSE;

    frame->GetOrigin(origin);

    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

    if (display && display->IsAbsolutelyPositioned()) {
      // If the primary frame or a parent is absolutely positioned
      // (fixed or absolute) we stop walking up the frame parent
      // chain

      is_absolutely_positioned = PR_TRUE;
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

        // If the tag of this frame matches the one passed in, break here
        if (tag.get() == aOffsetParentTag) {
          if (parent != frame) {
            *aOffsetParent = content;
            NS_ADDREF(*aOffsetParent);
          }

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
  nsresult res = GetOffsetRect(rcFrame,
                               nsHTMLAtoms::body,
                               getter_AddRefs(parent));

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
  nsresult res = GetOffsetRect(rcFrame,
                               nsHTMLAtoms::body,
                               getter_AddRefs(parent));

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
  nsresult res = GetOffsetRect(rcFrame,
                               nsHTMLAtoms::body,
                               getter_AddRefs(parent));

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
  nsresult res = GetOffsetRect(rcFrame,
                               nsHTMLAtoms::body,
                               getter_AddRefs(parent));

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
  nsresult res = GetOffsetRect(rcFrame,
                               nsHTMLAtoms::body,
                               getter_AddRefs(parent));
  if (NS_SUCCEEDED(res)) {
    if (parent) {
      res = parent->QueryInterface(NS_GET_IID(nsIDOMElement),
                                   (void**)aOffsetParent);
    } else {
      *aOffsetParent = nsnull;
    }
  }
  return res;
}

nsresult
nsGenericHTMLElement::GetInnerHTML(nsAWritableString& aInnerHTML)
{
  aInnerHTML.Truncate();

  if (!mDocument) {
    return NS_OK; // We rely on the document for doing HTML conversion
  }

  nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *,
                                                                 this)));
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  docEncoder = do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/html");

  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  docEncoder->Init(mDocument, NS_LITERAL_STRING("text/html"), 0);

  nsCOMPtr<nsIDOMRange> range(new nsRange);
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);

  rv = range->SelectNodeContents(thisNode);
  NS_ENSURE_SUCCESS(rv, rv);

  docEncoder->SetRange(range);

  docEncoder->EncodeToString(aInnerHTML);

  return rv;
}

nsresult
nsGenericHTMLElement::SetInnerHTML(const nsAReadableString& aInnerHTML)
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

  rv = nsrange->CreateContextualFragment(aInnerHTML, getter_AddRefs(df));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> tmpNode;
  return thisNode->AppendChild(df, getter_AddRefs(tmpNode));
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
  PRBool status = PR_FALSE;
  if (aDoc) {
    nsCompatibility mode;
    // multiple shells on the same doc are out of luck
    nsCOMPtr<nsIPresShell> shell;
    aDoc->GetShellAt(0, getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIPresContext> presContext;
      shell->GetPresContext(getter_AddRefs(presContext));
      if (presContext) {
        presContext->GetCompatibilityMode(&mode);
        if (eCompatibility_NavQuirks == mode) {
          status = PR_TRUE;
        }
      }
    }
  }
  return status;
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
  PRInt32 nameSpaceID;

  *aForm = nsnull;

  while (content) {
    content->GetTag(*getter_AddRefs(tag));
    content->GetNameSpaceID(nameSpaceID);

    // If the current ancestor is a form, return it as our form
    if ((tag.get() == nsHTMLAtoms::form) &&
        (kNameSpaceID_HTML == nameSpaceID)) {
      return CallQueryInterface(content, aForm);
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

  if ((NS_OK == ret) && (aEvent->message == NS_MOUSE_ENTER_SYNTH || 
                         aEvent->message == NS_MOUSE_EXIT_SYNTH ||
                         nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE)) {
    // If we're here, then aOuter should be an nsILink. We'll use the
    // nsILink interface to get a canonified URL that has been
    // correctly escaped and URL-encoded for the document's charset.
    nsCOMPtr<nsILink> link = do_QueryInterface(aOuter);
    NS_ASSERTION(link != nsnull, "aOuter is not an nsILink");

    if (!link)
      return NS_ERROR_UNEXPECTED;

    nsXPIDLCString hrefCStr;
    link->GetHrefCString(*getter_Copies(hrefCStr));

    // Only bother to handle the mouse event if there was an href
    // specified.
    if (hrefCStr) {
      nsAutoString href;
      href.AssignWithConversion(hrefCStr);
      // Strip off any unneeded CF/LF (for Bug 52119)
      // It can't be done in the parser because of Bug 15204
      href.StripChars("\r\n");

      switch (aEvent->message) {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
        {
          // don't make the link grab the focus if there is no link handler
          nsCOMPtr<nsILinkHandler> handler;
          nsresult rv = aPresContext->GetLinkHandler(getter_AddRefs(handler));
          if (NS_SUCCEEDED(rv) && handler) {
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
          GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
          if (target.Length() == 0) {
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

      case NS_MOUSE_ENTER_SYNTH:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(this, NS_EVENT_STATE_HOVER);
          NS_RELEASE(stateManager);
        }
        *aEventStatus = nsEventStatus_eConsumeNoDefault;
      }
      // Set the status bar the same for focus and mouseover
      case NS_FOCUS_CONTENT:
      {
        nsAutoString target;
        nsCOMPtr<nsIURI> baseURL;
        GetBaseURL(*getter_AddRefs(baseURL));
        GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
        if (target.Length() == 0) {
          GetBaseTarget(target);
        }
        ret = TriggerLink(aPresContext, eLinkVerb_Replace,
                          baseURL, href, target, PR_FALSE);
      }
      break;

      case NS_MOUSE_EXIT_SYNTH:
      {
        nsIEventStateManager *stateManager;
        if (NS_OK == aPresContext->GetEventStateManager(&stateManager)) {
          stateManager->SetContentState(nsnull, NS_EVENT_STATE_HOVER);
          NS_RELEASE(stateManager);
        }
        *aEventStatus = nsEventStatus_eConsumeNoDefault;

        nsAutoString empty;
        ret = TriggerLink(aPresContext, eLinkVerb_Replace, nsnull, empty,
                          empty, PR_FALSE);
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
  aID = kNameSpaceID_HTML;
  return NS_OK;
}

nsresult
nsGenericHTMLElement::NormalizeAttrString(const nsAReadableString& aStr,
                                          nsINodeInfo*& aNodeInfo)
{
  // XXX need to validate/strip namespace prefix
  nsAutoString lower(aStr);
  lower.ToLowerCase();

  nsCOMPtr<nsINodeInfoManager> nimgr;
  mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  return nimgr->GetNodeInfo(lower, nsnull, kNameSpaceID_None, aNodeInfo);
}

nsresult
nsGenericHTMLElement::SetAttr(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              const nsAReadableString& aValue,
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
    if (mDocument) {
      nsHTMLValue parsedValue;
      ParseStyleAttribute(aValue, parsedValue);
      result = SetHTMLAttribute(aAttribute, parsedValue, aNotify);
    }
    else {  // store it as string, will parse it later
      result = SetHTMLAttribute(aAttribute, nsHTMLValue(aValue), aNotify);
    }
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

  if (NS_CONTENT_ATTR_NOT_THERE !=
      StringToAttribute(aAttribute, aValue, val)) {
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
    PRBool  impact = NS_STYLE_HINT_NONE;
    PRInt32 modHint = modification ? PRInt32(nsIDOMMutationEvent::MODIFICATION)
                                   : PRInt32(nsIDOMMutationEvent::ADDITION);
    GetMappedAttributeImpact(aAttribute, modHint, impact);

    nsCOMPtr<nsIHTMLStyleSheet> sheet(dont_AddRef(GetAttrStyleSheet(mDocument)));
    if (sheet) { // set attr via style sheet
      result = sheet->SetAttributeFor(aAttribute, aValue,
                                      (NS_STYLE_HINT_CONTENT < impact),
                                      this, mAttributes);
    }
    else {  // manage this ourselves and re-sync when we connect to doc
      result = EnsureWritableAttributes(this, mAttributes, PR_TRUE);
      if (mAttributes) {
        PRInt32   count;
        result = mAttributes->SetAttributeFor(aAttribute, aValue,
                                              (NS_STYLE_HINT_CONTENT < impact),
                                              this, nsnull, count);
        if (0 == count) {
          ReleaseAttributes(mAttributes);
        }
      }
    }
  }

  if (mDocument) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(aAttribute, aNameSpaceID, PR_FALSE);

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
      if (!strValue.IsEmpty())
        mutation.mPrevAttrValue = getter_AddRefs(NS_NewAtom(strValue));
      if (!aValue.IsEmpty())
        mutation.mNewAttrValue = getter_AddRefs(NS_NewAtom(aValue));
      if (modification)
        mutation.mAttrChange = nsIDOMMutationEvent::MODIFICATION;
      else
        mutation.mAttrChange = nsIDOMMutationEvent::ADDITION;
      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }

    if (aNotify) {
      PRInt32 modHint = modification ? PRInt32(nsIDOMMutationEvent::MODIFICATION)
                                     : PRInt32(nsIDOMMutationEvent::ADDITION);
      mDocument->AttributeChanged(this, aNameSpaceID, aAttribute, modHint, 
                                  NS_STYLE_HINT_UNKNOWN);
      mDocument->EndUpdate();
    }
  }

  return result;
}

NS_IMETHODIMP
nsGenericHTMLElement::SetAttr(nsINodeInfo* aNodeInfo,
                              const nsAReadableString& aValue,
                              PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsCOMPtr<nsIAtom> atom;
  PRInt32 nsid;

  aNodeInfo->GetNameAtom(*getter_AddRefs(atom));
  aNodeInfo->GetNamespaceID(nsid);

  // We still rely on the old way of setting the attribute.

  return SetAttr(nsid, atom, aValue, aNotify);
}

PRBool nsGenericHTMLElement::IsEventName(nsIAtom* aName)
{
  return (nsLayoutAtoms::onclick == aName ||
    nsLayoutAtoms::ondblclick == aName ||
    nsLayoutAtoms::onmousedown == aName ||
    nsLayoutAtoms::onmouseup == aName ||
    nsLayoutAtoms::onmouseover == aName ||
    nsLayoutAtoms::onmouseout == aName ||
    nsLayoutAtoms::onkeydown == aName ||
    nsLayoutAtoms::onkeyup == aName ||
    nsLayoutAtoms::onkeypress == aName ||
    nsLayoutAtoms::onmousemove == aName ||
    nsLayoutAtoms::onload == aName ||
    nsLayoutAtoms::onunload == aName ||
    nsLayoutAtoms::onabort == aName ||
    nsLayoutAtoms::onerror == aName ||
    nsLayoutAtoms::onfocus == aName ||
    nsLayoutAtoms::onblur == aName ||
    nsLayoutAtoms::onsubmit == aName ||
    nsLayoutAtoms::onreset == aName ||
    nsLayoutAtoms::onchange == aName ||
    nsLayoutAtoms::onselect == aName || 
    nsLayoutAtoms::onpaint == aName ||
    nsLayoutAtoms::onresize == aName ||
    nsLayoutAtoms::onscroll == aName ||
    nsLayoutAtoms::oninput == aName ||
    nsLayoutAtoms::oncontextmenu == aName || 
    nsLayoutAtoms::onDOMAttrModified == aName ||
    nsLayoutAtoms::onDOMCharacterDataModified == aName || 
    nsLayoutAtoms::onDOMSubtreeModified == aName ||
    nsLayoutAtoms::onDOMNodeInsertedIntoDocument == aName || 
    nsLayoutAtoms::onDOMNodeRemovedFromDocument == aName ||
    nsLayoutAtoms::onDOMNodeInserted  == aName || 
    nsLayoutAtoms::onDOMNodeRemoved == aName
    );
}

static PRInt32 GetStyleImpactFrom(const nsHTMLValue& aValue)
{
  PRInt32 hint = NS_STYLE_HINT_NONE;

  if (eHTMLUnit_ISupports == aValue.GetUnit()) {
    nsCOMPtr<nsISupports> supports(dont_AddRef(aValue.GetISupportsValue()));
    if (supports) {
      nsCOMPtr<nsICSSStyleRule> cssRule(do_QueryInterface(supports));

      if (cssRule) {
        nsCOMPtr<nsICSSDeclaration> declaration(dont_AddRef(cssRule->GetDeclaration()));

        if (declaration) {
          declaration->GetStyleImpact(&hint);
        }
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

  PRBool  impact = NS_STYLE_HINT_NONE;
  GetMappedAttributeImpact(aAttribute, nsIDOMMutationEvent::MODIFICATION, impact);
  nsCOMPtr<nsIHTMLStyleSheet> sheet;
  if (mDocument) {
    if (aNotify) {
      mDocument->BeginUpdate();

      mDocument->AttributeWillChange(this, kNameSpaceID_None, aAttribute);

      if (nsHTMLAtoms::style == aAttribute) {
        nsHTMLValue oldValue;
        PRInt32 oldImpact = NS_STYLE_HINT_NONE;
        if (NS_CONTENT_ATTR_NOT_THERE != GetHTMLAttribute(aAttribute,
                                                          oldValue)) {
          oldImpact = GetStyleImpactFrom(oldValue);
        }
        impact = GetStyleImpactFrom(aValue);
        if (impact < oldImpact) {
          impact = oldImpact;
        }
      }
    }
    sheet = dont_AddRef(GetAttrStyleSheet(mDocument));
    if (sheet) { // set attr via style sheet
        result = sheet->SetAttributeFor(aAttribute, aValue,
                                        (NS_STYLE_HINT_CONTENT < impact),
                                        this, mAttributes);
    }

    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(aAttribute, kNameSpaceID_None, PR_TRUE);

    if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
      // XXX Figure out how to get the old value, so I can fill in
      // the prevValue field and so that I can correctly indicate
      // MODIFICATIONs/ADDITIONs.
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
      nsAutoString value;
      aValue.ToString(value);
      if (!value.IsEmpty())
        mutation.mNewAttrValue = getter_AddRefs(NS_NewAtom(value));
      mutation.mAttrChange = nsIDOMMutationEvent::MODIFICATION;
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
    result = EnsureWritableAttributes(this, mAttributes, PR_TRUE);
    if (mAttributes) {
      PRInt32   count;
      result = mAttributes->SetAttributeFor(aAttribute, aValue,
                                            (NS_STYLE_HINT_CONTENT < impact),
                                            this, nsnull, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }

  return result;
}

nsresult
nsGenericHTMLElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRBool aNotify)
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

  // Check for event handlers
  if (IsEventName(aAttribute)) {
    nsCOMPtr<nsIEventListenerManager> manager;
    GetListenerManager(getter_AddRefs(manager));

    if (manager) {
      result = manager->RemoveScriptEventListener(aAttribute);
    }
  }

  nsCOMPtr<nsIHTMLStyleSheet> sheet;
  if (mDocument) {
    PRInt32 impact = NS_STYLE_HINT_UNKNOWN;
    if (aNotify) {
      mDocument->BeginUpdate();

      mDocument->AttributeWillChange(this, aNameSpaceID, aAttribute);

      if (nsHTMLAtoms::style == aAttribute) {
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

      nsHTMLValue oldAttr;
      GetHTMLAttribute(aAttribute, oldAttr);
      nsAutoString attr;
      oldAttr.ToString(attr);
      if (!attr.IsEmpty())
        mutation.mPrevAttrValue = getter_AddRefs(NS_NewAtom(attr));
      mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }

    sheet = dont_AddRef(GetAttrStyleSheet(mDocument));
    if (sheet) { // set attr via style sheet
      result = sheet->UnsetAttributeFor(aAttribute, this, mAttributes);
    }

    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(aAttribute, aNameSpaceID, PR_TRUE);

    if (aNotify) {
      mDocument->AttributeChanged(this, aNameSpaceID, aAttribute, nsIDOMMutationEvent::REMOVAL, impact);
      mDocument->EndUpdate();
    }
  }
  if (!sheet) {  // manage this ourselves and re-sync when we connect to doc
    result = EnsureWritableAttributes(this, mAttributes, PR_FALSE);
    if (mAttributes) {
      PRInt32 count;
      result = mAttributes->UnsetAttributeFor(aAttribute, this, nsnull, count);
      if (0 == count) {
        ReleaseAttributes(mAttributes);
      }
    }
  }

  return result;
}

nsresult
nsGenericHTMLElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                              nsIAtom*& aPrefix, nsAWritableString& aResult) const
{
  aPrefix = nsnull;

  return GetAttr(aNameSpaceID, aAttribute, aResult);
}

nsresult
nsGenericHTMLElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                              nsAWritableString& aResult) const
{
  aResult.SetLength(0);

#if 0
  NS_ASSERTION((kNameSpaceID_HTML == aNameSpaceID) ||
               (kNameSpaceID_None == aNameSpaceID) ||
               (kNameSpaceID_Unknown == aNameSpaceID),
               "html content only holds HTML attributes");
#endif

  if ((kNameSpaceID_HTML != aNameSpaceID) &&
      (kNameSpaceID_None != aNameSpaceID) &&
      (kNameSpaceID_Unknown != aNameSpaceID)) {
    return NS_CONTENT_ATTR_NOT_THERE;
  }

  const nsHTMLValue*  value;
  nsresult  result = mAttributes ? mAttributes->GetAttribute(aAttribute, &value) :
                     NS_CONTENT_ATTR_NOT_THERE;

  nscolor color;
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    // Try subclass conversion routine first
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        AttributeToString(aAttribute, *value, aResult)) {
      return result;
    }

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
nsGenericHTMLElement::GetAttrNameAt(PRInt32 aIndex,
                                    PRInt32& aNameSpaceID,
                                    nsIAtom*& aName,
                                    nsIAtom*& aPrefix) const
{
  aNameSpaceID = kNameSpaceID_None;
  aPrefix = nsnull;
  if (nsnull != mAttributes) {
    return mAttributes->GetAttributeNameAt(aIndex, aName);
  }
  aName = nsnull;
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

nsresult
nsGenericHTMLElement::HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const
{
  if (nsnull != mAttributes) {
    return mAttributes->HasClass(aClass, aCaseSensitive);
  }
  return NS_COMFALSE;
}

nsresult
nsGenericHTMLElement::WalkContentStyleRules(nsIRuleWalker* aRuleWalker)
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
nsGenericHTMLElement::WalkInlineStyleRules(nsIRuleWalker* aRuleWalker)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIStyleRule> rule;
  
  if (aRuleWalker && mAttributes) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == mAttributes->GetAttribute(nsHTMLAtoms::style, value)) {
      if (eHTMLUnit_ISupports == value.GetUnit()) {
        nsCOMPtr<nsISupports> supports = getter_AddRefs(value.GetISupportsValue());
        if (supports)
          rule = do_QueryInterface(supports, &result);
      }
    }
  }

  if (rule)
    aRuleWalker->Forward(rule);

  return result;
}

nsresult
nsGenericHTMLElement::GetBaseURL(nsIURI*& aBaseURL) const
{
  nsHTMLValue baseHref;
  if (mAttributes) {
    mAttributes->GetAttribute(nsHTMLAtoms::_baseHref, baseHref);
  }
  return GetBaseURL(baseHref, mDocument, &aBaseURL);
}

nsresult
nsGenericHTMLElement::GetBaseURL(const nsHTMLValue& aBaseHref,
                                 nsIDocument* aDocument,
                                 nsIURI** aBaseURL)
{
  nsresult result = NS_OK;

  nsIURI* docBaseURL = nsnull;
  if (nsnull != aDocument) {
    result = aDocument->GetBaseURL(docBaseURL);
  }
  *aBaseURL = docBaseURL;

  if (eHTMLUnit_String == aBaseHref.GetUnit()) {
    nsAutoString baseHref;
    aBaseHref.GetStringValue(baseHref);
    baseHref.Trim(" \t\n\r");

    nsIURI* url = nsnull;
    {
      result = NS_NewURI(&url, baseHref, docBaseURL);
    }
    NS_IF_RELEASE(docBaseURL);
    *aBaseURL = url;
  }
  return result;
}

nsresult
nsGenericHTMLElement::GetBaseTarget(nsAWritableString& aBaseTarget) const
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
    buffer.AppendWithConversion("=");
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
  fprintf(out, "@%p", this);

  ListAttributes(out);

  fprintf(out, " refcount=%d<", mRefCnt);

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

  nsIAtom* tag;
  nsAutoString buf;
  GetTag(tag);
  if (tag != nsnull) {
    tag->ToString(buf);
    fputs("<",out);
    fputs(buf, out);

    if(aDumpAll) ListAttributes(out);

    fputs(">",out);
    NS_RELEASE(tag);
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
    fputs(buf, out);
    fputs(">",out);

    if(aIndent) fputs("\n", out);
  }

  return NS_OK;
}


NS_IMETHODIMP_(PRBool)
nsGenericHTMLElement::IsContentOfType(PRUint32 aFlags)
{
  return !(aFlags & ~(eELEMENT | eHTML));
}

PRUint32
nsGenericHTMLElement::BaseSizeOf(nsISizeOfHandler* aSizer) const
{
  PRUint32 sum = 0;
#ifdef DEBUG
  if (mAttributes) {
    PRUint32 attrs = 0;
    mAttributes->SizeOf(aSizer, attrs);
    sum += attrs;
  }
#endif
  return sum;
}


//----------------------------------------------------------------------


nsresult
nsGenericHTMLElement::AttributeToString(nsIAtom* aAttribute,
                                        const nsHTMLValue& aValue,
                                        nsAWritableString& aResult) const
{
  if (nsHTMLAtoms::style == aAttribute) {
    if (eHTMLUnit_ISupports == aValue.GetUnit()) {
      nsIStyleRule* rule = (nsIStyleRule*) aValue.GetISupportsValue();
      if (rule) {
        nsICSSStyleRule*  cssRule;
        if (NS_OK == rule->QueryInterface(NS_GET_IID(nsICSSStyleRule), (void**)&cssRule)) {
          nsICSSDeclaration* decl = cssRule->GetDeclaration();
          if (nsnull != decl) {
            decl->ToString(aResult);
            NS_RELEASE(decl);
          }
          NS_RELEASE(cssRule);
        }
        else {
          aResult.Assign(NS_LITERAL_STRING("Unknown rule type"));
        }
        NS_RELEASE(rule);
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
                                               PRInt32& aHint) const
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
nsGenericHTMLElement::ParseEnumValue(const nsAReadableString& aValue,
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
nsGenericHTMLElement::ParseCaseSensitiveEnumValue(const nsAReadableString& aValue,
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
                                        nsAWritableString& aResult,
                                        PRBool aFoldCase)
{
  aResult.Truncate(0);
  if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
    PRInt32 v = aValue.GetIntValue();
    while (nsnull != aTable->tag) {
      if (aTable->value == v) {
        aResult.Append(NS_ConvertASCIItoUCS2(aTable->tag));
        if (aFoldCase) {
          nsWritingIterator<PRUnichar> start; aResult.BeginWriting(start);
          *start.get() = nsCRT::ToUpper(*start.get());
        }
        return PR_TRUE;
      }
      aTable++;
    }
  }
  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseValueOrPercent(const nsAReadableString& aString,
                                          nsHTMLValue& aResult,
                                          nsHTMLUnit aValueUnit)
{
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if (NS_OK == ec) {
    if (val < 0) val = 0;
    if (tmp.Length() && tmp.RFindChar('%') >= 0) {/* XXX not 100% compatible with ebina's code */
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

  return PR_FALSE;
}

/* used to parse attribute values that could be either:
 *   integer  (n),
 *   percent  (n%),
 *   or proportional (n*)
 */
PRBool
nsGenericHTMLElement::ParseValueOrPercentOrProportional(const nsAReadableString& aString,
                                                        nsHTMLValue& aResult,
                                                        nsHTMLUnit aValueUnit)
{
  nsAutoString tmp(aString);
  tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
  PRInt32 ec, val = tmp.ToInteger(&ec);
  if ((NS_ERROR_ILLEGAL_VALUE == ec) && (tmp.Length() > 0)) {
    // NOTE: we need to allow non-integer values for the '*' case,
    //       so we allow for the ILLEGAL_VALUE error and set val to 0
    val = 0;
    ec = NS_OK;
  }
  if (NS_OK == ec) {
    if (val < 0) val = 0;
    if (tmp.Length() && tmp.RFindChar('%') >= 0) {/* XXX not 100% compatible with ebina's code */
      if (val > 100) val = 100;
      aResult.SetPercentValue(float(val)/100.0f);
    } else if (tmp.Length() && tmp.Last() == '*') {
      if (tmp.Length() == 1) {
        // special case: HTML spec says a value '*' == '1*'
        // see http://www.w3.org/TR/html4/types.html#type-multi-length
        // b=29061
        val = 1;
      }
      aResult.SetIntValue(val, eHTMLUnit_Proportional); // proportional values are integers
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

PRBool
nsGenericHTMLElement::ValueOrPercentToString(const nsHTMLValue& aValue,
                                             nsAWritableString& aResult)
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
                                                           nsAWritableString& aResult)
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
nsGenericHTMLElement::ParseValue(const nsAReadableString& aString, PRInt32 aMin,
                                 nsHTMLValue& aResult, nsHTMLUnit aValueUnit)
{
  nsAutoString str(aString);
  PRInt32 ec, val = str.ToInteger(&ec);
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

  return PR_FALSE;
}

PRBool
nsGenericHTMLElement::ParseValue(const nsAReadableString& aString, PRInt32 aMin,
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
nsGenericHTMLElement::ParseColor(const nsAReadableString& aString,
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
                                    nsAWritableString& aResult)
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

// XXX This creates a dependency between content and frames
nsresult
nsGenericHTMLElement::GetPrimaryFrame(nsIHTMLContent* aContent,
                                      nsIFormControlFrame *&aFormControlFrame,
                                      PRBool aFlushNotifications)
{
  nsIDocument* doc = nsnull;
  nsresult res = NS_NOINTERFACE;
   // Get the document
  if (NS_OK == aContent->GetDocument(doc)) {
    if (nsnull != doc) {
      if (aFlushNotifications) {
        // Cause a flushing of notifications, so we get
        // up-to-date presentation information
        doc->FlushPendingNotifications();
      }

       // Get presentation shell 0
      nsCOMPtr<nsIPresShell> presShell;
      doc->GetShellAt(0, getter_AddRefs(presShell));
      if (nsnull != presShell) {
        nsIFrame *frame = nsnull;
        presShell->GetPrimaryFrameFor(aContent, &frame);
        if (nsnull != frame) {
          res = frame->QueryInterface(NS_GET_IID(nsIFormControlFrame),
                                      (void**)&aFormControlFrame);
        }
      }
      NS_RELEASE(doc);
    }
  }

  return res;
}

nsresult
nsGenericHTMLElement::GetPrimaryPresState(nsIHTMLContent* aContent,
                                          nsIPresState** aPresState)
{
  NS_ENSURE_ARG_POINTER(aPresState);
  *aPresState = nsnull;

  nsresult result = NS_OK;

  // Generate the state key
  nsCOMPtr<nsIDocument> doc;
  result = aContent->GetDocument(*getter_AddRefs(doc));
  if (!doc) {
    return result;
  }

  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFrameManager> frameManager;
  presShell->GetFrameManager(getter_AddRefs(frameManager));
  NS_ENSURE_TRUE(frameManager, NS_ERROR_FAILURE);

  nsCAutoString stateKey;
  result = frameManager->GenerateStateKey(aContent, nsIStatefulFrame::eNoID, stateKey);
  NS_ENSURE_TRUE((NS_SUCCEEDED(result) && !stateKey.IsEmpty()), result);

  // Get the pres state for this key, if it doesn't exist, create one
  //
  // Return early if we can't get history - we don't want to create a
  // new history state that is free-floating, not in history.
  nsCOMPtr<nsILayoutHistoryState> history;
  result = presShell->GetHistoryState(getter_AddRefs(history));
  NS_ENSURE_TRUE(NS_SUCCEEDED(result) && history, result);

  history->GetState(stateKey, aPresState);
  if (!*aPresState) {
    result = nsComponentManager::CreateInstance(kPresStateCID, nsnull,
                                                NS_GET_IID(nsIPresState),
                                                (void**)aPresState);
    if (NS_SUCCEEDED(result)) {
      result = history->AddState(stateKey, *aPresState);
    }
  }

  return result;
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

static nsGenericHTMLElement::EnumTable kDivAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { "middle", NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
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

static nsGenericHTMLElement::EnumTable kTableVAlignTable[] = {
  { "top",     NS_STYLE_VERTICAL_ALIGN_TOP },
  { "middle",  NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { "bottom",  NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "baseline",NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { 0 }
};

PRBool
nsGenericHTMLElement::ParseCommonAttribute(nsIAtom* aAttribute,
                                           const nsAReadableString& aValue,
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
nsGenericHTMLElement::ParseAlignValue(const nsAReadableString& aString,
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
nsGenericHTMLElement::ParseTableHAlignValue(const nsAReadableString& aString,
                                            nsHTMLValue& aResult) const
{
  if (InNavQuirksMode(mDocument)) {
    return ParseEnumValue(aString, kCompatTableHAlignTable, aResult);
  }
  return ParseEnumValue(aString, kTableHAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::TableHAlignValueToString(const nsHTMLValue& aValue,
                                               nsAWritableString& aResult) const
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
nsGenericHTMLElement::ParseTableCellHAlignValue(const nsAReadableString& aString,
                                                nsHTMLValue& aResult) const
{
  if (InNavQuirksMode(mDocument)) {
    return ParseEnumValue(aString, kCompatTableCellHAlignTable, aResult);
  }
  return ParseEnumValue(aString, kTableCellHAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::TableCellHAlignValueToString(const nsHTMLValue& aValue,
                                                   nsAWritableString& aResult) const
{
  if (InNavQuirksMode(mDocument)) {
    return EnumValueToString(aValue, kCompatTableCellHAlignTable, aResult);
  }
  return EnumValueToString(aValue, kTableCellHAlignTable, aResult);
}

//----------------------------------------

PRBool
nsGenericHTMLElement::ParseTableVAlignValue(const nsAReadableString& aString,
                                            nsHTMLValue& aResult)
{
  return ParseEnumValue(aString, kTableVAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::AlignValueToString(const nsHTMLValue& aValue,
                                         nsAWritableString& aResult)
{
  return EnumValueToString(aValue, kAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::TableVAlignValueToString(const nsHTMLValue& aValue,
                                               nsAWritableString& aResult)
{
  return EnumValueToString(aValue, kTableVAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::ParseDivAlignValue(const nsAReadableString& aString,
                                         nsHTMLValue& aResult) const
{
  return ParseEnumValue(aString, kDivAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::DivAlignValueToString(const nsHTMLValue& aValue,
                                            nsAWritableString& aResult) const
{
  return EnumValueToString(aValue, kDivAlignTable, aResult);
}

PRBool
nsGenericHTMLElement::ParseImageAttribute(nsIAtom* aAttribute,
                                          const nsAReadableString& aString,
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
                                             nsAWritableString& aResult)
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
                                            const nsAReadableString& aString,
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
                                               nsAWritableString& aResult)
{
  if (aStandardMode) {
    return EnumValueToString(aValue, kFrameborderStandardTable, aResult);
  } else {
    return EnumValueToString(aValue, kFrameborderQuirksTable, aResult);
  }
}

PRBool
nsGenericHTMLElement::ParseScrollingValue(PRBool aStandardMode,
                                          const nsAReadableString& aString,
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
                                             nsAWritableString& aResult)
{
  if (aStandardMode) {
    return EnumValueToString(aValue, kScrollingStandardTable, aResult);
  } else {
    return EnumValueToString(aValue, kScrollingQuirksTable, aResult);
  }
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
nsGenericHTMLElement::ParseStyleAttribute(const nsAReadableString& aValue, nsHTMLValue& aResult)
{
  nsresult result = NS_OK;

  if (mDocument) {
    PRBool isCSS = PR_TRUE; // asume CSS until proven otherwise

    nsAutoString styleType;
    mDocument->GetHeaderData(nsHTMLAtoms::headerContentStyleType, styleType);
    if (!styleType.IsEmpty()) {
      static const char textCssStr[] = "text/css";
      isCSS = (styleType.EqualsIgnoreCase(textCssStr, sizeof(textCssStr) - 1));
    }

    if (isCSS) {
      nsCOMPtr<nsICSSLoader> cssLoader;
      nsCOMPtr<nsICSSParser> cssParser;
      nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(mDocument));

      if (htmlContainer) {
        htmlContainer->GetCSSLoader(*getter_AddRefs(cssLoader));
      }
      if (cssLoader) {
        result = cssLoader->GetParserFor(nsnull, getter_AddRefs(cssParser));

        static const char charsetStr[] = "charset=";
        PRInt32 charsetOffset = styleType.Find(charsetStr, PR_TRUE);
        if (charsetOffset > 0) {
          nsString charset;
          styleType.Right(charset, styleType.Length() -
                                   (charsetOffset + sizeof(charsetStr) - 1));
          (void)cssLoader->SetCharset(charset);
        }
      }
      else {
        result = NS_NewCSSParser(getter_AddRefs(cssParser));
      }
      if (cssParser) {
        nsCOMPtr<nsIURI> docURL;
        mDocument->GetBaseURL(*getter_AddRefs(docURL));

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

static void PostResolveCallback(nsStyleStruct* aStyleStruct, nsRuleData* aRuleData)
{
  if (!aRuleData->mAttributes)
    return;

  nsHTMLValue value;
  aRuleData->mAttributes->GetAttribute(nsHTMLAtoms::lang, value);
  if (value.GetUnit() == eHTMLUnit_String) {
    if (!gLangService) {
      nsServiceManager::GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID,
        NS_GET_IID(nsILanguageAtomService), (nsISupports**) &gLangService);
      if (!gLangService) {
        return;
      }
    }
    nsStyleVisibility* vis = (nsStyleVisibility*)aStyleStruct;
    
    nsAutoString lang;
    value.GetStringValue(lang);
    gLangService->LookupLanguage(lang.get(),
      getter_AddRefs(vis->mLanguage));
  }
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
      // Register a post-resolve callback for filling in the language atom
      // over in the computed style data.
      aData->mAttributes = (nsIHTMLMappedAttributes*)aAttributes;
      aData->mPostResolveCallback = &PostResolveCallback;
    }
  }
}

PRBool
nsGenericHTMLElement::GetCommonMappedAttributesImpact(const nsIAtom* aAttribute,
                                                      PRInt32& aHint)
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
                                                     PRInt32& aHint)
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

PRBool
nsGenericHTMLElement::GetImageAlignAttributeImpact(const nsIAtom* aAttribute,
                                                   PRInt32& aHint)
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
                                                    PRInt32& aHint)
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
        if (spec.Length() > 0) {
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
                aData->mColorData->mBackImage.SetStringValue(absURLSpec, eCSSUnit_URL);
            }
          }
        }
      } else if (aData->mPresContext) {
        // in NavQuirks mode, allow the empty string to set the background to empty
        nsCompatibility mode;
        aData->mPresContext->GetCompatibilityMode(&mode);
        if (eCompatibility_NavQuirks == mode &&
            eHTMLUnit_Empty == value.GetUnit())
          aData->mColorData->mBackImage.SetStringValue(NS_LITERAL_STRING(""), eCSSUnit_URL);
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
                                                    PRInt32& aHint)
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
    PRInt32 index;
    PRInt32 count = mChildren.Count();
    for (index = 0; index < count; index++) {
      nsIContent* child = (nsIContent*)mChildren.ElementAt(index);

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

  if (nsnull == slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(this);
    NS_ADDREF(slots->mChildNodes);
  }

  return slots->mChildNodes->QueryInterface(NS_GET_IID(nsIDOMNodeList), (void **)aChildNodes);
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
  nsIContent *child = (nsIContent *)mChildren.ElementAt(0);
  if (child) {
    nsresult res = child->QueryInterface(NS_GET_IID(nsIDOMNode),
                                         (void**)aNode);

    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  *aNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerElement::GetLastChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(mChildren.Count()-1);
  if (child) {
    nsresult res = child->QueryInterface(NS_GET_IID(nsIDOMNode),
                                         (void**)aNode);

    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
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
  nsIContent *child = (nsIContent *)mChildren.ElementAt(aIndex);
  if (nsnull != child) {
    NS_ADDREF(child);
  }
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
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
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
    oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
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
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
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

//----------------------------------------------------------------------

nsGenericHTMLContainerFormElement::nsGenericHTMLContainerFormElement()
{
  mForm = nsnull;
}

nsGenericHTMLContainerFormElement::~nsGenericHTMLContainerFormElement()
{
  // Do nothing
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

  if (aRemoveFromForm) {
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, idVal);

    if (mForm) {
      mForm->RemoveElement(this);

      if (!nameVal.IsEmpty())
        mForm->RemoveElementFromTable(this, nameVal);

      if (!idVal.IsEmpty())
        mForm->RemoveElementFromTable(this, idVal);
    }
  }

  if (aForm) {
    nsCOMPtr<nsIForm> theForm(do_QueryInterface(aForm));
    mForm = theForm;  // Even if we fail, update mForm (nsnull in failure)

    if (theForm) {
      theForm->AddElement(this);

      if (!nameVal.IsEmpty())
        theForm->AddElementToTable(this, nameVal);

      if (!idVal.IsEmpty())
        theForm->AddElementToTable(this, idVal);
    }
  } else {
    mForm = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  *aForm = nsnull;

  if (mForm) {
    mForm->QueryInterface(NS_GET_IID(nsIDOMHTMLFormElement), (void**)aForm);
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

  if (aDocument && mParent && !mForm) {
    rv = FindAndSetForm(this);
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
                                              const nsAReadableString& aValue,
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

    aForm->RemoveElement(thisControl);
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

    aForm->AddElement(thisControl);
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
                                           const nsAReadableString& aVal,
                                           PRBool aNotify)
{
  return SetFormControlAttribute(mForm, aNameSpaceID, aName, aVal, aNotify);
}

NS_IMETHODIMP
nsGenericHTMLContainerFormElement::SetAttr(nsINodeInfo* aNodeInfo,
                                           const nsAReadableString& aValue,
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
  // Do nothing
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

  if (aRemoveFromForm) {
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, idVal);

    if (mForm) {
      mForm->RemoveElement(this);

      if (nameVal.Length())
        mForm->RemoveElementFromTable(this, nameVal);

      if (idVal.Length())
        mForm->RemoveElementFromTable(this, idVal);
    }
  }

  if (aForm) {
    nsCOMPtr<nsIForm> theForm = do_QueryInterface(aForm);
    mForm = theForm;  // Even if we fail, update mForm (nsnull in failure)
    if (theForm) {
      theForm->AddElement(this);

      if (nameVal.Length())
        theForm->AddElementToTable(this, nameVal);

      if (idVal.Length())
        theForm->AddElementToTable(this, idVal);
    }
  } else {
    mForm = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  *aForm = nsnull;

  if (mForm) {
    mForm->QueryInterface(NS_GET_IID(nsIDOMHTMLFormElement), (void**)aForm);
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

  if (aDocument && mParent && !mForm) {
    rv = FindAndSetForm(this);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = nsGenericHTMLElement::SetDocument(aDocument, aDeep,
                                           aCompileEventHandlers);
  }

  return rv;
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::SetAttr(PRInt32 aNameSpaceID,
                                      nsIAtom* aName,
                                      const nsAReadableString& aValue,
                                      PRBool aNotify)
{
  return SetFormControlAttribute(mForm, aNameSpaceID, aName, aValue, aNotify);
}

NS_IMETHODIMP
nsGenericHTMLLeafFormElement::SetAttr(nsINodeInfo* aNodeInfo,
                                      const nsAReadableString& aValue,
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
    return SetFocus(presContext);
  }

  return RemoveFocus(presContext);
}

nsresult
nsGenericHTMLElement::HandleFrameOnloadEvent(nsIDOMEvent* aEvent)
{
  NS_ENSURE_TRUE(aEvent, NS_OK);

  nsAutoString type;
  aEvent->GetType(type);

  if (!type.EqualsIgnoreCase("load")) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresContext> ctx;

  GetPresContext(this, getter_AddRefs(ctx));
  NS_ENSURE_TRUE(ctx, NS_OK);

  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event;

  event.eventStructType = NS_EVENT;
  event.message = NS_PAGE_LOAD;

  return HandleDOMEvent(ctx, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
}

