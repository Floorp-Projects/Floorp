/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
//cl killer.cpp /link user32.lib

#include <windows.h>
#include <stdio.h>


//Define keywords to determine Assertion window
#define ASSERTION_DIALOG_KW_1 "nsDebug"
#define ASSERTION_DIALOG_KW_2 "Assertion"

//Define keywords to determine ERROR window
#define ERROR_DIALOG_KW_1 "mozilla"
#define ERROR_DIALOG_KW_2 "Error"


#define IGNORE_BUTTON_TITLE "&Ignore"
#define IGNORE_BUTTON_ID 5

#define OK_BUTTON_TITLE "OK"
#define OK_BUTTON_ID 1

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM out) {
	char title[1024];
	GetWindowText(hwnd, title, 1024);
	//printf("--Child Window: %x -> %s\n", hwnd, title);
	if ((!strcmp(title, OK_BUTTON_TITLE))||(!strcmp(title, IGNORE_BUTTON_TITLE))) {
        //printf("Found window:  %x -> %s\n", hwnd, title);
		*((HWND*)out) = hwnd;
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK EnumWindowsProc(  HWND hwnd, LPARAM lParam) {
	char title[1024];
	GetWindowText(hwnd, title, 1024);
	//printf("++Window: %x -> %s\n", hwnd, title);
	if (strstr(title, ERROR_DIALOG_KW_1) && strstr(title, ERROR_DIALOG_KW_2)) {
        printf("Found Error window:\\\\%s// /n",title);
		DWORD lp = 0, wp = 0;
		HWND ok;
		//really we can ommit this step but ...
		EnumChildWindows(hwnd, EnumChildProc, (LPARAM)(&ok));
		if (!ok) {
			printf("OK button not found !\n");
			return FALSE;
		}
		lp = (unsigned long)ok;
		wp = OK_BUTTON_ID;
		wp = wp | (BN_CLICKED << 16);
		//printf("COMMAND: %d (hwnd), %d (code), %d(id)\n", lp, HIWORD(wp), LOWORD(wp));
		SendMessage(hwnd, WM_COMMAND, wp, lp);
		return FALSE;
	}
    if (strstr(title, ASSERTION_DIALOG_KW_1) && strstr(title, ASSERTION_DIALOG_KW_2)) {
        printf("Found Assertion window:\\\\%s// /n",title);
		DWORD lp = 0, wp = 0;
		HWND ok;
		//really we can ommit this step but ...
		EnumChildWindows(hwnd, EnumChildProc, (LPARAM)(&ok));
		if (!ok) {
			printf("OK button not found !\n");
			return FALSE;
		}
		lp = (unsigned long)ok;
		wp = IGNORE_BUTTON_ID;
		wp = wp | (BN_CLICKED << 16);
		//printf("COMMAND: %d (hwnd), %d (code), %d(id)\n", lp, HIWORD(wp), LOWORD(wp));
		SendMessage(hwnd, WM_COMMAND, wp, lp);
		return FALSE;
	}
	return TRUE;
}

void main() {
	EnumWindows(EnumWindowsProc, 0);
}
