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
   EditorView.cpp -- class definition for the editor view class
   Created: Richard Hess <rhess@netscape.com>, 11-Nov-96
 */


#include "EditorFrame.h"
#include "EditorView.h"
#include "SpellHandler.h"

#include <Xm/Label.h>
#include <Xfe/Xfe.h>

#include "xpgetstr.h"
#include "fe_proto.h"
#include "edt.h"
#include "xeditor.h"
#ifndef NO_SECURITY
#include "pk11func.h"
#endif

#define FE_SYNTAX_ERROR() doSyntaxErrorAlert(view, info)

//
//    This acts as an encapsulator for the doCommand() method.
//    Sub-classes impliment a reallyDoCommand(), and leave the
//    boring maintainence work to this class. This approach
//    saves every sub-class from calling super::doCommand(),
//    which would really be a drag, now wouldn't it.
//
class XFE_EditorViewCommand : public XFE_ViewCommand
{
public:
	XFE_EditorViewCommand(char* name) : XFE_ViewCommand(name) {};
	
	virtual void    reallyDoCommand(XFE_View*, XFE_CommandInfo*) = 0;
	virtual XP_Bool requiresChromeUpdate() {
		return TRUE;
	};
	void            doCommand(XFE_View* v_view, XFE_CommandInfo* info) {
		XFE_EditorView* view = (XFE_EditorView*)v_view;
		reallyDoCommand(view, info);
		if (requiresChromeUpdate()) {
			view->updateChrome();
		}
	}; 
};

class UndoCommand : public XFE_EditorViewCommand
{
public:
	UndoCommand() : XFE_EditorViewCommand(xfeCmdUndo) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorCanUndo(view->getContext(), NULL);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorUndo(view->getContext());
	}; 
	char*   getLabel(XFE_View* view, XFE_CommandInfo*) {
#if EDITOR_UNDO
		char* label;
		fe_EditorCanUndo(view->getContext(), (unsigned char **)&label);
		return label;
#else
		//
		//    Well, in the new world order none of the stuff that I
		//    used to use for Undo works any more, so until we fix it...
		//
		Widget top = view->getToplevel()->getBaseWidget();
		return XfeSubResourceGetStringValue(top,
											"undo", 
											"UndoItem",
											XmNlabelString, 
											XmCLabelString,
											NULL);
#endif
	};
};

class RedoCommand : public XFE_EditorViewCommand
{
public:
	RedoCommand() : XFE_EditorViewCommand(xfeCmdRedo) {};
	
	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorCanRedo(view->getContext(), NULL);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorRedo(view->getContext());
	}; 
	char*   getLabel(XFE_View* view, XFE_CommandInfo*) {
#if EDITOR_UNDO
		char* label;
		fe_EditorCanRedo(view->getContext(), (unsigned char **)&label);
		return label;
#else
		//    See above.
		Widget top = view->getToplevel()->getBaseWidget();
		return XfeSubResourceGetStringValue(top,
											"redo", 
											"UndoItem",
											XmNlabelString, 
											XmCLabelString,
											NULL);
#endif
	};
};

class CutCommand : public XFE_EditorViewCommand
{
public:
	CutCommand() : XFE_EditorViewCommand(xfeCmdCut) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorCanCut(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
		fe_EditorCut(view->getContext(), fe_GetTimeFromEvent(info->event));
	}; 
};

class CopyCommand : public XFE_EditorViewCommand
{
public:
	CopyCommand() : XFE_EditorViewCommand(xfeCmdCopy) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorCanCopy(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* calldata); 
};

void CopyCommand::reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
	fe_EditorCopy(view->getContext(), fe_GetTimeFromEvent(info->event));
}; 

class PasteCommand : public XFE_EditorViewCommand
{
public:
	PasteCommand() : XFE_EditorViewCommand(xfeCmdPaste) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo* info) {
		XP_Bool rv = FALSE;

		if (info != NULL && *info->nparams > 0) {
			if (XP_STRCASECMP(info->params[0], "clipboard") == 0) {
				rv = fe_EditorCanPaste(view->getContext());
			} else if (XP_STRCASECMP(info->params[0], "selection") == 0) {
				rv = TRUE; // lazy, but this would be a real performance hit
			} else {
				FE_SYNTAX_ERROR();
			}
		} else {
			rv = fe_EditorCanPaste(view->getContext());
		}
		return rv;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
		Boolean from_selection = FALSE;
		MWContext* context = view->getContext();

		if (info != NULL) {

			Time now = fe_GetTimeFromEvent(info->event);

			if (*info->nparams > 0) {
				if (XP_STRCASECMP(info->params[0], "clipboard") == 0) {
					from_selection = FALSE;
				} else if (XP_STRCASECMP(info->params[0], "selection") == 0) {
					from_selection = TRUE;
				} else {
					FE_SYNTAX_ERROR();
				}
			}

			if (from_selection) {
				// NOTE... [ if it's triggered off a mouse click (Btn2), 
				//         [ set the location before you paste...
				//
				if ((info->event != NULL) &&
					(info->event->type == ButtonPress)) {
					fe_EditorGrabFocus(context, info->event);
				}
				fe_EditorPastePrimarySel(context, now);
			}
			else
				fe_EditorPaste(context, now);
		}
	}; 
};

class AlwaysEnabledCommand : public XFE_EditorViewCommand
{
public:
	AlwaysEnabledCommand(char* name) : XFE_EditorViewCommand(name) {};

	XP_Bool isEnabled(XFE_View*, XFE_CommandInfo*) {
		return True;
	};
};

class DeleteCommand : public AlwaysEnabledCommand
{
public:
	DeleteCommand() : AlwaysEnabledCommand(xfeCmdDeleteItem) {};

	virtual XP_Bool isSlow() {
		return FALSE;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
		Boolean previous = FALSE;
		if (info != NULL && *info->nparams > 0) {
			char* param = info->params[0];

			if (XP_STRCASECMP(param, "next") == 0)
				previous = FALSE;
			else if (XP_STRCASECMP(param, "previous") == 0)
				previous = TRUE;
			else if (XP_STRCASECMP(param, "endOfLine") == 0) {
				EDT_EndOfLine(view->getContext(), True);
			}
			else {
				FE_SYNTAX_ERROR();
			}
		}

		fe_EditorDeleteItem(view->getContext(), previous);
	}; 
};

class SpellCommand : public XFE_EditorViewCommand
{
public:
	SpellCommand() : XFE_EditorViewCommand(xfeCmdSpellCheck) {};

	XP_Bool isDynamic() { return TRUE; };

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return xfe_SpellCheckerAvailable(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		xfe_EditorSpellCheck(view->getContext());
	}; 
};

class SaveCommand : public AlwaysEnabledCommand
{
public:
	SaveCommand() : AlwaysEnabledCommand(xfeCmdSave) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorSave(view->getContext());
	}; 
};

class SaveAsCommand : public AlwaysEnabledCommand
{
public:
	SaveAsCommand() : AlwaysEnabledCommand(xfeCmdSaveAs) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorSaveAs(view->getContext());
	}; 
};

class PublishCommand : public AlwaysEnabledCommand
{
public:
	PublishCommand() : AlwaysEnabledCommand(xfeCmdPublish) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorPublish(view->getContext());
	}; 
};

class DeleteTableCommand : public XFE_EditorViewCommand
{
public:
	DeleteTableCommand() : XFE_EditorViewCommand(xfeCmdDeleteTable) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableCanDelete(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableDelete(view->getContext());
	}; 
};

class DeleteTableCellCommand : public XFE_EditorViewCommand
{
public:
	DeleteTableCellCommand() : XFE_EditorViewCommand(xfeCmdDeleteTableCell) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableCellCanDelete(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableCellDelete(view->getContext());
	}; 
};

class DeleteTableRowCommand : public XFE_EditorViewCommand
{
public:
	DeleteTableRowCommand() : XFE_EditorViewCommand(xfeCmdDeleteTableRow) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableRowCanDelete(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableRowDelete(view->getContext());
	}; 
};

class DeleteTableColumnCommand : public XFE_EditorViewCommand
{
public:
	DeleteTableColumnCommand() : XFE_EditorViewCommand(xfeCmdDeleteTableColumn) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableColumnCanDelete(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableColumnDelete(view->getContext());
	}; 
};

class RemoveLinkCommand : public XFE_EditorViewCommand
{
public:
	RemoveLinkCommand() : XFE_EditorViewCommand(xfeCmdRemoveLink) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorCanRemoveLinks(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorRemoveLinks(view->getContext());
	}; 
};

class SelectAllCommand : public AlwaysEnabledCommand
{
public:
	SelectAllCommand() : AlwaysEnabledCommand(xfeCmdSelectAll) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorSelectAll(view->getContext());
	}; 
};

class FindCommand : public AlwaysEnabledCommand
{
public:
	FindCommand() : AlwaysEnabledCommand(xfeCmdFindInObject) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorFind(view->getContext());
	}; 
};

class FindAgainCommand : public XFE_EditorViewCommand
{
public:
	FindAgainCommand() : XFE_EditorViewCommand(xfeCmdFindAgain) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorCanFindAgain(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorFindAgain(view->getContext());
	}; 
};

static char*
get_show_hide_label(Widget widget, char* root, Boolean show)
{
	char* prefix;
	char  name[256];
	
	if (show)
		prefix = "show";
	else
		prefix = "hide";

	XP_STRCPY(name, prefix);
	if (root != NULL)
		XP_STRCAT(name, root);

	return XfeSubResourceGetStringValue(widget,
										name, 
										XfeClassNameForWidget(widget),
										XmNlabelString, 
										XmCLabelString,
										NULL);
}

class ToggleParagraphMarksCommand : public AlwaysEnabledCommand
{
public:
	ToggleParagraphMarksCommand() :
		AlwaysEnabledCommand(xfeCmdToggleParagraphMarks) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorParagraphMarksGetState(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		Boolean    set = fe_EditorParagraphMarksGetState(context);
		fe_EditorParagraphMarksSetState(context, !set);
	}; 
	char*   getLabel(XFE_View* view, XFE_CommandInfo* info) {

		char* string = NULL;

		if (info != NULL) {
			MWContext* context = view->getContext();
			Boolean    set = fe_EditorParagraphMarksGetState(context);

			string = get_show_hide_label(info->widget, "ParagraphMarks", !set);
		}
		return string;
	};
};

class ToggleTableBordersCommand : public AlwaysEnabledCommand
{
public:
	ToggleTableBordersCommand() :
		AlwaysEnabledCommand(xfeCmdToggleTableBorders) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorDisplayTablesGet(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		Boolean    set = fe_EditorDisplayTablesGet(context);
		fe_EditorDisplayTablesSet(context, !set);
	}; 
};

class ReloadCommand : public AlwaysEnabledCommand
{
public:
	ReloadCommand() : AlwaysEnabledCommand(xfeCmdReload) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return  !EDT_IS_NEW_DOCUMENT(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {

		Boolean do_super = (info != NULL &&
							*info->nparams > 0 &&
							XP_STRCASECMP(info->params[0], "super") == 0);
		fe_EditorReload(view->getContext(), do_super);
	}; 
};

class RefreshCommand : public AlwaysEnabledCommand
{
public:
	RefreshCommand() : AlwaysEnabledCommand(xfeCmdRefresh) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorRefresh(view->getContext());
	}; 
};

class InsertLinkCommand : public AlwaysEnabledCommand
{
public:
	InsertLinkCommand() : AlwaysEnabledCommand(xfeCmdInsertLink) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorInsertLinkDialogDo(view->getContext());
	}; 
};

class InsertTargetCommand : public AlwaysEnabledCommand
{
public:
	InsertTargetCommand() : AlwaysEnabledCommand(xfeCmdInsertTarget) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTargetPropertiesDialogDo(view->getContext());
	}; 
};

class InsertImageCommand : public AlwaysEnabledCommand
{
public:
	InsertImageCommand() : AlwaysEnabledCommand(xfeCmdInsertImage) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorPropertiesDialogDo(view->getContext(),
									XFE_EDITOR_PROPERTIES_IMAGE_INSERT);
	}; 
};

class InsertBulletedListCommand : public AlwaysEnabledCommand
{
public:
	InsertBulletedListCommand() : 
		AlwaysEnabledCommand(xfeCmdInsertBulletedList) {};

	XP_Bool isDynamic() { return TRUE; };

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorToggleList(view->getContext(), P_UNUM_LIST);
	}; 
	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		return EDT_GetToggleListState(view->getContext(), P_UNUM_LIST);
	};
};

class InsertNumberedListCommand : public AlwaysEnabledCommand
{
public:
	InsertNumberedListCommand() : 
		AlwaysEnabledCommand(xfeCmdInsertNumberedList) {};

	XP_Bool isDynamic() { return TRUE; };

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorToggleList(view->getContext(), P_NUM_LIST);
	}; 
	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		return EDT_GetToggleListState(view->getContext(), P_NUM_LIST);
	};
};

class InsertHorizontalLineCommand : public AlwaysEnabledCommand
{
public:
	InsertHorizontalLineCommand() : 
		AlwaysEnabledCommand(xfeCmdInsertHorizontalLine) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {

		MWContext* context = view->getContext();

		if (info != NULL && 
			*info->nparams > 0 &&
			XP_STRCASECMP(info->params[0], "dialog")) {
			fe_EditorHorizontalRulePropertiesDialogDo(view->getContext());
		} else {
			fe_EditorInsertHorizontalRule(context);
		}
	}; 
};

class InsertTableCommand : public XFE_EditorViewCommand
{
public:
	InsertTableCommand() : XFE_EditorViewCommand(xfeCmdInsertTable) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableCanInsert(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableCreateDialogDo(view->getContext());
	}; 
};

class InsertTableRowCommand : public XFE_EditorViewCommand
{
public:
	InsertTableRowCommand() : XFE_EditorViewCommand(xfeCmdInsertTableRow) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableRowCanInsert(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableRowInsert(view->getContext(), NULL);
	}; 
};

class InsertTableColumnCommand : public XFE_EditorViewCommand
{
public:
	InsertTableColumnCommand() : XFE_EditorViewCommand(xfeCmdInsertTableColumn) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableColumnCanInsert(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableColumnInsert(view->getContext(), NULL);
	}; 
};

class InsertTableCellCommand : public XFE_EditorViewCommand
{
public:
	InsertTableCellCommand() : XFE_EditorViewCommand(xfeCmdInsertTableCell) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableCellCanInsert(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableCellInsert(view->getContext(), NULL);
	}; 
};

class InsertHtmlCommand : public AlwaysEnabledCommand
{
public:
	InsertHtmlCommand() : AlwaysEnabledCommand(xfeCmdInsertHtml) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorHtmlPropertiesDialogDo(view->getContext());
	}; 
};

class InsertBreakBelowImageCommand : public AlwaysEnabledCommand
{
public:
	InsertBreakBelowImageCommand() :
		AlwaysEnabledCommand(xfeCmdInsertBreakBelowImage) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {

		fe_EditorLineBreak(view->getContext(), ED_BREAK_BOTH);

	}; 
};

class InsertNonBreakingSpaceCommand : public AlwaysEnabledCommand
{
public:
	InsertNonBreakingSpaceCommand() :
		AlwaysEnabledCommand(xfeCmdInsertNonBreakingSpace) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {

		fe_EditorNonBreakingSpace(view->getContext());
	}; 
};

struct parse_tokens_st {
    char*    name;
    unsigned bits;
};

static unsigned
tokens_cmp(parse_tokens_st* mapping, char* s, unsigned& bits)
{
    char *p;
    char *q;
    int i;
	unsigned nmatches = 0;
	
	bits = 0;

    s = strdup(s); /* lazy */
	
    for (p = s; *p; ) {
		q = strchr(p, '|'); /* look for or */

		if (q)
			*q++ = '\0'; /* punch out or */
		else
			q = &p[strlen(p)];
		
		/*
		 *    Check the token.
		 */
		for (i = 0; mapping[i].name != NULL; i++) {
			if (strcmp(p, mapping[i].name) == 0) {
				bits |= mapping[i].bits;
				nmatches++;
				break;
			}
		}

		p = q;
    }
	
	/* Use first (default) value for no match */
	if (nmatches == 0)
		bits = mapping[0].bits;
		
    free(s);
	
    return nmatches;
}

static parse_tokens_st font_size_tokens[] = {
	{ "default", ED_FONTSIZE_ZERO },
	{ "-2",      ED_FONTSIZE_MINUS_TWO },
	{ "-1",      ED_FONTSIZE_MINUS_ONE },
	{  "0",      ED_FONTSIZE_ZERO },
	{ "+0",      ED_FONTSIZE_ZERO },
	{ "+1",      ED_FONTSIZE_PLUS_ONE },
	{ "+2",      ED_FONTSIZE_PLUS_TWO },
	{ "+3",      ED_FONTSIZE_PLUS_THREE },
	{ "+4",      ED_FONTSIZE_PLUS_FOUR },
	{ NULL }
};

static XFE_CommandParameters set_font_size_params[] = {
	{ "-2",      (void*)ED_FONTSIZE_MINUS_TWO  },
	{ "-1",      (void*)ED_FONTSIZE_MINUS_ONE  },
	{ "+0",      (void*)ED_FONTSIZE_ZERO       },
	{ "+1",      (void*)ED_FONTSIZE_PLUS_ONE   },
	{ "+2",      (void*)ED_FONTSIZE_PLUS_TWO   },
	{ "+3",      (void*)ED_FONTSIZE_PLUS_THREE },
	{ "+4",      (void*)ED_FONTSIZE_PLUS_FOUR  },
	{ NULL }
};


int
XFE_CommandParametersGetIndexByName(XFE_CommandParameters* list, char* name)
{
	unsigned i;
	for (i = 0; list[i].name != NULL; i++) {
		if (XP_STRCASECMP(name, list[i].name) == 0)
			return i;
	}
	return -1;
}

int
XFE_CommandParametersGetIndexByData(XFE_CommandParameters* list, void* data)
{
	unsigned i;
	for (i = 0; list[i].name != NULL; i++) {
		if (data == list[i].data)
			return i;
	}
	return -1;
}

static Boolean
style_is_determinate(MWContext* context, ED_TextFormat foo)
{
    EDT_CharacterData* edt_cdata = EDT_GetCharacterData(context);
	Boolean rv = FALSE;

	if (edt_cdata != NULL) {
		rv = ((edt_cdata->mask & foo) != 0);
		EDT_FreeCharacterData(edt_cdata);
	}
    
    return rv;
}

class SetFontSizeCommand : public AlwaysEnabledCommand
{
public:
	SetFontSizeCommand() : AlwaysEnabledCommand(xfeCmdSetFontSize) {};
	SetFontSizeCommand(char* name) : AlwaysEnabledCommand(name) {};

	virtual XP_Bool isDynamic() { return TRUE; };

	XP_Bool isDeterminate(XFE_View* view, XFE_CommandInfo*) {
		return style_is_determinate(view->getContext(), TF_FONT_SIZE);
	};
	XP_Bool isSelected(XFE_View* view, ED_FontSize size) {
		return size == fe_EditorFontSizeGet(view->getContext());
	}
	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo* info) {
		int i;
		if (info == NULL || info->params == NULL)
			return FALSE;

		for (i = 0; set_font_size_params[i].name != NULL; i++) {
			char*       name = set_font_size_params[i].name;
			ED_FontSize size = (ED_FontSize)set_font_size_params[i].data;
			if (XP_STRCMP(name, info->params[0]) == 0) {
				return isSelected(view, size);
			}
		}
		return FALSE;
	}
	XFE_CommandParameters* getParameters(XFE_View*) {
		return (XFE_CommandParameters*)set_font_size_params;
	}
	int getParameterIndex(XFE_View* view) {
		MWContext*  context;
		ED_FontSize size;
		unsigned    i;

		if (!isDeterminate(view, NULL))
			return -1;

		context = view->getContext();
		size = fe_EditorFontSizeGet(context);

		if (size == ED_FONTSIZE_DEFAULT)
			return 2;

		for (i = 0; set_font_size_params[i].name != NULL; i++) {
			ED_FontSize match = (ED_FontSize)set_font_size_params[i].data;
			if (match == size)
				return i;
		}
		return -1;
	}
	void    set(XFE_View* view, ED_FontSize size = ED_FONTSIZE_ZERO) {
		fe_EditorFontSizeSet(view->getContext(), size);
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* calldata) {
		//   FIXME
		XFE_CommandInfo* info = (XFE_CommandInfo*)calldata;
		ED_FontSize size = ED_FONTSIZE_ZERO; //  default

		if (info) {
			String* params = info->params;
			Cardinal nparams = *info->nparams;
			unsigned foo;

			if (nparams > 0) {
				MWContext* context = view->getContext();

				if (XP_STRCASECMP(params[0], "increase") == 0) {
					size = (ED_FontSize)
						((unsigned)fe_EditorFontSizeGet(context) + 1);
				} else if (XP_STRCASECMP(params[0], "decrease") == 0) {
					size = (ED_FontSize)
						((unsigned)fe_EditorFontSizeGet(context) - 1);
				} else if (tokens_cmp(font_size_tokens, params[0], foo)!=0) {
					size = (ED_FontSize)foo;
				} else { /* junk */
					FE_SYNTAX_ERROR();
					return;
				}
			}

			set(view, size);
		}

	};
	
};

class SetFontFaceCommand : public AlwaysEnabledCommand
{
private:
	XFE_CommandParameters* m_params;
public:
	SetFontFaceCommand() : AlwaysEnabledCommand(xfeCmdSetFontFace) {
		m_params = NULL;
	}
	SetFontFaceCommand(char* name) : AlwaysEnabledCommand(name) {
		m_params = NULL;
	}
	virtual XP_Bool isDynamic() { return TRUE; };
	XP_Bool isDeterminate(XFE_View* view, XFE_CommandInfo*) {
		return style_is_determinate(view->getContext(), TF_FONT_FACE);
	};
	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo* info) {
		XP_Bool rv = FALSE;

		if (info != NULL && info->params != NULL) {
			
			XFE_CommandParameters* params = getParameters(view);
			int index = XFE_CommandParametersGetIndexByName(params,
															info->params[0]);

			if (index == get(view))
				rv = TRUE;
		}
		return rv;
	}
	char*   getLabel(XFE_View*, XFE_CommandInfo* info) {
		char* label = NULL;
		if (info != NULL && info->params != NULL)
			label = info->params[0];
		return label;
	};
	int get(XFE_View* view) {
		MWContext* context = view->getContext();
		int index = EDT_GetFontFaceIndex(context);
		if (ED_FONT_LOCAL == index) {
			char* name = EDT_GetFontFace(context);
			XFE_CommandParameters* params = getParameters(view);
			index = XFE_CommandParametersGetIndexByName(params, name);
			if (index == -1)
				index = 0;
		}
		return index;
	}
	void set(XFE_View* view, int index) {
		MWContext* context = view->getContext();
		XFE_CommandParameters* params = getParameters(view);
		char* name = params[index].name;
		EDT_SetFontFace(context, NULL, index, name);
	}
	int getParameterIndex(XFE_View* view) {
		return get(view);
	}
	XFE_CommandParameters* getParameters(XFE_View* view);
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info);
};

static unsigned
edt_list_count(char* list)
{
	unsigned n;
	char* p;

	if (list == NULL)
		return 0;

	for (n = 0, p = list; *p != '\0'; p += XP_STRLEN(p) + 1, n++)
		;

	return n;
}

static char*
edt_list_next(char* list)
{
	char* p;

	if (list == NULL)
		return NULL;

	for (p = list; *p != '\0'; p++)
		;

	p++;

	if (*p == '\0')
		return NULL;
	else
		return p;
}

XFE_CommandParameters*
SetFontFaceCommand::getParameters(XFE_View*)
{
	if (m_params != NULL)
		return m_params;

	char* faces = EDT_GetFontFaces();
	if (faces == NULL)
		faces = "";

	unsigned n;
	char* p;
	for (n = 0, p = faces; *p != '\0'; p += strlen(p) + 1, n++)
		;

	m_params = (XFE_CommandParameters*)XP_ALLOC(sizeof(XFE_CommandParameters)
												*
												(edt_list_count(faces)+1));

	for (p = faces, n = 0; p != NULL; p = edt_list_next(p), n++) {
		m_params[n].name = XP_STRDUP(p); // probably dup.
		m_params[n].data = (void*)n;
	}
	m_params[n].name = NULL;

	return m_params;
}

void
SetFontFaceCommand::reallyDoCommand(XFE_View* view, XFE_CommandInfo* info)
{
	int index = 0;

	if (info != NULL && info->params != NULL) {
		index = XFE_CommandParametersGetIndexByName(getParameters(view),
													info->params[0]);
		if (index == -1) {
			FE_SYNTAX_ERROR();
			return;
		}
	}
	
	set(view, index);
}

class SetFontColorCommand : public AlwaysEnabledCommand
{
public:
	SetFontColorCommand() : AlwaysEnabledCommand(xfeCmdSetFontColor) {
		m_params = NULL;
	};

	virtual XP_Bool isDynamic() { return TRUE; };
	XP_Bool isDeterminate(XFE_View* view, XFE_CommandInfo*) {
		return style_is_determinate(view->getContext(), TF_FONT_COLOR);
	};
	XFE_CommandParameters* getParameters(XFE_View* view);
	int getParameterIndex(XFE_View* view);
	void setParameterIndex(XFE_View* view, unsigned index);
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info);
private:
	XFE_CommandParameters* m_params;
};

XFE_CommandParameters*
SetFontColorCommand::getParameters(XFE_View*)
{
    unsigned n;

	if (m_params != NULL)
		return m_params;

	m_params = (XFE_CommandParameters*)XP_ALLOC(sizeof(XFE_CommandParameters)
												*
												(MAX_NS_COLORS+2));

    m_params[0].name = NULL;
    m_params[0].data = (void*)0;
    LO_Color loColor;

	for (n = 1; n <= MAX_NS_COLORS; n++)
    {
        EDT_GetNSColor(n-1, &loColor);

        m_params[n].name = NULL;
        m_params[n].data = (void*)RGB_TO_WORD(loColor.red, loColor.green,
                                              loColor.blue);
    }
    m_params[MAX_NS_COLORS+1].name = NULL;
    m_params[MAX_NS_COLORS+1].data = (void*)0;

    return m_params;
}

int
SetFontColorCommand::getParameterIndex(XFE_View* view)
{
	LO_Color color;
	int index;

	if (!isDeterminate(view, NULL))
		return -1;

	MWContext* context = view->getContext();

	if (fe_EditorColorGet(context, &color)) {
		index = EDT_GetMatchingFontColorIndex(&color);
	} else {
		index = 0; // default color.
	}
	return index;
}

void
SetFontColorCommand::setParameterIndex(XFE_View* view, unsigned index)
{
	MWContext* context = view->getContext();
	LO_Color color;
	unsigned n;
	XFE_CommandParameters* colors = getParameters(view);

	if (index == 0) {
		fe_EditorColorSet(context, NULL);
		return;
	}

	//
	//    Use this because the stupid back-end routine takes forever
	//    (yes ladies and gentlemen it re-parses the colors), and
	//    crashes a lot.
	//
	for (n = 1; n < index && colors[n].name != NULL; n++)
		;
	
	if (colors[n].name != NULL) { // did not fall off end of list
		unsigned long word = (unsigned long)colors[n].data;
		color.red = WORD_TO_R(word);
		color.green = WORD_TO_G(word);
		color.blue = WORD_TO_B(word);
		fe_EditorColorSet(context, &color);
	}
}

void
SetFontColorCommand::reallyDoCommand(XFE_View* view, XFE_CommandInfo* info)
{
	uint8 red;
	uint8 green;
	uint8 blue;
	LO_Color color;
	if (info != NULL && *info->nparams > 0) {
		if (LO_ParseRGB(info->params[0], &red, &green, &blue)) {
			color.red = red;
			color.green = green;
			color.blue = blue;
			fe_EditorColorSet(view->getContext(), &color);
		}
	}
}

static parse_tokens_st paragraph_style_tokens[] = {
    { "normal",          P_NSDT       },
    { "headingOne",      P_HEADER_1   },
    { "headingTwo",      P_HEADER_2   },
    { "headingThree",    P_HEADER_3   },
    { "headingFour",     P_HEADER_4   },
    { "headingFive",     P_HEADER_5   },
    { "headingSix",      P_HEADER_6   },
    { "address",         P_ADDRESS    },
    { "formatted",       P_PREFORMAT  },
    { "listItem",        P_LIST_ITEM  },
    { "descriptionItem", P_DESC_TITLE },
    { "descriptionText", P_DESC_TEXT  },
	{ 0 }
};

class SetParagraphStyleCommand : public AlwaysEnabledCommand
{
public:
	SetParagraphStyleCommand() :
		AlwaysEnabledCommand(xfeCmdSetParagraphStyle) {};
	SetParagraphStyleCommand(char* name) : AlwaysEnabledCommand(name) {};

	virtual XP_Bool isDynamic() { return TRUE; };

	XFE_CommandParameters* getParameters(XFE_View*) {
		return (XFE_CommandParameters*)paragraph_style_tokens;
	}
	int getParameterIndex(XFE_View* view) {
		TagType foo = fe_EditorParagraphPropertiesGet(view->getContext());
		return XFE_CommandParametersGetIndexByData(getParameters(view),
												   (void*)foo);
	}
	void    set(XFE_View* view, TagType type = P_NSDT) {
		fe_EditorParagraphPropertiesSet(view->getContext(), type);
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*);
};

void
SetParagraphStyleCommand::reallyDoCommand(XFE_View* view, XFE_CommandInfo* info)
{
	TagType type = P_NSDT;

	if (info) {
		String* params = info->params;
		Cardinal nparams = *info->nparams;
		unsigned foo;

		if (nparams > 0) {
			if (tokens_cmp(paragraph_style_tokens, params[0], foo) != 0) {
				type = (TagType)foo;
			} else {
				FE_SYNTAX_ERROR();
				return;
			}
		}
		
		set(view, type);
	}
}

static parse_tokens_st list_style_tokens[] = {
    { "unnumbered",      P_UNUM_LIST  },
    { "numbered",        P_NUM_LIST   },
    { "directory",       P_DIRECTORY  },
    { "menu",            P_MENU       },
    { "description",     P_DESC_LIST  },
	{ 0 }
};

class SetListStyleCommand : public AlwaysEnabledCommand
{
public:
	SetListStyleCommand() :	AlwaysEnabledCommand(xfeCmdSetListStyle) {};
	SetListStyleCommand(char* name) : AlwaysEnabledCommand(name) {};

	void    set(XFE_View* view, TagType type = P_UNUM_LIST) {
		fe_EditorToggleList(view->getContext(), type);
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* calldata) {
		//   FIXME
		XFE_CommandInfo* info = (XFE_CommandInfo*)calldata;
		TagType type = P_UNUM_LIST;

		if (info) {
			String* params = info->params;
			Cardinal nparams = *info->nparams;
			unsigned foo;

			if (nparams > 0) {
				if (tokens_cmp(list_style_tokens, params[0], foo) != 0) {
					type = (TagType)foo;
				} else {
					FE_SYNTAX_ERROR();
					return;
				}
			}

			set(view, type);
		}
	};
};

static parse_tokens_st align_style_tokens[] = {
    { "left",      ED_ALIGN_LEFT  },
    { "center",    ED_ALIGN_ABSCENTER   },
    { "right",     ED_ALIGN_RIGHT  },
	{ 0 }
};

class SetAlignStyleCommand : public AlwaysEnabledCommand
{
public:
	SetAlignStyleCommand() :	
		AlwaysEnabledCommand(xfeCmdSetAlignmentStyle) {};
	SetAlignStyleCommand(char* name) : AlwaysEnabledCommand(name) {};

	virtual XP_Bool isDynamic() { return TRUE; };

	XFE_CommandParameters* getParameters(XFE_View*) {
		return (XFE_CommandParameters*)align_style_tokens;
	}
	int getParameterIndex(XFE_View* view) {
		ED_Alignment align = fe_EditorAlignGet(view->getContext());
		int index;

		if (align == ED_ALIGN_LEFT)
			index = 0;
		else if (align == ED_ALIGN_ABSCENTER)
			index = 1;
		else if (align == ED_ALIGN_RIGHT)
			index = 2;
		else
			index = -1;

		return index;
	}
	void setParameterIndex(XFE_View* view, unsigned index) {

		if (index < 3) {
			ED_Alignment align = (ED_Alignment)align_style_tokens[index].bits;
			fe_EditorAlignSet(view->getContext(), align);
		}
	}
	void    set(XFE_View* view, ED_Alignment align = ED_ALIGN_LEFT) {
		fe_EditorAlignSet(view->getContext(), align);
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
		ED_Alignment align = ED_ALIGN_LEFT;

		if (info) {
			String* params = info->params;
			Cardinal nparams = *info->nparams;
			unsigned foo;

			if (nparams > 0) {
				if (tokens_cmp(align_style_tokens, params[0], foo) != 0) {
					align = (ED_Alignment)foo;
				} else {
					FE_SYNTAX_ERROR();
					return;
				}
			}

			set(view, align);
		}
	};
};

static parse_tokens_st char_style_tokens[] = {
    { "bold", TF_BOLD },
    { "italic", TF_ITALIC },
    { "underline", TF_UNDERLINE },
    { "fixed",  TF_FIXED },
    { "superscript",  TF_SUPER },
    { "subscript",    TF_SUB   },
    { "strikethrough", TF_STRIKEOUT },
    { "blink",  TF_BLINK },
    { "nobreak",  TF_NOBREAK },
    { "none",   TF_NONE }, /* probably does nothing, but TF_NONE may change */
	{ 0 }
};

class SetCharacterStyleCommand : public AlwaysEnabledCommand
{
protected:
	unsigned parse_info(XFE_CommandInfo* info, unsigned& style);

	XP_Bool isDynamic() { return TRUE; };

	void    set(XFE_View* view, unsigned style = TF_NONE) {
		MWContext* context = view->getContext();
		
		if (style == TF_NONE)
			fe_EditorDoPoof(context);
		else
			fe_EditorCharacterPropertiesSet(context, style);
	}
	void    toggle(XFE_View* view, unsigned mask) {
		MWContext* context = view->getContext();
		unsigned values = (unsigned)fe_EditorCharacterPropertiesGet(context);

		values ^= mask;

		/*
		 *    Super and Sub should be mutually exclusive.
		 */
		if (mask == TF_SUPER && (values & TF_SUPER) != 0)
			values &= ~TF_SUB;
		else if (mask == TF_SUB && (values & TF_SUB) != 0)
			values &= ~TF_SUPER;

		fe_EditorCharacterPropertiesSet(context, values);
	}
public:
	SetCharacterStyleCommand() :	
		AlwaysEnabledCommand(xfeCmdSetCharacterStyle) {};
	SetCharacterStyleCommand(char* name) : AlwaysEnabledCommand(name) {};

	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* calldata);
};

//
//    I don't know why, but the SCO C++ compiler seems to object to
//    (just) this function being inline. The compiler complains that
//    it cannot deal with the return statement, yet this function
//    is nearly identical to many other doCommand() methods in the
//    file! Huh? ...djw Jan/23/97
//
void
SetCharacterStyleCommand::reallyDoCommand(XFE_View* view, XFE_CommandInfo* calldata) {

	XFE_CommandInfo* info = (XFE_CommandInfo*)calldata;
	unsigned style = 0x0;
	
	if (parse_info(info, style) == 0) {
		FE_SYNTAX_ERROR();
		return;
	}

	set(view, style);
}

//
//  Here's another problem inline function for the HP-UX
//  and SCO C++ compilers.
//  They don't like the two return statements.  -slamm 2-3-97
//
unsigned
SetCharacterStyleCommand::parse_info(XFE_CommandInfo* info, unsigned& style) {

  style = 0x0;
  
  if (info) {
    String* params = info->params;
    Cardinal nparams = *info->nparams;
    
    if (nparams >= 1)
      return tokens_cmp(char_style_tokens, params[0], style);
  }
  return 0;
}


class ToggleCharacterStyleCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleCommand() :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyle) {};

	XP_Bool isDynamic() { return TRUE; };

   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo* calldata);

	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* calldata) {
		XFE_CommandInfo* info = (XFE_CommandInfo*)calldata;
		unsigned style = 0x0;
	
		if (parse_info((XFE_CommandInfo*)calldata, style) == 0) {
			FE_SYNTAX_ERROR();
		} else {
			toggle(view, style);
		}
	}
};

//
//  Here's another problem inline function for the HP-UX
//  and SCO C++ compilers.    -slamm 2-3-97
//
Boolean
ToggleCharacterStyleCommand::isSelected(XFE_View* view, XFE_CommandInfo* calldata) {
  XFE_CommandInfo* info = (XFE_CommandInfo*)calldata;
  unsigned style;
  
  if (parse_info(info, style) == 0) {
    FE_SYNTAX_ERROR();
    return False;
  }
  MWContext* context = view->getContext();
  
  return (style & fe_EditorCharacterPropertiesGet(context)) != 0;
}

class IndentCommand : public AlwaysEnabledCommand
{
public:
	IndentCommand() : AlwaysEnabledCommand(xfeCmdIndent) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorIndent(view->getContext(), False);
	}
};

class OutdentCommand : public AlwaysEnabledCommand
{
public:
	OutdentCommand() : AlwaysEnabledCommand(xfeCmdOutdent) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorIndent(view->getContext(), True);
	}
};

class SetObjectPropertiesCommand : public AlwaysEnabledCommand
{
public:
	SetObjectPropertiesCommand() :
		AlwaysEnabledCommand(xfeCmdSetObjectProperties) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorObjectPropertiesDialogDo(view->getContext());
	}
	char*   getLabel(XFE_View* view, XFE_CommandInfo* info) {

		MWContext* context = view->getContext();
		char* name;
		ED_ElementType e_type = EDT_GetCurrentElementType(context);

		switch (e_type) {
		case ED_ELEMENT_HRULE:
			name = "hruleProperties";
			break;
		case ED_ELEMENT_IMAGE:
			name = "imageProperties";
			break;
		case ED_ELEMENT_UNKNOWN_TAG:
			name = "tagProperties";
			break;
		case ED_ELEMENT_TARGET:
			name = "targetProperties";
			break;
		default:
			name = "textProperties";
		}

		Widget widget = NULL;

		if (info != NULL)
			widget = info->widget;

		return view->getLabelString(name, widget);
	}
};

class SetPagePropertiesCommand : public AlwaysEnabledCommand
{
public:
	SetPagePropertiesCommand() : 
		AlwaysEnabledCommand(xfeCmdSetPageProperties) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorDocumentPropertiesDialogDo(view->getContext(),
								XFE_EDITOR_DOCUMENT_PROPERTIES_APPEARANCE);
	}
};

class SetTablePropertiesCommand : public XFE_EditorViewCommand
{
public:
	SetTablePropertiesCommand() : XFE_EditorViewCommand(xfeCmdSetTableProperties) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableCanDelete(view->getContext());
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTablePropertiesDialogDo(view->getContext(),
										 XFE_EDITOR_PROPERTIES_TABLE);
	}
};

#ifndef NO_SECURITY
class SetEncryptedCommand : public XFE_EditorViewCommand
{
public:
	SetEncryptedCommand() : XFE_EditorViewCommand(xfeCmdSetEncrypted) {};

	XP_Bool isDynamic() { return TRUE; };

   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo* /*calldata*/) {
	    return (XP_Bool) EDT_EncryptState(view->getContext());
	}
	XP_Bool isEnabled(XFE_View* /*view*/, XFE_CommandInfo*) {
		return (XP_Bool) PK11_TokenExists(CKM_SKIPJACK_CBC64);
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		EDT_EncryptToggle(view->getContext());
										 
	}
};
#endif

class SetCharacterColorCommand : public AlwaysEnabledCommand
{
public:
	SetCharacterColorCommand() :
		AlwaysEnabledCommand(xfeCmdSetCharacterColor) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorSetColorsDialogDo(view->getContext());
	}
};

class JavaPluginCommand : public AlwaysEnabledCommand
{
private:
	int32 category_id;
	int32 plugin_id;
public:
	JavaPluginCommand(char* name, int32 cid, int32 pid) :
		AlwaysEnabledCommand(name) {
		category_id = cid;
		plugin_id = pid;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		EDT_PerformPlugin(view->getContext(), category_id, plugin_id, 0, 0);
	}
	char*   getLabel(XFE_View*, XFE_CommandInfo*) {
		return EDT_GetPluginName(category_id, plugin_id);
	};
	char*   getDocString(XFE_View*, XFE_CommandInfo*) {
		return EDT_GetPluginMenuHelp(category_id, plugin_id);
	};
};

class BrowsePageCommand : public AlwaysEnabledCommand
{
public:
	BrowsePageCommand() : AlwaysEnabledCommand(xfeCmdBrowsePage) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*);
};

void    
BrowsePageCommand::reallyDoCommand(XFE_View* view, XFE_CommandInfo*)
{
	MWContext *context = view->getContext();
	URL_Struct *url;

	if (!FE_CheckAndSaveDocument(context))
		return;

	url = SHIST_CreateURLStructFromHistoryEntry(context,
											SHIST_GetCurrent(&context->hist));
	FE_GetURL(context, url);
}

class EditSourceCommand : public XFE_EditorViewCommand
{
public:
	EditSourceCommand() : XFE_EditorViewCommand(xfeCmdEditPageSource) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorDocumentIsSaved(view->getContext());
	}

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorEditSource(view->getContext());
	}
};

class ViewSourceCommand : public AlwaysEnabledCommand
{
public:
	ViewSourceCommand() : AlwaysEnabledCommand(xfeCmdViewPageSource) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		    fe_EditorDisplaySource(view->getContext());
	}
};

//
//    These should all go away, when commands can be properly paramaterized.
//
class SetFontSizeMinusTwoCommand : public SetFontSizeCommand
{
public:
	SetFontSizeMinusTwoCommand() :
	   		SetFontSizeCommand(xfeCmdSetFontSizeMinusTwo) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorFontSizeGet(context) == ED_FONTSIZE_MINUS_TWO;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_FONTSIZE_MINUS_TWO);
	};
};

class SetFontSizeMinusOneCommand : public SetFontSizeCommand
{
public:
	SetFontSizeMinusOneCommand() :
		SetFontSizeCommand(xfeCmdSetFontSizeMinusOne) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorFontSizeGet(context) == ED_FONTSIZE_MINUS_ONE;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_FONTSIZE_MINUS_ONE);
	};
};

class SetFontSizeZeroCommand : public SetFontSizeCommand
{
public:
	SetFontSizeZeroCommand() :
		SetFontSizeCommand(xfeCmdSetFontSizeZero) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorFontSizeGet(context) == ED_FONTSIZE_ZERO;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_FONTSIZE_ZERO);
	};
};

class SetFontSizePlusOneCommand : public SetFontSizeCommand
{
public:
	SetFontSizePlusOneCommand() :
		SetFontSizeCommand(xfeCmdSetFontSizePlusOne) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorFontSizeGet(context) == ED_FONTSIZE_PLUS_ONE;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_FONTSIZE_PLUS_ONE);
	};
};

class SetFontSizePlusTwoCommand : public SetFontSizeCommand
{
public:
	SetFontSizePlusTwoCommand() :
		SetFontSizeCommand(xfeCmdSetFontSizePlusTwo) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorFontSizeGet(context) == ED_FONTSIZE_PLUS_TWO;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_FONTSIZE_PLUS_TWO);
	};
};

class SetFontSizePlusThreeCommand : public SetFontSizeCommand
{
public:
	SetFontSizePlusThreeCommand() :
		SetFontSizeCommand(xfeCmdSetFontSizePlusThree) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorFontSizeGet(context) == ED_FONTSIZE_PLUS_THREE;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_FONTSIZE_PLUS_THREE);
	};
};

class SetFontSizePlusFourCommand : public SetFontSizeCommand
{
public:
	SetFontSizePlusFourCommand() :
		SetFontSizeCommand(xfeCmdSetFontSizePlusFour) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorFontSizeGet(context) == ED_FONTSIZE_PLUS_FOUR;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_FONTSIZE_PLUS_FOUR);
	};
};

class SetParagraphStyleNormalCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleNormalCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleNormal) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_NSDT;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view);
	};
};

class SetParagraphStyleHeadingOneCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleHeadingOneCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingOne) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_HEADER_1;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_HEADER_1);
	};
};

class SetParagraphStyleHeadingTwoCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleHeadingTwoCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingTwo) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_HEADER_2;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_HEADER_2);
	};
};

class SetParagraphStyleHeadingThreeCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleHeadingThreeCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingThree) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_HEADER_3;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_HEADER_3);
	};
};

class SetParagraphStyleHeadingFourCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleHeadingFourCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingFour) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_HEADER_4;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_HEADER_4);
	};
};

class SetParagraphStyleHeadingFiveCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleHeadingFiveCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingFive) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_HEADER_5;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_HEADER_5);
	};
};

class SetParagraphStyleHeadingSixCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleHeadingSixCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingSix) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_HEADER_6;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_HEADER_6);
	};
};

class SetParagraphStyleAddressCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleAddressCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleAddress) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_ADDRESS;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_ADDRESS);
	};
};

class SetParagraphStyleFormattedCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleFormattedCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleFormatted) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_PREFORMAT;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_PREFORMAT);
	};
};

class SetParagraphStyleTitleCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleTitleCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleDescriptionTitle) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_DESC_TITLE;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_DESC_TITLE);
	};
};

class SetParagraphStyleTextCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleTextCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleDescriptionText) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_DESC_TEXT;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_DESC_TEXT);
	};
};

class SetParagraphStyleBlockQuoteCommand : public SetParagraphStyleCommand
{
public:
	SetParagraphStyleBlockQuoteCommand() :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleBlockQuote) {};
	
	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		return fe_EditorParagraphPropertiesGet(context) == P_BLOCKQUOTE;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_BLOCKQUOTE);
	};
};

class SetListStyleNoneCommand : public SetListStyleCommand
{
public:
	SetListStyleNoneCommand() :
		SetListStyleCommand(xfeCmdSetListStyleNone) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		TagType    p_type;

		fe_EditorParagraphPropertiesGetAll(context, &p_type, NULL, NULL);

		return p_type != P_LIST_ITEM;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		
		fe_EditorParagraphPropertiesSetAll(context,
										   P_NSDT,
										   NULL,
										   fe_EditorAlignGet(context));
	};
};

class SetListStyleBulletedCommand : public SetListStyleCommand
{
public:
	SetListStyleBulletedCommand() :
		SetListStyleCommand(xfeCmdSetListStyleBulleted) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		TagType    p_type;
		EDT_ListData l_data;

		fe_EditorParagraphPropertiesGetAll(context, &p_type, &l_data, NULL);

		return (p_type == P_LIST_ITEM) && (l_data.iTagType == P_UNUM_LIST);
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_UNUM_LIST);
	};
};

class SetListStyleNumberedCommand : public SetListStyleCommand
{
public:
	SetListStyleNumberedCommand() :
		SetListStyleCommand(xfeCmdSetListStyleNumbered) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		TagType    p_type;
		EDT_ListData l_data;

		fe_EditorParagraphPropertiesGetAll(context, &p_type, &l_data, NULL);

		return (p_type == P_LIST_ITEM) && (l_data.iTagType == P_NUM_LIST);
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, P_NUM_LIST);
	};
};

class SetAlignStyleLeftCommand : public SetAlignStyleCommand
{
public:
	SetAlignStyleLeftCommand() :
		SetAlignStyleCommand(xfeCmdSetAlignmentStyleLeft) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorAlignGet(view->getContext()) == ED_ALIGN_LEFT;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_ALIGN_LEFT);
	};
};

class SetAlignStyleCenterCommand : public SetAlignStyleCommand
{
public:
	SetAlignStyleCenterCommand() :
		SetAlignStyleCommand(xfeCmdSetAlignmentStyleCenter) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorAlignGet(view->getContext()) == ED_ALIGN_ABSCENTER;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_ALIGN_ABSCENTER);
	};
};

class SetAlignStyleRightCommand : public SetAlignStyleCommand
{
public:
	SetAlignStyleRightCommand() :
		SetAlignStyleCommand(xfeCmdSetAlignmentStyleRight) {};

	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorAlignGet(view->getContext()) == ED_ALIGN_RIGHT;
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, ED_ALIGN_RIGHT);
	};
};

class ClearAllStylesCommand : public SetCharacterStyleCommand
{
public:
	ClearAllStylesCommand() :
		SetCharacterStyleCommand(xfeCmdClearAllStyles) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, TF_NONE);
	};
};

class ToggleCharacterStyleBoldCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleBoldCommand() :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleBold) {};

	XP_Bool isDeterminate(XFE_View* view, XFE_CommandInfo*) {
		return style_is_determinate(view->getContext(), TF_BOLD);
	};
   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();

		return (TF_BOLD & fe_EditorCharacterPropertiesGet(context)) != 0;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		toggle(view, TF_BOLD);
	};
};

class ToggleCharacterStyleUnderlineCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleUnderlineCommand() :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleUnderline) {};

	XP_Bool isDeterminate(XFE_View* view, XFE_CommandInfo*) {
		return style_is_determinate(view->getContext(), TF_UNDERLINE);
	};
   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();

		return (TF_UNDERLINE & fe_EditorCharacterPropertiesGet(context)) != 0;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		toggle(view, TF_UNDERLINE);
	};
};

class ToggleCharacterStyleItalicCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleItalicCommand() :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleItalic) {};

	XP_Bool isDeterminate(XFE_View* view, XFE_CommandInfo*) {
		return style_is_determinate(view->getContext(), TF_ITALIC);
	};
   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();

		return (TF_ITALIC & fe_EditorCharacterPropertiesGet(context)) != 0;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		toggle(view, TF_ITALIC);
	};
};

class ToggleCharacterStyleFixedCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleFixedCommand() :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleFixed) {};

   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();

		return (TF_FIXED & fe_EditorCharacterPropertiesGet(context)) != 0;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		toggle(view, TF_FIXED);
	};
};

class ToggleCharacterStyleSuperscriptCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleSuperscriptCommand() :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleSuperscript)
	{};

   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();

		return (TF_SUPER & fe_EditorCharacterPropertiesGet(context)) != 0;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		toggle(view, TF_SUPER);
	};
};

class ToggleCharacterStyleSubscriptCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleSubscriptCommand() :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleSubscript)
	{};

   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();

		return (TF_SUB & fe_EditorCharacterPropertiesGet(context)) != 0;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		toggle(view, TF_SUB);
	};
};

class ToggleCharacterStyleStrikethroughCommand :
	public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleStrikethroughCommand() :
	SetCharacterStyleCommand(xfeCmdToggleCharacterStyleStrikethrough) {
	};

   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();

		return (TF_STRIKEOUT & fe_EditorCharacterPropertiesGet(context))
			!= 0;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		toggle(view, TF_STRIKEOUT);
	};
};

class ToggleCharacterStyleBlinkCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleBlinkCommand() :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleBlink) {};

   	XP_Bool isSelected(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();

		return (TF_BLINK & fe_EditorCharacterPropertiesGet(context)) != 0;
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		toggle(view, TF_BLINK);
	};
};

enum {
	MOVE_NOWHERE = 0,
	MOVE_UP,       // previous line
	MOVE_DOWN,
	MOVE_NEXT,     // character
	MOVE_PREVIOUS, // character
	MOVE_BEGINNING_OF_LINE,
	MOVE_END_OF_LINE,
	MOVE_BEGINNING_OF_DOCUMENT,
	MOVE_END_OF_DOCUMENT,
	MOVE_NEXT_WORD,
	MOVE_PREVIOUS_WORD,
	MOVE_NEXT_TABLE_CELL,
	MOVE_PREVIOUS_TABLE_CELL
} MoveCommandDirection_t;

static parse_tokens_st move_command_directions[] = {
    { "next",            MOVE_NEXT },
    { "previous",        MOVE_PREVIOUS },
    { "up",              MOVE_UP },
    { "previousLine",    MOVE_UP },
    { "down",            MOVE_DOWN },
    { "nextLine",        MOVE_DOWN },
    { "beginningOfLine", MOVE_BEGINNING_OF_LINE },
    { "beginOfLine",     MOVE_BEGINNING_OF_LINE },
    { "endOfLine",       MOVE_END_OF_LINE },
    { "beginningOfPage", MOVE_BEGINNING_OF_DOCUMENT },
    { "endOfPage",       MOVE_END_OF_DOCUMENT },
    { "nextWord",        MOVE_NEXT_WORD },
    { "previousWord",    MOVE_PREVIOUS_WORD },
    { "nextCell",        MOVE_NEXT_TABLE_CELL },
    { "previousCell",    MOVE_PREVIOUS_TABLE_CELL },
	{ 0 },
};

class MoveCommand : public AlwaysEnabledCommand
{
public:
	MoveCommand() :	AlwaysEnabledCommand(xfeCmdMoveCursor) {};

	virtual XP_Bool isSlow() {
		return FALSE;
	};
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
		
		unsigned direction = MOVE_NOWHERE;
		MWContext* context = view->getContext();

		if (info != NULL && *info->nparams > 0) {
			char* param = info->params[0];

			if (tokens_cmp(move_command_directions, param, direction) != 1)
				direction = MOVE_NOWHERE;
		}

        Boolean shifted, ctrl;
        xfe_GetShiftAndCtrl(info->event, &shifted, &ctrl);

		switch (direction) {
		case MOVE_UP:       // previous line
			EDT_Up(context, shifted); break;
		case MOVE_DOWN:
			EDT_Down(context, shifted); break;
		case MOVE_NEXT:     // character
			EDT_NextChar(context, shifted); break;
		case MOVE_PREVIOUS: // character
			EDT_PreviousChar(context, shifted); break;
		case MOVE_BEGINNING_OF_LINE:
			EDT_BeginOfLine(context, shifted); break;
		case MOVE_END_OF_LINE:
			EDT_EndOfLine(context, shifted); break;
		case MOVE_BEGINNING_OF_DOCUMENT:
			EDT_BeginOfDocument(context, shifted); break;
		case MOVE_END_OF_DOCUMENT:
			EDT_EndOfDocument(context, shifted); break;
		case MOVE_NEXT_WORD:
			EDT_NextWord(context, shifted); break;
		case MOVE_PREVIOUS_WORD:
			EDT_PreviousWord(context, shifted); break;
		case MOVE_NEXT_TABLE_CELL:
			EDT_NextTableCell(context, shifted); break;
		case MOVE_PREVIOUS_TABLE_CELL:
			EDT_PreviousTableCell(context, shifted); break;
		default:
			FE_SYNTAX_ERROR();
		}
	};
};

class KeyInsertCommand : public AlwaysEnabledCommand
{
public:
	KeyInsertCommand() :	AlwaysEnabledCommand(xfeCmdKeyInsert) {};

	virtual XP_Bool requiresChromeUpdate() {
		return FALSE;
	};
	virtual XP_Bool isSlow() {
		return FALSE;
	};
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
		MWContext* context = view->getContext();
		if (info != NULL) {
			fe_EditorKeyInsert(context, info->widget, info->event);
		}
	};
};

class InsertReturnCommand : public AlwaysEnabledCommand
{
public:
	InsertReturnCommand() :	AlwaysEnabledCommand(xfeCmdInsertReturn) {};

	virtual XP_Bool isSlow() {
		return FALSE;
	};
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		EDT_ReturnKey(context);
	};
};

static parse_tokens_st insert_line_break_types[] = {
	{ "normal", (unsigned)ED_BREAK_NORMAL },
	{ "left",   (unsigned)ED_BREAK_LEFT   },
	{ "right",  (unsigned)ED_BREAK_RIGHT  },
	{ "both",   (unsigned)ED_BREAK_BOTH   },
	{ NULL }
};

class InsertLineBreakCommand : public AlwaysEnabledCommand
{
public:
	InsertLineBreakCommand() : AlwaysEnabledCommand(xfeCmdInsertLineBreak) {};
	
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
		ED_BreakType type = ED_BREAK_NORMAL;
		MWContext* context = view->getContext();
		
		if (info != NULL && *info->nparams > 0) {
			char* param = info->params[0];
			unsigned foo;

			if (tokens_cmp(insert_line_break_types, param, foo) != 1)
				type = (ED_BreakType)-1;
			else
				type = (ED_BreakType)foo;
		}
		
		switch (type) {
		case ED_BREAK_NORMAL:
		case ED_BREAK_LEFT:
		case ED_BREAK_RIGHT:
		case ED_BREAK_BOTH:
			fe_EditorLineBreak(context, type);
			break;
		default:
			FE_SYNTAX_ERROR();
		}
	};
};

class TabCommand : public AlwaysEnabledCommand
{
public:
	TabCommand() : AlwaysEnabledCommand(xfeCmdTab) {};

	virtual XP_Bool isSlow() {
		return FALSE;
	};

	virtual void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {

		Boolean forward = TRUE;
		Boolean force = FALSE;
		Boolean error = FALSE;

		if (info != NULL && *info->nparams > 0) {
			if (XP_STRCASECMP(info->params[0], "forward") == 0)
				forward = TRUE;
			else if (XP_STRCASECMP(info->params[0], "backward") == 0)
				forward = FALSE;
			else if (XP_STRCASECMP(info->params[0], "insert") == 0)
				force = TRUE;
			else {
				error = TRUE;
				FE_SYNTAX_ERROR();
			}
		}
		if (!error)
			fe_EditorTab(view->getContext(), forward, force);
	}; 
};

class SelectCommand : public XFE_ViewCommand
{
public:
	SelectCommand() : XFE_ViewCommand(xfeCmdSelect) {};
	
	virtual XP_Bool isSlow() {
		return FALSE;
	};

	//
	//    This guy impliments doCommand() itself so that it can manage
	//    update chrome dynamically.
	//
	void    doCommand(XFE_View* view, XFE_CommandInfo* info);
};

void
SelectCommand::doCommand(XFE_View* view, XFE_CommandInfo* info) {
	MWContext* context = view->getContext();
	
	if (info != NULL && *info->nparams > 0) {
		char*    param = info->params[0];
		unsigned do_chrome = 0;
		
		if (XP_STRCASECMP(param, "grab") == 0) {
			fe_EditorGrabFocus(context, info->event);
			do_chrome++;
		} else if (XP_STRCASECMP(param, "begin") == 0) {
			fe_EditorSelectionBegin(context, info->event);
			do_chrome++;
		} else if (XP_STRCASECMP(param, "extend") == 0) {
			fe_EditorSelectionExtend(context, info->event);
		} else if (XP_STRCASECMP(param, "end") == 0) {
			fe_EditorSelectionEnd(context, info->event);
			do_chrome++;
		} else if (XP_STRCASECMP(param, "word") == 0) {
			unsigned long x;
			unsigned long y;
			fe_EventLOCoords(context, info->event, &x, &y);
			EDT_DoubleClick(context, x, y); /* this takes screen co-ords?? */
			do_chrome++;
		} else {
			FE_SYNTAX_ERROR();
		}
		
		if (do_chrome != 0) {
			XFE_Component* top = view->getToplevel();
			top->notifyInterested(XFE_View::chromeNeedsUpdating);
		}
	}
}

class EditorObjectIsCommand : public XFE_ObjectIsCommand
{
public:
	char* getObjectType(XFE_View*);
};

char*
EditorObjectIsCommand::getObjectType(XFE_View* view)
{
    ED_ElementType e_type; /* jag */
	
    e_type = EDT_GetCurrentElementType(view->getContext());
	
    switch (e_type) {
    case ED_ELEMENT_TEXT:        return "text";
    case ED_ELEMENT_IMAGE:       return "image";
    case ED_ELEMENT_UNKNOWN_TAG: return "tag";
    case ED_ELEMENT_TARGET:      return "target";
    case ED_ELEMENT_HRULE:       return "hrule";
    default:                     return "unknown";
    }
}

class DialogCommand : public XFE_ViewCommand
{
public:
	DialogCommand() : XFE_ViewCommand(xfeCmdDialog) {};
	
	void doCommand(XFE_View* view, XFE_CommandInfo* info) {

		MWContext* context = view->getContext();
		unsigned   nerrors = 0;
		String*    av = info->params;

		if (info != NULL && *info->nparams == 1) {
			if (XP_STRCASECMP(av[0], "text") == 0)
				fe_EditorPropertiesDialogDo(context,
											XFE_EDITOR_PROPERTIES_CHARACTER);
			else if (XP_STRCASECMP(av[0], "paragraph") == 0)
				fe_EditorPropertiesDialogDo(context,
											XFE_EDITOR_PROPERTIES_PARAGRAPH);
			else if (XP_STRCASECMP(av[0], "link") == 0)
				fe_EditorPropertiesDialogDo(context,
											XFE_EDITOR_PROPERTIES_LINK);
			else if (XP_STRCASECMP(av[0], "target") == 0)
				fe_EditorTargetPropertiesDialogDo(context);
			else if (XP_STRCASECMP(av[0], "image") == 0)
				fe_EditorPropertiesDialogDo(context,
											XFE_EDITOR_PROPERTIES_IMAGE);
			else if (XP_STRCASECMP(av[0], "hrule") == 0)
				fe_EditorHorizontalRulePropertiesDialogDo(context);
			else if (XP_STRCASECMP(av[0], "tag") == 0)
				fe_EditorHtmlPropertiesDialogDo(context);
			else if (XP_STRCASECMP(av[0], "document") == 0)
				fe_EditorDocumentPropertiesDialogDo(context,
							XFE_EDITOR_DOCUMENT_PROPERTIES_APPEARANCE);
			else if (XP_STRCASECMP(av[0], "table") == 0)
				fe_EditorTablePropertiesDialogDo(context,
												 XFE_EDITOR_PROPERTIES_TABLE);
			else if (XP_STRCASECMP(av[0], "table-insert") == 0)
				fe_EditorTableCreateDialogDo(context);
			else if (XP_STRCASECMP(av[0], "color") == 0)
				fe_EditorSetColorsDialogDo(context);
			else if (XP_STRCASECMP(av[0], "publish") == 0)
				fe_EditorPublishDialogDo(context);
			else
				nerrors++;
		} else {
			nerrors++;
		}

		if (nerrors != 0)
			FE_SYNTAX_ERROR();
	};
};

static void
fe_get_selected_text_rect(MWContext* context,
			  LO_TextStruct* text,
			  int32*         rect_x,
			  int32*         rect_y,
			  int32*         rect_width,
			  int32*         rect_height)
{
    PA_Block    text_save = text->text;
    int         len_save = text->text_len;
    LO_TextInfo info;
    
    text->text_len = text->sel_start;
    XFE_GetTextInfo(context, text, &info);

    *rect_x = text->x + text->x_offset + info.max_width;
    *rect_y = text->y + text->y_offset;

    text->text = (PA_Block)((char*)text->text + text->sel_start);
    text->text_len = text->sel_end - text->sel_start;
    
    XFE_GetTextInfo(context, text, &info);

    *rect_width = info.max_width;
    *rect_height = info.ascent + info.descent;

    text->text = text_save;
    text->text_len = len_save;
}

static Boolean
fe_editor_selection_contains_point(MWContext* context, int32 x, int32 y)
{
    int32 start_selection;
    int32 end_selection;
    LO_Element* start_element = NULL;
    LO_Element* end_element = NULL;
    LO_Element* lo_element;
    int32 rect_x;
    int32 rect_y;
    int32 rect_width;
    int32 rect_height;
    CL_Layer *layer;

    if (!EDT_IsSelected(context))
		return FALSE;

    LO_GetSelectionEndpoints(context,
							 &start_element,
							 &end_element,
							 &start_selection,
							 &end_selection,
                             &layer);

    if (start_element == NULL)
		return FALSE;

    for (lo_element = start_element;
		 lo_element != NULL;
		 lo_element = ((LO_Any *)lo_element)->next) {

		if (lo_element->type == LO_TEXT &&
			(lo_element == start_element || lo_element == end_element)) {
			LO_TextStruct* text = (LO_TextStruct*)lo_element;

			if (text->text == NULL) {
				if (text->prev != NULL && text->prev->type == LO_TEXT) {
					text = (LO_TextStruct*)text->prev;
				} else {
					text = (LO_TextStruct*)text->next;
				}
			}
	    
			if (text->text == NULL)
				continue;
	    
			fe_get_selected_text_rect(context, text,
									  &rect_x,
									  &rect_y,
									  &rect_width,
									  &rect_height);
	    
		} else if (lo_element->type == LO_LINEFEED) {
			continue;
	    
		} else {
			LO_Any* lo_any = (LO_Any*)lo_element;
			rect_x = lo_any->x + lo_any->x_offset;
			rect_y = lo_any->y + lo_any->y_offset;
			rect_width = lo_any->width;
			rect_height = lo_any->height;
		}
	
		if (x > rect_x && y > rect_y &&
			x < (rect_x + rect_width) && y < (rect_y + rect_height))
			return TRUE;

		if (lo_element == end_element)
			break;
    }

    return FALSE;
}

class PopupCommand : public XFE_EditorViewCommand
{
public:
	PopupCommand() : XFE_EditorViewCommand(xfeCmdShowPopup) {};
	
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info);
};

void
PopupCommand::reallyDoCommand(XFE_View* v_view, XFE_CommandInfo* info)
{	
	XFE_EditorView* view = (XFE_EditorView*)v_view;

	if (info != NULL) {
		MWContext* context = view->getContext();
		unsigned long x;
		unsigned long y;
		XFE_PopupMenu* popup;
		
		fe_EventLOCoords(context, info->event, &x, &y);
		
		if (!fe_editor_selection_contains_point(context, x, y))
			EDT_SelectObject(context, x, y);
		
		popup = fe_EditorNewPopupMenu((XFE_Frame*)view->getToplevel(),
									  info->widget);

		view->setPopupMenu(popup);
		popup = view->getPopupMenu();

		if (popup != NULL) {
			popup->position(info->event);
			popup->show();
		}
	}
}

static char*
get_base_url(MWContext* context, char* rel_url)
{
	History_entry* hist_ent;

	if (context != NULL && rel_url != NULL) {
		hist_ent = SHIST_GetCurrent(&context->hist);
		if (hist_ent != NULL) {
			return NET_MakeAbsoluteURL(hist_ent->address, rel_url);
			/*caller must XP_FREE()*/
		}
	}
	return NULL;
}

static char label_buf[256];

class BrowseLinkCommand : public XFE_EditorViewCommand
{
public:
	BrowseLinkCommand() : XFE_EditorViewCommand(xfeCmdOpenLinkNew) {};
	
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		EDT_HREFData* href_data = EDT_GetHREFData(context);
		
		if (href_data != NULL) {
			if (href_data->pURL != NULL) {
				char* abs = get_base_url(context, href_data->pURL);
				if (abs != NULL) {
					URL_Struct* url = NET_CreateURLStruct(abs,NET_DONT_RELOAD);
					FE_GetURL(context, url);
					XP_FREE(abs);
				}
			}
			EDT_FreeHREFData(href_data);
		}
	}
	char*   getLabel(XFE_View* view, XFE_CommandInfo* info) {
		char* s = NULL;
		char* link = NULL;
		char* res_name = NULL;

		MWContext* context = view->getContext();
		EDT_HREFData* href_data = EDT_GetHREFData(context);
		
		if (href_data != NULL) {
			if (href_data->pURL != NULL) {
				link = XP_STRRCHR(href_data->pURL, '/');
				if (link != NULL)
					link++;
				else
					link = href_data->pURL;
				res_name = "browseToLink";
			}
		}

		if (info != NULL && res_name != NULL && link != NULL) {
			s = XfeSubResourceGetStringValue(info->widget,
											 res_name, 
											 XfeClassNameForWidget(info->widget),
											 XmNlabelString, 
											 XmCLabelString,
											 NULL);
			if (s != NULL)
				XP_SPRINTF(label_buf, s, link);

			s = label_buf;
		}

		if (href_data != NULL)
			EDT_FreeHREFData(href_data);

		return s;
	};
};

class EditLinkCommand : public XFE_EditorViewCommand
{
public:
	EditLinkCommand() : XFE_EditorViewCommand(xfeCmdOpenLinkEdit) {};
	
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		EDT_HREFData* href_data = EDT_GetHREFData(context);
    
		if (href_data != NULL) {
			if (href_data->pURL != NULL) {
				char* abs = get_base_url(context, href_data->pURL);
				if (abs != NULL) {
					fe_EditorNew(context, 0, 0, abs);
					XP_FREE(abs);
				}
			}
			EDT_FreeHREFData(href_data);
		}
	}
};

class BookmarkLinkCommand : public XFE_EditorViewCommand
{
public:
	BookmarkLinkCommand() : XFE_EditorViewCommand(xfeCmdAddLinkBookmark) {};
	
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		EDT_HREFData* href_data = EDT_GetHREFData(context);
    
		if (href_data != NULL) {
			if (href_data->pURL != NULL) {
				fe_AddToBookmark(context, 0,
					 NET_CreateURLStruct(href_data->pURL, NET_DONT_RELOAD), 0);
			}
			EDT_FreeHREFData(href_data);
		}
    }
};

class CopyLinkCommand : public XFE_EditorViewCommand
{
public:
	CopyLinkCommand() : XFE_EditorViewCommand(xfeCmdCopyLink) {};
	
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info) {
		if (info != NULL) {
			MWContext* context = view->getContext();
			EDT_HREFData* href_data = EDT_GetHREFData(context);
			
			Time now = fe_GetTimeFromEvent(info->event);
			
			if (href_data != NULL) {
				if (href_data->pURL != NULL) {
					fe_EditorCopyToClipboard(context, href_data->pURL, now);
				}
				EDT_FreeHREFData(href_data);
			}
		}
	}
};

//    END OF COMMAND DEFINES

static XFE_CommandList* my_commands = 0;

XFE_EditorView::XFE_EditorView(XFE_Component *toplevel_component,
			       Widget parent,
			       XFE_View *parent_view,
			       MWContext *context) 
  : XFE_HTMLView(toplevel_component, parent, parent_view, context)
{
	m_update_timer = 0;

	//
	// Register editor drop site
	//
	_dropSite=new XFE_EditorDrop(getBaseWidget(),this);
	_dropSite->enable();

	if (my_commands != 0)
		return;

	registerCommand(my_commands, new BrowseLinkCommand);
	registerCommand(my_commands, new EditLinkCommand);
	registerCommand(my_commands, new BookmarkLinkCommand);
	registerCommand(my_commands, new CopyLinkCommand);
	registerCommand(my_commands, new PopupCommand);
	registerCommand(my_commands, new UndoCommand);
	registerCommand(my_commands, new RedoCommand);
	registerCommand(my_commands, new CutCommand);
	registerCommand(my_commands, new CopyCommand);
	registerCommand(my_commands, new PasteCommand);
	registerCommand(my_commands, new DeleteCommand);
	registerCommand(my_commands, new SpellCommand);
	registerCommand(my_commands, new SaveCommand);
	registerCommand(my_commands, new SaveAsCommand);
	registerCommand(my_commands, new PublishCommand);
	registerCommand(my_commands, new DeleteTableCommand);
	registerCommand(my_commands, new DeleteTableCellCommand);
	registerCommand(my_commands, new DeleteTableRowCommand);
	registerCommand(my_commands, new DeleteTableColumnCommand);
	registerCommand(my_commands, new RemoveLinkCommand);
	registerCommand(my_commands, new SelectAllCommand);
	registerCommand(my_commands, new FindCommand);
	registerCommand(my_commands, new FindAgainCommand);
	registerCommand(my_commands, new ToggleTableBordersCommand);
	registerCommand(my_commands, new ToggleParagraphMarksCommand);
	registerCommand(my_commands, new ReloadCommand);
	registerCommand(my_commands, new RefreshCommand);
	registerCommand(my_commands, new InsertLinkCommand);
	registerCommand(my_commands, new InsertTargetCommand);
	registerCommand(my_commands, new InsertImageCommand);
	registerCommand(my_commands, new InsertHorizontalLineCommand);
	registerCommand(my_commands, new InsertBulletedListCommand);
	registerCommand(my_commands, new InsertNumberedListCommand);
	registerCommand(my_commands, new InsertTableCommand);
	registerCommand(my_commands, new InsertTableRowCommand);
	registerCommand(my_commands, new InsertTableColumnCommand);
	registerCommand(my_commands, new InsertTableCellCommand);
	registerCommand(my_commands, new InsertHtmlCommand);
	registerCommand(my_commands, new InsertLineBreakCommand);
	registerCommand(my_commands, new InsertBreakBelowImageCommand);
	registerCommand(my_commands, new InsertNonBreakingSpaceCommand);
	registerCommand(my_commands, new SetFontSizeCommand);
	registerCommand(my_commands, new SetFontFaceCommand);
	registerCommand(my_commands, new SetFontColorCommand);
	registerCommand(my_commands, new SetParagraphStyleCommand);
	registerCommand(my_commands, new SetListStyleCommand);
	registerCommand(my_commands, new SetAlignStyleCommand);
	registerCommand(my_commands, new SetCharacterStyleCommand);
	registerCommand(my_commands, new ToggleCharacterStyleCommand);
	registerCommand(my_commands, new ClearAllStylesCommand);
	registerCommand(my_commands, new IndentCommand);
	registerCommand(my_commands, new OutdentCommand);
	registerCommand(my_commands, new SetObjectPropertiesCommand);
	registerCommand(my_commands, new SetPagePropertiesCommand);
	registerCommand(my_commands, new SetTablePropertiesCommand);
#ifndef NO_SECURITY
	registerCommand(my_commands, new SetEncryptedCommand);
#endif
	registerCommand(my_commands, new SetCharacterColorCommand);
	registerCommand(my_commands, new BrowsePageCommand);
	registerCommand(my_commands, new EditSourceCommand);
	registerCommand(my_commands, new ViewSourceCommand);
	registerCommand(my_commands, new EditorObjectIsCommand);
	registerCommand(my_commands, new DialogCommand);
	registerCommand(my_commands, new TabCommand);

	//    non-paramaterized font size commands.
	registerCommand(my_commands, new SetFontSizeMinusTwoCommand);
	registerCommand(my_commands, new SetFontSizeMinusOneCommand);
	registerCommand(my_commands, new SetFontSizeZeroCommand);
	registerCommand(my_commands, new SetFontSizePlusOneCommand);
	registerCommand(my_commands, new SetFontSizePlusTwoCommand);
	registerCommand(my_commands, new SetFontSizePlusThreeCommand);
	registerCommand(my_commands, new SetFontSizePlusFourCommand);
	registerCommand(my_commands, new SetParagraphStyleNormalCommand);
	registerCommand(my_commands, new SetParagraphStyleHeadingOneCommand);
	registerCommand(my_commands, new SetParagraphStyleHeadingTwoCommand);
	registerCommand(my_commands, new SetParagraphStyleHeadingThreeCommand);
	registerCommand(my_commands, new SetParagraphStyleHeadingFourCommand);
	registerCommand(my_commands, new SetParagraphStyleHeadingFiveCommand);
	registerCommand(my_commands, new SetParagraphStyleHeadingSixCommand);

	registerCommand(my_commands, new SetParagraphStyleAddressCommand);
	registerCommand(my_commands, new SetParagraphStyleFormattedCommand);
	registerCommand(my_commands, new SetParagraphStyleTitleCommand);
	registerCommand(my_commands, new SetParagraphStyleTextCommand);
	registerCommand(my_commands, new SetParagraphStyleBlockQuoteCommand);

	registerCommand(my_commands, new SetListStyleNoneCommand);
	registerCommand(my_commands, new SetListStyleBulletedCommand);
	registerCommand(my_commands, new SetListStyleNumberedCommand);

	registerCommand(my_commands, new SetAlignStyleLeftCommand);
	registerCommand(my_commands, new SetAlignStyleCenterCommand);
	registerCommand(my_commands, new SetAlignStyleRightCommand);

	registerCommand(my_commands, new ToggleCharacterStyleBoldCommand);
	registerCommand(my_commands, new ToggleCharacterStyleItalicCommand);
	registerCommand(my_commands, new ToggleCharacterStyleUnderlineCommand);
	registerCommand(my_commands, new ToggleCharacterStyleFixedCommand);
	registerCommand(my_commands, new ToggleCharacterStyleSuperscriptCommand);
	registerCommand(my_commands, new ToggleCharacterStyleSubscriptCommand);
	registerCommand(my_commands, new ToggleCharacterStyleStrikethroughCommand);
	registerCommand(my_commands, new ToggleCharacterStyleBlinkCommand);

	//
	//    Register Java Plugin Commands.
	//
    int32 ncategories = EDT_NumberOfPluginCategories();

	for (int32 nc = 0; nc < ncategories; nc++) {

		int32 nplugins = EDT_NumberOfPlugins(nc);

		for (int32 np = 0; np < nplugins; np++) {
			char buf[64];
			char* name;

			sprintf(buf, "javaPlugin_%d_%d", nc, np);
			name = Command::intern(buf);

			registerCommand(my_commands, new JavaPluginCommand(name, nc, np));
		}
	}

	//
	//    Insert these last, so they are at the head of the command
	//    list, and will get found quickly.
	//
	registerCommand(my_commands, new InsertReturnCommand);
	registerCommand(my_commands, new SelectCommand);
	registerCommand(my_commands, new MoveCommand);
	registerCommand(my_commands, new KeyInsertCommand);
}

XFE_EditorView::~XFE_EditorView()
{
    if (_dropSite) {
        delete _dropSite;
        _dropSite=NULL;
    }
	if (m_update_timer != 0) {
		XtRemoveTimeOut(m_update_timer);
		m_update_timer = 0;
	}
}

char*
XFE_EditorView::getPlainText()
{
  // This method suppose to get the text message in the widget
  // and return in a char string block
  return NULL;
}

#ifdef DEBUG_editor
#define TRACE(x) printf x
#else
#define TRACE(x)
#endif

XFE_Command*
XFE_EditorView::getCommand(CommandType cmd)
{
	XFE_Command* command = findCommand(my_commands, cmd);

	if (command != NULL)
		return command;
	else
		return XFE_HTMLView::getCommand(cmd);
}

void 
XFE_EditorView::insertMessageCompositionText(const char* text, 
					     XP_Bool leaveCursorBeginning, XP_Bool isHTML)
{
#ifdef DEBUG_editor
	fprintf(stderr, "ping::[ insertMessageCompositionText ]\n");
#endif

	if (m_contextData) {
		Boolean  noTagsP = False;
		Boolean  inDraft = False;

		ED_BufferOffset ins = -1;

		if ( leaveCursorBeginning ){
			ins = EDT_GetInsertPointOffset( m_contextData );
		}

		if (isHTML) {
			//
			// NOTE:  let's try to see if they really have any HTML tags 
			//        in the text they've given us... [ poor man's parser ]
			//
			char *xtop = XP_STRSTR( text, "<HTML>" );
			
			int   s1 = XP_STRLEN( text );
			int   s2 = 0;
			int   n  = 0;

#ifdef DEBUG_editor
			fprintf(stderr, "  paste::[ %d ]\n", s1);
#endif

			if ( xtop ) {
				s2 = XP_STRLEN( xtop );
				n  = s1 - s2;
			}

			if ( xtop && ( n < 2 )) {
				inDraft = True;
			}
			else {
				s2 = 0;
				n  = 0;

				if (XP_STRCASESTR(text, "</A>")   ||
					XP_STRCASESTR(text, "<PRE>")  ||
					XP_STRCASESTR(text, "<HR")    ||
					XP_STRCASESTR(text, "<IMG")   ||
					XP_STRCASESTR(text, "<TABLE")
					) {
					noTagsP = False;
				}
				else {
#ifdef DEBUG_editor
					fprintf(stderr, 
							"WARNING... [ looks like plaintext to me ]\n");
#endif			
					noTagsP = True;
				}
			}

			EDT_PasteQuoteBegin( m_contextData, isHTML );

			char *xsig = 0;
			char *xtra = 0;

			if (inDraft) {
				// NOTE:  we're loading a draft message, don't mess with it!!!
				//
				xtra = 0;
				xsig = 0;

				EDT_PasteQuote( m_contextData, (char *) text );
			}
			else {
				// NOTE:  try to figure out if we've got a signature...
				//
				if (s1 > 2) {
					xsig = XP_STRSTR( text, "-- " );
					
					if (xsig) {
						s2 = XP_STRLEN( xsig );
						n  = s1 - s2;
					}
				}

				if ( n > 1 ) {
					xtra = (char *) XP_ALLOC( n + 1 );

					XP_STRNCPY_SAFE(xtra, text, n );
#ifdef DEBUG_editor
					fprintf(stderr, 
							"WARNING... [ hacking pre-signature string ]\n");
#endif
				}
				else {
					xtra = 0;

					if ( n == 0 ) {
						xsig = (char *) text;
					}
				}

				if ( xtra ) {
					EDT_PasteQuote( m_contextData, (char *) xtra );
				}

				EDT_PasteQuote( m_contextData, "<BR>");
#ifdef DEBUG_editor
				fprintf(stderr, 
						"WARNING... [ insert pre-sig break ]\n");
#endif
				if (noTagsP)  
					EDT_PasteQuote( m_contextData, "<PRE>");

				EDT_PasteQuote( m_contextData, (char *) xsig );

				if (noTagsP)  
					EDT_PasteQuote( m_contextData, "</PRE>" );

				EDT_PasteQuote( m_contextData, "<BR>" );
#ifdef DEBUG_editor
				fprintf(stderr, 
						"WARNING... [ insert post-sig break ]\n");
#endif
			}

			EDT_PasteQuoteEnd( m_contextData );

			if ( xtra ) {
				XP_FREE( xtra );
			}
		}
		else {
			EDT_PasteText( m_contextData, (char *) text );
		}

		if ( leaveCursorBeginning && ins != -1 ) {
			EDT_SetInsertPointToOffset( m_contextData, ins, 0 );
		}

	}
}

void 
XFE_EditorView::getMessageBody(char **pBody, uint32 *body_size, 
			       MSG_FontCode** /*font_changes*/)
{
#ifdef DEBUG_editor
   fprintf(stderr, "ping::[ getMessageBody ]\n");
#endif

   *pBody = NULL;
   *body_size = 0;

   EDT_SaveToBuffer(m_contextData, pBody);

   if ( !*pBody){
     // oops...
   }
   else {
     size_t lsize;
     lsize = strlen(*pBody);
     *body_size = lsize;
   }
}

void 
XFE_EditorView::doneWithMessageBody(char* /*pBody*/)
{
#ifdef DEBUG_editor
   fprintf(stderr, "ping::[ doneWithMessageBody ]\n");
#endif
}

void
XFE_EditorView::DocEncoding (XFE_NotificationCenter *, void *, void *callData)
{
	int new_doc_csid = (int)callData;

#ifdef DEBUG_editor
	fprintf(stderr, "DocEncoding::[ %d ]\n", new_doc_csid);
#endif
	EDT_SetEncoding(m_contextData, new_doc_csid);
}

Boolean
XFE_EditorView::isModified()
{
	return EDT_DirtyFlag(m_contextData);
}

void
XFE_EditorView::updateChromeTimeout(XtPointer closure, XtIntervalId*)
{
	XFE_EditorView* view = (XFE_EditorView*)closure;

	//    do this before you do anything else
	view->m_update_timer = 0;

	//    tell the frame to update.
	XFE_Component* top = view->getToplevel();
	top->notifyInterested(XFE_View::chromeNeedsUpdating);
}

void
XFE_EditorView::updateChrome()
{
	Cardinal delay = fe_globalData.editor_update_delay;
	
	//
	//    If there is an outstanding timer, reset, and start again.
	//    This way we always wait until we just not doing anything
	//    to redraw the toolbars. This should probably be a workproc
	//    thing, we don't have Xt WorkProcs and right now, I don't
	//    want to mess with fe_EventLoop() to make them work.
	//
	if (m_update_timer != 0) {
		XtRemoveTimeOut(m_update_timer);
		m_update_timer = 0;
	}

	if (delay != 0) {
		m_update_timer = XtAppAddTimeOut(fe_XtAppContext,
										 delay,
										 updateChromeTimeout,
										 this);
	} else {
		updateChromeTimeout(this, 0);
	}
}

XFE_View*
XFE_EditorView::getCommandView(XFE_Command*)
{
	return this;
}
