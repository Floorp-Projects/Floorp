/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Carbon/Carbon.r>
#include <HIToolbox/ControlDefinitions.r>

#include "PrintDialogPDE.h"


resource 'CNTL' (ePrintSelectionControlID, "Print Selection Only", purgeable) {
{110, 72, 125, 250},							/* Rect Top, Left, Bottom, Right */
 0, 															/* initial value */
 invisible,												/* attributes */
 1,																/* max */
 0,																/* min */
 kControlCheckBoxAutoToggleProc,	/* Type */
 0,																/* refcon */
 "Print Selection"								/* Title */
};

resource 'CNTL' (ePrintFrameAsIsControlID, "Print Frames As Is", purgeable) {
{130, 72, 145, 250},						
 0, 								
 invisible,						
 1,							
 0,							
 kControlRadioButtonAutoToggleProc,			
 0,								
 "As laid out on the screen"		
};


resource 'CNTL' (ePrintSelectedFrameControlID, "Print Selected Frame", purgeable) {
{150, 72, 165, 250},		
 0, 								
 invisible,	
 1,								
 0,		
 kControlRadioButtonAutoToggleProc,			
 0,		
 "The selected frame"					
};


resource 'CNTL' (ePrintFramesSeparatelyControlID, "Print Frames Separately", purgeable) {
{170, 72, 185, 250},					
 0, 							
 invisible,						
 1,							
 0,								
 kControlRadioButtonAutoToggleProc,		
 0,							
 "Each frame separately"		
};


resource 'CNTL' (eShrinkToFiControlID, "Print Shrink To Fit", purgeable) {
{190, 72, 205, 250},						
 0, 				
 invisible,		
 1,							
 0,							
 kControlCheckBoxAutoToggleProc,		
 0,							
 "Shrink To Fit Page Width"		
};

resource 'CNTL' (eRadioGroupControlID, "Radio Group", purgeable) {
{130, 72, 205, 250},						
 0, 				
 visible,		
 1,							
 0,							
 kControlRadioGroupProc,		
 0,							
 "RadioGroup"		
};

