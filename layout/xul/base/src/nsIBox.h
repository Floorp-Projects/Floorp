/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/**

  Eric D Vaughan
  nsBoxFrame is a frame that can lay its children out either vertically or horizontally.
  It lays them out according to a min max or preferred size.
 
**/

#ifndef nsIBox_h___
#define nsIBox_h___

class nsIPresContext;
class nsIFrame;
struct nsHTMLReflowState;
class nsBoxInfo;

// {02A560C0-01BF-11d3-B35C-00A0CC3C1CDE} 
#define NS_IBOX_IID { 0x2a560c0, 0x1bf, 0x11d3, { 0xb3, 0x5c, 0x0, 0xa0, 0xcc, 0x3c, 0x1c, 0xde } }

class nsBoxInfo {
public:
    nsSize prefSize;
    nsSize minSize;
    nsSize maxSize; 
    PRInt32 flex;
    nscoord ascent;

    nsBoxInfo();
    virtual void Clear();
    virtual ~nsBoxInfo();
  
};

class nsIBox : public nsISupports {

public:

  static const nsIID& GetIID() { static nsIID iid = NS_IBOX_IID; return iid; }

  /** Get the layout information object for this box. It will contains things like flexiblity,
    * preferred, min, max sizes. 
    */
  NS_IMETHOD GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)=0;

  /** clear any cached layout info about our children. If the child is specifically specified
   *  then only clear cached layout information for that specific child. If the child is not
   *  then clear all childrens cached information. 
   */
  NS_IMETHOD InvalidateCache(nsIFrame* aChild)=0;


  NS_IMETHOD SetDebug(nsIPresContext* aPresContext, PRBool aDebug)=0;

};

#endif

