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

#ifndef _JULIAN_FORMS_H
#define _JULIAN_FORMS_H

#include "julianform.h"
#include "julnstr.h"

class JULIAN_PUBLIC JulianServerProxy 
{
public:
	ICalComponent*	ic;

					JulianServerProxy() {};
	virtual			~JulianServerProxy() {};
	ICalComponent*	getByUid(char *uid) { return ic; };

	void			setICal(ICalComponent* i) { ic = i; };

};

#include "formFactory.h"

typedef struct
{
	char*	type;
	char*	data;
} form_data_combo;

class JULIAN_PUBLIC JulianForm
{
private:

	JulianString					htmlForm;
	char*							mimedata;
	char							buttonLabel[2048];
	NSCalendar*						imipCal;
	pJulian_Form_Callback_Struct	JulianForm_CallBacks;
    /* added 7-7-98 */
    static XP_Bool                  ms_bFoundNLSDataDirectory;
    JulianFormFactory*              jff;       

public:
			JulianForm();
	virtual ~JulianForm();

    int32                           refcount; /* Who's looking at this */
	int32							formDataCount; /* number of pointers in formData */
    PRMonitor *                     my_monitor;

	/* number of possible pointers in formData */
	#define formDataIndex 10
	form_data_combo					formData[formDataIndex]; /* a pointer to an array of pointers that point to a type/data string */

	char*							contextName;

    XP_Bool                         StartHTML();
	char*							getHTMLForm(XP_Bool Want_Detail, NET_StreamClass *this_stream = nil);
	void							setMimeData(char *mimedata);
	void							setCallbacks(pJulian_Form_Callback_Struct callBacks) { JulianForm_CallBacks = callBacks; };
	pJulian_Form_Callback_Struct	getCallbacks() { return JulianForm_CallBacks; };

    void setCalendar(NSCalendar* newCal)		{ imipCal = newCal; }
    NSCalendar * getCalendar()		{ return imipCal; }
    JulianPtrArray * getEvents()	{ if (imipCal) { return imipCal->getEvents(); } else return 0; }

    /* added 7-7-98 */
    static void setFoundNLSDataDirectory(XP_Bool bFound) { ms_bFoundNLSDataDirectory = bFound; }
    static XP_Bool isFoundNLSDataDirectory() { return ms_bFoundNLSDataDirectory; }

	MWContext*		getContext()	{ return (*JulianForm_CallBacks->FindNamedContextInList)((*JulianForm_CallBacks->FindSomeContext)(), contextName); }

	XP_Bool			hasComment()	{ return getComment() != nil; } 
	char*			getComment();
	char*			getDelTo();

	char*			getLabel()		{ return buttonLabel; }
	void			setLabel(char *newlabel) { if (newlabel) XP_STRCPY(buttonLabel, newlabel); if (getCallbacks() && getCallbacks()->PlusToSpace) (*getCallbacks()->PlusToSpace)(buttonLabel); }
};

XP_BEGIN_PROTOS

JulianForm*	jform_CreateNewForm	(char *calendar_mime_data, pJulian_Form_Callback_Struct callbacks, XP_Bool bFoundNLSDataDirectory);
void		jform_DeleteForm	(JulianForm *jf);
char*		jform_GetForm		(JulianForm *jf);
void		jform_CallBack		(JulianForm *jf, char *type);

XP_END_PROTOS
#endif

