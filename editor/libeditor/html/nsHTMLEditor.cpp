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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Daniel Glazman <glazman@netscape.com>
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
#include "nsICaret.h"
#include "nsCRT.h"

#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"

#include "nsEditorEventListeners.h"
#include "nsHTMLEditorMouseListener.h"
#include "TypeInState.h"

#include "nsHTMLURIRefObject.h"

#include "nsIDOMText.h"
#include "nsITextContent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOM3EventTarget.h" 
#include "nsIDOMKeyEvent.h"
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsISelectionController.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsGUIEvent.h"
#include "nsIDOMEventGroup.h"

#include "TransactionFactory.h"

#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"

#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsParserCIID.h"
#include "nsIImage.h"
#include "nsAOLCiter.h"
#include "nsInternetCiter.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "SetDocTitleTxn.h"
#include "nsGUIEvent.h"
#include "nsTextFragment.h"

// netwerk
#include "nsIURI.h"
#include "nsNetUtil.h"

// Drag & Drop, Clipboard
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIDragService.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIContentFilter.h"

// Transactionas
#include "nsStyleSheetTxns.h"

// Misc
#include "TextEditorTest.h"
#include "nsEditorUtils.h"
#include "nsITextContent.h"
#include "nsWSRunObject.h"
#include "nsHTMLObjectResizer.h"

#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIWidget.h"

static NS_DEFINE_CID(kCTransitionalDTDCID,  NS_CTRANSITIONAL_DTD_CID);

// Some utilities to handle annoying overloading of "A" tag for link and named anchor
static char hrefText[] = "href";
static char anchorTxt[] = "anchor";
static char namedanchorText[] = "namedanchor";

nsCOMPtr<nsIParserService> nsHTMLEditor::sParserService;
PRInt32 nsHTMLEditor::sInstanceCount = 0;

// some prototypes for rules creation shortcuts
nsresult NS_NewTextEditRules(nsIEditRules** aInstancePtrResult);
nsresult NS_NewHTMLEditRules(nsIEditRules** aInstancePtrResult);

#define IsLinkTag(s) (s.EqualsIgnoreCase(hrefText))
#define IsNamedAnchorTag(s) (s.EqualsIgnoreCase(anchorTxt) || s.EqualsIgnoreCase(namedanchorText))

nsHTMLEditor::nsHTMLEditor()
: nsPlaintextEditor()
, mIgnoreSpuriousDragEvent(PR_FALSE)
, mTypeInState(nsnull)
, mSelectedCellIndex(0)
, mHTMLCSSUtils(nsnull)
, mIsObjectResizingEnabled(PR_TRUE)
, mIsResizing(PR_FALSE)
, mIsAbsolutelyPositioningEnabled(PR_TRUE)
, mResizedObjectIsAbsolutelyPositioned(PR_FALSE)
, mGrabberClicked(PR_FALSE)
, mIsMoving(PR_FALSE)
, mSnapToGridEnabled(PR_FALSE)
, mIsInlineTableEditingEnabled(PR_TRUE)
, mGridSize(0)
{
  mBoldAtom = do_GetAtom("b");
  mItalicAtom = do_GetAtom("i");
  mUnderlineAtom = do_GetAtom("u");
  mFontAtom = do_GetAtom("font");
  mLinkAtom = do_GetAtom("a");
  ++sInstanceCount;
} 

nsHTMLEditor::~nsHTMLEditor()
{
  // remove the rules as an action listener.  Else we get a bad ownership loop later on.
  // it's ok if the rules aren't a listener; we ignore the error.
  nsCOMPtr<nsIEditActionListener> mListener = do_QueryInterface(mRules);
  RemoveEditActionListener(mListener);
  
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  nsCOMPtr<nsISelection>selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
  // if we don't get the selection, just skip this
  if (NS_SUCCEEDED(result) && selection) 
  {
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
    nsCOMPtr<nsISelectionListener>listener;
    listener = do_QueryInterface(mTypeInState);
    if (listener) {
      selPriv->RemoveSelectionListener(listener); 
    }
    listener = do_QueryInterface(mSelectionListenerP);
    if (listener) {
      selPriv->RemoveSelectionListener(listener); 
    }
  }

  NS_IF_RELEASE(mTypeInState);
  mSelectionListenerP = nsnull;

  if (--sInstanceCount == 0 && sParserService)
    sParserService = 0;

  if (mHTMLCSSUtils)
    delete mHTMLCSSUtils;
  
  // free any default style propItems
  RemoveAllDefaultProperties();
}

NS_IMPL_ADDREF_INHERITED(nsHTMLEditor, nsEditor)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditor, nsEditor)

NS_INTERFACE_MAP_BEGIN(nsHTMLEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLObjectResizer)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLAbsPosEditor)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLInlineTableEditor)
  NS_INTERFACE_MAP_ENTRY(nsITableEditor)
  NS_INTERFACE_MAP_ENTRY(nsIEditorStyleSheets)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
NS_INTERFACE_MAP_END_INHERITING(nsPlaintextEditor)


NS_IMETHODIMP nsHTMLEditor::Init(nsIDOMDocument *aDoc, 
                                 nsIPresShell   *aPresShell, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags)
{
  NS_PRECONDITION(aDoc && aPresShell, "bad arg");
  if (!aDoc || !aPresShell)
    return NS_ERROR_NULL_POINTER;

  nsresult result = NS_OK, rulesRes = NS_OK;

  // make a range util object for comparing dom points
  mRangeHelper = do_CreateInstance("@mozilla.org/content/range-utils;1");
  if (!mRangeHelper) return NS_ERROR_NULL_POINTER;
   
  if (1)
  {
    // block to scope nsAutoEditInitRulesTrigger
    nsAutoEditInitRulesTrigger rulesTrigger(NS_STATIC_CAST(nsPlaintextEditor*,this), rulesRes);

    // Set up a DTD   
    mDTD = do_CreateInstance(kCTransitionalDTDCID);
    if (!mDTD) result = NS_ERROR_FAILURE;

    // Init the plaintext editor
    result = nsPlaintextEditor::Init(aDoc, aPresShell, aRoot, aSelCon, aFlags);
    if (NS_FAILED(result)) { return result; }

    // the HTML Editor is CSS-aware only in the case of Composer
    mCSSAware = (0 == aFlags);

    // disable Composer-only features
    if (aFlags & eEditorMailMask)
    {
      SetAbsolutePositioningEnabled(PR_FALSE);
      SetSnapToGridEnabled(PR_FALSE);
    }

    // Init the HTML-CSS utils
    if (mHTMLCSSUtils)
      delete mHTMLCSSUtils;
    result = NS_NewHTMLCSSUtils(&mHTMLCSSUtils);
    if (NS_FAILED(result)) { return result; }
    mHTMLCSSUtils->Init(this);

    // disable links
    nsCOMPtr<nsIPresContext> context;
    aPresShell->GetPresContext(getter_AddRefs(context));
    if (!context) return NS_ERROR_NULL_POINTER;
    if (!(mFlags & eEditorPlaintextMask))
      context->SetLinkHandler(0);  

    nsCOMPtr<nsIDOMElement> bodyElement;
    result = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
    if (NS_FAILED(result)) { return result; }
    if (!bodyElement) { return NS_ERROR_NULL_POINTER; }

    // init the type-in state
    mTypeInState = new TypeInState();
    if (!mTypeInState) {return NS_ERROR_NULL_POINTER;}
    NS_ADDREF(mTypeInState);

    // init the selection listener for image resizing
    mSelectionListenerP = new ResizerSelectionListener(this);
    if (!mSelectionListenerP) {return NS_ERROR_NULL_POINTER;}

    // ignore any errors from this in case the file is missing
    AddOverrideStyleSheet(NS_LITERAL_STRING("resource:/res/EditorOverride.css"));

    nsCOMPtr<nsISelection>selection;
    result = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) { return result; }
    if (selection) 
    {
      nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
      nsCOMPtr<nsISelectionListener>listener;
      listener = do_QueryInterface(mTypeInState);
      if (listener) {
        selPriv->AddSelectionListener(listener); 
      }
      listener = do_QueryInterface(mSelectionListenerP);
      if (listener) {
        selPriv->AddSelectionListener(listener); 
      }
    }
  }

  if (NS_FAILED(rulesRes)) return rulesRes;
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::PostCreate()
{
  nsresult result = InstallEventListeners();
  if (NS_FAILED(result)) return result;

  result = nsEditor::PostCreate();
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::InstallEventListeners()
{
  NS_ASSERTION(mDocWeak, "no document set on this editor");
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;

  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;

  nsresult result;
  // get a key listener
  result = NS_NewEditorKeyListener(getter_AddRefs(mKeyListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }
  
  // get a mouse listener
  result = NS_NewHTMLEditorMouseListener(getter_AddRefs(mMouseListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // get a text listener
  result = NS_NewEditorTextListener(getter_AddRefs(mTextListenerP),this);
  if (NS_FAILED(result)) { 
#ifdef DEBUG_TAGUE
printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
    HandleEventListenerError();
    return result;
  }

  // get a composition listener
  result = NS_NewEditorCompositionListener(getter_AddRefs(mCompositionListenerP),this);
  if (NS_FAILED(result)) { 
#ifdef DEBUG_TAGUE
printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
    HandleEventListenerError();
    return result;
  }

  // get a drag listener
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
  result = NS_NewEditorDragListener(getter_AddRefs(mDragListenerP), presShell, this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // get a focus listener
  result = NS_NewEditorFocusListener(getter_AddRefs(mFocusListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  nsCOMPtr<nsIDOMEventReceiver> erP;
  result = GetDOMEventReceiver(getter_AddRefs(erP));

  //end hack
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // register the event listeners with the DOM event reveiver
  nsCOMPtr<nsIDOM3EventTarget> dom3Targ(do_QueryInterface(erP));
  nsCOMPtr<nsIDOMEventGroup> sysGroup;
  if (NS_SUCCEEDED(erP->GetSystemEventGroup(getter_AddRefs(sysGroup)))) {
    result = dom3Targ->AddGroupedEventListener(NS_LITERAL_STRING("keypress"), mKeyListenerP, PR_FALSE, sysGroup);
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register key listener in system group");
  }

  if (NS_SUCCEEDED(result))
  {
    result = erP->AddEventListenerByIID(mMouseListenerP, NS_GET_IID(nsIDOMMouseListener));
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register mouse listener");
    if (NS_SUCCEEDED(result))
    {
      result = erP->AddEventListenerByIID(mFocusListenerP, NS_GET_IID(nsIDOMFocusListener));
      NS_ASSERTION(NS_SUCCEEDED(result), "failed to register focus listener");
      if (NS_SUCCEEDED(result))
      {
        result = erP->AddEventListenerByIID(mTextListenerP, NS_GET_IID(nsIDOMTextListener));
        NS_ASSERTION(NS_SUCCEEDED(result), "failed to register text listener");
        if (NS_SUCCEEDED(result))
        {
          result = erP->AddEventListenerByIID(mCompositionListenerP, NS_GET_IID(nsIDOMCompositionListener));
          NS_ASSERTION(NS_SUCCEEDED(result), "failed to register composition listener");
          if (NS_SUCCEEDED(result))
          {
            result = erP->AddEventListenerByIID(mDragListenerP, NS_GET_IID(nsIDOMDragListener));
            NS_ASSERTION(NS_SUCCEEDED(result), "failed to register drag listener");
          }
        }
      }
    }
  }
  if (NS_FAILED(result)) {
    HandleEventListenerError();
  }
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFlags(PRUint32 *aFlags)
{
  if (!mRules || !aFlags) { return NS_ERROR_NULL_POINTER; }
  return mRules->GetFlags(aFlags);
}


NS_IMETHODIMP 
nsHTMLEditor::SetFlags(PRUint32 aFlags)
{
  if (!mRules) { return NS_ERROR_NULL_POINTER; }
  mCSSAware = ((aFlags & (eEditorNoCSSMask | eEditorMailMask)) == 0);

  return mRules->SetFlags(aFlags);
}

NS_IMETHODIMP nsHTMLEditor::InitRules()
{
  // instantiate the rules for the html editor
  nsresult res = NS_NewHTMLEditRules(getter_AddRefs(mRules));
  if (NS_FAILED(res)) return res;
  if (!mRules) return NS_ERROR_UNEXPECTED;
  res = mRules->Init(NS_STATIC_CAST(nsPlaintextEditor*,this), mFlags);
  
  return res;
}

NS_IMETHODIMP nsHTMLEditor::BeginningOfDocument()
{
  if (!mDocWeak || !mPresShellWeak) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res))
    return res;
  if (!selection)
    return NS_ERROR_NOT_INITIALIZED;
    
  // get the root element 
  nsCOMPtr<nsIDOMElement> rootElement; 
  res = GetRootElement(getter_AddRefs(rootElement)); 
  if (NS_FAILED(res)) return res; 
  if (!rootElement)   return NS_ERROR_NULL_POINTER; 
  
  // find first editable thingy
  PRBool done = PR_FALSE;
  nsCOMPtr<nsIDOMNode> curNode(rootElement), selNode;
  PRInt32 curOffset = 0, selOffset;
  while (!done)
  {
    nsWSRunObject wsObj(this, curNode, curOffset);
    nsCOMPtr<nsIDOMNode> visNode;
    PRInt32 visOffset=0;
    PRInt16 visType=0;
    wsObj.NextVisibleNode(curNode, curOffset, address_of(visNode), &visOffset, &visType);
    if ((visType==nsWSRunObject::eNormalWS) || 
        (visType==nsWSRunObject::eText))
    {
      selNode = visNode;
      selOffset = visOffset;
      done = PR_TRUE;
    }
    else if ((visType==nsWSRunObject::eBreak)    ||
             (visType==nsWSRunObject::eSpecial))
    {
      res = GetNodeLocation(visNode, address_of(selNode), &selOffset);
      if (NS_FAILED(res)) return res; 
      done = PR_TRUE;
    }
    else if (visType==nsWSRunObject::eOtherBlock)
    {
      // By definition of nsWSRunObject, a block element terminates 
      // a whitespace run. That is, although we are calling a method 
      // that is named "NextVisibleNode", the node returned
      // might not be visible/editable!
      // If the given block does not contain any visible/editable items,
      // we want to skip it and continue our search.

      if (!IsContainer(visNode))
      {
        // However, we were given a block that is not a container.
        // Since the block can not contain anything that's visible,
        // such a block only makes sense if it is visible by itself,
        // like a <hr>
        // We want to place the caret in front of that block.

        res = GetNodeLocation(visNode, address_of(selNode), &selOffset);
        if (NS_FAILED(res)) return res; 
        done = PR_TRUE;
      }
      else
      {
        PRBool isEmptyBlock;
        if (NS_SUCCEEDED(IsEmptyNode(visNode, &isEmptyBlock)) &&
            isEmptyBlock)
        {
          // skip the empty block
          res = GetNodeLocation(visNode, address_of(curNode), &curOffset);
          if (NS_FAILED(res)) return res; 
          ++curOffset;
        }
        else
        {
          curNode = visNode;
          curOffset = 0;
        }
        // keep looping
      }
    }
    else
    {
      // else we found nothing useful
      selNode = curNode;
      selOffset = curOffset;
      done = PR_TRUE;
    }
  }
  return selection->Collapse(selNode, selOffset);
}

/**
 * Returns true if the id represents an element of block type.
 * Can be used to determine if a new paragraph should be started.
 */
nsresult
nsHTMLEditor::NodeIsBlockStatic(nsIDOMNode *aNode, PRBool *aIsBlock)
{
  if (!aNode || !aIsBlock) { return NS_ERROR_NULL_POINTER; }

  *aIsBlock = PR_FALSE;

#define USE_PARSER_FOR_BLOCKNESS 1
#ifdef USE_PARSER_FOR_BLOCKNESS
  nsresult rv;

  nsCOMPtr<nsIDOMElement>element = do_QueryInterface(aNode);
  if (!element)
  {
    // We don't have an element -- probably a text node
    return NS_OK;
  }

  nsIAtom *tagAtom = GetTag(aNode);
  if (!tagAtom) return NS_ERROR_NULL_POINTER;

  if (!sParserService) {
    sParserService = do_GetService("@mozilla.org/parser/parser-service;1", &rv);
    if (NS_FAILED(rv)) return rv;
  }

  // Nodes we know we want to treat as block
  // even though the parser says they're not:
  if (tagAtom==nsEditProperty::body       ||
      tagAtom==nsEditProperty::head       ||
      tagAtom==nsEditProperty::tbody      ||
      tagAtom==nsEditProperty::thead      ||
      tagAtom==nsEditProperty::tfoot      ||
      tagAtom==nsEditProperty::tr         ||
      tagAtom==nsEditProperty::th         ||
      tagAtom==nsEditProperty::td         ||
      tagAtom==nsEditProperty::li         ||
      tagAtom==nsEditProperty::dt         ||
      tagAtom==nsEditProperty::dd         ||
      tagAtom==nsEditProperty::pre)
  {
    *aIsBlock = PR_TRUE;
    return NS_OK;
  }

  PRInt32 id;
  rv = sParserService->HTMLAtomTagToId(tagAtom, &id);
  if (NS_FAILED(rv)) return rv;
  rv = sParserService->IsBlock(id, *aIsBlock);

#ifdef DEBUG
  // Check this against what we would have said with the old code:
  if (tagAtom==nsEditProperty::p          ||
      tagAtom==nsEditProperty::div        ||
      tagAtom==nsEditProperty::blockquote ||
      tagAtom==nsEditProperty::h1         ||
      tagAtom==nsEditProperty::h2         ||
      tagAtom==nsEditProperty::h3         ||
      tagAtom==nsEditProperty::h4         ||
      tagAtom==nsEditProperty::h5         ||
      tagAtom==nsEditProperty::h6         ||
      tagAtom==nsEditProperty::ul         ||
      tagAtom==nsEditProperty::ol         ||
      tagAtom==nsEditProperty::dl         ||
      tagAtom==nsEditProperty::noscript   ||
      tagAtom==nsEditProperty::form       ||
      tagAtom==nsEditProperty::hr         ||
      tagAtom==nsEditProperty::table      ||
      tagAtom==nsEditProperty::fieldset   ||
      tagAtom==nsEditProperty::address    ||
      tagAtom==nsEditProperty::caption    ||
      tagAtom==nsEditProperty::col        ||
      tagAtom==nsEditProperty::colgroup   ||
      tagAtom==nsEditProperty::li         ||
      tagAtom==nsEditProperty::dt         ||
      tagAtom==nsEditProperty::dd         ||
      tagAtom==nsEditProperty::legend     )
  {
    if (!(*aIsBlock))
    {
      nsAutoString assertmsg (NS_LITERAL_STRING("Parser and editor disagree on blockness: "));

      nsAutoString tagName;
      rv = element->GetTagName(tagName);
      if (NS_FAILED(rv)) return rv;

      assertmsg.Append(tagName);
      char* assertstr = ToNewCString(assertmsg);
      NS_ASSERTION(*aIsBlock, assertstr);
      Recycle(assertstr);
    }
  }
#endif /* DEBUG */

  return rv;
#else /* USE_PARSER_FOR_BLOCKNESS */
  nsresult result = NS_ERROR_FAILURE;
  *aIsBlock = PR_FALSE;
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tagName;
    result = element->GetTagName(tagName);
    if (NS_SUCCEEDED(result))
    {
      ToLowerCase(tagName);
      nsCOMPtr<nsIAtom> tagAtom = do_GetAtom(tagName);
      if (!tagAtom) { return NS_ERROR_NULL_POINTER; }

      if (tagAtom==nsEditProperty::p          ||
          tagAtom==nsEditProperty::div        ||
          tagAtom==nsEditProperty::blockquote ||
          tagAtom==nsEditProperty::h1         ||
          tagAtom==nsEditProperty::h2         ||
          tagAtom==nsEditProperty::h3         ||
          tagAtom==nsEditProperty::h4         ||
          tagAtom==nsEditProperty::h5         ||
          tagAtom==nsEditProperty::h6         ||
          tagAtom==nsEditProperty::ul         ||
          tagAtom==nsEditProperty::ol         ||
          tagAtom==nsEditProperty::dl         ||
          tagAtom==nsEditProperty::pre        ||
          tagAtom==nsEditProperty::noscript   ||
          tagAtom==nsEditProperty::form       ||
          tagAtom==nsEditProperty::hr         ||
          tagAtom==nsEditProperty::fieldset   ||
          tagAtom==nsEditProperty::address    ||
          tagAtom==nsEditProperty::body       ||
          tagAtom==nsEditProperty::caption    ||
          tagAtom==nsEditProperty::table      ||
          tagAtom==nsEditProperty::tbody      ||
          tagAtom==nsEditProperty::thead      ||
          tagAtom==nsEditProperty::tfoot      ||
          tagAtom==nsEditProperty::tr         ||
          tagAtom==nsEditProperty::td         ||
          tagAtom==nsEditProperty::th         ||
          tagAtom==nsEditProperty::col        ||
          tagAtom==nsEditProperty::colgroup   ||
          tagAtom==nsEditProperty::li         ||
          tagAtom==nsEditProperty::dt         ||
          tagAtom==nsEditProperty::dd         ||
          tagAtom==nsEditProperty::legend     )
      {
        *aIsBlock = PR_TRUE;
      }
      result = NS_OK;
    }
  } else {
    // We don't have an element -- probably a text node
    nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(aNode);
    if (nodeAsText)
    {
      *aIsBlock = PR_FALSE;
      result = NS_OK;
    }
  }
  return result;

#endif /* USE_PARSER_FOR_BLOCKNESS */
}

NS_IMETHODIMP
nsHTMLEditor::NodeIsBlock(nsIDOMNode *aNode, PRBool *aIsBlock)
{
  return NodeIsBlockStatic(aNode, aIsBlock);
}

PRBool
nsHTMLEditor::IsBlockNode(nsIDOMNode *aNode)
{
  PRBool isBlock;
  NodeIsBlockStatic(aNode, &isBlock);
  return isBlock;
}

// Non-static version for the nsIEditor interface and JavaScript
NS_IMETHODIMP 
nsHTMLEditor::SetDocumentTitle(const nsAString &aTitle)
{
  SetDocTitleTxn *txn;
  nsresult result = TransactionFactory::GetNewTransaction(SetDocTitleTxn::GetCID(), (EditTxn **)&txn);
  if (NS_SUCCEEDED(result))  
  {
    result = txn->Init(this, &aTitle);

    if (NS_SUCCEEDED(result)) 
    {
      //Don't let Rules System change the selection
      nsAutoTxnsConserveSelection dontChangeSelection(this);

      result = nsEditor::DoTransaction(txn);  
    }
    // The transaction system (if any) has taken ownwership of txn
    NS_IF_RELEASE(txn);
  }
  return result;
}

/* ------------ Block methods moved from nsEditor -------------- */
///////////////////////////////////////////////////////////////////////////
// GetBlockNodeParent: returns enclosing block level ancestor, if any
//
nsCOMPtr<nsIDOMNode>
nsHTMLEditor::GetBlockNodeParent(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> tmp;
  nsCOMPtr<nsIDOMNode> p;

  if (!aNode)
  {
    NS_NOTREACHED("null node passed to GetBlockNodeParent()");
    return PR_FALSE;
  }

  if (NS_FAILED(aNode->GetParentNode(getter_AddRefs(p))))  // no parent, ran off top of tree
    return tmp;

  while (p)
  {
    PRBool isBlock;
    if (NS_FAILED(NodeIsBlockStatic(p, &isBlock)) || isBlock)
      break;
    if ( NS_FAILED(p->GetParentNode(getter_AddRefs(tmp))) || !tmp) // no parent, ran off top of tree
      return p;

    p = tmp;
  }
  return p;
}


///////////////////////////////////////////////////////////////////////////
// HasSameBlockNodeParent: true if nodes have same block level ancestor
//               
PRBool
nsHTMLEditor::HasSameBlockNodeParent(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2)
  {
    NS_NOTREACHED("null node passed to HasSameBlockNodeParent()");
    return PR_FALSE;
  }
  
  if (aNode1 == aNode2)
    return PR_TRUE;
    
  nsCOMPtr<nsIDOMNode> p1 = GetBlockNodeParent(aNode1);
  nsCOMPtr<nsIDOMNode> p2 = GetBlockNodeParent(aNode2);

  return (p1 == p2);
}


///////////////////////////////////////////////////////////////////////////
// GetBlockSection: return leftmost/rightmost nodes in aChild's block
//               
nsresult
nsHTMLEditor::GetBlockSection(nsIDOMNode *aChild,
                              nsIDOMNode **aLeftNode, 
                              nsIDOMNode **aRightNode) 
{
  nsresult result = NS_OK;
  if (!aChild || !aLeftNode || !aRightNode) {return NS_ERROR_NULL_POINTER;}
  *aLeftNode = aChild;
  *aRightNode = aChild;

  nsCOMPtr<nsIDOMNode>sibling;
  result = aChild->GetPreviousSibling(getter_AddRefs(sibling));
  while ((NS_SUCCEEDED(result)) && sibling)
  {
    PRBool isBlock;
    NodeIsBlockStatic(sibling, &isBlock);
    if (isBlock)
    {
      nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(sibling);
      if (!nodeAsText) {
        break;
      }
      // XXX: needs some logic to work for other leaf nodes besides text!
    }
    *aLeftNode = sibling;
    result = (*aLeftNode)->GetPreviousSibling(getter_AddRefs(sibling)); 
  }
  NS_ADDREF((*aLeftNode));
  // now do the right side
  result = aChild->GetNextSibling(getter_AddRefs(sibling));
  while ((NS_SUCCEEDED(result)) && sibling)
  {
    PRBool isBlock;
    NodeIsBlockStatic(sibling, &isBlock);
    if (isBlock) 
    {
      nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(sibling);
      if (!nodeAsText) {
        break;
      }
    }
    *aRightNode = sibling;
    result = (*aRightNode)->GetNextSibling(getter_AddRefs(sibling)); 
  }
  NS_ADDREF((*aRightNode));

  return result;
}


///////////////////////////////////////////////////////////////////////////
// GetBlockSectionsForRange: return list of block sections that intersect 
//                           this range
nsresult
nsHTMLEditor::GetBlockSectionsForRange(nsIDOMRange *aRange,
                                       nsCOMArray<nsIDOMRange>& aSections) 
{
  if (!aRange) {return NS_ERROR_NULL_POINTER;}

  nsresult result;
  nsCOMPtr<nsIContentIterator>iter =
    do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &result);
  if ((NS_SUCCEEDED(result)) && iter)
  {
    nsCOMPtr<nsIDOMRange> lastRange;
    iter->Init(aRange);
    while (iter->IsDone())
    {
      nsCOMPtr<nsIContent> currentContent = iter->GetCurrentNode();

      nsCOMPtr<nsIDOMNode>currentNode = do_QueryInterface(currentContent);
      if (currentNode)
      {
        // <BR> divides block content ranges.  We can achieve this by nulling out lastRange
        if (currentContent->Tag() == nsEditProperty::br)
        {
          lastRange = nsnull;
        }
        else
        {
          PRBool isNotInlineOrText;
          result = NodeIsBlockStatic(currentNode, &isNotInlineOrText);
          if (isNotInlineOrText)
          {
            PRUint16 nodeType;
            currentNode->GetNodeType(&nodeType);
            if (nsIDOMNode::TEXT_NODE == nodeType) {
              isNotInlineOrText = PR_TRUE;
            }
          }
          if (PR_FALSE==isNotInlineOrText)
          {
            nsCOMPtr<nsIDOMNode>leftNode;
            nsCOMPtr<nsIDOMNode>rightNode;
            result = GetBlockSection(currentNode,
                                     getter_AddRefs(leftNode),
                                     getter_AddRefs(rightNode));
            if ((NS_SUCCEEDED(result)) && leftNode && rightNode)
            {
              // add range to the list if it doesn't overlap with the previous range
              PRBool addRange=PR_TRUE;
              if (lastRange)
              {
                nsCOMPtr<nsIDOMNode> lastStartNode;
                nsCOMPtr<nsIDOMElement> blockParentOfLastStartNode;
                lastRange->GetStartContainer(getter_AddRefs(lastStartNode));
                blockParentOfLastStartNode = do_QueryInterface(GetBlockNodeParent(lastStartNode));
                if (blockParentOfLastStartNode)
                {
                  nsCOMPtr<nsIDOMElement> blockParentOfLeftNode;
                  blockParentOfLeftNode = do_QueryInterface(GetBlockNodeParent(leftNode));
                  if (blockParentOfLeftNode)
                  {
                    if (blockParentOfLastStartNode==blockParentOfLeftNode) {
                      addRange = PR_FALSE;
                    }
                  }
                }
              }
              if (PR_TRUE==addRange) 
              {
                nsCOMPtr<nsIDOMRange> range =
                     do_CreateInstance("@mozilla.org/content/range;1", &result);
                if ((NS_SUCCEEDED(result)) && range)
                { // initialize the range
                  range->SetStart(leftNode, 0);
                  range->SetEnd(rightNode, 0);
                  aSections.AppendObject(range);
                  lastRange = do_QueryInterface(range);
                }
              }        
            }
          }
        }
      }
      /* do not check result here, and especially do not return the result code.
       * we rely on iter->IsDone to tell us when the iteration is complete
       */
      iter->Next();
    }
  }
  return result;
}


///////////////////////////////////////////////////////////////////////////
// NextNodeInBlock: gets the next/prev node in the block, if any.  Next node
//                  must be an element or text node, others are ignored
nsCOMPtr<nsIDOMNode>
nsHTMLEditor::NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir)
{
  nsCOMPtr<nsIDOMNode> nullNode;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> blockContent;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (!aNode)  return nullNode;

  nsresult rv;
  nsCOMPtr<nsIContentIterator> iter =
       do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &rv);
  if (NS_FAILED(rv))
    return nullNode;

  // much gnashing of teeth as we twit back and forth between content and domnode types
  content = do_QueryInterface(aNode);
  PRBool isBlock;
  if (NS_SUCCEEDED(NodeIsBlockStatic(aNode, &isBlock)) && isBlock)
  {
    blockParent = aNode;
  }
  else
  {
    blockParent = GetBlockNodeParent(aNode);
  }
  if (!blockParent) return nullNode;
  blockContent = do_QueryInterface(blockParent);
  if (!blockContent) return nullNode;
  
  if (NS_FAILED(iter->Init(blockContent)))  return nullNode;
  if (NS_FAILED(iter->PositionAt(content)))  return nullNode;
  
  while (!iter->IsDone())
  {
    // ignore nodes that aren't elements or text, or that are the
    // block parent
    node = do_QueryInterface(iter->GetCurrentNode());
    if (node && IsTextOrElementNode(node) && node != blockParent &&
        node != aNode)
      return node;

    if (aDir == kIterForward)
      iter->Next();
    else
      iter->Prev();
  }
  
  return nullNode;
}

static const PRUnichar nbsp = 160;

///////////////////////////////////////////////////////////////////////////
// IsNextCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace or nbsp
nsresult 
nsHTMLEditor::IsNextCharWhitespace(nsIDOMNode *aParentNode, 
                                   PRInt32 aOffset,
                                   PRBool *outIsSpace,
                                   PRBool *outIsNBSP,
                                   nsCOMPtr<nsIDOMNode> *outNode,
                                   PRInt32 *outOffset)
{
  if (!outIsSpace || !outIsNBSP) return NS_ERROR_NULL_POINTER;
  *outIsSpace = PR_FALSE;
  *outIsNBSP = PR_FALSE;
  if (outNode) *outNode = nsnull;
  if (outOffset) *outOffset = -1;
  
  nsAutoString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    textNode->GetLength(&strLength);
    if ((PRUint32)aOffset < strLength)
    {
      // easy case: next char is in same node
      textNode->SubstringData(aOffset,aOffset+1,tempString);
      *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
      *outIsNBSP = (tempString.First() == nbsp);
      if (outNode) *outNode = do_QueryInterface(aParentNode);
      if (outOffset) *outOffset = aOffset+1;  // yes, this is _past_ the character; 
      return NS_OK;
    }
  }
  
  // harder case: next char in next node.
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterForward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    PRBool isBlock (PR_FALSE);
    NodeIsBlock(node, &isBlock);
    if (isBlock)  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node) && IsEditable(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          textNode->SubstringData(0,1,tempString);
          *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
          *outIsNBSP = (tempString.First() == nbsp);
          if (outNode) *outNode = do_QueryInterface(node);
          if (outOffset) *outOffset = 1;  // yes, this is _past_ the character; 
          return NS_OK;
        }
        // else it's an empty text node, or not editable; skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    tmp = node;
    node = NextNodeInBlock(tmp, kIterForward);
  }
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPrevCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace
nsresult 
nsHTMLEditor::IsPrevCharWhitespace(nsIDOMNode *aParentNode, 
                                   PRInt32 aOffset,
                                   PRBool *outIsSpace,
                                   PRBool *outIsNBSP,
                                   nsCOMPtr<nsIDOMNode> *outNode,
                                   PRInt32 *outOffset)
{
  if (!outIsSpace || !outIsNBSP) return NS_ERROR_NULL_POINTER;
  *outIsSpace = PR_FALSE;
  *outIsNBSP = PR_FALSE;
  if (outNode) *outNode = nsnull;
  if (outOffset) *outOffset = -1;
  
  nsAutoString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    if (aOffset > 0)
    {
      // easy case: prev char is in same node
      textNode->SubstringData(aOffset-1,aOffset,tempString);
      *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
      *outIsNBSP = (tempString.First() == nbsp);
      if (outNode) *outNode = do_QueryInterface(aParentNode);
      if (outOffset) *outOffset = aOffset-1;  
      return NS_OK;
    }
  }
  
  // harder case: prev char in next node
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterBackward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    PRBool isBlock (PR_FALSE);
    NodeIsBlock(node, &isBlock);
    if (isBlock)  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node) && IsEditable(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          // you could use nsITextContent::IsOnlyWhitespace here
          textNode->SubstringData(strLength-1,strLength,tempString);
          *outIsSpace = nsCRT::IsAsciiSpace(tempString.First());
          *outIsNBSP = (tempString.First() == nbsp);
          if (outNode) *outNode = do_QueryInterface(aParentNode);
          if (outOffset) *outOffset = strLength-1;  
          return NS_OK;
        }
        // else it's an empty text node, or not editable; skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    // otherwise we found a node we want to skip, keep going
    tmp = node;
    node = NextNodeInBlock(tmp, kIterBackward);
  }
  
  return NS_OK;
  
}



/* ------------ End Block methods -------------- */


PRBool nsHTMLEditor::IsVisBreak(nsIDOMNode *aNode)
{
  if (!aNode) 
    return PR_FALSE;
  if (!nsTextEditUtils::IsBreak(aNode)) 
    return PR_FALSE;
  // check if there is a later node in block after br
  nsCOMPtr<nsIDOMNode> priorNode, nextNode;
  GetPriorHTMLNode(aNode, address_of(priorNode), PR_TRUE); 
  GetNextHTMLNode(aNode, address_of(nextNode), PR_TRUE); 
  // if we are next to another break, we are visible
  if (priorNode && nsTextEditUtils::IsBreak(priorNode))
    return PR_TRUE;
  if (nextNode && nsTextEditUtils::IsBreak(nextNode))
    return PR_TRUE;
  
  // if we are right before block boundary, then br not visible
  if (!nextNode) 
    return PR_FALSE;  // this break is trailer in block, it's not visible
  if (IsBlockNode(nextNode))
    return PR_FALSE; // break is right before a block, it's not visible
    
  // sigh.  We have to use expensive whitespace calculation code to 
  // determine what is going on
  nsCOMPtr<nsIDOMNode> selNode, tmp;
  PRInt32 selOffset;
  GetNodeLocation(aNode, address_of(selNode), &selOffset);
  selOffset++; // lets look after the break
  nsWSRunObject wsObj(this, selNode, selOffset);
  nsCOMPtr<nsIDOMNode> visNode;
  PRInt32 visOffset=0;
  PRInt16 visType=0;
  wsObj.NextVisibleNode(selNode, selOffset, address_of(visNode), &visOffset, &visType);
  if (visType & nsWSRunObject::eBlock)
    return PR_FALSE;
  
  return PR_TRUE;
}


NS_IMETHODIMP
nsHTMLEditor::GetIsDocumentEditable(PRBool *aIsDocumentEditable)
{
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);

  nsCOMPtr<nsIDOMDocument> doc;
  GetDocument(getter_AddRefs(doc));
  *aIsDocumentEditable = doc ? IsModifiable() : PR_FALSE;

  return NS_OK;
}

PRBool nsHTMLEditor::IsModifiable()
{
  PRUint32 flags;
  if (NS_SUCCEEDED(GetFlags(&flags)))
    return ((flags & nsIPlaintextEditor::eEditorReadonlyMask) == 0);
  else
    return PR_FALSE;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIHTMLEditor methods 
#pragma mark -
#endif

NS_IMETHODIMP
nsHTMLEditor::UpdateBaseURL()
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  GetDocument(getter_AddRefs(domDoc));
  if (!domDoc) return NS_ERROR_FAILURE;

  // Look for an HTML <base> tag
  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsresult rv = domDoc->GetElementsByTagName(NS_LITERAL_STRING("base"), getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> baseNode;
  if (nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    if (count >= 1)
    {
      rv = nodeList->Item(0, getter_AddRefs(baseNode));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  // If no base tag, then set baseURL to the document's URL
  // This is very important, else relative URLs for links and images are wrong
  if (!baseNode)
  {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    if (!doc) return NS_ERROR_FAILURE;

    return doc->SetBaseURI(doc->GetDocumentURI());
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::HandleKeyPress(nsIDOMKeyEvent* aKeyEvent)
{
  PRUint32 keyCode, character;
  PRBool   isShift, ctrlKey, altKey, metaKey;
  nsresult res;

  if (!aKeyEvent) return NS_ERROR_NULL_POINTER;

  if (NS_SUCCEEDED(aKeyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(aKeyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(aKeyEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(aKeyEvent->GetAltKey(&altKey)) &&
      NS_SUCCEEDED(aKeyEvent->GetMetaKey(&metaKey)))
  {
    // this royally blows: because tabs come in from keyDowns instead
    // of keyPress, and because GetCharCode refuses to work for keyDown
    // i have to play games.
    if (keyCode == nsIDOMKeyEvent::DOM_VK_TAB) character = '\t';
    else aKeyEvent->GetCharCode(&character);
    
    if (keyCode == nsIDOMKeyEvent::DOM_VK_TAB)
    {
      if (!(mFlags & eEditorPlaintextMask)) {
        nsCOMPtr<nsISelection>selection;
        res = GetSelection(getter_AddRefs(selection));
        if (NS_FAILED(res)) return res;
        PRInt32 offset;
        nsCOMPtr<nsIDOMNode> node, blockParent;
        res = GetStartNodeAndOffset(selection, address_of(node), &offset);
        if (NS_FAILED(res)) return res;
        if (!node) return NS_ERROR_FAILURE;

        PRBool isBlock = PR_FALSE;
        NodeIsBlock(node, &isBlock);
        if (isBlock) blockParent = node;
        else blockParent = GetBlockNodeParent(node);
        
        if (blockParent)
        {
          PRBool bHandled = PR_FALSE;
          
          if (nsHTMLEditUtils::IsTableElement(blockParent))
          {
            res = TabInTable(isShift, &bHandled);
            if (bHandled)
              ScrollSelectionIntoView(PR_FALSE);
          }
          else if (nsHTMLEditUtils::IsListItem(blockParent))
          {
            nsAutoString indentstr;
            if (isShift) indentstr.AssignLiteral("outdent");
            else         indentstr.AssignLiteral("indent");
            res = Indent(indentstr);
            bHandled = PR_TRUE;
          }
          if (NS_FAILED(res)) return res;
          if (bHandled)
            return aKeyEvent->PreventDefault(); // consumed
        }
      }
      if (isShift)
        return NS_OK; // don't type text for shift tabs
    }
    else if (keyCode == nsIDOMKeyEvent::DOM_VK_RETURN
             || keyCode == nsIDOMKeyEvent::DOM_VK_ENTER)
    {
      aKeyEvent->PreventDefault();
      nsString empty;
      if (isShift && !(mFlags&eEditorPlaintextMask))
      {
        return TypedText(empty, eTypedBR);  // only inserts a br node
      }
      else 
      {
        return TypedText(empty, eTypedBreak);  // uses rules to figure out what to insert
      }
    }
    else if (keyCode == nsIDOMKeyEvent::DOM_VK_ESCAPE)
    {
      aKeyEvent->PreventDefault();
      // pass escape keypresses through as empty strings: needed forime support
      nsString empty;
      return TypedText(empty, eTypedText);
    }
    
    // if we got here we either fell out of the tab case or have a normal character.
    // Either way, treat as normal character.
    if (character && !altKey && !ctrlKey && !metaKey)
    {
      aKeyEvent->PreventDefault();
      nsAutoString key(character);
      return TypedText(key, eTypedText);
    }
  }
  return NS_ERROR_FAILURE;
}

/* This routine is needed to provide a bottleneck for typing for logging
   purposes.  Can't use HandleKeyPress() (above) for that since it takes
   a nsIDOMKeyEvent* parameter.  So instead we pass enough info through
   to TypedText() to determine what action to take, but without passing
   an event.
   */
NS_IMETHODIMP nsHTMLEditor::TypedText(const nsAString& aString,
                                      PRInt32 aAction)
{
  nsAutoPlaceHolderBatch batch(this, gTypingTxnName);

  switch (aAction)
  {
    case eTypedText:
    case eTypedBreak:
      {
        return nsPlaintextEditor::TypedText(aString, aAction);
      }
    case eTypedBR:
      {
        nsCOMPtr<nsIDOMNode> brNode;
        return InsertBR(address_of(brNode));  // only inserts a br node
      }
  } 
  return NS_ERROR_FAILURE; 
}

NS_IMETHODIMP nsHTMLEditor::TabInTable(PRBool inIsShift, PRBool *outHandled)
{
  if (!outHandled) return NS_ERROR_NULL_POINTER;
  *outHandled = PR_FALSE;

  // Find enclosing table cell from the selection (cell may be the selected element)
  nsCOMPtr<nsIDOMElement> cellElement;
    // can't use |NS_LITERAL_STRING| here until |GetElementOrParentByTagName| is fixed to accept readables
  nsresult res = GetElementOrParentByTagName(NS_LITERAL_STRING("td"), nsnull, getter_AddRefs(cellElement));
  if (NS_FAILED(res)) return res;
  // Do nothing -- we didn't find a table cell
  if (!cellElement) return NS_OK;

  // find enclosing table
  nsCOMPtr<nsIDOMNode> tbl = GetEnclosingTable(cellElement);
  if (!tbl) return res;

  // advance to next cell
  // first create an iterator over the table
  nsCOMPtr<nsIContentIterator> iter =
      do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  if (NS_FAILED(res)) return res;
  if (!iter) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIContent> cTbl = do_QueryInterface(tbl);
  nsCOMPtr<nsIContent> cBlock = do_QueryInterface(cellElement);
  res = iter->Init(cTbl);
  if (NS_FAILED(res)) return res;
  // position iter at block
  res = iter->PositionAt(cBlock);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> node;
  do
  {
    if (inIsShift)
      iter->Prev();
    else
      iter->Next();

    node = do_QueryInterface(iter->GetCurrentNode());

    if (node && nsHTMLEditUtils::IsTableCell(node) &&
        GetEnclosingTable(node) == tbl)
    {
      res = CollapseSelectionToDeepestNonTableFirstChild(nsnull, node);
      if (NS_FAILED(res)) return res;
      *outHandled = PR_TRUE;
      return NS_OK;
    }
  } while (!iter->IsDone());
  
  if (!(*outHandled) && !inIsShift)
  {
    // if we havent handled it yet then we must have run off the end of
    // the table.  Insert a new row.
    res = InsertTableRow(1, PR_TRUE);
    if (NS_FAILED(res)) return res;
    *outHandled = PR_TRUE;
    // put selection in right place
    // Use table code to get selection and index to new row...
    nsCOMPtr<nsISelection>selection;
    nsCOMPtr<nsIDOMElement> tblElement;
    nsCOMPtr<nsIDOMElement> cell;
    PRInt32 row;
    res = GetCellContext(getter_AddRefs(selection), 
                         getter_AddRefs(tblElement),
                         getter_AddRefs(cell), 
                         nsnull, nsnull,
                         &row, nsnull);
    if (NS_FAILED(res)) return res;
    // ...so that we can ask for first cell in that row...
    res = GetCellAt(tblElement, row, 0, getter_AddRefs(cell));
    if (NS_FAILED(res)) return res;
    // ...and then set selection there.
    // (Note that normally you should use CollapseSelectionToDeepestNonTableFirstChild(),
    //  but we know cell is an empty new cell, so this works fine)
    node = do_QueryInterface(cell);
    if (node) selection->Collapse(node,0);
    return NS_OK;
  }
  
  return res;
}

NS_IMETHODIMP nsHTMLEditor::CreateBRImpl(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                                         PRInt32 *aInOutOffset, 
                                         nsCOMPtr<nsIDOMNode> *outBRNode, 
                                         EDirection aSelect)
{
  if (!aInOutParent || !*aInOutParent || !aInOutOffset || !outBRNode) return NS_ERROR_NULL_POINTER;
  *outBRNode = nsnull;
  nsresult res;
  
  // we need to insert a br.  unfortunately, we may have to split a text node to do it.
  nsCOMPtr<nsIDOMNode> node = *aInOutParent;
  PRInt32 theOffset = *aInOutOffset;
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(node);
  NS_NAMED_LITERAL_STRING(brType, "br");
  nsCOMPtr<nsIDOMNode> brNode;
  if (nodeAsText)  
  {
    nsCOMPtr<nsIDOMNode> tmp;
    PRInt32 offset;
    PRUint32 len;
    nodeAsText->GetLength(&len);
    GetNodeLocation(node, address_of(tmp), &offset);
    if (!tmp) return NS_ERROR_FAILURE;
    if (!theOffset)
    {
      // we are already set to go
    }
    else if (theOffset == (PRInt32)len)
    {
      // update offset to point AFTER the text node
      offset++;
    }
    else
    {
      // split the text node
      res = SplitNode(node, theOffset, getter_AddRefs(tmp));
      if (NS_FAILED(res)) return res;
      res = GetNodeLocation(node, address_of(tmp), &offset);
      if (NS_FAILED(res)) return res;
    }
    // create br
    res = CreateNode(brType, tmp, offset, getter_AddRefs(brNode));
    if (NS_FAILED(res)) return res;
    *aInOutParent = tmp;
    *aInOutOffset = offset+1;
  }
  else
  {
    res = CreateNode(brType, node, theOffset, getter_AddRefs(brNode));
    if (NS_FAILED(res)) return res;
    (*aInOutOffset)++;
  }

  *outBRNode = brNode;
  if (*outBRNode && (aSelect != eNone))
  {
    nsCOMPtr<nsISelection> selection;
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
    res = GetNodeLocation(*outBRNode, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    if (aSelect == eNext)
    {
      // position selection after br
      selPriv->SetInterlinePosition(PR_TRUE);
      res = selection->Collapse(parent, offset+1);
    }
    else if (aSelect == ePrevious)
    {
      // position selection before br
      selPriv->SetInterlinePosition(PR_TRUE);
      res = selection->Collapse(parent, offset);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsHTMLEditor::CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  return CreateBRImpl(address_of(parent), &offset, outBRNode, aSelect);
}

NS_IMETHODIMP nsHTMLEditor::InsertBR(nsCOMPtr<nsIDOMNode> *outBRNode)
{
  PRBool bCollapsed;
  nsCOMPtr<nsISelection> selection;

  if (!outBRNode) return NS_ERROR_NULL_POINTER;
  *outBRNode = nsnull;

  // calling it text insertion to trigger moz br treatment by rules
  nsAutoRules beginRulesSniffing(this, kOpInsertText, nsIEditor::eNext);

  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  res = GetStartNodeAndOffset(selection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  
  res = CreateBR(selNode, selOffset, outBRNode);
  if (NS_FAILED(res)) return res;
    
  // position selection after br
  res = GetNodeLocation(*outBRNode, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  selPriv->SetInterlinePosition(PR_TRUE);
  res = selection->Collapse(selNode, selOffset+1);
  
  return res;
}

nsresult 
nsHTMLEditor::GetDOMEventReceiver(nsIDOMEventReceiver **aEventReceiver) 
{ 
  if (!aEventReceiver) 
    return NS_ERROR_NULL_POINTER; 

  *aEventReceiver = 0; 

  nsCOMPtr<nsIDOMElement> rootElement; 

  nsresult result = GetRootElement(getter_AddRefs(rootElement)); 

  if (NS_FAILED(result)) 
    return result; 

  if (!rootElement) 
    return NS_ERROR_FAILURE; 

  // Now hack to make sure we are not anonymous content. 
  // If we are grab the parent of root element for our observer. 

  nsCOMPtr<nsIContent> content = do_QueryInterface(rootElement); 

  if (content) 
  { 
    nsIContent* parent = content->GetParent();
    if (parent)
    { 
      if (parent->IndexOf(content) < 0)
      { 
        rootElement = do_QueryInterface(parent); //this will put listener on the form element basically 
        result = CallQueryInterface(rootElement, aEventReceiver); 
      } 
      else 
        rootElement = 0; // Let the event receiver work on the document instead of the root element 
    } 
  } 
  else 
    rootElement = 0; 

  if (!rootElement && mDocWeak) 
  { 
    // Don't use getDocument here, because we have no way of knowing if 
    // Init() was ever called.  So we need to get the document ourselves, 
    // if it exists. 

    nsCOMPtr<nsIDOMDocument> domdoc = do_QueryReferent(mDocWeak); 

    if (!domdoc) 
      return NS_ERROR_FAILURE; 

    result = domdoc->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), (void **)aEventReceiver); 
  } 

  return result; 
} 
  
nsresult 
nsHTMLEditor::CollapseSelectionToDeepestNonTableFirstChild(nsISelection *aSelection, nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  nsresult res;

  nsCOMPtr<nsISelection> selection;
  if (aSelection)
  {
    selection = aSelection;
  } else {
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> child;
  
  do {
    node->GetFirstChild(getter_AddRefs(child));
    
    if (child)
    {
      // Stop if we find a table
      // don't want to go into nested tables
      if (nsHTMLEditUtils::IsTable(child)) break;
      // hey, it'g gotta be a container too!
      if (!IsContainer(child)) break;
      node = child;
    }
  }
  while (child);

  selection->Collapse(node,0);
  return NS_OK;
}


// This is mostly like InsertHTMLWithCharsetAndContext, 
//  but we can't use that because it is selection-based and 
//  the rules code won't let us edit under the <head> node
NS_IMETHODIMP
nsHTMLEditor::ReplaceHeadContentsWithHTML(const nsAString& aSourceToInsert)
{
  nsAutoRules beginRulesSniffing(this, kOpIgnore, nsIEditor::eNone); // dont do any post processing, rules get confused
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  ForceCompositionEnd();

  // Do not use nsAutoRules -- rules code won't let us insert in <head>
  // Use the head node as a parent and delete/insert directly
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMNodeList>nodeList; 
  res = doc->GetElementsByTagName(NS_LITERAL_STRING("head"), getter_AddRefs(nodeList));
  if (NS_FAILED(res)) return res;
  if (!nodeList) return NS_ERROR_NULL_POINTER;

  PRUint32 count; 
  nodeList->GetLength(&count);
  if (count < 1) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> headNode;
  res = nodeList->Item(0, getter_AddRefs(headNode)); 
  if (NS_FAILED(res)) return res;
  if (!headNode) return NS_ERROR_NULL_POINTER;

  // First, make sure there are no return chars in the source.
  // Bad things happen if you insert returns (instead of dom newlines, \n)
  // into an editor document.
  nsAutoString inputString (aSourceToInsert);  // hope this does copy-on-write
 
  // Windows linebreaks: Map CRLF to LF:
  inputString.ReplaceSubstring(NS_LITERAL_STRING("\r\n").get(),
                               NS_LITERAL_STRING("\n").get());
 
  // Mac linebreaks: Map any remaining CR to LF:
  inputString.ReplaceSubstring(NS_LITERAL_STRING("\r").get(),
                               NS_LITERAL_STRING("\n").get());

  nsAutoEditBatch beginBatching(this);

  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // Get the first range in the selection, for context:
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res))
    return res;

  nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(range));
  if (!nsrange)
    return NS_ERROR_NO_INTERFACE;
  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  res = nsrange->CreateContextualFragment(inputString,
                                          getter_AddRefs(docfrag));

  //XXXX BUG 50965: This is not returning the text between <title> ... </title>
  // Special code is needed in JS to handle title anyway, so it really doesn't matter!

  if (NS_FAILED(res))
  {
#ifdef DEBUG
    printf("Couldn't create contextual fragment: error was %d\n", res);
#endif
    return res;
  }
  if (!docfrag) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> child;

  // First delete all children in head
  do {
    res = headNode->GetFirstChild(getter_AddRefs(child));
    if (NS_FAILED(res)) return res;
    if (child)
    {
      res = DeleteNode(child);
      if (NS_FAILED(res)) return res;
    }
  } while (child);

  // Now insert the new nodes
  PRInt32 offsetOfNewNode = 0;
  nsCOMPtr<nsIDOMNode> fragmentAsNode (do_QueryInterface(docfrag));

  // Loop over the contents of the fragment and move into the document
  do {
    res = fragmentAsNode->GetFirstChild(getter_AddRefs(child));
    if (NS_FAILED(res)) return res;
    if (child)
    {
      res = InsertNode(child, headNode, offsetOfNewNode++);
      if (NS_FAILED(res)) return res;
    }
  } while (child);

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::RebuildDocumentFromSource(const nsAString& aSourceString)
{
  ForceCompositionEnd();

  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMElement> bodyElement;
  res = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_NULL_POINTER;

  // Find where the <body> tag starts.
  // If user mangled that, then abort
  nsReadingIterator<PRUnichar> beginbody;
  nsReadingIterator<PRUnichar> endbody;
  aSourceString.BeginReading(beginbody);
  aSourceString.EndReading(endbody);
  if (!CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<body"),
                                     beginbody, endbody))
    return NS_ERROR_FAILURE;

  nsReadingIterator<PRUnichar> beginhead;
  nsReadingIterator<PRUnichar> endhead;
  aSourceString.BeginReading(beginhead);
  aSourceString.EndReading(endhead);
  if (!CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<head"),
                                     beginhead, endhead))
    return NS_ERROR_FAILURE;

  nsReadingIterator<PRUnichar> beginclosehead;
  nsReadingIterator<PRUnichar> endclosehead;
  aSourceString.BeginReading(beginclosehead);
  aSourceString.EndReading(endclosehead);

  // Find the index after "<head>"
  if (!CaseInsensitiveFindInReadable(NS_LITERAL_STRING("</head"),
                                     beginclosehead, endclosehead))
    beginclosehead = beginbody;
  // We'll be forgiving and assume head ends before body
  
  // Time to change the document
  nsAutoEditBatch beginBatching(this);

  // Try to replace body contents first
  res = SelectAll();
  if (NS_FAILED(res)) return res;

  nsReadingIterator<PRUnichar> endtotal;
  aSourceString.EndReading(endtotal);

  res = LoadHTML(Substring(beginbody,endtotal));
  if (NS_FAILED(res)) return res;
  selection->Collapse(bodyElement, 0);

  res = ReplaceHeadContentsWithHTML(Substring(beginhead,beginclosehead));
  if (NS_FAILED(res)) return res;

  // Now we must copy attributes user might have edited on the <body> tag
  //  because InsertHTML (actually, CreateContextualFragment()) 
  //  will never return a body node in the DOM fragment
  
  // We already know where "<body" begins
  nsReadingIterator<PRUnichar> beginclosebody = beginbody;
  nsReadingIterator<PRUnichar> endclosebody;
  aSourceString.EndReading(endclosebody);
  if (!FindInReadable(NS_LITERAL_STRING(">"),beginclosebody,endclosebody))
    return NS_ERROR_FAILURE;

  nsAutoString bodyTag(Substring(beginbody,endclosebody));//<bodyXXXX >
  // Truncate at the end of the body tag
  
  // Kludge of the year: fool the parser by replacing "body" with "div" so we get a node
  bodyTag.ReplaceSubstring(NS_LITERAL_STRING("body").get(),
                           NS_LITERAL_STRING("div").get());

  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(range));
  if (!nsrange) return NS_ERROR_NO_INTERFACE;

  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  res = nsrange->CreateContextualFragment(bodyTag, getter_AddRefs(docfrag));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> fragmentAsNode (do_QueryInterface(docfrag));
  if (!fragmentAsNode) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMNode> child;
  res = fragmentAsNode->GetFirstChild(getter_AddRefs(child));
  if (NS_FAILED(res)) return res;
  if (!child) return NS_ERROR_NULL_POINTER;
  
  // Copy all attributes from the div child to current body element
  res = CloneAttributes(bodyElement, child);
  if (NS_FAILED(res)) return res;
  
  // place selection at first editable content
  return BeginningOfDocument();
}

void
nsHTMLEditor::NormalizeEOLInsertPosition(nsIDOMNode *firstNodeToInsert,
                                     nsCOMPtr<nsIDOMNode> *insertParentNode,
                                     PRInt32 *insertOffset)
{
  /*
    This function will either correct the position passed in,
    or leave the position unchanged.

    When the (first) item to insert is a block level element, 
    and our insertion position is after the last visible item in a line, 
    i.e. the insertion position is just before a visible line break <br>, 
    we want to skip to the position just after the line break (see bug 68767)

    However, our logic to detect whether we should skip or not
    needs to be more clever.
    We must not skip when the caret appears to be positioned at the beginning
    of a block, in that case skipping the <br> would not insert the <br>
    at the caret position, but after the current empty line.
     
    So we have several cases to test:
     
    1) We only ever want to skip, if the next visible thing after the current position is a break
     
    2) We do not want to skip if there is no previous visible thing at all
       That is detected if the call to PriorVisibleNode gives us an offset of zero.
       Because PriorVisibleNode always positions after the prior node, we would
       see an offset > 0, if there were a prior node.
     
    3) We do not want to skip, if both the next and the previous visible things are breaks.
    
    4) We do not want to skip if the previous visible thing is in a different block
       than the insertion position.
  */

  if (!IsBlockNode(firstNodeToInsert))
    return;

  nsWSRunObject wsObj(this, *insertParentNode, *insertOffset);
  nsCOMPtr<nsIDOMNode> nextVisNode;
  nsCOMPtr<nsIDOMNode> prevVisNode;
  PRInt32 nextVisOffset=0;
  PRInt16 nextVisType=0;
  PRInt32 prevVisOffset=0;
  PRInt16 prevVisType=0;

  wsObj.NextVisibleNode(*insertParentNode, *insertOffset, address_of(nextVisNode), &nextVisOffset, &nextVisType);
  if (!nextVisNode)
    return;

  if (! (nextVisType & nsWSRunObject::eBreak))
    return;

  wsObj.PriorVisibleNode(*insertParentNode, *insertOffset, address_of(prevVisNode), &prevVisOffset, &prevVisType);
  if (!prevVisNode)
    return;

  if (prevVisType & nsWSRunObject::eBreak)
    return;

  if (prevVisType & nsWSRunObject::eThisBlock)
    return;

  nsCOMPtr<nsIDOMNode> brNode;
  PRInt32 brOffset=0;

  GetNodeLocation(nextVisNode, address_of(brNode), &brOffset);

  *insertParentNode = brNode;
  *insertOffset = brOffset + 1;
}

NS_IMETHODIMP
nsHTMLEditor::InsertElementAtSelection(nsIDOMElement* aElement, PRBool aDeleteSelection)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;
  
  if (!aElement)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
  
  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);

  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
    return NS_ERROR_FAILURE;

  // hand off to the rules system, see if it has anything to say about this
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
  ruleInfo.insertElement = aElement;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    if (aDeleteSelection)
    {
      nsCOMPtr<nsIDOMNode> tempNode;
      PRInt32 tempOffset;
      nsresult result = DeleteSelectionAndPrepareToCreateNode(tempNode,tempOffset);
      if (NS_FAILED(result))
        return result;
    }

    // If deleting, selection will be collapsed.
    // so if not, we collapse it
    if (!aDeleteSelection)
    {
      // Named Anchor is a special case,
      // We collapse to insert element BEFORE the selection
      // For all other tags, we insert AFTER the selection
      if (nsHTMLEditUtils::IsNamedAnchor(node))
      {
        selection->CollapseToStart();
      } else {
        selection->CollapseToEnd();
      }
    }

    nsCOMPtr<nsIDOMNode> parentSelectedNode;
    PRInt32 offsetForInsert;
    res = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
    // XXX: ERROR_HANDLING bad XPCOM usage
    if (NS_SUCCEEDED(res) && NS_SUCCEEDED(selection->GetAnchorOffset(&offsetForInsert)) && parentSelectedNode)
    {
#ifdef DEBUG_cmanske
      {
      nsAutoString name;
      parentSelectedNode->GetNodeName(name);
      printf("InsertElement: Anchor node of selection: ");
      wprintf(name.get());
      printf(" Offset: %d\n", offsetForInsert);
      }
#endif

      // Adjust position based on the node we are going to insert.
      NormalizeEOLInsertPosition(node, address_of(parentSelectedNode), &offsetForInsert);

      res = InsertNodeAtPoint(node, address_of(parentSelectedNode), &offsetForInsert, PR_FALSE);
      NS_ENSURE_SUCCESS(res, res);
      // Set caret after element, but check for special case 
      //  of inserting table-related elements: set in first cell instead
      if (!SetCaretInTableCell(aElement))
      {
        res = SetCaretAfterElement(aElement);
        if (NS_FAILED(res)) return res;
      }
      // check for inserting a whole table at the end of a block. If so insert a br after it.
      if (nsHTMLEditUtils::IsTable(node))
      {
        PRBool isLast;
        res = IsLastEditableChild(node, &isLast);
        if (NS_FAILED(res)) return res;
        if (isLast)
        {
          nsCOMPtr<nsIDOMNode> brNode;
          res = CreateBR(parentSelectedNode, offsetForInsert+1, address_of(brNode));
          if (NS_FAILED(res)) return res;
          selection->Collapse(parentSelectedNode, offsetForInsert+1);
        }
      }
    }
  }
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}


/* 
  InsertNodeAtPoint: attempts to insert aNode into the document, at a point specified by 
      {*ioParent,*ioOffset}.  Checks with strict dtd to see if containment is allowed.  If not
      allowed, will attempt to find a parent in the parent heirarchy of *ioParent that will
      accept aNode as a child.  If such a parent is found, will split the document tree from
      {*ioParent,*ioOffset} up to parent, and then insert aNode.  ioParent & ioOffset are then
      adjusted to point to the actual location that aNode was inserted at.  aNoEmptyNodes
      specifies if the splitting process is allowed to reslt in empty nodes.
              nsIDOMNode            *aNode           node to insert
              nsCOMPtr<nsIDOMNode>  *ioParent        insertion parent
              PRInt32               *ioOffset        insertion offset
              PRBool                aNoEmptyNodes    splitting can result in empty nodes?
*/
nsresult
nsHTMLEditor::InsertNodeAtPoint(nsIDOMNode *aNode, 
                                nsCOMPtr<nsIDOMNode> *ioParent, 
                                PRInt32 *ioOffset, 
                                PRBool aNoEmptyNodes)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(ioParent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(*ioParent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(ioOffset, NS_ERROR_NULL_POINTER);
  
  nsresult res = NS_OK;
  nsAutoString tagName;
  aNode->GetNodeName(tagName);
  ToLowerCase(tagName);
  nsCOMPtr<nsIDOMNode> parent = *ioParent;
  nsCOMPtr<nsIDOMNode> topChild = *ioParent;
  nsCOMPtr<nsIDOMNode> tmp;
  PRInt32 offsetOfInsert = *ioOffset;
   
  // Search up the parent chain to find a suitable container      
  while (!CanContainTag(parent, tagName))
  {
    // If the current parent is a root (body or table element)
    // then go no further - we can't insert
    if (nsTextEditUtils::IsBody(parent) || nsHTMLEditUtils::IsTableElement(parent))
      return NS_ERROR_FAILURE;
    // Get the next parent
    parent->GetParentNode(getter_AddRefs(tmp));
    NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
    topChild = parent;
    parent = tmp;
  }
  if (parent != topChild)
  {
    // we need to split some levels above the original selection parent
    res = SplitNodeDeep(topChild, *ioParent, *ioOffset, &offsetOfInsert, aNoEmptyNodes);
    if (NS_FAILED(res))
      return res;
    *ioParent = parent;
    *ioOffset = offsetOfInsert;
  }
  // Now we can insert the new node
  res = InsertNode(aNode, parent, offsetOfInsert);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SelectElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Must be sure that element is contained in the document body
  if (IsElementInBody(aElement))
  {
    nsCOMPtr<nsISelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    if (NS_SUCCEEDED(res) && parent)
    {
      PRInt32 offsetInParent;
      res = GetChildOffset(aElement, parent, offsetInParent);

      if (NS_SUCCEEDED(res))
      {
        // Collapse selection to just before desired element,
        res = selection->Collapse(parent, offsetInParent);
        if (NS_SUCCEEDED(res)) {
          //  then extend it to just after
          res = selection->Extend(parent, offsetInParent+1);
        }
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetCaretAfterElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Be sure the element is contained in the document body
  if (aElement && IsElementInBody(aElement))
  {
    nsCOMPtr<nsISelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(res)) return res;
    if (!parent) return NS_ERROR_NULL_POINTER;
    PRInt32 offsetInParent;
    res = GetChildOffset(aElement, parent, offsetInParent);
    if (NS_SUCCEEDED(res))
    {
      // Collapse selection to just after desired element,
      res = selection->Collapse(parent, offsetInParent+1);
#if 0 //def DEBUG_cmanske
      {
      nsAutoString name;
      parent->GetNodeName(name);
      printf("SetCaretAfterElement: Parent node: ");
      wprintf(name.get());
      printf(" Offset: %d\n\nHTML:\n", offsetInParent+1);
      nsAutoString Format("text/html");
      nsAutoString ContentsAs;
      OutputToString(Format, 2, ContentsAs);
      wprintf(ContentsAs.get());
      }
#endif
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SetParagraphFormat(const nsAString& aParagraphFormat)
{
  nsAutoString tag; tag.Assign(aParagraphFormat);
  ToLowerCase(tag);
  if (tag.EqualsLiteral("dd") || tag.EqualsLiteral("dt"))
    return MakeDefinitionItem(tag);
  else
    return InsertBasicBlock(tag);
}

// XXX: ERROR_HANDLING -- this method needs a little work to ensure all error codes are 
//                        checked properly, all null pointers are checked, and no memory leaks occur
NS_IMETHODIMP 
nsHTMLEditor::GetParentBlockTags(nsStringArray *aTagList, PRBool aGetLists)
{
  if (!aTagList) { return NS_ERROR_NULL_POINTER; }

  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  // Find out if the selection is collapsed:
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;
  if (isCollapsed)
  {
    nsCOMPtr<nsIDOMNode> node, blockParent;
    PRInt32 offset;
  
    res = GetStartNodeAndOffset(selection, address_of(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    if (NS_FAILED(res)) return res;
  
    nsCOMPtr<nsIDOMElement> blockParentElem;
    if (aGetLists)
    {
      // Get the "ol", "ul", or "dl" parent element
      res = GetElementOrParentByTagName(NS_LITERAL_STRING("list"), node, getter_AddRefs(blockParentElem));
      if (NS_FAILED(res)) return res;
    } 
    else 
    {
      PRBool isBlock (PR_FALSE);
      NodeIsBlock(node, &isBlock);
      if (isBlock) blockParent = node;
      else blockParent = GetBlockNodeParent(node);
      blockParentElem = do_QueryInterface(blockParent);
    }
    if (blockParentElem)
    {
      nsAutoString blockParentTag;
      blockParentElem->GetTagName(blockParentTag);
      aTagList->AppendString(blockParentTag);
    }
    
    return res;
  }

  // else non-collapsed selection
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if (NS_FAILED(res)) return res;
  //XXX: should be while loop?
  if (currentItem)
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    // scan the range for all the independent block content blockSections
    // and get the block parent of each
    nsCOMArray<nsIDOMRange> blockSections;
    res = GetBlockSectionsForRange(range, blockSections);
    if (NS_SUCCEEDED(res))
    {
      nsCOMPtr<nsIDOMRange> subRange = blockSections[0];
      while (subRange)
      {
        nsCOMPtr<nsIDOMNode>startParent;
        res = subRange->GetStartContainer(getter_AddRefs(startParent));
        if (NS_SUCCEEDED(res) && startParent) 
        {
          nsCOMPtr<nsIDOMElement> blockParent;
          if (aGetLists)
          {
            // Get the "ol", "ul", or "dl" parent element
            res = GetElementOrParentByTagName(NS_LITERAL_STRING("list"), startParent, getter_AddRefs(blockParent));
          } 
          else 
          {
            blockParent = do_QueryInterface(GetBlockNodeParent(startParent));
          }
          if (NS_SUCCEEDED(res) && blockParent)
          {
            nsAutoString blockParentTag;
            blockParent->GetTagName(blockParentTag);
            PRBool isRoot;
            IsRootTag(blockParentTag, isRoot);
            if ((!isRoot) && (-1==aTagList->IndexOf(blockParentTag))) {
              aTagList->AppendString(blockParentTag);
            }
          }
        }
        if (NS_FAILED(res))
          return res;
        blockSections.RemoveObject(0);
        if (blockSections.Count() == 0)
          break;
        subRange = blockSections[0];
      }
    }
  }
  return res;
}


NS_IMETHODIMP 
nsHTMLEditor::GetParagraphState(PRBool *aMixed, nsAString &outFormat)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  if (!aMixed) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetParagraphState(aMixed, outFormat);
}

NS_IMETHODIMP
nsHTMLEditor::GetBackgroundColorState(PRBool *aMixed, nsAString &aOutColor)
{
  nsresult res;
  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);
  if (useCSS) {
    // if we are in CSS mode, we have to check if the containing block defines
    // a background color
    res = GetCSSBackgroundColorState(aMixed, aOutColor, PR_TRUE);
  }
  else {
    // in HTML mode, we look only at page's background
    res = GetHTMLBackgroundColorState(aMixed, aOutColor);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetHighlightColorState(PRBool *aMixed, nsAString &aOutColor)
{
  nsresult res = NS_OK;
  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);
  *aMixed = PR_FALSE;
  aOutColor.AssignLiteral("transparent");
  if (useCSS) {
    // in CSS mode, text background can be added by the Text Highlight button
    // we need to query the background of the selection without looking for
    // the block container of the ranges in the selection
    res = GetCSSBackgroundColorState(aMixed, aOutColor, PR_FALSE);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetHighlightColor(PRBool *aMixed, PRUnichar **_retval)
{
  if (!aMixed || !_retval) return NS_ERROR_NULL_POINTER;
  nsAutoString outColorString(NS_LITERAL_STRING("transparent"));
  *aMixed = PR_FALSE;

  nsresult  err = NS_NOINTERFACE;
  err = GetHighlightColorState(aMixed, outColorString);
  *_retval = ToNewUnicode(outColorString);
  return err;
}


nsresult
nsHTMLEditor::GetCSSBackgroundColorState(PRBool *aMixed, nsAString &aOutColor, PRBool aBlockLevel)
{
  if (!aMixed) return NS_ERROR_NULL_POINTER;
  *aMixed = PR_FALSE;
  // the default background color is transparent
  aOutColor.AssignLiteral("transparent");
  
  // get selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  // get selection location
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = GetStartNodeAndOffset(selection, address_of(parent), &offset);
  if (NS_FAILED(res)) return res;
  
  // is the selection collapsed?
  PRBool bCollapsed;
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode> nodeToExamine;
  if (bCollapsed || IsTextNode(parent))
  {
    // we want to look at the parent and ancestors
    nodeToExamine = parent;
  }
  else
  {
    // otherwise we want to look at the first editable node after
    // {parent,offset} and it's ancestors for divs with alignment on them
    nodeToExamine = GetChildAt(parent, offset);
    //GetNextNode(parent, offset, PR_TRUE, address_of(nodeToExamine));
  }
  
  if (!nodeToExamine) return NS_ERROR_NULL_POINTER;

  // is the node to examine a block ?
  PRBool isBlock;
  res = NodeIsBlockStatic(nodeToExamine, &isBlock);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMHTMLHtmlElement> htmlElement;
  nsCOMPtr<nsIDOMNode> tmp;

  if (aBlockLevel) {
    // we are querying the block background (and not the text background), let's
    // climb to the block container
    nsCOMPtr<nsIDOMNode> blockParent = nodeToExamine;
    if (!isBlock) {
      blockParent = GetBlockNodeParent(nodeToExamine);
    }
    do {
      // retrieve the computed style of background-color for blockParent
      mHTMLCSSUtils->GetComputedProperty(blockParent, nsEditProperty::cssBackgroundColor,
                                         aOutColor);
      tmp = blockParent;
      res = tmp->GetParentNode(getter_AddRefs(blockParent));
      htmlElement = do_QueryInterface(tmp);
      // look at parent if the queried color is transparent and if the node to
      // examine is not the root of the document
    } while ( aOutColor.EqualsLiteral("transparent") && htmlElement );
    if (!htmlElement && aOutColor.EqualsLiteral("transparent")) {
      // we have hit the root of the document and the color is still transparent !
      // Grumble... Let's look at the default background color because that's the
      // color we are looking for
      mHTMLCSSUtils->GetDefaultBackgroundColor(aOutColor);
    }
  }
  else {
    // no, we are querying the text background for the Text Highlight button
    if (IsTextNode(nodeToExamine)) {
      // if the node of interest is a text node, let's climb a level
      res = nodeToExamine->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(res)) return res;
      nodeToExamine = parent;
    }
    do {
      // is the node to examine a block ?
      res = NodeIsBlockStatic(nodeToExamine, &isBlock);
      if (NS_FAILED(res)) return res;
      if (isBlock) {
        // yes it is a block; in that case, the text background color is transparent
        aOutColor.AssignLiteral("transparent");
        break;
      }
      else {
        // no, it's not; let's retrieve the computed style of background-color for the
        // node to examine
        mHTMLCSSUtils->GetComputedProperty(nodeToExamine, nsEditProperty::cssBackgroundColor,
                            aOutColor);
        if (!aOutColor.EqualsLiteral("transparent")) {
          break;
        }
      }
      res = nodeToExamine->GetParentNode(getter_AddRefs(tmp));
      if (NS_FAILED(res)) return res;
      nodeToExamine = tmp;
      htmlElement = do_QueryInterface(tmp);
    } while ( aOutColor.EqualsLiteral("transparent") && htmlElement );
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditor::GetHTMLBackgroundColorState(PRBool *aMixed, nsAString &aOutColor)
{
  //TODO: We don't handle "mixed" correctly!
  if (!aMixed) return NS_ERROR_NULL_POINTER;
  *aMixed = PR_FALSE;
  aOutColor.Truncate();
  
  nsCOMPtr<nsIDOMElement> element;
  PRInt32 selectedCount;
  nsAutoString tagName;
  nsresult res = GetSelectedOrParentTableElement(tagName,
                                                 &selectedCount,
                                                 getter_AddRefs(element));
  if (NS_FAILED(res)) return res;

  NS_NAMED_LITERAL_STRING(styleName, "bgcolor"); 

  while (element)
  {
    // We are in a cell or selected table
    res = element->GetAttribute(styleName, aOutColor);
    if (NS_FAILED(res)) return res;

    // Done if we have a color explicitly set
    if (!aOutColor.IsEmpty())
      return NS_OK;

    // Once we hit the body, we're done
    if(nsTextEditUtils::IsBody(element)) return NS_OK;

    // No color is set, but we need to report visible color inherited 
    // from nested cells/tables, so search up parent chain
    nsCOMPtr<nsIDOMNode> parentNode;
    res = element->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    element = do_QueryInterface(parentNode);
  }

  // If no table or cell found, get page body
  res = nsEditor::GetRootElement(getter_AddRefs(element));
  if (NS_FAILED(res)) return res;
  if (!element) return NS_ERROR_NULL_POINTER;

  return element->GetAttribute(styleName, aOutColor);
}

NS_IMETHODIMP 
nsHTMLEditor::GetListState(PRBool *aMixed, PRBool *aOL, PRBool *aUL, PRBool *aDL)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  if (!aMixed || !aOL || !aUL || !aDL) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetListState(aMixed, aOL, aUL, aDL);
}

NS_IMETHODIMP 
nsHTMLEditor::GetListItemState(PRBool *aMixed, PRBool *aLI, PRBool *aDT, PRBool *aDD)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  if (!aMixed || !aLI || !aDT || !aDD) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetListItemState(aMixed, aLI, aDT, aDD);
}

NS_IMETHODIMP
nsHTMLEditor::GetAlignment(PRBool *aMixed, nsIHTMLEditor::EAlignment *aAlign)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  if (!aMixed || !aAlign) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetAlignment(aMixed, aAlign);
}


NS_IMETHODIMP 
nsHTMLEditor::GetIndentState(PRBool *aCanIndent, PRBool *aCanOutdent)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  if (!aCanIndent || !aCanOutdent) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetIndentState(aCanIndent, aCanOutdent);
}

NS_IMETHODIMP
nsHTMLEditor::MakeOrChangeList(const nsAString& aListType, PRBool entireList, const nsAString& aBulletType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsISelection> selection;
  PRBool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpMakeList, nsIEditor::eNext);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsTextEditRules::kMakeList);
  ruleInfo.blockType = &aListType;
  ruleInfo.entireList = entireList;
  ruleInfo.bulletType = &aBulletType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Find out if the selection is collapsed:
    PRBool isCollapsed;
    res = selection->GetIsCollapsed(&isCollapsed);
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsIDOMNode> node;
    PRInt32 offset;
  
    res = GetStartNodeAndOffset(selection, address_of(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    if (NS_FAILED(res)) return res;
  
    if (isCollapsed)
    {
      // have to find a place to put the list
      nsCOMPtr<nsIDOMNode> parent = node;
      nsCOMPtr<nsIDOMNode> topChild = node;
      nsCOMPtr<nsIDOMNode> tmp;
    
      while ( !CanContainTag(parent, aListType))
      {
        parent->GetParentNode(getter_AddRefs(tmp));
        if (!tmp) return NS_ERROR_FAILURE;
        topChild = parent;
        parent = tmp;
      }
    
      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(topChild, node, offset, &offset);
        if (NS_FAILED(res)) return res;
      }

      // make a list
      nsCOMPtr<nsIDOMNode> newList;
      res = CreateNode(aListType, parent, offset, getter_AddRefs(newList));
      if (NS_FAILED(res)) return res;
      // make a list item
      nsCOMPtr<nsIDOMNode> newItem;
      res = CreateNode(NS_LITERAL_STRING("li"), newList, 0, getter_AddRefs(newItem));
      if (NS_FAILED(res)) return res;
      res = selection->Collapse(newItem,0);
      if (NS_FAILED(res)) return res;
    }
  }
  
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}


NS_IMETHODIMP
nsHTMLEditor::RemoveList(const nsAString& aListType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsISelection> selection;
  PRBool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpRemoveList, nsIEditor::eNext);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsTextEditRules::kRemoveList);
  if (aListType.LowerCaseEqualsLiteral("ol"))
    ruleInfo.bOrdered = PR_TRUE;
  else  ruleInfo.bOrdered = PR_FALSE;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  // no default behavior for this yet.  what would it mean?

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

nsresult
nsHTMLEditor::MakeDefinitionItem(const nsAString& aItemType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsISelection> selection;
  PRBool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpMakeDefListItem, nsIEditor::eNext);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kMakeDefListItem);
  ruleInfo.blockType = &aItemType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // todo: no default for now.  we count on rules to handle it.
  }

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

nsresult
nsHTMLEditor::InsertBasicBlock(const nsAString& aBlockType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsISelection> selection;
  PRBool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpMakeBasicBlock, nsIEditor::eNext);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kMakeBasicBlock);
  ruleInfo.blockType = &aBlockType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Find out if the selection is collapsed:
    PRBool isCollapsed;
    res = selection->GetIsCollapsed(&isCollapsed);
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsIDOMNode> node;
    PRInt32 offset;
  
    res = GetStartNodeAndOffset(selection, address_of(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    if (NS_FAILED(res)) return res;
  
    if (isCollapsed)
    {
      // have to find a place to put the block
      nsCOMPtr<nsIDOMNode> parent = node;
      nsCOMPtr<nsIDOMNode> topChild = node;
      nsCOMPtr<nsIDOMNode> tmp;
    
      while ( !CanContainTag(parent, aBlockType))
      {
        parent->GetParentNode(getter_AddRefs(tmp));
        if (!tmp) return NS_ERROR_FAILURE;
        topChild = parent;
        parent = tmp;
      }
    
      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(topChild, node, offset, &offset);
        if (NS_FAILED(res)) return res;
      }

      // make a block
      nsCOMPtr<nsIDOMNode> newBlock;
      res = CreateNode(aBlockType, parent, offset, getter_AddRefs(newBlock));
      if (NS_FAILED(res)) return res;
    
      // reposition selection to inside the block
      res = selection->Collapse(newBlock,0);
      if (NS_FAILED(res)) return res;  
    }
  }

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::Indent(const nsAString& aIndent)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  PRBool cancel, handled;
  PRInt32 theAction = nsTextEditRules::kIndent;
  PRInt32 opID = kOpIndent;
  if (aIndent.LowerCaseEqualsLiteral("outdent"))
  {
    theAction = nsTextEditRules::kOutdent;
    opID = kOpOutdent;
  }
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, opID, nsIEditor::eNext);
  
  // pre-process
  nsCOMPtr<nsISelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(theAction);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;
  
  if (!handled)
  {
    // Do default - insert a blockquote node if selection collapsed
    nsCOMPtr<nsIDOMNode> node;
    PRInt32 offset;
    PRBool isCollapsed;
    res = selection->GetIsCollapsed(&isCollapsed);
    if (NS_FAILED(res)) return res;

    res = GetStartNodeAndOffset(selection, address_of(node), &offset);
    if (!node) res = NS_ERROR_FAILURE;
    if (NS_FAILED(res)) return res;
  
    if (aIndent.EqualsLiteral("indent"))
    {
      if (isCollapsed)
      {
        // have to find a place to put the blockquote
        nsCOMPtr<nsIDOMNode> parent = node;
        nsCOMPtr<nsIDOMNode> topChild = node;
        nsCOMPtr<nsIDOMNode> tmp;
        NS_NAMED_LITERAL_STRING(bq, "blockquote");
        while ( !CanContainTag(parent, bq))
        {
          parent->GetParentNode(getter_AddRefs(tmp));
          if (!tmp) return NS_ERROR_FAILURE;
          topChild = parent;
          parent = tmp;
        }
    
        if (parent != node)
        {
          // we need to split up to the child of parent
          res = SplitNodeDeep(topChild, node, offset, &offset);
          if (NS_FAILED(res)) return res;
        }

        // make a blockquote
        nsCOMPtr<nsIDOMNode> newBQ;
        res = CreateNode(bq, parent, offset, getter_AddRefs(newBQ));
        if (NS_FAILED(res)) return res;
        // put a space in it so layout will draw the list item
        res = selection->Collapse(newBQ,0);
        if (NS_FAILED(res)) return res;
        res = InsertText(NS_LITERAL_STRING(" "));
        if (NS_FAILED(res)) return res;
        // reposition selection to before the space character
        res = GetStartNodeAndOffset(selection, address_of(node), &offset);
        if (NS_FAILED(res)) return res;
        res = selection->Collapse(node,0);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

//TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
nsHTMLEditor::Align(const nsAString& aAlignType)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpAlign, nsIEditor::eNext);

  nsCOMPtr<nsIDOMNode> node;
  PRBool cancel, handled;
  
  // Find out if the selection is collapsed:
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kAlign);
  ruleInfo.alignType = &aAlignType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(res))
    return res;
  
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetElementOrParentByTagName(const nsAString& aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)
{
  if (aTagName.IsEmpty() || !aReturn )
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> currentNode;

  if (aNode)
    currentNode = aNode;
  else
  {
    // If no node supplied, get it from anchor node of current selection
    nsCOMPtr<nsISelection>selection;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMNode> anchorNode;
    res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
    if(NS_FAILED(res)) return res;
    if (!anchorNode)  return NS_ERROR_FAILURE;

    // Try to get the actual selected node
    PRBool hasChildren = PR_FALSE;
    anchorNode->HasChildNodes(&hasChildren);
    if (hasChildren)
    {
      PRInt32 offset;
      res = selection->GetAnchorOffset(&offset);
      if(NS_FAILED(res)) return res;
      currentNode = nsEditor::GetChildAt(anchorNode, offset);
    }
    // anchor node is probably a text node - just use that
    if (!currentNode)
      currentNode = anchorNode;
  }
   
  nsAutoString TagName(aTagName);
  ToLowerCase(TagName);
  PRBool getLink = IsLinkTag(TagName);
  PRBool getNamedAnchor = IsNamedAnchorTag(TagName);
  if ( getLink || getNamedAnchor)
  {
    TagName.AssignLiteral("a");  
  }
  PRBool findTableCell = TagName.EqualsLiteral("td");
  PRBool findList = TagName.EqualsLiteral("list");

  // default is null - no element found
  *aReturn = nsnull;
  
  nsCOMPtr<nsIDOMNode> parent;
  PRBool bNodeFound = PR_FALSE;

  while (PR_TRUE)
  {
    nsAutoString currentTagName; 
    // Test if we have a link (an anchor with href set)
    if ( (getLink && nsHTMLEditUtils::IsLink(currentNode)) ||
         (getNamedAnchor && nsHTMLEditUtils::IsNamedAnchor(currentNode)) )
    {
      bNodeFound = PR_TRUE;
      break;
    } else {
      if (findList)
      {
        // Match "ol", "ul", or "dl" for lists
        if (nsHTMLEditUtils::IsList(currentNode))
          goto NODE_FOUND;

      } else if (findTableCell)
      {
        // Table cells are another special case:
        // Match either "td" or "th" for them
        if (nsHTMLEditUtils::IsTableCell(currentNode))
          goto NODE_FOUND;

      } else {
        currentNode->GetNodeName(currentTagName);
        if (currentTagName.Equals(TagName, nsCaseInsensitiveStringComparator()))
        {
NODE_FOUND:
          bNodeFound = PR_TRUE;
          break;
        } 
      }
    }
    // Search up the parent chain
    // We should never fail because of root test below, but lets be safe
    // XXX: ERROR_HANDLING error return code lost
    if (NS_FAILED(currentNode->GetParentNode(getter_AddRefs(parent))) || !parent)
      break;

    // Stop searching if parent is a body tag
    nsAutoString parentTagName;
    parent->GetNodeName(parentTagName);
    // Note: Originally used IsRoot to stop at table cells,
    //  but that's too messy when you are trying to find the parent table
    //PRBool isRoot;
    //if (NS_FAILED(IsRootTag(parentTagName, isRoot)) || isRoot)
    if(parentTagName.LowerCaseEqualsLiteral("body"))
      break;

    currentNode = parent;
  }
  if (bNodeFound)
  {
    nsCOMPtr<nsIDOMElement> currentElement = do_QueryInterface(currentNode);
    if (currentElement)
    {
      *aReturn = currentElement;
      // Getters must addref
      NS_ADDREF(*aReturn);
    }
  }
  else res = NS_EDITOR_ELEMENT_NOT_FOUND;

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectedElement(const nsAString& aTagName, nsIDOMElement** aReturn)
{
  if (!aReturn )
    return NS_ERROR_NULL_POINTER;
  
  // default is null - no element found
  *aReturn = nsnull;
  
  // First look for a single element in selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool bNodeFound = PR_FALSE;
  res=NS_ERROR_NOT_INITIALIZED;
  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);

  nsAutoString domTagName;
  nsAutoString TagName(aTagName);
  ToLowerCase(TagName);
  // Empty string indicates we should match any element tag
  PRBool anyTag = (TagName.IsEmpty());
  PRBool isLinkTag = IsLinkTag(TagName);
  PRBool isNamedAnchorTag = IsNamedAnchorTag(TagName);
  
  nsCOMPtr<nsIDOMElement> selectedElement;
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> startParent;
  PRInt32 startOffset, endOffset;
  res = range->GetStartContainer(getter_AddRefs(startParent));
  if (NS_FAILED(res)) return res;
  res = range->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> endParent;
  res = range->GetEndContainer(getter_AddRefs(endParent));
  if (NS_FAILED(res)) return res;
  res = range->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;

  // Optimization for a single selected element
  if (startParent && startParent == endParent && (endOffset-startOffset) == 1)
  {
    nsCOMPtr<nsIDOMNode> selectedNode = GetChildAt(startParent, startOffset);
    if (NS_FAILED(res)) return NS_OK;
    if (selectedNode)
    {
      selectedNode->GetNodeName(domTagName);
      ToLowerCase(domTagName);

      // Test for appropriate node type requested
      if (anyTag || (TagName == domTagName) ||
          (isLinkTag && nsHTMLEditUtils::IsLink(selectedNode)) ||
          (isNamedAnchorTag && nsHTMLEditUtils::IsNamedAnchor(selectedNode)))
      {
        bNodeFound = PR_TRUE;
        selectedElement = do_QueryInterface(selectedNode);
      }
    }
  }

  if (!bNodeFound)
  {
    if (isLinkTag)
    {
      // Link tag is a special case - we return the anchor node
      //  found for any selection that is totally within a link,
      //  included a collapsed selection (just a caret in a link)
      nsCOMPtr<nsIDOMNode> anchorNode;
      res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
      if (NS_FAILED(res)) return res;
      PRInt32 anchorOffset = -1;
      if (anchorNode)
        selection->GetAnchorOffset(&anchorOffset);
    
      nsCOMPtr<nsIDOMNode> focusNode;
      res = selection->GetFocusNode(getter_AddRefs(focusNode));
      if (NS_FAILED(res)) return res;
      PRInt32 focusOffset = -1;
      if (focusNode)
        selection->GetFocusOffset(&focusOffset);

      // Link node must be the same for both ends of selection
      if (NS_SUCCEEDED(res) && anchorNode)
      {
  #ifdef DEBUG_cmanske
        {
        nsAutoString name;
        anchorNode->GetNodeName(name);
        printf("GetSelectedElement: Anchor node of selection: ");
        wprintf(name.get());
        printf(" Offset: %d\n", anchorOffset);
        focusNode->GetNodeName(name);
        printf("Focus node of selection: ");
        wprintf(name.get());
        printf(" Offset: %d\n", focusOffset);
        }
  #endif
        nsCOMPtr<nsIDOMElement> parentLinkOfAnchor;
        res = GetElementOrParentByTagName(NS_LITERAL_STRING("href"), anchorNode, getter_AddRefs(parentLinkOfAnchor));
        // XXX: ERROR_HANDLING  can parentLinkOfAnchor be null?
        if (NS_SUCCEEDED(res) && parentLinkOfAnchor)
        {
          if (isCollapsed)
          {
            // We have just a caret in the link
            bNodeFound = PR_TRUE;
          } else if(focusNode) 
          {  // Link node must be the same for both ends of selection
            nsCOMPtr<nsIDOMElement> parentLinkOfFocus;
            res = GetElementOrParentByTagName(NS_LITERAL_STRING("href"), focusNode, getter_AddRefs(parentLinkOfFocus));
            if (NS_SUCCEEDED(res) && parentLinkOfFocus == parentLinkOfAnchor)
              bNodeFound = PR_TRUE;
          }
      
          // We found a link node parent
          if (bNodeFound) {
            // GetElementOrParentByTagName addref'd this, so we don't need to do it here
            *aReturn = parentLinkOfAnchor;
            NS_IF_ADDREF(*aReturn);
            return NS_OK;
          }
        }
        else if (anchorOffset >= 0)  // Check if link node is the only thing selected
        {
          nsCOMPtr<nsIDOMNode> anchorChild;
          anchorChild = GetChildAt(anchorNode,anchorOffset);
          if (anchorChild && nsHTMLEditUtils::IsLink(anchorChild) && 
              (anchorNode == focusNode) && focusOffset == (anchorOffset+1))
          {
            selectedElement = do_QueryInterface(anchorChild);
            bNodeFound = PR_TRUE;
          }
        }
      }
    } 

    if (!isCollapsed)   // Don't bother to examine selection if it is collapsed
    {
      nsCOMPtr<nsIEnumerator> enumerator;
      res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
      if (NS_SUCCEEDED(res))
      {
        if(!enumerator)
          return NS_ERROR_NULL_POINTER;

        enumerator->First(); 
        nsCOMPtr<nsISupports> currentItem;
        res = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if ((NS_SUCCEEDED(res)) && currentItem)
        {
          nsCOMPtr<nsIDOMRange> currange( do_QueryInterface(currentItem) );
          nsCOMPtr<nsIContentIterator> iter =
            do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
          if (NS_FAILED(res)) return res;

          iter->Init(currange);
          // loop through the content iterator for each content node
          while (!iter->IsDone())
          {
            // Query interface to cast nsIContent to nsIDOMNode
            //  then get tagType to compare to  aTagName
            // Clone node of each desired type and append it to the aDomFrag
            selectedElement = do_QueryInterface(iter->GetCurrentNode());
            if (selectedElement)
            {
              // If we already found a node, then we have another element,
              //  thus there's not just one element selected
              if (bNodeFound)
              {
                bNodeFound = PR_FALSE;
                break;
              }

              selectedElement->GetNodeName(domTagName);
              ToLowerCase(domTagName);

              if (anyTag)
              {
                // Get name of first selected element
                selectedElement->GetTagName(TagName);
                ToLowerCase(TagName);
                anyTag = PR_FALSE;
              }

              // The "A" tag is a pain,
              //  used for both link(href is set) and "Named Anchor"
              nsCOMPtr<nsIDOMNode> selectedNode = do_QueryInterface(selectedElement);
              if ( (isLinkTag && nsHTMLEditUtils::IsLink(selectedNode)) ||
                   (isNamedAnchorTag && nsHTMLEditUtils::IsNamedAnchor(selectedNode)) )
              {
                bNodeFound = PR_TRUE;
              } else if (TagName == domTagName) { // All other tag names are handled here
                bNodeFound = PR_TRUE;
              }
              if (!bNodeFound)
              {
                // Check if node we have is really part of the selection???
                break;
              }
            }
            iter->Next();
          }
        } else {
          // Should never get here?
          isCollapsed = PR_TRUE;
          printf("isCollapsed was FALSE, but no elements found in selection\n");
        }
      } else {
        printf("Could not create enumerator for GetSelectionProperties\n");
      }
    }
  }
  if (bNodeFound)
  {
    
    *aReturn = selectedElement;
    if (selectedElement)
    {  
      // Getters must addref
      NS_ADDREF(*aReturn);
    }
  } 
  else res = NS_EDITOR_ELEMENT_NOT_FOUND;

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::CreateElementWithDefaults(const nsAString& aTagName, nsIDOMElement** aReturn)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (aReturn)
    *aReturn = nsnull;

  if (aTagName.IsEmpty() || !aReturn)
//  if (!aTagName || !aReturn)
    return NS_ERROR_NULL_POINTER;
    
  nsAutoString TagName(aTagName);
  ToLowerCase(TagName);
  nsAutoString realTagName;

  if (IsLinkTag(TagName) || IsNamedAnchorTag(TagName))
  {
    realTagName.AssignLiteral("a");
  } else {
    realTagName = TagName;
  }
  //We don't use editor's CreateElement because we don't want to 
  //  go through the transaction system

  nsCOMPtr<nsIDOMElement>newElement;
  nsCOMPtr<nsIContent> newContent;
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(realTagName, getter_AddRefs(newContent));
  newElement = do_QueryInterface(newContent);
  if (NS_FAILED(res) || !newElement)
    return NS_ERROR_FAILURE;

  // Mark the new element dirty, so it will be formatted
  newElement->SetAttribute(NS_LITERAL_STRING("_moz_dirty"), EmptyString());

  // Set default values for new elements
  if (TagName.EqualsLiteral("hr"))
  {
    // Note that we read the user's attributes for these from prefs (in InsertHLine JS)
    res = SetAttributeOrEquivalent(newElement, NS_LITERAL_STRING("width"),
                                   NS_LITERAL_STRING("100%"), PR_TRUE);
    if (NS_FAILED(res)) return res;
    res = SetAttributeOrEquivalent(newElement, NS_LITERAL_STRING("size"),
                                   NS_LITERAL_STRING("2"), PR_TRUE);
  } else if (TagName.EqualsLiteral("table"))
  {
    res = newElement->SetAttribute(NS_LITERAL_STRING("cellpadding"),NS_LITERAL_STRING("2"));
    if (NS_FAILED(res)) return res;
    res = newElement->SetAttribute(NS_LITERAL_STRING("cellspacing"),NS_LITERAL_STRING("2"));
    if (NS_FAILED(res)) return res;
    res = newElement->SetAttribute(NS_LITERAL_STRING("border"),NS_LITERAL_STRING("1"));
  } else if (TagName.EqualsLiteral("td"))
  {
    res = SetAttributeOrEquivalent(newElement, NS_LITERAL_STRING("valign"),
                                   NS_LITERAL_STRING("top"), PR_TRUE);
  }
  // ADD OTHER TAGS HERE

  if (NS_SUCCEEDED(res))
  {
    *aReturn = newElement;
    // Getters must addref
    NS_ADDREF(*aReturn);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  nsresult res=NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection;

  if (!aAnchorElement) return NS_ERROR_NULL_POINTER; 


  // We must have a real selection
  res = GetSelection(getter_AddRefs(selection));
  if (!selection)
  {
    res = NS_ERROR_NULL_POINTER;
  }
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res))
    isCollapsed = PR_TRUE;
  
  if (isCollapsed)
  {
    printf("InsertLinkAroundSelection called but there is no selection!!!\n");     
    res = NS_OK;
  } else {
    // Be sure we were given an anchor element
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aAnchorElement);
    if (anchor)
    {
      nsAutoString href;
      res = anchor->GetHref(href);
      if (NS_FAILED(res)) return res;
      if (!href.IsEmpty())      
      {
        nsAutoEditBatch beginBatching(this);

        // Set all attributes found on the supplied anchor element
        nsCOMPtr<nsIDOMNamedNodeMap> attrMap;
        aAnchorElement->GetAttributes(getter_AddRefs(attrMap));
        if (!attrMap)
          return NS_ERROR_FAILURE;

        PRUint32 count, i;
        attrMap->GetLength(&count);
        nsAutoString name, value;

        for (i = 0; i < count; i++)
        {
          nsCOMPtr<nsIDOMNode> attrNode;
          res = attrMap->Item(i, getter_AddRefs(attrNode));
          if (NS_FAILED(res)) return res;

          if (attrNode)
          {
            nsCOMPtr<nsIDOMAttr> attribute = do_QueryInterface(attrNode);
            if (attribute)
            {
              // We must clear the string buffers
              //   because GetName, GetValue appends to previous string!
              name.Truncate();
              value.Truncate();

              res = attribute->GetName(name);
              if (NS_FAILED(res)) return res;

              res = attribute->GetValue(value);
              if (NS_FAILED(res)) return res;

              res = SetInlineProperty(nsEditProperty::a, name, value);
              if (NS_FAILED(res)) return res;
            }
          }
        }
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetHTMLBackgroundColor(const nsAString& aColor)
{
  NS_PRECONDITION(mDocWeak, "Missing Editor DOM Document");
  
  // Find a selected or enclosing table element to set background on
  nsCOMPtr<nsIDOMElement> element;
  PRInt32 selectedCount;
  nsAutoString tagName;
  nsresult res = GetSelectedOrParentTableElement(tagName, &selectedCount,
                                                 getter_AddRefs(element));
  if (NS_FAILED(res)) return res;

  PRBool setColor = !aColor.IsEmpty();

  NS_NAMED_LITERAL_STRING(bgcolor, "bgcolor");
  if (element)
  {
    if (selectedCount > 0)
    {
      // Traverse all selected cells
      nsCOMPtr<nsIDOMElement> cell;
      res = GetFirstSelectedCell(nsnull, getter_AddRefs(cell));
      if (NS_SUCCEEDED(res) && cell)
      {
        while(cell)
        {
          if (setColor)
            res = SetAttribute(cell, bgcolor, aColor);
          else
            res = RemoveAttribute(cell, bgcolor);
          if (NS_FAILED(res)) break;

          GetNextSelectedCell(nsnull, getter_AddRefs(cell));
        };
        return res;
      }
    }
    // If we failed to find a cell, fall through to use originally-found element
  } else {
    // No table element -- set the background color on the body tag
    res = nsEditor::GetRootElement(getter_AddRefs(element));
    if (NS_FAILED(res)) return res;
    if (!element)       return NS_ERROR_NULL_POINTER;
  }
  // Use the editor method that goes through the transaction system
  if (setColor)
    res = SetAttribute(element, bgcolor, aColor);
  else
    res = RemoveAttribute(element, bgcolor);

  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetBodyAttribute(const nsAString& aAttribute, const nsAString& aValue)
{
  nsresult res;
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  NS_ASSERTION(mDocWeak, "Missing Editor DOM Document");
  
  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;

  res = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (!bodyElement) res = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(res))
  {
    // Use the editor method that goes through the transaction system
    res = SetAttribute(bodyElement, aAttribute, aValue);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetLinkedObjects(nsISupportsArray** aNodeList)
{
  if (!aNodeList)
    return NS_ERROR_NULL_POINTER;

  nsresult res;

  res = NS_NewISupportsArray(aNodeList);
  if (NS_FAILED(res)) return res;
  if (!*aNodeList) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContentIterator> iter =
       do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  if (!iter) return NS_ERROR_NULL_POINTER;
  if ((NS_SUCCEEDED(res)))
  {
    nsCOMPtr<nsIDOMDocument> domdoc;
    nsEditor::GetDocument(getter_AddRefs(domdoc));
    if (!domdoc)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDocument> doc (do_QueryInterface(domdoc));
    if (!doc)
      return NS_ERROR_UNEXPECTED;

    iter->Init(doc->GetRootContent());

    // loop through the content iterator for each content node
    while (!iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode> node (do_QueryInterface(iter->GetCurrentNode()));
      if (node)
      {
        // Let nsURIRefObject make the hard decisions:
        nsCOMPtr<nsIURIRefObject> refObject;
        res = NS_NewHTMLURIRefObject(getter_AddRefs(refObject), node);
        if (NS_SUCCEEDED(res))
        {
          nsCOMPtr<nsISupports> isupp (do_QueryInterface(refObject));

          (*aNodeList)->AppendElement(isupp);
        }
      }
      iter->Next();
    }
  }

  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorStyleSheets methods 
#pragma mark -
#endif

NS_IMETHODIMP
nsHTMLEditor::AddStyleSheet(const nsAString &aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL))
    return NS_OK;

  // Lose the previously-loaded sheet so there's nothing to replace
  // This pattern is different from Override methods because
  //  we must wait to remove mLastStyleSheetURL and add new sheet
  //  at the same time (in StyleSheetLoaded callback) so they are undoable together
  mLastStyleSheetURL.Truncate();
  return ReplaceStyleSheet(aURL);
}

NS_IMETHODIMP
nsHTMLEditor::ReplaceStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL))
  {
    // Disable last sheet if not the same as new one
    if (!mLastStyleSheetURL.IsEmpty() && mLastStyleSheetURL.Equals(aURL))
        return EnableStyleSheet(mLastStyleSheetURL, PR_FALSE);

    return NS_OK;
  }

  nsCOMPtr<nsICSSLoader> cssLoader;
  nsresult rv = GetCSSLoader(aURL, getter_AddRefs(cssLoader));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> document;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  rv = ps->GetDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);;
  if (!document)     return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIURI> uaURI;
  rv = NS_NewURI(getter_AddRefs(uaURI), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICSSStyleSheet> sheet;
  rv = cssLoader->LoadAgentSheet(uaURI, this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RemoveStyleSheet(const nsAString &aURL)
{
  nsCOMPtr<nsICSSStyleSheet> sheet;
  nsresult rv = GetStyleSheetForURL(aURL, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!sheet)
    return NS_ERROR_UNEXPECTED;

  RemoveStyleSheetTxn* txn;
  rv = CreateTxnForRemoveStyleSheet(sheet, &txn);
  if (!txn) rv = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(rv))
  {
    rv = DoTransaction(txn);
    if (NS_SUCCEEDED(rv))
      mLastStyleSheetURL.Truncate();        // forget it

    // Remove it from our internal list
    rv = RemoveStyleSheetFromList(aURL);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // The transaction system (if any) has taken ownwership of txns
  NS_IF_RELEASE(txn);
  
  return rv;
}


NS_IMETHODIMP 
nsHTMLEditor::AddOverrideStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL))
    return NS_OK;

  nsCOMPtr<nsICSSLoader> cssLoader;
  nsresult rv = GetCSSLoader(aURL, getter_AddRefs(cssLoader));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uaURI;
  rv = NS_NewURI(getter_AddRefs(uaURI), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  // We MUST ONLY load synchronous local files (no @import)
  nsCOMPtr<nsICSSStyleSheet> sheet;
  rv = cssLoader->LoadAgentSheet(uaURI, getter_AddRefs(sheet));

  // Synchronous loads should ALWAYS return completed
  if (!sheet)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps)
    return NS_ERROR_NOT_INITIALIZED;

  // Add the override style sheet
  // (This checks if already exists)
  ps->AddOverrideStyleSheet(sheet);

  // Save doc pointer to be able to use nsIStyleSheet::SetEnabled()
  nsIDocument *document = ps->GetDocument();
  if (!document)
    return NS_ERROR_NULL_POINTER;
  sheet->SetOwningDocument(document);

  ps->ReconstructStyleData();

  // Save as the last-loaded sheet
  mLastOverrideStyleSheetURL = aURL;

  //Add URL and style sheet to our lists
  return AddNewStyleSheetToList(aURL, sheet);
}

NS_IMETHODIMP
nsHTMLEditor::ReplaceOverrideStyleSheet(const nsAString& aURL)
{
  // Enable existing sheet if already loaded.
  if (EnableExistingStyleSheet(aURL))
  {
    // Disable last sheet if not the same as new one
    if (!mLastOverrideStyleSheetURL.IsEmpty() && !mLastOverrideStyleSheetURL.Equals(aURL))
      return EnableStyleSheet(mLastOverrideStyleSheetURL, PR_FALSE);

    return NS_OK;
  }
  // Remove the previous sheet
  if (!mLastOverrideStyleSheetURL.IsEmpty())
    RemoveOverrideStyleSheet(mLastOverrideStyleSheetURL);

  return AddOverrideStyleSheet(aURL);
}

// Do NOT use transaction system for override style sheets
NS_IMETHODIMP
nsHTMLEditor::RemoveOverrideStyleSheet(const nsAString &aURL)
{
  nsCOMPtr<nsICSSStyleSheet> sheet;
  nsresult rv = GetStyleSheetForURL(aURL, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!sheet)
    return NS_OK; /// Don't fail if sheet not found

  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  ps->RemoveOverrideStyleSheet(sheet);
  ps->ReconstructStyleData();

  // Remove it from our internal list
  return RemoveStyleSheetFromList(aURL);
}

NS_IMETHODIMP
nsHTMLEditor::EnableStyleSheet(const nsAString &aURL, PRBool aEnable)
{
  nsCOMPtr<nsICSSStyleSheet> sheet;
  nsresult rv = GetStyleSheetForURL(aURL, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!sheet)
    return NS_OK; // Don't fail if sheet not found

  nsCOMPtr<nsIStyleSheet> nsISheet = do_QueryInterface(sheet);
  return nsISheet->SetEnabled(aEnable);
}


PRBool
nsHTMLEditor::EnableExistingStyleSheet(const nsAString &aURL)
{
  nsCOMPtr<nsICSSStyleSheet> sheet;
  nsresult rv = GetStyleSheetForURL(aURL, getter_AddRefs(sheet));
  NS_ENSURE_SUCCESS(rv, rv);

  // Enable sheet if already loaded.
  if (sheet)
  {
    nsCOMPtr<nsIStyleSheet> nsISheet = do_QueryInterface(sheet);
    nsISheet->SetEnabled(PR_TRUE);
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsHTMLEditor::AddNewStyleSheetToList(const nsAString &aURL,
                                     nsICSSStyleSheet *aStyleSheet)
{
  PRInt32 countSS = mStyleSheets.Count();
  PRInt32 countU = mStyleSheetURLs.Count();

  if (countU < 0 || countSS != countU)
    return NS_ERROR_UNEXPECTED;

  if (!mStyleSheetURLs.AppendString(aURL))
    return NS_ERROR_UNEXPECTED;

  return mStyleSheets.AppendObject(aStyleSheet) ? NS_OK : NS_ERROR_UNEXPECTED;
}

nsresult
nsHTMLEditor::RemoveStyleSheetFromList(const nsAString &aURL)
{
  // is it already in the list?
  PRInt32 foundIndex;
  foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex < 0)
    return NS_ERROR_FAILURE;

  // Attempt both removals; if one fails there's not much we can do.
  nsresult rv = NS_OK;
  if (!mStyleSheets.RemoveObjectAt(foundIndex))
    rv = NS_ERROR_FAILURE;
  if (!mStyleSheetURLs.RemoveStringAt(foundIndex))
    rv = NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP
nsHTMLEditor::GetStyleSheetForURL(const nsAString &aURL,
                                  nsICSSStyleSheet **aStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aStyleSheet);
  *aStyleSheet = 0;

  // is it already in the list?
  PRInt32 foundIndex;
  foundIndex = mStyleSheetURLs.IndexOf(aURL);
  if (foundIndex < 0)
    return NS_OK; //No sheet -- don't fail!

  *aStyleSheet = mStyleSheets[foundIndex];
  if (!*aStyleSheet)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetURLForStyleSheet(nsICSSStyleSheet *aStyleSheet,
                                  nsAString &aURL)
{
  // is it already in the list?
  PRInt32 foundIndex = mStyleSheets.IndexOf(aStyleSheet);

  // Don't fail if we don't find it in our list
  if (foundIndex == -1)
    return NS_OK;

  // Found it in the list!
  nsAString* strp = mStyleSheetURLs.StringAt(foundIndex);
  if (!strp)
    return NS_ERROR_UNEXPECTED;
  aURL = *strp;
  return NS_OK;
}

nsresult
nsHTMLEditor::GetCSSLoader(const nsAString& aURL, nsICSSLoader** aCSSLoader)
{
  if (!aCSSLoader)
    return NS_ERROR_NULL_POINTER;
  *aCSSLoader = 0;

  nsresult rv;

  nsCOMPtr<nsIDocument> document;

  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  rv = ps->GetDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);;
  if (!document)     return NS_ERROR_NULL_POINTER;

  NS_IF_ADDREF(*aCSSLoader = document->GetCSSLoader());
  if (!*aCSSLoader) {
    return NS_ERROR_NULL_POINTER;
  }

  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorMailSupport methods 
#pragma mark -
#endif

NS_IMETHODIMP
nsHTMLEditor::GetEmbeddedObjects(nsISupportsArray** aNodeList)
{
  if (!aNodeList)
    return NS_ERROR_NULL_POINTER;

  nsresult res;

  res = NS_NewISupportsArray(aNodeList);
  if (NS_FAILED(res)) return res;
  if (!*aNodeList) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContentIterator> iter =
      do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  if (!iter) return NS_ERROR_NULL_POINTER;
  if ((NS_SUCCEEDED(res)))
  {
    nsCOMPtr<nsIDOMDocument> domdoc;
    nsEditor::GetDocument(getter_AddRefs(domdoc));
    if (!domdoc)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDocument> doc (do_QueryInterface(domdoc));
    if (!doc)
      return NS_ERROR_UNEXPECTED;

    iter->Init(doc->GetRootContent());

    // loop through the content iterator for each content node
    while (!iter->IsDone())
    {
      nsIContent *content = iter->GetCurrentNode();
      nsCOMPtr<nsIDOMNode> node (do_QueryInterface(content));
      if (node)
      {
        nsAutoString tagName;
        node->GetNodeName(tagName);
        ToLowerCase(tagName);

        // See if it's an image or an embed
        if (tagName.EqualsLiteral("img") || tagName.EqualsLiteral("embed"))
          (*aNodeList)->AppendElement(node);
        else if (tagName.EqualsLiteral("a"))
        {
          // Only include links if they're links to file: URLs
          nsCOMPtr<nsIDOMHTMLAnchorElement> anchor (do_QueryInterface(content));
          if (anchor)
          {
            nsAutoString href;
            if (NS_SUCCEEDED(anchor->GetHref(href)))
              if (StringBeginsWith(href, NS_LITERAL_STRING("file:"),
                                   nsCaseInsensitiveStringComparator()))
                (*aNodeList)->AppendElement(node);
          }
        }
        else if (tagName.EqualsLiteral("body"))
        {
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
          if (element)
          {
            PRBool hasBackground = PR_FALSE;
            if (NS_SUCCEEDED(element->HasAttribute(NS_LITERAL_STRING("background"), &hasBackground)) && hasBackground)
              (*aNodeList)->AppendElement(node);
          }
        }
      }
      iter->Next();
    }
  }

  return res;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditor overrides 
#pragma mark -
#endif

NS_IMETHODIMP nsHTMLEditor::DeleteNode(nsIDOMNode * aNode)
{
  nsCOMPtr<nsIDOMNode> selectAllNode = FindUserSelectAllNode(aNode);
  
  if (selectAllNode)
  {
    return nsEditor::DeleteNode(selectAllNode);
  }
  return nsEditor::DeleteNode(aNode);
}

NS_IMETHODIMP nsHTMLEditor::DeleteText(nsIDOMCharacterData *aTextNode,
                                       PRUint32             aOffset,
                                       PRUint32             aLength)
{
  nsCOMPtr<nsIDOMNode> selectAllNode = FindUserSelectAllNode(aTextNode);
  
  if (selectAllNode)
  {
    return nsEditor::DeleteNode(selectAllNode);
  }
  return nsEditor::DeleteText(aTextNode, aOffset, aLength);
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  support utils
#pragma mark -
#endif

/* This routine examines aNode and it's ancestors looking for any node which has the
   -moz-user-select: all style lit.  Return the highest such ancestor.  */
nsCOMPtr<nsIDOMNode> nsHTMLEditor::FindUserSelectAllNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> resultNode;  // starts out empty
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMElement>root;
  GetRootElement(getter_AddRefs(root));
  if (!nsEditorUtils::IsDescendantOf(aNode, root))
    return nsnull;

  // retrieve the computed style of -moz-user-select for aNode
  nsAutoString mozUserSelectValue;
  while (node)
  {
    mHTMLCSSUtils->GetComputedProperty(node, nsEditProperty::cssMozUserSelect, mozUserSelectValue);
    if (mozUserSelectValue.EqualsLiteral("all"))
    {
      resultNode = node;
    }
    if (node != root)
    {
      nsCOMPtr<nsIDOMNode> tmp;
      node->GetParentNode(getter_AddRefs(tmp));
      node = tmp;
    }
    else
    {
      node = nsnull;
    }
  } 

  return resultNode;
}

static nsresult SetSelectionAroundHeadChildren(nsCOMPtr<nsISelection> aSelection, nsWeakPtr aDocWeak)
{
  nsresult res = NS_OK;
  // Set selection around <head> node
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(aDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMNodeList>nodeList; 
  res = doc->GetElementsByTagName(NS_LITERAL_STRING("head"), getter_AddRefs(nodeList));
  if (NS_FAILED(res)) return res;
  if (!nodeList) return NS_ERROR_NULL_POINTER;

  PRUint32 count; 
  nodeList->GetLength(&count);
  if (count < 1) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> headNode;
  res = nodeList->Item(0, getter_AddRefs(headNode)); 
  if (NS_FAILED(res)) return res;
  if (!headNode) return NS_ERROR_NULL_POINTER;

  // Collapse selection to before first child of the head,
  res = aSelection->Collapse(headNode, 0);
  if (NS_FAILED(res)) return res;

  //  then extend it to just after
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = headNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (!childNodes) return NS_ERROR_NULL_POINTER;
  PRUint32 childCount;
  childNodes->GetLength(&childCount);

  return aSelection->Extend(headNode, childCount+1);
}

NS_IMETHODIMP
nsHTMLEditor::GetHeadContentsAsHTML(nsAString& aOutputString)
{
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // Save current selection
  nsAutoSelectionReset selectionResetter(selection, this);

  res = SetSelectionAroundHeadChildren(selection, mDocWeak);
  if (NS_FAILED(res)) return res;

  res = OutputToString(NS_LITERAL_STRING("text/html"),
                       nsIDocumentEncoder::OutputSelectionOnly,
                       aOutputString);
  if (NS_SUCCEEDED(res))
  {
    // Selection always includes <body></body>,
    //  so terminate there
    nsReadingIterator<PRUnichar> findIter,endFindIter;
    aOutputString.BeginReading(findIter);
    aOutputString.EndReading(endFindIter);
    //counting on our parser to always lower case!!!
    if (CaseInsensitiveFindInReadable(NS_LITERAL_STRING("<body"),
                                      findIter, endFindIter))
    {
      nsReadingIterator<PRUnichar> beginIter;
      aOutputString.BeginReading(beginIter);
      PRInt32 offset = Distance(beginIter, findIter);//get the distance

      nsWritingIterator<PRUnichar> writeIter;
      aOutputString.BeginWriting(writeIter);
      // Ensure the string ends in a newline
      PRUnichar newline ('\n');
      findIter.advance(-1);
      if (offset ==0 || (offset >0 &&  (*findIter) != newline)) //check for 0
      {
        writeIter.advance(offset);
        *writeIter = newline;
        aOutputString.Truncate(offset+1);
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
#ifdef DEBUG
  if (!outNumTests || !outNumTestsFailed)
    return NS_ERROR_NULL_POINTER;

  TextEditorTest *tester = new TextEditorTest();
  if (!tester)
    return NS_ERROR_OUT_OF_MEMORY;
   
  tester->Run(this, outNumTests, outNumTestsFailed);
  delete tester;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorIMESupport overrides 
#pragma mark -
#endif

NS_IMETHODIMP
nsHTMLEditor::SetCompositionString(const nsAString& aCompositionString, nsIPrivateTextRangeList* aTextRangeList,nsTextEventReply* aReply)
{
  NS_ASSERTION(aTextRangeList, "null ptr");
  if (nsnull == aTextRangeList)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsICaret>  caretP;
  
  // workaround for windows ime bug 23558: we get every ime event twice. 
  // for escape keypress, this causes an empty string to be passed
  // twice, which freaks out the editor.  This is to detect and aviod that
  // situation:
  if (aCompositionString.IsEmpty() && !mIMETextNode) 
  {
    return NS_OK;
  }
  
  nsCOMPtr<nsISelection> selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;

  mIMETextRangeList = aTextRangeList;

  if (!mPresShellWeak)  
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) 
    return NS_ERROR_NOT_INITIALIZED;

  // XXX_kin: BEGIN HACK! HACK! HACK!
  // XXX_kin:
  // XXX_kin: This is lame! The IME stuff needs caret coordinates
  // XXX_kin: synchronously, but the editor could be using async
  // XXX_kin: updates (reflows and paints) for performance reasons.
  // XXX_kin: In order to give IME what it needs, we have to temporarily
  // XXX_kin: switch to sync updating during this call so that the
  // XXX_kin: nsAutoPlaceHolderBatch can force sync reflows, paints,
  // XXX_kin: and selection scrolling, so that we get back accurate
  // XXX_kin: caret coordinates.

  PRUint32 flags = 0;
  PRBool restoreFlags = PR_FALSE;

  if (NS_SUCCEEDED(GetFlags(&flags)) &&
     (flags & nsIPlaintextEditor::eEditorUseAsyncUpdatesMask))
  {
    if (NS_SUCCEEDED(SetFlags(flags & (~nsIPlaintextEditor::eEditorUseAsyncUpdatesMask))))
       restoreFlags = PR_TRUE;
  }

  // XXX_kin: END HACK! HACK! HACK!

  // we need the nsAutoPlaceHolderBatch destructor called before hitting
  // GetCaretCoordinates so the states in Frame system sync with content
  // therefore, we put the nsAutoPlaceHolderBatch into an inner block
  {
    nsAutoPlaceHolderBatch batch(this, gIMETxnName);

    result = InsertText(aCompositionString);

    mIMEBufferLength = aCompositionString.Length();

    ps->GetCaret(getter_AddRefs(caretP));
    if (caretP)
      caretP->SetCaretDOMSelection(selection);

    // second part of 23558 fix:
    if (aCompositionString.IsEmpty()) 
    {
      mIMETextNode = nsnull;
    }
  }

  // XXX_kin: BEGIN HACK! HACK! HACK!
  // XXX_kin:
  // XXX_kin: Restore the previous set of flags!

  if (restoreFlags)
    SetFlags(flags);

  // XXX_kin: END HACK! HACK! HACK!

  result = caretP->GetCaretCoordinates(nsICaret::eIMECoordinates, selection,
              &(aReply->mCursorPosition), &(aReply->mCursorIsCollapsed), nsnull);
  NS_ASSERTION(NS_SUCCEEDED(result), "cannot get caret position");

  
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::GetReconversionString(nsReconversionEventReply* aReply)
{
  nsresult res;

  nsCOMPtr<nsISelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
    return (res == NS_OK) ? NS_ERROR_FAILURE : res;

  // get the first range in the selection.  Since it is
  // unclear what to do if reconversion happens with a 
  // multirange selection, we will ignore any additional ranges.
  
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res) || !range)
    return (res == NS_OK) ? NS_ERROR_FAILURE : res;
  
  nsAutoString textValue;
  res = range->ToString(textValue);
  if (NS_FAILED(res))
    return res;
  
  aReply->mReconversionString = (PRUnichar*) nsMemory::Clone(textValue.get(),
                                                                (textValue.Length() + 1) * sizeof(PRUnichar));
  if (!aReply->mReconversionString)
    return NS_ERROR_OUT_OF_MEMORY;

  // delete the selection
  res = DeleteSelection(eNone);
  
  return res;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  StyleSheet utils 
#pragma mark -
#endif


NS_IMETHODIMP 
nsHTMLEditor::StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aNotify)
{
  nsresult rv = NS_OK;
  nsAutoEditBatch batchIt(this);

  if (!mLastStyleSheetURL.IsEmpty())
    RemoveStyleSheet(mLastStyleSheetURL);

  AddStyleSheetTxn* txn;
  rv = CreateTxnForAddStyleSheet(aSheet, &txn);
  if (!txn) rv = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(rv))
  {
    rv = DoTransaction(txn);
    if (NS_SUCCEEDED(rv))
    {
      // Get the URI, then url spec from the sheet
      nsCOMPtr<nsIStyleSheet> sheet = do_QueryInterface(aSheet);
      nsCOMPtr<nsIURI> uri;
      rv = sheet->GetURL(*getter_AddRefs(uri));
      if (NS_FAILED(rv))
        return rv;

      nsCAutoString spec;
      rv = uri->GetSpec(spec);
      if (NS_FAILED(rv))
        return rv;

      // Save it so we can remove before applying the next one
      mLastStyleSheetURL.AssignWithConversion(spec.get());

      // Also save in our arrays of urls and sheets
      AddNewStyleSheetToList(mLastStyleSheetURL, aSheet);
    }
  }
  // The transaction system (if any) has taken ownwership of txns
  NS_IF_RELEASE(txn);

  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsEditor overrides 
#pragma mark -
#endif


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsHTMLEditor::StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection)
{
  nsEditor::StartOperation(opID, aDirection);  // will set mAction, mDirection
  if (! ((mAction==kOpInsertText) || (mAction==kOpInsertIMEText)) )
    ClearInlineStylesCache();
  if (mRules) return mRules->BeforeEdit(mAction, mDirection);
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation */
NS_IMETHODIMP
nsHTMLEditor::EndOperation()
{
  // post processing
  if (! ((mAction==kOpInsertText) || (mAction==kOpInsertIMEText) || (mAction==kOpIgnore)) )
    ClearInlineStylesCache();
  nsresult res = NS_OK;
  if (mRules) res = mRules->AfterEdit(mAction, mDirection);
  nsEditor::EndOperation();  // will clear mAction, mDirection
  return res;
}  

PRBool 
nsHTMLEditor::TagCanContainTag(const nsAString& aParentTag, const nsAString& aChildTag)  
{
  // COtherDTD gives some unwanted results.  We override them here.
  if (aParentTag.LowerCaseEqualsLiteral("ol") ||
      aParentTag.LowerCaseEqualsLiteral("ul"))
  {
    // if parent is a list and tag is also a list, say "yes".
    // This is because the editor does sublists illegally for now. 
      if (aChildTag.LowerCaseEqualsLiteral("ol") ||
          aChildTag.LowerCaseEqualsLiteral("ul"))
      return PR_TRUE;
  }

  if (aParentTag.LowerCaseEqualsLiteral("li"))
  {
    // list items cant contain list items
    if (aChildTag.LowerCaseEqualsLiteral("li"))
      return PR_FALSE;
  }

/*  
  // if parent is a pre, and child is not inline, say "no"
  if ( aParentTag.EqualsLiteral("pre") )
  {
    if (aChildTag.EqualsLiteral("__moz_text"))
      return PR_TRUE;
    PRInt32 childTagEnum, parentTagEnum;
    nsAutoString non_const_childTag(aChildTag);
    nsAutoString non_const_parentTag(aParentTag);
    nsresult res = mDTD->StringTagToIntTag(non_const_childTag,&childTagEnum);
    if (NS_FAILED(res)) return PR_FALSE;
    res = mDTD->StringTagToIntTag(non_const_parentTag,&parentTagEnum);
    if (NS_FAILED(res)) return PR_FALSE;
    if (!mDTD->IsInlineElement(childTagEnum,parentTagEnum))
      return PR_FALSE;
  }
*/
  // else fall thru
  return nsEditor::TagCanContainTag(aParentTag, aChildTag);
}


NS_IMETHODIMP 
nsHTMLEditor::SelectEntireDocument(nsISelection *aSelection)
{
  nsresult res;
  if (!aSelection || !mRules) { return NS_ERROR_NULL_POINTER; }
  
  // get body node
  nsCOMPtr<nsIDOMElement>bodyElement;
  res = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
  if (!bodyNode) return NS_ERROR_FAILURE;
  
  // is doc empty?
  PRBool bDocIsEmpty;
  res = mRules->DocumentIsEmpty(&bDocIsEmpty);
  if (NS_FAILED(res)) return res;
    
  if (bDocIsEmpty)
  {
    // if its empty dont select entire doc - that would select the bogus node
    return aSelection->Collapse(bodyNode, 0);
  }
  else
  {
    return nsEditor::SelectEntireDocument(aSelection);
  }
  return res;
}



#ifdef XP_MAC
#pragma mark -
#pragma mark  Random methods 
#pragma mark -
#endif


NS_IMETHODIMP nsHTMLEditor::GetLayoutObject(nsIDOMNode *aNode, nsISupports **aLayoutObject)
{
  nsresult result = NS_ERROR_FAILURE;  // we return an error unless we get the index
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  if ((nsnull!=aNode))
  { // get the content interface
    nsCOMPtr<nsIContent> nodeAsContent( do_QueryInterface(aNode) );
    if (nodeAsContent)
    { // get the frame from the content interface
      //Note: frames are not ref counted, so don't use an nsCOMPtr
      *aLayoutObject = nsnull;
      result = ps->GetLayoutObjectFor(nodeAsContent, aLayoutObject);
    }
  }
  else {
    result = NS_ERROR_NULL_POINTER;
  }

  return result;
}


// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
void nsHTMLEditor::IsTextPropertySetByContent(nsIDOMNode        *aNode,
                                              nsIAtom           *aProperty, 
                                              const nsAString   *aAttribute, 
                                              const nsAString   *aValue, 
                                              PRBool            &aIsSet,
                                              nsIDOMNode       **aStyleNode,
                                              nsAString *outValue)
{
  nsresult result;
  aIsSet = PR_FALSE;  // must be initialized to false for code below to work
  nsAutoString propName;
  aProperty->ToString(propName);
  nsCOMPtr<nsIDOMNode>node = aNode;

  while (node)
  {
    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(node);
    if (element)
    {
      nsAutoString tag, value;
      element->GetTagName(tag);
      if (propName.Equals(tag, nsCaseInsensitiveStringComparator()))
      {
        PRBool found = PR_FALSE;
        if (aAttribute && 0!=aAttribute->Length())
        {
          element->GetAttribute(*aAttribute, value);
          if (outValue) *outValue = value;
          if (!value.IsEmpty())
          {
            if (!aValue) {
              found = PR_TRUE;
            }
            else
            {
              nsString tString(*aValue);
              if (tString.Equals(value, nsCaseInsensitiveStringComparator())) {
                found = PR_TRUE;
              }
              else {  // we found the prop with the attribute, but the value doesn't match
                break;
              }
            }
          }
        }
        else { 
          found = PR_TRUE;
        }
        if (found)
        {
          aIsSet = PR_TRUE;
          break;
        }
      }
    }
    nsCOMPtr<nsIDOMNode>temp;
    result = node->GetParentNode(getter_AddRefs(temp));
    if (NS_SUCCEEDED(result) && temp) {
      node = do_QueryInterface(temp);
    }
    else {
      node = do_QueryInterface(nsnull);
    }
  }
}

#ifdef XP_MAC
#pragma mark -
#endif

//================================================================
// HTML Editor methods
//
// Note: Table Editing methods are implemented in nsTableEditor.cpp
//


PRBool nsHTMLEditor::IsElementInBody(nsIDOMElement* aElement)
{
  return nsTextEditUtils::InBody(aElement, this);
}

PRBool
nsHTMLEditor::SetCaretInTableCell(nsIDOMElement* aElement)
{
  PRBool caretIsSet = PR_FALSE;

  if (aElement && IsElementInBody(aElement))
  {
    nsresult res = NS_OK;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    if (content)
    {
      nsIAtom *atom = content->Tag();
      if (atom == nsEditProperty::table ||
          atom == nsEditProperty::tbody ||
          atom == nsEditProperty::thead ||
          atom == nsEditProperty::tfoot ||
          atom == nsEditProperty::caption ||
          atom == nsEditProperty::tr ||
          atom == nsEditProperty::td )
      {
        nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
        nsCOMPtr<nsIDOMNode> parent;
        // This MUST succeed if IsElementInBody was TRUE
        node->GetParentNode(getter_AddRefs(parent));
        nsCOMPtr<nsIDOMNode>firstChild;
        // Find deepest child
        PRBool hasChild;
        while (NS_SUCCEEDED(node->HasChildNodes(&hasChild)) && hasChild)
        {
          if (NS_SUCCEEDED(node->GetFirstChild(getter_AddRefs(firstChild))))
          {
            parent = node;
            node = firstChild;
          }
        }
        // Set selection at beginning of deepest node
        nsCOMPtr<nsISelection> selection;
        res = GetSelection(getter_AddRefs(selection));
        if (NS_SUCCEEDED(res) && selection && firstChild)
        {
          res = selection->Collapse(firstChild, 0);
          if (NS_SUCCEEDED(res))
            caretIsSet = PR_TRUE;
        }
      }
    }
  }
  return caretIsSet;
}            



NS_IMETHODIMP
nsHTMLEditor::IsRootTag(nsString &aTag, PRBool &aIsTag)
{
  static char bodyTag[] = "body";
  static char tdTag[] = "td";
  static char thTag[] = "th";
  static char captionTag[] = "caption";
  if (aTag.EqualsIgnoreCase(bodyTag) ||
      aTag.EqualsIgnoreCase(tdTag) ||
      aTag.EqualsIgnoreCase(thTag) ||
      aTag.EqualsIgnoreCase(captionTag) )
  {
    aIsTag = PR_TRUE;
  }
  else {
    aIsTag = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::IsSubordinateBlock(nsString &aTag, PRBool &aIsTag)
{
  static char p[] = "p";
  static char h1[] = "h1";
  static char h2[] = "h2";
  static char h3[] = "h3";
  static char h4[] = "h4";
  static char h5[] = "h5";
  static char h6[] = "h6";
  static char address[] = "address";
  static char pre[] = "pre";
  static char li[] = "li";
  static char dt[] = "dt";
  static char dd[] = "dd";
  if (aTag.EqualsIgnoreCase(p)  ||
      aTag.EqualsIgnoreCase(h1) ||
      aTag.EqualsIgnoreCase(h2) ||
      aTag.EqualsIgnoreCase(h3) ||
      aTag.EqualsIgnoreCase(h4) ||
      aTag.EqualsIgnoreCase(h5) ||
      aTag.EqualsIgnoreCase(h6) ||
      aTag.EqualsIgnoreCase(address) ||
      aTag.EqualsIgnoreCase(pre) ||
      aTag.EqualsIgnoreCase(li) ||
      aTag.EqualsIgnoreCase(dt) ||
      aTag.EqualsIgnoreCase(dd) )
  {
    aIsTag = PR_TRUE;
  }
  else {
    aIsTag = PR_FALSE;
  }
  return NS_OK;
}



///////////////////////////////////////////////////////////////////////////
// GetEnclosingTable: find ancestor who is a table, if any
//                  
nsCOMPtr<nsIDOMNode> 
nsHTMLEditor::GetEnclosingTable(nsIDOMNode *aNode)
{
  NS_PRECONDITION(aNode, "null node passed to nsHTMLEditor::GetEnclosingTable");
  nsCOMPtr<nsIDOMNode> tbl, tmp, node = aNode;

  while (!tbl)
  {
    tmp = GetBlockNodeParent(node);
    if (!tmp) break;
    if (nsHTMLEditUtils::IsTable(tmp)) tbl = tmp;
    node = tmp;
  }
  return tbl;
}

#ifdef XP_MAC
#pragma mark -
#endif

void nsHTMLEditor::ClearInlineStylesCache()
{
  mCachedNode = nsnull;
}

#ifdef PRE_NODE_IN_BODY
nsCOMPtr<nsIDOMElement> nsHTMLEditor::FindPreElement()
{
  nsCOMPtr<nsIDOMDocument> domdoc;
  nsEditor::GetDocument(getter_AddRefs(domdoc));
  if (!domdoc)
    return 0;

  nsCOMPtr<nsIDocument> doc (do_QueryInterface(domdoc));
  if (!doc)
    return 0;

  nsCOMPtr<nsIContent> rootContent;
  doc->GetRootContent(getter_AddRefs(rootContent));
  if (!rootContent)
    return 0;

  nsCOMPtr<nsIDOMNode> rootNode (do_QueryInterface(rootContent));
  if (!rootNode)
    return 0;

  nsString prestr ("PRE");  // GetFirstNodeOfType requires capitals
  nsCOMPtr<nsIDOMNode> preNode;
  if (NS_FAILED(nsEditor::GetFirstNodeOfType(rootNode, prestr,
                                                 getter_AddRefs(preNode))))
    return 0;

  return do_QueryInterface(preNode);
}
#endif /* PRE_NODE_IN_BODY */

void nsHTMLEditor::HandleEventListenerError()
{
  // null out the nsCOMPtrs
  mKeyListenerP = nsnull;
  mMouseListenerP = nsnull;
  mTextListenerP = nsnull;
  mDragListenerP = nsnull;
  mCompositionListenerP = nsnull;
  mFocusListenerP = nsnull;
}

/* this method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * Uses nsEditor::JoinNodes so action is undoable. 
 * Should be called within the context of a batch transaction.
 */
NS_IMETHODIMP
nsHTMLEditor::CollapseAdjacentTextNodes(nsIDOMRange *aInRange)
{
  if (!aInRange) return NS_ERROR_NULL_POINTER;
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  nsVoidArray textNodes;  // we can't actually do anything during iteration, so store the text nodes in an array
                          // don't bother ref counting them because we know we can hold them for the 
                          // lifetime of this method


  // build a list of editable text nodes
  nsresult result;
  nsCOMPtr<nsIContentIterator> iter =
    do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &result);
  if (NS_FAILED(result)) return result;

  iter->Init(aInRange);

  while (!iter->IsDone())
  {
    nsIContent *content = iter->GetCurrentNode();  

    nsCOMPtr<nsIDOMCharacterData> text = do_QueryInterface(content);
    nsCOMPtr<nsIDOMNode>          node = do_QueryInterface(content);
    if (text && node && IsEditable(node))
    {
      textNodes.AppendElement(node.get());
    }

    iter->Next();
  }

  // now that I have a list of text nodes, collapse adjacent text nodes
  // NOTE: assumption that JoinNodes keeps the righthand node
  while (textNodes.Count() > 1)
  {
    // we assume a textNodes entry can't be nsnull
    nsIDOMNode *leftTextNode = (nsIDOMNode *)(textNodes.ElementAt(0));
    nsIDOMNode *rightTextNode = (nsIDOMNode *)(textNodes.ElementAt(1));
    NS_ASSERTION(leftTextNode && rightTextNode,"left or rightTextNode null in CollapseAdjacentTextNodes");

    // get the prev sibling of the right node, and see if it's leftTextNode
    nsCOMPtr<nsIDOMNode> prevSibOfRightNode;
    result = GetPriorHTMLSibling(rightTextNode, address_of(prevSibOfRightNode));
    if (NS_FAILED(result)) return result;
    if (prevSibOfRightNode && (prevSibOfRightNode == leftTextNode))
    {
      nsCOMPtr<nsIDOMNode> parent;
      result = rightTextNode->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(result)) return result;
      if (!parent) return NS_ERROR_NULL_POINTER;
      result = JoinNodes(leftTextNode, rightTextNode, parent);
      if (NS_FAILED(result)) return result;
    }

    textNodes.RemoveElementAt(0); // remove the leftmost text node from the list
  }

  return result;
}

NS_IMETHODIMP
nsHTMLEditor::GetNextElementByTagName(nsIDOMElement    *aCurrentElement,
                                      const nsAString   *aTagName,
                                      nsIDOMElement   **aReturn)
{
  nsresult res = NS_OK;
  if (!aCurrentElement || !aTagName || !aReturn)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIAtom> tagAtom = do_GetAtom(*aTagName);
  if (!tagAtom) { return NS_ERROR_NULL_POINTER; }
  if (tagAtom==nsEditProperty::th)
    tagAtom=nsEditProperty::td;

  nsCOMPtr<nsIDOMNode> currentNode = do_QueryInterface(aCurrentElement);
  if (!currentNode)
    return NS_ERROR_FAILURE;

  *aReturn = nsnull;

  nsCOMPtr<nsIDOMNode> nextNode;
  PRBool done = PR_FALSE;

  do {
    res = GetNextNode(currentNode, PR_TRUE, address_of(nextNode));
    if (NS_FAILED(res)) return res;
    if (!nextNode) break;

    if (GetTag(currentNode) == tagAtom)
    {
      nsCOMPtr<nsIDOMElement> element = do_QueryInterface(currentNode);
      if (!element) return NS_ERROR_NULL_POINTER;

      *aReturn = element;
      NS_ADDREF(*aReturn);
      done = PR_TRUE;
      return NS_OK;
    }
    currentNode = nextNode;
  } while (!done);

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SetSelectionAtDocumentStart(nsISelection *aSelection)
{
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = GetRootElement(getter_AddRefs(bodyElement));  
  if (NS_SUCCEEDED(res))
  {
    if (!bodyElement) return NS_ERROR_NULL_POINTER;
    res = aSelection->Collapse(bodyElement,0);
  }
  return res;
}

#ifdef XP_MAC
#pragma mark -
#endif

///////////////////////////////////////////////////////////////////////////
// RemoveBlockContainer: remove inNode, reparenting it's children into their
//                  the parent of inNode.  In addition, INSERT ANY BR's NEEDED
//                  TO PRESERVE IDENTITY OF REMOVED BLOCK.
//
nsresult
nsHTMLEditor::RemoveBlockContainer(nsIDOMNode *inNode)
{
  if (!inNode)
    return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> sibling, child, unused;
  
  // Two possibilities: the container cold be empty of editable content.
  // If that is the case, we need to compare what is before and after inNode
  // to determine if we need a br.
  // Or it could not be empty, in which case we have to compare previous
  // sibling and first child to determine if we need a leading br,
  // and compare following sibling and last child to determine if we need a
  // trailing br.
  
  res = GetFirstEditableChild(inNode, address_of(child));
  if (NS_FAILED(res)) return res;
  
  if (child)  // the case of inNode not being empty
  {
    // we need a br at start unless:
    // 1) previous sibling of inNode is a block, OR
    // 2) previous sibling of inNode is a br, OR
    // 3) first child of inNode is a block OR
    // 4) either is null
    
    res = GetPriorHTMLSibling(inNode, address_of(sibling));
    if (NS_FAILED(res)) return res;
    if (sibling && !IsBlockNode(sibling) && !nsTextEditUtils::IsBreak(sibling))
    {
      res = GetFirstEditableChild(inNode, address_of(child));
      if (NS_FAILED(res)) return res;
      if (child && !IsBlockNode(child))
      {
        // insert br node
        res = CreateBR(inNode, 0, address_of(unused));
        if (NS_FAILED(res)) return res;
      }
    }
    
    // we need a br at end unless:
    // 1) following sibling of inNode is a block, OR
    // 2) last child of inNode is a block, OR
    // 3) last child of inNode is a block OR
    // 4) either is null

    res = GetNextHTMLSibling(inNode, address_of(sibling));
    if (NS_FAILED(res)) return res;
    if (sibling && !IsBlockNode(sibling))
    {
      res = GetLastEditableChild(inNode, address_of(child));
      if (NS_FAILED(res)) return res;
      if (child && !IsBlockNode(child) && !nsTextEditUtils::IsBreak(child))
      {
        // insert br node
        PRUint32 len;
        res = GetLengthOfDOMNode(inNode, len);
        if (NS_FAILED(res)) return res;
        res = CreateBR(inNode, (PRInt32)len, address_of(unused));
        if (NS_FAILED(res)) return res;
      }
    }
  }
  else  // the case of inNode being empty
  {
    // we need a br at start unless:
    // 1) previous sibling of inNode is a block, OR
    // 2) previous sibling of inNode is a br, OR
    // 3) following sibling of inNode is a block, OR
    // 4) following sibling of inNode is a br OR
    // 5) either is null
    res = GetPriorHTMLSibling(inNode, address_of(sibling));
    if (NS_FAILED(res)) return res;
    if (sibling && !IsBlockNode(sibling) && !nsTextEditUtils::IsBreak(sibling))
    {
      res = GetNextHTMLSibling(inNode, address_of(sibling));
      if (NS_FAILED(res)) return res;
      if (sibling && !IsBlockNode(sibling) && !nsTextEditUtils::IsBreak(sibling))
      {
        // insert br node
        res = CreateBR(inNode, 0, address_of(unused));
        if (NS_FAILED(res)) return res;
      }
    }
  }
    
  // now remove container
  return RemoveContainer(inNode);
}


///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLSibling: returns the previous editable sibling, if there is
//                   one within the parent
//                       
nsresult
nsHTMLEditor::GetPriorHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode || !inNode) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  *outNode = nsnull;
  nsCOMPtr<nsIDOMNode> temp, node = do_QueryInterface(inNode);
  
  while (1)
  {
    res = node->GetPreviousSibling(getter_AddRefs(temp));
    if (NS_FAILED(res)) return res;
    if (!temp) return NS_OK;  // return null sibling
    // if it's editable, we're done
    if (IsEditable(temp)) break;
    // otherwise try again
    node = temp;
  }
  *outNode = temp;
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLSibling: returns the previous editable sibling, if there is
//                   one within the parent.  just like above routine but
//                   takes a parent/offset instead of a node.
//                       
nsresult
nsHTMLEditor::GetPriorHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode || !inParent) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  *outNode = nsnull;
  if (!inOffset) return NS_OK;  // return null sibling if at offset zero
  nsCOMPtr<nsIDOMNode> node = nsEditor::GetChildAt(inParent,inOffset-1);
  if (IsEditable(node)) 
  {
    *outNode = node;
    return res;
  }
  // else
  return GetPriorHTMLSibling(node, outNode);
}



///////////////////////////////////////////////////////////////////////////
// GetNextHTMLSibling: returns the next editable sibling, if there is
//                   one within the parent
//                       
nsresult
nsHTMLEditor::GetNextHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  *outNode = nsnull;
  nsCOMPtr<nsIDOMNode> temp, node = do_QueryInterface(inNode);
  
  while (1)
  {
    res = node->GetNextSibling(getter_AddRefs(temp));
    if (NS_FAILED(res)) return res;
    if (!temp) return NS_OK;  // return null sibling
    // if it's editable, we're done
    if (IsEditable(temp)) break;
    // otherwise try again
    node = temp;
  }
  *outNode = temp;
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetNextHTMLSibling: returns the next editable sibling, if there is
//                   one within the parent.  just like above routine but
//                   takes a parent/offset instead of a node.
//                       
nsresult
nsHTMLEditor::GetNextHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode || !inParent) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  *outNode = nsnull;
  nsCOMPtr<nsIDOMNode> node = nsEditor::GetChildAt(inParent,inOffset);
  if (!node) return NS_OK; // return null sibling if no sibling
  if (IsEditable(node)) 
  {
    *outNode = node;
    return res;
  }
  // else
  return GetPriorHTMLSibling(node, outNode);
}



///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLNode: returns the previous editable leaf node, if there is
//                   one within the <body>
//                       
nsresult
nsHTMLEditor::GetPriorHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, PRBool bNoBlockCrossing)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = GetPriorNode(inNode, PR_TRUE, address_of(*outNode), bNoBlockCrossing);
  if (NS_FAILED(res)) return res;
  
  // if it's not in the body, then zero it out
  if (*outNode && !nsTextEditUtils::InBody(*outNode, this))
  {
    *outNode = nsnull;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLNode: same as above but takes {parent,offset} instead of node
//                       
nsresult
nsHTMLEditor::GetPriorHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode, PRBool bNoBlockCrossing)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = GetPriorNode(inParent, inOffset, PR_TRUE, address_of(*outNode), bNoBlockCrossing);
  if (NS_FAILED(res)) return res;
  
  // if it's not in the body, then zero it out
  if (*outNode && !nsTextEditUtils::InBody(*outNode, this))
  {
    *outNode = nsnull;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetNextHTMLNode: returns the next editable leaf node, if there is
//                   one within the <body>
//                       
nsresult
nsHTMLEditor::GetNextHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode, PRBool bNoBlockCrossing)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = GetNextNode(inNode, PR_TRUE, address_of(*outNode), bNoBlockCrossing);
  if (NS_FAILED(res)) return res;
  
  // if it's not in the body, then zero it out
  if (*outNode && !nsTextEditUtils::InBody(*outNode, this))
  {
    *outNode = nsnull;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetNHTMLextNode: same as above but takes {parent,offset} instead of node
//                       
nsresult
nsHTMLEditor::GetNextHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode, PRBool bNoBlockCrossing)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = GetNextNode(inParent, inOffset, PR_TRUE, address_of(*outNode), bNoBlockCrossing);
  if (NS_FAILED(res)) return res;
  
  // if it's not in the body, then zero it out
  if (*outNode && !nsTextEditUtils::InBody(*outNode, this))
  {
    *outNode = nsnull;
  }
  return res;
}


nsresult 
nsHTMLEditor::IsFirstEditableChild( nsIDOMNode *aNode, PRBool *aOutIsFirst)
{
  // check parms
  if (!aOutIsFirst || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutIsFirst = PR_FALSE;
  
  // find first editable child and compare it to aNode
  nsCOMPtr<nsIDOMNode> parent, firstChild;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res)) return res;
  if (!parent) return NS_ERROR_FAILURE;
  res = GetFirstEditableChild(parent, address_of(firstChild));
  if (NS_FAILED(res)) return res;
  
  *aOutIsFirst = (firstChild.get() == aNode);
  return res;
}


nsresult 
nsHTMLEditor::IsLastEditableChild( nsIDOMNode *aNode, PRBool *aOutIsLast)
{
  // check parms
  if (!aOutIsLast || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutIsLast = PR_FALSE;
  
  // find last editable child and compare it to aNode
  nsCOMPtr<nsIDOMNode> parent, lastChild;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res)) return res;
  if (!parent) return NS_ERROR_FAILURE;
  res = GetLastEditableChild(parent, address_of(lastChild));
  if (NS_FAILED(res)) return res;
  
  *aOutIsLast = (lastChild.get() == aNode);
  return res;
}


nsresult 
nsHTMLEditor::GetFirstEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstChild)
{
  // check parms
  if (!aOutFirstChild || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutFirstChild = nsnull;
  
  // find first editable child
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = aNode->GetFirstChild(getter_AddRefs(child));
  if (NS_FAILED(res)) return res;
  
  while (child && !IsEditable(child))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = child->GetNextSibling(getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    if (!tmp) return NS_ERROR_FAILURE;
    child = tmp;
  }
  
  *aOutFirstChild = child;
  return res;
}


nsresult 
nsHTMLEditor::GetLastEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastChild)
{
  // check parms
  if (!aOutLastChild || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutLastChild = aNode;
  
  // find last editable child
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = aNode->GetLastChild(getter_AddRefs(child));
  if (NS_FAILED(res)) return res;
  
  while (child && !IsEditable(child))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = child->GetPreviousSibling(getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    if (!tmp) return NS_ERROR_FAILURE;
    child = tmp;
  }
  
  *aOutLastChild = child;
  return res;
}

nsresult 
nsHTMLEditor::GetFirstEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstLeaf)
{
  // check parms
  if (!aOutFirstLeaf || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutFirstLeaf = aNode;
  
  // find leftmost leaf
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = NS_OK;
  child = GetLeftmostChild(aNode);  
  while (child && (!IsEditable(child) || !nsEditorUtils::IsLeafNode(child)))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = GetNextHTMLNode(child, address_of(tmp));
    if (NS_FAILED(res)) return res;
    if (!tmp) return NS_ERROR_FAILURE;
    
    // only accept nodes that are descendants of aNode
    if (nsEditorUtils::IsDescendantOf(tmp, aNode))
      child = tmp;
    else
    {
      child = nsnull;  // this will abort the loop
    }
  }
  
  *aOutFirstLeaf = child;
  return res;
}


nsresult 
nsHTMLEditor::GetLastEditableLeaf( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastLeaf)
{
  // check parms
  if (!aOutLastLeaf || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutLastLeaf = nsnull;
  
  // find rightmost leaf
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = NS_OK;
  child = GetRightmostChild(aNode, PR_FALSE);  
  while (child && (!IsEditable(child) || !nsEditorUtils::IsLeafNode(child)))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = GetPriorHTMLNode(child, address_of(tmp));
    if (NS_FAILED(res)) return res;
    if (!tmp) return NS_ERROR_FAILURE;
    
    // only accept nodes that are descendants of aNode
    if (nsEditorUtils::IsDescendantOf(tmp, aNode))
      child = tmp;
    else
    {
      child = nsnull;
    }
  }
  
  *aOutLastLeaf = child;
  return res;
}

PRBool
nsHTMLEditor::IsTextInDirtyFrameVisible(nsIDOMNode *aNode)
{
  PRBool isEmptyTextNode;
  nsresult res = IsVisTextNode(aNode, &isEmptyTextNode, PR_FALSE);
  if (NS_FAILED(res))
  {
    // We are following the historical decision:
    //   if we don't know, we say it's visible...

    return PR_TRUE;
  }

  return !isEmptyTextNode;
}


///////////////////////////////////////////////////////////////////////////
// IsVisTextNode: figure out if textnode aTextNode has any visible content.
//                  
nsresult
nsHTMLEditor::IsVisTextNode( nsIDOMNode *aNode, 
                             PRBool *outIsEmptyNode, 
                             PRBool aSafeToAskFrames)
{
  if (!aNode || !outIsEmptyNode) 
    return NS_ERROR_NULL_POINTER;
  *outIsEmptyNode = PR_TRUE;
  nsresult res = NS_OK;

  nsCOMPtr<nsITextContent> textContent = do_QueryInterface(aNode);
  // callers job to only call us with text nodes
  if (!textContent) 
    return NS_ERROR_NULL_POINTER;
  PRUint32 length = textContent->TextLength();
  if (aSafeToAskFrames)
  {
    nsCOMPtr<nsISelectionController> selCon;
    res = GetSelectionController(getter_AddRefs(selCon));
    if (NS_FAILED(res)) return res;
    if (!selCon) return NS_ERROR_FAILURE;
    PRBool isVisible = PR_FALSE;
    // ask the selection controller for information about whether any
    // of the data in the node is really rendered.  This is really
    // something that frames know about, but we aren't supposed to talk to frames.
    // So we put a call in the selection controller interface, since it's already
    // in bed with frames anyway.  (this is a fix for bug 22227, and a
    // partial fix for bug 46209)
    res = selCon->CheckVisibility(aNode, 0, length, &isVisible);
    if (NS_FAILED(res)) return res;
    if (isVisible) 
    {
      *outIsEmptyNode = PR_FALSE;
    }
  }
  else if (length)
  {
    if (textContent->IsOnlyWhitespace())
    {
      nsWSRunObject wsRunObj(this, aNode, 0);
      nsCOMPtr<nsIDOMNode> visNode;
      PRInt32 outVisOffset=0;
      PRInt16 visType=0;
      res = wsRunObj.NextVisibleNode(aNode, 0, address_of(visNode), &outVisOffset, &visType);
      if (NS_FAILED(res)) return res;
      if ( (visType == nsWSRunObject::eNormalWS) ||
           (visType == nsWSRunObject::eText) )
      {
        *outIsEmptyNode = (aNode != visNode);
      }
    }
    else
    {
      *outIsEmptyNode = PR_FALSE;
    }
  }
  return NS_OK;  
}
  

///////////////////////////////////////////////////////////////////////////
// IsEmptyNode: figure out if aNode is an empty node.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//                  
nsresult
nsHTMLEditor::IsEmptyNode( nsIDOMNode *aNode, 
                           PRBool *outIsEmptyNode, 
                           PRBool aSingleBRDoesntCount,
                           PRBool aListOrCellNotEmpty,
                           PRBool aSafeToAskFrames)
{
  if (!aNode || !outIsEmptyNode) return NS_ERROR_NULL_POINTER;
  *outIsEmptyNode = PR_TRUE;
  PRBool seenBR = PR_FALSE;
  return IsEmptyNodeImpl(aNode, outIsEmptyNode, aSingleBRDoesntCount,
                         aListOrCellNotEmpty, aSafeToAskFrames, &seenBR);
}

///////////////////////////////////////////////////////////////////////////
// IsEmptyNodeImpl: workhorse for IsEmptyNode.
//                  
nsresult
nsHTMLEditor::IsEmptyNodeImpl( nsIDOMNode *aNode, 
                               PRBool *outIsEmptyNode, 
                               PRBool aSingleBRDoesntCount,
                               PRBool aListOrCellNotEmpty,
                               PRBool aSafeToAskFrames,
                               PRBool *aSeenBR)
{
  if (!aNode || !outIsEmptyNode || !aSeenBR) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;

  if (nsEditor::IsTextNode(aNode))
  {
    res = IsVisTextNode(aNode, outIsEmptyNode, aSafeToAskFrames);
    return res;
  }

  // if it's not a text node (handled above) and it's not a container,
  // then we dont call it empty (it's an <hr>, or <br>, etc).
  // Also, if it's an anchor then dont treat it as empty - even though
  // anchors are containers, named anchors are "empty" but we don't
  // want to treat them as such.  Also, don't call ListItems or table
  // cells empty if caller desires.  Form Widgets not empty.
  if (!IsContainer(aNode) || nsHTMLEditUtils::IsNamedAnchor(aNode) ||
        nsHTMLEditUtils::IsFormWidget(aNode)                       ||
       (aListOrCellNotEmpty && nsHTMLEditUtils::IsListItem(aNode)) ||
       (aListOrCellNotEmpty && nsHTMLEditUtils::IsTableCell(aNode)) ) 
  {
    *outIsEmptyNode = PR_FALSE;
    return NS_OK;
  }
    
  // need this for later
  PRBool isListItemOrCell = 
       nsHTMLEditUtils::IsListItem(aNode) || nsHTMLEditUtils::IsTableCell(aNode);
       
  // loop over children of node. if no children, or all children are either 
  // empty text nodes or non-editable, then node qualifies as empty
  nsCOMPtr<nsIDOMNode> child;
  aNode->GetFirstChild(getter_AddRefs(child));
   
  while (child)
  {
    nsCOMPtr<nsIDOMNode> node = child;
    // is the node editable and non-empty?  if so, return false
    if (nsEditor::IsEditable(node))
    {
      if (nsEditor::IsTextNode(node))
      {
        res = IsVisTextNode(node, outIsEmptyNode, aSafeToAskFrames);
        if (NS_FAILED(res)) return res;
        if (!*outIsEmptyNode) return NS_OK;  // break out if we find we aren't emtpy
      }
      else  // an editable, non-text node.  we need to check it's content.
      {
        // is it the node we are iterating over?
        if (node == aNode) break;
        else if (aSingleBRDoesntCount && !*aSeenBR && nsTextEditUtils::IsBreak(node))
        {
          // the first br in a block doesn't count if the caller so indicated
          *aSeenBR = PR_TRUE;
        }
        else
        {
          // is it an empty node of some sort?
          // note: list items or table cells are not considered empty
          // if they contain other lists or tables
          if (isListItemOrCell)
          {
            if (nsHTMLEditUtils::IsList(node) || nsHTMLEditUtils::IsTable(node))
            { // break out if we find we aren't empty
              *outIsEmptyNode = PR_FALSE;
              return NS_OK;
            }
          }
          // is it a form widget?
          else if (nsHTMLEditUtils::IsFormWidget(aNode))
          { // break out if we find we aren't empty
            *outIsEmptyNode = PR_FALSE;
            return NS_OK;
          }
          
          PRBool isEmptyNode = PR_TRUE;
          res = IsEmptyNodeImpl(node, &isEmptyNode, aSingleBRDoesntCount, 
                                aListOrCellNotEmpty, aSafeToAskFrames, aSeenBR);
          if (NS_FAILED(res)) return res;
          if (!isEmptyNode) 
          { 
            // otherwise it ain't empty
            *outIsEmptyNode = PR_FALSE;
            return NS_OK;
          }
        }
      }
    }
    node->GetNextSibling(getter_AddRefs(child));
  }
  
  return NS_OK;
}

// add to aElement the CSS inline styles corresponding to the HTML attribute
// aAttribute with its value aValue
nsresult
nsHTMLEditor::SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                       const nsAString & aAttribute,
                                       const nsAString & aValue,
                                       PRBool aSuppressTransaction)
{
  PRBool useCSS;
  nsresult res = NS_OK;
  GetIsCSSEnabled(&useCSS);
  if (useCSS && mHTMLCSSUtils) {
    PRInt32 count;
    res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(aElement, nsnull, &aAttribute, &aValue, &count,
                                                     aSuppressTransaction);
    if (NS_FAILED(res)) return res;
    if (count) {
      // we found an equivalence ; let's remove the HTML attribute itself if it is set
      nsAutoString existingValue;
      PRBool wasSet = PR_FALSE;
      res = GetAttributeValue(aElement, aAttribute, existingValue, &wasSet);
      if (NS_FAILED(res)) return res;
      if (wasSet) {
        if (aSuppressTransaction)
          res = aElement->RemoveAttribute(aAttribute);
        else
          res = RemoveAttribute(aElement, aAttribute);
      }
    }
    else {
      // count is an integer that represents the number of CSS declarations applied to the
      // element. If it is zero, we found no equivalence in this implementation for the
      // attribute
      if (aAttribute.EqualsLiteral("style")) {
        // if it is the style attribute, just add the new value to the existing style
        // attribute's value
        nsAutoString existingValue;
        PRBool wasSet = PR_FALSE;
        res = GetAttributeValue(aElement, NS_LITERAL_STRING("style"), existingValue, &wasSet);
        if (NS_FAILED(res)) return res;
        existingValue.AppendLiteral(" ");
        existingValue.Append(aValue);
        if (aSuppressTransaction)
          res = aElement->SetAttribute(aAttribute, existingValue);
        else
          res = SetAttribute(aElement, aAttribute, existingValue);
      }
      else {
        // we have no CSS equivalence for this attribute and it is not the style
        // attribute; let's set it the good'n'old HTML way
        if (aSuppressTransaction)
          res = aElement->SetAttribute(aAttribute, aValue);
        else
          res = SetAttribute(aElement, aAttribute, aValue);
      }
    }
  }
  else {
    // we are not in an HTML+CSS editor; let's set the attribute the HTML way
    if (aSuppressTransaction)
      res = aElement->SetAttribute(aAttribute, aValue);
    else
      res = SetAttribute(aElement, aAttribute, aValue);
  }  
  return res;
}

nsresult
nsHTMLEditor::RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                          const nsAString & aAttribute,
                                          PRBool aSuppressTransaction)
{
  PRBool useCSS;
  nsresult res = NS_OK;
  GetIsCSSEnabled(&useCSS);
  if (useCSS && mHTMLCSSUtils) {
    res = mHTMLCSSUtils->RemoveCSSEquivalentToHTMLStyle(aElement, nsnull, &aAttribute, nsnull,
                                                        aSuppressTransaction);
    if (NS_FAILED(res)) return res;
  }

  nsAutoString existingValue;
  PRBool wasSet = PR_FALSE;
  res = GetAttributeValue(aElement, aAttribute, existingValue, &wasSet);
  if (NS_FAILED(res)) return res;
  if (wasSet) {
    if (aSuppressTransaction)
      res = aElement->RemoveAttribute(aAttribute);
    else
      res = RemoveAttribute(aElement, aAttribute);
  }
  return res;
}

nsresult
nsHTMLEditor::SetIsCSSEnabled(PRBool aIsCSSPrefChecked)
{
  nsresult  err = NS_ERROR_NOT_INITIALIZED;
  if (mHTMLCSSUtils)
  {
    err = mHTMLCSSUtils->SetCSSEnabled(aIsCSSPrefChecked);
  }
  return err;
}

// Set the block background color
NS_IMETHODIMP
nsHTMLEditor::SetCSSBackgroundColor(const nsAString& aColor)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);

  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kSetTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator)    return NS_ERROR_FAILURE;

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    nsAutoString bgcolor; bgcolor.AssignLiteral("bgcolor");
    nsCOMPtr<nsIDOMNode> cachedBlockParent = nsnull;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem)   return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      PRInt32 startOffset, endOffset;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = range->GetEndContainer(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      res = range->GetStartOffset(&startOffset);
      if (NS_FAILED(res)) return res;
      res = range->GetEndOffset(&endOffset);
      if (NS_FAILED(res)) return res;
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        // let's find the block container of the text node
        nsCOMPtr<nsIDOMNode> blockParent;
        blockParent = GetBlockNodeParent(startNode);
        // and apply the background color to that block container
        if (cachedBlockParent != blockParent)
        {
          cachedBlockParent = blockParent;
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
          PRInt32 count;
          res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, PR_FALSE);
          if (NS_FAILED(res)) return res;
        }
      }
      else if ((startNode == endNode) && nsTextEditUtils::IsBody(startNode) && isCollapsed)
      {
        // we have no block in the document, let's apply the background to the body 
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(startNode);
        PRInt32 count;
        res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, PR_FALSE);
        if (NS_FAILED(res)) return res;
      }
      else if ((startNode == endNode) && (((endOffset-startOffset) == 1) || (!startOffset && !endOffset)))
      {
        // a unique node is selected, let's also apply the background color
        // to the containing block, possibly the node itself
        nsCOMPtr<nsIDOMNode> selectedNode = GetChildAt(startNode, startOffset);
        PRBool isBlock =PR_FALSE;
        res = NodeIsBlockStatic(selectedNode, &isBlock);
        if (NS_FAILED(res)) return res;
        nsCOMPtr<nsIDOMNode> blockParent = selectedNode;
        if (!isBlock) {
          blockParent = GetBlockNodeParent(selectedNode);
        }
        if (cachedBlockParent != blockParent)
        {
          cachedBlockParent = blockParent;
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
          PRInt32 count;
          res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, PR_FALSE);
          if (NS_FAILED(res)) return res;
        }
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        // there are up to three phases here.  There are all the nodes
        // reported by the subtree iterator to be processed.  And there
        // are potentially a starting textnode and an ending textnode
        // which are only partially contained by the range.
        
        // lets handle the nodes reported by the iterator.  These nodes
        // are entirely contained in the selection range.  We build up
        // a list of them (since doing operations on the document during
        // iteration would perturb the iterator).

        nsCOMPtr<nsIContentIterator> iter =
          do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &res);
        if (NS_FAILED(res)) return res;
        if (!iter)          return NS_ERROR_FAILURE;

        nsCOMArray<nsIDOMNode> arrayOfNodes;
        nsCOMPtr<nsIDOMNode> node;
                
        // iterate range and build up array
        res = iter->Init(range);
        // init returns an error if no nodes in range.
        // this can easily happen with the subtree 
        // iterator if the selection doesn't contain
        // any *whole* nodes.
        if (NS_SUCCEEDED(res))
        {
          while (!iter->IsDone())
          {
            node = do_QueryInterface(iter->GetCurrentNode());
            if (!node)
              return NS_ERROR_FAILURE;

            if (IsEditable(node))
            {
              arrayOfNodes.AppendObject(node);
            }

            iter->Next();
          }
        }
        // first check the start parent of the range to see if it needs to 
        // be separately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(startNode) && IsEditable(startNode))
        {
          nsCOMPtr<nsIDOMNode> blockParent;
          blockParent = GetBlockNodeParent(startNode);
          if (cachedBlockParent != blockParent)
          {
            cachedBlockParent = blockParent;
            nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
            PRInt32 count;
            res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, PR_FALSE);
            if (NS_FAILED(res)) return res;
          }
        }
        
        // then loop through the list, set the property on each node
        PRInt32 listCount = arrayOfNodes.Count();
        PRInt32 j;
        for (j = 0; j < listCount; j++)
        {
          node = arrayOfNodes[j];
          // do we have a block here ?
          PRBool isBlock =PR_FALSE;
          res = NodeIsBlockStatic(node, &isBlock);
          if (NS_FAILED(res)) return res;
          nsCOMPtr<nsIDOMNode> blockParent = node;
          if (!isBlock) {
            // no we don't, let's find the block ancestor
            blockParent = GetBlockNodeParent(node);
          }
          if (cachedBlockParent != blockParent)
          {
            cachedBlockParent = blockParent;
            nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
            PRInt32 count;
            // and set the property on it
            res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, PR_FALSE);
            if (NS_FAILED(res)) return res;
          }
        }
        arrayOfNodes.Clear();
        
        // last check the end parent of the range to see if it needs to 
        // be separately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(endNode) && IsEditable(endNode))
        {
          nsCOMPtr<nsIDOMNode> blockParent;
          blockParent = GetBlockNodeParent(endNode);
          if (cachedBlockParent != blockParent)
          {
            cachedBlockParent = blockParent;
            nsCOMPtr<nsIDOMElement> element = do_QueryInterface(blockParent);
            PRInt32 count;
            res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, nsnull, &bgcolor, &aColor, &count, PR_FALSE);
            if (NS_FAILED(res)) return res;
          }
        }
      }
      enumerator->Next();
    }
  }
  if (!cancel)
  {
    // post-process
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetBackgroundColor(const nsAString& aColor)
{
  nsresult res;
  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);
  if (useCSS) {
    // if we are in CSS mode, we have to apply the background color to the
    // containing block (or the body if we have no block-level element in
    // the document)
    res = SetCSSBackgroundColor(aColor);
  }
  else {
    // but in HTML mode, we can only set the document's background color
    res = SetHTMLBackgroundColor(aColor);
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// NodesSameType: do these nodes have the same tag?
//                    
PRBool 
nsHTMLEditor::NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2) 
  {
    NS_NOTREACHED("null node passed to nsEditor::NodesSameType()");
    return PR_FALSE;
  }

  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  nsIAtom *tag1 = GetTag(aNode1);

  if (tag1 == GetTag(aNode2)) {
    if (useCSS && tag1 == nsEditProperty::span) {
      if (mHTMLCSSUtils->ElementsSameStyle(aNode1, aNode2)) {
        return PR_TRUE;
      }
    }
    else {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLEditor::CopyLastEditableChildStyles(nsIDOMNode * aPreviousBlock, nsIDOMNode * aNewBlock,
                                          nsIDOMNode **aOutBrNode)
{
  *aOutBrNode = nsnull;
  nsCOMPtr<nsIDOMNode> child, tmp;
  nsresult res;
  // first, clear out aNewBlock.  Contract is that we want only the styles from previousBlock.
  res = aNewBlock->GetFirstChild(getter_AddRefs(child));
  while (NS_SUCCEEDED(res) && child)
  {
    res = DeleteNode(child);
    if (NS_FAILED(res)) return res;
    res = aNewBlock->GetFirstChild(getter_AddRefs(child));
  }
  // now find and clone the styles
  child = aPreviousBlock;
  tmp = aPreviousBlock;
  while (tmp) {
    child = tmp;
    res = GetLastEditableChild(child, address_of(tmp));
    if (NS_FAILED(res)) return res;
  }
  while (child && nsTextEditUtils::IsBreak(child)) {
    nsCOMPtr<nsIDOMNode> priorNode;
    res = GetPriorHTMLNode(child, address_of(priorNode));
    if (NS_FAILED(res)) return res;
    child = priorNode;
  }
  nsCOMPtr<nsIDOMNode> newStyles = nsnull, deepestStyle = nsnull;
  while (child && (child != aPreviousBlock)) {
    if (nsHTMLEditUtils::IsInlineStyle(child) ||
        nsEditor::NodeIsType(child, nsEditProperty::span)) {
      nsAutoString domTagName;
      child->GetNodeName(domTagName);
      ToLowerCase(domTagName);
      if (newStyles) {
        nsCOMPtr<nsIDOMNode> newContainer;
        res = InsertContainerAbove(newStyles, address_of(newContainer), domTagName);
        if (NS_FAILED(res)) return res;
        newStyles = newContainer;
      }
      else {
        res = CreateNode(domTagName, aNewBlock, 0, getter_AddRefs(newStyles));
        if (NS_FAILED(res)) return res;
        deepestStyle = newStyles;
      }
      res = CloneAttributes(newStyles, child);
      if (NS_FAILED(res)) return res;
    }
    nsCOMPtr<nsIDOMNode> tmp;
    res = child->GetParentNode(getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    child = tmp;
  }
  if (deepestStyle) {
    nsCOMPtr<nsIDOMNode> outBRNode;
    res = CreateBR(deepestStyle, 0, address_of(outBRNode));
    if (NS_FAILED(res)) return res;
    // Getters must addref
    *aOutBrNode = outBRNode;
    NS_ADDREF(*aOutBrNode);
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::GetElementOrigin(nsIDOMElement * aElement, PRInt32 & aX, PRInt32 & aY)
{
  // we are going to need the PresShell
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsIFrame *frame = 0; // not ref-counted
  ps->GetPrimaryFrameFor(content, &frame);

  float t2p;
  nsCOMPtr<nsIPresContext> pcontext;
  ps->GetPresContext(getter_AddRefs(pcontext));
  t2p = pcontext->TwipsToPixels();


  if (nsHTMLEditUtils::IsHR(aElement)) {
    frame = frame->GetNextSibling();
  }
  PRInt32 offsetX = 0, offsetY = 0;
  while (frame) {
    // Look for a widget so we can get screen coordinates
    nsIView* view = frame->GetViewExternal();
    if (view && view->HasWidget())
      break;
    
    // No widget yet, so count up the coordinates of the frame 
    nsPoint origin = frame->GetPosition();
    offsetX += origin.x;
    offsetY += origin.y;

    frame = frame->GetParent();
  }

  aX = NSTwipsToIntPixels(offsetX , t2p);
  aY = NSTwipsToIntPixels(offsetY , t2p);

  return NS_OK;
}

nsresult
nsHTMLEditor::EndUpdateViewBatch()
{
  nsresult res = nsEditor::EndUpdateViewBatch();
  if (NS_FAILED(res)) return res;

  // We may need to show resizing handles or update existing ones after
  // all transactions are done. This way of doing is preferred to DOM
  // mutation events listeners because all the changes the user can apply
  // to a document may result in multiple events, some of them quite hard
  // to listen too (in particular when an ancestor of the selection is
  // changed but the selection itself is not changed).
  if (mUpdateCount == 0) {
    nsCOMPtr<nsISelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NOT_INITIALIZED;
    res = CheckSelectionStateForAnonymousButtons(selection);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::IgnoreSpuriousDragEvent(PRBool aIgnoreSpuriousDragEvent)
{
  mIgnoreSpuriousDragEvent = aIgnoreSpuriousDragEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectionContainer(nsIDOMElement ** aReturn)
{
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  // if we don't get the selection, just skip this
  if (NS_FAILED(res) || !selection) return res;

  PRBool bCollapsed;
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> focusNode;

  if (bCollapsed) {
    res = selection->GetFocusNode(getter_AddRefs(focusNode));
    if (NS_FAILED(res)) return res;
  }
  else {

    PRInt32 rangeCount;
    res = selection->GetRangeCount(&rangeCount);
    if (NS_FAILED(res)) return res;

    if (rangeCount == 1) {

      nsCOMPtr<nsIDOMRange> range;
      res = selection->GetRangeAt(0, getter_AddRefs(range));
      if (NS_FAILED(res)) return res;
      if (!range) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMNode> startContainer, endContainer;
      res = range->GetStartContainer(getter_AddRefs(startContainer));
      if (NS_FAILED(res)) return res;
      res = range->GetEndContainer(getter_AddRefs(endContainer));
      if (NS_FAILED(res)) return res;
      PRInt32 startOffset, endOffset;
      res = range->GetStartOffset(&startOffset);
      if (NS_FAILED(res)) return res;
      res = range->GetEndOffset(&endOffset);
      if (NS_FAILED(res)) return res;

      nsCOMPtr<nsIDOMElement> focusElement;
      if (startContainer == endContainer && startOffset + 1 == endOffset) {
        res = GetSelectedElement(EmptyString(), getter_AddRefs(focusElement));
        if (NS_FAILED(res)) return res;
        if (focusElement)
          focusNode = do_QueryInterface(focusElement);
      }
      if (!focusNode) {
        res = range->GetCommonAncestorContainer(getter_AddRefs(focusNode));
        if (NS_FAILED(res)) return res;
      }
    }
    else {
      PRInt32 i;
      nsCOMPtr<nsIDOMRange> range;
      for (i = 0; i < rangeCount; i++)
      {
        res = selection->GetRangeAt(i, getter_AddRefs(range));
        if (NS_FAILED(res)) return res;
        nsCOMPtr<nsIDOMNode> startContainer;
        range->GetStartContainer(getter_AddRefs(startContainer));
        if (!focusNode)
          focusNode = startContainer;
        else if (focusNode != startContainer) {
          res = startContainer->GetParentNode(getter_AddRefs(focusNode));
          if (NS_FAILED(res)) return res;
          break;
        }
      }
    }
  }

  if (focusNode) {
    PRUint16 nodeType;
    focusNode->GetNodeType(&nodeType);
    if (nsIDOMNode::TEXT_NODE == nodeType) {
      nsCOMPtr<nsIDOMNode> parent;
      res = focusNode->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(res)) return res;
      focusNode = parent;
    }
  }

  nsCOMPtr<nsIDOMElement> focusElement = do_QueryInterface(focusNode);
  *aReturn = focusElement;
  NS_IF_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::IsAnonymousElement(nsIDOMElement * aElement, PRBool * aReturn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  *aReturn = content->IsNativeAnonymous();
  return NS_OK;
}

