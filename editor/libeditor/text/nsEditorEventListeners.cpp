/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsEditorEventListeners.h"
#include "nsEditor.h"

#include "CreateElementTxn.h"

#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMCharacterData.h"
#include "nsIEditProperty.h"
#include "nsISupportsArray.h"
#include "nsString.h"
#include "nsIStringStream.h"

// for testing only
#include "nsIHTMLEditor.h"
// end for testing only

#ifdef NS_DEBUG
#include "TextEditorTest.h"
#endif

static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMCharacterDataIID, NS_IDOMCHARACTERDATA_IID);

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsIDragService.h"
#include "nsITransferable.h"
#include "nsIGenericTransferable.h"
#include "nsIDataFlavor.h"
#include "nsIFormatConverter.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_IID(kIClipboardIID,     NS_ICLIPBOARD_IID);
static NS_DEFINE_CID(kCClipboardCID,     NS_CLIPBOARD_CID);

static NS_DEFINE_IID(kIDragServiceIID,   NS_IDRAGSERVICE_IID);
static NS_DEFINE_CID(kCDragServiceCID,   NS_DRAGSERVICE_CID);

static NS_DEFINE_IID(kIGenericTransferableIID,  NS_IGENERICTRANSFERABLE_IID);
static NS_DEFINE_CID(kCGenericTransferableCID,  NS_GENERICTRANSFERABLE_CID);
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);
static NS_DEFINE_IID(kCDataFlavorCID,    NS_DATAFLAVOR_CID);

static NS_DEFINE_IID(kCXIFFormatConverterCID,    NS_XIFFORMATCONVERTER_CID);
static NS_DEFINE_IID(kIFormatConverterIID, NS_IFORMATCONVERTER_IID);


/*
 * nsTextEditorKeyListener implementation
 */

NS_IMPL_ADDREF(nsTextEditorKeyListener)

NS_IMPL_RELEASE(nsTextEditorKeyListener)


nsTextEditorKeyListener::nsTextEditorKeyListener()
{
  NS_INIT_REFCNT();
}



nsTextEditorKeyListener::~nsTextEditorKeyListener() 
{
}



nsresult
nsTextEditorKeyListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
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
nsTextEditorKeyListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


#define HAVE_EVENT_CHARCODE				// on when we have the charCode in the event

nsresult
nsTextEditorKeyListener::GetCharFromKeyCode(PRUint32 aKeyCode, PRBool aIsShift, char *aChar)
{
  /* This is completely temporary to get this working while I check out Unicode conversion code. */
#ifdef XP_MAC
  if (aChar) {
    *aChar = (char)aKeyCode;
    return NS_OK;
    }
#else
  if (aKeyCode >= 0x41 && aKeyCode <= 0x5A) {
    if (aIsShift) {
      *aChar = (char)aKeyCode;
    }
    else {
      *aChar = (char)(aKeyCode + 0x20);
    }
    return NS_OK;
  }
  else if ((aKeyCode >= 0x30 && aKeyCode <= 0x39) || aKeyCode == 0x20) {
      *aChar = (char)aKeyCode;
      return NS_OK;
  }
#endif
  return NS_ERROR_FAILURE;
}

nsresult
nsTextEditorKeyListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  PRUint32 keyCode;
  PRBool   isShift;
  PRBool   ctrlKey;
  
  if (NS_SUCCEEDED(aKeyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(aKeyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(aKeyEvent->GetCtrlKey(&ctrlKey))
      ) {
    PRBool keyProcessed;
    ProcessShortCutKeys(aKeyEvent, keyProcessed);
    if (PR_FALSE==keyProcessed)
    {
      switch(keyCode) {
      case nsIDOMEvent::VK_BACK:
        mEditor->DeleteSelection(nsIEditor::eRTL);
        break;

      case nsIDOMEvent::VK_DELETE:
        mEditor->DeleteSelection(nsIEditor::eLTR);
        break;

      case nsIDOMEvent::VK_RETURN:
      //case nsIDOMEvent::VK_ENTER:			// why does this not exist?
        // Need to implement creation of either <P> or <BR> nodes.
        mEditor->InsertBreak();
        break;
      
      case nsIDOMEvent::VK_LEFT:
      case nsIDOMEvent::VK_RIGHT:
      case nsIDOMEvent::VK_UP:
      case nsIDOMEvent::VK_DOWN:
      	// these have already been handled in nsRangeList. Why are we getting them
      	// again here (Mac)? In switch to avoid putting in bogus chars.
      	break;
      
      case nsIDOMEvent::VK_HOME:
      case nsIDOMEvent::VK_END:
      case nsIDOMEvent::VK_PAGE_UP:
      case nsIDOMEvent::VK_PAGE_DOWN:
      	// who handles these?
#if DEBUG
		printf("Key not handled\n");
#endif
      	break;
      	
      default:
        {
          nsAutoString  key;
#ifdef HAVE_EVENT_CHARCODE
          PRUint32     character;
          // do we convert to Unicode here, or has this already been done? (sfraser)
          if (NS_SUCCEEDED(aKeyEvent->GetCharCode(&character)))
          {
            key += character;
            if (0!=character)
              mEditor->InsertText(key);
          }
#else
          char character;
          // XXX Replace with x-platform NS-virtkeycode transform.
          if (NS_OK == GetCharFromKeyCode(keyCode, isShift, & character)) {
            if (0!=character)
            {
              nsAutoString key;
              key += character;
              if (!isShift) {
                key.ToLowerCase();
              }
              mEditor->InsertText(key);
            }
          }
#endif
        }
        break;
      }
    }
  }
  
  return NS_ERROR_BASE;
}



nsresult
nsTextEditorKeyListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorKeyListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

/* these includes are for debug only.  this module should never instantiate it's own transactions */
#include "SplitElementTxn.h"
#include "TransactionFactory.h"
static NS_DEFINE_IID(kSplitElementTxnIID,   SPLIT_ELEMENT_TXN_IID);
nsresult
nsTextEditorKeyListener::ProcessShortCutKeys(nsIDOMEvent* aKeyEvent, PRBool& aProcessed)
{
  aProcessed=PR_FALSE;
  PRUint32 keyCode;
  PRBool isShift;
  PRBool ctrlKey;
  PRBool altKey;
  
  if (NS_SUCCEEDED(aKeyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(aKeyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(aKeyEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(aKeyEvent->GetAltKey(&altKey))
      ) 
  {
    // XXX: please please please get these mappings from an external source!
    switch (keyCode)
    {
      // XXX: hard-coded select all
      case nsIDOMEvent::VK_A:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->SelectAll();
        }
        break;

      // XXX: hard-coded cut
      case nsIDOMEvent::VK_X:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Cut();
        }
        break;

      // XXX: hard-coded copy
      case nsIDOMEvent::VK_C:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Copy();
        }
        break;

      // XXX: hard-coded paste
      case nsIDOMEvent::VK_V:
        if (PR_TRUE==ctrlKey)
        {
          printf("control-v\n");
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Paste();
        }
        break;

      // XXX: hard-coded undo
      case nsIDOMEvent::VK_Z:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Undo(1);
        }
        break;

      // XXX: hard-coded redo
      case nsIDOMEvent::VK_Y:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Redo(1);
        }
        break;

      // hard-coded ChangeTextAttributes test -- italics
      case nsIDOMEvent::VK_I:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            mEditor->GetTextProperty(nsIEditProperty::i, nsnull, nsnull, first, any, all);
            if (PR_FALSE==first) {
              mEditor->SetTextProperty(nsIEditProperty::i, nsnull, nsnull);
            }
            else {
              mEditor->RemoveTextProperty(nsIEditProperty::i, nsnull);
            }
          }
        }

      // Hardcoded Insert Arbitrary HTML
        else if (PR_TRUE==altKey)
        {
          printf("Trying to insert HTML\n");
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            nsString nsstr ("<b>This is bold <em>and emphasized</em></b>");
            nsresult res = mEditor->Insert(nsstr);
            if (!NS_SUCCEEDED(res))
              printf("nsTextEditor::Insert(string) failed\n");
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- bold
      case nsIDOMEvent::VK_B:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            mEditor->GetTextProperty(nsIEditProperty::b, nsnull, nsnull, first, any, all);
            if (PR_FALSE==first) {
              mEditor->SetTextProperty(nsIEditProperty::b, nsnull, nsnull);
            }
            else {
              mEditor->RemoveTextProperty(nsIEditProperty::b, nsnull);
            }
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- underline
      case nsIDOMEvent::VK_U:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            mEditor->GetTextProperty(nsIEditProperty::u, nsnull, nsnull, first, any, all);
            if (PR_FALSE==first) {
              mEditor->SetTextProperty(nsIEditProperty::u, nsnull, nsnull);
            }
            else {
              mEditor->RemoveTextProperty(nsIEditProperty::u, nsnull);
            }

          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font color red
      case nsIDOMEvent::VK_1:
        case nsIDOMEvent::VK_F:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            nsAutoString color = "COLOR";
            nsAutoString value = "red";
            mEditor->SetTextProperty(nsIEditProperty::font, &color, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font color green
      case nsIDOMEvent::VK_2:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            nsAutoString color = "COLOR";
            nsAutoString value = "green";
            mEditor->SetTextProperty(nsIEditProperty::font, &color, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font size +2
      case nsIDOMEvent::VK_3:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            nsAutoString prop = "SIZE";
            nsAutoString value = "+2";
            mEditor->SetTextProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font size -2
      case nsIDOMEvent::VK_4:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            nsAutoString prop = "SIZE";
            nsAutoString value = "-2";
            mEditor->SetTextProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

              // hard-coded ChangeTextAttributes test -- font face helvetica
      case nsIDOMEvent::VK_5:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            nsAutoString prop = "FACE";
            nsAutoString value = "helvetica";
            mEditor->SetTextProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font face times
      case nsIDOMEvent::VK_6:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            nsAutoString prop = "FACE";
            nsAutoString value = "times";
            mEditor->SetTextProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded change structure test -- block H1
      case nsIDOMEvent::VK_7:
        case nsIDOMEvent::VK_Q:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            nsCOMPtr<nsIHTMLEditor>htmlEditor;
            htmlEditor = do_QueryInterface(mEditor);
            if (htmlEditor) 
            {
              nsAutoString tag;
              nsIEditProperty::h1->ToString(tag);
              htmlEditor->AddBlockParent(tag);
            }
          }
        }
        break;

      // hard-coded change structure test -- block H2
      case nsIDOMEvent::VK_8:
        case nsIDOMEvent::VK_W:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            nsCOMPtr<nsIHTMLEditor>htmlEditor;
            htmlEditor = do_QueryInterface(mEditor);
            if (htmlEditor) 
            {
              nsAutoString tag;
              nsIEditProperty::h2->ToString(tag);
              htmlEditor->AddBlockParent(tag);
            }
          }
        }
        break;

      // hard-coded change structure test -- normal
      case nsIDOMEvent::VK_9:
        case nsIDOMEvent::VK_E:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            nsCOMPtr<nsIHTMLEditor>htmlEditor;
            htmlEditor = do_QueryInterface(mEditor);
            if (htmlEditor) {
              htmlEditor->RemoveBlockParent();
            }
          }
        }
        break;


#ifdef NS_DEBUG
      // hard-coded Text Editor Unit Test
      case nsIDOMEvent::VK_T:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            TextEditorTest *tester = new TextEditorTest();
            if (tester)
            {
              tester->Run(mEditor);
            }
          }
        }
        break;
#endif

    }
  }
  return NS_OK;
}



/*
 * nsTextEditorMouseListener implementation
 */



NS_IMPL_ADDREF(nsTextEditorMouseListener)

NS_IMPL_RELEASE(nsTextEditorMouseListener)


nsTextEditorMouseListener::nsTextEditorMouseListener() 
{
  NS_INIT_REFCNT();
}



nsTextEditorMouseListener::~nsTextEditorMouseListener() 
{
}



nsresult
nsTextEditorMouseListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
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
  if (aIID.Equals(kIDOMMouseListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsresult
nsTextEditorMouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorMouseListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMNode> target;
  if (NS_OK == aMouseEvent->GetTarget(getter_AddRefs(target))) {
//    nsSetCurrentNode(aTarget);
  }
  //Should not be error.  Need a new way to do return values
  return NS_ERROR_BASE;
}



nsresult
nsTextEditorMouseListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorMouseListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorMouseListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorMouseListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorMouseListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}



/*
 * nsTextEditorMouseListener implementation
 */



NS_IMPL_ADDREF(nsTextEditorTextListener)

NS_IMPL_RELEASE(nsTextEditorTextListener)


nsTextEditorTextListener::nsTextEditorTextListener()
:	mCommitText(PR_FALSE),
	mInTransaction(PR_FALSE)
{
  NS_INIT_REFCNT();
}



nsTextEditorTextListener::~nsTextEditorTextListener() 
{
}


/*
 * nsTextEditorDragListener implementation
 */



NS_IMPL_ADDREF(nsTextEditorDragListener)

NS_IMPL_RELEASE(nsTextEditorDragListener)


nsTextEditorDragListener::nsTextEditorDragListener() 
{
  NS_INIT_REFCNT();
}



nsTextEditorDragListener::~nsTextEditorDragListener() 
{
}



nsresult
nsTextEditorTextListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMTextListenerIID, NS_IDOMTEXTLISTENER_IID);
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
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
  if (aIID.Equals(kIDOMTextListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMTextListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsTextEditorTextListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorTextListener::HandleText(nsIDOMEvent* aTextEvent)
{
	nsString				composedText;
	const PRUnichar*		composedTextAsChar;
	PRBool					commitText;
	nsresult				result;

	aTextEvent->GetText(composedText);
	composedTextAsChar = (const PRUnichar*)(&composedText);
	
	if (!mInTransaction) {
//		mEditor->BeginTransaction();
		mInTransaction = PR_TRUE;
	}

	aTextEvent->GetCommitText(&commitText);
	if (commitText) {
		mEditor->Undo(1);
		result = mEditor->InsertText(composedText);
//		result = mEditor->EndTransaction();
		mInTransaction=PR_FALSE;
		mCommitText = PR_TRUE;
	} else {
		if (!mCommitText) {
			mEditor->Undo(1);
		} else {
			mCommitText = PR_FALSE;
		}	
		result = mEditor->InsertText(composedText);
	}

	return result;
}


nsresult
nsTextEditorDragListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMDragListenerIID, NS_IDOMDRAGLISTENER_IID);
  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
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
  if (aIID.Equals(kIDOMDragListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMDragListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsresult
nsTextEditorDragListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorDragListener::DragEnter(nsIDOMEvent* aDragEvent)
{
#ifdef NEW_DRAG_AND_DROP
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                           kIDragServiceIID,
                                           (nsISupports **)&dragService);
  if (NS_OK == rv) {
    dragService->SetCanDrop(PR_TRUE);
    NS_RELEASE(dragService);
  }
#endif
  return NS_OK;
}


nsresult
nsTextEditorDragListener::DragOver(nsIDOMEvent* aDragEvent)
{
#ifdef NEW_DRAG_AND_DROP
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                           kIDragServiceIID,
                                           (nsISupports **)&dragService);
  if (NS_OK == rv) {
    dragService->SetCanDrop(PR_TRUE);
    NS_RELEASE(dragService);
  }
#endif
  return NS_OK;
}


nsresult
nsTextEditorDragListener::DragExit(nsIDOMEvent* aDragEvent)
{
  return NS_OK;
}



nsresult
nsTextEditorDragListener::DragDrop(nsIDOMEvent* aMouseEvent)
{

#ifdef NEW_DRAG_AND_DROP
  nsString stuffToPaste;
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             kIDragServiceIID,
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsIGenericTransferable *genericTrans = 0;
    rv = nsComponentManager::CreateInstance(kCGenericTransferableCID, nsnull, 
                                            kIGenericTransferableIID, (void**) &genericTrans);
    if (NS_OK == rv) {
      nsIDataFlavor *flavor = 0;
      rv = nsComponentManager::CreateInstance(kCDataFlavorCID, nsnull, kIDataFlavorIID, (void**) &flavor);
      if (NS_OK == rv) {
        flavor->Init(kTextMime, "Text");

        genericTrans->AddDataFlavor(flavor);

        nsCOMPtr<nsITransferable> trans = do_QueryInterface(genericTrans);
        if (trans) {

          dragService->GetData(trans);

          char *str = 0;
          PRUint32 len;
          trans->GetTransferData(flavor, (void **)&str, &len);

          if (str) {
            if (str[len-1] == 0) {
              len--;
            }
            stuffToPaste.SetString(str, len);
            mEditor->InsertText(stuffToPaste);
          }

        }
        NS_IF_RELEASE(flavor);
      }
      NS_IF_RELEASE(genericTrans);
    }
    NS_IF_RELEASE(dragService);
  }

#endif  
  
  //mEditor->InsertText(nsAutoString("hello"));
  return NS_OK;
}



/*
 * Factory functions
 */



nsresult 
NS_NewEditorKeyListener(nsIDOMEventListener ** aInstancePtrResult, 
                        nsITextEditor *aEditor)
{
  nsTextEditorKeyListener* it = new nsTextEditorKeyListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetEditor(aEditor);

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}



nsresult
NS_NewEditorMouseListener(nsIDOMEventListener ** aInstancePtrResult, 
                          nsITextEditor *aEditor)
{
  nsTextEditorMouseListener* it = new nsTextEditorMouseListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetEditor(aEditor);

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}


nsresult
NS_NewEditorTextListener(nsIDOMEventListener** aInstancePtrResult, nsITextEditor* aEditor)
{
	nsTextEditorTextListener*	it = new nsTextEditorTextListener();
	if (nsnull==it) {
		return NS_ERROR_OUT_OF_MEMORY;
	}

	it->SetEditor(aEditor);
	static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

	return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);
}



nsresult
NS_NewEditorDragListener(nsIDOMEventListener ** aInstancePtrResult, 
                          nsITextEditor *aEditor)
{
  nsTextEditorDragListener* it = new nsTextEditorDragListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetEditor(aEditor);

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}



