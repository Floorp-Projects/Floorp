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
 * Original Author: David Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */
#ifndef nsIReflowCallback_h___
#define nsIReflowCallback_h___

// {03E6FD70-889C-44b1-8639-A7EF5A80ED5C}
#define NS_IREFLOWCALLBACK_IID \
{ 0x3e6fd70, 0x889c, 0x44b1, { 0x86, 0x39, 0xa7, 0xef, 0x5a, 0x80, 0xed, 0x5c } }

class nsIPresShell;

/**
 * Reflow callback interface
 */
class nsIReflowCallback : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IREFLOWCALLBACK_IID; return iid; }

  NS_IMETHOD ReflowFinished(nsIPresShell* aShell, PRBool* aFlushFlag) = 0;
};

#endif /* nsIFrameUtil_h___ */
