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

#include "nsIEditor.h"
#include "COM_auto_ptr.h"


/** implementation of an editor object.  it will be the controler/focal point 
 *  for the main editor services. i.e. the GUIControler, publishing, transaction 
 *  manager, event interfaces. the idea for the event interfaces is to have them 
 *  delegate the actual commands to the editor independent of the XPFE implementation.
 */
class Editor : public nsIEditor
{
private:
  COM_auto_ptr<nsIDOMDocument> mDomInterfaceP;
  COM_auto_ptr<nsEditorKeyListener> mKeyListenerP;
  COM_auto_ptr<nsEditorMouseListener> mMouseListenerP;
public:
  /** The default constructor. This should suffice. the setting of the interfaces is done
   *  after the construction of the editor class.
   */
  Editor();
  /** The default destructor. This should suffice. Should this be pure virtual 
   *  for someone to derive from the Editor later? I dont believe so.
   */
  ~Editor();

/*BEGIN nsIEditor interfaces*/
  /*see the nsIEditor for more details*/
  virtual nsresult Init(nsIDOMDocument *aDomInterface);

  virtual nsresult GetDomInterface(nsIDOMDocument **aDomInterface){*aDomInterface = mDomInterfaceP;}

  virtual nsresult SetProperties(PROPERTIES aProperty){}
  virtual nsresult GetProperties(PROPERTIES &){}
/*END nsIEditor interfaces*/

/*BEGIN Editor interfaces*/


/*KeyListener Methods*/
  /** KeyDown is called from the key event lister "probably"
   *  @param int aKeycode the keycode or ascii
   *  value of the key that was hit for now
   *  @return False if ignored
   */
  PR_Bool KeyDown(int aKeycode);

/*MouseListener Methods*/
  /** MouseClick
   *  @param int x the xposition of the click
   *  @param int y the yposition of the click
   */
  PR_Bool MouseClick(int aX,int aY); //it should also tell us the dom element that was selected.
};


