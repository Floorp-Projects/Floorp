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
#include "nsVoidArray.h"
#include "nsString.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMSelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIEditProperty.h"
#include "nsISupportsArray.h"
#include "nsIStringStream.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIPrivateTextEvent.h"
#include "nsIEditorMailSupport.h"

// for repainting hack only
#include "nsIView.h"
#include "nsIViewManager.h"
// end repainting hack only

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsIFormatConverter.h"
#include "nsIContentIterator.h"
#include "nsIContent.h"
#include "nsLayoutCID.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kContentIteratorCID,      NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCClipboardCID,           NS_CLIPBOARD_CID);
static NS_DEFINE_IID(kCXIFConverterCID,        NS_XIFFORMATCONVERTER_CID);



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
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMKeyListener::GetIID())) {
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

// individual key handlers return NS_OK to indicate NOT consumed
// by default, an error is returned indicating event is consumed
// joki is fixing this interface.
nsresult
nsTextEditorKeyListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  PRUint32 keyCode;
  PRBool   isShift, ctrlKey, altKey, metaKey;

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aKeyEvent);
  if (!uiEvent) {
    //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

//  PRBool keyProcessed;
//  ProcessShortCutKeys(aKeyEvent, keyProcessed);
//  if (PR_FALSE==keyProcessed)
	{
		if (NS_SUCCEEDED(uiEvent->GetKeyCode(&keyCode)) && 
				NS_SUCCEEDED(uiEvent->GetShiftKey(&isShift)) &&
				NS_SUCCEEDED(uiEvent->GetCtrlKey(&ctrlKey)) &&
				NS_SUCCEEDED(uiEvent->GetAltKey(&altKey)) &&
				NS_SUCCEEDED(uiEvent->GetMetaKey(&metaKey)))
		{
			 {
				switch(keyCode) {
	//      case nsIDOMUIEvent::VK_BACK:
	//        mEditor->DeleteSelection(nsIEditor::eDeleteLeft);
	//        break;

				case nsIDOMUIEvent::VK_DELETE:
					mEditor->DeleteSelection(nsIEditor::eDeleteNext);
					break;

	//      case nsIDOMUIEvent::VK_RETURN:
				//case nsIDOMUIEvent::VK_ENTER:			// why does this not exist?
					// Need to implement creation of either <P> or <BR> nodes.
	//        mEditor->InsertBreak();
	//        break;
      
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
					return NS_OK;
					break;

				case nsIDOMUIEvent::VK_PAGE_UP:
				case nsIDOMUIEvent::VK_PAGE_DOWN:
					//return NS_OK to allow page scrolling.
					return NS_OK;
      		break;

				case nsIDOMUIEvent::VK_TAB:
				{
					PRUint32 flags=0;
					mEditor->GetFlags(&flags);
					if (! (flags & nsIHTMLEditor::eEditorSingleLineMask))
					{
						if (metaKey || altKey)
							return NS_OK;	// don't consume
						// else we insert the tab straight through  
 						nsAutoString key;
						key += keyCode;

						nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
 						if (htmlEditor)
 							htmlEditor->InsertText(key);
						return NS_ERROR_BASE; // this means "I handled the event, don't do default processing"
					}
					else {
						return NS_OK;
					}
					break;
				}

				default:
					return NS_OK; // this indicates that we have not handled the keyDown event in any way.
				}
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
	nsAutoString  key;
	PRUint32     character;
	PRUint32 keyCode;

	nsCOMPtr<nsIDOMUIEvent>uiEvent;
	uiEvent = do_QueryInterface(aKeyEvent);
	if (!uiEvent) {
		//non-key event passed to keydown.  bad things.
		return NS_OK;
	}
	//
	// look at the keyCode if it is return or backspace, process it
	//	we handle these two special characters here because it makes windows integration
	//	eaiser
	//

  PRBool keyProcessed;
  ProcessShortCutKeys(aKeyEvent, keyProcessed);
  if (PR_FALSE==keyProcessed)
  {
	PRBool ctrlKey, altKey, metaKey;
	uiEvent->GetCtrlKey(&ctrlKey);
	uiEvent->GetAltKey(&altKey);
	uiEvent->GetMetaKey(&metaKey);

	if (metaKey)
		return NS_OK;	// don't consume

	nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
	if (!htmlEditor) return NS_ERROR_NO_INTERFACE;

	if (NS_SUCCEEDED(uiEvent->GetKeyCode(&keyCode)))
	{
		if (nsIDOMUIEvent::VK_BACK==keyCode) {
			mEditor->DeleteSelection(nsIEditor::eDeletePrevious);
			return NS_ERROR_BASE; // consumed
		}	
		if (nsIDOMUIEvent::VK_RETURN==keyCode) {
			htmlEditor->InsertBreak();
			return NS_ERROR_BASE; // consumed
		}
	}
	
 	if ((PR_FALSE==altKey) && (PR_FALSE==ctrlKey) &&
			(NS_SUCCEEDED(uiEvent->GetCharCode(&character))))
 	{
		if (nsIDOMUIEvent::VK_TAB==character) {
			return NS_OK; // ignore tabs here, they're handled in keyDown if at all
		}
 		key += character;
 		htmlEditor->InsertText(key);
 	}
	}

	return NS_ERROR_BASE; // consumed
  
}


/* these includes are for debug only.  this module should never instantiate it's own transactions */
#include "SplitElementTxn.h"
#include "TransactionFactory.h"

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

  if (NS_SUCCEEDED(uiEvent->GetCharCode(&keyCode)) && 
//  if (NS_SUCCEEDED(uiEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(uiEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(uiEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(uiEvent->GetAltKey(&altKey))
      ) 
  {
    if (PR_TRUE==ctrlKey) {
      aProcessed = PR_TRUE;
    } 
    // swallow all control keys
    // XXX: please please please get these mappings from an external source!
    switch (keyCode)
    {
      // XXX: hard-coded select all
      case nsIDOMUIEvent::VK_A:
        if (PR_TRUE==ctrlKey)
        {
          if (mEditor)
            mEditor->SelectAll();
        }
        break;

      // XXX: hard-coded cut
      case nsIDOMUIEvent::VK_X:
        if (PR_TRUE==ctrlKey)
        {
          if (mEditor)
            mEditor->Cut();
        }
        else if (PR_TRUE==altKey)
        {
          aProcessed=PR_TRUE;
          nsString output;
          nsresult res = NS_ERROR_FAILURE;
          nsString format;
          if (isShift)
            format = "text/plain";
          else
            format = "text/html";
          res = mEditor->OutputToString(output, format,
                                        nsEditor::EditorOutputFormatted);
          if (NS_SUCCEEDED(res))
          {
            char* buf = output.ToNewCString();
            if (buf)
            {
              puts(buf);
              delete[] buf;
            }
          }
        }
        break;

      // XXX: hard-coded copy
      case nsIDOMUIEvent::VK_C:
        if (PR_TRUE==ctrlKey)
        {
          if (mEditor)
            mEditor->Copy();
        }
        else if (PR_TRUE==altKey)
        {
          printf("Getting number of columns\n");
          aProcessed=PR_TRUE;
          nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
          if (mailEditor)
          {
	          PRInt32 wrap;
	          if (NS_SUCCEEDED(mailEditor->GetBodyWrapWidth(&wrap)))
	            printf("Currently wrapping to %d\n", wrap);
	          else
	            printf("GetBodyWrapWidth returned an error\n");	            
          }
        }
        break;

      case nsIDOMUIEvent::VK_OPEN_BRACKET:
        // hard coded "Decrease wrap size"
        if (PR_TRUE==altKey)
        {
	        nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
	        if (mailEditor)
	        {
	          aProcessed=PR_TRUE;
	          PRInt32 wrap;
	          if (!NS_SUCCEEDED(mailEditor->GetBodyWrapWidth(&wrap)))
	          {
	            printf("GetBodyWrapWidth returned an error\n");
	            break;
	          }
	          mailEditor->SetBodyWrapWidth(wrap - 5);
	          if (!NS_SUCCEEDED(mailEditor->GetBodyWrapWidth(&wrap)))
	          {
	            printf("Second GetBodyWrapWidth returned an error\n");
	            break;
	          }
	          else printf("Now wrapping to %d\n", wrap);
	        }
        }
        break;

      case nsIDOMUIEvent::VK_CLOSE_BRACKET:
        // hard coded "Increase wrap size"
        if (PR_TRUE==altKey)
        {
	        nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
	        if (mailEditor)
	        {
	          aProcessed=PR_TRUE;
	          PRInt32 wrap;
	          if (!NS_SUCCEEDED(mailEditor->GetBodyWrapWidth(&wrap)))
	          {
	            printf("GetBodyWrapWidth returned an error\n");
	            break;
	          }
	          mailEditor->SetBodyWrapWidth(wrap + 5);
	          if (!NS_SUCCEEDED(mailEditor->GetBodyWrapWidth(&wrap)))
	          {
	            printf("Second GetBodyWrapWidth returned an error\n");
	            break;
	          }
	          else printf("Now wrapping to %d\n", wrap);
	        }
        }
        break;

      // Hard coded "No wrap" or "wrap to window size"
      case nsIDOMUIEvent::VK_BACK_SLASH:
      {
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIEditorMailSupport> mailEditor =
            do_QueryInterface(mEditor);
          if (mailEditor)
          {
            aProcessed=PR_TRUE;
            mailEditor->SetBodyWrapWidth(0);
          }
        }
        else if (PR_TRUE==altKey)
        {
          nsCOMPtr<nsIEditorMailSupport> mailEditor =
            do_QueryInterface(mEditor);
          if (mailEditor)
          {
            aProcessed=PR_TRUE;
            mailEditor->SetBodyWrapWidth(-1);
          }
        }
      }

      // XXX: hard-coded paste
      case nsIDOMUIEvent::VK_V:
        if (PR_TRUE==ctrlKey)
        {
          printf("control-v\n");
          if (mEditor)
          {
            if (altKey)
            {
              nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
              if (mailEditor)
                mailEditor->PasteAsQuotation();
            }
            else
              mEditor->Paste();
          }
        }
        break;

      // XXX: hard-coded undo
      case nsIDOMUIEvent::VK_Z:
        if (PR_TRUE==ctrlKey)
        {
          if (mEditor)
            mEditor->Undo(1);
        }
        break;

      // XXX: hard-coded redo
      case nsIDOMUIEvent::VK_Y:
        if (PR_TRUE==ctrlKey)
        {
          if (mEditor)
            mEditor->Redo(1);
        }
        break;

      // hard-coded ChangeTextAttributes test -- italics
      case nsIDOMUIEvent::VK_I:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            htmlEditor->GetInlineProperty(nsIEditProperty::i, nsnull, nsnull, first, any, all);
            if (PR_FALSE==first) {
              htmlEditor->SetInlineProperty(nsIEditProperty::i, nsnull, nsnull);
            }
            else {
              htmlEditor->RemoveInlineProperty(nsIEditProperty::i, nsnull);
            }
          }
        }

      // Hardcoded Insert Arbitrary HTML
        else if (PR_TRUE==altKey)
        {
          aProcessed=PR_TRUE;
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            nsString nsstr ("This is <b>bold <em>and emphasized</em></b> text");
            htmlEditor->InsertHTML(nsstr);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- bold
      case nsIDOMUIEvent::VK_B:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            htmlEditor->GetInlineProperty(nsIEditProperty::b, nsnull, nsnull, first, any, all);
            if (PR_FALSE==first) {
              htmlEditor->SetInlineProperty(nsIEditProperty::b, nsnull, nsnull);
            }
            else {
              htmlEditor->RemoveInlineProperty(nsIEditProperty::b, nsnull);
            }
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- underline
      case nsIDOMUIEvent::VK_U:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            htmlEditor->GetInlineProperty(nsIEditProperty::u, nsnull, nsnull, first, any, all);
            if (PR_FALSE==first) {
              htmlEditor->SetInlineProperty(nsIEditProperty::u, nsnull, nsnull);
            }
            else {
              htmlEditor->RemoveInlineProperty(nsIEditProperty::u, nsnull);
            }

          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font color red
      case nsIDOMUIEvent::VK_1:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            nsAutoString color = "COLOR";
            nsAutoString value = "red";
            htmlEditor->GetInlineProperty(nsIEditProperty::font, &color, &value, first, any, all);
            if (!all) {
              htmlEditor->SetInlineProperty(nsIEditProperty::font, &color, &value);
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
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            PRBool any = PR_FALSE;
            PRBool all = PR_FALSE;
            PRBool first = PR_FALSE;
            nsAutoString color = "COLOR";
            htmlEditor->GetInlineProperty(nsIEditProperty::font, &color, nsnull, first, any, all);
            if (any) {
              htmlEditor->RemoveInlineProperty(nsIEditProperty::font, &color);
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
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            //PRBool any = PR_FALSE;
            //PRBool all = PR_FALSE;
            //PRBool first = PR_FALSE;
            nsAutoString prop = "SIZE";
            nsAutoString value = "+2";
            htmlEditor->SetInlineProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font size -2
      case nsIDOMUIEvent::VK_4:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            //PRBool any = PR_FALSE;
            //PRBool all = PR_FALSE;
            //PRBool first = PR_FALSE;
            nsAutoString prop = "SIZE";
            nsAutoString value = "-2";
            htmlEditor->SetInlineProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font face helvetica
      case nsIDOMUIEvent::VK_5:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            //PRBool any = PR_FALSE;
            //PRBool all = PR_FALSE;
            //PRBool first = PR_FALSE;
            nsAutoString prop = "FACE";
            nsAutoString value = "helvetica";
            htmlEditor->SetInlineProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded ChangeTextAttributes test -- font face times
      case nsIDOMUIEvent::VK_6:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            // XXX: move this logic down into texteditor rules delegate
            //      should just call mEditor->ChangeTextProperty(prop)
            //PRBool any = PR_FALSE;
            //PRBool all = PR_FALSE;
            //PRBool first = PR_FALSE;
            nsAutoString prop = "FACE";
            nsAutoString value = "times";
            htmlEditor->SetInlineProperty(nsIEditProperty::font, &prop, &value);
          }
        }
        break;

      // hard-coded change structure test -- transform block H1
      case nsIDOMUIEvent::VK_7:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor) 
          {
            nsAutoString tag;
            nsIEditProperty::h1->ToString(tag);
            htmlEditor->ReplaceBlockParent(tag);
          }
        }
        break;

      // hard-coded change structure test -- transform block H2
      case nsIDOMUIEvent::VK_8:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor) 
          {
            nsAutoString tag;
            nsIEditProperty::h2->ToString(tag);
            htmlEditor->ReplaceBlockParent(tag);
          }
        }
        break;

      // hard-coded change structure test -- normal
      case nsIDOMUIEvent::VK_9:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor) {
            htmlEditor->RemoveParagraphStyle();
          }
        }
        break;

      // hard-coded change structure test -- GetParagraphStyle
      case nsIDOMUIEvent::VK_0:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
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
        break;

      // hard-coded change structure test -- block blockquote (indent)
      case nsIDOMUIEvent::VK_COMMA:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor)
          {
            nsAutoString tag;
            nsIEditProperty::blockquote->ToString(tag);
            htmlEditor->AddBlockParent(tag);
          }
        }
        break;

      // hard-coded change structure test -- un-BlockQuote
      case nsIDOMUIEvent::VK_PERIOD:
        if (PR_TRUE==ctrlKey)
        {
          nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
          if (htmlEditor) 
          {
            nsAutoString tag;
            nsIEditProperty::blockquote->ToString(tag);
            htmlEditor->RemoveParent(tag);
          }
        }
        break;


#ifdef NS_DEBUG
      // hard-coded Text Editor Unit Test
      case nsIDOMUIEvent::VK_T:
        if (PR_TRUE==ctrlKey)
        {
          if (mEditor)
          {
            PRInt32  numTests, numFailed;
            // the unit tests are only exposed through nsIEditor
            nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
            if (editor)
	            editor->DebugUnitTests(&numTests, &numFailed);
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

  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMMouseListener::GetIID())) {
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



// XXX: evil global functions, move them to proper support module

nsresult
IsNodeInSelection(nsIDOMNode *aInNode, nsIDOMSelection *aInSelection, PRBool &aOutIsInSel)
{
	aOutIsInSel = PR_FALSE;	// init out-param
	if (!aInNode || !aInSelection) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIContentIterator>iter;
  nsresult result = nsComponentManager::CreateInstance(kContentIteratorCID, nsnull,
                                                       nsIContentIterator::GetIID(), 
																											 getter_AddRefs(iter));
	if (NS_FAILED(result)) { return result; }
	if (!iter) { return NS_ERROR_OUT_OF_MEMORY; }

	nsCOMPtr<nsIEnumerator> enumerator;
  result = aInSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) { return result; }
	if (!enumerator) { return NS_ERROR_OUT_OF_MEMORY; }

  for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
  {
    nsCOMPtr<nsISupports> currentItem;
    result = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if ((NS_SUCCEEDED(result)) && (currentItem))
    {
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

			iter->Init(range);
			nsCOMPtr<nsIContent> currentContent;
			iter->CurrentNode(getter_AddRefs(currentContent));
			while (NS_COMFALSE == iter->IsDone())
			{
				nsCOMPtr<nsIDOMNode>currentNode = do_QueryInterface(currentContent);
				if (currentNode.get()==aInNode)
				{
					// if it's a start or end node, need to test whether the (x,y) 
					// of the event falls within the selection

					// talk to Mike

					aOutIsInSel = PR_TRUE;
					return NS_OK;
				}
				/* do not check result here, and especially do not return the result code.
				 * we rely on iter->IsDone to tell us when the iteration is complete
				 */
				iter->Next();
				iter->CurrentNode(getter_AddRefs(currentContent));
			}
		}
	}
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

	nsCOMPtr<nsIEditor> editor (do_QueryInterface(mEditor));
  if (!editor) { return NS_OK; }

  
	// get the document
	nsCOMPtr<nsIDOMDocument>domDoc;
  editor->GetDocument(getter_AddRefs(domDoc));
	if (!domDoc) { return NS_OK; }
	nsCOMPtr<nsIDocument>doc = do_QueryInterface(domDoc);
	if (!doc) { return NS_OK; }

  PRUint16 button = 0;
  uiEvent->GetButton(&button);
	// left button click might be a drag-start
	if (1==button)
	{
#ifndef EXPERIMENTAL_DRAG_CODE
		return NS_OK;
#else
		nsString XIFBuffer;
		// get the DOM select here
		nsCOMPtr<nsIDOMSelection> sel;
		editor->GetSelection(getter_AddRefs(sel));
    
		// convert the DOMselection to XIF
		if (sel)
		{
			// if we are within the selection, start the drag

			nsCOMPtr<nsIDOMNode> target;
			nsresult rv = aMouseEvent->GetTarget(getter_AddRefs(target));
			if (NS_FAILED(rv) || !target) { return NS_OK; }
			PRBool isInSel;
			rv = IsNodeInSelection(target, sel, isInSel);
			if (NS_FAILED(rv) || PR_FALSE==isInSel) { return NS_OK; }
			doc->CreateXIF(XIFBuffer, sel);

			// Get the Clipboard
			nsIClipboard* clipboard;
			rv = nsServiceManager::GetService(kCClipboardCID,
			    															nsIClipboard::GetIID(),
																			  (nsISupports **)&clipboard);
			if (NS_OK == rv) 
			{
				// Create a data flavor to tell the transferable 
				// that it is about to receive XIF
				nsAutoString flavor(kXIFMime);

				// Create a transferable for putting data on the Clipboard
				nsCOMPtr<nsITransferable> trans;
				rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
																								nsITransferable::GetIID(), 
																								(void**) getter_AddRefs(trans));
				if (NS_OK == rv) {
					// The data on the clipboard will be in "XIF" format
					// so give the clipboard transferable a "XIFConverter" for 
					// converting from XIF to other formats
					nsCOMPtr<nsIFormatConverter> xifConverter;
					rv = nsComponentManager::CreateInstance(kCXIFConverterCID, nsnull, 
																									nsIFormatConverter::GetIID(), (void**) getter_AddRefs(xifConverter));
					if (NS_OK == rv) {
						// Add the XIF DataFlavor to the transferable
						// this tells the transferable that it can handle receiving the XIF format
						trans->AddDataFlavor(&flavor);

						// Add the converter for going from XIF to other formats
						trans->SetConverter(xifConverter);

						// Now add the XIF data to the transferable
						// the transferable wants the number bytes for the data and since it is double byte
						// we multiply by 2
						trans->SetTransferData(&flavor, XIFBuffer.ToNewUnicode(), XIFBuffer.Length()*2);

						// Now invoke the drag session
						nsIDragService* dragService; 
						nsresult rv = nsServiceManager::GetService(kCDragServiceCID, 
																											 nsIDragService::GetIID(), 
																											 (nsISupports **)&dragService); 
						if (NS_OK == rv) { 
							nsCOMPtr<nsISupportsArray> items;
							NS_NewISupportsArray(getter_AddRefs(items));
							if ( items ) {
								items->AppendElement(trans);
								dragService->InvokeDragSession(items, nsnull, nsIDragService::DRAGDROP_ACTION_COPY | nsIDragService::DRAGDROP_ACTION_MOVE);
							}
							nsServiceManager::ReleaseService(kCDragServiceCID, dragService); 
						} 
					}
				}
				nsServiceManager::ReleaseService(kCClipboardCID, clipboard);
			}
			return NS_ERROR_BASE;	// return that we've handled the event
		}
#endif
  }
  // middle-mouse click (paste);
  else if (button == 2)
	{

    // Set the selection to the point under the mouse cursor:
		nsCOMPtr<nsIDOMNSUIEvent> mouseEvent (do_QueryInterface(aMouseEvent));

		if (!mouseEvent)
			return NS_ERROR_BASE; // NS_ERROR_BASE means "We did process the event".
		nsCOMPtr<nsIDOMNode> parent;
		if (!NS_SUCCEEDED(mouseEvent->GetRangeParent(getter_AddRefs(parent))))
			return NS_ERROR_BASE; // NS_ERROR_BASE means "We did process the event".
		PRInt32 offset = 0;
		if (!NS_SUCCEEDED(mouseEvent->GetRangeOffset(&offset)))
			return NS_ERROR_BASE; // NS_ERROR_BASE means "We did process the event".

		nsCOMPtr<nsIDOMSelection> selection;
		if (NS_SUCCEEDED(editor->GetSelection(getter_AddRefs(selection))))
			(void)selection->Collapse(parent, offset);

		// If the ctrl key is pressed, we'll do paste as quotation.
		// Would've used the alt key, but the kde wmgr treats alt-middle specially. 
		PRBool ctrlKey = PR_FALSE;
		uiEvent->GetCtrlKey(&ctrlKey);

		if (ctrlKey)
		{
			nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
			if (mailEditor)
				mailEditor->PasteAsQuotation();
		}
		editor->Paste();
		return NS_ERROR_BASE; // NS_ERROR_BASE means "We did process the event".
	}
	return NS_OK;	// did not process the event
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
	nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
  if (htmlEditor)
  {
    nsCOMPtr<nsIDOMElement> selectedElement;
    if (NS_SUCCEEDED(htmlEditor->GetSelectedElement("", getter_AddRefs(selectedElement))) && selectedElement)
    {
      nsAutoString TagName;
      selectedElement->GetTagName(TagName);
      TagName.ToLowerCase();

#if DEBUG_cmanske
      char szTagName[64];
      TagName.ToCString(szTagName, 64);
      printf("Single Selected element found: %s\n", szTagName);
#endif
    }
  }
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

  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMTextListener::GetIID())) {
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
	nsresult				result = NS_OK;
	nsCOMPtr<nsIPrivateTextEvent> textEvent;
	nsIPrivateTextRangeList		*textRangeList;
	nsTextEventReply			*textEventReply;

	textEvent = do_QueryInterface(aTextEvent);
	if (!textEvent) {
		//non-ui event passed in.  bad things.
		return NS_OK;
	}

	textEvent->GetText(composedText);
	textEvent->GetInputRange(&textRangeList);
	textEvent->GetEventReply(&textEventReply);
	textRangeList->AddRef();
	nsCOMPtr<nsIEditorIMESupport> imeEditor = do_QueryInterface(mEditor, &result);
	if (imeEditor)
    result = imeEditor->SetCompositionString(composedText,textRangeList,textEventReply);
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

  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMDragListener::GetIID())) {
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
nsTextEditorDragListener::DragGesture(nsIDOMEvent* aDragEvent)
{
  // ...figure out if a drag should be started...
  
  return NS_OK;
}


nsresult
nsTextEditorDragListener::DragEnter(nsIDOMEvent* aDragEvent)
{
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));

    nsAutoString textFlavor(kTextMime);
    if (dragSession && 
        (NS_OK == dragSession->IsDataFlavorSupported(&textFlavor))) {
      dragSession->SetCanDrop(PR_TRUE);
    }
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  }

  return NS_OK;
}


nsresult
nsTextEditorDragListener::DragOver(nsIDOMEvent* aDragEvent)
{
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
        // because that is the only type of data we are
        // looking for at the moment.
        nsAutoString textMime (kTextMime);
        trans->AddDataFlavor(&textMime);
        //trans->AddDataFlavor(mImageDataFlavor);

        // Fill the transferable with data for each drag item in succession
        PRUint32 numItems = 0; 
        if (NS_SUCCEEDED(dragSession->GetNumDropItems(&numItems))) { 

          printf("Num Drop Items %d\n", numItems); 

          PRUint32 i; 
          for (i=0;i<numItems;++i) {
            if (NS_SUCCEEDED(dragSession->GetData(trans, i))) { 
 
              // Get the string data out of the transferable
              // Note: the transferable owns the pointer to the data
              char *str = 0;
              PRUint32 len;
              trans->GetAnyTransferData(&textMime, (void **)&str, &len);

              // If the string was not empty then paste it in
              if (str)
              {
                nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
                stuffToPaste.SetString(str, len);
                if (htmlEditor)
                  htmlEditor->InsertText(stuffToPaste);
                dragSession->SetCanDrop(PR_TRUE);
              }

              // XXX This is where image support might go
              //void * data;
              //trans->GetTransferData(mImageDataFlavor, (void **)&data, &len);
            }
          } // foreach drag item
        }
      } // if valid transferable
    } // if valid drag session
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  } // if valid drag service

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
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMCompositionListener::GetIID())) {
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

void nsTextEditorCompositionListener::SetEditor(nsIEditor *aEditor)
{
  nsCOMPtr<nsIEditorIMESupport> imeEditor = do_QueryInterface(aEditor);
  if (!imeEditor) return;		// should return an error here!
  
  // note that we don't hold an extra reference here.
  mEditor = imeEditor;
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
                        nsIEditor *aEditor)
{
  nsTextEditorKeyListener* it = new nsTextEditorKeyListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetEditor(aEditor);

  return it->QueryInterface(nsIDOMEventListener::GetIID(), (void **) aInstancePtrResult);   
}



nsresult
NS_NewEditorMouseListener(nsIDOMEventListener ** aInstancePtrResult, 
                          nsIEditor *aEditor)
{
  nsTextEditorMouseListener* it = new nsTextEditorMouseListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetEditor(aEditor);

  return it->QueryInterface(nsIDOMEventListener::GetIID(), (void **) aInstancePtrResult);   
}


nsresult
NS_NewEditorTextListener(nsIDOMEventListener** aInstancePtrResult, nsIEditor* aEditor)
{
	nsTextEditorTextListener*	it = new nsTextEditorTextListener();
	if (nsnull==it) {
		return NS_ERROR_OUT_OF_MEMORY;
	}

	it->SetEditor(aEditor);

	return it->QueryInterface(nsIDOMEventListener::GetIID(), (void **) aInstancePtrResult);
}



nsresult
NS_NewEditorDragListener(nsIDOMEventListener ** aInstancePtrResult, 
                          nsIEditor *aEditor)
{
  nsTextEditorDragListener* it = new nsTextEditorDragListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetEditor(aEditor);

  return it->QueryInterface(nsIDOMEventListener::GetIID(), (void **) aInstancePtrResult);   
}

nsresult
NS_NewEditorCompositionListener(nsIDOMEventListener** aInstancePtrResult, nsIEditor* aEditor)
{
	nsTextEditorCompositionListener*	it = new nsTextEditorCompositionListener();
	if (nsnull==it) {
		return NS_ERROR_OUT_OF_MEMORY;
	}
	it->SetEditor(aEditor);
  return it->QueryInterface(nsIDOMEventListener::GetIID(), (void **) aInstancePtrResult);
}

nsresult 
NS_NewEditorFocusListener(nsIDOMEventListener ** aInstancePtrResult, 
                          nsIEditor *aEditor)
{
  nsTextEditorFocusListener* it = new nsTextEditorFocusListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetEditor(aEditor);
	return it->QueryInterface(nsIDOMEventListener::GetIID(), (void **) aInstancePtrResult);
}



/*
 * nsTextEditorFocusListener implementation
 */

NS_IMPL_ADDREF(nsTextEditorFocusListener)

NS_IMPL_RELEASE(nsTextEditorFocusListener)


nsTextEditorFocusListener::nsTextEditorFocusListener() 
{
  NS_INIT_REFCNT();
}

nsTextEditorFocusListener::~nsTextEditorFocusListener() 
{
}

nsresult
nsTextEditorFocusListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kIDOMFocusListenerIID, NS_IDOMFOCUSLISTENER_IID);
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
  if (aIID.Equals(kIDOMFocusListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMFocusListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsTextEditorFocusListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsTextEditorFocusListener::Focus(nsIDOMEvent* aEvent)
{
  // turn on selection and caret
  if (mEditor)
  {
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (! (flags & nsIHTMLEditor::eEditorDisabledMask))
    { // only enable caret and selection if the editor is not disabled
      nsCOMPtr<nsIEditor>editor = do_QueryInterface(mEditor);
      if (editor)
      {
        nsCOMPtr<nsIPresShell>ps;
        editor->GetPresShell(getter_AddRefs(ps));
        if (ps)
        {
          if (! (flags & nsIHTMLEditor::eEditorReadonlyMask))
          { // only enable caret if the editor is not readonly
            ps->SetCaretEnabled(PR_TRUE);
          }

          nsCOMPtr<nsIDOMDocument>domDoc;
          editor->GetDocument(getter_AddRefs(domDoc));
          if (domDoc)
          {
            nsCOMPtr<nsIDocument>doc = do_QueryInterface(domDoc);
            if (doc)
            {
              doc->SetDisplaySelection(PR_TRUE);
            }
          }
  // begin hack repaint
          nsCOMPtr<nsIViewManager> viewmgr;
          ps->GetViewManager(getter_AddRefs(viewmgr));
          if (viewmgr) {
            nsIView* view;
            viewmgr->GetRootView(view);			// views are not refCounted
            if (view) {
              viewmgr->UpdateView(view,nsnull,NS_VMREFRESH_IMMEDIATE);
            }
  // end hack repaint
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsTextEditorFocusListener::Blur(nsIDOMEvent* aEvent)
{
  // turn off selection and caret
  if (mEditor)
  {
    nsCOMPtr<nsIEditor>editor = do_QueryInterface(mEditor);
    if (editor)
    {
      nsCOMPtr<nsIPresShell>ps;
      editor->GetPresShell(getter_AddRefs(ps));
      if (ps)
      {
        ps->SetCaretEnabled(PR_FALSE);

        nsCOMPtr<nsIDOMDocument>domDoc;
        editor->GetDocument(getter_AddRefs(domDoc));
        if (domDoc)
        {
          nsCOMPtr<nsIDocument>doc = do_QueryInterface(domDoc);
          if (doc)
          {
            doc->SetDisplaySelection(PR_FALSE);
          }
        }
// begin hack repaint
        nsCOMPtr<nsIViewManager> viewmgr;
        ps->GetViewManager(getter_AddRefs(viewmgr));
        if (viewmgr) 
        {
          nsIView* view;
          viewmgr->GetRootView(view);			// views are not refCounted
          if (view) {
            viewmgr->UpdateView(view,nsnull,NS_VMREFRESH_IMMEDIATE);
          }
        }
// end hack repaint
      }
    }
  }
  return NS_OK;
}

