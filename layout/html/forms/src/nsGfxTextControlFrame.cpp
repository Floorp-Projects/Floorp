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
#include "nsIDocumentLoader.h"
#include "nsINameSpaceManager.h"
#include "nsIPref.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsIDocumentViewer.h"
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"

#include "nsIHTMLEditor.h"
#include "nsIDocumentEncoder.h"
#include "nsIEditorMailSupport.h"
#include "nsEditorCID.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMUIEvent.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"
#include "nsStyleUtil.h"

// for anonymous content and frames
#include "nsHTMLParts.h"
#include "nsITextContent.h"

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kTextCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENT_LOADER_OBSERVER_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);

static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);

#define EMPTY_DOCUMENT "about:blank"
#define PASSWORD_REPLACEMENT_CHAR '*'

//#define NOISY
const nscoord kSuggestedNotSet = -1;

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

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
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::Init(nsIPresContext&  aPresContext,
                            nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIStyleContext* aContext,
                            nsIFrame*        aPrevInFlow)
{
  mFramePresContext = &aPresContext;
  return (nsTextControlFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow));
}

NS_IMETHODIMP
nsGfxTextControlFrame::CreateEditor()
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

    // create the focus listener for HTML Input content
  if (mDisplayContent)
  {
    mFocusListenerForContent = new nsEnderFocusListenerForContent();
    if (!mFocusListenerForContent) { return NS_ERROR_OUT_OF_MEMORY; }
    mFocusListenerForContent->SetFrame(this);
    NS_ADDREF(mFocusListenerForContent);
    // get the DOM event receiver
    nsCOMPtr<nsIDOMEventReceiver> er;
    result = mDisplayContent->QueryInterface(nsIDOMEventReceiver::GetIID(), getter_AddRefs(er));
    if (NS_SUCCEEDED(result) && er) 
      result = er->AddEventListenerByIID(mFocusListenerForContent, nsIDOMFocusListener::GetIID());
    // should check to see if mDisplayContent or mContent has focus and call CreateSubDoc instead if it does
    // do something with result
  }

  nsCOMPtr<nsIHTMLEditor> theEditor;
  result = nsComponentManager::CreateInstance(kHTMLEditorCID,
                                              nsnull,
                                              nsIHTMLEditor::GetIID(), getter_AddRefs(theEditor));
  if (NS_FAILED(result)) { return result; }
  if (!theEditor) { return NS_ERROR_OUT_OF_MEMORY; }
  mEditor = do_QueryInterface(theEditor);
  if (!mEditor) { return NS_ERROR_NO_INTERFACE; }
  return NS_OK;
}

nsGfxTextControlFrame::nsGfxTextControlFrame()
: mWebShell(0), mCreatingViewer(PR_FALSE),
  mTempObserver(0), mDocObserver(0),
  mNotifyOnInput(PR_FALSE),
  mIsProcessing(PR_FALSE),
  mNeedsStyleInit(PR_TRUE),
  mFramePresContext(nsnull),
  mCachedState(nsnull),
  mWeakReferent(this),
  mDisplayFrame(nsnull)
{
}

nsGfxTextControlFrame::~nsGfxTextControlFrame()
{
  nsresult result;
  if (mDisplayFrame) {
    mDisplayFrame->Destroy(*mFramePresContext);
  }
  if (mTempObserver)
  {
    if (mWebShell) {
      nsCOMPtr<nsIDocumentLoader> docLoader;
      mWebShell->GetDocumentLoader(*getter_AddRefs(docLoader));
      if (docLoader) {
        docLoader->RemoveObserver(mTempObserver);
      }
    }
    mTempObserver->SetFrame(nsnull);
    NS_RELEASE(mTempObserver);
  }

  if (mDisplayContent && mFocusListenerForContent)
  {
    nsCOMPtr<nsIDOMEventReceiver> er;
    result = mContent->QueryInterface(nsIDOMEventReceiver::GetIID(), getter_AddRefs(er));
    if (NS_SUCCEEDED(result) && er) 
    {
      er->RemoveEventListenerByIID(mFocusListenerForContent, nsIDOMFocusListener::GetIID());
    }
    mFocusListenerForContent->SetFrame(nsnull);
    NS_RELEASE(mFocusListenerForContent);
  }  

  if (mEventListener)
  {
    if (mEditor)
    {
      nsCOMPtr<nsIDOMSelection>selection;
      result = mEditor->GetSelection(getter_AddRefs(selection));
      if (NS_SUCCEEDED(result) && selection) 
      {
        nsCOMPtr<nsIDOMSelectionListener>selListener;
        selListener = do_QueryInterface(mEventListener);
        if (selListener) {
          selection->RemoveSelectionListener(selListener); 
        }
      }
      nsCOMPtr<nsIDOMDocument>domDoc;
      result = mEditor->GetDocument(getter_AddRefs(domDoc));
      if (NS_SUCCEEDED(result) && domDoc)
      {
        nsCOMPtr<nsIDOMEventReceiver> er;
        result = domDoc->QueryInterface(nsIDOMEventReceiver::GetIID(), getter_AddRefs(er));
        if (NS_SUCCEEDED(result) && er) 
        {
          nsCOMPtr<nsIDOMKeyListener>keyListener;
          keyListener = do_QueryInterface(mEventListener);
          if (keyListener)
            er->RemoveEventListenerByIID(keyListener, nsIDOMKeyListener::GetIID());
          nsCOMPtr<nsIDOMMouseListener>mouseListener;
          mouseListener = do_QueryInterface(mEventListener);
          if (mouseListener)
            er->RemoveEventListenerByIID(mouseListener, nsIDOMMouseListener::GetIID());
          nsCOMPtr<nsIDOMFocusListener>focusListener;
          focusListener = do_QueryInterface(mEventListener);
          if (focusListener)
            er->RemoveEventListenerByIID(focusListener, nsIDOMFocusListener::GetIID());
        }
      }
    }
  }

  mEditor = 0;  // editor must be destroyed before the webshell!
  if (mWebShell) 
  {
    mWebShell->Destroy();    
    NS_RELEASE(mWebShell);
  }
  if (mDocObserver)
  {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDoc);
    if (doc)
    {
      doc->RemoveObserver(mDocObserver);
    }
    mDocObserver->SetFrame(nsnull);
    NS_RELEASE(mDocObserver);
  }

  if (mCachedState) {
    delete mCachedState;
    mCachedState = nsnull;
  }
}

NS_METHOD nsGfxTextControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                             nsGUIEvent* aEvent,
                                             nsEventStatus& aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  aEventStatus = nsEventStatus_eConsumeDoDefault;   // this is the default
  
  switch (aEvent->message) {
     case NS_MOUSE_LEFT_CLICK:
        MouseClicked(&aPresContext);
     break;

	   case NS_KEY_PRESS:
	    if (NS_KEY_EVENT == aEvent->eventStructType) {
	      nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
	      if (NS_VK_RETURN == keyEvent->keyCode) 
        {
	        EnterPressed(aPresContext);
          aEventStatus = nsEventStatus_eConsumeNoDefault;
	      }
	      else if (NS_VK_SPACE == keyEvent->keyCode) 
        {
	        MouseClicked(&aPresContext);
	      }
	    }
	    break;
  }
  return NS_OK;
}


void
nsGfxTextControlFrame::EnterPressed(nsIPresContext& aPresContext) 
{
  if (mFormFrame && mFormFrame->CanSubmit(*this)) {
    nsIContent *formContent = nsnull;

    nsEventStatus status = nsEventStatus_eIgnore;

    mFormFrame->GetContent(&formContent);
    if (nsnull != formContent) {
      nsEvent event;
      event.eventStructType = NS_EVENT;
      event.message = NS_FORM_SUBMIT;
      formContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
      NS_RELEASE(formContent);
    }

    if (nsEventStatus_eConsumeNoDefault != status) {
      mFormFrame->OnSubmit(&aPresContext, this);
    }
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
      if (PR_TRUE==IsInitialized())
      {
        if (mEditor)
        {
          nsString format ("text/plain");
          mEditor->OutputToString(*aText, format, 0);
        }
        // we've never built our editor, so the content attribute is the value
        else
        {
          result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
        }
      }
      else {
        if (mCachedState) {
          *aText = *mCachedState;
	        result = NS_OK;
	      } else {
          result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
	      }
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
                                        PRInt32         aNameSpaceID,
                                        nsIAtom*        aAttribute,
                                        PRInt32         aHint)
{
  if (PR_FALSE==IsInitialized()) {return NS_ERROR_NOT_INITIALIZED;}
  nsresult result = NS_OK;

  if (nsHTMLAtoms::value == aAttribute) 
  {
    if (mEditor)
    {
      nsString value;
      GetText(&value, PR_TRUE);           // get the initial value from the content attribute
      mEditor->EnableUndo(PR_FALSE);      // wipe out undo info
      SetTextControlFrameState(value);    // set new text value
      mEditor->EnableUndo(PR_TRUE);       // fire up a new txn stack
    }
    if (aHint != NS_STYLE_HINT_REFLOW)
      nsFormFrame::StyleChangeReflow(aPresContext, this);
  } 
  else if (nsHTMLAtoms::maxlength == aAttribute) 
  {
    PRInt32 maxLength;
    nsresult rv = GetMaxLength(&maxLength);
    
    nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
    if (htmlEditor)
    {
      if (NS_CONTENT_ATTR_NOT_THERE != rv) 
      {  // set the maxLength attribute
          htmlEditor->SetMaxTextLength(maxLength);
        // if maxLength>docLength, we need to truncate the doc content
      }
      else { // unset the maxLength attribute
          htmlEditor->SetMaxTextLength(-1);
      }
    }
  } 
  else if (mEditor && nsHTMLAtoms::readonly == aAttribute) 
  {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));     
    nsresult rv = DoesAttributeExist(nsHTMLAtoms::readonly);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= nsIHTMLEditor::eEditorReadonlyMask;
      presShell->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(nsIHTMLEditor::eEditorReadonlyMask);
      presShell->SetCaretEnabled(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (mEditor && nsHTMLAtoms::disabled == aAttribute) 
  {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));     
    nsresult rv = DoesAttributeExist(nsHTMLAtoms::disabled);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= nsIHTMLEditor::eEditorDisabledMask;
      presShell->SetCaretEnabled(PR_FALSE);
      nsCOMPtr<nsIDocument> doc; 
      presShell->GetDocument(getter_AddRefs(doc));
      NS_ASSERTION(doc, "null document");
      if (!doc) { return NS_ERROR_NULL_POINTER; }
      doc->SetDisplaySelection(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(nsIHTMLEditor::eEditorDisabledMask);
      presShell->SetCaretEnabled(PR_TRUE);
      nsCOMPtr<nsIDocument> doc; 
      presShell->GetDocument(getter_AddRefs(doc));
      NS_ASSERTION(doc, "null document");
      if (!doc) { return NS_ERROR_NULL_POINTER; }
      doc->SetDisplaySelection(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (nsHTMLAtoms::size == aAttribute && aHint != NS_STYLE_HINT_REFLOW) {
    nsFormFrame::StyleChangeReflow(aPresContext, this);
  }
  // Allow the base class to handle common attributes supported
  // by all form elements... 
  else {
    result = nsFormControlFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
  }

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

// aSizeOfSubdocContainer is in pixels, not twips
nsresult 
nsGfxTextControlFrame::CreateSubDoc(nsRect *aSizeOfSubdocContainer)
{
  if (!mFramePresContext) { return NS_ERROR_NULL_POINTER; }

  nsresult rv = NS_OK;

  if (!IsInitialized() && !mCreatingViewer)
  {
    // if the editor hasn't been created yet, create it
    if (!mEditor)
    {
      rv = CreateEditor();
      if (NS_FAILED(rv)) { return rv; }
      NS_ASSERTION(mEditor, "null EDITOR after attempt to create.");
    }

    // initialize the subdoc, if it hasn't already been constructed
    // if the size is not 0 and there is a src, create the web shell
  
    if (nsnull == mWebShell) 
    {
      nsSize  size;
      GetSize(size);
      nsRect subBounds;
      if (aSizeOfSubdocContainer)
      {
        subBounds = *aSizeOfSubdocContainer;
      }
      else
      {
        nsMargin borderPadding;
        borderPadding.SizeTo(0, 0, 0, 0);
        // Get the CSS border
        const nsStyleSpacing* spacing;
        GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
        spacing->CalcBorderPaddingFor(this, borderPadding);
        CalcSizeOfSubDocInTwips(borderPadding, size, subBounds);
        float t2p;
        mFramePresContext->GetTwipsToPixels(&t2p);
        subBounds.x = NSToCoordRound(subBounds.x * t2p);
        subBounds.y = NSToCoordRound(subBounds.y * t2p);
        subBounds.width = NSToCoordRound(subBounds.width * t2p);
        subBounds.height = NSToCoordRound(subBounds.height * t2p);
      }

      rv = CreateWebShell(*mFramePresContext, size);
      if (NS_FAILED(rv)) { return rv; }
      NS_ASSERTION(mWebShell, "null web shell after attempt to create.");
      if (!mWebShell) return NS_ERROR_NULL_POINTER;
      if (gNoisy) { printf("%p webshell in CreateSubDoc set to bounds: x=%d, y=%d, w=%d, h=%d\n", mWebShell, subBounds.x, subBounds.y, subBounds.width, subBounds.height); }
      mWebShell->SetBounds(subBounds.x, subBounds.y, subBounds.width, subBounds.height);
    }
    mCreatingViewer=PR_TRUE;
    nsAutoString url(EMPTY_DOCUMENT);
    rv = mWebShell->LoadURL(url.GetUnicode());  // URL string with a default nsnull value for post Data

    // force an incremental reflow of the text control
    /* XXX: this is to get the view/webshell positioned correctly.
            I don't know why it's required, it looks like this code positions the view and webshell
            exactly the same way reflow does, but reflow works and the code above does not
            when the text control is in a view which is scrolled.
    */
    nsCOMPtr<nsIReflowCommand> cmd;
    rv = NS_NewHTMLReflowCommand(getter_AddRefs(cmd), this, nsIReflowCommand::StyleChanged);
    if (NS_FAILED(rv)) { return rv; }
    if (!cmd) { return NS_ERROR_NULL_POINTER; }
    nsCOMPtr<nsIPresShell> shell;
    rv = mFramePresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(rv)) { return rv; }
    if (!shell) { return NS_ERROR_NULL_POINTER; }
    rv = shell->EnterReflowLock();
    if (NS_FAILED(rv)) { return rv; }
    rv = shell->AppendReflowCommand(cmd);
    // must do this next line regardless of result of AppendReflowCommand
    shell->ExitReflowLock();
  }
  return rv;
}

void nsGfxTextControlFrame::CalcSizeOfSubDocInTwips(const nsMargin &aBorderPadding, const nsSize &aFrameSize, nsRect &aSubBounds)
{

    // XXX: the point here is to make a single-line edit field as wide as it wants to be, 
    //      so it will scroll horizontally if the characters take up more space than the field
    aSubBounds.x      = aBorderPadding.left;
    aSubBounds.y      = aBorderPadding.top;
    aSubBounds.width  = (aFrameSize.width - (aBorderPadding.left + aBorderPadding.right));
    aSubBounds.height = (aFrameSize.height - (aBorderPadding.top + aBorderPadding.bottom));
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
nsGfxTextControlFrame::Reset(nsIPresContext* aPresContext) 
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
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) 
  {
    nsAutoString text(" ");
    nsRect rect(0, 0, mRect.width, mRect.height);
    PaintTextControl(aPresContext, aRenderingContext, aDirtyRect, 
                     text, mStyleContext, rect);
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
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) 
  {
    nsAutoString text(" ");
    nsRect rect(0, 0, mRect.width, mRect.height);
    PaintTextControl(aPresContext, aRenderingContext, aDirtyRect, 
                     text, mStyleContext, rect);
  }
}

// stolen directly from nsContainerFrame
void
nsGfxTextControlFrame::PaintChild(nsIPresContext&      aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect&        aDirtyRect,
                                  nsIFrame*            aFrame,
                                  nsFramePaintLayer    aWhichLayer)
{
  nsIView *pView;
  aFrame->GetView(&aPresContext, &pView);
  if (nsnull == pView) {
    nsRect kidRect;
    aFrame->GetRect(kidRect);
    nsFrameState state;
    aFrame->GetFrameState(&state);

    // Compute the constrained damage area; set the overlap flag to
    // PR_TRUE if any portion of the child frame intersects the
    // dirty rect.
    nsRect damageArea;
    PRBool overlap;
    if (NS_FRAME_OUTSIDE_CHILDREN & state) {
      // If the child frame has children that leak out of our box
      // then we don't constrain the damageArea to just the childs
      // bounding rect.
      damageArea = aDirtyRect;
      overlap = PR_TRUE;
    }
    else {
      // Compute the intersection of the dirty rect and the childs
      // rect (both are in our coordinate space). This limits the
      // damageArea to just the portion that intersects the childs
      // rect.
      overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
#ifdef NS_DEBUG
      if (!overlap && (0 == kidRect.width) && (0 == kidRect.height)) {
        overlap = PR_TRUE;
      }
#endif
    }

    if (overlap) {
      // Translate damage area into the kids coordinate
      // system. Translate rendering context into the kids
      // coordinate system.
      damageArea.x -= kidRect.x;
      damageArea.y -= kidRect.y;
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);

      // Paint the kid
      aFrame->Paint(aPresContext, aRenderingContext, damageArea, aWhichLayer);
      PRBool clipState;
      aRenderingContext.PopState(clipState);

#ifdef NS_DEBUG
      // Draw a border around the child
      if (nsIFrameDebug::GetShowFrameBorders() && !kidRect.IsEmpty()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(kidRect);
      }
#endif
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
    nsCompatibility mode;
    aPresContext.GetCompatibilityMode(&mode);
    const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)aStyleContext->GetStyleData(eStyleStruct_Spacing);
    PRIntn skipSides = 0;
    nsRect rect(0, 0, mRect.width, mRect.height);
    const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
	  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect,  *color, *mySpacing, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *mySpacing, aStyleContext, skipSides);
    
    if (!mWebShell)
    {
      if (mDisplayFrame) {
        PaintChild(aPresContext, aRenderingContext, aDirtyRect, mDisplayFrame, NS_FRAME_PAINT_LAYER_FOREGROUND);
        //mDisplayFrame->Paint(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FOREGROUND);
      }
    }
  }
}

//XXX: this needs to be fixed for HTML output
void nsGfxTextControlFrame::GetTextControlFrameState(nsString& aValue)
{
  aValue = "";  // initialize out param
  
  if (mEditor && PR_TRUE==IsInitialized()) 
  {
    nsString format ("text/plain");
    PRUint32 flags = 0;

    if (PR_TRUE==IsPlainTextControl()) {
      flags |= nsIDocumentEncoder::OutputBodyOnly;   // OutputNoDoctype if head info needed
    }

    nsFormControlHelper::nsHTMLTextWrap wrapProp;
    nsresult result = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
    if (NS_CONTENT_ATTR_NOT_THERE != result) 
    {
      if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard)
      {
        flags |= nsIDocumentEncoder::OutputFormatted;
      }
    }

    mEditor->OutputToString(aValue, format, flags);
  }
  // otherwise, just return our text attribute
  else {
    if (mCachedState) {
      aValue = *mCachedState;
    } else {
      nsFormControlHelper::GetInputElementValue(mContent, &aValue, PR_TRUE);
    }
  }
}     

void nsGfxTextControlFrame::SetTextControlFrameState(const nsString& aValue)
{
  if (mEditor && PR_TRUE==IsInitialized()) 
  {
    nsAutoString currentValue;
    nsString format ("text/plain");
    nsresult result = mEditor->OutputToString(currentValue, format, 0);
    if (PR_TRUE==IsSingleLineTextControl()) {
      RemoveNewlines(currentValue); 
    }
    if (PR_FALSE==currentValue.Equals(aValue))  // this is necessary to avoid infinite recursion
    {
      nsCOMPtr<nsIDOMSelection>selection;
      result = mEditor->GetSelection(getter_AddRefs(selection));
			if (NS_FAILED(result)) return;
			if (!selection) return;

      nsCOMPtr<nsIDOMDocument>domDoc;
      result = mEditor->GetDocument(getter_AddRefs(domDoc));
			if (NS_FAILED(result)) return;
			if (!domDoc) return;

      nsCOMPtr<nsIDOMNode> bodyNode;
      nsAutoString bodyTag = "body";
      result = GetFirstNodeOfType(bodyTag, domDoc, getter_AddRefs(bodyNode));
      SelectAllTextContent(bodyNode, selection);
      nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
			if (!htmlEditor) return;

			// get the flags, remove readonly and disabled, set the value, restore flags
			PRUint32 flags, savedFlags;
			mEditor->GetFlags(&savedFlags);
			flags = savedFlags;
			flags &= ~(nsIHTMLEditor::eEditorDisabledMask);
			flags &= ~(nsIHTMLEditor::eEditorReadonlyMask);
			mEditor->SetFlags(flags);
      htmlEditor->InsertText(aValue);
			mEditor->SetFlags(savedFlags);
    }
  }
  else {
    if (mCachedState) delete mCachedState;
    mCachedState = new nsString(aValue);
    NS_ASSERTION(mCachedState, "Error: new nsString failed!");
    //if (!mCachedState) rv = NS_ERROR_OUT_OF_MEMORY;
    if (mDisplayContent)
    {
      const PRUnichar *text = mCachedState->GetUnicode();
      PRInt32 len = mCachedState->Length();
      mDisplayContent->SetText(text, len, PR_TRUE);
      if (mContent)
      {
        nsIDocument *doc;
        mContent->GetDocument(doc);
        if (doc) 
        {
          doc->ContentChanged(mContent, nsnull);
          NS_RELEASE(doc);
        }
      }
    }
  }
}

NS_IMETHODIMP nsGfxTextControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue)
{
  if (PR_FALSE==mIsProcessing)
  {
    mIsProcessing = PR_TRUE;
    if (nsHTMLAtoms::value == aName) 
    {
      if (mEditor) {
        mEditor->EnableUndo(PR_FALSE);    // wipe out undo info
      }
      SetTextControlFrameState(aValue);   // set new text value
      if (mEditor)  {
        mEditor->EnableUndo(PR_TRUE);     // fire up a new txn stack
      }
    }
    else {
      return Inherited::SetProperty(aPresContext, aName, aValue);
    }
    mIsProcessing = PR_FALSE;
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
    return Inherited::GetProperty(aName, aValue);
  }

  return NS_OK;
}  

void nsGfxTextControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  nsresult result;
  if (!mWebShell)
  {
    result = CreateSubDoc(nsnull);
    if (NS_FAILED(result)) return;
  }

  if (aOn) {
    nsresult result = NS_OK;

    nsIContentViewer *viewer = nsnull;
    mWebShell->GetContentViewer(&viewer);
    if (viewer) {
      nsIDocumentViewer* docv = nsnull;
      viewer->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx = nsnull;
        docv->GetPresContext(cx);
        if (nsnull != cx) {
          nsIPresShell  *shell = nsnull;
          cx->GetShell(&shell);
          if (nsnull != shell) {
            nsIViewManager  *vm = nsnull;
            shell->GetViewManager(&vm);
            if (nsnull != vm) {
              nsIView *rootview = nsnull;
              vm->GetRootView(rootview);
              if (rootview) {
                nsIWidget* widget;
                rootview->GetWidget(widget);
                if (widget) {
                  result = widget->SetFocus();
                  NS_RELEASE(widget);
                }
              }
              NS_RELEASE(vm);
            }
            NS_RELEASE(shell);
          }
          NS_RELEASE(cx);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(viewer);
    }
  }
  else {
    mWebShell->RemoveFocus();
  }
}

/* --------------------- Ender methods ---------------------- */


nsresult
nsGfxTextControlFrame::CreateWebShell(nsIPresContext& aPresContext,
                                      const nsSize& aSize)
{
  nsresult rv;

  rv = nsComponentManager::CreateInstance(kWebShellCID, nsnull, kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "could not create web widget");
    return rv;
  }
  
  // pass along marginwidth, marginheight, scrolling so sub document can use it
  mWebShell->SetMarginWidth(0);
  mWebShell->SetMarginHeight(0);
 
  /* our parent must be a webshell.  we need to get our prefs from our parent */
  nsCOMPtr<nsISupports> container;
  aPresContext.GetContainer(getter_AddRefs(container));
  NS_ASSERTION(container, "bad container");
  if (!container) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIWebShell> outerShell = do_QueryInterface(container);
  NS_ASSERTION(outerShell, "parent must be a webshell");
  if (!outerShell) return NS_ERROR_UNEXPECTED;

  nsIPref* outerPrefs = nsnull;  // connect the prefs
  outerShell->GetPrefs(outerPrefs);
  NS_ASSERTION(outerPrefs, "no prefs");
  if (!outerPrefs) return NS_ERROR_UNEXPECTED;

  mWebShell->SetPrefs(outerPrefs);
  NS_RELEASE(outerPrefs);

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
  GetOffsetFromView(&aPresContext, origin, &parView);  

  nsRect viewBounds(origin.x, origin.y, aSize.width, aSize.height);
  if (gNoisy) { printf("%p view bounds: x=%d, y=%d, w=%d, h=%d\n", view, origin.x, origin.y, aSize.width, aSize.height); }

  nsCOMPtr<nsIViewManager> viewMan;
  presShell->GetViewManager(getter_AddRefs(viewMan));  
  rv = view->Init(viewMan, viewBounds, parView);
  viewMan->InsertChild(parView, view, 0);
  rv = view->CreateWidget(kCChildCID);
  SetView(&aPresContext, view);

  // if the visibility is hidden, reflect that in the view
  const nsStyleDisplay* display;
  GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
  if (NS_STYLE_VISIBILITY_VISIBLE != display->mVisible) {
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
  NS_RELEASE(widget);

  nsCOMPtr<nsIDocumentLoader> docLoader;
  mWebShell->GetDocumentLoader(*getter_AddRefs(docLoader));
  if (docLoader) {
    docLoader->AddObserver(mTempObserver);
  }
  mWebShell->Show();
  return NS_OK;
}


PRInt32
nsGfxTextControlFrame::CalculateSizeNavQuirks (nsIPresContext*       aPresContext, 
                                              nsIRenderingContext*  aRendContext,
                                              nsIFormControlFrame*  aFrame,
                                              const nsSize&         aCSSSize, 
                                              nsInputDimensionSpec& aSpec, 
                                              nsSize&               aDesiredSize, 
                                              nsSize&               aMinSize, 
                                              PRBool&               aWidthExplicit, 
                                              PRBool&               aHeightExplicit, 
                                              nscoord&              aRowHeight,
                                              nsMargin&             aBorderPadding) 
{
  nscoord charWidth   = 0; 
  aWidthExplicit      = PR_FALSE;
  aHeightExplicit     = PR_FALSE;

  aDesiredSize.width  = CSS_NOTSET;
  aDesiredSize.height = CSS_NOTSET;

  // Quirks does not use rowAttr
  nsHTMLValue colAttr;
  nsresult    colStatus;
  nsHTMLValue rowAttr;
  nsresult    rowStatus;
  if (NS_ERROR_FAILURE == GetColRowSizeAttr(aFrame, 
                                            aSpec.mColSizeAttr, colAttr, colStatus,
                                            aSpec.mRowSizeAttr, rowAttr, rowStatus)) {
    return 0;
  }

  // Get the Font Metrics for the Control
  // without it we can't calculate  the size
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsFormControlHelper::GetFrameFontFM(*aPresContext, aFrame, getter_AddRefs(fontMet));
  if (fontMet) {
    aRendContext->SetFont(fontMet);

    // Figure out the number of columns
    // and set that as the default col size
    if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
      PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
      col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
      aSpec.mColDefaultSize = col;
    }
    charWidth = nsFormControlHelper::CalcNavQuirkSizing(*aPresContext, 
                                                        aRendContext, fontMet, 
                                                        aFrame, aSpec, aDesiredSize);
    aMinSize.width = aDesiredSize.width;

    // If COLS was not set then check to see if CSS has the width set
    if (NS_CONTENT_ATTR_HAS_VALUE != colStatus) {  // col attr will provide width
      if (CSS_NOTSET != aCSSSize.width) {  // css provides width
        NS_ASSERTION(aCSSSize.width >= 0, "form control's computed width is < 0"); 
        if (NS_INTRINSICSIZE != aCSSSize.width) {
          aDesiredSize.width = aCSSSize.width;
          aDesiredSize.width += aBorderPadding.left + aBorderPadding.right;
          aWidthExplicit = PR_TRUE;
        }
      }
    }

    aDesiredSize.height = aDesiredSize.height * aSpec.mRowDefaultSize;
    if (CSS_NOTSET != aCSSSize.height) {  // css provides height
      NS_ASSERTION(aCSSSize.height > 0, "form control's computed height is <= 0"); 
      if (NS_INTRINSICSIZE != aCSSSize.height) {
        aDesiredSize.height = aCSSSize.height;
        aDesiredSize.height += aBorderPadding.top + aBorderPadding.bottom;
        aHeightExplicit = PR_TRUE;
      }
    }

  } else {
    NS_ASSERTION(fontMet, "Couldn't get Font Metrics"); 
    aDesiredSize.width = 300;  // arbitrary values
    aDesiredSize.width = 1500;
  }

  aRowHeight      = aDesiredSize.height;
  aMinSize.height = aDesiredSize.height;

  PRInt32 numRows = (aRowHeight > 0) ? (aDesiredSize.height / aRowHeight) : 0;

  return numRows;
}

//------------------------------------------------------------------
NS_IMETHODIMP
nsGfxTextControlFrame::ReflowNavQuirks(nsIPresContext& aPresContext,
                                        nsHTMLReflowMetrics& aDesiredSize,
                                        const nsHTMLReflowState& aReflowState,
                                        nsReflowStatus& aStatus,
                                        nsMargin& aBorderPadding)
{
  nsMargin borderPadding;
  borderPadding.SizeTo(0, 0, 0, 0);
  // Get the CSS border
  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);

  // This calculates the reflow size
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(aPresContext, aReflowState, styleSize);

  nsSize desiredSize;
  nsSize minSize;
  
  PRBool widthExplicit, heightExplicit;
  PRInt32 ignore;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    PRInt32 width = 0;
    if (NS_CONTENT_ATTR_HAS_VALUE != GetSizeFromContent(&width)) {
      width = GetDefaultColumnWidth();
    }
    nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull,
                                  nsnull, width, 
                                  PR_FALSE, nsnull, 1);
    CalculateSizeNavQuirks(&aPresContext, aReflowState.rendContext, this, styleSize, 
                           textSpec, desiredSize, minSize, widthExplicit, 
                           heightExplicit, ignore, aBorderPadding);
  } else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, 
                                  nsnull, GetDefaultColumnWidth(), 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSizeNavQuirks(&aPresContext, aReflowState.rendContext, this, styleSize, 
                           areaSpec, desiredSize, minSize, widthExplicit, 
                           heightExplicit, ignore, aBorderPadding);
  }

  aDesiredSize.width   = desiredSize.width;
  aDesiredSize.height  = desiredSize.height;
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = minSize.width;
    aDesiredSize.maxElementSize->height = minSize.height;
  }

  // In Nav Quirks mode we only add in extra size for padding
  nsMargin padding;
  padding.SizeTo(0, 0, 0, 0);
  spacing->CalcPaddingFor(this, padding);

  aDesiredSize.width  += padding.left + padding.right;
  aDesiredSize.height += padding.top + padding.bottom;

  // Check to see if style was responsible 
  // for setting the height or the width
  PRBool addBorder = PR_FALSE;
  PRInt32 width;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetSizeFromContent(&width)) {
    addBorder = (width < GetDefaultColumnWidth());
  }

  if (addBorder) {
    if (CSS_NOTSET != styleSize.width || 
        CSS_NOTSET != styleSize.height) {  // css provides width
      nsMargin border;
      border.SizeTo(0, 0, 0, 0);
      spacing->CalcBorderFor(this, border);
      if (CSS_NOTSET != styleSize.width) {  // css provides width
        aDesiredSize.width  += border.left + border.right;
      }
      if (CSS_NOTSET != styleSize.height) {  // css provides heigth
        aDesiredSize.height += border.top + border.bottom;
      }
    }
  }
  return NS_OK;
}

nsresult 
nsGfxTextControlFrame::GetColRowSizeAttr(nsIFormControlFrame*  aFrame,
                                         nsIAtom *     aColSizeAttr,
                                         nsHTMLValue & aColSize,
                                         nsresult &    aColStatus,
                                         nsIAtom *     aRowSizeAttr,
                                         nsHTMLValue & aRowSize,
                                         nsresult &    aRowStatus)
{
  nsIContent* iContent = nsnull;
  aFrame->GetFormContent((nsIContent*&) iContent);
  if (!iContent) {
    return NS_ERROR_FAILURE;
  }
  nsIHTMLContent* hContent = nsnull;
  nsresult result = iContent->QueryInterface(kIHTMLContentIID, (void**)&hContent);
  if ((NS_OK != result) || !hContent) {
    NS_RELEASE(iContent);
    return NS_ERROR_FAILURE;
  }

  aColStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aColSizeAttr) {
    aColStatus = hContent->GetHTMLAttribute(aColSizeAttr, aColSize);
  }

  aRowStatus= NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aRowSizeAttr) {
    aRowStatus = hContent->GetHTMLAttribute(aRowSizeAttr, aRowSize);
  }

  NS_RELEASE(hContent);
  NS_RELEASE(iContent);
  
  return NS_OK;
}

PRInt32
nsGfxTextControlFrame::CalculateSizeStandard (nsIPresContext*       aPresContext, 
                                              nsIRenderingContext*  aRendContext,
                                              nsIFormControlFrame*  aFrame,
                                              const nsSize&         aCSSSize, 
                                              nsInputDimensionSpec& aSpec, 
                                              nsSize&               aDesiredSize, 
                                              nsSize&               aMinSize, 
                                              PRBool&               aWidthExplicit, 
                                              PRBool&               aHeightExplicit, 
                                              nscoord&              aRowHeight,
                                              nsMargin&             aBorderPadding) 
{
  nscoord charWidth   = 0; 
  aWidthExplicit      = PR_FALSE;
  aHeightExplicit     = PR_FALSE;

  aDesiredSize.width  = CSS_NOTSET;
  aDesiredSize.height = CSS_NOTSET;

  nsHTMLValue colAttr;
  nsresult    colStatus;
  nsHTMLValue rowAttr;
  nsresult    rowStatus;
  if (NS_ERROR_FAILURE == GetColRowSizeAttr(aFrame, 
                                            aSpec.mColSizeAttr, colAttr, colStatus,
                                            aSpec.mRowSizeAttr, rowAttr, rowStatus)) {
    return 0;
  }

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);

  // determine the width, char height, row height
  if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
    PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
    col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
    charWidth = nsFormControlHelper::GetTextSize(*aPresContext, aFrame, col, aDesiredSize, aRendContext);
    aMinSize.width = aDesiredSize.width;
    aDesiredSize.width += aBorderPadding.left + aBorderPadding.right;
  } else {
    charWidth = nsFormControlHelper::GetTextSize(*aPresContext, aFrame, aSpec.mColDefaultSize, aDesiredSize, aRendContext); 
    aMinSize.width = aDesiredSize.width;
    if (CSS_NOTSET != aCSSSize.width) {  // css provides width
      NS_ASSERTION(aCSSSize.width >= 0, "form control's computed width is < 0"); 
      if (NS_INTRINSICSIZE != aCSSSize.width) {
        aDesiredSize.width = aCSSSize.width;
        aDesiredSize.width += aBorderPadding.left + aBorderPadding.right;
        aWidthExplicit = PR_TRUE;
      }
    } else {
      aDesiredSize.width += aBorderPadding.left + aBorderPadding.right;
    }
  }

  nscoord fontHeight  = 0;
  nscoord fontLeading = 0;
  // get leading
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsFormControlHelper::GetFrameFontFM(*aPresContext, aFrame, getter_AddRefs(fontMet));
  if (fontMet) {
    aRendContext->SetFont(fontMet);
    fontMet->GetHeight(fontHeight);
    fontMet->GetLeading(fontLeading);
    aDesiredSize.height += fontLeading;
  }
  aRowHeight      = aDesiredSize.height;
  aMinSize.height = aDesiredSize.height;
  PRInt32 numRows = 0;

  if (NS_CONTENT_ATTR_HAS_VALUE == rowStatus) { // row attr will provide height
    PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) 
                            ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
    numRows = (rowAttrInt > 0) ? rowAttrInt : 1;
    aDesiredSize.height = aDesiredSize.height * numRows;
    aDesiredSize.height += aBorderPadding.top + aBorderPadding.bottom;
  } else {
    aDesiredSize.height = aDesiredSize.height * aSpec.mRowDefaultSize;
    if (CSS_NOTSET != aCSSSize.height) {  // css provides height
      NS_ASSERTION(aCSSSize.height > 0, "form control's computed height is <= 0"); 
      if (NS_INTRINSICSIZE != aCSSSize.height) {
        aDesiredSize.height = aCSSSize.height;
        aDesiredSize.height += aBorderPadding.top + aBorderPadding.bottom;
        aHeightExplicit = PR_TRUE;
      }
    } else {
      aDesiredSize.height += aBorderPadding.top + aBorderPadding.bottom;
    }
  }

  numRows = (aRowHeight > 0) ? (aDesiredSize.height / aRowHeight) : 0;
  if (numRows == 1) {
    PRInt32 type;
    GetType(&type);
    if (NS_FORM_TEXTAREA == type) {
      aDesiredSize.height += fontHeight;
    }
  }

  return numRows;
}

NS_IMETHODIMP
nsGfxTextControlFrame::ReflowStandard(nsIPresContext& aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      const nsHTMLReflowState& aReflowState,
                                      nsReflowStatus& aStatus,
                                      nsMargin& aBorderPadding)
{
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(aPresContext, aReflowState, styleSize);

  nsSize desiredSize;
  nsSize minSize;
  
  PRBool widthExplicit, heightExplicit;
  PRInt32 ignore;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    PRInt32 width = 0;
    if (NS_CONTENT_ATTR_HAS_VALUE != GetSizeFromContent(&width)) {
      width = GetDefaultColumnWidth();
    }
    nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull,
                                  nsnull, width, 
                                  PR_FALSE, nsnull, 1);
    CalculateSizeStandard(&aPresContext, aReflowState.rendContext, this, styleSize, 
                           textSpec, desiredSize, minSize, widthExplicit, 
                           heightExplicit, ignore, aBorderPadding);
  } else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, 
                                  nsnull, GetDefaultColumnWidth(), 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSizeStandard(&aPresContext, aReflowState.rendContext, this, styleSize, 
                           areaSpec, desiredSize, minSize, widthExplicit, 
                           heightExplicit, ignore, aBorderPadding);
  }

  // CalculateSize makes calls in the nsFormControlHelper that figures
  // out the entire size of the control when in NavQuirks mode. For the
  // textarea, this means the scrollbar sizes hav already been added to
  // its overall size and do not need to be added here.
  if (NS_FORM_TEXTAREA == type) {
    float   p2t;
    aPresContext.GetPixelsToTwips(&p2t);

    nscoord scrollbarWidth  = 0;
    nscoord scrollbarHeight = 0;
    float   scale;
    nsCOMPtr<nsIDeviceContext> dx;
    aPresContext.GetDeviceContext(getter_AddRefs(dx));
    if (dx) { 
      float sbWidth;
      float sbHeight;
      dx->GetCanonicalPixelScale(scale);
      dx->GetScrollBarDimensions(sbWidth, sbHeight);
      scrollbarWidth  = PRInt32(sbWidth * scale);
      scrollbarHeight = PRInt32(sbHeight * scale);
    } else {
      scrollbarWidth  = GetScrollbarWidth(p2t);
      scrollbarHeight = scrollbarWidth;
    }

    if (!heightExplicit) {
      desiredSize.height += scrollbarHeight;
      minSize.height     += scrollbarHeight;
    } 
    if (!widthExplicit) {
      desiredSize.width += scrollbarWidth;
      minSize.width     += scrollbarWidth;
    }
  }

  aDesiredSize.width   = desiredSize.width;
  aDesiredSize.height  = desiredSize.height;
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = minSize.width;
    aDesiredSize.maxElementSize->height = minSize.height;
  }

  return NS_OK;

}

NS_IMETHODIMP
nsGfxTextControlFrame::Reflow(nsIPresContext& aPresContext,
                              nsHTMLReflowMetrics& aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus& aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsGfxTextControlFrame::Reflow: aMaxSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  // add ourself as an nsIFormControlFrame
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }

  // Figure out if we are doing Quirks or Standard
  nsCompatibility mode;
  aPresContext.GetCompatibilityMode(&mode);

  nsMargin borderPadding;
  borderPadding.SizeTo(0, 0, 0, 0);
  // Get the CSS border
  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
  spacing->CalcBorderPaddingFor(this, borderPadding);

  // calculate the the desired size for the text control
  // use the suggested size if it has been set
  nsresult rv = NS_OK;
  nsHTMLReflowState suggestedReflowState(aReflowState);
  if ((kSuggestedNotSet != mSuggestedWidth) || 
      (kSuggestedNotSet != mSuggestedHeight)) {
      // Honor the suggested width and/or height.
    if (kSuggestedNotSet != mSuggestedWidth) {
      suggestedReflowState.mComputedWidth = mSuggestedWidth;
      aDesiredSize.width = mSuggestedWidth;
    }

    if (kSuggestedNotSet != mSuggestedHeight) {
      suggestedReflowState.mComputedHeight = mSuggestedHeight;
      aDesiredSize.height = mSuggestedHeight;
    }
    rv = NS_OK;
  
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;

    aStatus = NS_FRAME_COMPLETE;
  } else {

    // this is the right way
    // Quirks mode will NOT obey CSS border and padding
    // GetDesiredSize calculates the size without CSS borders
    // the nsLeafFrame::Reflow will add in the borders
    if (eCompatibility_NavQuirks == mode) {
      rv = ReflowNavQuirks(aPresContext, aDesiredSize, aReflowState, aStatus, borderPadding);
    } else {
      rv = ReflowStandard(aPresContext, aDesiredSize, aReflowState, aStatus, borderPadding);
    }

    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
      if (aReflowState.mComputedWidth > aDesiredSize.width) {
        aDesiredSize.width = aReflowState.mComputedWidth;
      }
    }
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
      if (aReflowState.mComputedHeight > aDesiredSize.height) {
        aDesiredSize.height = aReflowState.mComputedHeight;
      }
    }
    aStatus = NS_FRAME_COMPLETE;
  }

#ifdef NOISY
  printf ("exit nsGfxTextControlFrame::Reflow: size=%d,%d",
           aDesiredSize.width, aDesiredSize.height);
#endif

  float t2p, p2t;
  mFramePresContext->GetTwipsToPixels(&t2p);
  mFramePresContext->GetPixelsToTwips(&p2t);
  // resize the sub document within the defined rect 
  // the sub-document will fit inside the border
  if (NS_SUCCEEDED(rv)) 
  {
    nsRect subBounds;
    nsRect subBoundsInPixels;
    nsSize desiredSize(aDesiredSize.width, aDesiredSize.height);
    CalcSizeOfSubDocInTwips(borderPadding, desiredSize, subBounds);
    subBoundsInPixels.x = NSToCoordRound(subBounds.x * t2p);
    subBoundsInPixels.y = NSToCoordRound(subBounds.y * t2p);
    subBoundsInPixels.width = NSToCoordRound(subBounds.width * t2p);
    subBoundsInPixels.height = NSToCoordRound(subBounds.height * t2p);

    if (eReflowReason_Initial == aReflowState.reason)
    {
      if (!mWebShell)
      { // if we haven't already created a webshell, then create something to hold the initial text
        PRInt32 type;
        GetType(&type);
        if ((PR_FALSE==IsSingleLineTextControl()) || (NS_FORM_INPUT_PASSWORD == type))
        { // password controls and multi-line text areas get their subdoc right away
          rv = CreateSubDoc(&subBoundsInPixels);
        }
        else
        { // single line text controls get a display frame rather than a subdoc.
          // the subdoc will be created when the frame first gets focus
          // create anonymous text content
          nsIContent* content;
          rv = NS_NewTextNode(&content);
          if (NS_FAILED(rv)) { return rv; }
          if (!content) { return NS_ERROR_NULL_POINTER; }
          nsIDocument* doc;
          mContent->GetDocument(doc);
          content->SetDocument(doc, PR_FALSE);
          NS_RELEASE(doc);
          mContent->AppendChildTo(content, PR_FALSE);

          // set the value of the text node
          content->QueryInterface(nsITextContent::GetIID(), getter_AddRefs(mDisplayContent));
          if (!mDisplayContent) {return NS_ERROR_NO_INTERFACE; }
          nsAutoString value;
          GetText(&value, PR_FALSE);  // get the text value, either from input element attribute or cached state
          PRInt32 len = value.Length();
          if (0<len)
          {
            // for password fields, set the display text to '*', one per character
            // XXX: the replacement character should be controllable via CSS
            // for normal text fields, set the display text normally
            if (NS_FORM_INPUT_PASSWORD == type) 
            {
              PRUnichar *initialPasswordText;
              initialPasswordText = new PRUnichar[len+1];
              if (!initialPasswordText) { return NS_ERROR_NULL_POINTER; }
              PRInt32 i=0;
              for (; i<len; i++) {
                initialPasswordText[i] = PASSWORD_REPLACEMENT_CHAR;
              }
              mDisplayContent->SetText(initialPasswordText, len, PR_TRUE);
              delete [] initialPasswordText;
            }
            else
            {
              const PRUnichar *initialText;
              initialText = value.GetUnicode();
              mDisplayContent->SetText(initialText, len, PR_TRUE);
            }
          }        

          // create the pseudo frame for the anonymous content
          rv = NS_NewBlockFrame((nsIFrame**)&mDisplayFrame, NS_BLOCK_SPACE_MGR);
          if (NS_FAILED(rv)) { return rv; }
          if (!mDisplayFrame) { return NS_ERROR_NULL_POINTER; }
        
          // create the style context for the anonymous frame
          nsCOMPtr<nsIStyleContext> styleContext;
          rv = aPresContext.ResolvePseudoStyleContextFor(content, 
                                                         nsHTMLAtoms::mozSingleLineTextControlFrame,
                                                         mStyleContext,
                                                         PR_FALSE,
                                                         getter_AddRefs(styleContext));
          if (NS_FAILED(rv)) { return rv; }
          if (!styleContext) { return NS_ERROR_NULL_POINTER; }

          // create a text frame and put it inside the block frame
          nsIFrame *textFrame;
          rv = NS_NewTextFrame(&textFrame);
          if (NS_FAILED(rv)) { return rv; }
          if (!textFrame) { return NS_ERROR_NULL_POINTER; }
          nsCOMPtr<nsIStyleContext> textStyleContext;
          rv = aPresContext.ResolvePseudoStyleContextFor(content, 
                                                         nsHTMLAtoms::mozSingleLineTextControlFrame,
                                                         styleContext,
                                                         PR_FALSE,
                                                         getter_AddRefs(textStyleContext));
          if (NS_FAILED(rv)) { return rv; }
          if (!textStyleContext) { return NS_ERROR_NULL_POINTER; }
          textFrame->Init(aPresContext, content, mDisplayFrame, textStyleContext, nsnull);
          textFrame->SetInitialChildList(aPresContext, nsnull, nsnull);
        
          rv = mDisplayFrame->Init(aPresContext, content, this, styleContext, nsnull);
          if (NS_FAILED(rv)) { return rv; }

          mDisplayFrame->SetInitialChildList(aPresContext, nsnull, textFrame);
        }
      }
    }
    if (mWebShell) 
    {
      if (mDisplayFrame) 
      {
        mDisplayFrame->Destroy(*mFramePresContext);
        mDisplayFrame = nsnull;
      }
      if (gNoisy) { printf("%p webshell in reflow set to bounds: x=%d, y=%d, w=%d, h=%d\n", mWebShell, subBoundsInPixels.x, subBoundsInPixels.y, subBoundsInPixels.width, subBoundsInPixels.height); }
      mWebShell->SetBounds(subBoundsInPixels.x, subBoundsInPixels.y, subBoundsInPixels.width, subBoundsInPixels.height);

#ifdef NOISY
      printf("webshell set to (%d, %d, %d %d)\n", 
             borderPadding.left, borderPadding.top, 
             (aDesiredSize.width - (borderPadding.left + borderPadding.right)), 
             (aDesiredSize.height - (borderPadding.top + borderPadding.bottom)));
#endif
    }
    else
    {
      // pass the reflow to mDisplayFrame, forcing him to fit
//      mDisplayFrame->SetSuggestedSize(subBounds.width, subBounds.height);
      if (mDisplayFrame)
      {
        // fix any possible pixel roundoff error (the webshell is sized in whole pixel units, 
        // and we need to make sure the text is painted in exactly the same place using either frame or shell.)
        subBounds.x = NSToCoordRound(subBoundsInPixels.x * p2t);
        subBounds.y = NSToCoordRound(subBoundsInPixels.y * p2t);
        subBounds.width = NSToCoordRound(subBoundsInPixels.width * p2t);
        subBounds.height = NSToCoordRound(subBoundsInPixels.height * p2t);

        // build the data structures for reflowing mDisplayFrame
        nsSize availSize(subBounds.width, subBounds.height);
        nsHTMLReflowMetrics kidSize(&availSize);
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState, mDisplayFrame,
                                         availSize);

        // Send the WillReflow notification, and reflow the child frame
        mDisplayFrame->WillReflow(aPresContext);
        nsReflowStatus status;
        rv = mDisplayFrame->Reflow(aPresContext, kidSize, kidReflowState, status);
        // notice how status is ignored here
        if (gNoisy) { printf("%p mDisplayFrame resized to: x=%d, y=%d, w=%d, h=%d\n", mWebShell, subBounds.x, subBounds.y, subBounds.width, subBounds.height); }
        mDisplayFrame->SetRect(&aPresContext, subBounds);
      }
    }
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsGfxTextControlFrame::Reflow: size=%d,%d",
                  aDesiredSize.width, aDesiredSize.height));

// This code below will soon be changed over for NSPR logging
// It is used to figure out what font and font size the textarea or text field
// are and compares it to the know NavQuirks size
#ifdef DEBUG_rods
//#ifdef NS_DEBUG
  {
    nsFont font(aPresContext.GetDefaultFixedFontDeprecated());
    GetFont(&aPresContext, font);
    nsCOMPtr<nsIDeviceContext> deviceContext;
    aPresContext.GetDeviceContext(getter_AddRefs(deviceContext));

    nsIFontMetrics* fontMet;
    deviceContext->GetMetricsFor(font, fontMet);

    const nsFont& normal = aPresContext.GetDefaultFixedFontDeprecated();
    PRInt32 scaler;
    aPresContext.GetFontScaler(&scaler);
    float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
    PRInt32 fontSize = nsStyleUtil::FindNextSmallerFontSize(font.size, (PRInt32)normal.size, 
                                                            scaleFactor)+1;
    PRBool doMeasure = PR_FALSE;
    nsILookAndFeel::nsMetricNavFontID fontId;

    nsFont defFont(aPresContext.GetDefaultFixedFontDeprecated());
    if (font.name == defFont.name) {
      fontId    = nsILookAndFeel::eMetricSize_Courier;
      doMeasure = PR_TRUE;
    } else {
      nsAutoString sansSerif("sans-serif");
      if (font.name == sansSerif) {
        fontId    = nsILookAndFeel::eMetricSize_SansSerif;
        doMeasure = PR_TRUE;
      }
    }
    NS_RELEASE(fontMet);

    if (doMeasure) {
      nsCOMPtr<nsILookAndFeel> lf;
      aPresContext.GetLookAndFeel(getter_AddRefs(lf));

      PRInt32 type;
      GetType(&type);
      if (NS_FORM_TEXTAREA == type) {
        if (fontSize > -1) {
          nsSize size;
          lf->GetNavSize(nsILookAndFeel::eMetricSize_TextArea, fontId, fontSize, size);
          COMPARE_QUIRK_SIZE("nsGfxText(textarea)", size.width, size.height)  // text area
        }
      } else {
        if (fontSize > -1) {
          nsSize size;
          lf->GetNavSize(nsILookAndFeel::eMetricSize_TextField, fontId, fontSize, size);
          COMPARE_QUIRK_SIZE("nsGfxText(field)", size.width, size.height)  // text field
        }
      }
    }
  }
#endif
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
//      looking for the first and last editable text nodes
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
      aSelection->Collapse(firstChild, 0);
      nsCOMPtr<nsIDOMCharacterData>text = do_QueryInterface(lastChild);
      if (text)
      {
        PRUint32 length;
        text->GetLength(&length);
        aSelection->Extend(lastChild, length);
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
// we've allocated the event listener or not.
PRBool nsGfxTextControlFrame::IsInitialized() const
{
  return (PRBool)(mEditor && mEventListener);
}

PRInt32 
nsGfxTextControlFrame::GetWidthInCharacters() const
{
  // see if there's a COL attribute, if so it wins
  nsCOMPtr<nsIHTMLContent> content;
  nsresult result = mContent->QueryInterface(nsIHTMLContent::GetIID(), getter_AddRefs(content));
  if (NS_SUCCEEDED(result) && content)
  {
    nsHTMLValue resultValue;
    result = content->GetHTMLAttribute(nsHTMLAtoms::cols, resultValue);
    if (NS_CONTENT_ATTR_NOT_THERE != result) 
    {
      if (resultValue.GetUnit() == eHTMLUnit_Integer) 
      {
        return (resultValue.GetIntValue());
      }
    }
  }

  // otherwise, see if CSS has a width specified.  If so, work backwards to get the 
  // number of characters this width represents.
 
  
  // otherwise, the default is just returned.
  return GetDefaultColumnWidth();
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
    nsCOMPtr<nsIViewManager> vm;
    presShell->GetViewManager(getter_AddRefs(vm));
    if (vm) 
    {
      nsIScrollableView *sv=nsnull;
      vm->GetRootScrollableView(&sv);
      if (sv) {
        if (PR_TRUE==IsSingleLineTextControl()) {
          sv->SetScrollPreference(nsScrollPreference_kNeverScroll);
        } else {
          PRInt32 type;
          GetType(&type);
          if (NS_FORM_TEXTAREA == type) {
            nsScrollPreference scrollPref = nsScrollPreference_kAlwaysScroll;
            nsFormControlHelper::nsHTMLTextWrap wrapProp;
            result = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
            if (NS_CONTENT_ATTR_NOT_THERE != result) {
              if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Soft ||
                  wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard) {
                scrollPref = nsScrollPreference_kAuto;
              }
            }
            sv->SetScrollPreference(scrollPref);
          }
        }
        // views are not refcounted
      }
    }

    nsCOMPtr<nsIDocument> doc; 
    presShell->GetDocument(getter_AddRefs(doc));
    NS_ASSERTION(doc, "null document");
    if (!doc) { return NS_ERROR_NULL_POINTER; }
    mDoc = do_QueryInterface(doc);
    if (!mDoc) { return NS_ERROR_NULL_POINTER; }
    if (mDocObserver) {
      doc->AddObserver(mDocObserver);
    }

    PRUint32 editorFlags = 0;
    if (IsPlainTextControl())
      editorFlags |= nsIHTMLEditor::eEditorPlaintextMask;
    if (IsSingleLineTextControl())
      editorFlags |= nsIHTMLEditor::eEditorSingleLineMask;
    if (IsPasswordTextControl())
      editorFlags |= nsIHTMLEditor::eEditorPasswordMask;
      
    // initialize the editor
    result = mEditor->Init(mDoc, presShell, editorFlags);
    // set data from the text control into the editor
    if (NS_SUCCEEDED(result)) {
      result = InitializeTextControl(presShell, mDoc);
    }
    // install our own event handlers before the editor's event handlers
    if (NS_SUCCEEDED(result)) {
      result = InstallEventListeners();
    }
    // finish editor initialization, including event handler installation
		if (NS_SUCCEEDED(result)) {
			mEditor->PostCreate();
		}

    // check to see if mContent has focus, and if so tell the webshell.
    nsIEventStateManager *manager;
    result = mFramePresContext->GetEventStateManager(&manager);
    if (NS_FAILED(result)) { return result; }
    if (!manager) { return NS_ERROR_NULL_POINTER; }

    nsIContent *focusContent=nsnull;
    result = manager->GetFocusedContent(&focusContent);
    if (NS_FAILED(result)) { return result; }
    if (focusContent)
    {
      if (mContent==focusContent)
      {
        SetFocus();
      }
    }
    NS_RELEASE(manager);
  }
  mCreatingViewer = PR_FALSE;
  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::InstallEventListeners()
{
  nsresult result;

  // get the DOM event receiver
  nsCOMPtr<nsIDOMEventReceiver> er;
  result = mDoc->QueryInterface(nsIDOMEventReceiver::GetIID(), getter_AddRefs(er));
  if (!er) { result = NS_ERROR_NULL_POINTER; }

  // get the view from the webshell
  nsCOMPtr<nsIPresShell> presShell;
  result = GetPresShellFor(mWebShell, getter_AddRefs(presShell));
  if (NS_FAILED(result)) { return result; }
  if (!presShell) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIViewManager> vm;
  presShell->GetViewManager(getter_AddRefs(vm));
  if (!vm) { return NS_ERROR_NULL_POINTER; }
  nsIScrollableView *sv=nsnull;
  vm->GetRootScrollableView(&sv);
  if (!sv) { return NS_ERROR_NULL_POINTER; }
  nsIView *view;
  sv->QueryInterface(nsIView::GetIID(), (void **)&view);
  if (!view) { return NS_ERROR_NULL_POINTER; }

  // we need to hook up our listeners before the editor is initialized
  result = NS_NewEnderEventListener(getter_AddRefs(mEventListener));
  if (NS_FAILED(result)) { return result ; }
  if (!mEventListener) { return NS_ERROR_NULL_POINTER; }
  mEventListener->SetFrame(this);
  mEventListener->SetPresContext(mFramePresContext);
  mEventListener->SetView(view);

  nsCOMPtr<nsIDOMKeyListener> keyListener = do_QueryInterface(mEventListener);
  if (!keyListener) { return NS_ERROR_NO_INTERFACE; }
  result = er->AddEventListenerByIID(keyListener, nsIDOMKeyListener::GetIID());
  if (NS_FAILED(result)) { return result; }

  nsCOMPtr<nsIDOMMouseListener> mouseListener = do_QueryInterface(mEventListener);
  if (!mouseListener) { return NS_ERROR_NO_INTERFACE; }
  result = er->AddEventListenerByIID(mouseListener, nsIDOMMouseListener::GetIID());
  if (NS_FAILED(result)) { return result; }
  
  nsCOMPtr<nsIDOMFocusListener> focusListener = do_QueryInterface(mEventListener);
  if (!focusListener) { return NS_ERROR_NO_INTERFACE; }
  result = er->AddEventListenerByIID(focusListener, nsIDOMFocusListener::GetIID());
  if (NS_FAILED(result)) { return result; }

  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::InitializeTextControl(nsIPresShell *aPresShell, nsIDOMDocument *aDoc)
{
  nsresult result;
  if (!aPresShell || !aDoc) { return NS_ERROR_NULL_POINTER; }

  /* needing a frame here is a hack.  We can't remove this hack until we can
   * set presContext info before style resolution on the webshell
   */
  nsIFrame *frame;
  result = aPresShell->GetRootFrame(&frame);
  if (NS_FAILED(result)) { return result; }
  if (!frame) { return NS_ERROR_NULL_POINTER; }

  PRInt32 type;
  GetType(&type);

  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
  if (!htmlEditor)  { return NS_ERROR_NO_INTERFACE; }

  nsCOMPtr<nsIPresContext>presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));
  NS_ASSERTION(presContext, "null presentation context");
  if (!presContext) { return NS_ERROR_NULL_POINTER; }

  /* set all style that propogates from the text control to its content
   * into the presContext for the webshell.
   * what I would prefer to do is hand the webshell my own 
   * pres context at creation, rather than having it create its own.
   */

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

  /* HACK:
   * since I don't yet have a hook for setting info on the pres context before style is
   * resolved, I need to call remap style on the root frame's style context.
   * The above code for setting presContext data should happen on a presContext that
   * I create and pass into the webshell, rather than having the webshell create its own
   */
  nsCOMPtr<nsIStyleContext> sc;
  result = frame->GetStyleContext(getter_AddRefs(sc));
  if (NS_FAILED(result)) { return result; }
  if (nsnull==sc) { return NS_ERROR_NULL_POINTER; }
  sc->RemapStyle(presContext);
	// end HACK

  // now that the style context is initialized, initialize the content
  nsAutoString value;
  if (mCachedState) {
    value = *mCachedState;
    delete mCachedState;
    mCachedState = nsnull;
  } else {
    GetText(&value, PR_TRUE);
  }
  mEditor->EnableUndo(PR_FALSE);

  PRInt32 maxLength;
  result = GetMaxLength(&maxLength);
  if (NS_CONTENT_ATTR_NOT_THERE != result) {
    htmlEditor->SetMaxTextLength(maxLength);
  }

  nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
  if (mailEditor)
  {
    PRBool wrapToContainerWidth = PR_TRUE;
    if (PR_TRUE==IsSingleLineTextControl())
    { // no wrapping for single line text controls
      result = mailEditor->SetBodyWrapWidth(-1);
      wrapToContainerWidth = PR_FALSE;
    }
    else
    { // if WRAP="OFF", turn wrapping off in the editor
      nsFormControlHelper::nsHTMLTextWrap wrapProp;
      result = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
      if (NS_CONTENT_ATTR_NOT_THERE != result) 
      {
        if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Off)
        {
          result = mailEditor->SetBodyWrapWidth(-1);
          wrapToContainerWidth = PR_FALSE;
        }
        if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard)
        {
          PRInt32 widthInCharacters = GetWidthInCharacters();
          result = mailEditor->SetBodyWrapWidth(widthInCharacters);
          wrapToContainerWidth = PR_FALSE;
        }
      } else {
        wrapToContainerWidth = PR_FALSE;
      }
    }
    if (PR_TRUE==wrapToContainerWidth)
    { // if we didn't set wrapping explicitly, turn on default wrapping here
      result = mailEditor->SetBodyWrapWidth(0);
    }
    NS_ASSERTION((NS_SUCCEEDED(result)), "error setting body wrap width");
    if (NS_FAILED(result)) { return result; }
  }

  nsCOMPtr<nsIEditor>editor = do_QueryInterface(mEditor);
  NS_ASSERTION(editor, "bad QI to nsIEditor from mEditor");
  if (editor)
  {
    nsCOMPtr<nsIDOMSelection>selection;
    result = editor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) { return result; }
    if (!selection) { return NS_ERROR_NULL_POINTER; }
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
          result = htmlEditor->InsertText(value);
          if (NS_FAILED(result)) { return result; }
          // collapse selection to beginning of text
          nsCOMPtr<nsIDOMNode>firstChild;
          result = bodyNode->GetFirstChild(getter_AddRefs(firstChild));
          if (NS_FAILED(result)) { return result; }
          selection->Collapse(firstChild, 0);
        }
      }
    }
  }

  // finish initializing editor
  mEditor->EnableUndo(PR_TRUE);

  // set readonly and disabled states
  if (mContent)
  {
    PRUint32 flags=0;
    if (IsPlainTextControl()) {
      flags |= nsIHTMLEditor::eEditorPlaintextMask;
    }
    if (IsSingleLineTextControl()) {
      flags |= nsIHTMLEditor::eEditorSingleLineMask;
    }
    if (IsPasswordTextControl()) {
      flags |= nsIHTMLEditor::eEditorPasswordMask;
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
        flags |= nsIHTMLEditor::eEditorReadonlyMask;
        aPresShell->SetCaretEnabled(PR_FALSE);
      }
      result = content->GetAttribute(nameSpaceID, nsHTMLAtoms::disabled, resultValue);
      if (NS_CONTENT_ATTR_NOT_THERE != result) 
      {
        flags |= nsIHTMLEditor::eEditorDisabledMask;
        aPresShell->SetCaretEnabled(PR_FALSE);
        nsCOMPtr<nsIDocument>doc = do_QueryInterface(aDoc);
        if (doc) {
          doc->SetDisplaySelection(PR_FALSE);
        }
      }
    }
    mEditor->SetFlags(flags);

    // turn on oninput notifications now that I'm done manipulating the editable content
    mNotifyOnInput = PR_TRUE;

    // add the selection listener
    nsCOMPtr<nsIDOMSelection>selection;
    if (mEditor && mEventListener)
    {
      result = mEditor->GetSelection(getter_AddRefs(selection));
      if (NS_FAILED(result)) { return result; }
      if (!selection) { return NS_ERROR_NULL_POINTER; }
      nsCOMPtr<nsIDOMSelectionListener> selectionListener = do_QueryInterface(mEventListener);
      if (!selectionListener) { return NS_ERROR_NO_INTERFACE; }
      selection->AddSelectionListener(selectionListener);
      if (NS_FAILED(result)) { return result; }
    }
  }
  return result;
}

// this is where we propogate a content changed event
NS_IMETHODIMP
nsGfxTextControlFrame::InternalContentChanged()
{
  NS_PRECONDITION(mContent, "illegal to call unless we map to a content node");

  if (!mContent) { return NS_ERROR_NULL_POINTER; }

  if (PR_FALSE==mNotifyOnInput) { 
    return NS_OK; // if notification is turned off, just return ok
  } 

  // Dispatch the change event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  event.widget = nsnull;
  event.message = NS_FORM_INPUT;
  event.flags = NS_EVENT_FLAG_INIT;

  // Have the content handle the event, propogating it according to normal DOM rules.
  nsresult result = mContent->HandleDOMEvent(*mFramePresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
  return result;
}

void nsGfxTextControlFrame::RemoveNewlines(nsString &aString)
{
  // strip CR/LF
  static const char badChars[] = {10, 13, 0};
  aString.StripChars(badChars);
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsGfxTextControlFrame::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
  nsIView* view;
  GetView(aPresContext, &view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fputs("<\n", out);

  // Dump out frames contained in interior web-shell
  if (mWebShell) {
    nsCOMPtr<nsIContentViewer> viewer;
    mWebShell->GetContentViewer(getter_AddRefs(viewer));
    if (viewer) {
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(viewer));
      if (docv) {
        nsCOMPtr<nsIPresContext> cx;
        docv->GetPresContext(*getter_AddRefs(cx));
        if (cx) {
          nsCOMPtr<nsIPresShell> shell;
          cx->GetShell(getter_AddRefs(shell));
          if (shell) {
            nsIFrame* rootFrame;
            shell->GetRootFrame(&rootFrame);
            if (rootFrame) {
              nsIFrameDebug*  frameDebug;

              if (NS_SUCCEEDED(rootFrame->QueryInterface(nsIFrameDebug::GetIID(), (void**)&frameDebug))) {
                frameDebug->List(aPresContext, out, aIndent + 1);
              }
              nsCOMPtr<nsIDocument> doc;
              docv->GetDocument(*getter_AddRefs(doc));
              if (doc) {
                nsCOMPtr<nsIContent> rootContent(getter_AddRefs(doc->GetRootContent()));
                if (rootContent) {
                  rootContent->List(out, aIndent + 1);
                }
              }
            }
          }
        }
      }
    }
  }

  IndentBy(out, aIndent);
  fputs(">\n", out);

    if (mDisplayFrame) {
      nsIFrameDebug*  frameDebug;
      
      IndentBy(out, aIndent);
      nsAutoString tmp;
      fputs("<\n", out);
      if (NS_SUCCEEDED(mDisplayFrame->QueryInterface(nsIFrameDebug::GetIID(), (void**)&frameDebug))) {
        frameDebug->List(aPresContext, out, aIndent + 1);
      }
      IndentBy(out, aIndent);
      fputs(">\n", out);
    }


  return NS_OK;
}
#endif

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

nsEnderDocumentObserver::nsEnderDocumentObserver()
{ 
  NS_INIT_REFCNT(); 
}

nsEnderDocumentObserver::~nsEnderDocumentObserver() 
{
}

nsresult
nsEnderDocumentObserver::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentObserverIID)) {
    *aInstancePtr = (void*) ((nsIStreamObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIDocumentObserver*)this));
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
                                                        PRInt32      aNameSpaceID,
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
{ 
  return NS_OK; 
}



/*******************************************************************************
 * nsEnderEventListener
 ******************************************************************************/

nsresult 
NS_NewEnderEventListener(nsIEnderEventListener ** aInstancePtr)
{
  nsEnderEventListener* it = new nsEnderEventListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(nsIEnderEventListener::GetIID(), (void **) aInstancePtr);   
}

NS_IMPL_ADDREF(nsEnderEventListener)

NS_IMPL_RELEASE(nsEnderEventListener)


nsEnderEventListener::nsEnderEventListener()
{
  NS_INIT_REFCNT();
  mView = nsnull;
  // other fields are objects that initialize themselves
}

nsEnderEventListener::~nsEnderEventListener()
{
  // all refcounted objects are held as nsCOMPtrs, clear themselves
}

NS_IMETHODIMP
nsEnderEventListener::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame.SetReference(aFrame->WeakReferent());
  if (aFrame)
  {
    aFrame->GetContent(getter_AddRefs(mContent));
  }
  return NS_OK;
}

nsresult
nsEnderEventListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMKeyListener *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);  
  if (aIID.Equals(kIDOMEventListenerIID)) {
    nsIDOMKeyListener *kl = (nsIDOMKeyListener*)this;
    nsIDOMEventListener *temp = kl;
    *aInstancePtr = (void*)temp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  if (aIID.Equals(nsIDOMKeyListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMMouseListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMFocusListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMFocusListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMSelectionListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMSelectionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIEnderEventListener::GetIID())) {
    *aInstancePtr = (void*)(nsIEnderEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsresult
nsEnderEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsEnderEventListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aKeyEvent);
  if (!uiEvent) { //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsKeyEvent event;
    event.eventStructType = NS_KEY_EVENT;
    event.widget = nsnull;
    event.message = NS_KEY_DOWN;
    event.flags = NS_EVENT_FLAG_INIT;
    uiEvent->GetKeyCode(&(event.keyCode));
    event.charCode = 0;
    uiEvent->GetShiftKey(&(event.isShift));
    uiEvent->GetCtrlKey(&(event.isControl));
    uiEvent->GetAltKey(&(event.isAlt));
    uiEvent->GetMetaKey(&(event.isMeta));

    nsIEventStateManager *manager=nsnull;
    result = mContext->GetEventStateManager(&manager);
    if (NS_SUCCEEDED(result) && manager) 
    {
      //1. Give event to event manager for pre event state changes and generation of synthetic events.
      result = manager->PreHandleEvent(*mContext, &event, gfxFrame, status, mView);

      //2. Give event to the DOM for third party and JS use.
      if (NS_SUCCEEDED(result)) {
        result = mContent->HandleDOMEvent(*mContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
      }
    
      //3. In this case, the frame does no processing of the event

      //4. Give event to event manager for post event state changes and generation of synthetic events.
      gfxFrame = mFrame.Reference(); // check for deletion
      if (gfxFrame && NS_SUCCEEDED(result)) {
        result = manager->PostHandleEvent(*mContext, &event, gfxFrame, status, mView);
      }
      NS_RELEASE(manager);
    }
  }
  return result;
}

nsresult
nsEnderEventListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aKeyEvent);
  if (!uiEvent) { //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsKeyEvent event;
    event.eventStructType = NS_KEY_EVENT;
    event.widget = nsnull;
    event.message = NS_KEY_UP;
    event.flags = NS_EVENT_FLAG_INIT;
    uiEvent->GetKeyCode(&(event.keyCode));
    event.charCode = 0;
    uiEvent->GetShiftKey(&(event.isShift));
    uiEvent->GetCtrlKey(&(event.isControl));
    uiEvent->GetAltKey(&(event.isAlt));
    uiEvent->GetMetaKey(&(event.isMeta));

    nsIEventStateManager *manager=nsnull;
    result = mContext->GetEventStateManager(&manager);
    if (NS_SUCCEEDED(result) && manager) 
    {
      //1. Give event to event manager for pre event state changes and generation of synthetic events.
      result = manager->PreHandleEvent(*mContext, &event, gfxFrame, status, mView);

      //2. Give event to the DOM for third party and JS use.
      if (NS_SUCCEEDED(result)) {
        result = mContent->HandleDOMEvent(*mContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
      }
    
      //3. In this case, the frame does no processing of the event

      //4. Give event to event manager for post event state changes and generation of synthetic events.
      gfxFrame = mFrame.Reference(); // check for deletion
      if (gfxFrame && NS_SUCCEEDED(result)) {
        result = manager->PostHandleEvent(*mContext, &event, gfxFrame, status, mView);
      }
      NS_RELEASE(manager);
    }
  }
  return result;
}

nsresult
nsEnderEventListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aKeyEvent);
  if (!uiEvent) { //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent && mContext && mView)
  {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsKeyEvent event;
    event.eventStructType = NS_KEY_EVENT;
    event.widget = nsnull;
    event.message = NS_KEY_PRESS;
    event.flags = NS_EVENT_FLAG_INIT;
    uiEvent->GetKeyCode(&(event.keyCode));
    uiEvent->GetCharCode(&(event.charCode));
    uiEvent->GetShiftKey(&(event.isShift));
    uiEvent->GetCtrlKey(&(event.isControl));
    uiEvent->GetAltKey(&(event.isAlt));
    uiEvent->GetMetaKey(&(event.isMeta));

    nsIEventStateManager *manager=nsnull;
    result = mContext->GetEventStateManager(&manager);
    if (NS_SUCCEEDED(result) && manager) 
    {
      //1. Give event to event manager for pre event state changes and generation of synthetic events.
      result = manager->PreHandleEvent(*mContext, &event, gfxFrame, status, mView);

      //2. Give event to the DOM for third party and JS use.
      if (NS_SUCCEEDED(result)) {
        result = mContent->HandleDOMEvent(*mContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
      }
    
      //3. Give event to the frame for browser default processing
      gfxFrame = mFrame.Reference(); // check for deletion
      if (gfxFrame && NS_SUCCEEDED(result)) {
        result = gfxFrame->HandleEvent(*mContext, &event, status);
      }

      //4. Give event to event manager for post event state changes and generation of synthetic events.
      gfxFrame = mFrame.Reference(); // check for deletion
      if (gfxFrame && NS_SUCCEEDED(result)) {
        result = manager->PostHandleEvent(*mContext, &event, gfxFrame, status, mView);
      }
      NS_RELEASE(manager);
    }
  }
  return result;
}

void GetWidgetForView(nsIView *aView, nsIWidget *&aWidget)
{
  aWidget = nsnull;
  nsIView *view = aView;
  while (!aWidget && view)
  {
    view->GetWidget(aWidget);
    if (!aWidget)
      view->GetParent(view);
  }
}

nsresult
nsEnderEventListener::DispatchMouseEvent(nsIDOMUIEvent *aEvent, PRInt32 aEventType)
{
  nsresult result = NS_OK;
  if (aEvent)
  {
    nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
    if (gfxFrame)
    {
      // Dispatch the event
      nsEventStatus status = nsEventStatus_eIgnore;
      nsMouseEvent event;
      event.eventStructType = NS_MOUSE_EVENT;
      event.widget = nsnull;
      event.flags = NS_EVENT_FLAG_INIT;
      aEvent->GetShiftKey(&(event.isShift));
      aEvent->GetCtrlKey(&(event.isControl));
      aEvent->GetAltKey(&(event.isAlt));
      PRUint16 clickCount;
      aEvent->GetClickCount(&clickCount);
      event.clickCount = clickCount;
      event.message = aEventType;
      GetWidgetForView(mView, event.widget);
      NS_ASSERTION(event.widget, "null widget in mouse event redispatch.  right mouse click will crash!");

      nsIEventStateManager *manager=nsnull;
      result = mContext->GetEventStateManager(&manager);
      if (NS_SUCCEEDED(result) && manager) 
      {
        //1. Give event to event manager for pre event state changes and generation of synthetic events.
        result = manager->PreHandleEvent(*mContext, &event, gfxFrame, status, mView);

        //2. Give event to the DOM for third party and JS use.
        if (NS_SUCCEEDED(result)) {
          result = mContent->HandleDOMEvent(*mContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
        }
    
        //3. Give event to the frame for browser default processing
        if (NS_SUCCEEDED(result)) {
          result = gfxFrame->HandleEvent(*mContext, &event, status);
        }

        //4. Give event to event manager for post event state changes and generation of synthetic events.
        gfxFrame = mFrame.Reference(); // check for deletion
        if (gfxFrame && NS_SUCCEEDED(result)) {
          result = manager->PostHandleEvent(*mContext, &event, gfxFrame, status, mView);
        }
        NS_IF_RELEASE(manager);
      }
    }
  }
  return result;
}

nsresult
nsEnderEventListener::MouseDown(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) { //non-key event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    PRUint16 button;
    uiEvent->GetButton(&button);
    PRInt32 eventType;
    switch(button)
    {
      case 1: //XXX: I can't believe there isn't a symbol for this!
        eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
        break;
      case 2: //XXX: I can't believe there isn't a symbol for this!
        eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
        break;
      case 3: //XXX: I can't believe there isn't a symbol for this!
        eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
        break;
      default:
        NS_ASSERTION(0, "bad button type");
        return NS_OK;
    }
    result = DispatchMouseEvent(uiEvent, eventType);
  }  
  return result;
}

nsresult
nsEnderEventListener::MouseUp(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) { //non-key event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    PRUint16 button;
    uiEvent->GetButton(&button);
    PRInt32 eventType;
    switch(button)
    {
      case 1:
        eventType = NS_MOUSE_LEFT_BUTTON_UP;
        break;
      case 2:
        eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
        break;
      case 3:
        eventType = NS_MOUSE_RIGHT_BUTTON_UP;
        break;
      default:
        NS_ASSERTION(0, "bad button type");
        return NS_OK;
    }
    result = DispatchMouseEvent(uiEvent, eventType);
  }
  return result;
}

nsresult
nsEnderEventListener::MouseClick(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) { //non-key event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    // Dispatch the event
    PRUint16 button;
    uiEvent->GetButton(&button);
    PRInt32 eventType;
    switch(button)
    {
      case 1:
        eventType = NS_MOUSE_LEFT_CLICK;
        break;
      case 2:
        eventType = NS_MOUSE_MIDDLE_CLICK;
        break;
      case 3:
        eventType = NS_MOUSE_RIGHT_CLICK;
        break;
      default:
        NS_ASSERTION(0, "bad button type");
        return NS_OK;
    }
    result = DispatchMouseEvent(uiEvent, eventType);
  }
  return result;
}

nsresult
nsEnderEventListener::MouseDblClick(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) { //non-key event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    // Dispatch the event
    PRUint16 button;
    uiEvent->GetButton(&button);
    PRInt32 eventType;
    switch(button)
    {
      case 1:
        eventType = NS_MOUSE_LEFT_DOUBLECLICK;
        break;
      case 2:
        eventType = NS_MOUSE_MIDDLE_DOUBLECLICK;
        break;
      case 3:
        eventType = NS_MOUSE_RIGHT_DOUBLECLICK;
        break;
      default:
        NS_ASSERTION(0, "bad button type");
        return NS_OK;
    }
    result = DispatchMouseEvent(uiEvent, eventType);
  }
  
  return result;
}

nsresult
nsEnderEventListener::MouseOver(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) { //non-key event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    // XXX: Need to synthesize MouseEnter here
    result = DispatchMouseEvent(uiEvent, NS_MOUSE_MOVE);
  }
  
  return result;
}

nsresult
nsEnderEventListener::MouseOut(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) { //non-key event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    result = DispatchMouseEvent(uiEvent, NS_MOUSE_EXIT);
  }
  
  return result;
}


nsresult
nsEnderEventListener::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) { 
    return NS_OK;
  }

  nsresult result = NS_OK;

  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();

  if (gfxFrame && mContent && mView)
  {
    mTextValue = "";
    gfxFrame->GetText(&mTextValue, PR_FALSE);
    // Dispatch the event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    event.widget = nsnull;
    event.message = NS_FOCUS_CONTENT;
    event.flags = NS_EVENT_FLAG_INIT;

    nsIEventStateManager *manager=nsnull;
    result = mContext->GetEventStateManager(&manager);
    if (NS_SUCCEEDED(result) && manager) 
    {
      //1. Give event to event manager for pre event state changes and generation of synthetic events.
      result = manager->PreHandleEvent(*mContext, &event, gfxFrame, status, mView);

      //2. Give event to the DOM for third party and JS use.
      if (NS_SUCCEEDED(result)) {
        result = mContent->HandleDOMEvent(*mContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
      }
    
      //3. In this case, the frame does no processing of the event

      //4. Give event to event manager for post event state changes and generation of synthetic events.
      gfxFrame = mFrame.Reference(); // check for deletion
      if (gfxFrame && NS_SUCCEEDED(result)) {
        result = manager->PostHandleEvent(*mContext, &event, gfxFrame, status, mView);
      }
      NS_RELEASE(manager);
    }
  }

  return NS_OK;
}

nsresult
nsEnderEventListener::Blur(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) {
    return NS_OK;
  }

  nsresult result = NS_OK;

  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();

  if (gfxFrame && mContent && mView)
  {
    nsString currentValue;
    gfxFrame->GetText(&currentValue, PR_FALSE);
    if (PR_FALSE==currentValue.Equals(mTextValue))
    {
      // Dispatch the change event
      nsEventStatus status = nsEventStatus_eIgnore;
      nsGUIEvent event;
      event.eventStructType = NS_GUI_EVENT;
      event.widget = nsnull;
      event.message = NS_FORM_CHANGE;
      event.flags = NS_EVENT_FLAG_INIT;

      // Have the content handle the event.
      mContent->HandleDOMEvent(*mContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
    }

    // Dispatch the blur event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    event.widget = nsnull;
    event.message = NS_BLUR_CONTENT;
    event.flags = NS_EVENT_FLAG_INIT;

    nsIEventStateManager *manager=nsnull;
    result = mContext->GetEventStateManager(&manager);
    if (NS_SUCCEEDED(result) && manager) 
    {
      //1. Give event to event manager for pre event state changes and generation of synthetic events.
      result = manager->PreHandleEvent(*mContext, &event, gfxFrame, status, mView);

      //2. Give event to the DOM for third party and JS use.
      if (NS_SUCCEEDED(result)) {
        result = mContent->HandleDOMEvent(*mContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
      }
    
      //3. In this case, the frame does no processing of the event

      //4. Give event to event manager for post event state changes and generation of synthetic events.
      gfxFrame = mFrame.Reference(); // check for deletion
      if (gfxFrame && NS_SUCCEEDED(result)) {
        result = manager->PostHandleEvent(*mContext, &event, gfxFrame, status, mView);
      }
      NS_RELEASE(manager);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEnderEventListener::NotifySelectionChanged()
{
  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    // Dispatch the event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    event.widget = nsnull;
    event.message = NS_FORM_SELECTED;
    event.flags = NS_EVENT_FLAG_INIT;

    // Have the content handle the event.
    mContent->HandleDOMEvent(*mContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
    
    // Now have the frame handle the event
    gfxFrame->HandleEvent(*mContext, &event, status);
  }
  return NS_OK;
}



/*******************************************************************************
 * nsEnderFocusListenerForContent
 ******************************************************************************/

NS_IMPL_ADDREF(nsEnderFocusListenerForContent);
NS_IMPL_RELEASE(nsEnderFocusListenerForContent);

nsresult
nsEnderFocusListenerForContent::QueryInterface(const nsIID& aIID,
                                               void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsIDOMFocusListener::GetIID())) {
    *aInstancePtr = (void*) ((nsIDOMFocusListener*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIDOMFocusListener*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsEnderFocusListenerForContent::nsEnderFocusListenerForContent() :
  mFrame(nsnull)
{
  NS_INIT_REFCNT();
}

nsEnderFocusListenerForContent::~nsEnderFocusListenerForContent()
{
}

nsresult 
nsEnderFocusListenerForContent::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


nsresult
nsEnderFocusListenerForContent::Focus(nsIDOMEvent* aEvent)
{
  if (mFrame)
  {
    mFrame->CreateSubDoc(nsnull);
  }
  return NS_OK;
}

nsresult
nsEnderFocusListenerForContent::Blur (nsIDOMEvent* aEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
nsEnderFocusListenerForContent::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame = aFrame;
  return NS_OK;
}


/*******************************************************************************
 * EnderTempObserver
 ******************************************************************************/
// XXX temp implementation

NS_IMPL_ADDREF(EnderTempObserver);
NS_IMPL_RELEASE(EnderTempObserver);

EnderTempObserver::EnderTempObserver()
{ 
  NS_INIT_REFCNT(); 
  mFirstCall = PR_TRUE;
}

EnderTempObserver::~EnderTempObserver()
{
}

nsresult
EnderTempObserver::QueryInterface(const nsIID& aIID,
                                  void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentLoaderObserverIID)) {
    *aInstancePtr = (void*) ((nsIDocumentLoaderObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIDocumentLoaderObserver*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


// document loader observer implementation
NS_IMETHODIMP
EnderTempObserver::OnStartDocumentLoad(nsIDocumentLoader* loader, 
                                       nsIURI* aURL, 
                                       const char* aCommand)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnEndDocumentLoad(nsIDocumentLoader* loader,
                                     nsIChannel* channel,
                                     nsresult aStatus,
                                     nsIDocumentLoaderObserver * aObserver)
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
EnderTempObserver::OnStartURLLoad(nsIDocumentLoader* loader,
                                  nsIChannel* channel, 
                                  nsIContentViewer* aViewer)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnProgressURLLoad(nsIDocumentLoader* loader,
                                     nsIChannel* channel,
                                     PRUint32 aProgress, 
                                     PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnStatusURLLoad(nsIDocumentLoader* loader,
                                   nsIChannel* channel,
                                   nsString& aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnEndURLLoad(nsIDocumentLoader* loader,
                                nsIChannel* channel,
                                nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::HandleUnknownContentType(nsIDocumentLoader* loader,
                                            nsIChannel* channel,
                                            const char *aContentType,
                                            const char *aCommand)
{
  return NS_OK;
}


NS_IMETHODIMP
EnderTempObserver::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame = aFrame;
  return NS_OK;
}

