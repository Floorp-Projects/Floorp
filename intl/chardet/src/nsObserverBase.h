/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifndef nsObserverBase_h__
#define nsObserverBase_h__


#include "nsIParser.h"
//========================================================================== 
//
// Class declaration for the class 
//
//========================================================================== 
class nsObserverBase {

public:

  nsObserverBase() {};
  virtual ~nsObserverBase() {};

  /*
   *   Subject call observer when the parser hit the tag
   *   @param aDocumentID- ID of the document
   *   @param aTag- the tag
   *   @param valueArray - array of value
   */
protected:

  NS_IMETHOD NotifyWebShell(PRUint32 aDocumentID, 
                            const char* charset, 
                            nsCharsetSource source,
                            const char* aCmd = nsnull);

};

#endif /* nsObserverBase_h__ */
