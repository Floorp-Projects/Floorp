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

data 'CURS' (1120, purgeable) {
	$"0000 0280 0280 0280 0280 1290 3298 7EFC"            /* ...€.€.€.€.2˜~ü */
	$"3298 1290 0280 0280 0280 0280 0000 0000"            /* 2˜..€.€.€.€.... */
	$"0000 07C0 07C0 07C0 1FF0 3FF8 7FFC FFFE"            /* ...À.À.À.ğ?ø.üÿş */
	$"7FFC 3FF8 1FF0 07C0 07C0 07C0 0000 0000"            /* .ü?ø.ğ.À.À.À.... */
	$"0007 0007"                                          /* .... */
};

data 'CURS' (1136, purgeable) {
	$"0000 0100 0380 07C0 0100 0100 7FFC 0000"            /* .....€.À.....ü.. */
	$"7FFC 0100 0100 07C0 0380 0100 0000 0000"            /* .ü.....À.€...... */
	$"0100 0380 07C0 0FE0 0FE0 7FFC 7FFC 7FFC"            /* ...€.À.à.à.ü.ü.ü */
	$"7FFC 7FFC 0FE0 0FE0 07C0 0380 0100 0000"            /* .ü.ü.à.à.À.€.... */
	$"0007 0007"                                          /* .... */
};

