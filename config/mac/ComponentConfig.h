/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
	#define MOZ_MAIL_NEWS
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
