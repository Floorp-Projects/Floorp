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
 * $Id: awt_Plugin.h,v 1.2 2001/07/12 19:57:44 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

/*
 * @(#)awt_Plugin.h	1.5 00/05/04
 *
 * Copyright 2000 Sun Microsystems, Inc. All rights reserved.
 * Copyright 2000 Sun Microsystems, Inc. Tous droits réservés.
 *
 * This software is the proprietary information of Sun Microsystems, Inc.
 * Use is subject to license terms.
 */

/*
 * Fix 4221246: Export functions for Netscape to use to get AWT info
 */

#ifndef _AWT_PLUGIN_H_
#define _AWT_PLUGIN_H_

#include <jni.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void getAwtLockFunctions(void (**AwtLock)(JNIEnv *),
			 void (**AwtUnlock)(JNIEnv *),
			 void (**AwtNoFlushUnlock)(JNIEnv *),
			 void *);

void getExtAwtData(Display *,
		   int,
		   int *,          /* awt_depth */
		   Colormap *,     /* awt_cmap  */
		   Visual **,      /* awt_visInfo.visual */
		   int *,          /* awt_num_colors */
		   void *);

void getAwtData(int *, Colormap *, Visual **, int *, void *);

Display *getAwtDisplay(void);

#endif /* _AWT_PLUGIN_H_ */
