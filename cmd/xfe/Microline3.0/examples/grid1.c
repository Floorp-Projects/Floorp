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

/*
 * The following source code is part of the Microline Widget Library.
 * The Microline widget library is made available to Mozilla developers
 * under the Netscape Public License (NPL) by Neuron Data.  To learn
 * more about Neuron Data, please visit the Neuron Data Home Page at
 * http://www.neurondata.com.
 */


#include <Xm/Xm.h>
#include <XmL/Grid.h>

static char *data = 
"Europe|CD-ROM|$29\n\
Yugoslovia|Floppy|$39\n\
North America|Tape|$29\n\
South America|CD-ROM|$49\n\
Japan|Tape|$49\n\
Russia|Floppy|$49\n\
Poland|CD-ROM|$39\n\
Norway|CD-ROM|$29\n\
England|Tape|$49\n\
Jordan|CD-ROM|$39";

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell, grid;

	shell =  XtAppInitialize(&app, "Grid1", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	grid = XtVaCreateManagedWidget("grid",
		xmlGridWidgetClass, shell,
		XmNrows, 10,
		XmNvisibleRows, 7,
		XmNcolumns, 3,
		XmNsimpleWidths, "20c 8c 8c",
		XmNhorizontalSizePolicy, XmVARIABLE,
		NULL);
	XmLGridSetStrings(grid, data);

	XtRealizeWidget(shell);

	XtAppMainLoop(app);
}

