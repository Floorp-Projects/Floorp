/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "macstdlibextras.h"


#include <MacTypes.h>
#include <Memory.h>

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <Quickdraw.h>
#include <TextServices.h>
#include <Movies.h>

int strcmpcore(const char*, const char*, int, int);


/*
size_t strlen(const char *source)
{
	size_t currentLength = 0;
	
	if (source == NULL)
		return currentLength;
			
	while (*source++ != '\0')
		currentLength++;
		
	return currentLength;
}
*/

int strcmpcore(const char *str1, const char *str2, int caseSensitive, int length)
{
	char 	currentChar1, currentChar2;
	int compareLength = (length >= 0);

	while (1) {
	
		if (compareLength) {

			if ( length <= 0 )
				return 0;
			length--;

		}

		currentChar1 = *str1;
		currentChar2 = *str2;
		
		if (!caseSensitive) {
			
			if ((currentChar1 >= 'a') && (currentChar1 <= 'z'))
				currentChar1 += ('A' - 'a');
		
			if ((currentChar2 >= 'a') && (currentChar2 <= 'z'))
				currentChar2 += ('A' - 'a');
				
		
		}
	
		if (currentChar1 == '\0')
			break;
	
		if (currentChar1 != currentChar2)
			return currentChar1 - currentChar2;
			
		str1++;
		str2++;
	
	}
	
	return currentChar1 - currentChar2;
}

/*
int strcmp(const char *str1, const char *str2)
{
	return strcmpcore(str1, str2, true, -1);
}
*/

int strcasecmp(const char *str1, const char *str2)
{
	/* This doesnÕt really belong here; but since it uses strcmpcore, weÕll keep it. */
	return strcmpcore(str1, str2, false, -1);
}


int strncasecmp(const char *str1, const char *str2, int length)
{
	/* This doesnÕt really belong here; but since it uses strcmpcore, weÕll keep it. */
	return strcmpcore(str1, str2, false, length);
}


char *strdup(const char *source)
{
	char 	*newAllocation;
	size_t	stringLength;

/*
#ifdef DEBUG
	PR_ASSERT(source);
#endif
*/
	
	stringLength = strlen(source) + 1;
	
	/* since this gets freed by an XP_FREE, it must do an XP_ALLOC */
	/* newAllocation = (char *)XP_ALLOC(stringLength); */
	newAllocation = (char *)malloc(stringLength);
	if (newAllocation == NULL)
		return NULL;
	BlockMoveData(source, newAllocation, stringLength);
	return newAllocation;
}

#pragma mark -


void InitializeMacToolbox(void)
{
	// once only, macintosh specific initialization
	static Boolean alreadyInitialized = false;
	if (!alreadyInitialized)
	{
		long CMMavail = 0;

		alreadyInitialized = true;
#if !TARGET_CARBON
// pinkerton - don't need to init toolbox under Carbon. They finally do that for us!
		InitGraf(&qd.thePort);
		InitFonts();
		InitWindows();
		InitMenus();
		TEInit();
		InitDialogs(0);
		InitCursor();
		Gestalt(gestaltContextualMenuAttr, &CMMavail);
		if ((CMMavail == gestaltContextualMenuTrapAvailable) &&
		    ((long)InitContextualMenus != kUnresolvedCFragSymbolAddress))
		  InitContextualMenus();
		
		InitTSMAwareApplication();
#endif
	    
		// init QuickTime if we have it
		if ((long)EnterMovies != kUnresolvedCFragSymbolAddress)
			EnterMovies();

#if DEBUG
		InitializeSIOUX(false);
#endif
	}
}

#pragma mark -

#if DEBUG

#include <SIOUX.h>

void InitializeSIOUX(unsigned char isStandAlone)
{

	SIOUXSettings.initializeTB = isStandAlone;
	SIOUXSettings.standalone = isStandAlone;
	SIOUXSettings.setupmenus = isStandAlone;
	SIOUXSettings.autocloseonquit = true;
	SIOUXSettings.asktosaveonclose = false;
	SIOUXSettings.showstatusline = false;
	
	if (isStandAlone)
	{
		SIOUXSettings.toppixel = 42;
		SIOUXSettings.leftpixel = 6;
		SIOUXSettings.rows = 40;
		SIOUXSettings.columns = 82;
	}
	else
	{
		SIOUXSettings.toppixel = 480;
		SIOUXSettings.leftpixel = 4;
		SIOUXSettings.rows = 20;
		SIOUXSettings.columns = 100;
	}
	
	//InstallConsole();
}


Boolean IsSIOUXWindow(WindowPtr inWindow)
{
  return SIOUXIsAppWindow(inWindow);
}

#endif
