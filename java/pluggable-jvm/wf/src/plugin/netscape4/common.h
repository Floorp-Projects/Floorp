/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is The Waterfall Java Plugin Module
 * 
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 *
 * $Id: common.h,v 1.1 2001/05/10 18:12:36 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#ifndef _common_h
#define _common_h
#include "npapi.h"
/***********************************************************************
 * Instance state information about the plugin.
 *
 * PLUGIN DEVELOPERS:
 *	Use this struct to hold per-instance information that you'll
 *	need in the various functions in this file.
 ***********************************************************************/

typedef struct _PluginInstance
{
  void*                 info; /* really "this" */
  NPP                   m_peer;
  jint                  m_id;
  jint                  m_wid;
  int16                 m_argc;
  char**                m_argn;
  char**                m_argv;
  char*                 m_docbase;
}  PluginInstance;

typedef struct _GlobalData
{
#ifdef XP_UNIX
  Display*              m_dpy;
  /* message queue ID of shared memory transport in host -> JVM direction */
  int                   m_msgid;
  /* message queue ID of shared memory transport in JVM -> host direction */
  int                   m_msgid1;
  /* XID of widget used to notify browser about our requests */
  XID                   m_xid;
#endif 
} GlobalData;

#define  PE_NOPE      0
#define  PE_CREATE    1
#define  PE_SETWINDOW 2
#define  PE_DESTROY   3
#define  PE_START     4
#define  PE_STOP      5
#define  PE_NEWPARAMS 6
#define  PE_SETTYPE   7

#define  PV_UNKNOWN   0
#define  PV_MOZILLA6  1
#define  PV_MOZILLA4  2

#define PT_UNKNOWN    0
#define PT_EMBED      1
#define PT_OBJECT     2
#define PT_APPLET     3
#define PT_PLUGLET    4

#define WFNetscape4VendorID   2 /* Netscape 4 */
#define WFNetscape4ExtVersion 1

#endif
