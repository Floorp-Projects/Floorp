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
 * $Id: jvmp_vendor.h,v 1.2 2001/07/12 19:58:08 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_VENDOR_H
#define _JVMP_VENDOR_H
#ifdef __cplusplus
extern "C" {
#endif
struct JVMP_PluginDescription {
  const char* vendor;       /* string describing JVM vendor, like "Sun" */
  const char* jvm_version;  /* JVM version, like "1.3.1" */
  const char* jvm_kind;     /* JVM kind, like "hotspot" */
  const char* jvmp_version; /* plugin version, like "1.1" */
  const char* platform;     /* platform */
  const char* arch;         /* architecture this plugin compiled for */
  const void* vendor_data;  /* vendor-specific data */
};
typedef struct JVMP_PluginDescription  JVMP_PluginDescription;

struct JVMP_ExtDescription {
  const char* vendor;       /* string describing ext vendor, like "Sun" */
  const char* version;      /* extension version, like "0.9" */
  const char* descr;        /* description of this extension, like
			       "Mozilla applet viewer Waterfall extension" */
  const char* platform;     /* platform, like "linux" */
  const char* arch;         /* architecture this extension compiled for, 
			       like "i386" */
  const char* license;      /* license type of this extension, like "GPL" */
  const void* vendor_data;  /* vendor-specific data */
};
typedef struct JVMP_ExtDescription JVMP_ExtDescription; 
#ifdef __cplusplus
};
#endif
#endif
