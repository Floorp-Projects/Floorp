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
/*
   jscookie.h -- javascript reflection of cookies for filters.
   Created: Frederick G.M. Roeber <roeber@netscape.com>, 12-Jul-97.
   Adopted: Judson Valeski, 1997
 */

#ifndef _JSCOOKIE_H_
#define _JSCOOKIE_H_

typedef enum {
    JSCF_accept,
    JSCF_reject,
    JSCF_ask,
    JSCF_whatever,
    JSCF_error
}
    JSCFResult;

typedef struct {
    char *path_from_header;
    char *host_from_header;
    char *name_from_header;
    char *cookie_from_header;
    time_t expires;
    char *url;
    Bool secure;
    Bool domain;
    Bool prompt; /* the preference */
    NET_CookieBehaviorEnum preference;
}
    JSCFCookieData;

extern JSCFResult JSCF_Execute(
                               MWContext *mwcontext,
                               const char *script_name,
                               JSCFCookieData *data,
                               Bool *data_changed
                               );

/* runs the garbage collector on the filter context. Probably a good
   idea to call on completion of NET_GetURL or something. */
extern void JSCF_Cleanup(void);

#endif /* _JSCOOKIE_H_ */

