/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * $Id: jvmp_errors.h,v 1.2 2001/07/12 19:58:07 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_ERRORS_H
#define _JVMP_ERRORS_H
#ifdef __cplusplus
extern "C" {
#endif
#define JVMP_MAXERRORLENGTH        1024
#define JVMP_NUMERRORS             32

/* error codes */
#define JVMP_ERROR_NOERROR         0
#define JVMP_ERROR_NOSUCHCLASS     1
#define JVMP_ERROR_NOSUCHMETHOD    2
#define JVMP_ERROR_FILENOTFOUND    3
#define JVMP_ERROR_TRANSPORTFAILED 4
#define JVMP_ERROR_FAILURE         5
#define JVMP_ERROR_OUTOFMEMORY     6
#define JVMP_ERROR_NULLPOINTER     7
#define JVMP_ERROR_INITFAILED      8
#define JVMP_ERROR_NOACCESS        9
#define JVMP_ERROR_JAVAEXCEPTION   10
#define JVMP_ERROR_METHODFAILED    11
#define JVMP_ERROR_INVALIDARGS     12
#define JVMP_ERROR_CAPSNOTGIVEN    13
#ifdef __cplusplus
};
#endif
#endif
