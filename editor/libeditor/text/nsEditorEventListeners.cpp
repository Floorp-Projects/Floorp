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
#include "nsIDOMSelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIEditProperty.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIStringStream.h"
#include "nsIDOMUIEvent.h"

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
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsIFormatConverter.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);


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

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aKeyEvent);
  if (!uiEvent) {
    //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

  if (NS_SUCCEEDED(uiEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(uiEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(uiEvent->GetCtrlKey(&ctrlKey))
      ) {
    PRBool keyProcessed;
    ProcessShortCutKeys(aKeyEvent, keyProcessed);
    if (PR_FALSE==keyProcessed)
    {
      switch(keyCode) {
      case nsIDOMUIEvent::VK_BACK:
        mEditor->DeleteSelection(nsIEditor::eDeleteLeft);
        break;

      case nsIDOMUIEvent::VK_DELETE:
        mEditor->DeleteSelection(nsIEditor::eDeleteRight);
        break;

      case nsIDOMUIEvent::VK_RETURN:
      //case nsIDOMUIEvent::VK_ENTER:			// why does this not exist?
        // Need to implement creation of either <P> or <BR> nodes.
        mEditor->InsertBreak();
        break;
      
      case nsIDOMUIEvent::VK_LEFT:
      case nsIDOMUIEvent::VK_RIGHT:
      case nsIDOMUIEvent::VK_UP:
      case nsIDOMUIEvent::VK_DOWN:
      	// these have already been handled in nsRangeList. Why are we getting them
      	// again here (Mac)? In switch to avoid putting in bogus chars.

        //return NS_OK to allow page scrolling.
        return NS_OK;
      	break;
      
      case nsIDOMUIEvent::VK_HOME:
      case nsIDOMUIEvent::VK_END:
      	// who handles these?
#if DEBUG
		printf("Key not handled\n");
#endif
        break;

      case nsIDOMUIEvent::VK_PAGE_UP:
      case nsIDOMUIEvent::VK_PAGE_DOWN:
        //return NS_OK to allow page scrolling.
        return NS_OK;
      	break;
      	
      default:
        {
          nsAutoString  key;
#ifdef HAVE_EVENT_CHARCODE
          PRUint32     character;
          // do we convert to Unicode here, or has this already been done? (sfraser)
          if (NS_SUCCEEDED(uiEvent->GetCharCode(&character)))
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

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aKeyEvent);
  if (!uiEvent) {
    //non-key event passed in.  bad things.
    return NS_OK;
  }

  if (NS_SUCCEEDED(uiEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(uiEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(uiEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(uiEvent->GetAltKey(&altKey))
      ) 
  {
    // XXX: please please please get these mappings from an external source!
    switch (keyCode)
    {
      // XXX: hard-coded select all
      case nsIDOMUIEvent::VK_A:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->SelectAll();
        }
        break;

      // XXX: hard-coded cut
      case nsIDOMUIEvent::VK_X:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Cut();
        }
        break;

      // XXX: hard-coded copy
      case nsIDOMUIEvent::VK_C:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Copy();
        }
        else if (PR_TRUE==altKey)
        {
          printf("Getting number of columns\n");
          aProcessed=PR_TRUE;
          PRInt32 wrap;
          if (NS_SUCCEEDED(mEditor->GetBodyWrapWidth(&wrap)))
            printf("Currently wrapping to %d\n", wrap);
          else
            printf("GetBodyWrapWidth returned an error\n");
        }
        break;

      case nsIDOMUIEvent::VK_OPEN_BRACKET:
        // hard coded "Decrease wrap size"
        if (PR_TRUE==altKey)
        {
          aProcessed=PR_TRUE;
          PRInt32 wrap;
          if (!NS_SUCCEEDED(mEditor->GetBodyWrapWidth(&wrap)))
          {
            printf("GetBodyWrapWidth returned an error\n");
            break;
          }
          mEditor->SetBodyWrapWidth(wrap - 5);
          if (!NS_SUCCEEDED(mEditor->GetBodyWrapWidth(&wrap)))
          {
            printf("Second GetBodyWrapWidth returned an error\n");
            break;
          }
          else printf("Now wrapping to %d\n", wrap);
        }
        break;

      case nsIDOMUIEvent::VK_CLOSE_BRACKET:
        // hard coded "Increase wrap size"
        if (PR_TRUE==altKey)
        {
          aProcessed=PR_TRUE;
          PRInt32 wrap;
          if (!NS_SUCCEEDED(mEditor->GetBodyWrapWidth(&wrap)))
          {
            printf("GetBodyWrapWidth returned an error\n");
            break;
          }
          mEditor->SetBodyWrapWidth(wrap + 5);
          if (!NS_SUCCEEDED(mEditor->GetBodyWrapWidth(&wrap)))
          {
            printf("Second GetBodyWrapWidth returned an error\n");
            break;
          }
          else printf("Now wrapping to %d\n", wrap);
        }
        break;

      // XXX: hard-coded paste
      case nsIDOMUIEvent::VK_V:
        if (PR_TRUE==ctrlKey)
        {
          printf("control-v\n");
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Paste();
        }
        break;

      // XXX: hard-coded undo
      case nsIDOMUIEvent::VK_Z:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Undo(1);
        }
        break;

      // XXX: hard-coded redo
      case nsIDOMUIEvent::VK_Y:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
            mEditor->Redo(1);
        }
        break;

      // hard-coded ChangeTextAttributes test -- italics
      case nsIDOMUIEvent::VK_I:
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
          nsCOMPtr<nsIHTMLEditor> htmlEditor (do_QueryInterface(mEditor));
          if (htmlEditor)
          {
            nsString nsstr ("<b>This is bold <em>and emphasized</em></b>");
            nsresult res = htmlEditor->Insert(nsstr);
            if (!NS_SUCCEEDED(res))
              printf("nsTextEditor::Insert(string) failed\n");
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- bold
      case nsIDOMUIEvent::VK_B:
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
      case nsIDOMUIEvent::VK_U:
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
      case nsIDOMUIEvent::VK_1:
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
            mEditor->GetTextProperty(nsIEditProperty::font, &color, &value, first, any, all);
            if (!all) {
              mEditor->SetTextProperty(nsIEditProperty::font, &color, &value);
            }
            else {
              printf("NOOP: all selected text is already red\n");
            }
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- remove font color
      case nsIDOMUIEvent::VK_2:
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
            mEditor->GetTextProperty(nsIEditProperty::font, &color, nsnull, first, any, all);
            if (any) {
              mEditor->RemoveTextProperty(nsIEditProperty::font, &color);
            }
            else {
              printf("NOOP: no color set\n");
            }
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font size +2
      case nsIDOMUIEvent::VK_3:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            //PRBool any = PR_FALSE;
            //PRBool all = PR_FALSE;
            //PRBool first = PR_FALSE;
            nsAutoString prop = "SIZE";
            nsAutoString value = "+2";
            mEditor->SetTextProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font size -2
      case nsIDOMUIEvent::VK_4:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            //PRBool any = PR_FALSE;
            //PRBool all = PR_FALSE;
            //PRBool first = PR_FALSE;
            nsAutoString prop = "SIZE";
            nsAutoString value = "-2";
            mEditor->SetTextProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font face helvetica
      case nsIDOMUIEvent::VK_5:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            //PRBool any = PR_FALSE;
            //PRBool all = PR_FALSE;
            //PRBool first = PR_FALSE;
            nsAutoString prop = "FACE";
            nsAutoString value = "helvetica";
            mEditor->SetTextProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font face times
      case nsIDOMUIEvent::VK_6:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            //PRBool any = PR_FALSE;
            //PRBool all = PR_FALSE;
            //PRBool first = PR_FALSE;
            nsAutoString prop = "FACE";
            nsAutoString value = "times";
            mEditor->SetTextProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded change structure test -- transform block H1
      case nsIDOMUIEvent::VK_7:
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
              htmlEditor->ReplaceBlockParent(tag);
            }
          }
        }
        break;

      // hard-coded change structure test -- transform block H2
      case nsIDOMUIEvent::VK_8:
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
              htmlEditor->ReplaceBlockParent(tag);
            }
          }
        }
        break;

      // hard-coded change structure test -- normal
      case nsIDOMUIEvent::VK_9:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            nsCOMPtr<nsIHTMLEditor>htmlEditor;
            htmlEditor = do_QueryInterface(mEditor);
            if (htmlEditor) {
              htmlEditor->RemoveParagraphStyle();
            }
          }
        }
        break;

      // hard-coded change structure test -- GetParagraphStyle
      case nsIDOMUIEvent::VK_0:
        if (PR_TRUE==ctrlKey)
        {
          aProcessed=PR_TRUE;
          if (mEditor)
          {
            nsCOMPtr<nsIHTMLEditor>htmlEditor;
            htmlEditor = do_QueryInterface(mEditor);
            if (htmlEditor) 
            {
              printf("testing GetParagraphStyle\n");
              nsStringArray styles;
              nsresult result = htmlEditor->GetParagraphStyle(&styles);
              if (NS_SUCCEEDED(result))
              {
                PRInt32 count = styles.Count();
                PRInt32 i;
                for (i=0; i<count; i++)
                {
                  nsString *tag = styles.StringAt(i);
                  char *tagCString = tag->ToNewCString();
                  printf("%s ", tagCString);
                  delete [] tagCString;
                }
                printf("\n");
              }
            }
          }
        }
        break;

      // hard-coded change structure test -- block blockquote (indent)
      case nsIDOMUIEvent::VK_COMMA:
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
              nsIEditProperty::blockquote->ToString(tag);
              htmlEditor->AddBlockParent(tag);
            }
          }
        }
        break;

      // hard-coded change structure test -- un-BlockQuote
      case nsIDOMUIEvent::VK_PERIOD:
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
              nsIEditProperty::blockquote->ToString(tag);
              htmlEditor->RemoveParent(tag);
            }
          }
        }
        break;


#ifdef NS_DEBUG
      // hard-coded Text Editor Unit Test
      case nsIDOMUIEvent::VK_T:
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
  if (!aMouseEvent)
    return NS_OK;   // NS_OK means "we didn't process the event".  Go figure.

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aMouseEvent);
  if (!uiEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // We only do anything special for middle-mouse click (paste);
  // ignore all other events.
  PRUint32 button = 0;
  uiEvent->GetButton(&button);
  if (button != 2)
    return NS_OK;

  nsCOMPtr<nsIEditor> editor (do_QueryInterface(mEditor));
  if (!editor)
    return NS_ERROR_FAILURE;

  // There's currently no way to set the selection to the mouse position
  // outside of liblayout -- nsIDOMEvent has an API to get the node under
  // the mouse, but not to get the offset within the node.
#ifdef PASTE_TO_MOUSE_POSITION
  nsCOMPtr<nsIDOMNode> target;
  if (NS_SUCCEEDED(aMouseEvent->GetTarget(getter_AddRefs(target))))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    if (NS_SUCCEEDED(editor->GetSelection(getter_AddRefs(selection))))
    {
      if (NS_SUCCEEDED(selection->Collapse(target, 0)))
      {
        editor->Paste();
      }
    }
  }
#else /* PASTE_TO_MOUSE_POSITION */
  // Until we can get node AND offset,
  // it's better to paste to the current selection position:
  editor->Paste();
#endif /* PASTE_TO_MOUSE_POSITION */

  return NS_ERROR_BASE; // NS_ERROR_BASE means "We did process the event".
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
	nsresult				result;

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aTextEvent);
  if (!uiEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

	uiEvent->GetText(composedText);
	result = mEditor->SetCompositionString(composedText);
	return result;
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
                                             nsIDragService::GetIID(),
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));

    nsAutoString textFlavor(kTextMime);
    if (dragSession && NS_OK == dragSession->IsDataFlavorSupported(&textFlavor)) {
      dragSession->SetCanDrop(PR_TRUE);
    }
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
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
                                           nsIDragService::GetIID(),
                                           (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
    nsAutoString textFlavor(kTextMime);
    if (dragSession && NS_OK == dragSession->IsDataFlavorSupported(&textFlavor)) {
      dragSession->SetCanDrop(PR_TRUE);
    } 
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
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
  // String for doing paste
  nsString stuffToPaste;

  // Create drag service for getting state of drag
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
  
    if (dragSession) {

      // Create transferable for getting the drag data
      nsCOMPtr<nsITransferable> trans;
      rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                              nsITransferable::GetIID(), 
                                              (void**) getter_AddRefs(trans));
      if ( NS_SUCCEEDED(rv) && trans ) {
        // Add the text Flavor to the transferable, 
        // because that is the only type of data we are looking for at the moment
        nsAutoString textMime(kTextMime);
        trans->AddDataFlavor(&textMime);
        //genericTrans->AddDataFlavor(mImageDataFlavor);

        if (trans) {

          // Fill the transferable with our requested data flavor
          dragSession->GetData(trans);

          // Get the string data out of the transferable
          // Note: the transferable owns the pointer to the data
          char *str = 0;
          PRUint32 len;
          trans->GetTransferData(&textMime, (void **)&str, &len);

          // If the string was not empty then paste it in
          if (str) {
            stuffToPaste.SetString(str, len);
            mEditor->InsertText(stuffToPaste);
            dragSession->SetCanDrop(PR_TRUE);
          }

          // XXX This is where image support might go
          //void * data;
          //trans->GetTransferData(mImageDataFlavor, (void **)&data, &len);

        }
      }
    }
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  }

#endif  
  
  return NS_OK;
}


nsTextEditorCompositionListener::nsTextEditorCompositionListener()
{
  NS_INIT_REFCNT();
}

nsTextEditorCompositionListener::~nsTextEditorCompositionListener() 
{
}


nsresult
nsTextEditorCompositionListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMCompositionListenerIID, NS_IDOMCOMPOSITIONLISTENER_IID);
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
  if (aIID.Equals(kIDOMCompositionListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMCompositionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsTextEditorCompositionListener)

NS_IMPL_RELEASE(nsTextEditorCompositionListener)

nsresult
nsTextEditorCompositionListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}
nsresult
nsTextEditorCompositionListener::HandleStartComposition(nsIDOMEvent* aCompositionEvent)
{
	return mEditor->BeginComposition();
}

nsresult
nsTextEditorCompositionListener::HandleEndComposition(nsIDOMEvent* aCompositionEvent)
{
	return mEditor->EndComposition();
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

nsresult
NS_NewEditorCompositionListener(nsIDOMEventListener** aInstancePtrResult, nsITextEditor* aEditor)
{
	nsTextEditorCompositionListener*	it = new nsTextEditorCompositionListener();
	if (nsnull==it) {
		return NS_ERROR_OUT_OF_MEMORY;
	}

	it->SetEditor(aEditor);
	static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);

	return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);
}


