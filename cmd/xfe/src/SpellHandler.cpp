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
   SpellHandler.cpp -- class definition for the Spell Handler class
   Created: Richard Hess <rhess@netscape.com>, 12-May-97
 */


#include "SpellHandler.h"

#include "spellchk.h"

#include "xpgetstr.h"
#include "fe_proto.h"
#include "edt.h"
#include "xeditor.h"

#include "prefs.h"
#include "prefapi.h"
#include "prlink.h"

#include <Xm/Text.h>
#include <DtWidgets/ComboBoxP.h>
#include <Xfe/Xfe.h>

extern "C" {
	void fe_GetProgramDirectory(char *path, int len);
}

static XFE_SpellHandler  *xfe_spellHandler = NULL;

// Preference strings
static char *LanguagePref = "spellchecker.default_language";
static char *DialectPref  = "spellchecker.default_dialect";

extern int NO_SPELL_SHLIB_FOUND;

// WARNING... [ be VERY careful with this index stuff ]
//
#define XSP_NULL       0
#define XSP_REPLACE    1
#define XSP_REPLACEALL 2
#define XSP_CHECK      3
#define XSP_IGNORE     4
#define XSP_IGNOREALL  5
#define XSP_LEARN      6
#define XSP_STOP       7
//
// IMPORTANT:  if you add to this list you need to update the value of
//             XSP_CACHE_SIZE in SpellHandler.h to reflect the change
//             in the size of the tag cache...
//


static void
xfe_spell_focus_in_cb (Widget , XtPointer closure, XtPointer )
{
	struct xfe_spell_data *data = (struct xfe_spell_data *) closure;
	char *tmp = XmTextFieldGetString(data->text);
        
	if (data->inList) {
		data->inList = FALSE;
		XmListDeselectAllItems(data->list);

		if (XP_STRCMP( tmp, data->xWord ) == 0) {
			XtSetSensitive(data->replace, FALSE);
			XtSetSensitive(data->replaceAll, FALSE);
			XtSetSensitive(data->check, FALSE);

			XtVaSetValues(data->dialog,  XmNdefaultButton, data->ignore, 0);
			XtVaSetValues(data->replace, XmNshowAsDefault, False, 0);
			XtVaSetValues(data->ignore,  XmNshowAsDefault, True, 0);
			XmProcessTraversal(data->ignore, XmTRAVERSE_CURRENT);
		} else {
			XtSetSensitive(data->replace, True);
			XtSetSensitive(data->replaceAll, True);
			XtSetSensitive(data->check, TRUE);

			XtVaSetValues(data->dialog,  XmNdefaultButton, data->replace, 0);
			XtVaSetValues(data->ignore,  XmNshowAsDefault, False, 0);
			XtVaSetValues(data->replace, XmNshowAsDefault, True, 0);
			XmProcessTraversal(data->replace, XmTRAVERSE_CURRENT);
		}
	}

	if (data->nuText)
		XP_FREE(data->nuText);
	
	data->nuText = XP_STRDUP(tmp);
	
	if (tmp)
		XtFree(tmp);
}

static void
xfe_spell_focus_out_cb (Widget , XtPointer closure, XtPointer )
{
	struct xfe_spell_data *data = (struct xfe_spell_data *) closure;
        
	if (data->inList) {
		XtSetSensitive(data->check, False);
	}
}

static void
xfe_spell_text_cb (Widget , XtPointer closure, XtPointer )
{
	struct xfe_spell_data *data = (struct xfe_spell_data *) closure;
	char *tmp = XmTextFieldGetString(data->text);
        
	if (data->xWord != NULL) {
		if (XP_STRCMP( tmp, data->xWord ) == 0) {
			XtSetSensitive(data->replace, FALSE);
			XtSetSensitive(data->replaceAll, FALSE);
			XtSetSensitive(data->check, FALSE);

			XtVaSetValues(data->dialog,  XmNdefaultButton, data->ignore, 0);
			XtVaSetValues(data->replace, XmNshowAsDefault, False, 0);
			XtVaSetValues(data->ignore,  XmNshowAsDefault, True, 0);
		} else {
			XtSetSensitive(data->replace, True);
			XtSetSensitive(data->replaceAll, True);
			XtSetSensitive(data->check, TRUE);

			XtVaSetValues(data->dialog,  XmNdefaultButton, data->replace, 0);
			XtVaSetValues(data->ignore,  XmNshowAsDefault, False, 0);
			XtVaSetValues(data->replace, XmNshowAsDefault, True, 0);
		}
	}

	if (data->nuText)
		XP_FREE(data->nuText);
	
	data->nuText = XP_STRDUP(tmp);
	
	if (tmp)
		XtFree(tmp);
}

static void
xfe_spell_list_cb (Widget , XtPointer closure, XtPointer call_data)
{
	struct xfe_spell_data *data = (struct xfe_spell_data *) closure;
	XmListCallbackStruct *cdata = (XmListCallbackStruct *) call_data;
	ISpellChecker        *spell = data->spell;
	char tmp[125];

	int  i = cdata->item_position - 1;

	if (!data->inList) {
		XtSetSensitive(data->replace, True);
		XtSetSensitive(data->replaceAll, True);

		XtSetSensitive(data->check, False);

		XtVaSetValues(data->dialog,  XmNdefaultButton, data->replace, 0);
		XtVaSetValues(data->ignore,  XmNshowAsDefault, False, 0);
		XtVaSetValues(data->replace, XmNshowAsDefault, True, 0);
	}

	data->inList = TRUE;

    if (spell)
        spell->GetAlternative(i, tmp, sizeof(tmp));

	if (data->nuText)
		XP_FREE(data->nuText);

	data->nuText = XP_STRDUP(tmp);
}
	
static void
xfe_spell_combo_cb (Widget , XtPointer closure, XtPointer call_data)
{
	struct xfe_spell_data    *data = (struct xfe_spell_data *) closure;
	DtComboBoxCallbackStruct *info = (DtComboBoxCallbackStruct*)call_data;
	int i = info->item_position;
	int NewLanguage = data->langTags[i].language;
	int NewDialect  = data->langTags[i].dialect;

	PREF_SetIntPref(LanguagePref, NewLanguage);
	PREF_SetIntPref(DialectPref, NewDialect);

	if (data->langIndx != i) {
		data->langIndx = i;

		xfe_spellHandler->ReprocessDocument();
	}
}
	
static void
xfe_spell_doit_cb (Widget , XtPointer closure, XtPointer )
{
	struct xfe_spell_tag  *tag  = (struct xfe_spell_tag *) closure;
	
	xfe_spellHandler->ProcessError(tag);
	xfe_spellHandler->UpdateGUI();
}
	
static void
xfe_spell_destroy_cb (Widget , XtPointer , XtPointer )
{
	xfe_spellHandler->DialogDestroyed();
}
	

// -----------------------------------------------------------[ SpellHandler ]


/*static*/ PRLibrary* XFE_SpellHandler::m_spellCheckerLib = NULL;
/*static*/ XP_Bool XFE_SpellHandler::m_triedToLoad = FALSE;

XFE_SpellHandler::XFE_SpellHandler(MWContext* )
{
	int i = 0;

	m_triedToLoad = FALSE;
	m_active = FALSE;

	m_data.spell     = NULL;
	m_data.interface = NULL;
	m_data.dialog  = NULL;
	m_data.list    = NULL;
	m_data.combo   = NULL;
	m_data.replace = NULL;
	m_data.replaceAll = NULL;
	m_data.check      = NULL;
	m_data.ignore     = NULL;
	m_data.ignoreAll  = NULL;
	m_data.learn      = NULL;
	m_data.stop       = NULL;
	m_data.status  = NULL;
	m_data.text    = NULL;
	m_data.nuText  = NULL;
	m_data.langTags  = NULL;
	m_data.langCount = 0;
	m_data.langIndx  = 0;
	m_data.inList  = TRUE;
	m_data.inCheck = FALSE;
	m_data.isOk    = FALSE;
	m_data.isDone  = FALSE;

	for (i = 0 ; i < XSP_CACHE_SIZE ; i++) {
		//
		// WARNING... [ be VERY careful with this index stuff ]
		//
		m_tags[i].action = i;
		m_tags[i].data = &m_data;
	}

	// NOTE:  we try to create the spell checker backend in order to 
	//        determine it's availability... [ m_active ]
	//
	initSpellChecker();
	nukeSpellChecker();
}

XFE_SpellHandler::~XFE_SpellHandler()
{
	if (m_data.interface != NULL)
		delete m_data.interface;

	if (m_data.spell != NULL)
		nukeSpellChecker();

	if (m_data.langTags != NULL)
		XP_FREE(m_data.langTags);

	m_active       = FALSE;

	m_data.spell     = NULL;
	m_data.interface = NULL;
	m_data.dialog  = NULL;
	m_data.list    = NULL;
	m_data.combo   = NULL;
	m_data.replace = NULL;
	m_data.replaceAll = NULL;
	m_data.check      = NULL;
	m_data.ignore     = NULL;
	m_data.ignoreAll  = NULL;
	m_data.learn      = NULL;
	m_data.stop       = NULL;
	m_data.status  = NULL;
	m_data.text    = NULL;
	m_data.langTags  = NULL;
	m_data.langCount = 0;
	m_data.langIndx  = 0;
	m_data.inList  = TRUE;
	m_data.inCheck = FALSE;
	m_data.isOk    = FALSE;
	m_data.isDone  = FALSE;
}

void
XFE_SpellHandler::initSpellChecker()
{
    // Let people disable spell checking in case they have NFS-related hangs:
    XP_Bool disable_spell_checker;
    PREF_GetBoolPref("editor.disable_spell_checker", &disable_spell_checker);
    if (disable_spell_checker) {
        m_active = FALSE;
        return;
    }

	if (m_data.spell != NULL)
        return;     // already initialized

    if (!m_triedToLoad)
    {
        m_triedToLoad = TRUE;
        //const char* libPath = PR_GetLibraryPath();
        //PR_SetLibraryPath("/usr/local/netscape/");

        char* libname = "libspellchk.so";
        m_spellCheckerLib = PR_LoadLibrary(libname);

        // Set path back to original path
        //PR_SetLibraryPath(libPath);

        if (m_spellCheckerLib == NULL)
        {
#ifdef DEBUG_akkana
            printf("Couldn't find library NSSpellChecker\n");
#endif
            m_active = 0;
            return;
        }
        m_active = TRUE;
    }

    if (m_data.spell == NULL && m_active)
    {
        typedef ISpellChecker*(*sc_create_func)();
        sc_create_func sc_createProc =
            (sc_create_func)PR_FindSymbol(m_spellCheckerLib, "SC_Create");
        if (sc_createProc == NULL)
        {
#ifdef DEBUG_akkana
            printf("Couldn't find symbol SC_Create\n");
#endif
            m_active = FALSE;
            return;
        }

        m_data.spell = sc_createProc();
    }

	if (m_data.spell == NULL)
    {
#ifdef DEBUG_akkana
        printf("Couldn't initialize spellchecker\n");
#endif
        m_active = FALSE;
        return;
    }

    int32 Language = 0;
    int32 Dialect = 0;

    // First see if any language preferences have been set
    PREF_GetIntPref(LanguagePref, &Language);
    PREF_GetIntPref(DialectPref, &Dialect);

    char *spellDir = getSpellCheckerDir();
    char *personal = getPersonalDicPath();

#ifdef DEBUG_spellcheck
    fprintf(stderr, "spellDir::[ %s ]\n", spellDir);
    fprintf(stderr, "personal::[ %s ]\n", personal);
#endif

    if (m_data.spell->Initialize(Language, Dialect, 
                                 spellDir, personal)) {
        m_active = FALSE;
    }
    else {
        m_active = TRUE;
    }
}

void
XFE_SpellHandler::nukeSpellChecker()
{
	if (m_data.spell != NULL
        && m_spellCheckerLib != NULL
        )
    {
        typedef void (*sc_destroy_func)(ISpellChecker*);
        sc_destroy_func sc_destroyProc =
            (sc_destroy_func)PR_FindSymbol(m_spellCheckerLib, "SC_Destroy");
        if (sc_destroyProc != NULL)
            sc_destroyProc(m_data.spell);
		m_data.spell = NULL;
	}
}

char *
XFE_SpellHandler::getSpellCheckerDir()
{
	char *home = NULL;
	char *mozilla_home = NULL;
	char buf[MAXPATHLEN];


	if (home = getenv("HOME")) {
		XP_MEMSET(buf, '\0', sizeof(buf));

		// Form "$HOME/.netscape/spell/" into buf...
		//
		XP_STRNCPY_SAFE(buf, home, sizeof(buf)-1);
		XP_STRNCAT_SAFE(buf, "/.netscape/spell/",
						sizeof(buf)-1 - XP_STRLEN(buf));
		buf[sizeof(buf)-1] = '\0';
		
		if (fe_isDir(buf)) {
			return XP_STRDUP(buf);
		}
	}

	if (mozilla_home = getenv("MOZILLA_HOME")) {
		XP_MEMSET(buf, '\0', sizeof(buf));

		// Form "$MOZILLA_HOME/spell/" into buf...
		//
		XP_STRNCPY_SAFE(buf, mozilla_home, sizeof(buf)-1);
		XP_STRNCAT_SAFE(buf, "/spell/", 
						sizeof(buf)-1 - XP_STRLEN(buf));
		buf[sizeof(buf)-1] = '\0';

		if (fe_isDir(buf)) {
			return XP_STRDUP(buf);
		}
	}

	XP_MEMSET(buf, '\0', sizeof(buf));
	//
	// Form "<program-dir>/spell/" into buf...
	//
	fe_GetProgramDirectory(buf, sizeof(buf)-1);
	if (XP_STRLEN(buf) > 0) {
		// WARNING... [ the program dir already has a trailing slash ]
		//
		XP_STRNCAT_SAFE(buf, "spell/",
						sizeof(buf)-1 - XP_STRLEN(buf));
		buf[sizeof(buf)-1] = '\0';

		if (fe_isDir(buf)) {
			return XP_STRDUP(buf);
		}
	}

	// last chance, look for "/usr/local/netscape/spell/"...
	//
	if (fe_isDir("/usr/local/netscape/spell/")) {
		return XP_STRDUP("/usr/local/netscape/spell/");
	}

	// NOTE:  punt on the directory...
	//
	return XP_STRDUP(".");
}

char *
XFE_SpellHandler::getPersonalDicPath()
{
	char buf[MAXPATHLEN];
	char *home = NULL;

	if (home = getenv("HOME"))
	{
		/* Form "$HOME/.netscape/custom.dic" into buf */
		XP_STRNCPY_SAFE(buf, home, sizeof(buf)-1);
		XP_STRNCAT_SAFE(buf, "/.netscape/custom.dic",
						sizeof(buf)-1 - XP_STRLEN(buf));
		buf[sizeof(buf)-1] = '\0';
	}

	return XP_STRDUP(buf);
}

XP_Bool
XFE_SpellHandler::IsActive()
{
	return m_active;
}

XP_Bool
XFE_SpellHandler::IsAvailable()
{
	return (m_active && (m_data.spell == NULL));
}

void
XFE_SpellHandler::DialogDestroyed()
{
	if (m_data.xWord) {
		m_data.interface->RemoveAllErrorHilites();
	}

	if (m_data.nuText) {
		XP_FREE(m_data.nuText);
	}

	if (m_data.langTags != NULL)
		XP_FREE(m_data.langTags);

	delete m_data.interface;
	nukeSpellChecker();

	m_data.spell     = NULL;
	m_data.interface = NULL;
	m_data.dialog  = NULL;
	m_data.list    = NULL;
	m_data.combo   = NULL;
	m_data.replace = NULL;
	m_data.replaceAll = NULL;
	m_data.check      = NULL;
	m_data.ignore     = NULL;
	m_data.ignoreAll  = NULL;
	m_data.learn      = NULL;
	m_data.stop       = NULL;
	m_data.status  = NULL;
	m_data.text    = NULL;
	m_data.nuText  = NULL;
	m_data.langTags  = NULL;
	m_data.langCount = 0;
	m_data.langIndx  = 0;
    m_data.inList  = TRUE;
    m_data.inCheck = FALSE;
	m_data.isOk    = FALSE;
	m_data.isDone  = FALSE;
}

void 
XFE_SpellHandler::UpdateGUI()
{
	char  *string = NULL;
	XmString xstr = NULL;

	XmListDeleteAllItems(m_data.list);

	if (m_data.inCheck) {
		// WARNING... [ don't nuke the text that you're going to use!! ]
		//
	}
	else {
		if (m_data.nuText) {
			XP_FREE(m_data.nuText);
			m_data.nuText  = NULL;
		}
	}

	if (m_data.isDone) {
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "done", 
											  "Done",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
	}
	else {
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "replace",
											  "Replace",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
	}
	xstr = XmStringCreateLtoR( string, XmSTRING_DEFAULT_CHARSET);
	XtVaSetValues(m_data.replace, XmNlabelString, xstr, 0);
	XmStringFree(xstr);

	xstr = NULL;

	if (m_data.xWord) {
		int numAlts = 0;

		if (!m_data.isOk) {
			numAlts = m_data.spell->GetNumAlternatives(m_data.xWord);
			XmTextSetString(m_data.text, m_data.xWord);
		}

		if (numAlts) {
			char AltString[125];
			char *tmp;
			int i = 0;

			for (i=0;i<numAlts;i++) {
				m_data.spell->GetAlternative(i, AltString, 
											 sizeof(AltString));
				tmp = XP_STRDUP(AltString);
				xstr = XmStringCreateLtoR( tmp, XmSTRING_DEFAULT_CHARSET);
				XmListAddItem(m_data.list, xstr, 0);
				XmStringFree(xstr);
				XP_FREE(tmp);
			}
			XmListSelectPos(m_data.list, 1, True);

			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "msgUnRecognized", 
												  "MsgUnRecognized",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);

			xstr = XmStringCreateLtoR( string, XmSTRING_DEFAULT_CHARSET);
			XtVaSetValues(m_data.status, XmNlabelString, xstr, 0);
			XmStringFree(xstr);

			XtSetSensitive(m_data.text, True);
			XtSetSensitive(m_data.list, True);
			XtSetSensitive(m_data.replace, True);
			XtSetSensitive(m_data.replaceAll, True);
			XtSetSensitive(m_data.check, False);
			XtSetSensitive(m_data.ignore, True);
			XtSetSensitive(m_data.ignoreAll, True);
			XtSetSensitive(m_data.learn, True);
			XtSetSensitive(m_data.stop, True);

			XtVaSetValues(m_data.dialog,  XmNdefaultButton, m_data.replace, 0);
			XtVaSetValues(m_data.ignore,  XmNshowAsDefault, False, 0);
			XtVaSetValues(m_data.replace, XmNshowAsDefault, True, 0);
			XmProcessTraversal(m_data.replace, XmTRAVERSE_CURRENT);
		}
		else {
			if (m_data.isOk) {
				string = XfeSubResourceGetStringValue(m_data.dialog,
													  "msgCorrect", 
													  "MsgCorrect",
													  XmNlabelString, 
													  XmCLabelString,
													  NULL);
			}
			else {
				string = XfeSubResourceGetStringValue(m_data.dialog,
													  "msgNoSuggestions", 
													  "MsgNoSuggestions",
													  XmNlabelString, 
													  XmCLabelString,
													  NULL);
			}

			xstr = XmStringCreateLtoR( string, XmSTRING_DEFAULT_CHARSET);
			XtVaSetValues(m_data.status, XmNlabelString, xstr, 0);
			XmStringFree(xstr);

			XtSetSensitive(m_data.text, True);
			XtSetSensitive(m_data.list, False);
			XtSetSensitive(m_data.replace, m_data.inCheck);
			XtSetSensitive(m_data.replaceAll, m_data.inCheck);
			XtSetSensitive(m_data.check, False);
			XtSetSensitive(m_data.ignore, True);
			XtSetSensitive(m_data.ignoreAll, True);
			XtSetSensitive(m_data.learn, True);
			XtSetSensitive(m_data.stop, True);

			if (m_data.inCheck) {
				XtVaSetValues(m_data.dialog, 
							  XmNdefaultButton, m_data.replace, 0);
				XtVaSetValues(m_data.ignore,  XmNshowAsDefault, False, 0);
				XtVaSetValues(m_data.replace, XmNshowAsDefault, True, 0);
				XmProcessTraversal(m_data.replace, XmTRAVERSE_CURRENT);
			}
			else {
				XtVaSetValues(m_data.dialog, 
							  XmNdefaultButton, m_data.ignore, 0);
				XtVaSetValues(m_data.replace, XmNshowAsDefault, False, 0);
				XtVaSetValues(m_data.ignore,  XmNshowAsDefault, True, 0);
				XmProcessTraversal(m_data.ignore, XmTRAVERSE_CURRENT);
			}
		}
	}
	else {
		XtSetSensitive(m_data.text, False);
		// WARNING... [ need to use resource string ]
		//
		XmTextSetString(m_data.text, "");

		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "msgFinished", 
											  "MsgFinished",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);

		xstr = XmStringCreateLtoR( string, XmSTRING_DEFAULT_CHARSET);
		XtVaSetValues(m_data.status, XmNlabelString, xstr, 0);
		XmStringFree(xstr);

		XtSetSensitive(m_data.list, False);
		XtSetSensitive(m_data.replace, True);
		XtSetSensitive(m_data.replaceAll, False);
		XtSetSensitive(m_data.check, False);
		XtSetSensitive(m_data.ignore, False);
		XtSetSensitive(m_data.ignoreAll, False);
		XtSetSensitive(m_data.learn, False);
		XtSetSensitive(m_data.stop, False);
	}

	m_data.inList  = TRUE;
}


void
XFE_SpellHandler::PopupDialog(MWContext* context, char* eWord)
{
	Widget   dialog;
	Widget   label;
	Widget   list;
	Widget   status;
	Widget   frame;
	Widget   text;
	Widget   form;
	Widget   replace;
	Widget   replaceAll;
	Widget   check;
	Widget   ignore;
	Widget   ignoreAll;
	Widget   learn;
	Widget   stop;
	Widget   right_rc;
	Widget   left_rc;
	Widget   space;
	Widget   cframe;
	Widget   combo;
	Arg      args[20];
	int      n;
	int      rc_delta;
	char*    string;
	XmString xstr = NULL;
	Dimension height;

	dialog = fe_CreatePromptDialog(context, "spellDialog",
								   FALSE, FALSE, FALSE, FALSE, TRUE);

	n = 0;
	form = XmCreateForm(dialog, "form", args, n);
	XtManageChild(form);

	n = 0;
	XtSetArg(args [n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNrightAttachment, XmATTACH_FORM); n++;
	frame = XmCreateFrame(form, "frame", args, n);
	XtManageChild(frame);

	n = 0;
	XtSetArg(args [n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	status = XmCreateLabelGadget(frame, "status", args, n);
	XtManageChild(status);

	string = XfeSubResourceGetStringValue(dialog,
										  "msgNull", 
										  "MsgNull",
										  XmNlabelString, 
										  XmCLabelString,
										  NULL);

	xstr = XmStringCreateLtoR( string, XmSTRING_DEFAULT_CHARSET);
	XtVaSetValues(status, XmNlabelString, xstr, 0);
	XmStringFree(xstr);

	n = 0;
	XtSetArg(args [n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNorientation, XmVERTICAL); n++;
	// SPacINg madness...
	//
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	right_rc = XmCreateRowColumn(form, "right_rc", args, n);
	XtManageChild(right_rc);

	n = 0;
	XtSetArg(args [n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args [n], XmNrightWidget, right_rc); n++;
	XtSetArg(args [n], XmNorientation, XmVERTICAL); n++;
	//
	// WARNING... [ these should go in the Resource file ]
	//
	XtSetArg(args [n], XmNrightOffset, 15); n++;
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	left_rc = XmCreateRowColumn(form, "left_rc", args, n);
	XtManageChild(left_rc);

	n = 0;
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	XtSetArg(args [n], XmNdefaultButtonShadowThickness, 1); n++;
	replace = XmCreatePushButtonGadget(right_rc, "replace", args, n);
	XtManageChild(replace);

	n = 0;
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	XtSetArg(args [n], XmNdefaultButtonShadowThickness, 1); n++;
	replaceAll = XmCreatePushButtonGadget(right_rc, "replace_all", args, n);
	XtManageChild(replaceAll);

	n = 0;
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	XtSetArg(args [n], XmNdefaultButtonShadowThickness, 1); n++;
	check = XmCreatePushButtonGadget(right_rc, "check", args, n);
	XtManageChild(check);

	n = 0;
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	XtSetArg(args [n], XmNdefaultButtonShadowThickness, 1); n++;
	ignore = XmCreatePushButtonGadget(right_rc, "ignore", args, n);
	XtManageChild(ignore);

	n = 0;
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	XtSetArg(args [n], XmNdefaultButtonShadowThickness, 1); n++;
	ignoreAll = XmCreatePushButtonGadget(right_rc, "ignore_all", args, n);
	XtManageChild(ignoreAll);

	n = 0;
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	XtSetArg(args [n], XmNdefaultButtonShadowThickness, 1); n++;
	learn = XmCreatePushButtonGadget(right_rc, "learn", args, n);
	XtManageChild(learn);

	n = 0;
	XtSetArg(args [n], XmNmarginWidth, 0); n++;
	XtSetArg(args [n], XmNmarginHeight, 0); n++;
	XtSetArg(args [n], XmNspacing, 0); n++;
	XtSetArg(args [n], XmNdefaultButtonShadowThickness, 1); n++;
	stop = XmCreatePushButtonGadget(right_rc, "stop", args, n);
	XtManageChild(stop);

	n = 0;
	label = XmCreateLabelGadget(left_rc, "text_label", args, n);
	XtManageChild(label);

	// WARNING... [ mAjOR layout hackery ]
	//
	XtVaGetValues(label, XmNheight, &height, 0);
	rc_delta = (int)height - 5;
	XtVaSetValues(right_rc, XmNtopOffset, rc_delta, 0);

	// WARNING... [ do I need to dup this? ]
	//
	string = eWord;

	n = 0;
	XtSetArg(args [n], XmNvalue, string); n++;
    XtSetArg(args [n], XmNeditable, True); n++;
	XtSetArg(args [n], XmNcursorPositionVisible, True); n++;
	text = fe_CreateTextField(left_rc, "errorText", args, n);
	XtManageChild(text);

	n = 0;
	label = XmCreateLabelGadget(left_rc, "list_label", args, n);
	XtManageChild(label);

	n = 0;
	XtSetArg(args [n], XmNvisibleItemCount, 10); n++;
	list = XmCreateScrolledList(left_rc, "list", args, n);
	XtManageChild(list);

	n = 0;
	XtSetArg(args [n], XmNseparatorType, XmNO_LINE); n++;
	XtSetArg(args [n], XmNheight, 4); n++;
	space = XmCreateSeparatorGadget(left_rc, "space", args, n);
	XtManageChild(space);

	n = 0;
	XtSetArg(args [n], XmNshadowType, XmSHADOW_IN); n++;
	XtSetArg(args [n], XmNshadowThickness, 1); n++;
	cframe = XmCreateFrame(left_rc, "combo_frame", args, n);
	XtManageChild(cframe);

	Visual*  v = 0;
	Colormap cmap = 0;
	Cardinal depth = 0;
	XtVaGetValues(XfeAncestorFindTopLevelShell(form),
				  XtNvisual, &v,
				  XtNcolormap, &cmap,
				  XtNdepth, &depth,
				  0);

	n = 0;
	XtSetArg(args [n], XmNvisual, v); n++;
	XtSetArg(args [n], XmNdepth, depth); n++;
	XtSetArg(args [n], XmNcolormap, cmap); n++;
	XtSetArg(args [n], XmNtype, XmDROP_DOWN_LIST_BOX); n++;
	XtSetArg(args [n], XmNshadowThickness, 1); n++;
	XtSetArg(args [n], XmNarrowType, XmMOTIF); n++;
	combo = DtCreateComboBox(cframe, "comboBox", args, n);
	XtManageChild(combo);

	XtVaSetValues(left_rc, 
				  XmNbottomAttachment, XmATTACH_WIDGET, 
				  XmNbottomWidget,     frame,
				  XmNbottomOffset,     6, 
				  0);

	XtVaSetValues(right_rc, 
				  XmNbottomAttachment, XmATTACH_WIDGET, 
				  XmNbottomWidget,     frame,
				  XmNbottomOffset,     6, 
				  0);

	m_data.dialog  = dialog;
	m_data.list    = list;
	m_data.combo   = combo;
	m_data.replace = replace;
	m_data.replaceAll = replaceAll;
	m_data.check      = check;
	m_data.ignore     = ignore;
	m_data.ignoreAll  = ignoreAll;
	m_data.learn      = learn;
	m_data.stop       = stop;
	m_data.status  = status;
	m_data.text    = text;

	XtVaSetValues(dialog, XmNdefaultButton, replace, 0);
	XtVaSetValues(dialog, XmNinitialFocus, list, 0);

	// XtAddCallback(dialog, XmNcancelCallback, fe_open_url_cb, data);
	// XtAddCallback(dialog, XmNapplyCallback, fe_clear_text_cb, text);

	XtAddCallback(text, 
				  XmNvalueChangedCallback, xfe_spell_text_cb, &m_data);
	XtAddCallback(text, 
				  XmNfocusCallback, xfe_spell_focus_in_cb, &m_data);
	XtAddCallback(text, 
				  XmNlosingFocusCallback, xfe_spell_focus_out_cb, &m_data);
	XtAddCallback(list, 
				  XmNbrowseSelectionCallback, xfe_spell_list_cb, &m_data);
	XtAddCallback(combo, 
				  XmNselectionCallback, xfe_spell_combo_cb, &m_data);

	XtAddCallback(replace, XmNactivateCallback, 
				  xfe_spell_doit_cb, &m_tags[XSP_REPLACE]);

	XtAddCallback(replaceAll, XmNactivateCallback, 
				  xfe_spell_doit_cb, &m_tags[XSP_REPLACEALL]);

	XtAddCallback(check, XmNactivateCallback, 
				  xfe_spell_doit_cb, &m_tags[XSP_CHECK]);

	XtAddCallback(ignore, XmNactivateCallback, 
				  xfe_spell_doit_cb, &m_tags[XSP_IGNORE]);

	XtAddCallback(ignoreAll, XmNactivateCallback, 
				  xfe_spell_doit_cb, &m_tags[XSP_IGNOREALL]);

	XtAddCallback(learn, XmNactivateCallback, 
				  xfe_spell_doit_cb, &m_tags[XSP_LEARN]);

	XtAddCallback(stop, XmNactivateCallback, 
				  xfe_spell_doit_cb, &m_tags[XSP_STOP]);


	XtAddCallback(dialog, 
				  XmNdestroyCallback, xfe_spell_destroy_cb, &m_data);
	
	// NOTE:  load the combo box...
	//
	initLanguageList();

	fe_NukeBackingStore (dialog);

	XtManageChild (dialog);
}

void 
XFE_SpellHandler::initLanguageList()
{
    int       Language;
	int       Dialect;
	int       i;
	char     *string;
	XmString  xm_string;
    int       count = m_data.spell->GetNumOfDictionaries();

	
	m_data.langCount = count;
	m_data.langTags = (xfe_lang_tag *) XP_CALLOC(count, sizeof(xfe_lang_tag));

    for (i = 0; i < count; i++)
    {
        if (m_data.spell->GetDictionaryLanguage(i, Language, Dialect) == 0)
        {
			string = getLanguageString(Language, Dialect);

			xm_string = XmStringCreateLocalized(string);
			DtComboBoxAddItem(m_data.combo, xm_string, i + 1, FALSE);
			XmStringFree(xm_string);
        }
		else {
			Language = 0;
			Dialect = 0;

			xm_string = XmStringCreateLocalized("");
			DtComboBoxAddItem(m_data.combo, xm_string, i + 1, FALSE);
			XmStringFree(xm_string);
		}
		m_data.langTags[i].language = Language;
		m_data.langTags[i].dialect = Dialect;
    }
	XtVaSetValues(m_data.combo, XmNvisibleItemCount, (XtPointer)i, 0);

    // Select the current default
	//
    m_data.spell->GetCurrentLanguage(Language, Dialect);

	if (Language == 0 && Dialect == 0) {
		updateLang(-1);
	}
    else {
        for (i = 0; i < count; i++)
        {
            if (m_data.langTags[i].language == Language &&
				m_data.langTags[i].dialect == Dialect)
            {
				updateLang(i);
                break;
            }
        }
    }
}

void
XFE_SpellHandler::updateLang(int indx)
{
   if (indx < 0) {
	   m_data.langIndx = indx;

	   XmString blank = XmStringCreateLocalized("");
	   XtVaSetValues(m_data.combo,
					 XmNupdateLabel, False,
					 XmNlabelString, blank,
					 0);
	   XmStringFree(blank);
   } else {
	   m_data.langIndx = indx;

	   XtVaSetValues(m_data.combo,
					 XmNupdateLabel, True,
					 XmNselectedPosition, indx,
					 0);
   }
}

char *
XFE_SpellHandler::getLanguageString(int lang, int dialect)
{
	char *string;

    switch (lang)
    {
    case L_CZECH:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langCzech", 
											  "LangCzech",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_RUSSIAN:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langRussian", 
											  "LangRussian",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_CATALAN:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langCatalan", 
											  "LangCatalan",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_HUNGARIAN:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langHungarian", 
											  "LangHungarian",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_FRENCH:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langFrench", 
											  "LangFrench",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_GERMAN:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langGerman", 
											  "LangGerman",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_SWEDISH:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langSwedish", 
											  "LangSwedish",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_SPANISH:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langSpanish", 
											  "LangSpanish",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_ITALIAN:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langItalian", 
											  "LangItalian",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_DANISH:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langDanish", 
											  "LangDanish",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_DUTCH:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langDutch", 
											  "LangDutch",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;

    case L_PORTUGUESE:  
		if (dialect == D_BRAZILIAN)
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langPortugueseBrazilian", 
												  "LangPortugueseBrazilian",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		else if (dialect == D_EUROPEAN)
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langPortugueseEuropean", 
												  "LangPortugueseEuropean",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		else
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langPortuguese", 
												  "LangPortuguese",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		break;

    case L_NORWEGIAN:   
		if (dialect == D_BOKMAL)
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langNorwegianBokmal", 
												  "LangNorwegianBokmal",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		else if (dialect == D_NYNORSK)
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langNorwegianNynorsk",
												  "LangNorwegianNynorsk",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		else
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langNorwegian",
												  "LangNorwegian",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		break;

    case L_FINNISH:     
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langFinnish",
											  "LangFinnish",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_GREEK:       
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langGreek",
											  "LangGreek",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;

    case L_ENGLISH:     
		if (dialect == D_US_ENGLISH)
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langEnglishUS",
												  "LangEnglishUS",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		else if (dialect == D_UK_ENGLISH)
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langEnglishUK",
												  "LangEnglishUK",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		else
			string = XfeSubResourceGetStringValue(m_data.dialog,
												  "langEnglish",
												  "LangEnglish",
												  XmNlabelString, 
												  XmCLabelString,
												  NULL);
		break;

    case L_AFRIKAANS:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langAfrikaans",
											  "LangAfrikaans",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;
    case L_POLISH:
		string = XfeSubResourceGetStringValue(m_data.dialog,
											  "langPolish",
											  "LangPolish",
											  XmNlabelString, 
											  XmCLabelString,
											  NULL);
		break;

    default:
		string = "";
		break;
    }

    return string;
}

XP_Bool
XFE_SpellHandler::ProcessDocument(MWContext* context, XP_Bool isHtml)
{
	initSpellChecker();

    if (m_data.spell == NULL)
        return FALSE;

	if (m_data.interface == NULL) {
		if (isHtml) {
			m_data.interface = new XFE_HtmlSpellCheck(m_data.spell, context);
		}
		else {
			m_data.interface = new XFE_TextSpellCheck(m_data.spell, context);
		}
	}

	m_data.inCheck = FALSE;
	m_data.isOk    = FALSE;
	m_data.isDone  = FALSE;

	XP_Bool oops = m_data.interface->ProcessDocument(&m_tags[XSP_NULL]);

	PopupDialog(context, m_data.xWord);
	UpdateGUI();

	return oops;
}

XP_Bool
XFE_SpellHandler::ReprocessDocument()
{
	m_data.interface->ProcessError(&m_tags[XSP_STOP]);

	nukeSpellChecker();
	initSpellChecker();

	m_data.inCheck = FALSE;
	m_data.isOk    = FALSE;
	m_data.isDone  = FALSE;

	XP_Bool oops = m_data.interface->ProcessDocument(&m_tags[XSP_NULL]);

	UpdateGUI();

	return oops;
}

XP_Bool
XFE_SpellHandler::ProcessError(xfe_spell_tag *tag)
{
	int              action  = tag->action;
	xfe_spell_data  *data    = tag->data;
	ISpellChecker   *spell   = data->spell;

	data->inCheck = FALSE;
	data->isOk    = FALSE;

	switch (action) {
	case XSP_STOP:
		// call this to cleanup before nuking the dialog...
		(data->interface)->ProcessError(tag);
		(data->interface)->resetVars();
	case XSP_REPLACE:
		if (data->isDone) {
			XtUnmanageChild(data->dialog);
			//
			// Nuke it...
			//
			XtDestroyWidget(data->dialog);

			return FALSE;
		}
		break;
	case XSP_CHECK:
		data->inCheck = TRUE;

		if (spell->CheckWord(data->nuText)) {
			data->isOk = TRUE;
		} 
		data->xWord = data->nuText;

		return TRUE;
	default:
		break;
	}

	return (data->interface)->ProcessError(tag);
}


// -------------------------------------------------------------[ SpellCheck ]


XFE_SpellCheck::XFE_SpellCheck(ISpellChecker *spell, MWContext* context)
{
	m_spellChecker     = spell;

	m_bufferSize       = 0;
	m_xpDelta          = 0;
	m_contextData      = context;
	m_misspelledWord   = NULL;
	m_selStart         = 0;
	m_selEnd           = 0;
}

XFE_SpellCheck::~XFE_SpellCheck()
{
	m_spellChecker     = NULL;
}

void
XFE_SpellCheck::resetVars()
{
	m_bufferSize       = 0;
	m_xpDelta          = 0;
	m_selStart         = 0;
	m_selEnd           = 0;
}

XP_Bool
XFE_SpellCheck::ProcessError(xfe_spell_tag* tag)
{
	int              action  = tag->action;
	xfe_spell_data  *data    = tag->data;
	char            *usrText = data->nuText;
    char            *tmp     = NULL;

	if (m_misspelledWord) {
		switch (action) {
		case XSP_REPLACE:
			ReplaceHilitedText(usrText, FALSE);
			break;
		case XSP_REPLACEALL:
			ReplaceHilitedText(usrText, TRUE);
			break;
		case XSP_IGNORE:
			IgnoreHilitedText(FALSE);
			break;
		case XSP_IGNOREALL:
			IgnoreHilitedText(TRUE);
			break;
		case XSP_LEARN:
			tmp = XmTextFieldGetString(data->text);

			if (tmp) {
				m_spellChecker->AddWordToPersonalDictionary(tmp);

				if (XP_STRCMP( m_misspelledWord, tmp) == 0) {
					IgnoreHilitedText(TRUE);
				}
				else {
					ReplaceHilitedText(tmp, TRUE);
				}

				XtFree(tmp);
			}
			break;
		case XSP_STOP:
			m_misspelledWord = NULL;
			data->xWord  = NULL;
			data->isDone = TRUE;

			RemoveAllErrorHilites();
			return FALSE;
		default:
			break;
		}
		m_misspelledWord = GetNextError();
	}
	else {
		m_misspelledWord = GetFirstError();
	}

	if (!m_misspelledWord) {
		m_misspelledWord = NULL;
		data->xWord  = NULL;
		data->isDone = TRUE;
		
		RemoveAllErrorHilites();
		return FALSE;
	}
	else {
		data->xWord = m_misspelledWord;
		return TRUE;
	}
}

XP_Bool
XFE_SpellCheck::ProcessDocument(xfe_spell_tag* tag)
{
	xfe_spell_data  *data    = tag->data;
	ISpellChecker   *spell   = data->spell;
	
	// NOTE: important to make sure that we are in sync with the Handler...
	//
	if (m_spellChecker != spell) {
		m_spellChecker = spell;
	}

    // Get the text buffer from the document
    XP_HUGE_CHAR_PTR pBuf = GetBuffer();

    if (pBuf == NULL)
        return 0;                       // nothing to spell check

	// If we were spell checking a selection in the previous pass,
	// adjust the selection for any corrections made in the last pass.
	//
	if (m_selEnd > 0) {
		m_selEnd += (XP_STRLEN(pBuf) - (m_bufferSize - m_xpDelta));
	}
	else {
		GetSelection(m_selStart, m_selEnd);
	}

    // pass text buffer to the spell checker
    int retcode = m_spellChecker->SetBuf(pBuf, m_selStart, m_selEnd);

    m_bufferSize = (int) m_spellChecker->GetBufSize();
	m_xpDelta    = m_bufferSize - XP_STRLEN(pBuf);

    // release the buffer (the Spell Checker makes a local copy)
    XP_HUGE_FREE(pBuf);         

	// WARNING... [ marking all misspelled words ]
	//
    return ProcessError(tag);
}

// ---------------------------------------------------------[ HtmlSpellCheck ]

XFE_HtmlSpellCheck::XFE_HtmlSpellCheck(ISpellChecker *spell, 
									   MWContext *context)
  : XFE_SpellCheck(spell, context)
{
}

XFE_HtmlSpellCheck::~XFE_HtmlSpellCheck()
{
}

char *
XFE_HtmlSpellCheck::GetBuffer()
{
	return EDT_GetPositionalText(m_contextData);	
}

void 
XFE_HtmlSpellCheck::IgnoreHilitedText(int AllInstances)
{
    char *pOldWord = (char *)LO_GetSelectionText(m_contextData);

    if (pOldWord != NULL)
    {
        EDT_IgnoreMisspelledWord(m_contextData, pOldWord, AllInstances);
        XP_FREE(pOldWord);
    }
}

void 
XFE_HtmlSpellCheck::RemoveAllErrorHilites()
{
	//	EDT_BeginOfDocument(m_contextData, FALSE);
	//	EDT_SetRefresh(m_contextData, TRUE);

    // ignore any unprocessed misspelled words 
    EDT_IgnoreMisspelledWord(m_contextData, NULL, TRUE);
}

char *
XFE_HtmlSpellCheck::GetFirstError()
{
    // turn off refreshing and spell check the text.
    EDT_SetRefresh(m_contextData, FALSE);

    // underline the misspelled words
    EDT_CharacterData* pCharData = EDT_NewCharacterData();
    pCharData->mask = TF_SPELL;
    pCharData->values = TF_SPELL;

    unsigned long Offset, Len;
    while (m_spellChecker->GetNextMisspelledWord(Offset, Len) == 0)
	    EDT_SetCharacterDataAtOffset(m_contextData, pCharData, Offset, Len);
  
    XP_FREE(pCharData);

    // set sursor position at the beginning of document so that 
    // EDT_SelectNextMisspelledWord() to start at the beginning.
    EDT_BeginOfDocument(m_contextData, FALSE);
	EDT_SetRefresh(m_contextData, TRUE);

	// m_pView->UpdateWindow();
	fe_EditorRefresh(m_contextData);

    // Select and return the first mispelled word 
    if (EDT_SelectFirstMisspelledWord(m_contextData))
        return (char *)LO_GetSelectionText(m_contextData);
    else
        return NULL;
}

char *
XFE_HtmlSpellCheck::GetNextError()
{
    if (EDT_SelectNextMisspelledWord(m_contextData))
        return (char *)LO_GetSelectionText(m_contextData);
    else
        return 0;
}

XP_Bool
XFE_HtmlSpellCheck::GetSelection(int32 &SelStart, int32 &SelEnd)
{
    char *pSelection;
   
    if ((pSelection = (char *)LO_GetSelectionText(m_contextData)) != NULL)
    {
        XP_FREE(pSelection);
        EDT_GetSelectionOffsets(m_contextData, &SelStart, &SelEnd);
        return TRUE;
    }
    else
        return FALSE;       // no selection
}

void
XFE_HtmlSpellCheck::ReplaceHilitedText(const char *NewText, 
									   XP_Bool AllInstances)
{
    char *pOldWord = (char *)LO_GetSelectionText(m_contextData);

    if (pOldWord != NULL)
    {
        EDT_ReplaceMisspelledWord(m_contextData, 
								  pOldWord, (char*)NewText, AllInstances);
        XP_FREE(pOldWord);
    }
    else {
#ifdef DEBUG_spellcheck
		fprintf(stderr, "WARNING... [ unable to fetch pOldWord ]\n");
#endif		
	}
}



// --------------------------------------------------------[ TextSpellCheck ]

XFE_TextSpellCheck::XFE_TextSpellCheck(ISpellChecker *spell,
									   MWContext *context)
  : XFE_SpellCheck(spell, context)
{
	m_textWidget = CONTEXT_DATA(m_contextData)->mcBodyText;
	m_dirty = FALSE;
	m_offset = 0;
	m_len = 0;
}

XFE_TextSpellCheck::~XFE_TextSpellCheck()
{
	m_textWidget = NULL;
}

char *
XFE_TextSpellCheck::GetBuffer()
{
	return XmTextGetString(m_textWidget);
}

void  
XFE_TextSpellCheck::IgnoreHilitedText(int AllInstances)
{
	if (m_selEnd > 0) {
		XmTextSetHighlight(m_textWidget, 
						   m_offset, (m_offset + m_len),
						   XmHIGHLIGHT_SECONDARY_SELECTED
						   );
	}
	else {
		XmTextSetHighlight(m_textWidget, 
						   m_offset, (m_offset + m_len),
						   XmHIGHLIGHT_NORMAL 
						   );
	}

	if (AllInstances) {
		m_spellChecker->IgnoreWord(m_misspelledWord);
	}
}

void  
XFE_TextSpellCheck::RemoveAllErrorHilites()
{
	if (m_selEnd > 0 && !m_dirty) {
		Time time = XtLastTimestampProcessed(XtDisplay(m_textWidget));

		// Nuke the selection if it still exists after the spell check...
		//
		XmTextClearSelection(m_textWidget, time);

		//		XmTextSetHighlight(m_textWidget, 
		//						   m_selStart, m_selEnd, 
		//						   XmHIGHLIGHT_SELECTED
		//						   );
	}
	else {
		XmTextSetHighlight(m_textWidget, 
						   0, (m_bufferSize - 1), 
						   XmHIGHLIGHT_NORMAL
						   );
	}
}

char *
XFE_TextSpellCheck::GetFirstError()
{
	if (m_selEnd > m_selStart) {
		XmTextSetHighlight(m_textWidget, 
						   m_selStart, m_selEnd, 
						   XmHIGHLIGHT_SECONDARY_SELECTED
						   );
	}

	return GetNextError();
}

char *
XFE_TextSpellCheck::GetNextError()
{
    unsigned long Offset, Len;
    char *pMisspelledWord = NULL;

    if (m_spellChecker->GetNextMisspelledWord(Offset, Len) == 0)
    {
		m_offset = (int) Offset;
		m_len = (int) Len;
        // hilight mispelled word
		XmTextSetHighlight(m_textWidget, 
						   m_offset, (m_offset + m_len),
						   XmHIGHLIGHT_SELECTED 
						   );

        // Extract mispelled word 
        XP_HUGE_CHAR_PTR pBuf = GetBuffer();
        if (pBuf != NULL)
        {
            pMisspelledWord = (char *)XP_ALLOC(Len + 1);
            if (pMisspelledWord != NULL)
                XP_STRNCPY_SAFE(pMisspelledWord, pBuf + Offset, Len+1);

            XP_HUGE_FREE(pBuf);
        }
    }
	else {
		m_offset = 0;
		m_len = 0;
	}

    return pMisspelledWord;
}

XP_Bool
XFE_TextSpellCheck::GetSelection(int32 &SelStart, int32 &SelEnd)
{
    XmTextPosition xRight;
    XmTextPosition xLeft;

    XmTextGetSelectionPosition(m_textWidget, &xRight, &xLeft);

    if (xLeft > xRight)
    {
        SelStart = (int) xRight;
        SelEnd = (int) xLeft;

		XmTextSetHighlight(m_textWidget, 
						   SelStart, SelEnd, 
						   XmHIGHLIGHT_SECONDARY_SELECTED
						   );
        return TRUE;
    }
    else {
		SelStart = 0;
		SelEnd = 0;

        return FALSE;
	}
}

void
XFE_TextSpellCheck::ReplaceHilitedText(const char *NewText, 
									   XP_Bool AllInstances)
{
    if (m_spellChecker->ReplaceMisspelledWord(NewText, AllInstances) != 0) {
#ifdef DEBUG_spellcheck
		fprintf(stderr, "WARNING... [ ReplaceMisspelledWord failed ]\n");
#endif
	}

    unsigned long NewBufSize = m_spellChecker->GetBufSize();
    char *pNewBuf = (char *)XP_ALLOC(NewBufSize);

    if (pNewBuf == NULL) {
#ifdef DEBUG_spellcheck
		fprintf(stderr, "WARNING... [ unable to alloc pNewBuf ]\n");
#endif
        return;
    }

	int delta = NewBufSize - m_bufferSize;
	int nuEnd = 0;

	if (m_selEnd > 0) {
		nuEnd = m_selEnd + delta;

		if (nuEnd < m_selEnd) {
			// NOTE: we have to fix up the end of the the selection marker...
			//
			XmTextSetHighlight(m_textWidget, 
							   m_selStart, m_selEnd, 
							   XmHIGHLIGHT_NORMAL
							   );
		}
	}

    m_spellChecker->GetBuf(pNewBuf, NewBufSize);
    XmTextSetString(m_textWidget, pNewBuf);

	// NOTE: this denotes the lose of the "selection" in the text widget...
	//
	m_dirty = True;

	m_bufferSize = (int)NewBufSize;

	if (m_selEnd > 0) {
		m_selEnd = nuEnd;

		XmTextSetHighlight(m_textWidget, 
						   m_selStart, m_selEnd, 
						   XmHIGHLIGHT_SECONDARY_SELECTED
						   );
	}

    XP_FREE(pNewBuf);
}



// --------------------------------------------------------------------[ API ]

Boolean
xfe_SpellCheckerAvailable(MWContext* context)
{
	if (xfe_spellHandler == NULL) {
		xfe_spellHandler = new XFE_SpellHandler(context);
	}

	return xfe_spellHandler->IsAvailable();
}

Boolean
xfe_EditorSpellCheck(MWContext* context)
{
	if (xfe_SpellCheckerAvailable(context)) {
		xfe_spellHandler->ProcessDocument(context, TRUE);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

Boolean
xfe_TextSpellCheck(MWContext* context)
{
	if (xfe_SpellCheckerAvailable(context)) {
		xfe_spellHandler->ProcessDocument(context, FALSE);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// ----<eof>
