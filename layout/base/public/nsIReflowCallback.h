/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David Hyatt (hyatt@netscape.com)
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
