/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is Basic Socket Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

#ifndef bsutil_h___
#define bsutil_h___

PR_BEGIN_EXTERN_C

extern PRBool
bs_util_is_ip (char *str);

extern PRStatus
bs_util_resolve_host (const char *hostname, PRNetAddr *na);

extern bschar *
bs_util_linebuffer (bschar *newline, bschar **buffer, PRBool flush);

extern char **
bs_util_delimbuffer_to_array (char *longbuf, int *lines, char delim);

PR_END_EXTERN_C

#endif /* bsutil_h___ */
