/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

  NS_IMETHOD NotifyWebShell(nsISupports* aDocumentID, 
                            const char* charset, 
                            nsCharsetSource source);

};

#endif /* nsObserverBase_h__ */
