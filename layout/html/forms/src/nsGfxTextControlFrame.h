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

#ifndef nsGfxTextControlFrame_h___
#define nsGfxTextControlFrame_h___

#include "nsFormControlFrame.h"
#include "nsTextControlFrame.h"
#include "nsIStreamObserver.h"
#include "nsITextEditor.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMDocument.h"
#include "nsIPresContext.h"
#include "nsIContent.h"

class nsIFrame;
class nsIWebShell;

class nsGfxTextControlFrame;


/*******************************************************************************
 * EnderTempObserver XXX temporary until doc manager/loader is in place
 ******************************************************************************/
class EnderTempObserver : public nsIStreamObserver
{
public:
  EnderTempObserver() 
  { 
    NS_INIT_REFCNT(); 
    mFirstCall = PR_TRUE;
  }

  NS_IMETHOD SetFrame(nsGfxTextControlFrame *aFrame);

  virtual ~EnderTempObserver();

  // nsISupports
  NS_DECL_ISUPPORTS

#ifdef NECKO
  // nsIStreamObserver methods:
  NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports *ctxt);
  NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg);
#else
  // nsIStreamObserver
  NS_IMETHOD OnStartRequest(nsIURI* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
  NS_IMETHOD OnStatus(nsIURI* aURL, const PRUnichar* aMsg);
  NS_IMETHOD OnStopRequest(nsIURI* aURL, nsresult status, const PRUnichar* aMsg);
#endif

protected:

  nsString mURL;
  nsString mOverURL;
  nsString mOverTarget;
  nsGfxTextControlFrame *mFrame; // not ref counted
  PRBool mFirstCall;
};

/*******************************************************************************
 * nsEnderDocumentObserver
 * This class responds to document changes
 ******************************************************************************/
class nsEnderDocumentObserver : public nsIDocumentObserver
{
public:
  nsEnderDocumentObserver();

  NS_IMETHOD SetFrame(nsGfxTextControlFrame *aFrame);

  virtual ~nsEnderDocumentObserver(); 

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_IMETHOD BeginUpdate(nsIDocument *aDocument);
  NS_IMETHOD EndUpdate(nsIDocument *aDocument);
  NS_IMETHOD BeginLoad(nsIDocument *aDocument);
  NS_IMETHOD EndLoad(nsIDocument *aDocument);
  NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell);
  NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell);
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                            nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2);
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled);
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint);
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument);


protected:

  nsGfxTextControlFrame *mFrame; // not ref counted
};

/******************************************************************************
 * nsEnderKeyListener
 ******************************************************************************/

class nsEnderKeyListener; // forward declaration for factory

/* factory for ender key listener */
nsresult NS_NewEnderKeyListener(nsEnderKeyListener ** aInstancePtrResult);

class nsEnderKeyListener : public nsIDOMKeyListener 
{
public:

  /** the default destructor */
  virtual ~nsEnderKeyListener();

  /** SetFrame sets the frame we send event messages to, when necessary
   *  @param aEditor the frame, can be null, not ref counted (guaranteed to outlive us!)
   */
  void SetFrame(nsGfxTextControlFrame *aFrame);

  void SetPresContext(nsIPresContext *aCx) {mContext = do_QueryInterface(aCx);}

  /*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  /* nsIDOMKeyListener interfaces */
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
  virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);
  /*END interfaces from nsIDOMKeyListener*/

  friend nsresult NS_NewEnderKeyListener(nsEnderKeyListener ** aInstancePtrResult);

protected:
  /** the default constructor.  Protected, use the factory to create an instance.
    * @see NS_NewEnderKeyListener
    */
  nsEnderKeyListener();

protected:
  nsGfxTextControlFrame    *mFrame;   // not ref counted
  nsCOMPtr<nsIPresContext>  mContext; // ref counted
  nsCOMPtr<nsIContent>      mContent; // ref counted
};


/******************************************************************************
 * nsGfxTextControlFrame
 ******************************************************************************/

// XXX code related to the dummy native text control frame is marked with DUMMY
// and should be removed asap

#include "nsNativeTextControlFrame.h" // DUMMY

class nsGfxTextControlFrame : public nsTextControlFrame
{
public:
  nsGfxTextControlFrame();
  virtual ~nsGfxTextControlFrame();

  /** nsIFrame override of Init.
    * all we do here is cache the pres context for later use
    */
  NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow);

  NS_IMETHOD InitTextControl();

       // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);

  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  NS_IMETHOD GetText(nsString* aValue, PRBool aInitialValue);

  virtual void EnterPressed(nsIPresContext& aPresContext) ;

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);
  virtual void Reset();

  // override to interact with webshell
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  virtual void PaintTextControlBackground(nsIPresContext& aPresContext,
                                          nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect,
                                          nsFramePaintLayer aWhichLayer);

  virtual void PaintTextControl(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect, nsString& aText,
                                nsIStyleContext* aStyleContext,
                                nsRect& aRect);
 
  // Utility methods to get and set current widget state
  void GetTextControlFrameState(nsString& aValue);
  void SetTextControlFrameState(const nsString& aValue); 
  
  NS_IMETHOD InstallEditor();

  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);

  NS_IMETHOD InternalContentChanged();
  NS_IMETHOD DoesAttributeExist(nsIAtom *aAtt);

  void RemoveNewlines(nsString &aString);

protected:

 
  NS_IMETHOD CreateWebShell(nsIPresContext& aPresContext,
                            const nsSize& aSize);

  NS_IMETHOD InitializeTextControl(nsIPresShell *aPresShell, nsIDOMDocument *aDoc);

  NS_IMETHOD GetPresShellFor(nsIWebShell* aWebShell, nsIPresShell** aPresShell);
  
  NS_IMETHOD GetFirstNodeOfType(const nsString& aTag, nsIDOMDocument *aDOMDoc, nsIDOMNode **aBodyNode);

  NS_IMETHOD GetFirstFrameForType(const nsString& aTag, nsIPresShell *aPresShell, nsIDOMDocument *aDOMDoc, nsIFrame **aResult);

  NS_IMETHOD SelectAllTextContent(nsIDOMNode *aBodyNode, nsIDOMSelection *aSelection);

  PRBool IsSingleLineTextControl() const;

  PRBool IsPlainTextControl() const;

  PRBool IsPasswordTextControl() const;

  PRBool IsInitialized() const;


protected:
  nsIWebShell* mWebShell;
  PRBool mCreatingViewer;
  EnderTempObserver* mTempObserver;
  nsEnderDocumentObserver *mDocObserver;
  nsCOMPtr<nsEnderKeyListener> mKeyListener;  // ref counted
  nsCOMPtr<nsITextEditor>   mEditor;  // ref counted
  nsCOMPtr<nsIDOMDocument>  mDoc;     // ref counted
  nsNativeTextControlFrame *mDummyFrame; //DUMMY
  PRBool mNeedsStyleInit;
  PRBool mDummyInitialized; //DUMMY
  nsIPresContext *mFramePresContext; // not ref counted
};

#endif
