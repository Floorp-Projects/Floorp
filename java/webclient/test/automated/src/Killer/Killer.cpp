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
#define ERROR_DIALOG_KW_1 "java"
#define ERROR_DIALOG_KW_2 "Error"


#define IGNORE_BUTTON_TITLE "&Ignore"
#define IGNORE_BUTTON_ID 5

#define OK_BUTTON_TITLE "OK"
#define OK_BUTTON_ID 1

void safeAppendToLog(char* str);
char * logFile = NULL;

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM out) {
	char title[1024];
	GetWindowText(hwnd, title, 1024);
	if ((!strcmp(title, OK_BUTTON_TITLE))||(!strcmp(title, IGNORE_BUTTON_TITLE))) {
		*((HWND*)out) = hwnd;
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK EnumWindowsProc(  HWND hwnd, LPARAM lParam) {
	char title[1024];
    char msg[1200];
	GetWindowText(hwnd, title, 1024);
	if (strstr(title, ERROR_DIALOG_KW_1) && strstr(title, ERROR_DIALOG_KW_2)) {
        memset(msg,0,1200);
        sprintf(msg,"Found Error window:\"%s\" /n",title);
        safeAppendToLog(msg);
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
		SendMessage(hwnd, WM_COMMAND, wp, lp);
		return FALSE;
	}
    if (strstr(title, ASSERTION_DIALOG_KW_1) && strstr(title, ASSERTION_DIALOG_KW_2)) {
        memset(msg,0,1200);
        sprintf(msg,"Found Assertion window:\"%s\"/n",title);
        safeAppendToLog(msg);
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
		SendMessage(hwnd, WM_COMMAND, wp, lp);
		return FALSE;
	}
	return TRUE;
}
void usage() {
    printf("Usage: Killer.exe <logFile>");
    exit(-1);
}

void safeAppendToLog(char* str) {
    FILE* log = NULL;
    if((log = fopen (logFile,"a+")) == NULL) {
        printf("Can't open log file \"%s\"",logFile);
        exit(-2);
    }
    fprintf(log,"KILLER: %s",str);
    if(fclose(log)) {
        printf("Can't close log file \"%s\"",logFile);
        exit(-2);

    }
}

void main(int argc, char *argv[]) {
    if(argc != 2) {
        usage();
    }
    logFile = argv[1];
	EnumWindows(EnumWindowsProc, 0);
}





