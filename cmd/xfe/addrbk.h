/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
   addrbk.h --- X-specific headers for the front end.
   Created: Tao Cheng  <tao@netscape.com>, 10-nov-94.
 */
#ifndef _ADDRBK_H_
#define _ADDRBK_H_

#include "addrbook.h"
#include "abcom.h"
#include "xfe.h"

#define AB_MAX_ENTRIES  175000
#define AB_MAX_STRLEN   1024

typedef struct fe_addrbk_data 
{

  ABPane *abpane;
  ABID editUID;
  ABID editLID;

  feABtype type;

  Widget mcwin;
  Widget outline;
  Widget edituser;              /* Edit user container widget */

  /* Personal Folder */
  Widget edituser_p;  
  Widget edituser_p_fn;
  Widget edituser_p_mi;
  Widget edituser_p_ln;
  Widget edituser_p_org;
  Widget edituser_p_desp;
  Widget edituser_p_locality;
  Widget edituser_p_region;
  Widget edituser_p_nickname;
  Widget edituser_p_em;

  /* Security Folder */
  Widget edituser_b; 
  Widget edituser_b_text;

  /* Edit list stuff */
  Widget editlist;  
  Widget editlist_m;
  Widget editlist_m_nickname;
  Widget editlist_m_name;
  Widget editlist_m_desp;
  Widget editlist_m_members;

  /* General window */
  Widget title;
  Widget nickname;
  Widget name;
  Widget lname;
  Widget mname;

  /* tool bar stuff */
  Widget queryText;
  Widget helpBar;

  /* find stuff */
  Widget findshell; 
  Widget findtext;
  Widget findnicknameT;
  Widget findnameT;
  Widget findlocationT;
  Widget finddescriptionT;
  Widget findcaseT;
  Widget findwordT;

  Widget popup;			/* addrbook popup menu */
} fe_addrbk_data;

/* define callback proc and struc
 */

typedef enum {
  TO = 0,
  CC,
  BCC,
  REPLYTO,
  FOLLOWUPTO,
  NEWSGROUP
} SEND_STATUS;

typedef struct {
  SEND_STATUS  status;
  DIR_Server  *dir;  
  ABID         id;
  char        *emailAddr;
  char        *dplyStr;
  ABID         type;
} StatusID_t;

typedef struct {
  StatusID_t** m_pairs;
  int          m_count;
} ABAddrMsgCBProcStruc;

typedef void (*ABAddrMsgCBProc)(ABAddrMsgCBProcStruc* clientData, 
								void*                 callData);

XP_BEGIN_PROTOS

void
fe_showAddrMSGDlg(Widget toplevel, 
                  ABAddrMsgCBProc proc, void* callData, MWContext *context);

XP_END_PROTOS

#endif /* _ADDRBK_H_ */
