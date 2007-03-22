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

#include <Types.r>

resource 'DITL' (1550, "Save format panel", purgeable) {
	{	/* array DITLarray: 2 elements */
		/* [1] */
		{16, 12, 32, 75},
		StaticText {
			disabled,
			"Save as:"
		},
		/* [2] */
		{12, 68, 36, 236},
		Control {
			enabled,
			1550
		}
	}
};

resource 'MENU' (1550) {
	1550,
	textMenuProc,
	allEnabled,
	enabled,
	"Save Format",
	{	/* array: 3 elements */
		/* [1] */
		"Text Only", noIcon, noKey, noMark, plain,
		/* [2] */
		"HTML", noIcon, noKey, noMark, plain,
		/* [3] */
		"HTML Complete", noIcon, noKey, noMark, plain
	}
};

resource 'CNTL' (1550, "Save format popup", purgeable) {
	{12, 68, 36, 236},
	0,
	visible,
	0,
	1550,
	401,
	0,
	""
};
