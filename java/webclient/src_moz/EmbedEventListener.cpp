/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <nsCOMPtr.h>
#include <nsIDOMMouseEvent.h>

#include "nsIDOMKeyEvent.h"

#include "EmbedEventListener.h"
#include "NativeBrowserControl.h"

#include "ns_globals.h" // for prLogModuleInfo

EmbedEventListener::EmbedEventListener(void)
{
  mOwner = nsnull;
}

EmbedEventListener::~EmbedEventListener()
{
}

NS_IMPL_ADDREF(EmbedEventListener)
NS_IMPL_RELEASE(EmbedEventListener)
NS_INTERFACE_MAP_BEGIN(EmbedEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
NS_INTERFACE_MAP_END

nsresult
EmbedEventListener::Init(NativeBrowserControl *aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

// All of the event listeners below return NS_OK to indicate that the
// event should not be consumed in the default case.

NS_IMETHODIMP
EmbedEventListener::HandleEvent(nsIDOMEvent* aDOMEvent)
{
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyDown(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMKeyEvent> keyEvent;
    keyEvent = do_QueryInterface(aDOMEvent);
    if (!keyEvent)
	return NS_OK;
    // Return FALSE to this function to mark the event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_KEY_DOWN],
		  (void *)keyEvent, &return_val);
    **********/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyUp(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMKeyEvent> keyEvent;
    keyEvent = do_QueryInterface(aDOMEvent);
    if (!keyEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /***************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_KEY_UP],
		  (void *)keyEvent, &return_val);
    ****************/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyPress(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMKeyEvent> keyEvent;
    keyEvent = do_QueryInterface(aDOMEvent);
    if (!keyEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /***********
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_KEY_PRESS],
		  (void *)keyEvent, &return_val);
    ************/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseDown(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
    mouseEvent = do_QueryInterface(aDOMEvent);
    if (!mouseEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /**************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_MOUSE_DOWN],
		  (void *)mouseEvent, &return_val);
    ***********/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseUp(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
    mouseEvent = do_QueryInterface(aDOMEvent);
    if (!mouseEvent)
	return NS_OK;
    // Return FALSE to this function to mark the event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /**************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_MOUSE_UP],
		  (void *)mouseEvent, &return_val);
    *************/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseClick(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
    mouseEvent = do_QueryInterface(aDOMEvent);
    if (!mouseEvent)
	return NS_OK;
    // Return FALSE to this function to mark the event as not
    // consumed...
    PRBool return_val = FALSE;
    /****************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_MOUSE_CLICK],
		  (void *)mouseEvent, &return_val);
    ***********/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseDblClick(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
    mouseEvent = do_QueryInterface(aDOMEvent);
    if (!mouseEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /***************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_MOUSE_DBL_CLICK],
		  (void *)mouseEvent, &return_val);
    **********/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseOver(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
    mouseEvent = do_QueryInterface(aDOMEvent);
    if (!mouseEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	   ("EmbedEventListener::MouseOver: \n"));

    /*************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_MOUSE_OVER],
		  (void *)mouseEvent, &return_val);
    **********/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseOut(nsIDOMEvent* aDOMEvent)
{
    nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
    mouseEvent = do_QueryInterface(aDOMEvent);
    if (!mouseEvent)
	return NS_OK;
    // return FALSE to this function to mark this event as not
    // consumed...
    PRBool return_val = PR_FALSE;
    /************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[DOM_MOUSE_OUT],
		  (void *)mouseEvent, &return_val);
    ***********/
    if (return_val) {
	aDOMEvent->StopPropagation();
	aDOMEvent->PreventDefault();
    }
    return NS_OK;
}
