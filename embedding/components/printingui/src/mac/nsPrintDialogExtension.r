/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the guts of the find algorithm.
 *
 * The Initial Developer of the Original Code is Akkana Peck.
 *
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

#include <Types.r>

resource 'DITL' (128) {
	{	/* array DITLarray: 6 elements */
		/* [1] */
		{16, 32, 33, 178},
		CheckBox {
			enabled,
			"Print Selection Only"
		},
		/* [2] */
		{68, 32, 86, 186},
		RadioButton {
			enabled,
			"As laid out on screen"
		},
		/* [3] */
		{87, 32, 104, 189},
		RadioButton {
			enabled,
			"The selected frame"
		},
		/* [4] */
		{105, 32, 123, 202},
		RadioButton {
			enabled,
			"Each frame separately"
		},
		/* [5] */
		{54, 24, 131, 210},
		UserItem {
			disabled
		},
		/* [6] */
		{45, 34, 62, 118},
		StaticText {
			disabled,
			"Print Frames"
		}
	}
};
