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

// mozXMLTermListeners.h: classes for key/text/mouse/drag event listeners
// used by mozXMLTerminal:
//   mozXMLTermKeyListener
//   mozXMLTermTextListener
//   mozXMLTermMouseListener
//   mozXMLTermDragListener


#ifndef mozXMLTermListeners_h__
#define mozXMLTermListeners_h__

#include "nsIDOMEvent.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMTextListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMDragListener.h"
#include "nsCOMPtr.h"

#include "mozIXMLTerminal.h"

/* XMLTerm Key Listener */
class mozXMLTermKeyListener : public nsIDOMKeyListener,
                              public mozIXMLTermSuspend
{
public:
  mozXMLTermKeyListener();
  virtual ~mozXMLTermKeyListener();

  /** Save non-owning reference to containing XMLTerminal object
   * @param aXMLTerminal the XMLTerm instance
   */
  void SetXMLTerminal(mozIXMLTerminal *aXMLTerminal)
    {mXMLTerminal = aXMLTerminal;}

  // Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

  // nsIDOMEventListener interface
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMKeyListener interface
  virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);

  // mozIXMLTermSuspend interface
  NS_IMETHOD GetSuspend(PRBool* aSuspend);
  NS_IMETHOD SetSuspend(const PRBool aSuspend);

protected:
  /** non-owning reference to containing XMLTerminal object (for callback) */
  mozIXMLTerminal* mXMLTerminal;

  /** suspend flag */
  PRBool mSuspend;
};


/* XMLTerm Text Listener */
class mozXMLTermTextListener : public nsIDOMTextListener {
public:
  mozXMLTermTextListener();
  virtual ~mozXMLTermTextListener();

  /** Save non-owning reference to containing XMLTerminal object
   * @param aXMLTerminal the XMLTerm instance
   */
  void SetXMLTerminal(mozIXMLTerminal *aXMLTerminal)
    {mXMLTerminal = aXMLTerminal;}

  // Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

  // nsIDOMEventListener interface
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMTextListener interface
  virtual nsresult HandleText(nsIDOMEvent* aTextEvent);

protected:
  /** non-owning reference to containing XMLTerminal object (for callback) */
  mozIXMLTerminal* mXMLTerminal;
};


/* XMLTerm Mouse Listener */
class mozXMLTermMouseListener : public nsIDOMMouseListener {
public:
  mozXMLTermMouseListener();
  virtual ~mozXMLTermMouseListener();

  /** Save non-owning reference to containing XMLTerminal object
   * @param aXMLTerminal the XMLTerm instance
   */
  void SetXMLTerminal(mozIXMLTerminal *aXMLTerminal)
    {mXMLTerminal = aXMLTerminal;}

  // Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

  // nsIDOMEventListener interface
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMMouseListener interface
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);

protected:
  /** non-owning reference to containing XMLTerminal object (for callback) */
  mozIXMLTerminal* mXMLTerminal;
};


/* XMLTerm Drag Listener */
class mozXMLTermDragListener : public nsIDOMDragListener {
public:
  mozXMLTermDragListener();
  virtual ~mozXMLTermDragListener();

  /** Save non-owning reference to containing XMLTerminal object
   * @param aXMLTerminal the XMLTerm instance
   */
  void SetXMLTerminal(mozIXMLTerminal *aXMLTerminal)
    {mXMLTerminal = aXMLTerminal;}

  // Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

  // nsIDOMEventListener interface
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMDragListener interface
  virtual nsresult DragEnter(nsIDOMEvent* aDragEvent);
  virtual nsresult DragOver(nsIDOMEvent* aDragEvent);
  virtual nsresult DragExit(nsIDOMEvent* aDragEvent);
  virtual nsresult DragDrop(nsIDOMEvent* aDragEvent);
  virtual nsresult DragGesture(nsIDOMEvent* aDragEvent);

protected:
  /** non-owning reference to containing XMLTerminal object (for callback) */
  mozIXMLTerminal* mXMLTerminal;
};


// Factory for XMLTermKeyListener
extern nsresult NS_NewXMLTermKeyListener(nsIDOMEventListener** aInstancePtrResult, mozIXMLTerminal *aXMLTerminal);

// Factory for XMLTermTextListener
extern nsresult NS_NewXMLTermTextListener(nsIDOMEventListener** aInstancePtrResult, mozIXMLTerminal *aXMLTerminal);

// Factory for XMLTermMouseListener
extern nsresult NS_NewXMLTermMouseListener(nsIDOMEventListener** aInstancePtrResult, mozIXMLTerminal *aXMLTerminal);

// Factory for XMLTermDragListener
extern nsresult NS_NewXMLTermDragListener(nsIDOMEventListener** aInstancePtrResult, mozIXMLTerminal *aXMLTerminal);

#endif //mozXMLTermListeners_h__
