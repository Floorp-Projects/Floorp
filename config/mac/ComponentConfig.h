/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
* ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
*	Component_Config.h
* ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
* This file is the Mac equivalent to ns/config/liteness.mak on Windows.
* Set different #define's to control what components get built.
*/

#if defined(MOZ_LITE)	/* Nav-only */
	#define MOZ_JSD
	#define MOZ_NAV_BUILD_PREFIX
#elif defined(MOZ_MEDIUM)	/* Nav + Composer */
	#define EDITOR
	#define MOZ_JSD
	#define MOZ_COMMUNICATOR_CONFIG_JS
	#define MOZ_SPELLCHK
#else	/* The WHOLE enchilada */
	#define EDITOR
	#define MOZ_OFFLINE
	#define MOZ_LOC_INDEP
	#define MOZ_TASKBAR
	#define MOZ_LDAP
	#define MOZ_ADMIN_LIB
	#define MOZ_COMMUNICATOR_NAME
	#define MOZ_JSD
	#define MOZ_IFC_TOOLS
	#define MOZ_NETCAST
	#define MOZ_COMMUNICATOR_ABOUT
	#define MOZ_COMMUNICATOR_CONFIG_JS
	#define MOZ_SPELLCHK
#endif
