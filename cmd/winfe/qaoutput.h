/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
// qaoutput.h
// API Testing

#ifndef _qaoutput_h_
#define _qaoutput_h_

extern char *TestcaseOutputFilename;

int AssignFilename(const char* newfilename);
int DeleteFilename();

int QAOpenOutputFile();
int QACloseOutputFile();
int QAAddToOutputFile(char* s);

#endif // _qaoutput_h_
