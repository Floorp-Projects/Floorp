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

/*
 * The following source code is part of the Microline Widget Library.
 * The Microline widget library is made available to Mozilla developers
 * under the Netscape Public License (NPL) by Neuron Data.  To learn
 * more about Neuron Data, please visit the Neuron Data Home Page at
 * http://www.neurondata.com.
 */


#include <Xm/Xm.h>
#include <XmL/Progress.h>

Boolean compute();

Widget progress;

main(argc, argv)
int argc;
char *argv[];
{
	XtAppContext app;
	Widget shell;

	shell =  XtAppInitialize(&app, "Prog2", NULL, 0,
		&argc, argv, NULL, NULL, 0);

	progress = XtVaCreateManagedWidget("progress",
		xmlProgressWidgetClass, shell,
		XmNshowTime, True,
		XtVaTypedArg, XmNbackground, XmRString, "white", 6,
		XtVaTypedArg, XmNforeground, XmRString, "#800000", 8,
		XmNwidth, 300,
		XmNheight, 25,
		NULL);

	XtAppAddWorkProc(app, compute, NULL);
	XtRealizeWidget(shell);
	XtAppMainLoop(app);
}

Boolean compute(clientData)
XtPointer clientData;
{
	int i;

	XtVaSetValues(progress,
		XmNvalue, 0,
		XmNcompleteValue, 7,
		NULL);
	for (i = 0; i < 7; i++)
	{
		XtVaSetValues(progress,
			XmNvalue, i,
			NULL);

		/* perform processing */
		sleep(1);
	}
	XtVaSetValues(progress,
		XmNvalue, i,
		NULL);
	return(FALSE);
}
