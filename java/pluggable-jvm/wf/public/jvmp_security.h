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
 * $Id: jvmp_security.h,v 1.2 2001/07/12 19:58:08 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_SECURITY_H
#define _JVMP_SECURITY_H
#ifdef __cplusplus
extern "C" {
#endif
#include "jvmp_caps.h"

struct JVMP_CallingContext;
typedef struct JVMP_CallingContext  JVMP_CallingContext;
struct JVMP_CallingContext {
  JVMP_CallingContext* This;      /* pointer to this structure */
  JNIEnv           *env;          /* JNIEnv of the caller. NULL if remote case.
				     Anyway users of JVMP shouldn't rely on this field.*/
  void*            data;          /* data for this threading context */
  JVMP_ThreadInfo* source_thread; /* associated thread */
  void*            reserved0;     /* reserved fields for future using */
  void*            reserved1;
  void*            reserved2;
  JVMP_SecurityCap caps;          /* current capabilities of this context */
  jbyteArray       jcaps;         /* cached Java structure to store caps */
  jint             err;
  /* those functions to simplify work with caps */
  jint             (*AllowCap)(JVMP_CallingContext *ctx,
			       jint cap_no);
  jint             (*GetCap)(JVMP_CallingContext *ctx,
			     jint cap_no);
  jint             (*SetCaps)(JVMP_CallingContext *ctx,
			      JVMP_SecurityCap *new_caps);
  jint             (*IsActionAllowed)(JVMP_CallingContext *ctx,
				      JVMP_SecurityAction *action);
};
#ifdef __cplusplus
};
#endif
#endif




