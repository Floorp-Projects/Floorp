/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsCalNetFetchCommand_h__
#define nsCalNetFetchCommand_h__

#include "nscalexport.h"
#include "nsCalNetFetchVals.h"
#include "nsICalNetFetchCommand.h"
#include "nsIArray.h"


class NS_CALENDAR nsCalNetFetchCommand 
{

  nsIArray * mpLayerList ;		// all the layers that need the events from this range

public:
  nsCalNetFetchCommand(nsISupports* outer);
  ~nsCalNetFetchCommand();

  NS_DECL_ISUPPORTS

  NS_IMETHOD UpdateRange(DateTime d1, DateTime d2);
  NS_IMETHOD AddLayer(nsILayer* pLayer);
  NS_IMETHOD Execute(PRInt32 iPri);
  NS_IMETHOD Cancel();
};

#endif //nsCalNetFetchCommand_h__
