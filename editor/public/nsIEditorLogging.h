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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIEditorLogging_h__
#define nsIEditorLogging_h__
#include "nsISupports.h"
#include "nscore.h"


#define NS_IEDITORLOGGING_IID                            \
{ /* 4805e681-49b9-11d3-9ce4-ed60bd6cb5bc} */            \
0x4805e681, 0x49b9, 0x11d3,                              \
{ 0x9c, 0xe4, 0xed, 0x60, 0xbd, 0x6c, 0xb5, 0xbc } }


class nsIFileSpec;

class nsIEditorLogging : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITORLOGGING_IID; return iid; }


  /* Start logging */
  NS_IMETHOD StartLogging(nsIFile *aLogFile)=0;

  /* Stop logging */
  NS_IMETHOD StopLogging()=0;
  
};


#endif // nsIEditorLogging_h__
