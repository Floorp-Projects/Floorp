 /*
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

#include "resgui.h"					// main window constants

#ifdef ALPHA
//	read 'TEXT' (ABOUT_LICENSE, "Export License", purgeable, protected ) "::xp:LICENSE-alpha";

#elif defined (BETA)

	#ifdef EXPORT_VERSION
// 	read 'TEXT' (ABOUT_LICENSE, "Export License", purgeable, protected ) "::xp:LICENSE-export-beta";
	#else
//	read 'TEXT' (ABOUT_LICENSE, "U.S. License", purgeable, protected ) "::xp:LICENSE-us-beta";
	#endif

#else	/* Final release */

	#ifdef NET
		#ifdef EXPORT_VERSION
// 		read 'TEXT' (ABOUT_LICENSE, "Export License", purgeable, protected ) "::xp:LICENSE-export-net";
		#else
// 		read 'TEXT' (ABOUT_LICENSE, "U.S. License", purgeable, protected ) "::xp:LICENSE-us-net";
		#endif
	#else /* RETAIL */
		#ifdef EXPORT_VERSION
// 		read 'TEXT' (ABOUT_LICENSE, "Export License", purgeable, protected ) "::xp:LICENSE-export";
		#else
// 		read 'TEXT' (ABOUT_LICENSE, "U.S. License", purgeable, protected ) "::xp:LICENSE-us";
		#endif
	#endif
#endif
