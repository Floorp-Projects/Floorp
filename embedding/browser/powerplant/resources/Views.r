/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is PPEmbed code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ApplIDs.h"
#include <Types.r>
#include <PowerPlant.r>

data 'CTYP' (129, "CBrowserShell") {
	$"0001 6F62 6A64 0000 0024 7670 7465 0D43"            /* ..objd...$vpte.C */
	$"4272 6F77 7365 7253 6865 6C6C 0000 0042"            /* BrowserShell...B */
	$"726F 5376 6965 7700 C800 C800 0000 6265"            /* roSview.».»...be */
	$"6773 6F62 6A64 0000 0023 696E 7476 0B43"            /* gsobjd...#intv.C */
	$"6872 6F6D 6546 6C61 6773 0000 0000 0101"            /* hromeFlags...... */
	$"5050 6F62 0000 0001 0020 0000 006F 626A"            /* PPob..... ...obj */
	$"6400 0000 2569 6E74 760D 4973 4D61 696E"            /* d...%intv.IsMain */
	$"436F 6E74 656E 7400 0000 0001 0150 506F"            /* Content......PPo */
	$"6200 0000 0100 0800 0000 656E 6473 656E"            /* b.........endsen */
	$"642E"                                               /* d. */
};

data 'CTYP' (130, "CThrobber") {
	$"0001 6F62 6A64 0000 0020 7670 7465 0943"            /* ..objd... vpte∆C */
	$"5468 726F 6262 6572 0000 0054 6872 6263"            /* Throbber...Thrbc */
	$"6E74 6C00 2000 2000 0000 6265 6773 6F62"            /* ntl. . ...begsob */
	$"6A64 0000 0022 7265 736C 0552 6573 4944"            /* jd..."resl.ResID */
	$"0000 0000 0101 5050 6F62 0000 0080 0010"            /* ......PPob...Ä.. */
	$"0100 0000 4749 4620 656E 6473 656E 642E"            /* ....GIF endsend. */
};

data 'CTYP' (131, "IconServiceIcon") {
	$"0001 6F62 6A64 0000 0028 7670 7465 1143"            /* ..objd...(vpte.C */
	$"4963 6F6E 5365 7276 6963 6573 4963 6F6E"            /* IconServicesIcon */
	$"0000 0043 4953 4363 6E74 6C00 2000 2000"            /* ...CISCcntl. . . */
	$"0000 6265 6773 6F62 6A64 0000 0020 696E"            /* ..begsobjd... in */
	$"7476 0849 636F 6E54 7970 6500 0000 0001"            /* tv.IconType..... */
	$"0150 506F 624E 6F72 6D00 2000 0000 6F62"            /* .PPobNorm. ...ob */
	$"6A64 0000 0026 7265 736C 0949 636F 6E52"            /* jd...&resl∆IconR */
	$"6573 4944 0000 0000 0101 5050 6F62 0000"            /* esID......PPob.. */
	$"03E8 0010 0100 0000 5478 7472 656E 6473"            /* .Ë......Txtrends */
	$"656E 642E"                                          /* end. */
};

resource 'MBAR' (128, "MBAR_Standard") {
	{	/* array MenuArray: 3 elements */
		/* [1] */
		128,
		/* [2] */
		129,
		/* [3] */
		130
	}
};

resource 'MBAR' (129, "MBAR_Aqua") {
	{	/* array MenuArray: 3 elements */
		/* [1] */
		128,
		/* [2] */
		1129,
		/* [3] */
		1130
	}
};

resource 'MENU' (128, "Apple") {
	128,
	textMenuProc,
	0x7FFFFFFD,
	enabled,
	apple,
	{	/* array: 2 elements */
		/* [1] */
		"About PPEmbed…", noIcon, noKey, noMark, plain,
		/* [2] */
		"-", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (129, "File_Standard") {
	129,
	textMenuProc,
	0x7FFFFC00,
	enabled,
	"File",
	{	/* array: 11 elements */
		/* [1] */
		"New", noIcon, "N", noMark, plain,
		/* [2] */
		"Open File…", noIcon, "O", noMark, plain,
		/* [3] */
		"Open Directory…", noIcon, noKey, noMark, plain,
		/* [4] */
		"-", noIcon, noKey, noMark, plain,
		/* [5] */
		"Close", noIcon, "W", noMark, plain,
		/* [6] */
		"Save As…", noIcon, noKey, noMark, plain,
		/* [7] */
		"-", noIcon, noKey, noMark, plain,
		/* [8] */
		"Page Setup…", noIcon, noKey, noMark, plain,
		/* [9] */
		"Print…", noIcon, "P", noMark, plain,
		/* [10] */
		"-", noIcon, noKey, noMark, plain,
		/* [11] */
		"Quit", noIcon, "Q", noMark, plain
	}
};

resource 'MENU' (130, "Edit_Standard") {
	130,
	textMenuProc,
	0x7FFF5B7D,
	enabled,
	"Edit",
	{	/* array: 18 elements */
		/* [1] */
		"Undo", noIcon, "Z", noMark, plain,
		/* [2] */
		"-", noIcon, noKey, noMark, plain,
		/* [3] */
		"Cut", noIcon, "X", noMark, plain,
		/* [4] */
		"Copy", noIcon, "C", noMark, plain,
		/* [5] */
		"Paste", noIcon, "V", noMark, plain,
		/* [6] */
		"Clear", noIcon, noKey, noMark, plain,
		/* [7] */
		"Select All", noIcon, "A", noMark, plain,
		/* [8] */
		"-", noIcon, noKey, noMark, plain,
		/* [9] */
		"Find…", noIcon, "F", noMark, plain,
		/* [10] */
		"Find Next", noIcon, "G", noMark, plain,
		/* [11] */
		"-", noIcon, noKey, noMark, plain,
		/* [12] */
		"Save Form Data", noIcon, noKey, noMark, plain,
		/* [13] */
		"Prefill Form", noIcon, noKey, noMark, plain,
		/* [14] */
		"-", noIcon, noKey, noMark, plain,
		/* [15] */
		"Preferences…", noIcon, noKey, noMark, plain,
		/* [16] */
		"-", noIcon, noKey, noMark, plain,
		/* [17] */
		"Profiles…", noIcon, noKey, noMark, plain,
		/* [18] */
		"Logout…", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (1129, "File_Aqua") {
	1129,
	textMenuProc,
	0x7FFFFE00,
	enabled,
	"File",
	{	/* array: 9 elements */
		/* [1] */
		"New", noIcon, "N", noMark, plain,
		/* [2] */
		"Open File…", noIcon, "O", noMark, plain,
		/* [3] */
		"Open Directory…", noIcon, noKey, noMark, plain,
		/* [4] */
		"-", noIcon, noKey, noMark, plain,
		/* [5] */
		"Close", noIcon, "W", noMark, plain,
		/* [6] */
		"Save As…", noIcon, noKey, noMark, plain,
		/* [7] */
		"-", noIcon, noKey, noMark, plain,
		/* [8] */
		"Page Setup…", noIcon, noKey, noMark, plain,
		/* [9] */
		"Print…", noIcon, "P", noMark, plain
	}
};

resource 'MENU' (1130, "Edit_Aqua") {
	1130,
	textMenuProc,
	0x7FFFDB7D,
	enabled,
	"Edit",
	{	/* array: 16 elements */
		/* [1] */
		"Undo", noIcon, "Z", noMark, plain,
		/* [2] */
		"-", noIcon, noKey, noMark, plain,
		/* [3] */
		"Cut", noIcon, "X", noMark, plain,
		/* [4] */
		"Copy", noIcon, "C", noMark, plain,
		/* [5] */
		"Paste", noIcon, "V", noMark, plain,
		/* [6] */
		"Clear", noIcon, noKey, noMark, plain,
		/* [7] */
		"Select All", noIcon, "A", noMark, plain,
		/* [8] */
		"-", noIcon, noKey, noMark, plain,
		/* [9] */
		"Find…", noIcon, "F", noMark, plain,
		/* [10] */
		"Find Next", noIcon, "G", noMark, plain,
		/* [11] */
		"-", noIcon, noKey, noMark, plain,
		/* [12] */
		"Save Form Data", noIcon, noKey, noMark, plain,
		/* [13] */
		"Prefill Form", noIcon, noKey, noMark, plain,
		/* [14] */
		"-", noIcon, noKey, noMark, plain,
		/* [15] */
		"Profiles…", noIcon, noKey, noMark, plain,
		/* [16] */
		"Logout…", noIcon, noKey, noMark, plain
	}
};

resource 'Mcmd' (128, "Apple") {
	{	/* array CommandArray: 2 elements */
		/* [1] */
		cmd_About,
		/* [2] */
		msg_Nothing
	}
};

resource 'Mcmd' (129, "File_Standard") {
	{	/* array CommandArray: 11 elements */
		/* [1] */
		cmd_New,
		/* [2] */
		cmd_Open,
		/* [3] */
		cmd_OpenDirectory,
		/* [4] */
		msg_Nothing,
		/* [5] */
		cmd_Close,
		/* [6] */
		cmd_SaveAs,
		/* [7] */
		msg_Nothing,
		/* [8] */
		cmd_PageSetup,
		/* [9] */
		cmd_Print,
		/* [10] */
		msg_Nothing,
		/* [11] */
		cmd_Quit
	}
};

resource 'Mcmd' (130, "Edit_Standard") {
	{	/* array CommandArray: 18 elements */
		/* [1] */
		cmd_Undo,
		/* [2] */
		msg_Nothing,
		/* [3] */
		cmd_Cut,
		/* [4] */
		cmd_Copy,
		/* [5] */
		cmd_Paste,
		/* [6] */
		cmd_Clear,
		/* [7] */
		cmd_SelectAll,
		/* [8] */
		msg_Nothing,
		/* [9] */
		cmd_Find,
		/* [10] */
		cmd_FindNext,
		/* [11] */
		msg_Nothing,
		/* [12] */
		cmd_SaveFormData,
		/* [13] */
		cmd_PrefillForm,
		/* [14] */
		msg_Nothing,
		/* [15] */
		27,								/* cmd_Preferences */
		/* [16] */
		msg_Nothing,
		/* [17] */
		cmd_ManageProfiles,
		/* [18] */
		cmd_Logout
	}
};

resource 'Mcmd' (1129, "File_Aqua") {
	{	/* array CommandArray: 9 elements */
		/* [1] */
		cmd_New,
		/* [2] */
		cmd_Open,
		/* [3] */
		cmd_OpenDirectory,
		/* [4] */
		msg_Nothing,
		/* [5] */
		cmd_Close,
		/* [6] */
		cmd_SaveAs,
		/* [7] */
		msg_Nothing,
		/* [8] */
		cmd_PageSetup,
		/* [9] */
		cmd_Print
	}
};

resource 'Mcmd' (1130, "Edit_Aqua") {
	{	/* array CommandArray: 16 elements */
		/* [1] */
		cmd_Undo,
		/* [2] */
		msg_Nothing,
		/* [3] */
		cmd_Cut,
		/* [4] */
		cmd_Copy,
		/* [5] */
		cmd_Paste,
		/* [6] */
		cmd_Clear,
		/* [7] */
		cmd_SelectAll,
		/* [8] */
		msg_Nothing,
		/* [9] */
		cmd_Find,
		/* [10] */
		cmd_FindNext,
		/* [11] */
		msg_Nothing,
		/* [12] */
		cmd_SaveFormData,
		/* [13] */
		cmd_PrefillForm,
		/* [14] */
		msg_Nothing,
		/* [15] */
		cmd_ManageProfiles,
		/* [16] */
		cmd_Logout
	}
};

resource 'PPob' (130, "Status Bar") {
	{	/* array TagArray: 9 elements */
		/* [1] */
		ObjectData {
			AbstractView {
				0,
				{600, 16},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				0,
				0,
				0,
				defaultSuperView,
				24,
				44,
				0,
				0,
				16,
				16,
				noReconcileOverhang
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowHeader {
				0,
				{602, 16},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				-1,
				0,
				0,
				defaultSuperView,
				0,
				0,
				0,
				0,
				1,
				1,
				noReconcileOverhang,
				regular
			}
		},
		/* [4] */
		BeginSubs {

		},
		/* [5] */
		ObjectData {
			StaticText {
				1400136052,
				{416, 12},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				9,
				2,
				0,
				defaultSuperView,
				130,
				""
			}
		},
		/* [6] */
		ObjectData {
			ProgressBar {
				1349676903,
				{150, 14},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				unbound,
				429,
				1,
				0,
				defaultSuperView,
				0,
				0,
				0,
				0,
				determinate
			}
		},
		/* [7] */
		ObjectData {
			IconControl {
				1282368363,
				{16, 12},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				unbound,
				582,
				2,
				0,
				defaultSuperView,
				0,
				1320,
				suite,
				noTracking,
				center
			}
		},
		/* [8] */
		EndSubs {

		},
		/* [9] */
		EndSubs {

		}
	}
};

data 'PPob' (131, "ToolBar") {
	$"0002 6F62 6A64 0000 003C 7669 6577 0000"            /* ..objd...<view.. */
	$"0000 0258 0026 0101 0101 0100 0000 0000"            /* ...X.&.......... */
	$"0000 0000 0000 0000 FFFF FFFF 0000 0018"            /* ........ˇˇˇˇ.... */
	$"0000 002C 0000 0000 0000 0000 0000 0010"            /* ...,............ */
	$"0000 0010 0000 6265 6773 6F62 6A64 0000"            /* ......begsobjd.. */
	$"0051 7769 6E68 5749 4E48 0259 0027 0101"            /* .QwinhWINH.Y.'.. */
	$"0101 0100 FFFF FFFF FFFF FFFF 0000 0000"            /* ....ˇˇˇˇˇˇˇˇ.... */
	$"FFFF FFFF 0000 0000 0000 0000 0000 0000"            /* ˇˇˇˇ............ */
	$"0000 0000 0000 0001 0000 0001 0000 0000"            /* ................ */
	$"0000 0000 0000 0000 0000 0000 0000 0150"            /* ...............P */
	$"0000 0062 6567 736F 626A 6400 0000 3843"            /* ...begsobjd...8C */
	$"4953 4342 6163 6B00 2000 2001 0100 0000"            /* ISCBack. . ..... */
	$"0000 0000 0600 0000 0300 0000 00FF FFFF"            /* .............ˇˇˇ */
	$"FF42 6163 6B00 0000 0000 0000 0000 0000"            /* ˇBack........... */
	$"0042 6163 6B05 1E6F 626A 6400 0000 3843"            /* .Back..objd...8C */
	$"4953 4346 6F72 7700 2000 2001 0100 0000"            /* ISCForw. . ..... */
	$"0000 0000 2800 0000 0300 0000 00FF FFFF"            /* ....(........ˇˇˇ */
	$"FF46 6F72 7700 0000 0000 0000 0000 0000"            /* ˇForw........... */
	$"0046 6F72 7705 1F6F 626A 6400 0000 3843"            /* .Forw..objd...8C */
	$"4953 4352 4C6F 6100 2000 2001 0100 0000"            /* ISCRLoa. . ..... */
	$"0000 0000 4C00 0000 0300 0000 00FF FFFF"            /* ....L........ˇˇˇ */
	$"FF52 4C6F 6100 0000 0000 0000 0000 0000"            /* ˇRLoa........... */
	$"0052 4C6F 6105 206F 626A 6400 0000 3843"            /* .RLoa. objd...8C */
	$"4953 4353 746F 7000 2000 2001 0100 0000"            /* ISCStop. . ..... */
	$"0000 0000 6F00 0000 0300 0000 00FF FFFF"            /* ....o........ˇˇˇ */
	$"FF53 746F 7000 0000 0000 0000 0000 0000"            /* ˇStop........... */
	$"0053 746F 7005 2164 6F70 6C55 726C 466F"            /* .Stop.!doplUrlFo */
	$"626A 6400 0000 3B65 7478 7467 5572 6C01"            /* bjd...;etxtgUrl. */
	$"8F00 1601 0101 0001 0000 0000 9800 0000"            /* è...........ò... */
	$"0800 0000 00FF FFFF FF00 0000 0000 0000"            /* .....ˇˇˇˇ....... */
	$"0000 0000 0000 0000 0001 1000 8200 0400"            /* ............Ç... */
	$"2003 6F62 6A64 0000 0034 5468 7262 5448"            /*  .objd...4ThrbTH */
	$"5242 0020 0020 0101 0000 0100 0000 0234"            /* RB. . .........4 */
	$"0000 0003 0000 0000 FFFF FFFF 0000 0000"            /* ........ˇˇˇˇ.... */
	$"0000 0000 0000 0000 0000 0000 0080 656E"            /* .............Äen */
	$"6473 656E 6473 656E 642E"                           /* dsendsend. */
};

resource 'PPob' (1281, "Find Dialog") {
	{	/* array TagArray: 12 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1281,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				1131307884
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			EditText {
				1415936116,
				{276, 49},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				45,
				7,
				0,
				defaultSuperView,
				0,
				regular,
				useSystemFont,
				"",
				255,
				noBox,
				noWordWrap,
				hasAutoScroll,
				noTextBuffering,
				noOutlineHilite,
				noInlineInput,
				noTextServices,
				printingCharFilter
			}
		},
		/* [5] */
		ObjectData {
			CheckBox {
				1130460005,
				{127, 18},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				44,
				65,
				0,
				defaultSuperView,
				0,
				unchecked,
				useSystemFont,
				"Case Sensitive"
			}
		},
		/* [6] */
		ObjectData {
			StaticText {
				0,
				{37, 16},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				6,
				8,
				0,
				defaultSuperView,
				useSystemFont,
				"Find:"
			}
		},
		/* [7] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				259,
				134,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"Find",
				isDefault
			}
		},
		/* [8] */
		ObjectData {
			PushButton {
				1131307884,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				180,
				134,
				0,
				defaultSuperView,
				901,
				0,
				textOnly,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [9] */
		ObjectData {
			CheckBox {
				1467113840,
				{134, 18},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				178,
				66,
				0,
				defaultSuperView,
				0,
				unchecked,
				useSystemFont,
				"Wrap Around"
			}
		},
		/* [10] */
		ObjectData {
			CheckBox {
				1113678699,
				{148, 18},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				178,
				85,
				0,
				defaultSuperView,
				0,
				unchecked,
				useSystemFont,
				"Search Backwards"
			}
		},
		/* [11] */
		ObjectData {
			CheckBox {
				1164866674,
				{108, 18},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				44,
				84,
				0,
				defaultSuperView,
				0,
				unchecked,
				useSystemFont,
				"Entire Word"
			}
		},
		/* [12] */
		EndSubs {

		}
	}
};

resource 'PPob' (1282, "Alert") {
	{	/* array TagArray: 7 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1282,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				-1
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeAlert,
				inactiveAlert,
				activeAlert,
				inactiveAlert
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				331,
				120,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			IconControl {
				0,
				{32, 32},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				8,
				8,
				0,
				defaultSuperView,
				0,
				0,
				cicn,
				noTracking,
				center
			}
		},
		/* [6] */
		ObjectData {
			StaticText {
				1299408672,
				{333, 94},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				58,
				9,
				0,
				defaultSuperView,
				134,
				""
			}
		},
		/* [7] */
		EndSubs {

		}
	}
};

resource 'PPob' (1283, "Confirm") {
	{	/* array TagArray: 8 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1283,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				1131307884
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				331,
				94,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			IconControl {
				0,
				{32, 32},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				8,
				8,
				0,
				defaultSuperView,
				0,
				1,
				cicn,
				noTracking,
				center
			}
		},
		/* [6] */
		ObjectData {
			StaticText {
				1299408672,
				{335, 66},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				57,
				8,
				0,
				defaultSuperView,
				134,
				""
			}
		},
		/* [7] */
		ObjectData {
			PushButton {
				1131307884,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				251,
				94,
				0,
				defaultSuperView,
				901,
				0,
				textOnly,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [8] */
		EndSubs {

		}
	}
};

resource 'PPob' (1284, "ConfirmCheck") {
	{	/* array TagArray: 9 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1284,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				1131307884
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				331,
				108,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			IconControl {
				0,
				{32, 32},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				8,
				8,
				0,
				defaultSuperView,
				0,
				1,
				cicn,
				noTracking,
				center
			}
		},
		/* [6] */
		ObjectData {
			PushButton {
				1131307884,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				253,
				108,
				0,
				defaultSuperView,
				901,
				0,
				textOnly,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [7] */
		ObjectData {
			StaticText {
				1299408672,
				{331, 66},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				59,
				8,
				0,
				defaultSuperView,
				134,
				""
			}
		},
		/* [8] */
		ObjectData {
			CheckBox {
				1130914667,
				{330, 24},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				56,
				80,
				0,
				defaultSuperView,
				0,
				unchecked,
				134,
				""
			}
		},
		/* [9] */
		EndSubs {

		}
	}
};

resource 'PPob' (1285, "Prompt") {
	{	/* array TagArray: 10 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1285,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				1131307884
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				331,
				125,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			IconControl {
				0,
				{32, 32},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				8,
				8,
				0,
				defaultSuperView,
				0,
				1,
				cicn,
				noTracking,
				center
			}
		},
		/* [6] */
		ObjectData {
			StaticText {
				1299408672,
				{333, 55},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				54,
				8,
				0,
				defaultSuperView,
				134,
				""
			}
		},
		/* [7] */
		ObjectData {
			PushButton {
				1131307884,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				254,
				125,
				0,
				defaultSuperView,
				901,
				0,
				textOnly,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [8] */
		ObjectData {
			EditText {
				1383296116,
				{334, 23},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				54,
				72,
				0,
				defaultSuperView,
				0,
				regular,
				134,
				"",
				1024,
				noBox,
				hasWordWrap,
				hasAutoScroll,
				noTextBuffering,
				noOutlineHilite,
				noInlineInput,
				noTextServices,
				printingCharFilter
			}
		},
		/* [9] */
		ObjectData {
			CheckBox {
				1130914667,
				{333, 18},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				52,
				99,
				0,
				defaultSuperView,
				0,
				unchecked,
				134,
				"Remember Value"
			}
		},
		/* [10] */
		EndSubs {

		}
	}
};

resource 'PPob' (1286, "PromptNameAndPass") {
	{	/* array TagArray: 16 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1286,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				1131307884
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				328,
				154,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			IconControl {
				0,
				{32, 32},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				8,
				8,
				0,
				defaultSuperView,
				0,
				1,
				cicn,
				noTracking,
				center
			}
		},
		/* [6] */
		ObjectData {
			PushButton {
				1131307884,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				251,
				154,
				0,
				defaultSuperView,
				901,
				0,
				textOnly,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [7] */
		ObjectData {
			StaticText {
				0,
				{50, 16},
				visible,
				enabled,
				bound,
				unbound,
				unbound,
				bound,
				9,
				69,
				0,
				defaultSuperView,
				135,
				"Name:"
			}
		},
		/* [8] */
		ObjectData {
			StaticText {
				0,
				{60, 16},
				visible,
				enabled,
				bound,
				unbound,
				unbound,
				bound,
				-1,
				100,
				0,
				defaultSuperView,
				135,
				"Password:"
			}
		},
		/* [9] */
		ObjectData {
			StaticText {
				1299408672,
				{340, 48},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				53,
				8,
				0,
				defaultSuperView,
				134,
				""
			}
		},
		/* [10] */
		ObjectData {
			TabGroupView {
				0,
				{334, 57},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				59,
				65,
				0,
				defaultSuperView,
				0,
				0,
				0,
				0,
				1,
				1,
				noReconcileOverhang
			}
		},
		/* [11] */
		BeginSubs {

		},
		/* [12] */
		ObjectData {
			EditText {
				1315007845,
				{329, 23},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				2,
				2,
				0,
				defaultSuperView,
				0,
				regular,
				134,
				"",
				255,
				noBox,
				noWordWrap,
				hasAutoScroll,
				noTextBuffering,
				noOutlineHilite,
				noInlineInput,
				noTextServices,
				printingCharFilter
			}
		},
		/* [13] */
		ObjectData {
			EditText {
				1348563827,
				{329, 23},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				2,
				32,
				0,
				defaultSuperView,
				0,
				password,
				134,
				"",
				255,
				noBox,
				noWordWrap,
				hasAutoScroll,
				noTextBuffering,
				noOutlineHilite,
				noInlineInput,
				noTextServices,
				printingCharFilter
			}
		},
		/* [14] */
		EndSubs {

		},
		/* [15] */
		ObjectData {
			CheckBox {
				1130914667,
				{328, 18},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				59,
				126,
				0,
				defaultSuperView,
				0,
				unchecked,
				134,
				"Remember Name And Password"
			}
		},
		/* [16] */
		EndSubs {

		}
	}
};

resource 'PPob' (1287, "PromptPassword") {
	{	/* array TagArray: 10 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1287,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				1131307884
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				331,
				120,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			IconControl {
				0,
				{32, 32},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				8,
				8,
				0,
				defaultSuperView,
				0,
				1,
				cicn,
				noTracking,
				center
			}
		},
		/* [6] */
		ObjectData {
			PushButton {
				1131307884,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				254,
				120,
				0,
				defaultSuperView,
				901,
				0,
				textOnly,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [7] */
		ObjectData {
			EditText {
				1348563827,
				{337, 23},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				52,
				67,
				0,
				defaultSuperView,
				0,
				password,
				134,
				"",
				255,
				noBox,
				noWordWrap,
				hasAutoScroll,
				noTextBuffering,
				noOutlineHilite,
				noInlineInput,
				noTextServices,
				printingCharFilter
			}
		},
		/* [8] */
		ObjectData {
			StaticText {
				1299408672,
				{338, 48},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				53,
				7,
				0,
				defaultSuperView,
				134,
				""
			}
		},
		/* [9] */
		ObjectData {
			CheckBox {
				1130914667,
				{334, 18},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				50,
				93,
				0,
				defaultSuperView,
				0,
				unchecked,
				134,
				"Remember Password"
			}
		},
		/* [10] */
		EndSubs {

		}
	}
};

resource 'PPob' (1290, "PrintProgress") {
	{	/* array TagArray: 7 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1290,
				regular,
				noCloseBox,
				noTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				-1,
				1131307884
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			ProgressBar {
				1349676903,
				{360, 14},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				unbound,
				20,
				41,
				0,
				defaultSuperView,
				0,
				0,
				0,
				0,
				indeterminate
			}
		},
		/* [5] */
		ObjectData {
			PushButton {
				1131307884,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				315,
				63,
				0,
				defaultSuperView,
				901,
				0,
				0,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [6] */
		ObjectData {
			StaticText {
				1400136052,
				{360, 32},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				unbound,
				20,
				4,
				0,
				defaultSuperView,
				128,
				"Printing: ^0"
			}
		},
		/* [7] */
		EndSubs {

		}
	}
};

data 'PPob' (1300, "Manage Profiles") {
	$"0002 646F 706C 646C 6F67 6F62 6A64 0000"            /* ..dopldlogobjd.. */
	$"0024 6764 6C67 0514 0000 0862 0000 0040"            /* .$gdlg.....b...@ */
	$"0040 FFFF FFFF FFFF FFFF 0000 0000 4F4B"            /* .@ˇˇˇˇˇˇˇˇ....OK */
	$"2020 436E 636C 6265 6773 6F62 6A64 0000"            /*   Cnclbegsobjd.. */
	$"0012 7774 6861 FFFF FFFE 0101 0001 0002"            /* ..wthaˇˇˇ˛...... */
	$"0001 0002 6F62 6A64 0000 003A 7075 7368"            /* ....objd...:push */
	$"4F4B 2020 003A 0014 0101 0000 0000 0000"            /* OK  .:.......... */
	$"00E2 0000 00A8 0000 0000 FFFF FFFF 0000"            /* .‚...®....ˇˇˇˇ.. */
	$"0384 0000 0000 0000 0000 0000 0000 0170"            /* .Ñ.............p */
	$"0000 024F 4B01 6F62 6A64 0000 003E 7075"            /* ...OK.objd...>pu */
	$"7368 436E 636C 0040 0014 0101 0000 0000"            /* shCncl.@........ */
	$"0000 0091 0000 00A8 0000 0000 FFFF FFFF"            /* ...ë...®....ˇˇˇˇ */
	$"0000 0385 0000 0000 0000 0000 0000 0000"            /* ...Ö............ */
	$"0170 0000 0643 616E 6365 6C00 6F62 6A64"            /* .p...Cancel.objd */
	$"0000 003F 7075 7368 526E 616D 0054 0014"            /* ...?pushRnam.T.. */
	$"0101 0000 0000 0000 000B 0000 001B 0000"            /* ................ */
	$"0000 FFFF FFFF 0000 07D2 0000 0000 0000"            /* ..ˇˇˇˇ...“...... */
	$"0000 0000 0000 0170 0000 0752 656E 616D"            /* .......p...Renam */
	$"65C9 006F 626A 6400 0000 3E70 7573 6844"            /* e….objd...>pushD */
	$"656C 6500 5400 1401 0100 0000 0000 0000"            /* ele.T........... */
	$"0A00 0000 4000 0000 00FF FFFF FF00 0007"            /* ¬...@....ˇˇˇˇ... */
	$"D100 0000 0000 0000 0000 0000 0001 7000"            /* —.............p. */
	$"0006 4465 6C65 7465 006F 626A 6400 0000"            /* ..Delete.objd... */
	$"3C70 7573 684E 6577 5000 5400 1401 0100"            /* <pushNewP.T..... */
	$"0000 0000 0000 0B00 0000 6600 0000 00FF"            /* ..........f....ˇ */
	$"FFFF FF00 0007 D000 0000 0000 0000 0000"            /* ˇˇˇ...–......... */
	$"0000 0001 7000 0004 4E65 77C9 006F 626A"            /* ....p...New….obj */
	$"6400 0000 4C67 6662 6400 0000 0000 B900"            /* d...Lgfbd.....π. */
	$"7D01 0100 0000 0000 0000 6800 0000 0C00"            /* }.........h..... */
	$"0000 00FF FFFF FF00 0000 0000 0000 0000"            /* ...ˇˇˇˇ......... */
	$"0000 0000 0000 0000 0000 0100 0000 0100"            /* ................ */
	$"0046 7261 6D4C 6973 7401 0100 0000 0F01"            /* .FramList....... */
	$"0162 6567 736F 626A 6400 0000 3C76 6965"            /* .begsobjd...<vie */
	$"7746 7261 6D00 B100 7501 0101 0101 0100"            /* wFram.±.u....... */
	$"0000 0400 0000 0400 0000 00FF FFFF FF00"            /* ...........ˇˇˇˇ. */
	$"0000 0000 0000 0000 0000 0000 0000 0000"            /* ................ */
	$"0000 0100 0000 0100 0062 6567 736F 626A"            /* .........begsobj */
	$"6400 0000 3C76 6965 7700 0000 0000 B300"            /* d...<view.....≥. */
	$"6701 0101 0101 01FF FFFF FF00 0000 0F00"            /* g......ˇˇˇˇ..... */
	$"0000 00FF FFFF FF00 0000 0000 0000 0000"            /* ...ˇˇˇˇ......... */
	$"0000 0000 0000 0000 0000 0100 0000 0100"            /* ................ */
	$"0062 6567 736F 626A 6400 0000 4873 6372"            /* .begsobjd...Hscr */
	$"6C53 6372 6C00 B300 6701 0101 0101 0100"            /* lScrl.≥.g....... */
	$"0000 0000 0000 0000 0000 00FF FFFF FF00"            /* ...........ˇˇˇˇ. */
	$"0000 0100 0000 0100 0000 0000 0000 0000"            /* ................ */
	$"0000 0000 0000 0000 00FF FF00 0F00 0000"            /* .........ˇˇ..... */
	$"004C 6973 7462 6567 736F 626A 6400 0000"            /* .Listbegsobjd... */
	$"1663 6572 73FF FFFF FE01 0100 0000 0000"            /* .cersˇˇˇ˛....... */
	$"00FF FFFF FFFF FF6F 626A 6400 0000 4A54"            /* .ˇˇˇˇˇˇobjd...JT */
	$"7461 624C 6973 7400 A400 6701 0101 0101"            /* tabList.§.g..... */
	$"0100 0000 0000 0000 0000 0000 00FF FFFF"            /* .............ˇˇˇ */
	$"FF00 0000 0000 0000 0000 0000 0000 0000"            /* ˇ............... */
	$"0000 0000 0100 0000 0100 0100 0000 0000"            /* ................ */
	$"0000 FF00 8100 0000 0065 6E64 7365 6E64"            /* ..ˇ.Å....endsend */
	$"736F 626A 6400 0000 5170 6C63 6400 0000"            /* sobjd...Qplcd... */
	$"0000 B300 1101 0101 0101 00FF FFFF FFFF"            /* ..≥........ˇˇˇˇˇ */
	$"FFFF FF00 0000 00FF FFFF FF00 0000 0000"            /* ˇˇˇ....ˇˇˇˇ..... */
	$"0000 0000 0000 0000 0000 0000 0000 0100"            /* ................ */
	$"0000 0100 0000 0000 0000 0000 0000 0000"            /* ................ */
	$"0000 0000 0000 E000 0000 6265 6773 6F62"            /* ......‡...begsob */
	$"6A64 0000 0049 7374 7874 0000 0000 00A4"            /* jd...Istxt.....§ */
	$"000D 0101 0101 0101 0000 0002 0000 0002"            /* ................ */
	$"0000 0000 FFFF FFFF 0000 0000 0000 0000"            /* ....ˇˇˇˇ........ */
	$"0000 0000 0000 0000 0120 0085 1241 7661"            /* ......... .Ö.Ava */
	$"696C 6162 6C65 2050 726F 6669 6C65 7365"            /* ilable Profilese */
	$"6E64 7365 6E64 7365 6E64 736F 626A 6400"            /* ndsendsendsobjd. */
	$"0000 4563 6862 7853 686F 7700 7900 1201"            /* ..EchbxShow.y... */
	$"0100 0000 0000 0000 0B00 0000 A800 0000"            /* ............®... */
	$"00FF FFFF FF00 0000 0000 0000 0000 0000"            /* .ˇˇˇˇ........... */
	$"0000 0000 0201 7100 860E 4173 6B20 6174"            /* ......q.Ü.Ask at */
	$"2053 7461 7274 7570 656E 6473 656E 642E"            /*  Startupendsend. */
};

resource 'PPob' (1301, "New Profile") {
	{	/* array TagArray: 8 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1301,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				1131307884
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				unbound,
				259,
				76,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			PushButton {
				1131307884,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				177,
				76,
				0,
				defaultSuperView,
				901,
				0,
				textOnly,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [6] */
		ObjectData {
			EditText {
				1315007845,
				{259, 24},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				60,
				18,
				0,
				defaultSuperView,
				0,
				regular,
				useSystemFont,
				"",
				255,
				noBox,
				hasWordWrap,
				hasAutoScroll,
				noTextBuffering,
				noOutlineHilite,
				noInlineInput,
				noTextServices,
				printingCharFilter
			}
		},
		/* [7] */
		ObjectData {
			StaticText {
				0,
				{46, 16},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				10,
				22,
				0,
				defaultSuperView,
				useSystemFont,
				"Name:"
			}
		},
		/* [8] */
		EndSubs {

		}
	}
};

resource 'PPob' (1288, "ConfirmEx") {
	{	/* array TagArray: 10 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1288,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1114926640,
				1114926641
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeDialog,
				inactiveDialog,
				activeDialog,
				inactiveDialog
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1114926640,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				331,
				108,
				0,
				defaultSuperView,
				1114926640,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			IconControl {
				0,
				{32, 32},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				8,
				8,
				0,
				defaultSuperView,
				0,
				1,
				cicn,
				noTracking,
				center
			}
		},
		/* [6] */
		ObjectData {
			PushButton {
				1114926641,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				253,
				108,
				0,
				defaultSuperView,
				1114926641,
				0,
				textOnly,
				useSystemFont,
				"Cancel",
				notDefault
			}
		},
		/* [7] */
		ObjectData {
			StaticText {
				1299408672,
				{331, 62},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				59,
				8,
				0,
				defaultSuperView,
				134,
				""
			}
		},
		/* [8] */
		ObjectData {
			CheckBox {
				1130914667,
				{330, 24},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				56,
				76,
				0,
				defaultSuperView,
				0,
				unchecked,
				134,
				""
			}
		},
		/* [9] */
		ObjectData {
			PushButton {
				1114926642,
				{64, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				174,
				108,
				0,
				defaultSuperView,
				1114926642,
				0,
				textOnly,
				useSystemFont,
				"",
				notDefault
			}
		},
		/* [10] */
		EndSubs {

		}
	}
};

resource 'PPob' (1305, "DownloadProgressWindow") {
	{	/* array TagArray: 5 elements */
		/* [1] */
		ClassAlias {
			'MDPW'
		},
		/* [2] */
		ObjectData {
			Window {
				1305,
				regular,
				hasCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				noTarget,
				hasGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				-1,
				-1,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0
			}
		},
		/* [3] */
		BeginSubs {

		},
		/* [4] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeModelessDialog,
				inactiveModelessDialog,
				activeModelessDialog,
				inactiveModelessDialog
			}
		},
		/* [5] */
		EndSubs {

		}
	}
};

data 'PPob' (1306, "DownloadProgressItemView") {
	$"0002 646F 706C 4450 7256 6F62 6A64 0000"            /* ..doplDPrVobjd.. */
	$"003C 7669 6577 0000 0000 0208 0082 0101"            /* .<view.......Ç.. */
	$"0000 0000 0000 0000 0000 0000 0000 0000"            /* ................ */
	$"FFFF FFFF 0000 0018 0000 002C 0000 0000"            /* ˇˇˇˇ.......,.... */
	$"0000 0000 0000 0010 0000 0010 0000 6265"            /* ..............be */
	$"6773 6F62 6A64 0000 0051 7769 6E68 0000"            /* gsobjd...Qwinh.. */
	$"0000 0208 0010 0101 0101 0100 0000 0000"            /* ................ */
	$"0000 0000 0000 0000 FFFF FFFF 0000 0000"            /* ........ˇˇˇˇ.... */
	$"0000 0000 0000 0000 0000 0000 0000 0001"            /* ................ */
	$"0000 0001 0000 0000 0000 0000 0000 0000"            /* ................ */
	$"0000 0000 0000 0150 0000 0062 6567 736F"            /* .......P...begso */
	$"626A 6400 0000 3843 4953 4343 6C6F 7300"            /* bjd...8CISCClos. */
	$"0E00 0E01 0000 0000 0000 0000 0900 0000"            /* ............∆... */
	$"0100 0000 00FF FFFF FF43 6C6F 7300 0000"            /* .....ˇˇˇˇClos... */
	$"0000 0000 0000 0000 0043 6C50 6E05 2B65"            /* .........ClPn.+e */
	$"6E64 736F 626A 6400 0000 3870 6261 7250"            /* ndsobjd...8pbarP */
	$"726F 6700 D600 0E01 0100 0000 0000 0000"            /* rog.÷........... */
	$"0C00 0000 1E00 0000 00FF FFFF FF00 0000"            /* .........ˇˇˇˇ... */
	$"0000 0000 0000 0000 0000 0000 6400 5000"            /* ............d.P. */
	$"0000 016F 626A 6400 0000 3E73 7478 7453"            /* ...objd...>stxtS */
	$"554C 6100 3200 0F01 0100 0000 0000 0000"            /* ULa.2........... */
	$"0800 0000 5500 0000 00FF FFFF FF00 0000"            /* ....U....ˇˇˇˇ... */
	$"0000 0000 0000 0000 0000 0000 0001 2000"            /* .............. . */
	$"8807 536F 7572 6365 3A6F 626A 6400 0000"            /* à.Source:objd... */
	$"4173 7478 7444 464C 6100 3E00 0F01 0100"            /* AstxtDFLa.>..... */
	$"0000 00FF FFFF FC00 0000 6C00 0000 00FF"            /* ...ˇˇˇ¸...l....ˇ */
	$"FFFF FF00 0000 0000 0000 0000 0000 0000"            /* ˇˇˇ............. */
	$"0000 0001 2000 880A 5361 7669 6E67 2054"            /* .... .à¬Saving T */
	$"6F3A 6F62 6A64 0000 0037 7374 7874 5355"            /* o:objd...7stxtSU */
	$"5249 01C0 000E 0101 0000 0100 0000 003D"            /* RI.¿...........= */
	$"0000 0055 0000 0000 FFFF FFFF 0000 0000"            /* ...U....ˇˇˇˇ.... */
	$"0000 0000 0000 0000 0000 0000 0120 0086"            /* ............. .Ü */
	$"006F 626A 6400 0000 3773 7478 7444 6573"            /* .objd...7stxtDes */
	$"7401 C000 0E01 0100 0001 0000 0000 3D00"            /* t.¿...........=. */
	$"0000 6C00 0000 00FF FFFF FF00 0000 0000"            /* ..l....ˇˇˇˇ..... */
	$"0000 0000 0000 0000 0000 0001 2000 8600"            /* ............ .Ü. */
	$"6F62 6A64 0000 003E 7075 7368 436E 636C"            /* objd...>pushCncl */
	$"0044 0014 0101 0000 0000 0000 00F3 0000"            /* .D...........Û.. */
	$"001A 0000 0000 FFFF FFFF 436E 636C 0000"            /* ......ˇˇˇˇCncl.. */
	$"0000 0000 0000 0000 0000 0170 0000 0643"            /* ...........p...C */
	$"616E 6365 6C00 6F62 6A64 0000 003C 7075"            /* ancel.objd...<pu */
	$"7368 4F70 656E 0044 0014 0100 0000 0000"            /* shOpen.D........ */
	$"0000 0148 0000 001A 0000 0000 FFFF FFFF"            /* ...H........ˇˇˇˇ */
	$"4F70 656E 0000 0000 0000 0000 0000 0000"            /* Open............ */
	$"0170 0000 044F 7065 6E00 6F62 6A64 0000"            /* .p...Open.objd.. */
	$"003E 7075 7368 5276 656C 0044 0014 0100"            /* .>pushRvel.D.... */
	$"0000 0000 0000 019A 0000 001A 0000 0000"            /* .......ö........ */
	$"FFFF FFFF 5276 656C 0000 0000 0000 0000"            /* ˇˇˇˇRvel........ */
	$"0000 0000 0170 0000 0652 6576 6561 6C00"            /* .....p...Reveal. */
	$"6F62 6A64 0000 003E 7374 7874 5374 4C61"            /* objd...>stxtStLa */
	$"002C 000F 0101 0000 0000 0000 000E 0000"            /* .,.............. */
	$"003E 0000 0000 FFFF FFFF 0000 0000 0000"            /* .>....ˇˇˇˇ...... */
	$"0000 0000 0000 0000 0000 0120 0088 0753"            /* ........... .à.S */
	$"7461 7475 733A 6F62 6A64 0000 0037 7374"            /* tatus:objd...7st */
	$"7874 5374 6174 00DC 000E 0101 0000 0100"            /* xtStat.‹........ */
	$"0000 003D 0000 003E 0000 0000 FFFF FFFF"            /* ...=...>....ˇˇˇˇ */
	$"0000 0000 0000 0000 0000 0000 0000 0000"            /* ................ */
	$"0120 0086 006F 626A 6400 0000 4673 7478"            /* . .Ü.objd...Fstx */
	$"7454 694C 6100 5E00 0F01 0100 0000 0000"            /* tTiLa.^......... */
	$"0001 1B00 0000 3E00 0000 00FF FFFF FF00"            /* ......>....ˇˇˇˇ. */
	$"0000 0000 0000 0000 0000 0000 0000 0001"            /* ................ */
	$"2000 880F 5469 6D65 2052 656D 6169 6E69"            /*  .à.Time Remaini */
	$"6E67 3A6F 626A 6400 0000 3773 7478 7454"            /* ng:objd...7stxtT */
	$"696D 6500 8200 0E01 0100 0001 0000 0001"            /* ime.Ç........... */
	$"7B00 0000 3E00 0000 00FF FFFF FF00 0000"            /* {...>....ˇˇˇˇ... */
	$"0000 0000 0000 0000 0000 0000 0001 2000"            /* .............. . */
	$"8600 6F62 6A64 0000 0037 7365 706C 0000"            /* Ü.objd...7sepl.. */
	$"0000 0208 0003 0101 0100 0101 0000 0000"            /* ................ */
	$"0000 007F 0000 0000 FFFF FFFF 0000 0000"            /* ........ˇˇˇˇ.... */
	$"0000 0000 0000 0000 0000 0000 0090 0000"            /* .............ê.. */
	$"006F 626A 6400 0000 3773 6570 6C00 0000"            /* .objd...7sepl... */
	$"0001 F400 0301 0101 0001 0000 0000 0A00"            /* ..Ù...........¬. */
	$"0000 3500 0000 00FF FFFF FF00 0000 0000"            /* ..5....ˇˇˇˇ..... */
	$"0000 0000 0000 0000 0000 0000 9000 0000"            /* ............ê... */
	$"656E 6473 656E 642E"                                /* endsend. */
};

resource 'PPob' (1289, "AlertCheck") {
	{	/* array TagArray: 8 elements */
		/* [1] */
		ObjectData {
			DialogBox {
				1289,
				modal,
				noCloseBox,
				hasTitleBar,
				noResize,
				noSizeBox,
				noZoom,
				noShowNew,
				enabled,
				hasTarget,
				noGetSelectClick,
				noHideOnSuspend,
				noDelaySelect,
				hasEraseOnUpdate,
				64,
				64,
				screenSize,
				screenSize,
				screenSize,
				screenSize,
				0,
				1330323488,
				-1
			}
		},
		/* [2] */
		BeginSubs {

		},
		/* [3] */
		ObjectData {
			WindowThemeAttachment {
				any,
				execute,
				hasOwner,
				activeAlert,
				inactiveAlert,
				activeAlert,
				inactiveAlert
			}
		},
		/* [4] */
		ObjectData {
			PushButton {
				1330323488,
				{58, 20},
				visible,
				enabled,
				unbound,
				unbound,
				bound,
				bound,
				331,
				120,
				0,
				defaultSuperView,
				900,
				0,
				textOnly,
				useSystemFont,
				"OK",
				isDefault
			}
		},
		/* [5] */
		ObjectData {
			IconControl {
				0,
				{32, 32},
				visible,
				enabled,
				unbound,
				unbound,
				unbound,
				unbound,
				8,
				8,
				0,
				defaultSuperView,
				0,
				0,
				cicn,
				noTracking,
				center
			}
		},
		/* [6] */
		ObjectData {
			StaticText {
				1299408672,
				{333, 72},
				visible,
				enabled,
				bound,
				bound,
				bound,
				bound,
				58,
				9,
				0,
				defaultSuperView,
				134,
				""
			}
		},
		/* [7] */
		ObjectData {
			CheckBox {
				1130914667,
				{333, 24},
				visible,
				enabled,
				bound,
				unbound,
				bound,
				bound,
				57,
				87,
				0,
				defaultSuperView,
				0,
				unchecked,
				134,
				""
			}
		},
		/* [8] */
		EndSubs {

		}
	}
};

resource 'RidL' (130) {
	{	/* array ResourceIDList: 3 elements */
		/* [1] */
		1400136052,
		/* [2] */
		1349676903,
		/* [3] */
		1282368363
	}
};

resource 'RidL' (131) {
	{	/* array ResourceIDList: 7 elements */
		/* [1] */
		1464421960,
		/* [2] */
		1113678699,
		/* [3] */
		1181708919,
		/* [4] */
		1380740961,
		/* [5] */
		1400139632,
		/* [6] */
		1733653100,
		/* [7] */
		1414025794
	}
};

resource 'RidL' (1281, "Find Dialog") {
	{	/* array ResourceIDList: 7 elements */
		/* [1] */
		1415936116,
		/* [2] */
		1130460005,
		/* [3] */
		1330323488,
		/* [4] */
		1131307884,
		/* [5] */
		1467113840,
		/* [6] */
		1113678699,
		/* [7] */
		1164866674
	}
};

resource 'RidL' (1282, "Alert") {
	{	/* array ResourceIDList: 2 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1299408672
	}
};

resource 'RidL' (1283, "Confirm") {
	{	/* array ResourceIDList: 3 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1299408672,
		/* [3] */
		1131307884
	}
};

resource 'RidL' (1284, "ConfirmCheck") {
	{	/* array ResourceIDList: 4 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1131307884,
		/* [3] */
		1299408672,
		/* [4] */
		1130914667
	}
};

resource 'RidL' (1285, "Prompt") {
	{	/* array ResourceIDList: 5 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1299408672,
		/* [3] */
		1131307884,
		/* [4] */
		1383296116,
		/* [5] */
		1130914667
	}
};

resource 'RidL' (1286, "PromptNameAndPass") {
	{	/* array ResourceIDList: 6 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1131307884,
		/* [3] */
		1299408672,
		/* [4] */
		1315007845,
		/* [5] */
		1348563827,
		/* [6] */
		1130914667
	}
};

resource 'RidL' (1287, "PromptPassword") {
	{	/* array ResourceIDList: 5 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1131307884,
		/* [3] */
		1348563827,
		/* [4] */
		1299408672,
		/* [5] */
		1130914667
	}
};

resource 'RidL' (1290, "PrintProgress") {
	{	/* array ResourceIDList: 3 elements */
		/* [1] */
		1349676903,
		/* [2] */
		1131307884,
		/* [3] */
		1400136052
	}
};

resource 'RidL' (1300, "Manage Profiles") {
	{	/* array ResourceIDList: 6 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1131307884,
		/* [3] */
		1382965613,
		/* [4] */
		1147497573,
		/* [5] */
		1315272528,
		/* [6] */
		1399353207
	}
};

resource 'RidL' (1301, "New Profile") {
	{	/* array ResourceIDList: 3 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1131307884,
		/* [3] */
		1315007845
	}
};

resource 'RidL' (1288, "ConfirmEx") {
	{	/* array ResourceIDList: 5 elements */
		/* [1] */
		1114926640,
		/* [2] */
		1114926641,
		/* [3] */
		1299408672,
		/* [4] */
		1130914667,
		/* [5] */
		1114926642
	}
};

resource 'RidL' (1305, "DownloadProgressWindow") {
	{	/* array ResourceIDList: 0 elements */
	}
};

resource 'RidL' (1306, "DownloadProgressItemView") {
	{	/* array ResourceIDList: 13 elements */
		/* [1] */
		1131179891,
		/* [2] */
		1349676903,
		/* [3] */
		1398099041,
		/* [4] */
		1145457761,
		/* [5] */
		1398100553,
		/* [6] */
		1147499380,
		/* [7] */
		1131307884,
		/* [8] */
		1332766062,
		/* [9] */
		1383490924,
		/* [10] */
		1400130657,
		/* [11] */
		1400136052,
		/* [12] */
		1416186977,
		/* [13] */
		1416195429
	}
};

resource 'RidL' (1289, "AlertCheck") {
	{	/* array ResourceIDList: 3 elements */
		/* [1] */
		1330323488,
		/* [2] */
		1299408672,
		/* [3] */
		1130914667
	}
};

resource 'STR#' (200, "Standards", purgeable) {
	{	/* array StringArray: 2 elements */
		/* [1] */
		"PPEmbed",
		/* [2] */
		"Save File As:"
	}
};

resource 'Txtr' (128, "System Font") {
	defaultSize,
	0,
	flushDefault,
	srcOr,
	0,
	0,
	0,
	0,
	""
};

resource 'Txtr' (129, "App Font") {
	defaultSize,
	0,
	flushDefault,
	srcOr,
	0,
	0,
	0,
	1,
	""
};

resource 'Txtr' (130, "Geneva 9") {
	9,
	0,
	flushDefault,
	srcOr,
	0,
	0,
	0,
	useName,
	"Geneva"
};

resource 'Txtr' (131, "System Font Centered") {
	defaultSize,
	0,
	center,
	srcOr,
	0,
	0,
	0,
	0,
	""
};

resource 'Txtr' (132, "SysFontRightJust") {
	defaultSize,
	0,
	flushRight,
	srcCopy,
	0,
	0,
	0,
	0,
	""
};

resource 'Txtr' (133, "Appl Font, 9, Bold, Center") {
	9,
	1,
	center,
	srcCopy,
	0,
	0,
	0,
	1,
	""
};

resource 'Txtr' (134, "Appl Font, 10") {
	10,
	0,
	flushDefault,
	srcCopy,
	0,
	0,
	0,
	1,
	""
};

resource 'Txtr' (135, "Appl Font, 10, Right") {
	10,
	0,
	flushRight,
	srcCopy,
	0,
	0,
	0,
	1,
	""
};

resource 'Txtr' (136, "Appl Font, 10, Right, Bold") {
	11,
	1,
	flushRight,
	srcCopy,
	0,
	0,
	0,
	1,
	""
};

resource 'WIND' (1281, "Find Dialog") {
	{40, 16, 208, 347},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Find",
	centerParentWindow
};

resource 'WIND' (1282, "Alert") {
	{40, 16, 195, 416},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Alert",
	alertPositionMainScreen
};

resource 'WIND' (1283, "Confirm") {
	{40, 16, 169, 416},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Confirm",
	alertPositionMainScreen
};

resource 'WIND' (1284, "ConfirmCheck") {
	{40, 16, 183, 416},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Confirm",
	alertPositionMainScreen
};

resource 'WIND' (1285, "Prompt") {
	{40, 16, 200, 416},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Prompt",
	alertPositionMainScreen
};

resource 'WIND' (1286, "PromptNameAndPass") {
	{40, 16, 225, 416},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Name And Password",
	alertPositionMainScreen
};

resource 'WIND' (1287, "PromptPassword") {
	{40, 16, 195, 416},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Password",
	alertPositionMainScreen
};

resource 'WIND' (1290, "PrintProgress") {
	{40, 16, 135, 416},
	dBoxProc,
	invisible,
	noGoAway,
	0x0,
	"",
	alertPositionMainScreen
};

resource 'WIND' (1300, "Manage Profiles") {
	{40, 16, 240, 316},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Manage Profiles",
	centerMainScreen
};

resource 'WIND' (1301, "New Profile") {
	{40, 16, 154, 350},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"New Profile",
	centerParentWindow
};

resource 'WIND' (1288, "ConfirmEx") {
	{40, 16, 183, 416},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Confirm",
	alertPositionMainScreen
};

resource 'WIND' (1305, "DownloadProgressWindow") {
	{42, 4, 157, 524},
	noGrowDocProc,
	invisible,
	goAway,
	0x0,
	"Download Progress",
	alertPositionMainScreen
};

resource 'WIND' (1289, "AlertCheck") {
	{40, 16, 195, 416},
	movableDBoxProc,
	invisible,
	noGoAway,
	0x0,
	"Alert",
	alertPositionMainScreen
};

resource 'icm#' (1320, "lock-insecure") {
	{	/* array: 2 elements */
		/* [1] */
		$"",
		/* [2] */
		$"007C 00FE 01FF 01C7 01C7 FFE7 FFE0 FFE0"
		$"FFE0 FFE0 FFE0 FFE0"
	}
};

resource 'icm#' (1321, "lock-secure") {
	{	/* array: 2 elements */
		/* [1] */
		$"",
		/* [2] */
		$"03E0 07F0 0FF8 0E38 0E38 1FFC 1FFC 1FFC"
		$"1FFC 1FFC 1FFC 1FFC"
	}
};

resource 'icm#' (1322, "lock-broken") {
	{	/* array: 2 elements */
		/* [1] */
		$"",
		/* [2] */
		$"0670 0E78 1E7C 1C1C 1C1C 3E7E 3E7E 3E7E"
		$"3E7E 3E7E 3E7E 3E7E"
	}
};

resource 'icm8' (1320, "lock-insecure") {
	$"0000 0000 0000 0000 00FC FDFE D681 0000"
	$"0000 0000 0000 0000 D0F8 F7F7 F7F7 FD00"
	$"0000 0000 0000 00FC F7F7 F4FD EAF7 F7A5"
	$"0000 0000 0000 00F3 F7F4 0000 00FE F7FE"
	$"0000 0000 0000 00FD F7D0 0000 00FE F7FD"
	$"FDF3 D0B2 FEFE FFFD F3FD D600 00D6 FDFE"
	$"F3F7 F7F8 F8F8 5656 F9F9 D000 0000 0000"
	$"D0FF FEFD FCFB 81F9 56F8 FE00 0000 0000"
	$"FDF7 F7F8 F8F8 5656 F9F9 FD00 0000 0000"
	$"F3FF FEFD FCFB 81F9 56F8 F300 0000 0000"
	$"FDF7 F7F8 F8F8 5656 F9F9 D100 0000 0000"
	$"D0FD FED6 FDD6 FDFE D0FE D6"
};

resource 'icm8' (1321, "lock-secure") {
	$"0000 0000 0000 ACFD FED6 8200 0000 0000"
	$"0000 0000 00D0 56F7 F7F7 56FD 0000 0000"
	$"0000 0000 88F7 F7F3 FEF3 F7F7 FC00 0000"
	$"0000 0000 D0F7 F300 0000 E0F7 FE00 0000"
	$"0000 0000 DFF7 FD00 0000 D6F7 FD00 0000"
	$"0000 00FD F3D0 B2FE FEFF FDF3 FDD6 0000"
	$"0000 00F3 F7F7 F8F8 F856 56F9 F9D0 0000"
	$"0000 00D0 FFFE FDFC FB81 F956 F8FE 0000"
	$"0000 00FD F7F7 F8F8 F856 56F9 F9FD 0000"
	$"0000 00F3 FFFE FDFC FB81 F956 F8F3 0000"
	$"0000 00FD F7F7 F8F8 F856 56F9 F9D1 0000"
	$"0000 00D0 FDFE D6FD D6FD FED0 FED6"
};

resource 'icm8' (1322, "lock-broken") {
	$"0000 0000 00AC FD00 00FE D682 0000 0000"
	$"0000 0000 D056 F700 00F7 F756 FD00 0000"
	$"0000 0088 F7F7 F300 00FE F3F7 F7FC 0000"
	$"0000 00D0 F7F3 0000 0000 00E0 F7FE 0000"
	$"0000 00DF F7FD 0000 0000 00D6 F7FD 0000"
	$"0000 FDF3 D0B2 FE00 00FE FFFD F3FD D600"
	$"0000 F3F7 F7F8 F800 00F8 5656 F9F9 D000"
	$"0000 D0FF FEFD FC00 00FB 81F9 56F8 FE00"
	$"0000 FDF7 F7F8 F800 00F8 5656 F9F9 FD00"
	$"0000 F3FF FEFD FC00 00FB 81F9 56F8 F300"
	$"0000 FDF7 F7F8 F800 00F8 5656 F9F9 D100"
	$"0000 D0FD FED6 FD00 00D6 FDFE D0FE D6"
};

resource 'icns' (1310, "back") {
	{	/* array elementArray: 3 elements */
		/* [1] */
		'ICN#',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 001F F800 007F FE00"
		$"01FF FF80 03FF FFC0 07FF FFE0 0FFF FFF0"
		$"0FFF FFF0 1FFF FFF8 1FFF FFF8 3FFF FFFC"
		$"3FFF FFFC 3FFF FFFC 3FFF FFFC 3FFF FFFC"
		$"3FFF FFFC 3FFF FFFC 3FFF FFFC 3FFF FFFC"
		$"3FFF FFFC 1FFF FFF8 1FFF FFF8 0FFF FFF0"
		$"0FFF FFF0 07FF FFE0 03FF FFC0 01FF FF80"
		$"007F FE00 001F F800 0000 0000 0000 0000",
		/* [2] */
		'il32',
		$"C7FF 0BF0 AF77 4928 1414 2745 6FA7 EE8F"
		$"FF0F EA93 444D 7DA1 BDCA C7B0 916C 423B"
		$"86E6 8CFF 11B7 5469 AEDE DCD7 D2CD C7C3"
		$"BEBD B588 5146 A98A FF13 9E4A A1E2 E0DB"
		$"D6D1 CCC4 BFBD B8B4 B3AB A673 398B 88FF"
		$"159E 61C3 E2DE DBD6 CFC7 C1BA B5B0 AFAD"
		$"AAA6 A19E 8244 8986 FF17 B74A C3E2 DEDB"
		$"D4CF C6BE B77E 8499 A6A5 A3A0 9C97 947A"
		$"34A3 84FF 19EA 54A1 E2DE DBD6 CEC4 BCB4"
		$"94FF FF7F 9B9E 9C9B 9794 9089 613E E283"
		$"FF0A 9269 E1DE D9D3 CDC4 BAB3 9680 FF0B"
		$"FA8B 9597 9694 8E89 8681 3C77 82FF 0AF0"
		$"44AC DED8 D3C9 C2B9 B095 82FF 0B90 9091"
		$"908D 8986 817D 5C2E EA81 FF09 AF4D DCD8"
		$"D2C8 BFB7 AA95 82FF 0C90 8989 8888 8683"
		$"7F7C 7773 2A95 81FF 0876 7DD8 D2C7 BDB3"
		$"A587 82FF 0D90 8883 827F 7E7D 7978 7572"
		$"6E3D 5781 FF07 499D D3C8 BDB3 A578 82FF"
		$"0E92 8E90 8E8E 867C 786F 6F6E 6B68 4B30"
		$"81FF 0728 B8CD C1B3 AA77 FA86 FF0A F8EF"
		$"E2CE 6D65 6566 6352 1981 FF06 14C4 C7BC"
		$"ADA4 9486 FF0B FDF6 ECE5 D869 5F5F 605E"
		$"560F 81FF 0714 C2C4 B5A9 9E7C F684 FF0C"
		$"FDFA F1E7 E0D3 6B59 5B5A 5951 0F81 FF1B"
		$"25AF BFB5 A99C 929F FAFD FFFF E099 7A77"
		$"7472 6E6E 635C 5656 5552 4718 81FF 0944"
		$"8FBA B5AA 9F92 869C F680 FA05 CD7A 6561"
		$"5E58 8056 0555 5451 4F39 2B81 FF1B 6F6A"
		$"B8B0 AA9F 9487 7FA5 EFF1 EFED C877 5A58"
		$"595A 5756 5552 4F4B 2A50 81FF 0AA7 41B2"
		$"ADA6 9F96 887D 79AB 80E5 0DE1 C169 5557"
		$"5A57 5551 4F4B 461C 8C81 FF1B EE3A 85A8"
		$"A39E 968C 7F72 73B0 DBD9 D8D3 5F55 5657"
		$"5652 4D4B 4634 25E7 82FF 1984 4EA3 9F99"
		$"948B 8175 6A61 ADCD BE8D 5C52 5655 524D"
		$"4A46 4121 6983 FF19 E545 709A 9490 8982"
		$"796D 6359 6063 4F50 524B 4F4D 4A45 422E"
		$"2EDD 84FF 17A8 387E 908C 8881 7A6F 655E"
		$"5652 524F 4C51 4D4A 4541 3721 9486 FF15"
		$"8B42 7887 827D 7872 6861 5A57 5651 4F4D"
		$"4A45 4136 2575 88FF 1388 335C 7D78 746F"
		$"6963 5C59 5551 4D48 4541 2E21 748A FF11"
		$"A23D 3A59 6E69 6560 5A56 514C 4846 3321"
		$"2E94 8CFF 0FE2 762E 293C 4750 544F 4337"
		$"2A1C 2568 DE8F FF0B EA94 5630 190F 0F18"
		$"2B50 8CE7 C7FF C7FF 0BF1 B27B 4D2C 1717"
		$"2A4A 74AA EF8F FF0F EB96 4854 83AA C6D4"
		$"D0BA 9974 4840 8AE7 8CFF 11BA 586E B6E8"
		$"E4E1 DDD8 D2CE CDC8 C493 584D AD8A FF13"
		$"A24F A8EC E8E4 E2DC D7D1 CCC9 C4C0 BFB9"
		$"B47F 4091 88FF 15A2 67CB ECE8 E6DF DAD3"
		$"CEC8 C3BF BDBA B6B4 B0AB 8F4C 8F86 FF17"
		$"BA4F CBEB E8E4 DFDA D3CA C492 97AA B5B4"
		$"B0AF ABA6 A288 3BA8 84FF 19EB 58A8 EBE8"
		$"E4DF D9D3 C9C3 A6FF FF93 AAAC ABAA A6A2"
		$"9D9A 6C44 E483 FF0A 966E EBE7 E3DF D9D1"
		$"C8C2 A781 FF0A 9DA6 A6A5 A2A0 9B96 9145"
		$"7E82 FF0A F148 B5E8 E3DE D6CE C7BF A782"
		$"FF0B A2A1 A1A0 9C9A 9692 8D6B 34EB 81FF"
		$"09B2 52E6 E2DC D4CE C4BA A782 FF0C A39D"
		$"9B9C 9A97 9591 8D8A 8533 9A81 FF08 7A82"
		$"E1DD D3CA C2B5 9B82 FF0D A39C 9796 9392"
		$"8F8D 8A88 857F 495E 81FF 074D A6DE D4CA"
		$"C0B4 8D82 FF0E A5A1 A3A1 A199 908B 8381"
		$"817F 7B58 3681 FF06 2CC1 D8CD C2B9 8B87"
		$"FF0A FDF6 EDDE 837A 7A79 7666 1E81 FF06"
		$"17D0 D3C8 BEB4 A687 FF0A FCF6 EEE4 7E75"
		$"7574 7168 1281 FF07 17CB CFC4 B9AF 8FFC"
		$"86FF 04F8 F2EB E181 8070 026C 6612 81FF"
		$"072A B7CC C3B6 ACA2 AF81 FF0F EBA9 8E8B"
		$"8986 8484 7772 6B6C 6B68 581D 81FF 0949"
		$"99C7 C0B9 AFA4 98AC FC80 FF0E DC8F 7A76"
		$"746F 6E6C 6C6B 6A67 6348 3381 FF09 7372"
		$"C4BE B8AE A49B 93B7 80F8 0EF6 D78B 716F"
		$"706F 6C6B 6A67 6360 3957 81FF 1BAA 47BE"
		$"BAB4 AEA5 9C90 8DBB F0F0 EEEC CF7E 6C6D"
		$"6F6D 6B67 6360 5C26 9281 FF1B EE3F 90B5"
		$"B0AB A49B 9288 88BF E7E6 E4DF 756B 6C6D"
		$"6B68 6360 5C46 2DE9 82FF 1989 57B0 ACA7"
		$"A29D 9489 7F77 BDDC CFA1 7268 6B6A 6763"
		$"5E5C 572E 7083 FF19 E74B 7CA9 A2A0 9A94"
		$"8A80 7970 7677 6667 6A5E 6363 5E5C 583E"
		$"36DF 84FF 17AC 3E8C A09B 9792 8A83 7B72"
		$"6D68 6862 5E65 635E 5C57 4A2B 9986 FF15"
		$"904B 8597 928E 8884 7C75 706C 6A67 6362"
		$"5E5B 574A 307C 88FF 138E 3C69 8E8A 8481"
		$"7C76 716C 6A66 635E 5B57 3D2B 7C8A FF11"
		$"A743 4467 817C 7974 6D6A 6762 5E5B 442C"
		$"3699 8CFF 0FE4 7C34 3148 5763 6765 5849"
		$"3726 2B70 DF8F FF0B EB99 5E35 1E12 121D"
		$"3357 92E9 C7FF C7FF 0BF2 B680 5332 1C1C"
		$"304F 7AAF F08F FF0F EC9A 4E59 8BB5 D3E2"
		$"DEC6 A67F 4F46 90E9 8CFF 11BD 5E75 C0F4"
		$"F1EE EAE5 E2DE DAD5 D1A1 6251 B18A FF13"
		$"A756 B2F8 F4F2 EDE8 E3DF DBD6 D5D0 CCC9"
		$"C58A 4796 88FF 15A7 6CD6 F8F4 F1EC E8E3"
		$"DDD9 D3D0 CECA C7C4 C0BC 9F55 9486 FF17"
		$"BD55 D5F7 F4F1 EDE7 E2DA D5A7 ACBC C7C4"
		$"C2C0 BCB7 B599 43AD 84FF 19EC 5EB2 F7F3"
		$"F1EC E7DF D9D4 B3FF FFA8 BDBF BDBA B7B5"
		$"B0AC 7A4A E683 FF0A 9A75 F7F3 F1EC E5DF"
		$"D8D1 B381 FF0A B1B8 B8B6 B3B0 ACA8 A450"
		$"8482 FF0A F24E C0F3 EEEA E4DE D6D0 B382"
		$"FF0B B6B3 B3B2 AEAC A8A4 A17B 3BEC 81FF"
		$"09B5 59F2 EEE9 E3DB D4CB B382 FF0C B7B1"
		$"ADAE ACA9 A7A3 9F9D 993C A081 FF08 808A"
		$"EDEA E3D9 D3C7 AD82 FF0D B7AF ADAA A8A5"
		$"A4A0 9E9B 9994 5566 81FF 0753 B1EC E3DA"
		$"D1C6 A382 FF0E B8B4 B7B4 B4AC A5A0 9998"
		$"9592 9069 3D81 FF06 32CC E5DD D1CA A088"
		$"FF09 FDF6 E999 908F 8D8B 7825 81FF 061C"
		$"DDE0 D8CB C6B2 88FF 09FD F9F1 958B 8B8A"
		$"867E 1881 FF06 1CD9 DDD4 CBC2 A388 FF09"
		$"F9F3 EC98 8786 8682 7B18 81FF 0730 C4DA"
		$"D3C7 C0B6 C081 FF0F F3BC A2A2 9F9D 9999"
		$"8D8A 8282 817E 6D24 81FF 084F A3D5 D0CA"
		$"C0B6 ADBD 81FF 0EE8 A492 8D8C 8887 8382"
		$"8180 7E7B 5A3B 81FF 1B79 7CD3 CEC7 C0B7"
		$"AEA7 C5FD FFFD FEE4 A28A 8887 8683 8180"
		$"7C79 7646 5E81 FF10 AE4E CFCA C5BF B7AE"
		$"A7A3 CCF8 F8F9 F6DE 9580 8507 8380 7D79"
		$"7472 3198 81FF 1BF0 469D C6C1 BBB6 ADA5"
		$"9D9E D0F1 F2ED EA8C 8282 8380 7D78 7672"
		$"5833 EA82 FF19 9061 C1BD B8B3 ADA7 9E95"
		$"8FCE E9DD B48A 8081 7E7D 7874 726E 3A78"
		$"83FF 19E8 5187 BAB5 B1AD A89F 968F 878D"
		$"8D80 7E80 7379 7876 726E 4F3F E184 FF17"
		$"B146 9CB1 ADA9 A49E 9890 8883 8080 7B73"
		$"7B78 7471 6D5D 339F 86FF 1596 5495 A8A6"
		$"A19C 9891 8B85 8280 7B78 7876 716D 5D3A"
		$"8388 FF13 9442 77A1 9E98 9590 8B86 8280"
		$"7B78 7471 6E4F 3383 8AFF 11AC 4A4F 7895"
		$"908C 8882 807C 7774 7157 393F 9F8C FF0F"
		$"E583 3B3B 5468 767C 786A 5845 3033 78E1"
		$"8FFF 0BEC 9F65 3D25 1818 233B 5E97 EAC7"
		$"FF",
		/* [3] */
		'l8mk',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 005B BED6 FFFF"
		$"FFFF D6BE 5B00 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 00A2 F6FF FFFF FFFF"
		$"FFFF FFFF FFF6 A200 0000 0000 0000 0000"
		$"0000 0000 0000 0030 E8FF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFE8 3000 0000 0000 0000"
		$"0000 0000 0000 81FF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FF81 0000 0000 0000"
		$"0000 0000 0081 FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF 8100 0000 0000"
		$"0000 0000 30FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FF30 0000 0000"
		$"0000 0000 E8FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFE8 0000 0000"
		$"0000 00A2 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF A200 0000"
		$"0000 00F6 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF F600 0000"
		$"0000 5BFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FF5B 0000"
		$"0000 BEFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFBE 0000"
		$"0000 D6FF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFD6 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 D6FF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFD6 0000"
		$"0000 BEFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFBE 0000"
		$"0000 5BFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FF5B 0000"
		$"0000 00F6 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF F600 0000"
		$"0000 00A2 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF A200 0000"
		$"0000 0000 E8FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFE8 0000 0000"
		$"0000 0000 30FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FF30 0000 0000"
		$"0000 0000 0081 FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF 8100 0000 0000"
		$"0000 0000 0000 81FF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FF81 0000 0000 0000"
		$"0000 0000 0000 0030 E8FF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFE8 3000 0000 0000 0000"
		$"0000 0000 0000 0000 00A2 F6FF FFFF FFFF"
		$"FFFF FFFF FFF6 A200 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 005B BED6 FFFF"
		$"FFFF D6BE 5B00 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
	}
};

resource 'icns' (1311, "forward") {
	{	/* array elementArray: 3 elements */
		/* [1] */
		'ICN#',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 001F F800 007F FE00"
		$"01FF FF80 03FF FFC0 07FF FFE0 0FFF FFF0"
		$"0FFF FFF0 1FFF FFF8 1FFF FFF8 3FFF FFFC"
		$"3FFF FFFC 3FFF FFFC 3FFF FFFC 3FFF FFFC"
		$"3FFF FFFC 3FFF FFFC 3FFF FFFC 3FFF FFFC"
		$"3FFF FFFC 1FFF FFF8 1FFF FFF8 0FFF FFF0"
		$"0FFF FFF0 07FF FFE0 03FF FFC0 01FF FF80"
		$"007F FE00 001F F800 0000 0000 0000 0000",
		/* [2] */
		'il32',
		$"C7FF 03F0 9351 3A81 3303 394D 8DEE 8FFF"
		$"0FEA 6834 385A 7AB8 C7C5 AE71 5236 3360"
		$"E68C FF11 A83A 3F91 DADA D4D0 CBC6 C2BE"
		$"BBB4 7339 379C 8AFF 137A 337B E0DF D7D2"
		$"CCC7 C2C0 BBB7 B3B1 A9A4 5C33 6D88 FF15"
		$"7A38 B2E0 DCD9 D2CA C2BB B7B3 B0AE ABA9"
		$"A49F 9C78 346B 86FF 17A8 33B2 E0DC D9D2"
		$"CDC2 B8A1 717A 9FA2 A3A1 9E9A 9592 7033"
		$"9584 FF19 EA3A 7BE0 DCD9 D5CF C7BA 9C81"
		$"FFE9 7194 9A98 9894 928E 8750 35E2 83FF"
		$"0A68 3FDF DCD9 D4CF CAC0 B685 81FF 0A81"
		$"9292 908F 8B87 847F 3458 82FF 0BF0 3490"
		$"DCD7 D2CD C8C2 B8AC 7C82 FF00 7A80 8706"
		$"8582 7F7B 5132 EA81 FF0C 9338 DBD7 D2CB"
		$"C5BE B7AE A489 8781 FF0A F587 807B 7D7C"
		$"7875 7130 7E81 FF0D 505B DAD2 CBC1 B8B1"
		$"A9A4 9D97 8098 81FF 09F7 8C75 7175 726D"
		$"6C37 4381 FF0E 3A7B D4CC C2B7 A375 6967"
		$"6867 6B64 A381 FF08 F796 6A6A 6C69 6541"
		$"3181 FF0A 33B9 CDC7 BBAD 6BC5 E4F0 F886"
		$"FF07 F777 6161 6260 5033 81FF 0B33 C7C8"
		$"C1B3 A390 D7E1 EAF5 FD86 FF06 8B5B 5B5C"
		$"5953 3381 FF0C 33C3 C5BB AD9E 8CD2 DCE6"
		$"F0F8 FD84 FF07 FC71 5457 5754 4E33 81FF"
		$"0933 AEC0 B6A8 9981 8187 8A81 8C0D A9FA"
		$"FFFF FDCD 6F53 5152 524F 4533 81FF 1438"
		$"6FB9 B3A6 9A92 8A81 7C77 726F 8CE4 F8FA"
		$"F8CB 7152 8051 034E 4C35 2F81 FF1B 4D51"
		$"B6AF A69D 948B 8277 726F 98E0 ECF0 EFC3"
		$"7452 4E4F 514F 4C48 2D3F 81FF 1B8C 35B1"
		$"ACA4 9D98 9085 7B6D 8CD7 E0E4 E4BE 7251"
		$"4E4F 4F4E 4C48 432D 7781 FF10 EE33 71A6"
		$"A19C 9792 877A 6C90 D2D6 D7B5 7080 4E07"
		$"4F4E 4A48 4332 31E7 82FF 0E60 38A1 9D97"
		$"938C 8476 6772 C6CA B370 804F 074E 4D4A"
		$"4743 3E2E 5083 FF19 E536 5A98 928E 8982"
		$"786A 5E6C 776A 534F 5148 4A49 4742 3F2E"
		$"31DD 84FF 0D9A 3374 8E8A 867F 776B 615B"
		$"5451 5280 4E06 4A47 423E 3433 8886 FF15"
		$"6D34 6F85 807B 766D 655E 5956 544F 4D4A"
		$"4742 3E33 315E 88FF 136B 334D 7B76 726C"
		$"6760 5957 524E 4A45 423E 2E33 5D8A FF11"
		$"9435 344E 6C67 635D 5853 4E49 4543 312E"
		$"3188 8CFF 0FE2 5832 3036 3F4E 514C 4134"
		$"2D2D 314F DD8F FF03 E97D 4231 8133 032F"
		$"3F76 E7C7 FFC7 FF03 F197 5741 813B 0340"
		$"5491 EF8F FF0F EB6E 3C40 6283 C3D4 CFB9"
		$"7A5A 3D3B 67E7 8CFF 11AC 4146 9AE6 E3DE"
		$"DBD7 D2D0 CDC7 C47F 413F A08A FF13 7F3B"
		$"83EB E8E2 DED9 D5D0 CEC9 C6C0 BFB9 B468"
		$"3B74 88FF 157F 3FBB EBE7 E5DD D8D0 CDC8"
		$"C3C0 BEBA B6B4 B0AB 863C 7386 FF17 AC3B"
		$"BBEA E7E3 DFD9 D2C8 B585 8CB2 B6B4 B0AF"
		$"ABA6 A27F 3B9B 84FF 19EB 4183 EAE7 E3E0"
		$"D9D5 C9AC 93FF EF85 AAAB A9A9 A6A2 9D9B"
		$"5B3D E483 FF0A 6D46 EAE6 E2E0 DDD6 CEC5"
		$"9781 FF0A 93A5 A2A2 A0A0 9C97 923D 6082"
		$"FF0B F13C 99E7 E3DE DAD5 CFC8 BD92 82FF"
		$"0A8F 9D9C 9B99 9693 8E60 3AEB 81FF 0C97"
		$"40E6 E2DD D8D3 CAC6 BEB7 9D99 81FF 0AF7"
		$"9B94 9392 908D 8A85 3984 81FF 0D57 62E3"
		$"DDD7 CFC5 C0BA B7B0 AA96 A781 FF09 FC9E"
		$"8987 8986 857F 424A 81FF 0E41 83DE D9D2"
		$"C6B7 897B 7B7E 7C80 79B1 81FF 08FB A67F"
		$"7F80 7F7A 4E39 81FF 093B C4DA D4CA BE7F"
		$"D4EF F787 FF01 FC8D 8078 0275 663B 81FF"
		$"0A3B D3D5 CEC3 B7A1 E3ED F5FB 87FF 009D"
		$"8074 0270 673B 81FF 0A3B CED1 C8BE B2A0"
		$"E1E8 F1F7 87FF 0688 7070 6E6B 653B 81FF"
		$"0E3B B8CC C5B8 AD96 939C 9EA0 A0A1 A1B7"
		$"81FF 08DC 856D 6A6B 6A66 583B 81FF 0E3F"
		$"79C8 C1B6 ADA5 9E97 928D 8985 9EEE 80FF"
		$"09D7 876C 6A6A 6767 6243 3881 FF1B 5359"
		$"C4BF B6AC A6A0 978E 8985 A7EB F6F7 F8D3"
		$"8A6C 6866 6766 625F 3947 81FF 1291 3DBE"
		$"BBB4 AEA9 A199 8F83 9EE4 EBEE EECE 896B"
		$"8068 0567 6260 5C36 7D81 FF1B EE3B 7CB5"
		$"B0AC A6A1 9B8E 82A6 E1E4 E3C5 8568 6867"
		$"6867 635F 5C44 39E9 82FF 1966 41B0 ACA7"
		$"A4A0 998E 8088 D5D9 C687 6866 6864 6463"
		$"5D5C 5738 5883 FF19 E73E 66A9 A2A0 9C97"
		$"8B7F 7781 8D80 6D68 6A5C 6062 5D5C 583D"
		$"39DF 84FF 179F 3B83 A09B 9793 8B80 7A74"
		$"6D67 6A64 6265 635D 5C57 483B 8E86 FF15"
		$"733C 7D97 938F 8982 7B75 706C 6968 6461"
		$"5D5B 5748 3965 88FF 1372 3B5A 8F8A 8580"
		$"7C76 726C 6965 635E 5B57 3C3B 668A FF11"
		$"9B3C 3C5D 817C 7973 6E69 6762 5E5A 4237"
		$"398E 8CFF 0FE3 5F3A 3841 4D63 6665 5844"
		$"3936 3958 DF8F FF03 EB83 4A38 813B 0338"
		$"477E E9C7 FFC7 FF03 F29C 5F49 8144 0348"
		$"5C97 F08F FF0F EC74 4548 6B8E D1E4 DFC7"
		$"8665 4644 6EE9 8CFF 11B0 4A4F A5F4 F0EE"
		$"EAE6 E5E0 DCD6 D38D 4B47 A58A FF13 8644"
		$"8EF9 F5F2 EDE9 E4E0 DDDA D7D3 CECB C773"
		$"447B 88FF 1586 48C6 F9F5 F2ED E7E1 DDDA"
		$"D5D2 D0CD C9C6 C2BE 9646 7986 FF17 B044"
		$"C6F8 F5F2 EFE8 E2DB CA9A 9FC8 CAC7 C3C1"
		$"BEB9 B791 44A1 84FF 19EC 4A8E F8F4 F2EE"
		$"E9E4 DCC3 A1FF F79A BDC2 BDBC B9B7 B2AF"
		$"6945 E683 FF0A 744F F8F4 F2EE E9E4 DFD5"
		$"A581 FF0A A1BA B5B5 B4B3 AFAB A746 6782"
		$"FF0B F245 A5F4 F0ED EAE5 DFD8 D0A7 82FF"
		$"0AA5 B1B1 AFAE ABA7 A470 43EC 81FF 0C9C"
		$"48F4 F0EB E6E2 DCD7 D1CC B3A9 81FF 0AFB"
		$"AAAB A9A7 A5A2 9F9B 428C 81FF 0D5F 6BF0"
		$"EDE6 DFD8 D3CD CCC5 BFAF B582 FF08 AEA2"
		$"9F9F 9D9B 964E 5381 FF0E 498E EEE9 E1D7"
		$"CC9D 8E8E 918F 948C BD82 FF07 B49A 9897"
		$"9492 5E41 81FF 0844 D1EA E4DA D192 DFFC"
		$"89FF 06A4 9191 8F8D 7A44 81FF 0944 E1E5"
		$"DCD5 CCB0 F2F7 FD88 FF06 B08E 8D8A 8880"
		$"4481 FF09 44DE DFDB D1C7 B5EF F5FA 88FF"
		$"06A0 8987 8784 7D44 81FF 0E44 C6DB D6CB"
		$"C2AC A1B1 B4B3 B3B6 B6C3 81FF 08EB 9F88"
		$"8485 847F 6F44 81FF 0E48 84D7 D2C9 C2B9"
		$"B4AF AAA4 A19F AEF7 80FF 09E5 A086 8484"
		$"8080 7E54 4181 FF1B 5C63 D3CE C8C1 BBB4"
		$"ACA6 A29F B5F3 FFFF FEE0 A486 8383 807E"
		$"7B78 4650 81FF 1B97 46D1 CCC6 C1BC B5AF"
		$"A69C AEEF F8F7 FADC A185 8383 7F7E 7B76"
		$"753F 8681 FF10 F044 89C8 C3BE B9B4 AFA4"
		$"9CBA EFEE F2D7 9F80 8307 817E 7A78 7555"
		$"42EA 82FF 196E 4AC3 BFBA B7B3 ACA4 9BA0"
		$"E1E9 D6A0 8483 837D 7D7A 7675 7142 6183"
		$"FF19 E847 71BC B7B3 B1AB A197 9096 A598"
		$"8683 8373 7878 7975 714C 43E2 84FF 17A5"
		$"4494 B3AF ABA7 A098 918A 8884 847E 797E"
		$"7A76 7470 5C44 9586 FF15 7A45 8DAA A9A4"
		$"9F9A 928D 8885 827E 7D7A 7974 705C 426E"
		$"88FF 1379 4467 A4A0 9B97 928D 8984 827D"
		$"7A77 7471 4C44 6E8A FF11 A145 466D 9792"
		$"8E8A 8582 7E79 7773 5541 4395 8CFF 0FE5"
		$"6743 424D 5D78 7E7A 6C52 453F 4261 E28F"
		$"FF03 EC8A 5341 8144 0341 5085 EAC7 FF",
		/* [3] */
		'l8mk',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 005B BED6 FFFF"
		$"FFFF D6BE 5B00 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 00A2 F6FF FFFF FFFF"
		$"FFFF FFFF FFF6 A200 0000 0000 0000 0000"
		$"0000 0000 0000 0030 E8FF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFE8 3000 0000 0000 0000"
		$"0000 0000 0000 81FF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FF81 0000 0000 0000"
		$"0000 0000 0081 FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF 8100 0000 0000"
		$"0000 0000 30FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FF30 0000 0000"
		$"0000 0000 E8FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFE8 0000 0000"
		$"0000 00A2 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF A200 0000"
		$"0000 00F6 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF F600 0000"
		$"0000 5BFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FF5B 0000"
		$"0000 BEFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFBE 0000"
		$"0000 D6FF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFD6 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 D6FF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFD6 0000"
		$"0000 BEFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFBE 0000"
		$"0000 5BFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FF5B 0000"
		$"0000 00F6 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF F600 0000"
		$"0000 00A2 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF A200 0000"
		$"0000 0000 E8FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFE8 0000 0000"
		$"0000 0000 30FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FF30 0000 0000"
		$"0000 0000 0081 FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF 8100 0000 0000"
		$"0000 0000 0000 81FF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FF81 0000 0000 0000"
		$"0000 0000 0000 0030 E8FF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFE8 3000 0000 0000 0000"
		$"0000 0000 0000 0000 00A2 F6FF FFFF FFFF"
		$"FFFF FFFF FFF6 A200 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 005B BED6 FFFF"
		$"FFFF D6BE 5B00 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
	}
};

resource 'icns' (1312, "reload") {
	{	/* array elementArray: 3 elements */
		/* [1] */
		'ICN#',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 001F F800 007F FE00"
		$"01FF FF80 03FF FFC0 07FF FFE0 0FFF FFF0"
		$"0FFF FFF0 1FFF FFF8 1FFF FFF8 3FFF FFFC"
		$"3FFF FFFC 3FFF FFFC 3FFF FFFC 3FFF FFFC"
		$"3FFF FFFC 3FFF FFFC 3FFF FFFC 3FFF FFFC"
		$"3FFF FFFC 1FFF FFF8 1FFF FFF8 0FFF FFF0"
		$"0FFF FFF0 07FF FFE0 03FF FFC0 01FF FF80"
		$"007F FE00 001F F800 0000 0000 0000 0000",
		/* [2] */
		'il32',
		$"C7FF 0BF0 AF77 4928 1414 2745 6FA7 EE8F"
		$"FF0F EA93 444C 7CA0 BBCA C6AF 906B 413B"
		$"85E6 8CFF 11B7 5468 ABDA D7D4 D0CB C7C3"
		$"BEBB B486 5045 A98A FF13 9E49 A0DB D6D4"
		$"D4CF CBC7 C3BE B9B3 AEA9 A471 388B 88FF"
		$"159E 60BF D9D6 D6D1 CCC8 C5C0 BCB7 B3AE"
		$"A8A4 9E9A 8143 8886 FF17 B74A BFD7 D9D4"
		$"CDC8 C3BC BBB7 B2AE A9A6 A19E 9993 9078"
		$"33A2 84FF 19EA 549F DADA D4CC BD90 9598"
		$"999A 9F9E 9F9D 9794 9390 8C87 5F3D E283"
		$"FF08 9268 DCDA D4CB C281 F782 FF0B FCF5"
		$"EBE0 8A8E 8B87 847F 3B76 82FF 08F0 44AB"
		$"DBD5 CDC3 A7C5 85FF 0AF7 E186 8685 847F"
		$"7B5A 2DEA 81FF 08AF 4CDB D7D0 C7BD 98D6"
		$"81FF 0EF2 A3A8 A7A8 9C81 8180 7D7A 7571"
		$"2994 81FF 0876 7CDA D2CB C2B6 95D8 82FF"
		$"0DD6 8585 817F 7C7B 7B78 7570 6C3B 5781"
		$"FF0B 499F D4CD C7BD B194 DBFF FFF6 80FF"
		$"0CD6 807C 7A77 7576 7270 6B65 492F 81FF"
		$"0C28 B9CF CAC2 B6AC 90DB FFFF 76F6 80FF"
		$"0BB1 7774 716F 6F6C 6966 6050 1881 FF0C"
		$"14C7 CAC5 BDB2 A68C DBFF FF7C 8980 FF02"
		$"F280 6D80 6B05 6766 605B 530F 81FF 1B14"
		$"C3C6 BDB8 ACA1 8AD7 FFFF 7C80 ABFF FFF8"
		$"AE6A 6767 6663 6159 564E 0F81 FF11 25AE"
		$"C1B9 B3A4 9A87 D6FF FF7C 7B7F E9F8 F6C5"
		$"8063 0662 6159 5451 4517 81FF 1B43 8EBB"
		$"B6AC A295 86BE F8F8 7C76 71C6 F2EA C767"
		$"5E60 5E59 544F 4C37 2A81 FF1B 6E69 B7B1"
		$"A89F 948A 80A3 9C77 706B A8E7 E2C6 6659"
		$"5B58 5651 4C48 294F 81FF 1BA6 40B1 ACA4"
		$"9D95 8A82 7B72 6F6A 67B8 DFD7 B75C 5456"
		$"534F 4C48 431A 8B81 FF1B EE3A 83A6 A19D"
		$"948C 857B 716B 666F C7D4 CFAD 564C 514E"
		$"4A48 4332 24E7 82FF 1984 4DA1 9D97 928B"
		$"847C 7068 618E CBC7 C1B3 484A 4C49 4743"
		$"3E1F 6883 FF19 E544 6E98 928E 8984 7C70"
		$"635D 9EBF BAB5 7E43 4547 4542 3F2C 2DDD"
		$"84FF 17A7 377C 8E8A 8680 7A6F 635B 6AB1"
		$"AB8A 5142 4444 423E 3520 9386 FF0D 8A41"
		$"7785 807B 766D 6359 5254 4F4E 8043 0440"
		$"3E34 2374 88FF 1388 335B 7B76 726C 635C"
		$"534E 4947 4442 403D 2C20 738A FF11 A13C"
		$"3857 6C67 625B 544F 4C47 4340 301F 2D93"
		$"8CFF 0FE2 762D 283A 454E 4F4A 4034 281B"
		$"2466 DD8F FF0B E993 552F 180F 0F18 2A4F"
		$"8AE7 C7FF C7FF 0BF1 B27C 4E2D 1818 2B4B"
		$"74AA EF8F FF0F EB97 4954 83AA C5D4 D1BA"
		$"9A74 4841 8AE7 8CFF 11BA 596E B5E5 E1DE"
		$"DBD7 D3D0 CDC7 C493 584D AD8A FF03 A24F"
		$"A8E6 80E0 0CDB D8D4 D0CB C8C0 BCB9 B47F"
		$"4191 88FF 15A2 67C9 E3E1 E2DD DAD7 D3CE"
		$"CAC6 C1BB B8B4 AFAA 904C 8F86 FF17 BA50"
		$"C9E1 E3E0 DCD7 D3CD CAC6 C2BE BAB6 B1AF"
		$"ABA4 A188 3BA7 84FF 19EB 59A7 E3E5 DED9"
		$"CDA5 AAAC ABAB B1B1 B0AF ABA7 A6A3 9C9B"
		$"6C44 E483 FF08 966E E7E5 DED9 D397 FD83"
		$"FF0A FBF6 EB9E A19E 9C97 9245 7E82 FF08"
		$"F149 B5E6 E3DA D2B9 D385 FF0A FDEC 9C99"
		$"9997 938E 6B34 EB81 FF08 B252 E6E2 DCD7"
		$"CDAC E181 FF0E F8B4 BCB8 B6AE 9797 9492"
		$"8E8A 8533 9A81 FF08 7A82 E3DF D9D0 C9AB"
		$"E382 FF0D E19B 9D98 9592 8F8F 8B89 857F"
		$"485E 81FF 0B4E A8DE DCD5 CBC0 A6E3 FFFF"
		$"FB80 FF0C E196 948F 8C8A 8A87 8480 7A58"
		$"3781 FF0C 2DC4 D9D6 D0C5 BBA5 E3FF FF8C"
		$"FB80 FF0B C38E 8988 8584 827F 7A75 661E"
		$"81FF 0C18 D3D6 D1CB C1B6 A1E3 FFFF 949E"
		$"80FF 0BF8 9685 8382 7F7E 7A75 7067 1381"
		$"FF0D 18CE D2CC C6BD B1A0 E2FF FF94 96BE"
		$"80FF 0ABF 807E 7F7B 7B76 706B 6513 81FF"
		$"1B2B B8CD C8C1 B7AD 9CE1 FFFF 9491 95F2"
		$"FFFB D37C 7A79 7876 706B 6758 1D81 FF1B"
		$"4999 C9C4 BEB3 AA9C CEFF FF94 8C88 D5FA"
		$"F5D7 7F75 7574 706B 6862 4833 81FF 1B73"
		$"72C5 C0B9 B0A7 9E96 B4AE 8E87 84BC F3EE"
		$"D57E 7271 6E6B 6762 5F39 5781 FF1B AA47"
		$"BEBB B4AE A69E 978F 8985 807F CBEB E7CA"
		$"756D 6C6A 6862 605C 2591 81FF 1BEE 3F8F"
		$"B5B0 ACA5 A098 9088 837E 85D7 E2DD C16F"
		$"6667 6763 5F5C 472D E982 FF19 8957 B0AC"
		$"A7A2 9E99 9285 7E7A A2DA D8D5 C864 6362"
		$"625D 5C57 2E70 83FF 19E7 4C7C A9A2 A09C"
		$"988F 857C 75B4 CFCE CA93 5F5E 605C 5C58"
		$"3E36 DF84 FF17 AC3E 8CA0 9B97 948E 847A"
		$"7180 C6C0 A067 5C5D 5C5C 574A 2B99 86FF"
		$"1590 4B86 9793 8F89 8279 716B 6D68 675D"
		$"5C5C 5957 4A2F 7B88 FF13 8E3C 6A8F 8A85"
		$"8079 746C 6763 605E 5C59 563D 2B7C 8AFF"
		$"11A7 4343 6781 7C77 716B 6665 605D 5744"
		$"2B36 998C FF0F E37C 3431 4757 6365 6357"
		$"4836 262B 70DF 8FFF 0BEB 985E 351E 1313"
		$"1E33 5792 E9C7 FFC7 FF0B F2B6 8154 331D"
		$"1D31 507A B0F0 8FFF 0FEC 9B4F 5A8C B6D4"
		$"E4E0 C8A8 8050 4790 E98C FF11 BE5F 76C1"
		$"F3EF EEEA E6E5 E0DC D6D3 A363 52B2 8AFF"
		$"13A7 57B3 F4EE EFEE EAE6 E4E0 DBD8 D3CC"
		$"C9C7 8B48 9788 FF15 A76D D5F2 EFF0 EBEA"
		$"E6E2 DFDA D6D2 CECB C4BE BDA1 5694 86FF"
		$"17BE 56D4 EFF3 EFEB E6E2 E0DB D6D5 D1CC"
		$"C8C4 C1BE B7B5 9B44 AD84 FF0E EC5F B2F2"
		$"F3EE E9DF B9BD BCBE BEC1 C180 BF07 B8BA"
		$"B8B0 AF7C 4BE6 83FF 079B 76F5 F3EE E9E2"
		$"AE85 FF09 FCF4 B3B5 B3AF ABA7 5184 82FF"
		$"08F2 4FC1 F3EF EAE2 CCDE 86FF 09F9 B1B0"
		$"AEAB A7A4 7D3C EC81 FF08 B65A F4F0 EAE5"
		$"DCC0 E981 FF0E FDC3 CCCB C6C1 AEAB AAA6"
		$"A29F 9B3D A181 FF08 818B F0EF E9E0 DABE"
		$"EB82 FF0D E9B0 B3B1 ACA9 A6A6 A29F 9B96"
		$"5667 81FF 0854 B4EE EBE5 DCD5 B9EC 83FF"
		$"0CE9 AEAA A7A4 A2A0 9F9A 9692 6B3E 81FF"
		$"0B33 D1E9 E5DF D8D0 BAEC FFFF A481 FF0B"
		$"D3A6 A4A0 9D9C 9A94 918D 7A26 81FF 0C1D"
		$"E1E5 E0DB D2CB B6EC FFFF AAB3 80FF 0BFD"
		$"AEA0 9D9A 9896 918D 8880 1981 FF0D 1DDE"
		$"E0DF D7D0 C4B4 EBFF FFAA AECD 80FF 0AD1"
		$"9C97 9493 918C 8984 7D19 81FF 1B31 C6DC"
		$"D8D2 CCC2 B1E9 FFFF AAA9 ACF9 FFFF DE95"
		$"9392 8F8C 8884 806F 2581 FF1B 50A4 D8D5"
		$"CEC6 BEB3 DCFF FFAA A4A1 E0FF FEE6 998E"
		$"8E8C 8883 817E 5C3C 81FF 1B7A 7DD5 D0CB"
		$"C3BB B4AB C3C1 A6A0 9DCC FCF8 E598 8D8A"
		$"8783 7F7B 7848 5F81 FF1B AF4F D1CC C6C1"
		$"BAB4 ACA6 A29F 9C99 DBF4 F2DA 8F88 8583"
		$"7F7B 7675 3299 81FF 1BF0 479E C8C3 BDB8"
		$"B3AC A6A0 9D98 9FE4 EEEC D289 8380 807A"
		$"7875 5B34 EA82 FF19 9062 C3BF BAB7 B3AC"
		$"A79F 9891 B5EA E7E4 D880 7F7E 7B76 7571"
		$"3C78 83FF 19E8 5288 BCB7 B3B1 ACA5 9D95"
		$"90C9 E1DF DCAB 7B7A 7979 7571 5140 E284"
		$"FF17 B147 9EB3 AFAB A9A2 9C92 8D9A D9D3"
		$"B585 7977 7674 705F 349F 86FF 1596 5597"
		$"AAA9 A4A0 9A92 8B85 8884 817A 7777 7470"
		$"5F3B 8388 FF13 9443 79A4 A09B 9691 8A86"
		$"817E 7A77 7574 7051 3483 8AFF 11AD 4B50"
		$"7A97 928D 8984 7F7B 7976 7058 3A40 9F8C"
		$"FF0F E584 3C3C 556A 787D 796B 5946 3134"
		$"78E2 8FFF 0BEC 9F66 3E26 1919 243C 5F98"
		$"EAC7 FF",
		/* [3] */
		'l8mk',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 005B BED6 FFFF"
		$"FFFF D6BE 5B00 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 00A2 F6FF FFFF FFFF"
		$"FFFF FFFF FFF6 A200 0000 0000 0000 0000"
		$"0000 0000 0000 0030 E8FF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFE8 3000 0000 0000 0000"
		$"0000 0000 0000 81FF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FF81 0000 0000 0000"
		$"0000 0000 0081 FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF 8100 0000 0000"
		$"0000 0000 30FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FF30 0000 0000"
		$"0000 0000 E8FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFE8 0000 0000"
		$"0000 00A2 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF A200 0000"
		$"0000 00F6 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF F600 0000"
		$"0000 5BFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FF5B 0000"
		$"0000 BEFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFBE 0000"
		$"0000 D6FF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFD6 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 D6FF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFD6 0000"
		$"0000 BEFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFBE 0000"
		$"0000 5BFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FF5B 0000"
		$"0000 00F6 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF F600 0000"
		$"0000 00A2 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF A200 0000"
		$"0000 0000 E8FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFE8 0000 0000"
		$"0000 0000 30FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FF30 0000 0000"
		$"0000 0000 0081 FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF 8100 0000 0000"
		$"0000 0000 0000 81FF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FF81 0000 0000 0000"
		$"0000 0000 0000 0030 E8FF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFE8 3000 0000 0000 0000"
		$"0000 0000 0000 0000 00A2 F6FF FFFF FFFF"
		$"FFFF FFFF FFF6 A200 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 005B BED6 FFFF"
		$"FFFF D6BE 5B00 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
	}
};

resource 'icns' (1313, "stop") {
	{	/* array elementArray: 3 elements */
		/* [1] */
		'ICN#',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 001F F800 007F FE00"
		$"01FF FF80 03FF FFC0 07FF FFE0 0FFF FFF0"
		$"0FFF FFF0 1FFF FFF8 1FFF FFF8 3FFF FFFC"
		$"3FFF FFFC 3FFF FFFC 3FFF FFFC 3FFF FFFC"
		$"3FFF FFFC 3FFF FFFC 3FFF FFFC 3FFF FFFC"
		$"3FFF FFFC 1FFF FFF8 1FFF FFF8 0FFF FFF0"
		$"0FFF FFF0 07FF FFE0 03FF FFC0 01FF FF80"
		$"007F FE00 001F F800 0000 0000 0000 0000",
		/* [2] */
		'il32',
		$"C7FF 0BF0 AF76 4927 1313 2645 6FA7 EE8F"
		$"FF0F EA93 434D 7CA1 BCCB C7B1 916C 413A"
		$"86E6 8BFF 12FE B753 66A9 DDDB D6D3 CEC9"
		$"C5BF BCB4 8550 45A9 89FF 14FE 9E49 95D6"
		$"DBD8 D5D1 CEC9 C5C0 BAB4 AFA8 9C6E 3A8C"
		$"87FF 16FE 9E5B ADD9 D5D1 CFCE CAC6 C4BF"
		$"B7B2 ACA6 A19D 907A 438A 85FF 18FE B749"
		$"A8D9 CAB0 B4C4 C3C1 C0BF B7B1 AAA2 8B8F"
		$"9092 8874 34A3 84FF 08E9 538B D5C4 96F2"
		$"F288 80B7 0DB5 AFA6 A086 A2D4 B68F 8A7E"
		$"5E3D E283 FF19 9262 CFD0 8BFC FFFF E89D"
		$"AFAC AAA5 9D84 A8EC E3D5 B183 8079 3B76"
		$"82FF 06F0 43A2 D6C8 90F3 80FF 11DA 8DA1"
		$"9E9C 83AC F9EF E7DA A37C 7C77 5B2E EA81"
		$"FF06 AF4D D8D3 C6B6 8681 FF09 D489 928E"
		$"ADFF FCF4 E7A5 8076 0375 712A 9581 FF07"
		$"767C D9D1 C5BB AC8A 81FF 0FD8 B6B6 FFFF"
		$"FCF2 B276 7575 7270 6D3C 5781 FF08 499F"
		$"D5CF C6BB B0A2 8F85 FF0A FAAB 7674 7472"
		$"706C 674A 2F81 FF09 27BB D0CC C5BB B0A5"
		$"98B6 84FF 0AAF 7272 7472 6F6B 6762 5218"
		$"81FF 0A13 C9CB C7C1 B9AD A197 8ED4 82FF"
		$"0BFC 8D6F 6F70 6F6B 6762 5D56 0E81 FF0A"
		$"13C5 C7C0 BBB2 A799 9081 D482 FF0B F77F"
		$"686A 6868 6662 5C58 510E 81FF 0925 B0C2"
		$"BAB4 A89C 9285 6882 FF0C FDEA C665 6061"
		$"6261 5C57 5347 1781 FF1B 448F BBB5 AA9C"
		$"9086 68FD FFFF FDF4 F4F7 EFE0 BA5B 5859"
		$"5856 524E 392A 81FF 1B6E 69B6 ACA0 9285"
		$"68EF F7F7 F5AC 8A94 E7E8 E0D4 AD51 4E51"
		$"514E 4A2A 4F81 FF1B A640 ADA6 9889 68E4"
		$"E8EA EA9D 6A6A 6683 DAD9 D3C0 A147 494C"
		$"4945 1C8C 81FF 1BEE 3A81 A094 68D0 D9DE"
		$"DE98 6C6C 6862 607A CBC8 BFAA 7642 4743"
		$"3425 E782 FF19 844E 9894 68AA CBD0 8D6B"
		$"6C6F 6B65 5B56 72BB B29E 673F 3E3E 2168"
		$"83FF 19E5 456D 8B68 868A 886C 6D70 706C"
		$"655B 5148 6B83 6843 393A 2E2E DD84 FF03"
		$"A838 7780 8068 1071 7070 6D6A 635C 524A"
		$"433E 3E38 3535 2293 86FF 158A 4272 7B7B"
		$"7675 716C 6A65 605B 534D 4743 3B38 3426"
		$"7588 FF13 8933 5C79 7572 6F6A 655E 5B56"
		$"514C 4540 3D2E 2275 8AFF 11A2 3C3A 586D"
		$"6865 605B 5651 4C48 4533 212E 938C FF0F"
		$"E276 2E28 3B47 4F53 4E43 362A 1C25 68DE"
		$"8FFF 0BEA 9456 2F18 0E0E 172A 4F8C E7C7"
		$"FFC7 FF0B F1B2 7A4D 2B17 172A 4A74 AAEF"
		$"8FFF 0FEB 9648 5482 A9C6 D5D1 BB9A 7348"
		$"3F8A E78B FF12 FEB9 586B B2E6 E4E0 DCD9"
		$"D4D0 CBC7 C190 584B AD89 FF14 FEA2 4D9D"
		$"E1E5 E1E0 DCD9 D4D0 CBC6 C0BC B6A8 7940"
		$"9187 FF16 FEA1 64B6 E3E1 DEDA D9D6 D3D0"
		$"CBC6 BFBA B5B0 AD9E 874C 9085 FF18 FEBA"
		$"50B2 E4D9 C1C2 D0CF CECC CAC4 C0B8 B29D"
		$"A1A4 A297 813C A884 FF19 EB59 94DF D1A9"
		$"F9F9 9AC7 C5C5 C1BC B6B0 9AB2 E0C6 A199"
		$"8C69 44E4 83FF 0496 68D9 DC9F 80FF 11F2"
		$"AEBC BBB8 B3AD 99B9 F4EE E3C0 9592 8945"
		$"7D82 FF06 F149 AAE1 D5A2 FA80 FF11 E3A0"
		$"B3AE AC98 BCFF F8F0 E5B6 908F 8769 34EB"
		$"81FF 06B1 52E1 DED3 C59A 81FF 10DE 9DA3"
		$"A2BE FFFF FAF0 B58B 8B8A 8883 329A 81FF"
		$"077A 81E1 DBD4 C9BB 9E81 FF02 E1C3 C380"
		$"FF09 F9C2 8B8A 8786 857E 495E 81FF 084D"
		$"A8DF DAD1 C7BD B2A3 86FF 09BB 8A87 8786"
		$"8280 7B58 3781 FF09 2BC5 DAD7 D1C7 BDB5"
		$"A9C3 84FF 0ABD 8786 8685 827E 7976 651D"
		$"81FF 0A17 D4D6 D2CD C6BB B0A9 A2DE 83FF"
		$"0A9F 8382 8381 7E79 7671 6812 81FF 0A17"
		$"CFD2 CCC6 C0B6 A9A2 95DE 82FF 0BFD 917E"
		$"7D7D 7C7B 7671 6C65 1281 FF09 2AB8 CDC6"
		$"C0B7 ACA4 9A7D 83FF 02F4 D27B 8076 0574"
		$"716C 6857 1C81 FF08 4999 C7C0 B8AC A39A"
		$"7D81 FF06 FAFA FDF8 EBCA 7280 6E04 6A68"
		$"6348 3381 FF1B 7372 C2BA B1A4 997D F8FD"
		$"FDFC BD9F A6F0 F2EB E1BD 6766 6767 635F"
		$"3757 81FF 1BA9 46BA B5A8 9B7D EFF2 F4F4"
		$"AE7E 7E7C 98E5 E5E0 CFB3 5F61 605E 5B26"
		$"9281 FF1B EE3E 8CAE A47D DCE5 EAE9 A881"
		$"807C 7774 8FDA D6CE BB8A 595D 5845 2CE9"
		$"82FF 1989 56A6 A37D BBD8 DEA0 8081 827E"
		$"796F 6D88 CDC7 B37D 5754 542D 7083 FF19"
		$"E74A 799A 7D9A 9E9B 8182 8382 8079 7167"
		$"5F80 967D 594F 4F3E 35DF 84FF 03AC 3E85"
		$"8F80 7D10 8583 8381 7D77 6F67 605A 5755"
		$"4C49 492C 9986 FF15 904A 808B 8C8A 8683"
		$"807C 7874 6D69 635D 5950 4B47 317C 88FF"
		$"138E 3B68 8987 8280 7D78 736E 6A67 625B"
		$"5553 3C2C 7C8A FF11 A742 4467 817C 7873"
		$"6E69 6762 5E5A 442C 3599 8CFF 0FE4 7C34"
		$"3147 5763 6764 5849 3626 2B70 DF8F FF0B"
		$"EB99 5E35 1D12 121C 3357 92E9 C7FF C7FF"
		$"0BF1 B67F 5331 1C1C 304F 7AAF F08F FF0F"
		$"EC9A 4D59 8BB4 D2E2 DFC7 A67E 4F45 90E8"
		$"8BFF 12FE BD5D 73BC F3F0 EEEA E6E2 DDDA"
		$"D5D0 9E61 52B1 89FF 14FE A756 A6EF F1EF"
		$"EBE9 E5E1 DDD8 D6D1 CCC7 B986 4897 87FF"
		$"16FE A76A C2EF EEEA E7E7 E5E1 DDD8 D3D0"
		$"C9C6 C1BE AF96 5694 86FF 17BD 55BD F0E6"
		$"CED3 E1DF DCDB D7D3 CDC8 C2AE B3B6 B4A9"
		$"9145 AD84 FF19 EC60 9EEB DDB8 FEFE ABD7"
		$"D6D5 D2CD C7C1 AAC3 EBD3 B2AD 9F77 4AE5"
		$"83FF 049A 71E5 E9AF 80FF 11F9 C1CD CCC9"
		$"C3BF A8C6 FDF5 ECD0 AAA6 9C50 8482 FF05"
		$"F14E B5EE E4B4 81FF 11E9 B4C3 C1BE A9C9"
		$"FFFE F9F1 C4A5 A29B 793A EC81 FF06 B559"
		$"EEEB E1D5 AA81 FF04 E6B0 B5B4 CC80 FF08"
		$"F9C6 A09F 9D9B 973C A081 FF07 7F8A EEEA"
		$"E1D8 CDB0 81FF 02E9 CECE 80FF 09FE D2A0"
		$"9F9C 9B98 9355 6681 FF08 53B3 ECE7 E1D7"
		$"D0C4 B586 FF09 C89F 9D9B 9A96 938F 693D"
		$"81FF 0931 D0E7 E4DD D7CD C6BC CE84 FF0A"
		$"CE9D 9C9B 9A96 928E 8A78 2581 FF0A 1CE0"
		$"E4DF DAD5 CCC3 B9B4 E683 FF0A B09A 9897"
		$"9693 8E8A 857E 1881 FF0A 1CDC DFDC D6D0"
		$"C6BC B4AA E683 FF0A A794 9292 918E 8987"
		$"827A 1881 FF09 30C4 DBD6 D0C7 BEB4 AC92"
		$"83FF 02FB DD92 808C 0588 8582 7E6B 2381"
		$"FF08 4FA3 D6D0 C8BD B6AC 9284 FF0B FEF4"
		$"D789 8585 8380 7E7A 5A3A 81FF 0879 7CD1"
		$"C9BF B8AB 92FE 80FF 0FCB B3B8 F8F9 F4EB"
		$"CD7E 7E7D 7B79 7546 5E81 FF1B AE4E C9C4"
		$"B9AF 92F6 F9FB FDC1 9696 92AA F1EF EBDF"
		$"C279 7978 7371 3198 81FF 1BEF 4599 C1B7"
		$"92E9 EFF3 F1BC 9794 938E 8CA4 E7E5 DDCD"
		$"9F73 746E 5732 EA82 FF19 8F60 B5B5 92CD"
		$"E6E7 B396 9697 948F 8784 9DDB D6C5 936F"
		$"6A6B 3977 83FF 19E8 5084 AB92 AAB1 AE96"
		$"9797 9694 8F85 7E79 97AA 9471 6565 4F3E"
		$"E184 FF03 B046 95A0 8092 109A 9896 9391"
		$"8A85 7E78 746F 6F62 5D5B 339F 86FF 1595"
		$"5490 9CA1 9D9B 9894 918C 8882 7D79 7473"
		$"6660 5B3C 8388 FF13 9444 759B 9B96 9391"
		$"8C88 8380 7B76 716B 6A4F 3383 8AFF 11AC"
		$"4A4F 7894 8F8C 8883 7F7B 7674 7056 383E"
		$"9F8C FF0F E583 3A3B 5468 757B 786A 5745"
		$"2F32 77E1 8FFF 0BEC 9F65 3D25 1818 223A"
		$"5E97 EAC7 FF",
		/* [3] */
		'l8mk',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 005B BED6 FFFF"
		$"FFFF D6BE 5B00 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 00A2 F6FF FFFF FFFF"
		$"FFFF FFFF FFF6 A200 0000 0000 0000 0000"
		$"0000 0000 0000 0030 E8FF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFE8 3000 0000 0000 0000"
		$"0000 0000 0000 81FF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FF81 0000 0000 0000"
		$"0000 0000 0081 FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF 8100 0000 0000"
		$"0000 0000 30FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FF30 0000 0000"
		$"0000 0000 E8FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFE8 0000 0000"
		$"0000 00A2 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF A200 0000"
		$"0000 00F6 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF F600 0000"
		$"0000 5BFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FF5B 0000"
		$"0000 BEFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFBE 0000"
		$"0000 D6FF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFD6 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF 0000"
		$"0000 D6FF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFD6 0000"
		$"0000 BEFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FFBE 0000"
		$"0000 5BFF FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF FF5B 0000"
		$"0000 00F6 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF F600 0000"
		$"0000 00A2 FFFF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFFF A200 0000"
		$"0000 0000 E8FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FFE8 0000 0000"
		$"0000 0000 30FF FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF FF30 0000 0000"
		$"0000 0000 0081 FFFF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FFFF 8100 0000 0000"
		$"0000 0000 0000 81FF FFFF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFFF FF81 0000 0000 0000"
		$"0000 0000 0000 0030 E8FF FFFF FFFF FFFF"
		$"FFFF FFFF FFFF FFE8 3000 0000 0000 0000"
		$"0000 0000 0000 0000 00A2 F6FF FFFF FFFF"
		$"FFFF FFFF FFF6 A200 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 005B BED6 FFFF"
		$"FFFF D6BE 5B00 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
	}
};

resource 'icns' (1323, "close pane") {
	{	/* array elementArray: 3 elements */
		/* [1] */
		'ics#',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0FE0 1FF0 3FF8 7FFC 7FFC 7FFC 7FFC"
		$"7FFC 7FFC 7FFC 3FF8 1FF0 0FE0 0000 0000",
		/* [2] */
		'is32',
		$"91FF 01D8 8D80 5401 8DD8 85FF 008D 8454"
		$"008D 83FF 008D 8654 008D 81FF 00D8 8854"
		$"00D8 80FF 008D 8054 04FF 8C54 8CFF 8054"
		$"008D 80FF 8154 048C FFB2 FF8C 8154 80FF"
		$"8254 02B2 FFB2 8254 80FF 8154 048C FFB2"
		$"FF8C 8154 80FF 008D 8054 04FF 8C54 8CFF"
		$"8054 008D 80FF 00D8 8854 00D8 81FF 008D"
		$"8654 008D 83FF 008D 8454 008D 85FF 01D8"
		$"8D80 5401 8DD8 A2FF 91FF 01D8 8D80 5401"
		$"8DD8 85FF 008D 8454 008D 83FF 008D 8654"
		$"008D 81FF 00D8 8854 00D8 80FF 008D 8054"
		$"04FF 8C54 8CFF 8054 008D 80FF 8154 048C"
		$"FFB2 FF8C 8154 80FF 8254 02B2 FFB2 8254"
		$"80FF 8154 048C FFB2 FF8C 8154 80FF 008D"
		$"8054 04FF 8C54 8CFF 8054 008D 80FF 00D8"
		$"8854 00D8 81FF 008D 8654 008D 83FF 008D"
		$"8454 008D 85FF 01D8 8D80 5401 8DD8 A2FF"
		$"91FF 01D8 8D80 5401 8DD8 85FF 008D 8454"
		$"008D 83FF 008D 8654 008D 81FF 00D8 8854"
		$"00D8 80FF 008D 8054 04FF 8C54 8CFF 8054"
		$"008D 80FF 8154 048C FFB2 FF8C 8154 80FF"
		$"8254 02B2 FFB2 8254 80FF 8154 048C FFB2"
		$"FF8C 8154 80FF 008D 8054 04FF 8C54 8CFF"
		$"8054 008D 80FF 00D8 8854 00D8 81FF 008D"
		$"8654 008D 83FF 008D 8454 008D 85FF 01D8"
		$"8D80 5401 8DD8 A2FF",
		/* [3] */
		's8mk',
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 1B50 B9B9 B950 1B00 0000 0000"
		$"0000 0050 B9B9 B9B9 B9B9 B950 0000 0000"
		$"0000 50B9 B9B9 B9B9 B9B9 B9B9 5000 0000"
		$"001B B9B9 B9B9 B9B9 B9B9 B9B9 B91B 0000"
		$"0050 B9B9 B9B9 B9B9 B9B9 B9B9 B950 0000"
		$"00B9 B9B9 B9B9 B9B9 B9B9 B9B9 B9B9 0000"
		$"00B9 B9B9 B9B9 B9B9 B9B9 B9B9 B9B9 0000"
		$"00B9 B9B9 B9B9 B9B9 B9B9 B9B9 B9B9 0000"
		$"0050 B9B9 B9B9 B9B9 B9B9 B9B9 B950 0000"
		$"001B B9B9 B9B9 B9B9 B9B9 B9B9 B91B 0000"
		$"0000 50B9 B9B9 B9B9 B9B9 B9B9 5000 0000"
		$"0000 0050 B9B9 B9B9 B9B9 B950 0000 0000"
		$"0000 0000 1B50 B9B9 B950 1B00 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
		$"0000 0000 0000 0000 0000 0000 0000 0000"
	}
};

data 'µMWC' (32000) {
};
