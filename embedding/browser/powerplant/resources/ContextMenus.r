/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#include <PowerPlant.r>
#include <Types.r>
#include "ApplIDs.h"

/*
    These are all of the commands which can be done by CBrowserShell in
    any possible context. PowerPlant's LCMAttachment takes care
    of removing any commands which should not be enabled for the current
    click by means of FindCommandStatus of the target (CBrowserShell)
*/

resource 'Mcmd' (mcmd_BrowserShellContextMenuCmds, "BrowserShellContextMenu") { {
    cmd_OpenLinkInNewWindow,
    msg_Nothing,
	cmd_Back,
	cmd_Forward,
	cmd_Reload,
	cmd_Stop,
	msg_Nothing,
	cmd_ViewPageSource,
	cmd_ViewImage,
	cmd_ViewBackgroundImage,
	msg_Nothing,
	cmd_SelectAll,
	cmd_Copy,
	msg_Nothing,
	cmd_CopyImage,
	cmd_CopyLinkLocation,
	cmd_CopyImageLocation,
	msg_Nothing,
	cmd_SaveLinkTarget,
	cmd_SaveImage,
	msg_Nothing,
	cmd_PrefillForm
} };

/*
    The MENU/Mcmd combination that we use to get the text for
    context menus items. PowerPlant searches its menu list in
    order to do this. We can add this menu to the menu list as
    a hierarchical menu so it won't show but can be used for
    this purpose. The name "buzzwords" is taken from MacApp which
    uses (and officially supports) this technique.
*/

resource 'MENU' (menu_Buzzwords, "Buzzwords") {
	menu_Buzzwords, textMenuProc, allEnabled, enabled,	"",
	{
	    "Open Link In New Window",  noIcon,	noKey,	noMark,	plain,
		"Back",		                noIcon,	noKey,	noMark,	plain,
		"Forward",	                noIcon, noKey,	noMark,	plain,
		"Reload",		            noIcon, noKey,	noMark, plain,
		"Stop",						noIcon, noKey,	noMark, plain,
		"View Page Source",			noIcon,	noKey,	noMark,	plain,
		"View Image",		        noIcon, noKey,	noMark, plain,
		"View Background Image",    noIcon, noKey,	noMark, plain,
		"Copy Image",		        noIcon, noKey,	noMark, plain,
		"Save Link Target As…",  noIcon, noKey,	noMark, plain,
		"Save Image As…",       noIcon, noKey,	noMark, plain,
		"Copy Link Location",		noIcon, noKey,	noMark, plain,
		"Copy Image Location",		noIcon, noKey,	noMark, plain,
	}
};

resource 'Mcmd' (menu_Buzzwords, "Buzzwords") { {
    cmd_OpenLinkInNewWindow,
	cmd_Back,
	cmd_Forward,
	cmd_Reload,
	cmd_Stop,
	cmd_ViewPageSource,
	cmd_ViewImage,
	cmd_ViewBackgroundImage,
	cmd_CopyImage,
	cmd_SaveLinkTarget,
	cmd_SaveImage,
	cmd_CopyLinkLocation,
	cmd_CopyImageLocation,
} };
