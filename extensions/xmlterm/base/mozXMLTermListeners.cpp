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
 */

// mozXMLTermListeners.cpp: implementation of classes in mozXMLTermListeners.h

#include "nsIServiceManager.h"

#include "mozXMLT.h"
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

  return listener->QueryInterface(nsIDOMEventListener::GetIID(),
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

  return listener->QueryInterface(nsIDOMEventListener::GetIID(),
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

  return listener->QueryInterface(nsIDOMEventListener::GetIID(),
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

  return listener->QueryInterface(nsIDOMEventListener::GetIID(),
                                  (void **) aInstancePtrResult);
}

/////////////////////////////////////////////////////////////////////////
// mozXMLTermKeyListener implementation
/////////////////////////////////////////////////////////////////////////

mozXMLTermKeyListener::mozXMLTermKeyListener()
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

  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsIDOMKeyListener*,this));

  } else if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*,this);

  } else if (aIID.Equals(nsIDOMKeyListener::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMKeyListener*,this);

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();

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
  nsresult result;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) {
    // Non-key event passed to keydown, do not consume it
    return NS_OK;
  }

  PRBool isShift, ctrlKey, altKey;
  PRUint32 keyCode;

  XMLT_LOG(mozXMLTermKeyListener::KeyDown,50,("\n"));

  if (NS_SUCCEEDED(keyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(keyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(keyEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(keyEvent->GetAltKey(&altKey)) ) {

    XMLT_LOG(mozXMLTermKeyListener::KeyDown,52,
          ("keyCode=0x%x, ctrlKey=%d, altKey=%d\n", keyCode, ctrlKey, altKey));

    PRUint32 keyChar = 0;

    if (!ctrlKey && !altKey) {
      // Not control/alt key event
      switch (keyCode) {
      case nsIDOMKeyEvent::DOM_VK_LEFT:
        keyChar = U_CTL_B;
        break;
      case nsIDOMKeyEvent::DOM_VK_RIGHT:
        keyChar = U_CTL_F;
        break;
      case nsIDOMKeyEvent::DOM_VK_UP:
        keyChar = U_CTL_P;
        break;
      case nsIDOMKeyEvent::DOM_VK_DOWN:
        keyChar = U_CTL_N;
        break;
      case nsIDOMKeyEvent::DOM_VK_TAB: // Consume TAB to avoid scroll problems
        keyChar = 0;
        break;
      default: // ignore event without consuming
        return NS_OK;
      }

    } else if (ctrlKey == PR_TRUE) {
      keyChar = keyCode - 0x40U;   // Is this portable?
    }

    XMLT_LOG(mozXMLTermKeyListener::KeyDown,52,("keyChar=0x%x\n", keyChar));

    if ((keyChar > 0) && (keyChar < U_SPACE)) {
      // Transmit valid non-null control character
      const PRUnichar temUString[] = {keyChar,0};
      nsAutoString keyString(temUString);
      result = mXMLTerminal->SendTextAux(keyString);
    }
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

  XMLT_LOG(mozXMLTermKeyListener::KeyPress,50,("\n"));

  nsCOMPtr<nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) {
    // Non-key event passed to keydown, do not consume it
    return NS_OK;
  }

  PRUint32 keyCode;
  PRBool isShift, ctrlKey, altKey;
  if (NS_SUCCEEDED(keyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(keyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(keyEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(keyEvent->GetAltKey(&altKey)) ) {

    PRUint32 keyChar = 0;
    result = keyEvent->GetCharCode(&keyChar);

    XMLT_LOG(mozXMLTermKeyListener::KeyPress,52,
          ("keyChar=0x%x, ctrlKey=%d, altKey=%d\n", keyChar, ctrlKey, altKey));

    if (ctrlKey == PR_TRUE) {
      // Do nothing for Ctrl-Alt key events; just consume then

      if (altKey == PR_FALSE) {
        // Control character, without Alt

        if ((keyChar > 0) && (keyChar < U_SPACE)) {
          // Transmit valid non-null control character
          const PRUnichar temUString[] = {keyChar,0};
          nsAutoString keyString(temUString);
          result = mXMLTerminal->SendTextAux(keyString);
        }
      }

    } else {
      // Unmodified key event (including TAB/BACKSPACE/RETURN/LINEFEED)

      if (keyChar == 0) {
        // Key that hasn't been mapped to a character code
        switch (keyCode) {
        case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
        case nsIDOMKeyEvent::DOM_VK_DELETE:
          keyChar = U_BACKSPACE;
          break;
        case nsIDOMKeyEvent::DOM_VK_TAB:
          keyChar = U_TAB;
          break;
        case nsIDOMKeyEvent::DOM_VK_RETURN:
          keyChar = U_LINEFEED;
          break;
        default: // ignore event without consuming
          return NS_OK;
        }
      }

      // Translate Carriage Return to LineFeed (may not be portable??)
      if (keyChar == U_CRETURN) keyChar = U_LINEFEED;

      if ((keyChar > 0) && (keyChar <= 0xFFFDU)) {
        // Transmit valid non-null Unicode character
        const PRUnichar temUString[] = {keyChar,0};
        nsAutoString keyString(temUString);
        result = mXMLTerminal->SendTextAux(keyString);
      }
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

  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsIDOMTextListener*,this));

  } else if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*,this);

  } else if (aIID.Equals(nsIDOMTextListener::GetIID())) {
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

  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsIDOMMouseListener*,this));

  } else if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*,this);

  } else if (aIID.Equals(nsIDOMMouseListener::GetIID())) {
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
  return NS_OK;
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

  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsIDOMDragListener*,this));

  } else if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*,this);

  } else if (aIID.Equals(nsIDOMDragListener::GetIID())) {
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
