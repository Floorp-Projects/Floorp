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
#ifndef nsIRobotSink_h___
#define nsIRobotSink_h___

#include "nsIHTMLContentSink.h"
class nsIURI;
class nsIRobotSinkObserver;

/* 61256800-cfd8-11d1-9328-00805f8add32 */
#define NS_IROBOTSINK_IID     \
{ 0x61256800, 0xcfd8, 0x11d1, \
  {0x93, 0x28, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsIRobotSink : public nsIHTMLContentSink {
public:
  NS_IMETHOD Init(nsIURI* aDocumentURL) = 0;
  NS_IMETHOD AddObserver(nsIRobotSinkObserver* aObserver) = 0;
  NS_IMETHOD RemoveObserver(nsIRobotSinkObserver* aObserver) = 0;

};

extern nsresult NS_NewRobotSink(nsIRobotSink** aInstancePtrResult);

#endif /* nsIRobotSink_h___ */
