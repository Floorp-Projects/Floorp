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

#ifndef nsMessageCompCID_h__
#define nsMessageCompCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_MSGCOMPOSESERVICE_CID				  \
{ /* 588595FE-1ADA-11d3-A715-0060B0EB39B5 */      \
 0x588595fe, 0x1ada, 0x11d3,                      \
 {0xa7, 0x15, 0x0, 0x60, 0xb0, 0xeb, 0x39, 0xb5}}

#define NS_MSGCOMPOSE_CID						  \
{ /* EB5BDAF8-BBC6-11d2-A6EC-0060B0EB39B5 */      \
 0xeb5bdaf8, 0xbbc6, 0x11d2,                      \
 {0xa6, 0xec, 0x0, 0x60, 0xb0, 0xeb, 0x39, 0xb5}}

#define NS_MSGCOMPFIELDS_CID	                  \
{ /* 6D222BA0-BD46-11d2-8293-000000000000 */      \
 0x6d222ba0, 0xbd46, 0x11d2,                      \
 {0x82, 0x93, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}

#define NS_MSGSEND_CID							  \
{ /* 935284E0-C5D8-11d2-8297-000000000000 */      \
 0x935284e0, 0xc5d8, 0x11d2,                      \
 {0x82, 0x97, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}

#define NS_MSGSENDLATER_CID							              \
{ /* E15C83F1-1CF4-11d3-8EF0-00A024A7D144 */      \
 0xe15c83f1, 0x1cf4, 0x11d3,                      \
 {0x8e, 0xf0, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 }}

#define NS_MSGPOST_CID                            \
{ /* 80a368c0-f1bf-11d2-8921-004005263078 */      \
 0x80a368c0, 0xf1bf, 0x11d2,                      \
 {0x89, 0x21, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78}}

#define NS_SMTPURL_CID                            \
{ /* BE59DBF0-2812-11d3-80A3-006008128C4E} */      \
 0xbe59dbf0, 0x2812, 0x11d3,                      \
 {0x80, 0xa3, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}

//
// nsSmtpServer
//

#define NS_SMTPSERVER_PROGID \
  "component://netscape/messenger/smtp/server"

#define NS_SMTPSERVER_CID                      \
{ /* 60dc861a-56ce-11d3-9118-00a0c900d445 */   \
  0x60dc861a,0x56ce,0x11d3,0x91,0x18,          \
    { 0x0, 0xa0, 0xc9, 0x0, 0xd4, 0x45 }};     \

#define NS_SMTPSERVICE_CID						  \
{ /* 5B6419F1-CA9B-11d2-8063-006008128C4E */      \
 0x5b6419f1, 0xca9b, 0x11d2,                      \
 {0x80, 0x63, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}

#endif // nsMessageCompCID_h__
