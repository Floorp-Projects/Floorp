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

#ifndef editorInterfaces_h__
#define editorInterfaces_h__

#include "nsIDOMEvent.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMDragListener.h"
#include "nsITextEditor.h"
#include "nsCOMPtr.h"

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
  void SetEditor(nsITextEditor *aEditor){mEditor = do_QueryInterface(aEditor);}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN interfaces in to the keylister base interface. must be supplied to handle pure virtual interfaces
  see the nsIDOMKeyListener interface implementation for details
  */
  virtual nsresult ProcessEvent(nsIDOMEvent* aEvent);
  virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);
/*END interfaces from nsIDOMKeyListener*/

protected:
  virtual nsresult GetCharFromKeyCode(PRUint32 aKeyCode, PRBool aIsShift, char *aChar);
  virtual nsresult ProcessShortCutKeys(nsIDOMEvent* aKeyEvent, PRBool& aProcessed);

protected:
  nsCOMPtr<nsITextEditor> mEditor;
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
  void SetEditor(nsITextEditor *aEditor){mEditor = do_QueryInterface(aEditor);}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of mouseevent handler interface*/
    virtual nsresult ProcessEvent(nsIDOMEvent* aEvent);
public:
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);
/*END implementations of mouseevent handler interface*/

protected:
  nsCOMPtr<nsITextEditor> mEditor;

};

/** editor Implementation of the MouseListener interface
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
  void SetEditor(nsITextEditor *aEditor){mEditor = do_QueryInterface(aEditor);}

/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of mouseevent handler interface*/
    virtual nsresult ProcessEvent(nsIDOMEvent* aEvent);
public:
  virtual nsresult DragStart(nsIDOMEvent* aDragEvent);
  virtual nsresult DragDrop(nsIDOMEvent* aDragEvent);
/*END implementations of mouseevent handler interface*/

protected:
  nsCOMPtr<nsITextEditor> mEditor;

};

/** factory for the editor key listener
 */
extern nsresult NS_NewEditorKeyListener(nsIDOMEventListener ** aInstancePtrResult, nsITextEditor *aEditor);

/** factory for the editor mouse listener
 */
extern nsresult NS_NewEditorMouseListener(nsIDOMEventListener ** aInstancePtrResult, nsITextEditor *aEditor);

/** factory for the editor drag listener
 */
extern nsresult NS_NewEditorDragListener(nsIDOMEventListener ** aInstancePtrResult, nsITextEditor *aEditor);

#endif //editorInterfaces_h__

