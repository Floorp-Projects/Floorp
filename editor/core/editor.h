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


/*implementation of an editor object.  it ALSO supplies an nsIEditor Interface*/

class Editor : public nsIEditor
{
private:
  COM_auto_ptr<nsIDOMDocument> mDomInterfaceP;
public:
/*nsIEditorInterfaces*/
  virtual nsresult Init();
  virtual nsresult Init() = 0;

/*BEGIN interface manipulators and accessors*/

  /*SetDomInterface accepts an interface to a nsIDOMDocument and it will add a reference to it 
  since it will keep it for some time*/
  virtual nsresult SetDomInterface(nsIDOMDocument *aDomInterface){mDomInterfaceP = aDomInterface}
  
  /*GetDomInterface returns a "look" at the dom interface. it will NOT ADD A REFERENCE!!!!!.*/
  virtual nsresult GetDomInterface(nsIDOMDocument **aDomInterface){*aDomInterface = mDomInterfaceP;}
/*END interface manipulators*/

  virtual nsresult SetProperties(PROPERTIES aProperty){}
  virtual nsresult GetProperties(PROPERTIES &){}

/*EditorInterfaces*/
  Editor();
  ~Editor();

/*KeyListener Methods*/

  /*KeyDown :accepts a keycode
   :returns False if ignored*/
  PR_Bool KeyDown(int keycode);

/*MouseListener Methods*/

  /* Mouse Click accepts an x and 
  a y for location in the webshell*/
  PR_Bool MouseClick(int x,int y); //it should also tell us the dom element that was selected.
};


