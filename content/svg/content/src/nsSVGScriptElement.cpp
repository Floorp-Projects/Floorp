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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGScriptElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsCOMPtr.h"
#include "nsSVGAnimatedString.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIScriptElement.h"
#include "nsIScriptLoader.h"
#include "nsIScriptLoaderObserver.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsGUIEvent.h"
#include "nsIDOMText.h"

typedef nsSVGElement nsSVGScriptElementBase;

class nsSVGScriptElement : public nsSVGScriptElementBase,
                           public nsIDOMSVGScriptElement, 
                           public nsIDOMSVGURIReference,
                           public nsIScriptLoaderObserver,
                           public nsIScriptElement
{
protected:
  friend nsresult NS_NewSVGScriptElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGScriptElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGScriptElement();
  virtual nsresult Init();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSCRIPTELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE
  NS_DECL_NSISCRIPTLOADEROBSERVER

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGScriptElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGScriptElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGScriptElementBase::)

  // nsIScriptElement
  virtual void GetScriptType(nsAString& type);
  virtual already_AddRefed<nsIURI> GetScriptURI();
  virtual void GetScriptText(nsAString& text);
  virtual void GetScriptCharset(nsAString& charset); 
  virtual void SetScriptLineNumber(PRUint32 aLineNumber);
  virtual PRUint32 GetScriptLineNumber();

  // nsISVGValueObserver specializations:
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsIContent specializations:
  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify, PRBool aDeepSetDocument);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                 PRBool aDeepSetDocument);
    
protected:
  /**
   * Processes the script if it's in the document-tree and links to or
   * contains a script. Once it has been evaluated there is no way to make it
   * reevaluate the script, you'll have to create a new element. This also means
   * that when adding a href attribute to an element that already contains an
   * inline script, the script referenced by the src attribute will not be
   * loaded.
   *
   * In order to be able to use multiple childNodes, or to use the
   * fallback-mechanism of using both inline script and linked script you have
   * to add all attributes and childNodes before adding the element to the
   * document-tree.
   */
  void MaybeProcessScript();
  
  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;
  PRUint32 mLineNumber;
  PRPackedBool mIsEvaluated;
  PRPackedBool mEvaluating;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Script)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGScriptElement,nsSVGScriptElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGScriptElement,nsSVGScriptElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGScriptElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGScriptElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY(nsIScriptLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIScriptElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGScriptElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGScriptElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGScriptElement::nsSVGScriptElement(nsINodeInfo *aNodeInfo)
  : nsSVGScriptElementBase(aNodeInfo),
    mLineNumber(0),
    mIsEvaluated(PR_FALSE),
    mEvaluating(PR_FALSE)
{

}

nsSVGScriptElement::~nsSVGScriptElement()
{
}

  
nsresult
nsSVGScriptElement::Init()
{
  nsresult rv;

  // nsIDOMSVGURIReference properties

  // DOM property: href , #REQUIRED attrib: xlink:href
  // XXX: enforce requiredness
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_SVG_DOM_CLONENODE(Script)

//----------------------------------------------------------------------
// nsIDOMSVGScriptElement methods

/* attribute DOMString type; */
NS_IMETHODIMP
nsSVGScriptElement::GetType(nsAString & aType)
{
  return GetAttr(kNameSpaceID_None, nsSVGAtoms::type, aType);
}
NS_IMETHODIMP
nsSVGScriptElement::SetType(const nsAString & aType)
{
  NS_ERROR("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGScriptElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIScriptLoaderObserver methods

// variation of this code in nsHTMLScriptElement - check if changes
// need to be transfered when modifying

/* void scriptAvailable (in nsresult aResult, in nsIScriptElement aElement , in nsIURI aURI, in PRInt32 aLineNo, in PRUint32 aScriptLength, [size_is (aScriptLength)] in wstring aScript); */
NS_IMETHODIMP
nsSVGScriptElement::ScriptAvailable(nsresult aResult,
                                    nsIScriptElement *aElement,
                                    PRBool aIsInline,
                                    PRBool aWasPending,
                                    nsIURI *aURI,
                                    PRInt32 aLineNo,
                                    const nsAString& aScript)
{
  if (!aIsInline && NS_FAILED(aResult)) {
    nsCOMPtr<nsIPresContext> presContext;
    if (mDocument) {
      nsIPresShell *presShell = mDocument->GetShellAt(0);
      if (presShell)
        presShell->GetPresContext(getter_AddRefs(presContext));
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    nsScriptErrorEvent event(NS_SCRIPT_ERROR);

    event.lineNr = aLineNo;

    NS_NAMED_LITERAL_STRING(errorString, "Error loading script");
    event.errorMsg = errorString.get();

    nsCAutoString spec;
    aURI->GetSpec(spec);

    NS_ConvertUTF8toUCS2 fileName(spec);
    event.fileName = fileName.get();

    HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT,
                   &status);
  }

  return NS_OK;
}

// variation of this code in nsHTMLScriptElement - check if changes
// need to be transfered when modifying

/* void scriptEvaluated (in nsresult aResult, in nsIScriptElement aElement); */
NS_IMETHODIMP
nsSVGScriptElement::ScriptEvaluated(nsresult aResult,
                                    nsIScriptElement *aElement,
                                    PRBool aIsInline,
                                    PRBool aWasPending)
{
  nsresult rv = NS_OK;
  if (!aIsInline) {
    nsCOMPtr<nsIPresContext> presContext;
    if (mDocument) {
      nsIPresShell *presShell = mDocument->GetShellAt(0);
      if (presShell)
        presShell->GetPresContext(getter_AddRefs(presContext));
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(NS_SUCCEEDED(aResult) ? NS_SCRIPT_LOAD : NS_SCRIPT_ERROR);
    rv = HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT,
                        &status);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIScriptElement methods

void
nsSVGScriptElement::GetScriptType(nsAString& type)
{
  GetType(type);
}

// variation of this code in nsHTMLScriptElement - check if changes
// need to be transfered when modifying

already_AddRefed<nsIURI>
nsSVGScriptElement::GetScriptURI()
{
  nsIURI *uri = nsnull;
  nsAutoString src;
  mHref->GetAnimVal(src);
  if (!src.IsEmpty())
    NS_NewURI(&uri, src);
  return uri;
}

void
nsSVGScriptElement::GetScriptText(nsAString& text)
{
  GetContentsAsText(text);
}

void
nsSVGScriptElement::GetScriptCharset(nsAString& charset)
{
  charset.Truncate();
}

void 
nsSVGScriptElement::SetScriptLineNumber(PRUint32 aLineNumber)
{
  mLineNumber = aLineNumber;
}

PRUint32
nsSVGScriptElement::GetScriptLineNumber()
{
  return mLineNumber;
}


//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGScriptElement::DidModifySVGObservable(nsISVGValue* aObservable)
{
  nsresult rv = nsSVGScriptElementBase::DidModifySVGObservable(aObservable);
  
  // if aObservable==mHref:
  MaybeProcessScript();
  
  return rv;
}

//----------------------------------------------------------------------
// nsIContent methods

void
nsSVGScriptElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                PRBool aCompileEventHandlers)
{
  nsSVGScriptElementBase::SetDocument(aDocument, aDeep, aCompileEventHandlers);
  if (aDocument) {
    MaybeProcessScript();
  }
}

nsresult
nsSVGScriptElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                  PRBool aNotify, PRBool aDeepSetDocument)
{
  nsresult rv = nsSVGScriptElementBase::InsertChildAt(aKid, aIndex, aNotify,
                                                      aDeepSetDocument);
  if (NS_SUCCEEDED(rv) && aNotify) {
    MaybeProcessScript();
  }

  return rv;
}

nsresult
nsSVGScriptElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                  PRBool aDeepSetDocument)
{
  nsresult rv = nsSVGScriptElementBase::AppendChildTo(aKid, aNotify,
                                                      aDeepSetDocument);
  if (NS_SUCCEEDED(rv) && aNotify) {
    MaybeProcessScript();
  }

  return rv;
}

//----------------------------------------------------------------------
// implementation helpers

// variation of this code in nsHTMLScriptElement - check if changes
// need to be transfered when modifying
void
nsSVGScriptElement::MaybeProcessScript()
{
  if (mIsEvaluated || mEvaluating || !mDocument || !GetParent()) {
    return;
  }

  // We'll always call this to make sure that
  // ScriptAvailable/ScriptEvaluated gets called. See bug 153600
  nsresult rv = NS_OK;
  nsCOMPtr<nsIScriptLoader> loader = mDocument->GetScriptLoader();
  if (loader) {
    mEvaluating = PR_TRUE;
    rv = loader->ProcessScriptElement(this, this);
    mEvaluating = PR_FALSE;
  }

  // But we'll only set mIsEvaluated if we did really load or evaluate
  // something
  if (HasAttr(kNameSpaceID_XLink, nsSVGAtoms::href) ||
      mAttrsAndChildren.ChildCount()) {
    mIsEvaluated = PR_TRUE;
  }
}
