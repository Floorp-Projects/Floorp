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

#include <stdio.h>
#include "nsLayer.h"

static NS_DEFINE_IID(kILayerIID,       NS_ILAYER_IID);

nsLayer::nsLayer(nsISupports* outer)
{
  NS_INIT_REFCNT();
  mpCal = 0;
}

NS_IMPL_QUERY_INTERFACE(nsLayer, kILayerIID)
NS_IMPL_ADDREF(nsLayer)
NS_IMPL_RELEASE(nsLayer)

nsLayer::~nsLayer()
{
}

nsresult nsLayer::Init()
{
  return (NS_OK);
}

nsresult nsLayer::SetCurl(const JulianString& s)
{
  msCurl = s;
  return (NS_OK);
}

nsresult nsLayer::GetCurl(JulianString& s)
{
  s = msCurl;
  return (NS_OK);
}


nsresult nsLayer::SetCal(NSCalendar* aCal)
{
  mpCal = aCal;
  return (NS_OK);
}


nsresult nsLayer::GetCal(NSCalendar*& aCal)
{
  aCal = mpCal;
  return (NS_OK);
}


nsresult nsLayer::FetchEventsByRange(
                      const DateTime*  aStart, 
                      const DateTime*  aStop,
                      JulianPtrArray*& anArray
                      )
{
  return (NS_OK);
}




