/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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

#include "prmon.h"
#include "nsIEditor.h"
#include "nsIContextLoader.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "COM_auto_ptr.h"
#include "editorInterfaces.h"
//#include "nsISelection.h"


//This is the monitor for the editor.
PRMonitor *getEditorMonitor();


/** implementation of an editor object.  it will be the controler/focal point 
 *  for the main editor services. i.e. the GUIManager, publishing, transaction 
 *  manager, event interfaces. the idea for the event interfaces is to have them 
 *  delegate the actual commands to the editor independent of the XPFE implementation.
 */
class nsEditor : public nsIEditor
{
private:
  COM_auto_ptr<nsIDOMDocument>      mDomInterfaceP;
  COM_auto_ptr<nsIDOMEventListener> mKeyListenerP;
  COM_auto_ptr<nsIDOMEventListener> mMouseListenerP;
//  COM_auto_ptr<nsISelection>        mSelectionP;
public:
  /** The default constructor. This should suffice. the setting of the interfaces is done
   *  after the construction of the editor class.
   */
  nsEditor();
  /** The default destructor. This should suffice. Should this be pure virtual 
   *  for someone to derive from the nsEditor later? I dont believe so.
   */
  virtual ~nsEditor();

/*BEGIN nsIEditor interfaces*/
/*see the nsIEditor for more details*/
  
  /*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  virtual nsresult Init(nsIDOMDocument *aDomInterface);

  virtual nsresult GetDomInterface(nsIDOMDocument **aDomInterface);

  virtual nsresult SetProperties(PROPERTIES aProperty);

  virtual nsresult GetProperties(PROPERTIES **);

  virtual nsresult InsertString(nsString *aString);
  
  virtual nsresult Commit(PRBool aCtrlKey);

/*END nsIEditor interfaces*/

/*BEGIN nsEditor interfaces*/


/*KeyListener Methods*/
  /** KeyDown is called from the key event lister "probably"
   *  @param int aKeycode the keycode or ascii
   *  value of the key that was hit for now
   *  @return False if ignored
   */
  PRBool KeyDown(int aKeycode);

/*MouseListener Methods*/
  /** MouseClick
   *  @param int x the xposition of the click
   *  @param int y the yposition of the click
   */
  PRBool MouseClick(int aX,int aY); //it should also tell us the dom element that was selected.

/*BEGIN private methods used by the implementations of the above functions*/

  /** AppendText is a private method that accepts a pointer to a string
   *  and will append it to the current node.  this will be depricated
   *  @param nsString *aStr is the pointer to the valid string
   */
  nsresult AppendText(nsString *aStr);

  /** GetCurrentNode ADDREFFS and will get the current node from selection.
   *  now it simply returns the first node in the dom
   *  @param nsIDOMNode **aNode is the return location of the dom node
   */
  nsresult GetCurrentNode(nsIDOMNode ** aNode);

  /** GetFirstTextNode ADDREFFS and will get the next available text node from the passed
   *  in node parameter it can also return NS_ERROR_FAILURE if no text nodes are available
   *  now it simply returns the first node in the dom
   *  @param nsIDOMNode *aNode is the node to start looking from
   *  @param nsIDOMNode **aRetNode is the return location of the text dom node
   */
  nsresult GetFirstTextNode(nsIDOMNode *aNode, nsIDOMNode **aRetNode);

/*END private methods of nsEditor*/
};


/*
factory method(s)
*/

nsresult NS_MakeEditorLoader(nsIContextLoader **aResult);
