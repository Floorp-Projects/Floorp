/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef editorInterfaces_h__
#define editorInterfaces_h__

#include "nsCOMPtr.h"


#include "nsIDOMEvent.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMTextListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMCompositionListener.h"
#include "nsIDOMFocusListener.h"

#include "nsIEditor.h"
#include "nsIPlaintextEditor.h"

/** The nsTextEditorKeyListener public nsIDOMKeyListener
 *  This class will delegate events to its editor according to the translation
 *  it is responsible for.  i.e. 'c' becomes a keydown, but 'ESC' becomes nothing.
 */
class nsTextEditorKeyListener : public nsIDOMKeyListener {
public:
  /** the default constructor
   */
  nsTextEditorKeyListener();
  /** the default destructor. virtual due to the possibility of derivation.
   */
  virtual ~nsTextEditorKeyListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetEditor(nsIEditor *aEditor){mEditor = aEditor;}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN interfaces in to the keylister base interface. must be supplied to handle pure virtual interfaces
  see the nsIDOMKeyListener interface implementation for details
  */
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);
/*END interfaces from nsIDOMKeyListener*/

protected:
  virtual nsresult ProcessShortCutKeys(nsIDOMEvent* aKeyEvent, PRBool& aProcessed);

protected:
  nsIEditor*     mEditor;		// weak reference
};


/** editor Implementation of the TextListener interface
 */
class nsTextEditorTextListener : public nsIDOMTextListener
{
public:
  /** default constructor
   */
  nsTextEditorTextListener();
  /** default destructor
   */
  virtual ~nsTextEditorTextListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetEditor(nsIEditor *aEditor){mEditor = aEditor;}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of textevent handler interface*/
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
    NS_IMETHOD HandleText(nsIDOMEvent* aTextEvent);
/*END implementations of textevent handler interface*/

protected:
  nsIEditor*      mEditor;		// weak reference
	PRBool					mCommitText;
	PRBool					mInTransaction;
};


class nsIEditorIMESupport;

class nsTextEditorCompositionListener : public nsIDOMCompositionListener
{
public:
  /** default constructor
   */
  nsTextEditorCompositionListener();
  /** default destructor
   */
  virtual ~nsTextEditorCompositionListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetEditor(nsIEditor *aEditor);

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of textevent handler interface*/
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD HandleStartComposition(nsIDOMEvent* aCompositionEvent);
  NS_IMETHOD HandleEndComposition(nsIDOMEvent* aCompositionEvent);
  NS_IMETHOD HandleQueryComposition(nsIDOMEvent* aCompositionEvent);
  NS_IMETHOD HandleQueryReconversion(nsIDOMEvent* aReconvertionEvent);
/*END implementations of textevent handler interface*/

protected:
  nsIEditorIMESupport*     mEditor;		// weak reference
};


/** editor Implementation of the MouseListener interface
 */
class nsTextEditorMouseListener : public nsIDOMMouseListener 
{
public:
  /** default constructor
   */
  nsTextEditorMouseListener();
  /** default destructor
   */
  virtual ~nsTextEditorMouseListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetEditor(nsIEditor *aEditor){mEditor = aEditor;}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of mouseevent handler interface*/
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);
/*END implementations of mouseevent handler interface*/

protected:
  nsIEditor*     mEditor;		// weak reference

};


/** editor Implementation of the DragListener interface
 */
class nsTextEditorDragListener : public nsIDOMDragListener 
{
public:
  /** default constructor
   */
  nsTextEditorDragListener();
  /** default destructor
   */
  virtual ~nsTextEditorDragListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetEditor(nsIEditor *aEditor){mEditor = aEditor;}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of dragevent handler interface*/
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD DragEnter(nsIDOMEvent* aDragEvent);
  NS_IMETHOD DragOver(nsIDOMEvent* aDragEvent);
  NS_IMETHOD DragExit(nsIDOMEvent* aDragEvent);
  NS_IMETHOD DragDrop(nsIDOMEvent* aDragEvent);
  NS_IMETHOD DragGesture(nsIDOMEvent* aDragEvent);
/*END implementations of dragevent handler interface*/

protected:
  nsIEditor*    mEditor;

};

/** editor Implementation of the FocusListener interface
 */
class nsTextEditorFocusListener : public nsIDOMFocusListener 
{
public:
  /** default constructor
   */
  nsTextEditorFocusListener();
  /** default destructor
   */
  virtual ~nsTextEditorFocusListener();

  /** SetEditor gives an address to the editor that will be accessed
   *  @param aEditor the editor this listener calls for editing operations
   */
  void SetEditor(nsIEditor *aEditor){mEditor = aEditor;}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of focus event handler interface*/
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);
/*END implementations of focus event handler interface*/

protected:
  nsIEditor*     mEditor;		// weak reference
};



/** factory for the editor key listener
 */
extern nsresult NS_NewEditorKeyListener(nsIDOMEventListener ** aInstancePtrResult, nsIEditor *aEditor);

/** factory for the editor mouse listener
 */
extern nsresult NS_NewEditorMouseListener(nsIDOMEventListener ** aInstancePtrResult, nsIEditor *aEditor);

/** factory for the editor text listener
 */
extern nsresult NS_NewEditorTextListener(nsIDOMEventListener** aInstancePtrResult, nsIEditor *aEditor);

/** factory for the editor drag listener
 */
extern nsresult NS_NewEditorDragListener(nsIDOMEventListener ** aInstancePtrResult, nsIEditor *aEditor);

/** factory for the editor composition listener 
 */
extern nsresult NS_NewEditorCompositionListener(nsIDOMEventListener** aInstancePtrResult, nsIEditor *aEditor);

/** factory for the editor composition listener 
 */
extern nsresult NS_NewEditorFocusListener(nsIDOMEventListener** aInstancePtrResult, nsIEditor *aEditor);

#endif //editorInterfaces_h__

