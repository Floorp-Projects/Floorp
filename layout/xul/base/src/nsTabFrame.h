/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/**

  Eric D Vaughan
  This class lays out its children either vertically or horizontally
 
**/

#ifndef nsTabFrame_h___
#define nsTabFrame_h___

#include "nsHTMLButtonControlFrame.h"

class nsTabControlFrame;

class nsTabFrame : public nsHTMLButtonControlFrame
{
public:

  friend nsresult NS_NewTabFrame(nsIFrame*& aNewFrame);

 
  virtual void MouseClicked(nsIPresContext* aPresContext);

protected:

  virtual nsresult GetChildWithTag(nsIAtom* atom, nsIContent* start, nsIContent*& tabpanel);
  virtual nsresult GetTabControl(nsIContent* content, nsIContent*& tabcontrol);
  virtual nsresult GetIndexInParent(nsIContent* content, PRInt32& index);

private:
   
  nsTabControlFrame* mTabControlFrame;

}; // class nsTabFrame



#endif

