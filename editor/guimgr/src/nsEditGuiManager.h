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

#ifndef nsEditGuiManager_h__
#define nsEditGuiManager_h__

#include "nsIEditGuiManager.h"

/** implementation of an Editor GUI Manager object.
 *
 */
class nsEditGuiManager : public nsIEditGuiManager
{
private:


public:

  /** The default constructor.
   */
  nsEditGuiManager();

  /** The default destructor.
   */
  virtual ~nsEditGuiManager();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsEditGuiManager method implementations. */

  /* nsEditGuiManager specific methods. */
};

#endif // nsGuiManager_h__
