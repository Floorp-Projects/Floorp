/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// mozXMLTermListeners.cpp: implementation of classes in mozXMLTermListeners.h

#include "nsIServiceManager.h"

#include "mozXMLT.h"
#include "mozXMLTermUtils.h"
#include "mozXMLTermListeners.h"

#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIPrivateTextEvent.h"

/////////////////////////////////////////////////////////////////////////
// mozXMLTermKeyListener factory
/////////////////////////////////////////////////////////////////////////

nsresult 
NS_NewXMLTermKeyListener(nsIDOMEventListener ** aInstancePtrResult, 
                         mozIXMLTerminal *aXMLTerminal)
{
  mozXMLTermKeyListener* listener = new mozXMLTermKeyListener();
  if (listener == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Save non-owning reference to embedding XMLTerminal object
  listener->SetXMLTerminal(aXMLTerminal);

  return listener->QueryInterface(NS_GET_IID(nsIDOMEventListener),
                                  (void **) aInstancePtrResult);
}

nsresult 
NS_NewXMLTermTextListener(nsIDOMEventListener ** aInstancePtrResult, 
                         mozIXMLTerminal *aXMLTerminal)
{
  mozXMLTermTextListener* listener = new mozXMLTermTextListener();
  if (listener == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Save non-owning reference to embedding XMLTerminal object
  listener->SetXMLTerminal(aXMLTerminal);

  return listener->QueryInterface(NS_GET_IID(nsIDOMEventListener),
                                  (void **) aInstancePtrResult);
}

nsresult 
NS_NewXMLTermMouseListener(nsIDOMEventListener ** aInstancePtrResult, 
                         mozIXMLTerminal *aXMLTerminal)
{
  mozXMLTermMouseListener* listener = new mozXMLTermMouseListener();
  if (listener == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Save non-owning reference to embedding XMLTerminal object
  listener->SetXMLTerminal(aXMLTerminal);

  return listener->QueryInterface(NS_GET_IID(nsIDOMEventListener),
                                  (void **) aInstancePtrResult);
}

nsresult 
NS_NewXMLTermDragListener(nsIDOMEventListener ** aInstancePtrResult, 
                         mozIXMLTerminal *aXMLTerminal)
{
  mozXMLTermDragListener* listener = new mozXMLTermDragListener();
  if (listener == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Save non-owning reference to embedding XMLTerminal object
  listener->SetXMLTerminal(aXMLTerminal);

  return listener->QueryInterface(NS_GET_IID(nsIDOMEventListener),
                                  (void **) aInstancePtrResult);
}

/////////////////////////////////////////////////////////////////////////
// mozXMLTermKeyListener implementation
/////////////////////////////////////////////////////////////////////////

mozXMLTermKeyListener::mozXMLTermKeyListener() :
  mXMLTerminal(nsnull),
  mSuspend(PR_FALSE)
{
  NS_INIT_REFCNT();
}


mozXMLTermKeyListener::~mozXMLTermKeyListener() 
{
}


NS_IMPL_ADDREF(mozXMLTermKeyListener)

NS_IMPL_RELEASE(mozXMLTermKeyListener)


NS_IMETHODIMP
mozXMLTermKeyListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsIDOMKeyListener*,this));

  } else if (aIID.Equals(NS_GET_IID(nsIDOMEventListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*,this);

  } else if (aIID.Equals(NS_GET_IID(nsIDOMKeyListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMKeyListener*,this);

  } else if (aIID.Equals(NS_GET_IID(mozIXMLTermSuspend))) {
    *aInstancePtr = NS_STATIC_CAST(mozIXMLTermSuspend*,this);

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();

  return NS_OK;
}


NS_IMETHODIMP mozXMLTermKeyListener::GetSuspend(PRBool* aSuspend)
{
  if (!*aSuspend)
    return NS_ERROR_NULL_POINTER;
  *aSuspend = mSuspend;
  return NS_OK;
}


NS_IMETHODIMP mozXMLTermKeyListener::SetSuspend(const PRBool aSuspend)
{
  XMLT_LOG(mozXMLTermKeyListener::SetSuspend,50,("aSuspend=0x%x\n",
                                                 aSuspend));
  mSuspend = aSuspend;
  return NS_OK;
}

// Individual key handlers return NS_OK to indicate NOT consumed
// by default, an error is returned indicating event is consumed

NS_IMETHODIMP
mozXMLTermKeyListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


// Process KeyDown events (handles control/alt modified key events)
NS_IMETHODIMP
mozXMLTermKeyListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) {
    // Non-key event passed to keydown, do not consume it
    return NS_OK;
  }

  PRBool shiftKey, ctrlKey, altKey;
  PRUint32 keyCode;

  XMLT_LOG(mozXMLTermKeyListener::KeyDown,50,("mSuspend=0x%x\n",
                                               mSuspend));

  if (NS_SUCCEEDED(keyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(keyEvent->GetShiftKey(&shiftKey)) &&
      NS_SUCCEEDED(keyEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(keyEvent->GetAltKey(&altKey)) ) {

    XMLT_LOG(mozXMLTermKeyListener::KeyDown,52,
          ("code=0x%x, shift=%d, ctrl=%d, alt=%d\n",
           keyCode, shiftKey, ctrlKey, altKey));
  }

  // Consume key down event
  return NS_ERROR_BASE;
}


NS_IMETHODIMP
mozXMLTermKeyListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}


// Handle KeyPress events (non control/alt modified key events)
NS_IMETHODIMP
mozXMLTermKeyListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsresult result;

  XMLT_LOG(mozXMLTermKeyListener::KeyPress,50,("mSuspend=0x%x\n",
                                               mSuspend));

  nsCOMPtr<nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) {
    // Non-key event passed to keydown, do not consume it
    return NS_OK;
  }

#if 0  // Debugging
  nsCOMPtr<nsIPresShell> presShell;
  result = mXMLTerminal->GetPresShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(result) && presShell) {
    nsCOMPtr<nsISelection> selection;
    result = presShell->GetSelection(SELECTION_NORMAL,
                                     getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection) {
      nsCOMPtr<nsIDOMNode> childNode, parentNode;
      result = selection->GetFocusNode(getter_AddRefs(childNode));
      if (NS_SUCCEEDED(result) && childNode) {
        PRInt32 j;
        nsAutoString nodeName;
        for (j=0; (j<6) && childNode; j++) {
          result = childNode->GetParentNode(getter_AddRefs(parentNode));
          if (NS_SUCCEEDED(result) && parentNode) {
            result = parentNode->GetNodeName(nodeName);
            if (NS_SUCCEEDED(result)) {
              nsCAutoString CNodeName = nodeName;
              XMLT_LOG(mozXMLTermKeyListener::KeyPress,58,("nodeName=%s\n",
                                              CNodeName.GetBuffer()));
            }
            childNode = parentNode;
          } else {
            childNode = nsnull;
          }
        }
      }
    }
  }
#endif

  PRUint32 keyCode;
  PRBool shiftKey, ctrlKey, altKey;
  if (NS_SUCCEEDED(keyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(keyEvent->GetShiftKey(&shiftKey)) &&
      NS_SUCCEEDED(keyEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(keyEvent->GetAltKey(&altKey)) ) {

    PRUint32 keyChar = 0;
    PRUint32 escPrefix = 0;
    nsAutoString JSCommand; JSCommand.SetLength(0);

    PRBool screenMode = 0;
    result = mXMLTerminal->GetScreenMode(&screenMode);

    result = keyEvent->GetCharCode(&keyChar);

    XMLT_LOG(mozXMLTermKeyListener::KeyPress,52,
          ("code=0x%x, char=0x%x, shift=%d, ctrl=%d, alt=%d, screenMode=%d\n",
           keyCode, keyChar, shiftKey, ctrlKey, altKey, screenMode));

    if (keyChar == 0) {
      // Key that hasn't been mapped to a character code

      switch (keyCode) {
      case nsIDOMKeyEvent::DOM_VK_SHIFT:
      case nsIDOMKeyEvent::DOM_VK_CONTROL:
      case nsIDOMKeyEvent::DOM_VK_ALT:
        break;                         // ignore modifier key event
      case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
        keyChar = U_BACKSPACE;
        break;
      case nsIDOMKeyEvent::DOM_VK_DELETE:
        keyChar = U_DEL;
        break;
      case nsIDOMKeyEvent::DOM_VK_TAB:
        keyChar = U_TAB;
        break;
      case nsIDOMKeyEvent::DOM_VK_RETURN:
        keyChar = U_CRETURN;
        break;
      case nsIDOMKeyEvent::DOM_VK_UP:
        escPrefix = 1;
        keyChar = U_A_CHAR;
        break;
      case nsIDOMKeyEvent::DOM_VK_DOWN:
        escPrefix = 1;
        keyChar = U_B_CHAR;
        break;
      case nsIDOMKeyEvent::DOM_VK_RIGHT:
        escPrefix = 1;
        keyChar = U_C_CHAR;
        break;
      case nsIDOMKeyEvent::DOM_VK_LEFT:
        escPrefix = 1;
        keyChar = U_D_CHAR;
        break;
      case nsIDOMKeyEvent::DOM_VK_ESCAPE:
        keyChar = U_ESCAPE;
        break;
      case nsIDOMKeyEvent::DOM_VK_HOME:
        JSCommand.AssignWithConversion("ScrollHomeKey");
        break;
      case nsIDOMKeyEvent::DOM_VK_END:
        JSCommand.AssignWithConversion("ScrollEndKey");
        break;
      case nsIDOMKeyEvent::DOM_VK_PAGE_UP:
        JSCommand.AssignWithConversion("ScrollPageUpKey");
        break;
      case nsIDOMKeyEvent::DOM_VK_PAGE_DOWN:
        JSCommand.AssignWithConversion("ScrollPageDownKey");
        break;
      case nsIDOMKeyEvent::DOM_VK_F1:
        JSCommand.AssignWithConversion("F1Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F2:
        JSCommand.AssignWithConversion("F2Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F3:
        JSCommand.AssignWithConversion("F3Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F4:
        JSCommand.AssignWithConversion("F4Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F5:
        JSCommand.AssignWithConversion("F5Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F6:
        JSCommand.AssignWithConversion("F6Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F7:
        JSCommand.AssignWithConversion("F7Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F8:
        JSCommand.AssignWithConversion("F8Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F9:
        JSCommand.AssignWithConversion("F9Key");
        break;
      default: 
        if ( (ctrlKey && (keyCode ==nsIDOMKeyEvent::DOM_VK_SPACE)) ||
             (ctrlKey && shiftKey && (keyCode ==nsIDOMKeyEvent::DOM_VK_2)) ) {
          // Hack to handle input of NUL characters in NUL-terminated strings
          // See also: mozLineTerm::Write
          keyChar = U_PRIVATE0;
        } else {
          // ignore event without consuming
          return NS_OK;
        }
      }

    } else if ((ctrlKey == PR_TRUE) && (altKey == PR_FALSE) &&
               (keyChar >= 0x40U) && (keyChar < 0x80U)) {
      // Control character, without Alt; adjust character code
      // Is this portable?
      keyChar = (keyChar >= 0x60U) ? keyChar-0x60U : keyChar-0x40U;
    }

    XMLT_LOG(mozXMLTermKeyListener::KeyPress,53,
             ("escPrefix=%d, keyChar=0x%x, \n", escPrefix, keyChar));

    if (JSCommand.Length() > 0) {
      // Execute JS command
      nsCOMPtr<nsIDOMDocument> domDocument;
      result = mXMLTerminal->GetDocument(getter_AddRefs(domDocument));

      if (NS_SUCCEEDED(result) && domDocument) {
        nsAutoString JSInput = JSCommand;
        nsAutoString JSOutput; JSOutput.SetLength(0);
        JSInput.AppendWithConversion("(");
        JSInput.AppendInt(shiftKey,10);
        JSInput.AppendWithConversion(",");
        JSInput.AppendInt(ctrlKey,10);
        JSInput.AppendWithConversion(");");
        result = mozXMLTermUtils::ExecuteScript(domDocument,
                                                JSInput,
                                                JSOutput);
      }
    }

    if (!mSuspend && (keyChar > 0) && (keyChar <= 0xFFFDU)) {
      // Transmit valid non-null Unicode character
      nsAutoString keyString; keyString.SetLength(0);
      if (escPrefix) {
        keyString.Append((PRUnichar) U_ESCAPE);
        keyString.Append((PRUnichar) U_LBRACKET);
      }

      keyString.Append((PRUnichar) keyChar);

      result = mXMLTerminal->SendTextAux(keyString);
    }
  }

  // Consume key press event
  return NS_ERROR_BASE;
}


/////////////////////////////////////////////////////////////////////////
// mozXMLTermTextListener implementation
/////////////////////////////////////////////////////////////////////////

mozXMLTermTextListener::mozXMLTermTextListener()
{
  NS_INIT_REFCNT();
}


mozXMLTermTextListener::~mozXMLTermTextListener() 
{
}


NS_IMPL_ADDREF(mozXMLTermTextListener)

NS_IMPL_RELEASE(mozXMLTermTextListener)


NS_IMETHODIMP
mozXMLTermTextListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsIDOMTextListener*,this));

  } else if (aIID.Equals(NS_GET_IID(nsIDOMEventListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*,this);

  } else if (aIID.Equals(NS_GET_IID(nsIDOMTextListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMTextListener*,this);

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();

  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermTextListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermTextListener::HandleText(nsIDOMEvent* aTextEvent)
{
  nsCOMPtr<nsIPrivateTextEvent> textEvent (do_QueryInterface(aTextEvent));
  if (!textEvent) {
    // Soft failure
    return NS_OK;
  }

  XMLT_LOG(mozXMLTermTextListener::HandleText,50,("\n"));

  nsAutoString textStr;
  textEvent->GetText(textStr);

  // Transmit text to terminal
  mXMLTerminal->SendTextAux(textStr);

  return NS_OK;
}


/////////////////////////////////////////////////////////////////////////
// mozXMLTermMouseListener implementation
/////////////////////////////////////////////////////////////////////////

mozXMLTermMouseListener::mozXMLTermMouseListener()
{
  NS_INIT_REFCNT();
}


mozXMLTermMouseListener::~mozXMLTermMouseListener() 
{
}


NS_IMPL_ADDREF(mozXMLTermMouseListener)

NS_IMPL_RELEASE(mozXMLTermMouseListener)


NS_IMETHODIMP
mozXMLTermMouseListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsIDOMMouseListener*,this));

  } else if (aIID.Equals(NS_GET_IID(nsIDOMEventListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*,this);

  } else if (aIID.Equals(NS_GET_IID(nsIDOMMouseListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMMouseListener*,this);

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();

  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermMouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermMouseListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if (!aMouseEvent)
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aMouseEvent);
  if (!mouseEvent) {
    // Non-mouse event passed; do not consume it
    return NS_OK;
  }

  PRUint16 buttonCode = 0;
  mouseEvent->GetButton(&buttonCode);

  XMLT_LOG(mozXMLTermMouseListener::MouseDown,50,("buttonCode=%d\n",
                                                  buttonCode));

  if (buttonCode == 2) {
    // Middle-mouse button pressed; initiate paste
    mXMLTerminal->Paste();
  }

  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermMouseListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermMouseListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsresult result;

  if (!aMouseEvent)
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aMouseEvent);
  if (!mouseEvent) {
    // Non-mouse event passed; do not consume it
    return NS_OK;
  }

  PRUint16 buttonCode = 0;
  mouseEvent->GetButton(&buttonCode);

  PRInt32 screenX, screenY;
  result = mouseEvent->GetScreenX(&screenX);
  result = mouseEvent->GetScreenY(&screenY);

  XMLT_LOG(mozXMLTermMouseListener::MouseClick,50, ("buttonCode=%d\n",
                                                    buttonCode));

#ifndef NO_WORKAROUND
  // Without this workaround, clicking on the xmlterm window to give it
  // focus fails position the cursor at the end of the last line
  // (For some reason, the MouseDown event causes the cursor to be positioned
  // in the line above the location of the mouse click

  // Get selection
  nsCOMPtr<nsIPresShell> presShell;
  result = mXMLTerminal->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(result) || !presShell)
    return NS_OK; // Do not consume mouse event
  nsCOMPtr<nsISelectionController> selCon;
  selCon = do_QueryInterface(presShell);
  if (!selCon)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsISelection> selection;
  result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                     getter_AddRefs(selection));

  if (NS_FAILED(result) || !selection)
    return NS_OK; // Do not consume mouse event

  // Locate selection range
  nsCOMPtr<nsIDOMNSUIEvent> uiEvent (do_QueryInterface(mouseEvent));
  if (!uiEvent)
    return NS_OK; // Do not consume mouse event

  nsCOMPtr<nsIDOMNode> parentNode;
  result = uiEvent->GetRangeParent(getter_AddRefs(parentNode));
  if (NS_FAILED(result) || !parentNode)
    return NS_OK; // Do not consume mouse event

  PRInt32 offset = 0;
  result = uiEvent->GetRangeOffset(&offset);
  if (NS_FAILED(result))
    return NS_OK; // Do not consume mouse event

  (void)selection->Collapse(parentNode, offset);
#endif // !NO_WORKAROUND

  return NS_OK; // Do not consume mouse event
}


NS_IMETHODIMP
mozXMLTermMouseListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermMouseListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermMouseListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}


/////////////////////////////////////////////////////////////////////////
// mozXMLTermDragListener implementation
/////////////////////////////////////////////////////////////////////////

mozXMLTermDragListener::mozXMLTermDragListener()
{
  NS_INIT_REFCNT();
}


mozXMLTermDragListener::~mozXMLTermDragListener() 
{
}


NS_IMPL_ADDREF(mozXMLTermDragListener)

NS_IMPL_RELEASE(mozXMLTermDragListener)


NS_IMETHODIMP
mozXMLTermDragListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsIDOMDragListener*,this));

  } else if (aIID.Equals(NS_GET_IID(nsIDOMEventListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*,this);

  } else if (aIID.Equals(NS_GET_IID(nsIDOMDragListener))) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMDragListener*,this);

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();

  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermDragListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermDragListener::DragEnter(nsIDOMEvent* aDragEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermDragListener::DragOver(nsIDOMEvent* aDragEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermDragListener::DragExit(nsIDOMEvent* aDragEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermDragListener::DragDrop(nsIDOMEvent* aDragEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
mozXMLTermDragListener::DragGesture(nsIDOMEvent* aDragEvent)
{
  return NS_OK;
}
