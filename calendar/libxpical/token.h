/*
** Copyright (C) 1998-1999 Greg Stein. All Rights Reserved.
**
** By using this file, you agree to the terms and conditions set forth in
** the LICENSE.html file which can be found at the top level of the mod_dav
** distribution or at http://www.webdav.org/mod_dav/license-1.html.
**
** Contact information:
**   Greg Stein, PO Box 3151, Redmond, WA, 98073
**   gstein@lyra.org, http://www.webdav.org/mod_dav/
*/

/*
** DAV opaquelocktoken scheme implementation
**
** Written 5/99 by Keith Wannamaker, wannamak@us.ibm.com
** Adapted from ISO/DCE RPC spec and a former Internet Draft
** by Leach and Salz:
** http://www.ics.uci.edu/pub/ietf/webdav/uuid-guid/draft-leach-uuids-guids-01
**
** Portions of the code are covered by the following license:
**
** Copyright (c) 1990- 1993, 1996 Open Software Foundation, Inc.
** Copyright (c) 1989 by Hewlett-Packard Company, Palo Alto, Ca. &
** Digital Equipment Corporation, Maynard, Mass.
** Copyright (c) 1998 Microsoft.
** To anyone who acknowledges that this file is provided "AS IS"
** without any express or implied warranty: permission to use, copy,
** modify, and distribute this file for any purpose is hereby
** granted without fee, provided that the above copyright notices and
** this notice appears in all source code copies, and that none of
** the names of Open Software Foundation, Inc., Hewlett-Packard
** Company, or Digital Equipment Corporation be used in advertising
** or publicity pertaining to distribution of the software without
** specific, written prior permission.  Neither Open Software
** Foundation, Inc., Hewlett-Packard Company, Microsoft, nor Digital Equipment
** Corporation makes any representations about the suitability of
** this software for any purpose.
*/

/*This file was stolen from webtools/mozbot/uuidgen/token.h */

#ifndef _TOKEN_H_
#define _TOKEN_H_

typedef unsigned long   unsigned32;
typedef unsigned short  unsigned16;
typedef unsigned char   unsigned8;

typedef struct {
    char nodeID[6];
} uuid_node_t;

#undef uuid_t

typedef struct _uuid_t
{
    unsigned32	time_low;
    unsigned16	time_mid;
    unsigned16	time_hi_and_version;
    unsigned8	clock_seq_hi_and_reserved;
    unsigned8	clock_seq_low;
    unsigned8	node[6];
} uuid_t;

/* data type for UUID generator persistent state */
	
typedef struct {
    uuid_node_t node;     /* saved node ID */
    unsigned16 cs;        /* saved clock sequence */
} uuid_state;

extern const uuid_t null_locktoken;

/* in dav_opaquelock.c */
int create_token(uuid_state *st, uuid_t *u);
void create_uuid_state(uuid_state *st);
void format_token(char *target, const uuid_t *u);
int compare_token(const uuid_t a, const uuid_t b);
int parse_token(const char *char_token, uuid_t *bin_token);

#endif /* _TOKEN_H_ */
