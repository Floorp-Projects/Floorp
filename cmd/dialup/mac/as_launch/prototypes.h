/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#ifdef __cplusplus
extern "C" {
#endif

void main(void);

void showError(short errStrIndex);

extern Boolean isAppRunning(OSType theSig,ProcessSerialNumber *thePSN, ProcessInfoRec *theProcInfo);

extern int strlen(char *p);

extern void modifyComponentDockPref(FSSpecPtr appFSSpecPtr);

extern Boolean checkVERS(FSSpecPtr appFSSpecPtr);

pascal Boolean netscapeFileFilter(CInfoPBPtr pb, void *data);

OSErr FindAppInCurrentFolder(OSType theSig, FSSpecPtr theFSSpecPtr);

OSErr FindApp(OSType theSig, FSSpecPtr theFSSpecPtr);

OSErr LaunchApp(FSSpecPtr theFSSpecPtr, AEDesc *launchDesc);

extern OSErr QuitApp(ProcessSerialNumber *thePSN);

#ifdef __cplusplus
}
#endif
