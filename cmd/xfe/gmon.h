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

#ifndef _GMON_H_
#define _GMON_H_

#ifdef	__cplusplus
extern "C" {
#endif

extern void gmon_init();
extern void gmon_start();
extern void gmon_stop();
extern void gmon_reset();
extern unsigned gmon_is_running();
extern void gmon_report_set_reverse(unsigned reverse);
extern unsigned gmon_report_get_reverse();
extern void gmon_report_set_size(unsigned reverse);
extern unsigned gmon_report_get_size();
extern void gmon_dump();
extern void gmon_dump_to(char*);
extern int  gmon_gprof_to(char* gmonout, char* gprofout, unsigned nfuncs);
extern int  gmon_html_filter_to(char* gprofout, char* html, unsigned reverse);

#ifdef	__cplusplus
};
#endif

#endif /*_GMON_H_*/
