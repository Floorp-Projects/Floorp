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

/*
	The one and only job of this project is to provide dummy
	libraries for the main project, when you are doing a build
	with some components turned off.
	
	For example, if you compile MOZ_LITE, there is no editor.
	But to avoid having to have multiple targets in the final
	project, we just provide a stub library from here, to
	keep the IDE happy.
	
	There will be one target for each dummy library that we
	need.
*/

void NuthinToDo(void) {}
