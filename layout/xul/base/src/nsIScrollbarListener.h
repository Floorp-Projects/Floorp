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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#ifndef nsIScrollbarListener_h___
#define nsIScrollbarListener_h___

// {A0ADBD81-2911-11d3-97FA-00400553EEF0}
#define NS_ISCROLLBARLISTENER_IID \
{ 0xa0adbd81, 0x2911, 0x11d3, { 0x97, 0xfa, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

static NS_DEFINE_IID(kIScrollbarListenerIID,     NS_ISCROLLBARLISTENER_IID);

class nsIPresContext;

class nsIScrollbarListener : public nsISupports {

public:

  static const nsIID& GetIID() { static nsIID iid = NS_ISCROLLBARLISTENER_IID; return iid; }
  
  NS_IMETHOD PositionChanged(nsIPresContext* aPresContext, PRInt32 aOldIndex, PRInt32& aNewIndex) = 0;

  NS_IMETHOD PagedUpDown() = 0;
};

#endif

