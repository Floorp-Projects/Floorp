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
 * $Id: test_extension.c,v 1.2 2001/07/12 19:58:14 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include "jvmp.h"

#include <stdlib.h>
#include <string.h>

/* Extension parameters */
static const char* WFCID = "@sun.com/wf/tests/testextension;1";
static const jint WFExtVersion    = 1;
static JVMP_SecurityCap g_moz_caps;
static JVMP_SecurityCap g_moz_cap_mask;
/* range of used capabilities */
#define MOZ_CAPS_LOW      101
#define MOZ_CAPS_HIGH     105
static JVMP_ExtDescription g_Description = 
{
  "Sun",    /* vendor */
  "1.0",    /* version */
  "Waterfall test extension", /* description */  
  PLATFORM, /* platform */
  ARCH,     /* architecture */
  "GPL",    /* license */
  NULL      /* vendor data */
};


/* extension API implementation */
static jint JNICALL JVMPExt_Init(jint side)
{
    int i;

    JVMP_FORBID_ALL_CAPS(g_moz_caps);
    JVMP_ALLOW_ALL_CAPS(g_moz_cap_mask);
    for (i=MOZ_CAPS_LOW; i <= MOZ_CAPS_HIGH; i++)	
	JVMP_ALLOW_CAP(g_moz_caps, i);
    return JNI_TRUE;
}

static jint JNICALL JVMPExt_Shutdown()
{
  return JNI_TRUE;
}
static jint JNICALL JVMPExt_GetExtInfo(const char*           *pCID, 
				       jint                  *pVersion,
				       JVMP_ExtDescription*  *pDesc)
{
  *pCID = WFCID;
  *pVersion = WFExtVersion;
  if (pDesc) *pDesc = &g_Description;
  return JNI_TRUE;
}
static jint JNICALL JVMPExt_GetBootstrapClass(char* *bootstrapClassPath,
					      char* *bootstrapClassName)
{
  /* should be defined at installation time */
  *bootstrapClassPath = "";
  *bootstrapClassName = "sun.jvmp.test.TestPeerFactory";
  return JNI_TRUE;
}
static jint JNICALL JVMPExt_ScheduleRequest(JVMP_ShmRequest* req, jint local)
{
  return JNI_TRUE;
}

static jint JNICALL JVMPExt_GetCapsRange(JVMP_SecurityCap* caps,
					 JVMP_SecurityCap* sh_mask)
{    
    if ((caps == NULL) || (sh_mask == NULL)) return JNI_FALSE;
    memcpy(caps, &g_moz_caps, sizeof(JVMP_SecurityCap));
    memcpy(sh_mask, &g_moz_cap_mask, sizeof(JVMP_SecurityCap));
    return JNI_TRUE;
}


static JVMP_Extension JVMP_Ext = 
{
    &JVMPExt_Init,
    &JVMPExt_Shutdown,
    &JVMPExt_GetExtInfo,
    &JVMPExt_GetBootstrapClass,
    &JVMPExt_ScheduleRequest,
    &JVMPExt_GetCapsRange
};

JNIEXPORT jint JNICALL JVMP_GetExtension(JVMP_Extension** ext)
{
    *ext = &JVMP_Ext;
    return JNI_TRUE;
}
