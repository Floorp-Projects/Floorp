/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsICalICalendarContentSink_h___
#define nsICalICalendarContentSink_h___

#include "nsISupports.h"
#include "nsIWebViewerContainer.h"

// e2549d00-5a4d-11d2-9432-006008268c31
#define NS_ICALICALENDAR_CONTENT_SINK_IID   \
{ 0xe2549d00, 0x5a4d, 0x11d2,    \
{ 0x94, 0x32, 0x00, 0x60, 0x08, 0x26, 0x8c, 0x31 } }

class nsICalICalendarContentSink : public nsISupports
{

public:

  NS_IMETHOD                 Init() = 0 ;
  NS_IMETHOD                 SetViewerContainer(nsIWebViewerContainer * aViewerContainer) = 0;

};

#endif /* nsICalICalendarContentSink_h___ */

