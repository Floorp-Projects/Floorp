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
#include "nsCOMPtr.h"
#include "nsGfxTextControlFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsITextWidget.h"
#include "nsITextAreaWidget.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"
#include "nsString.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsFont.h"
#include "nsDOMEvent.h"
#include "nsIFormControl.h"
#include "nsFormFrame.h"
#include "nsIContent.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"

#include "nsCSSRendering.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"

#include "nsIWebShell.h"
#include "nsINameSpaceManager.h"
#include "nsIPref.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsIDocumentViewer.h"
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"

#include "nsIHTMLEditor.h"
#include "nsEditorCID.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h"

#ifdef SingleSignon
#include "prmem.h"
#include "nsIURL.h"
#include "nsIWalletService.h"
#include "nsIServiceManager.h"
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);
#endif

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kTextCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);

static NS_DEFINE_IID(kITextEditorIID, NS_ITEXTEDITOR_IID);
static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID);
static NS_DEFINE_IID(kIHTMLEditorIID, NS_IHTMLEDITOR_IID);
static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);
static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);
static NS_DEFINE_CID(kEditorCID, NS_EDITOR_CID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);


#define EMPTY_DOCUMENT "resource:/res/html/empty_doc.html"

//#define NOISY

extern nsresult NS_NewNativeTextControlFrame(nsIFrame** aNewFrame); 

nsresult
NS_NewGfxTextControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  *aNewFrame = new nsGfxTextControlFrame;
  if (nsnull == aNewFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult result = ((nsGfxTextControlFrame*)(*aNewFrame))->InitTextControl();
  if (NS_FAILED(result))
  { // can't properly initialized ender, probably it isn't installed
    //NS_RELEASE(*aNewFrame); // XXX: need to release allocated ender text frame!
    result = NS_NewNativeTextControlFrame(aNewFrame);
  }
  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::InitTextControl()
{
  nsresult result = NS_OK;

  mWebShell       = nsnull;
  mCreatingViewer = PR_FALSE;
  // create the stream observer
  mTempObserver   = new EnderTempObserver();
  if (!mTempObserver) { return NS_ERROR_OUT_OF_MEMORY; }
  mTempObserver->SetFrame(this);
  NS_ADDREF(mTempObserver);

  // create the document observer
  mDocObserver   = new nsEnderDocumentObserver();
  if (!mDocObserver) { return NS_ERROR_OUT_OF_MEMORY; }
  mDocObserver->SetFrame(this);
  NS_ADDREF(mDocObserver);

  if (PR_TRUE==IsPlainTextControl())
  {
    nsCOMPtr<nsITextEditor> theEditor;
    result = nsComponentManager::CreateInstance(kTextEditorCID,
                                                nsnull,
                                                kITextEditorIID, 
                                                getter_AddRefs(theEditor));
    if (NS_FAILED(result)) { return result; }
    if (!theEditor) { return NS_ERROR_OUT_OF_MEMORY; }
    mEditor = do_QueryInterface(theEditor);
    if (!mEditor) { return NS_ERROR_NO_INTERFACE; }
  }
  else
  {
    nsCOMPtr<nsIHTMLEditor> theEditor;
    result = nsComponentManager::CreateInstance(kHTMLEditorCID,
                                                nsnull,
                                                kIHTMLEditorIID, getter_AddRefs(theEditor));
    if (NS_FAILED(result)) { return result; }
    if (!theEditor) { return NS_ERROR_OUT_OF_MEMORY; }
    mEditor = do_QueryInterface(theEditor);
    if (!mEditor) { return NS_ERROR_NO_INTERFACE; }
  }

  // allocate mDummy here to self-check native text control impl. vs. ender text control impl.
  //NS_NewNativeTextControlFrame((nsIFrame **)&mDummyFrame); //DUMMY
  // mDummyInitialized = PR_TRUE; //DUMMY
  return NS_OK;
}

nsGfxTextControlFrame::nsGfxTextControlFrame()
: mWebShell(0), mCreatingViewer(PR_FALSE),
  mTempObserver(0), mDocObserver(0), 
  mDummyFrame(0), mNeedsStyleInit(PR_TRUE),
  mDummyInitialized(PR_FALSE) // DUMMY
{
}

nsGfxTextControlFrame::~nsGfxTextControlFrame()
{
  if (mTempObserver)
  {
    mTempObserver->SetFrame(nsnull);
    NS_RELEASE(mTempObserver);
  }
  if (mKeyListener) 
  {
    nsCOMPtr<nsIEditor>editor = do_QueryInterface(mEditor);
    NS_ASSERTION(editor, "bad QI to nsIEditor from mEditor");
    if (editor)
    {
      nsCOMPtr<nsIDOMDocument>domDoc;
      nsresult result = editor->GetDocument(getter_AddRefs(domDoc));
      if (NS_SUCCEEDED(result) && domDoc)
      {
        nsCOMPtr<nsIDOMEventReceiver> er;
        result = domDoc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(er));
        if (NS_SUCCEEDED(result) && er) {
          er->RemoveEventListenerByIID(mKeyListener, kIDOMKeyListenerIID);
        }
      }
    }
  }
  mEditor = do_QueryInterface(nsnull);  // editor must be destroyed before the webshell!
  if (nsnull != mWebShell) 
  {
    mWebShell->Destroy();    
    NS_RELEASE(mWebShell);
  }
  if (mDocObserver)
  {
    mDocObserver->SetFrame(nsnull);
    NS_RELEASE(mDocObserver);
  }

  // this will be a leak -- NS_IF_RELEASE(mDummyFrame);
}

void
nsGfxTextControlFrame::EnterPressed(nsIPresContext& aPresContext) 
{
  if (mFormFrame && mFormFrame->CanSubmit(*this)) {
    nsIContent *formContent = nsnull;

    mFormFrame->GetContent(&formContent);
    if (nsnull != formContent) {
      nsEvent event;
      nsEventStatus status = nsEventStatus_eIgnore;

      event.eventStructType = NS_EVENT;
      event.message = NS_FORM_SUBMIT;
      formContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
      NS_RELEASE(formContent);
    }

    mFormFrame->OnSubmit(&aPresContext, this);
  }
}

nsWidgetInitData*
nsGfxTextControlFrame::GetWidgetInitData(nsIPresContext& aPresContext)
{
  return nsnull;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetText(nsString* aText, PRBool aInitialValue)
{
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) 
  {
    if (PR_TRUE==aInitialValue)
    {
      result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
    }
    else
    {
      if (PR_TRUE==IsInitialized()) {
        nsString format ("text/html");
        mEditor->OutputToString(*aText, format, 0);
      }
      else {
        result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
      }
    }
    RemoveNewlines(*aText);
  } 
  else 
  {
    nsIDOMHTMLTextAreaElement* textArea = nsnull;
    result = mContent->QueryInterface(kIDOMHTMLTextAreaElementIID, (void**)&textArea);
    if ((NS_OK == result) && textArea) {
      if (PR_TRUE == aInitialValue) {
        result = textArea->GetDefaultValue(*aText);
      }
      else {
        result = textArea->GetValue(*aText);
      }
      NS_RELEASE(textArea);
    }
  }
  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                        nsIContent*     aChild,
                                        nsIAtom*        aAttribute,
                                        PRInt32         aHint)
{
  if (PR_FALSE==IsInitialized()) {return NS_ERROR_NOT_INITIALIZED;}
  nsresult result = NS_OK;

  if (nsHTMLAtoms::value == aAttribute) 
  {
    nsString value;
    GetText(&value, PR_TRUE);           // get the initial value from the content attribute
    mEditor->EnableUndo(PR_FALSE);      // wipe out undo info
    SetTextControlFrameState(value);    // set new text value
    mEditor->EnableUndo(PR_TRUE);       // fire up a new txn stack
    nsFormFrame::StyleChangeReflow(aPresContext, this);
  } 
  else if (nsHTMLAtoms::maxlength == aAttribute) 
  {
    PRInt32 maxLength;
    nsresult rv = GetMaxLength(&maxLength);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    {  // set the maxLength attribute
      mEditor->SetMaxTextLength(maxLength);
      // if maxLength>docLength, we need to truncate the doc content
    }
    else { // unset the maxLength attribute
      mEditor->SetMaxTextLength(-1);
    }
  } 
  else if (nsHTMLAtoms::readonly == aAttribute) 
  {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));     
    nsresult rv = DoesAttributeExist(nsHTMLAtoms::readonly);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= TEXT_EDITOR_FLAG_READONLY;
      presShell->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(TEXT_EDITOR_FLAG_READONLY);
      presShell->SetCaretEnabled(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (nsHTMLAtoms::disabled == aAttribute) 
  {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));     
    nsresult rv = DoesAttributeExist(nsHTMLAtoms::disabled);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= TEXT_EDITOR_FLAG_DISABLED;
      presShell->SetCaretEnabled(PR_FALSE);
      nsCOMPtr<nsIDocument> doc; 
      presShell->GetDocument(getter_AddRefs(doc));
      NS_ASSERTION(doc, "null document");
      if (!doc) { return NS_ERROR_NULL_POINTER; }
      doc->SetDisplaySelection(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(TEXT_EDITOR_FLAG_DISABLED);
      presShell->SetCaretEnabled(PR_TRUE);
      nsCOMPtr<nsIDocument> doc; 
      presShell->GetDocument(getter_AddRefs(doc));
      NS_ASSERTION(doc, "null document");
      if (!doc) { return NS_ERROR_NULL_POINTER; }
      doc->SetDisplaySelection(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (nsHTMLAtoms::size == aAttribute) {
    nsFormFrame::StyleChangeReflow(aPresContext, this);
  }
  // Allow the base class to handle common attributes supported
  // by all form elements... 
  else {
    result = nsFormControlFrame::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
  }

// DUMMY
  if (mDummyFrame)
  {
    nsresult dummyResult = mDummyFrame->AttributeChanged(aPresContext, aChild, aAttribute, aHint);
    NS_ASSERTION((NS_SUCCEEDED(dummyResult)), "dummy frame attribute changed failed.");
  }
// END DUMMY

  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::DoesAttributeExist(nsIAtom *aAtt)
{
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) 
  {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(aAtt, value);
    NS_RELEASE(content);
  }
  return result;
}

void 
nsGfxTextControlFrame::PostCreateWidget(nsIPresContext* aPresContext,
                                          nscoord& aWidth, 
                                          nscoord& aHeight)
{
  nsresult rv;
  // initialize the webshell, if it hasn't already been constructed
  // if the size is not 0 and there is a src, create the web shell
  if (aPresContext && (aWidth >= 0) && (aHeight >= 0))
  {
    if (nsnull == mWebShell) 
    {
      nsSize  maxSize(aWidth, aHeight);
      rv = CreateWebShell(*aPresContext, maxSize);
      NS_ASSERTION(nsnull!=mWebShell, "null web shell after attempt to create.");
    }

    if (nsnull != mWebShell) 
    {
      mCreatingViewer=PR_TRUE;
      nsAutoString url(EMPTY_DOCUMENT);
      rv = mWebShell->LoadURL(url.GetUnicode());  // URL string with a default nsnull value for post Data
    }
  }
}

PRBool
nsGfxTextControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                        nsString* aValues, nsString* aNames)
{
  if (!aValues || !aNames) { return PR_FALSE; }

  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_NOT_THERE == result)) {
    return PR_FALSE;
  }

  aNames[0] = name;  
  aNumValues = 1;

  GetText(&(aValues[0]), PR_FALSE);
  // XXX: error checking
  return PR_TRUE;
}


void 
nsGfxTextControlFrame::Reset() 
{
  nsAutoString value;
  nsresult valStatus = GetText(&value, PR_TRUE);
  NS_ASSERTION((NS_SUCCEEDED(valStatus)), "GetText failed");
  if (NS_SUCCEEDED(valStatus))
  {
    SetTextControlFrameState(value);
  }
}  

NS_METHOD 
nsGfxTextControlFrame::Paint(nsIPresContext& aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               const nsRect& aDirtyRect,
                               nsFramePaintLayer aWhichLayer)
{
  if (mWebShell)
  {
    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) 
    {
      nsAutoString text(" ");
      nsRect rect(0, 0, mRect.width, mRect.height);
      PaintTextControl(aPresContext, aRenderingContext, aDirtyRect, 
                       text, mStyleContext, rect);
    }
  }
  return NS_OK;
}

void 
nsGfxTextControlFrame::PaintTextControlBackground(nsIPresContext& aPresContext,
                                                    nsIRenderingContext& aRenderingContext,
                                                    const nsRect& aDirtyRect,
                                                    nsFramePaintLayer aWhichLayer)
{
  // we paint our own border, but everything else is painted by the mWebshell
  if (mWebShell)
  {
    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) 
    {
      nsAutoString text(" ");
      nsRect rect(0, 0, mRect.width, mRect.height);
      PaintTextControl(aPresContext, aRenderingContext, aDirtyRect, 
                       text, mStyleContext, rect);
    }
  }
}

void 
nsGfxTextControlFrame::PaintTextControl(nsIPresContext& aPresContext,
                                          nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect, 
                                          nsString& aText,
                                          nsIStyleContext* aStyleContext,
                                          nsRect& aRect)
{
  // XXX: aText is currently unused!
  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (disp->mVisible) 
  {
    const nsStyleSpacing* mySpacing =
      (const nsStyleSpacing*)aStyleContext->GetStyleData(eStyleStruct_Spacing);
    PRIntn skipSides = 0;
    nsRect rect(0, 0, mRect.width, mRect.height);
    //PaintTextControl(aPresContext, aRenderingContext, text, mStyleContext, rect);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *mySpacing, aStyleContext, skipSides);
  }
}

void nsGfxTextControlFrame::GetTextControlFrameState(nsString& aValue)
{
  aValue = "";  // initialize out param
  if (PR_TRUE==IsInitialized()) {
    nsString format ("text/html");
    mEditor->OutputToString(aValue, format, 0);
  }
}     

void nsGfxTextControlFrame::SetTextControlFrameState(const nsString& aValue)
{
  if (PR_TRUE==IsInitialized()) 
  {
    nsAutoString currentValue;
    nsString format ("text/html");
    nsresult result = mEditor->OutputToString(currentValue, format, 0);
    if (PR_TRUE==IsSingleLineTextControl()) {
      RemoveNewlines(currentValue); 
    }
    if (PR_FALSE==currentValue.Equals(aValue))  // this is necessary to avoid infinite recursion
    {
      nsCOMPtr<nsIEditor>editor = do_QueryInterface(mEditor);
      NS_ASSERTION(editor, "bad QI to nsIEditor from mEditor");
      if (editor)
      {
        nsCOMPtr<nsIDOMSelection>selection;
        result = editor->GetSelection(getter_AddRefs(selection));
        if (NS_SUCCEEDED(result) && selection)
        {
          nsCOMPtr<nsIDOMDocument>domDoc;
          editor->GetDocument(getter_AddRefs(domDoc));
          if (domDoc)
          {
            nsCOMPtr<nsIDOMNode> bodyNode;
            nsAutoString bodyTag = "body";
            result = GetFirstNodeOfType(bodyTag, domDoc, getter_AddRefs(bodyNode));
            SelectAllTextContent(bodyNode, selection);
            mEditor->InsertText(aValue);
          }
        }
      }
    }
  }    
}

NS_IMETHODIMP nsGfxTextControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::value == aName) 
  {
    mEditor->EnableUndo(PR_FALSE);      // wipe out undo info
    SetTextControlFrameState(aValue);   // set new text value
    mEditor->EnableUndo(PR_TRUE);       // fire up a new txn stack
  }
  else {
    return nsFormControlFrame::SetProperty(aName, aValue);
  }
  return NS_OK;
}      

NS_IMETHODIMP nsGfxTextControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::value == aName) {
    GetTextControlFrameState(aValue);
  }
  else {
    return nsFormControlFrame::GetProperty(aName, aValue);
  }

  return NS_OK;
}  


/* --------------------- Ender methods ---------------------- */


nsresult
nsGfxTextControlFrame::CreateWebShell(nsIPresContext& aPresContext,
                                        const nsSize& aSize)
{
  nsresult rv;
  nsIContent* content;
  GetContent(&content); // ??? 

  rv = nsComponentManager::CreateInstance(kWebShellCID, nsnull, kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "could not create web widget");
    return rv;
  }
  
  // pass along marginwidth, marginheight, scrolling so sub document can use it
  mWebShell->SetMarginWidth(0);
  mWebShell->SetMarginHeight(0);
  nsCompatibility mode;
  aPresContext.GetCompatibilityMode(&mode);
  mWebShell->SetIsFrame(PR_TRUE);
  
  /* XXX
  nsString frameName;
  if (GetName(content, frameName)) {
    mWebShell->SetName(frameName.GetUnicode());
  }
  */

  // If our container is a web-shell, inform it that it has a new
  // child. If it's not a web-shell then some things will not operate
  // properly.
  nsISupports* container;
  aPresContext.GetContainer(&container);
  if (nsnull != container) 
  {
    nsIWebShell* outerShell = nsnull;
    container->QueryInterface(kIWebShellIID, (void**) &outerShell);
    if (nsnull != outerShell) {
      outerShell->AddChild(mWebShell);

      // connect the container...
      nsIWebShellContainer* outerContainer = nsnull;
      container->QueryInterface(kIWebShellContainerIID, (void**) &outerContainer);
      if (nsnull != outerContainer) {
        mWebShell->SetContainer(outerContainer);
        NS_RELEASE(outerContainer);
      }

#ifdef INCLUDE_XUL
      nsWebShellType parentType;
      outerShell->GetWebShellType(parentType);
      nsIAtom* typeAtom = NS_NewAtom("type");
	    nsAutoString value;
	    content->GetAttribute(kNameSpaceID_None, typeAtom, value);
	    if (value.EqualsIgnoreCase("content")) {
		    // The web shell's type is content.
		    mWebShell->SetWebShellType(nsWebShellContent);
        nsCOMPtr<nsIWebShellContainer> shellAsContainer;
				shellAsContainer = do_QueryInterface(mWebShell);
				shellAsContainer->ContentShellAdded(mWebShell, content);
      }
      else {
        // Inherit our type from our parent webshell.  If it is
		    // chrome, we'll be chrome.  If it is content, we'll be
		    // content.
		    mWebShell->SetWebShellType(parentType);
      }
#endif // INCLUDE_XUL 

      nsIPref*  outerPrefs = nsnull;  // connect the prefs
      outerShell->GetPrefs(outerPrefs);
      if (nsnull != outerPrefs) 
      {
        mWebShell->SetPrefs(outerPrefs);
        NS_RELEASE(outerPrefs);
      } 
      NS_RELEASE(outerShell);
    }
    NS_RELEASE(container);
  }

  float t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext.GetShell(getter_AddRefs(presShell));     

  // create, init, set the parent of the view
  nsIView* view;
  rv = nsComponentManager::CreateInstance(kCViewCID, nsnull, kIViewIID,
                                        (void **)&view);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "Could not create view for nsHTMLFrame");
    return rv;
  }

  nsIView* parView;
  nsPoint origin;
  GetOffsetFromView(origin, &parView);  
  nsRect viewBounds(origin.x, origin.y, aSize.width, aSize.height);

  nsCOMPtr<nsIViewManager> viewMan;
  presShell->GetViewManager(getter_AddRefs(viewMan));  
  rv = view->Init(viewMan, viewBounds, parView);
  viewMan->InsertChild(parView, view, 0);
  rv = view->CreateWidget(kCChildCID);
  SetView(view);

  // if the visibility is hidden, reflect that in the view
  const nsStyleDisplay* display;
  GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
  if (NS_STYLE_VISIBILITY_HIDDEN == display->mVisible) {
    view->SetVisibility(nsViewVisibility_kHide);
  }

  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
  nsMargin border;
  spacing->CalcBorderFor(this, border);

  nsIWidget* widget;
  view->GetWidget(widget);
  nsRect webBounds(NSToCoordRound(border.left * t2p), 
                   NSToCoordRound(border.top * t2p), 
                   NSToCoordRound((aSize.width  - border.right) * t2p), 
                   NSToCoordRound((aSize.height - border.bottom) * t2p));

  mWebShell->Init(widget->GetNativeData(NS_NATIVE_WIDGET), 
                  webBounds.x, webBounds.y,
                  webBounds.width, webBounds.height);
  NS_RELEASE(content);
  NS_RELEASE(widget);

  mWebShell->SetObserver(mTempObserver);
  mWebShell->Show();

  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::Reflow(nsIPresContext& aPresContext,
                                nsHTMLReflowMetrics& aMetrics,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus& aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsGfxTextControlFrame::Reflow: aMaxSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  nsresult rv = nsFormControlFrame::Reflow(aPresContext, aMetrics, aReflowState, aStatus);
#ifdef NOISY
  printf ("exit nsGfxTextControlFrame::Reflow: size=%d,%d",
           aMetrics.width, aMetrics.height);
#endif


  // resize the sub document
  if (NS_SUCCEEDED(rv) && mWebShell) 
  {
    // get the border
    const nsStyleSpacing* spacing;
    GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);    nsMargin border;
    spacing->CalcBorderFor(this, border);

    float t2p;
    aPresContext.GetTwipsToPixels(&t2p);
    nsRect subBounds;

    // XXX: the point here is to make a single-line edit field as wide as it wants to be, 
    //      so it will scroll horizontally if the characters take up more space than the field
    subBounds.x = NSToCoordRound(border.left * t2p);
    subBounds.y = NSToCoordRound(border.top * t2p);
    subBounds.width  = NSToCoordRound((aMetrics.width - (border.left + border.right)) * t2p);
    subBounds.height = NSToCoordRound((aMetrics.height - (border.top + border.bottom)) * t2p);
    mWebShell->SetBounds(subBounds.x, subBounds.y, 106, 20);
    mWebShell->SetBounds(subBounds.x, subBounds.y, subBounds.width, subBounds.height);
    mWebShell->Repaint(PR_TRUE); 
#ifdef NOISY
    printf("webshell set to (%d, %d, %d %d)\n", 
           border.left, border.top, 
           (aMetrics.width - (border.left + border.right)), 
           (aMetrics.height - (border.top + border.bottom)));
#endif
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsGfxTextControlFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));

// DUMMY
  if (mDummyFrame)
  {
    if (!mDummyInitialized)
    {
      mDummyFrame->Init(aPresContext, mContent, mParent, mStyleContext, nsnull);
      mDummyInitialized = PR_TRUE;
    }
    nsHTMLReflowMetrics metrics = aMetrics;
    nsHTMLReflowState reflowState = aReflowState;
    nsReflowStatus status = aStatus;
    nsresult dummyResult = mDummyFrame->Reflow(aPresContext, metrics, reflowState, status);
    NS_ASSERTION((NS_SUCCEEDED(dummyResult)), "dummy frame reflow failed.");
    if (aMetrics.width != metrics.width) 
    { 
      printf("CT: different widths\n"); 
      NS_ASSERTION(0, "CT: different widths\n");
    }
    if (aMetrics.height != metrics.height) 
    { 
      printf("CT: different heights\n"); 
      NS_ASSERTION(0, "CT: different heights\n");
    }
  }
// END DUMMY

  return NS_OK;
}

nsresult 
nsGfxTextControlFrame::RequiresWidget(PRBool &aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetPresShellFor(nsIWebShell* aWebShell, nsIPresShell** aPresShell)
{
  if (!aWebShell || !aPresShell) { return NS_ERROR_NULL_POINTER; }
  nsresult result = NS_ERROR_NULL_POINTER;
  *aPresShell = nsnull;
  nsIContentViewer* cv = nsnull;
  aWebShell->GetContentViewer(&cv);
  if (nsnull != cv) 
  {
    nsIDocumentViewer* docv = nsnull;
    cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
    if (nsnull != docv) 
    {
      nsIPresContext* cx;
      docv->GetPresContext(cx);
	    if (nsnull != cx) 
      {
	      result = cx->GetShell(aPresShell);
	      NS_RELEASE(cx);
	    }
      NS_RELEASE(docv);
    }
    NS_RELEASE(cv);
  }
  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetFirstNodeOfType(const nsString& aTag, nsIDOMDocument *aDOMDoc, nsIDOMNode **aNode)
{
  if (!aDOMDoc || !aNode) { return NS_ERROR_NULL_POINTER; }
  *aNode=nsnull;
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsresult result = aDOMDoc->GetElementsByTagName(aTag, getter_AddRefs(nodeList));
  if ((NS_SUCCEEDED(result)) && nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    result = nodeList->Item(0, aNode);
    if (!aNode) { result = NS_ERROR_NULL_POINTER; }
  }
  return result;
}

nsresult
nsGfxTextControlFrame::GetFirstFrameForType(const nsString& aTag, nsIPresShell *aPresShell, 
                                              nsIDOMDocument *aDOMDoc, nsIFrame **aResult)
{
  if (!aPresShell || !aDOMDoc || !aResult) { return NS_ERROR_NULL_POINTER; }
  nsresult result;
  *aResult = nsnull;
  nsCOMPtr<nsIDOMNode>node;
  result = GetFirstNodeOfType(aTag, aDOMDoc, getter_AddRefs(node));
  if ((NS_SUCCEEDED(result)) && node)
  {
    nsCOMPtr<nsIContent>content = do_QueryInterface(node);
    if (content)
    {
      result = aPresShell->GetPrimaryFrameFor(content, aResult);
    }
  }
  return result;
}

// XXX: this really should use a content iterator over the whole document
//      looking for the first and last text node
nsresult
nsGfxTextControlFrame::SelectAllTextContent(nsIDOMNode *aBodyNode, nsIDOMSelection *aSelection)
{
  if (!aBodyNode || !aSelection) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMNode>firstChild, lastChild;
  nsresult result = aBodyNode->GetFirstChild(getter_AddRefs(firstChild));
  if ((NS_SUCCEEDED(result)) && firstChild)
  {
    result = aBodyNode->GetLastChild(getter_AddRefs(lastChild));
    if ((NS_SUCCEEDED(result)) && lastChild)
    {
      aSelection->Collapse(firstChild, 0, SELECTION_NORMAL);
      nsCOMPtr<nsIDOMCharacterData>text = do_QueryInterface(lastChild);
      if (text)
      {
        PRUint32 length;
        text->GetLength(&length);
        aSelection->Extend(lastChild, length, SELECTION_NORMAL);
      }
    }
  }
  return result;
}

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsGfxTextControlFrame::IsSingleLineTextControl() const
{
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT==type) || (NS_FORM_INPUT_PASSWORD==type)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsGfxTextControlFrame::IsPlainTextControl() const
{
  // need to check HTML attribute of mContent and/or CSS.
  return PR_TRUE;
}

PRBool nsGfxTextControlFrame::IsPasswordTextControl() const
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_PASSWORD==type) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

// so we don't have to keep an extra flag around, just see if
// we've allocated the key listener or not.
PRBool nsGfxTextControlFrame::IsInitialized() const
{
  return (PRBool)(nsnull!=mEditor.get() && nsnull!=mKeyListener.get());
}

NS_IMETHODIMP
nsGfxTextControlFrame::InstallEditor()
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (mEditor)
  {
    nsCOMPtr<nsIPresShell> presShell;
    result = GetPresShellFor(mWebShell, getter_AddRefs(presShell));
    if (NS_FAILED(result)) { return result; }
    if (!presShell) { return NS_ERROR_NULL_POINTER; }

    // set the scrolling behavior
    if (PR_TRUE==IsSingleLineTextControl())
    {
      nsCOMPtr<nsIViewManager> vm;
      presShell->GetViewManager(getter_AddRefs(vm));
      if (vm) 
      {
        nsIScrollableView *sv=nsnull;
        vm->GetRootScrollableView(&sv);
        if (sv) {
          sv->SetScrollPreference(nsScrollPreference_kNeverScroll);
          // views are not refcounted
        }
      }
    }

    nsCOMPtr<nsIDOMDocument> domDoc; 
    nsCOMPtr<nsIDocument> doc; 
    presShell->GetDocument(getter_AddRefs(doc));
    NS_ASSERTION(doc, "null document");
    if (!doc) { return NS_ERROR_NULL_POINTER; }
    domDoc = do_QueryInterface(doc);
    if (!domDoc) { return NS_ERROR_NULL_POINTER; }

    if (mDocObserver) {
      doc->AddObserver(mDocObserver);
    }

    // we need to hook up our key listener before the editor is initialized
    result = NS_NewEnderKeyListener(getter_AddRefs(mKeyListener));
    if (NS_SUCCEEDED(result) && mKeyListener)
    {
      mKeyListener->SetFrame(this);
      // get the DOM event receiver
      nsCOMPtr<nsIDOMEventReceiver> er;
      result = domDoc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(er));
      if (!er) { result = NS_ERROR_NULL_POINTER; }
      if (NS_SUCCEEDED(result)) {
        result = er->AddEventListenerByIID(mKeyListener, kIDOMKeyListenerIID);
      }
      if (NS_FAILED(result)) { // either we couldn't get er, or we couldn't add the listener
        mKeyListener = do_QueryInterface(nsnull); // null out the listener, it's useless
      }
    }

    result = mEditor->Init(domDoc, presShell);
    if (NS_SUCCEEDED(result)) {
      result = InitializeTextControl(presShell, domDoc);
    }
  }
  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::InitializeTextControl(nsIPresShell *aPresShell, nsIDOMDocument *aDoc)
{
  nsresult result;
  if (!aPresShell || !aDoc) { return NS_ERROR_NULL_POINTER; }

  nsIFrame *htmlFrame;
  nsAutoString htmlTag("body");
  result = GetFirstFrameForType(htmlTag, aPresShell, aDoc, &htmlFrame);
  if (NS_SUCCEEDED(result) && htmlFrame)
  {
    PRInt32 type;
    GetType(&type);

    nsCOMPtr<nsIPresContext>presContext;
    aPresShell->GetPresContext(getter_AddRefs(presContext));
    NS_ASSERTION(presContext, "null presentation context");
    if (!presContext) { return NS_ERROR_NULL_POINTER; }

    /* this is the code for setting the pres context, which is MUCH better
     * than grabbing the imbedded HTML frame and setting it.
     * alas, the mechanisms for doing this are not yet in place.
     * what I want to do is hand the webshell my pres context at creation
     */
    /*
    nsFont font(presContext->GetDefaultFixedFontDeprecated()); 
    GetFont(presContext, font);
    const nsStyleFont* controlFont;
    GetStyleData(eStyleStruct_Font,  (const nsStyleStruct *&)controlFont);
    presContext->SetDefaultFont(font);
    presContext->SetDefaultFixedFont(font);

    const nsStyleColor* controlColor;
    GetStyleData(eStyleStruct_Color,  (const nsStyleStruct *&)controlColor);
    presContext->SetDefaultColor(controlColor->mColor);
    presContext->SetDefaultBackgroundColor(controlColor->mBackgroundColor);
    presContext->SetDefaultBackgroundImageRepeat(controlColor->mBackgroundRepeat);
    presContext->SetDefaultBackgroundImageAttachment(controlColor->mBackgroundAttachment);
    presContext->SetDefaultBackgroundImageOffset(controlColor->mBackgroundXPosition, controlColor->mBackgroundYPosition);
    presContext->SetDefaultBackgroundImage(controlColor->mBackgroundImage);
    */

    nsIStyleContext* bodySC = nsnull;
    htmlFrame->GetStyleContext(&bodySC);
    nsStyleSpacing* bodySpacing = nsnull;
    bodySpacing = (nsStyleSpacing*)bodySC->GetMutableStyleData(eStyleStruct_Spacing);
    nsStyleCoord zero(0);
    bodySpacing->mMargin.SetLeft(zero);
    bodySpacing->mMargin.SetRight(zero);
    bodySpacing->mMargin.SetTop(zero);
    bodySpacing->mMargin.SetBottom(zero);
    bodySpacing->mPadding.SetLeft(zero);
    bodySpacing->mPadding.SetRight(zero);
    bodySpacing->mPadding.SetTop(zero);
    bodySpacing->mPadding.SetBottom(zero);
    bodySpacing->mBorder.SetLeft(zero);
    bodySpacing->mBorder.SetRight(zero);
    bodySpacing->mBorder.SetTop(zero);
    bodySpacing->mBorder.SetBottom(zero);

    nsStylePosition* bodyPosition = nsnull;
    bodyPosition = (nsStylePosition*)bodySC->GetMutableStyleData(eStyleStruct_Position);
    const nsStylePosition* controlPosition;
    GetStyleData(eStyleStruct_Position,  (const nsStyleStruct *&)controlPosition);
    *bodyPosition = *controlPosition;
/* qqq
    if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type) {
      bodyPosition->mWidth.SetIntValue(10000, eStyleUnit_Chars);
    }
*/

    nsStyleFont* bodyFont = nsnull;
    bodyFont = (nsStyleFont*)bodySC->GetMutableStyleData(eStyleStruct_Font);
    //const nsStyleFont* controlFont;
    //GetStyleData(eStyleStruct_Font,  (const nsStyleStruct *&)controlFont);
    //*bodyFont = *controlFont;
    nsFont font(presContext->GetDefaultFixedFontDeprecated()); 
    GetFont(presContext, font);
    bodyFont->mFont = font;
    bodyFont->mFixedFont = font;

    nsStyleColor* bodyColor = nsnull;
    bodyColor = (nsStyleColor*)bodySC->GetMutableStyleData(eStyleStruct_Color);
    const nsStyleColor* controlColor;
    GetStyleData(eStyleStruct_Color,  (const nsStyleStruct *&)controlColor);
    *bodyColor = *controlColor;

    nsStyleText* bodyText = nsnull;
    bodyText = (nsStyleText*)bodySC->GetMutableStyleData(eStyleStruct_Text);
    const nsStyleText* controlText;
    GetStyleData(eStyleStruct_Text,  (const nsStyleStruct *&)controlText);
    *bodyText = *controlText;
    if (NS_FORM_INPUT_TEXT == type || NS_FORM_INPUT_PASSWORD == type) {
      // qqq bodyText->mWhiteSpace = NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
      bodyText->mWhiteSpace = NS_STYLE_WHITESPACE_PRE;
    }
    bodyText->mTextAlign = NS_STYLE_TEXT_ALIGN_LEFT;

    bodySC->RecalcAutomaticData(presContext);
    NS_RELEASE(bodySC);

    // now that the style context is initialized, initialize the content
    nsAutoString value;
    if (PR_TRUE == IsSingleLineTextControl()) 
    {
#ifdef SingleSignon
      // get name of text 
      PRBool failed = PR_TRUE;
      nsAutoString name;
      GetName(&name);

      // get url name 
      char *URLName = nsnull;
      nsIURI* docURL = nsnull;
      nsIDocument* doc = nsnull;
      mContent->GetDocument(doc);
      if (nsnull != doc) {
        docURL = doc->GetDocumentURL();
        NS_RELEASE(doc);
        if (nsnull != docURL) {
#ifdef NECKO
          char* spec;
#else
          const char* spec;
#endif
          (void)docURL->GetSpec(&spec);
          if (nsnull != spec) {
            URLName = (char*)PR_Malloc(PL_strlen(spec)+1);
            PL_strcpy(URLName, spec);
          }
#ifdef NECKO
          nsCRT::free(spec);
#endif
          NS_RELEASE(docURL);
        }
      }

      if (nsnull != URLName) {
        // invoke single-signon to get previously-used value of text 
        nsIWalletService *service;
        nsresult res = nsServiceManager::GetService(kWalletServiceCID,
                                                    kIWalletServiceIID,
                                                    (nsISupports **)&service);
        if ((NS_OK == res) && (nsnull != service)) {
          char* valueString = NULL;
          char* nameString = name.ToNewCString();
          res = service->SI_RestoreSignonData(URLName, nameString, &valueString);
          delete[] nameString;
          NS_RELEASE(service);
          PR_FREEIF(URLName);
          if (valueString && *valueString) {
            value = valueString;
            failed = PR_FALSE;
          }
        }
      }
      if (failed) {
        GetText(&value, PR_TRUE);
      }
#else
      GetText(&value, PR_TRUE);
#endif
    }
    else {
      GetText(&value, PR_TRUE);
    }
    mEditor->EnableUndo(PR_FALSE);

    PRInt32 maxLength;
    result = GetMaxLength(&maxLength);
    if (NS_CONTENT_ATTR_NOT_THERE != result) {
      mEditor->SetMaxTextLength(maxLength);
    }

    nsCOMPtr<nsIEditor>editor = do_QueryInterface(mEditor);
    NS_ASSERTION(editor, "bad QI to nsIEditor from mEditor");
    if (editor)
    {
      nsCOMPtr<nsIDOMSelection>selection;
      result = editor->GetSelection(getter_AddRefs(selection));
      if (NS_SUCCEEDED(result) && selection)
      {
        nsCOMPtr<nsIDOMNode>bodyNode;
        nsAutoString bodyTag = "body";
        result = GetFirstNodeOfType(bodyTag, aDoc, getter_AddRefs(bodyNode));
        if (NS_SUCCEEDED(result) && bodyNode)
        {
          result = SelectAllTextContent(bodyNode, selection);
          if (NS_SUCCEEDED(result))
          {
            if (0!=value.Length())
            {
              result = mEditor->InsertText(value);
              result = SelectAllTextContent(bodyNode, selection);
            }
            selection->ClearSelection(SELECTION_NORMAL);
          }
        }
      }
    }


    /*
    SetColors(*aPresContext);

    if (nsFormFrame::GetDisabled(this)) {
      mWidget->Enable(PR_FALSE);
    }
  */

    // finish initializing editor
    mEditor->EnableUndo(PR_TRUE);

    // set readonly and disabled states
    if (mContent)
    {
      PRUint32 flags=0;
      if (IsPlainTextControl()) {
        flags |= TEXT_EDITOR_FLAG_PLAINTEXT;
      }
      if (IsSingleLineTextControl()) {
        flags |= TEXT_EDITOR_FLAG_SINGLELINE;
      }
      if (IsPasswordTextControl()) {
        flags |= TEXT_EDITOR_FLAG_PASSWORD;
      }
      nsCOMPtr<nsIContent> content;
      result = mContent->QueryInterface(nsIContent::GetIID(), getter_AddRefs(content));
      if (NS_SUCCEEDED(result) && content)
      {
        PRInt32 nameSpaceID;
        content->GetNameSpaceID(nameSpaceID);
        nsAutoString resultValue;
        result = content->GetAttribute(nameSpaceID, nsHTMLAtoms::readonly, resultValue);
        if (NS_CONTENT_ATTR_NOT_THERE != result) {
          flags |= TEXT_EDITOR_FLAG_READONLY;
          aPresShell->SetCaretEnabled(PR_FALSE);
        }
        result = content->GetAttribute(nameSpaceID, nsHTMLAtoms::disabled, resultValue);
        if (NS_CONTENT_ATTR_NOT_THERE != result) 
        {
          flags |= TEXT_EDITOR_FLAG_DISABLED;
          aPresShell->SetCaretEnabled(PR_FALSE);
          nsCOMPtr<nsIDocument>doc = do_QueryInterface(aDoc);
          if (doc) {
            doc->SetDisplaySelection(PR_FALSE);
          }
        }
      }
      mEditor->SetFlags(flags);
    }
  }
  else { result = NS_ERROR_NULL_POINTER; }
  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::InternalContentChanged()
{
  if (!IsInitialized()) { return NS_ERROR_NOT_INITIALIZED; }
  nsAutoString textValue;
  // XXX: need to check here if we're an HTML edit field or a text edit field
  nsString format ("text/html");
  mEditor->OutputToString(textValue, format, 0);
  if (IsSingleLineTextControl()) {
    RemoveNewlines(textValue); 
  }
  SetTextControlFrameState(textValue);   // set new text value
  return NS_OK;
}

void nsGfxTextControlFrame::RemoveNewlines(nsString &aString)
{
  // strip CR/LF
  static const char badChars[] = {10, 13, 0};
  aString.StripChars(badChars);
}


/*******************************************************************************
 * EnderFrameLoadingInfo
 ******************************************************************************/
class EnderFrameLoadingInfo : public nsISupports
{
public:
  EnderFrameLoadingInfo(const nsSize& aSize);

  // nsISupports interface...
  NS_DECL_ISUPPORTS

protected:
  virtual ~EnderFrameLoadingInfo() {}

public:
  nsSize mFrameSize;
};

EnderFrameLoadingInfo::EnderFrameLoadingInfo(const nsSize& aSize)
{
  NS_INIT_REFCNT();

  mFrameSize = aSize;
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ISUPPORTS(EnderFrameLoadingInfo,kISupportsIID);


/*******************************************************************************
 * nsEnderDocumentObserver
 ******************************************************************************/
NS_IMPL_ADDREF(nsEnderDocumentObserver);
NS_IMPL_RELEASE(nsEnderDocumentObserver);


nsresult
nsEnderDocumentObserver::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIDocumentObserver*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



NS_IMETHODIMP 
nsEnderDocumentObserver::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame = aFrame;
  return NS_OK;
}

NS_IMETHODIMP nsEnderDocumentObserver::BeginUpdate(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::EndUpdate(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::BeginLoad(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::EndLoad(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::EndReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{ return NS_OK; }

NS_IMETHODIMP nsEnderDocumentObserver::ContentChanged(nsIDocument *aDocument,
                                                      nsIContent* aContent,
                                                      nsISupports* aSubContent)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::ContentStatesChanged(nsIDocument* aDocument,
                                                            nsIContent* aContent1,
                                                            nsIContent* aContent2)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::AttributeChanged(nsIDocument *aDocument,
                                                        nsIContent*  aContent,
                                                        nsIAtom*     aAttribute,
                                                        PRInt32      aHint)
{ return NS_OK; }

NS_IMETHODIMP nsEnderDocumentObserver::ContentAppended(nsIDocument *aDocument,
                                                       nsIContent* aContainer,
                                                       PRInt32     aNewIndexInContainer)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::ContentInserted(nsIDocument *aDocument,
                                                       nsIContent* aContainer,
                                                       nsIContent* aChild,
                                                       PRInt32 aIndexInContainer)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::ContentReplaced(nsIDocument *aDocument,
                                                       nsIContent* aContainer,
                                                       nsIContent* aOldChild,
                                                       nsIContent* aNewChild,
                                                       PRInt32 aIndexInContainer)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::ContentRemoved(nsIDocument *aDocument,
                                                      nsIContent* aContainer,
                                                      nsIContent* aChild,
                                                      PRInt32 aIndexInContainer)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::StyleSheetAdded(nsIDocument *aDocument,
                                                       nsIStyleSheet* aStyleSheet)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleSheetRemoved(nsIDocument *aDocument,
                                                         nsIStyleSheet* aStyleSheet)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                                                      nsIStyleSheet* aStyleSheet,
                                                                      PRBool aDisabled)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleRuleChanged(nsIDocument *aDocument,
                                                        nsIStyleSheet* aStyleSheet,
                                                        nsIStyleRule* aStyleRule,
                                                        PRInt32 aHint)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleRuleAdded(nsIDocument *aDocument,
                                                      nsIStyleSheet* aStyleSheet,
                                                      nsIStyleRule* aStyleRule)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleRuleRemoved(nsIDocument *aDocument,
                                                        nsIStyleSheet* aStyleSheet,
                                                        nsIStyleRule* aStyleRule)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::DocumentWillBeDestroyed(nsIDocument *aDocument)
{ return NS_OK; }



/*******************************************************************************
 * nsEnderKeyListener
 ******************************************************************************/

nsresult 
NS_NewEnderKeyListener(nsEnderKeyListener ** aInstancePtrResult)
{
  nsEnderKeyListener* it = new nsEnderKeyListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIDOMKeyListenerIID, (void **) aInstancePtrResult);   
}

NS_IMPL_ADDREF(nsEnderKeyListener)

NS_IMPL_RELEASE(nsEnderKeyListener)

nsresult
nsEnderKeyListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMKeyListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsEnderKeyListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsEnderKeyListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  PRUint32 keyCode;

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aKeyEvent);
  if (!uiEvent) { //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

  if (NS_SUCCEEDED(uiEvent->GetKeyCode(&keyCode)))
  {
    if (nsIDOMUIEvent::VK_RETURN==keyCode) 
    {
      if (mFrame)
      {
        // need the pres context.  could save it from some other function, but that seems so wrong
        //mFrame->EnterPressed(presContext);
        return nsEventStatus_eConsumeNoDefault;
      }
    }
  }
  
  return nsEventStatus_eIgnore;
}

nsresult
nsEnderKeyListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return nsEventStatus_eIgnore;
}

nsresult
nsEnderKeyListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return nsEventStatus_eIgnore;
}






/*******************************************************************************
 * EnderTempObserver
 ******************************************************************************/
// XXX temp implementation

NS_IMPL_ADDREF(EnderTempObserver);
NS_IMPL_RELEASE(EnderTempObserver);

nsresult
EnderTempObserver::QueryInterface(const nsIID& aIID,
                                  void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIDocumentObserver*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


#ifndef NECKO
NS_IMETHODIMP
EnderTempObserver::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
  return NS_OK;
}
#endif

NS_IMETHODIMP
#ifdef NECKO
EnderTempObserver::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
#else
EnderTempObserver::OnStartRequest(nsIURI* aURL, const char *aContentType)
#endif
{
  return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
EnderTempObserver::OnStopRequest(nsIChannel* channel, nsISupports *ctxt, nsresult status,
                                 const PRUnichar *errorMsg)
#else
EnderTempObserver::OnStopRequest(nsIURI* aURL, nsresult status, const PRUnichar* aMsg)
#endif
{
  if (PR_TRUE==mFirstCall)
  {
    mFirstCall = PR_FALSE;
    if (mFrame) {
      mFrame->InstallEditor();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame = aFrame;
  return NS_OK;
}

