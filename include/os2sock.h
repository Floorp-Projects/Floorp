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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* New file created by IBM-VPB050196 */
#ifndef _OS2SOCK_H
#define _OS2SOCK_H
#if !defined(RC_INVOKED)
#include "mcom_db.h"
#endif

/*DSR050297 - this is based on types.h in the TCP/IP headers...               */
/*I'm not including types.h because that causes all sorts of damage...        */
#define MAXHOSTNAMELEN 120

#if defined(XP_OS2_DOUGSOCK)
#ifndef BSD_SELECT
#error you need BSD_SELECT defined in your command line for all files
#endif
#include <nerrno.h>
#include <sys\socket.h>
#include <sys\select.h>
#include <sys\time.h>
#include <sys\ioctl.h>
#include <netdb.h>
#include <utils.h>

#else
/*DSR072196 - replaced many files with pmwsock.h...*/
#include <pmwsock.h>

#ifndef IP_MULTICAST_IF
#define IP_MULTICAST_IF    2            /* set/get IP multicast interface*/
#define IP_MULTICAST_TTL   3            /* set/get IP multicast timetolive*/
#define IP_MULTICAST_LOOP  4            /* set/get IP multicast loopback */
#define IP_ADD_MEMBERSHIP  5            /* add  an IP group membership   */
#define IP_DROP_MEMBERSHIP 6            /* drop an IP group membership   */

#define IP_DEFAULT_MULTICAST_TTL  1     /* normally limit m'casts to 1 hop */
#define IP_DEFAULT_MULTICAST_LOOP 1     /* normally hear sends if a member */
#define IP_MAX_MEMBERSHIPS       20     /* per socket; must fit in one mbuf*/

/*
 * Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP.
 */
struct ip_mreq {
        struct in_addr  imr_multiaddr;  /* IP multicast address of group */
        struct in_addr  imr_interface;  /* local IP address of interface */
};
#endif
#endif

#endif

