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
 * The Original Code is XMLterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

// mozXMLTermListeners.cpp: implementation of classes in mozXMLTermListeners.h

#include "nsIServiceManager.h"

#include "mozXMLT.h"
#include "mozXMLTermUtils.h"
#include "mozXMLTermListeners.h"

#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIPrivateTextEvent.h"
#include "nsIPresShell.h"
#include "nsISelectionController.h"

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
              nsCAutoString CNodeName; CNodeName.AssignWithConversion(nodeName);
              XMLT_LOG(mozXMLTermKeyListener::KeyPress,58,("nodeName=%s\n",
                                              CNodeName.get()));
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
        JSCommand.AssignLiteral("ScrollHomeKey");
        break;
      case nsIDOMKeyEvent::DOM_VK_END:
        JSCommand.AssignLiteral("ScrollEndKey");
        break;
      case nsIDOMKeyEvent::DOM_VK_PAGE_UP:
        JSCommand.AssignLiteral("ScrollPageUpKey");
        break;
      case nsIDOMKeyEvent::DOM_VK_PAGE_DOWN:
        JSCommand.AssignLiteral("ScrollPageDownKey");
        break;
      case nsIDOMKeyEvent::DOM_VK_F1:
        JSCommand.AssignLiteral("F1Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F2:
        JSCommand.AssignLiteral("F2Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F3:
        JSCommand.AssignLiteral("F3Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F4:
        JSCommand.AssignLiteral("F4Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F5:
        JSCommand.AssignLiteral("F5Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F6:
        JSCommand.AssignLiteral("F6Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F7:
        JSCommand.AssignLiteral("F7Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F8:
        JSCommand.AssignLiteral("F8Key");
        break;
      case nsIDOMKeyEvent::DOM_VK_F9:
        JSCommand.AssignLiteral("F9Key");
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

    if (!JSCommand.IsEmpty()) {
      // Execute JS command
      nsCOMPtr<nsIDOMDocument> domDocument;
      result = mXMLTerminal->GetDocument(getter_AddRefs(domDocument));

      if (NS_SUCCEEDED(result) && domDocument) {
        nsAutoString JSInput = JSCommand;
        nsAutoString JSOutput; JSOutput.SetLength(0);
        JSInput.AppendLiteral("(");
        JSInput.AppendInt(shiftKey,10);
        JSInput.AppendLiteral(",");
        JSInput.AppendInt(ctrlKey,10);
        JSInput.Append(NS_LITERAL_STRING(");"));
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

      result = mXMLTerminal->SendTextAux(keyString.get());
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
  mXMLTerminal->SendTextAux(textStr.get());

  return NS_OK;
}


/////////////////////////////////////////////////////////////////////////
// mozXMLTermMouseListener implementation
/////////////////////////////////////////////////////////////////////////

mozXMLTermMouseListener::mozXMLTermMouseListener()
{
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

  PRUint16 buttonCode = (PRUint16)-1;
  mouseEvent->GetButton(&buttonCode);

  XMLT_LOG(mozXMLTermMouseListener::MouseDown,50,("buttonCode=%d\n",
                                                  buttonCode));

  if (buttonCode == 1) {
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

  PRUint16 buttonCode = (PRUint16)-1;
  mouseEvent->GetButton(&buttonCode);

  PRInt32 screenX, screenY;
  result = mouseEvent->GetScreenX(&screenX);
  result = mouseEvent->GetScreenY(&screenY);

  XMLT_LOG(mozXMLTermMouseListener::MouseClick,50, ("buttonCode=%d\n",
                                                    buttonCode));

#if 0
  // NO MORE NEED FOR THIS WORKAROUND!
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

  PRBool isCollapsed;
  result = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(result))
    return NS_OK; // Do not consume mouse event

  XMLT_LOG(mozXMLTermMouseListener::MouseClick,50, ("isCollapsed=%d\n",
                                                    isCollapsed));

  // If non-collapsed selection, do not collapse it
  if (!isCollapsed)
    return NS_OK;

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
#endif

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
