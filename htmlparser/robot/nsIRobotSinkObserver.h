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
#ifndef nsIRobotSinkObserver_h___
#define nsIRobotSinkObserver_h___

#include "nsISupports.h"
class nsString;

/* fab1d970-cfda-11d1-9328-00805f8add32 */
#define NS_IROBOTSINKOBSERVER_IID \
{ 0xfab1d970, 0xcfda, 0x11d1,	  \
  {0x93, 0x28, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsIRobotSinkObserver : public nsISupports {
public:
  NS_IMETHOD ProcessLink(const nsString& aURLSpec) = 0;
  NS_IMETHOD VerifyDirectory(const char * verify_dir) = 0;
};

#endif /* nsIRobotSinkObserver_h___ */
