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

#include "rosetta.h"
#include "EditorFrame.h"
#include "EditorView.h"
#include "SpellHandler.h"
#include "htrdf.h"       // for HT_AddBookmark()
#include <Xm/Label.h>
#include <Xfe/Xfe.h>

#include "xpgetstr.h"
#include "fe_proto.h"
#include "edt.h"
#include "xeditor.h"
#ifndef NO_SECURITY
#include "pk11func.h"
#endif

extern "C" {
int XFE_EDITOR_NEWTABLE_COLS;
#if defined(MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
void fe_mailto_cb(Widget , XtPointer, XtPointer);
#endif
}

#define FE_SYNTAX_ERROR() doSyntaxErrorAlert(view, info)

class UndoCommand : public XFE_EditorViewCommand
{
public:
	UndoCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdUndo, v) {};

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
	RedoCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdRedo, v) {};
	
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
	CutCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdCut, v) {};

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
	CopyCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdCopy, v) {};

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
	PasteCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdPaste, v) {};

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

class DeleteCommand : public AlwaysEnabledCommand
{
public:
	DeleteCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdDeleteItem, v) {};

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
	SpellCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdSpellCheck, v) {};

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
	SaveCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdSave, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorSave(view->getContext());
	}; 
};

class SaveAsCommand : public AlwaysEnabledCommand
{
public:
	SaveAsCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdSaveAs, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorSaveAs(view->getContext());
	}; 
};

class PublishCommand : public AlwaysEnabledCommand
{
public:
	PublishCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdPublish, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorPublish(view->getContext());
	}; 
};

#if defined(MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
class SendPageCommand : public AlwaysEnabledCommand
{
public:
	SendPageCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdSendPage, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
      fe_mailto_cb(CONTEXT_WIDGET (view->getContext()), 
                   (XtPointer) view->getContext(), NULL);
	}; 
};
#endif /* MOZ_MAIL_NEWS || MOZ_MAIL_COMPOSE */

class DeleteTableCommand : public XFE_EditorViewCommand
{
public:
	DeleteTableCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdDeleteTable, v) {};

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
	DeleteTableCellCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdDeleteTableCell, v) {};

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
	DeleteTableRowCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdDeleteTableRow, v) {};

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
	DeleteTableColumnCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdDeleteTableColumn, v) {};

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
	RemoveLinkCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdRemoveLink, v) {};

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
	SelectAllCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdSelectAll, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorSelectAll(view->getContext());
	}; 
};

class FindCommand : public AlwaysEnabledCommand
{
public:
	FindCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdFindInObject, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorFind(view->getContext());
	}; 
};

class FindAgainCommand : public XFE_EditorViewCommand
{
public:
	FindAgainCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdFindAgain, v) {};

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
	ToggleParagraphMarksCommand(XFE_EditorView *v) :
		AlwaysEnabledCommand(xfeCmdToggleParagraphMarks, v) {};

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
	ToggleTableBordersCommand(XFE_EditorView *v) :
		AlwaysEnabledCommand(xfeCmdToggleTableBorders, v) {};

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
	ReloadCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdReload, v) {};

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
	RefreshCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdRefresh, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorRefresh(view->getContext());
	}; 
};

class InsertLinkCommand : public AlwaysEnabledCommand
{
public:
	InsertLinkCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdInsertLink, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorInsertLinkDialogDo(view->getContext());
	}; 
};

class InsertTargetCommand : public AlwaysEnabledCommand
{
public:
	InsertTargetCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdInsertTarget, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTargetPropertiesDialogDo(view->getContext());
	}; 
};

class InsertImageCommand : public AlwaysEnabledCommand
{
public:
	InsertImageCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdInsertImage, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorPropertiesDialogDo(view->getContext(),
									XFE_EDITOR_PROPERTIES_IMAGE_INSERT);
	}; 
};

class InsertBulletedListCommand : public AlwaysEnabledCommand
{
public:
	InsertBulletedListCommand(XFE_EditorView *v) : 
		AlwaysEnabledCommand(xfeCmdInsertBulletedList, v) {};

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
	InsertNumberedListCommand(XFE_EditorView *v) : 
		AlwaysEnabledCommand(xfeCmdInsertNumberedList, v) {};

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
	InsertHorizontalLineCommand(XFE_EditorView *v) : 
		AlwaysEnabledCommand(xfeCmdInsertHorizontalLine, v) {};

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
	InsertTableCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdInsertTable, v) {};

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
	InsertTableRowCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdInsertTableRow, v) {};

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
	InsertTableColumnCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdInsertTableColumn, v) {};

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
	InsertTableCellCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdInsertTableCell, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableCellCanInsert(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTableCellInsert(view->getContext(), NULL);
	}; 
};

class SelectTableCommand : public XFE_EditorViewCommand
{
public:
	SelectTableCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdSelectTable, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return (EDT_IsInsertPointInTable(view->getContext()) != 0);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
        EDT_ChangeTableSelection(view->getContext(), ED_HIT_SEL_TABLE,
                                 ED_MOVE_NONE,
                                 EDT_GetTableCellData(view->getContext()));
	}; 
};

class SelectTableCellCommand : public XFE_EditorViewCommand
{
public:
	SelectTableCellCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdSelectTableCell, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return (EDT_IsInsertPointInTable(view->getContext()) != 0);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
        EDT_ChangeTableSelection(view->getContext(), ED_HIT_SEL_CELL,
                                 ED_MOVE_NONE,
                                 EDT_GetTableCellData(view->getContext()));
	}; 
};

class SelectTableRowCommand : public XFE_EditorViewCommand
{
public:
	SelectTableRowCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdSelectTableRow, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return (EDT_IsInsertPointInTable(view->getContext()) != 0);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
        EDT_ChangeTableSelection(view->getContext(), ED_HIT_SEL_ROW,
                                 ED_MOVE_NONE,
                                 EDT_GetTableCellData(view->getContext()));
	}; 
};

class SelectTableColumnCommand : public XFE_EditorViewCommand
{
public:
	SelectTableColumnCommand(XFE_EditorView *v)
      : XFE_EditorViewCommand(xfeCmdSelectTableColumn, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return (EDT_IsInsertPointInTable(view->getContext()) != 0);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
        EDT_ChangeTableSelection(view->getContext(), ED_HIT_SEL_COL,
                                 ED_MOVE_NONE,
                                 EDT_GetTableCellData(view->getContext()));
	}; 
};

class SelectTableAllCellsCommand : public XFE_EditorViewCommand
{
public:
	SelectTableAllCellsCommand(XFE_EditorView *v)
      : XFE_EditorViewCommand(xfeCmdSelectTableAllCells, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return (EDT_IsInsertPointInTable(view->getContext()) != 0);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
        EDT_ChangeTableSelection(view->getContext(), ED_HIT_SEL_ALL_CELLS,
                                 ED_MOVE_NONE,
                                 EDT_GetTableCellData(view->getContext()));
	}; 
};

class TableJoinCommand : public XFE_EditorViewCommand
{
public:
	TableJoinCommand(XFE_EditorView *v)
      : XFE_EditorViewCommand(xfeCmdTableJoin, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
        return (EDT_GetMergeTableCellsType(view->getContext()) != 0);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
        EDT_MergeTableCells(view->getContext());
	}; 
};

class ConvertTextToTableCommand : public XFE_EditorViewCommand
{
public:
	ConvertTextToTableCommand(XFE_EditorView *v)
      : XFE_EditorViewCommand(xfeCmdConvertTextToTable, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return EDT_CanConvertTextToTable(view->getContext());
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
        char* cols_str = XFE_Prompt(view->getContext(),
                                    XP_GetString(XFE_EDITOR_NEWTABLE_COLS),
                                    "1");
        int cols = atoi(cols_str);
        if (cols <= 0)
          return;
        EDT_ConvertTextToTable(view->getContext(), cols);
	}; 
};

class ConvertTableToTextCommand : public XFE_EditorViewCommand
{
public:
	ConvertTableToTextCommand(XFE_EditorView *v)
      : XFE_EditorViewCommand(xfeCmdConvertTableToText, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return (EDT_IsInsertPointInTable(view->getContext()) != 0);
	};
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
        EDT_ConvertTableToText(view->getContext());
	}; 
};

class InsertHtmlCommand : public AlwaysEnabledCommand
{
public:
	InsertHtmlCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdInsertHtml, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorHtmlPropertiesDialogDo(view->getContext());
	}; 
};

class InsertBreakBelowImageCommand : public AlwaysEnabledCommand
{
public:
	InsertBreakBelowImageCommand(XFE_EditorView *v) :
		AlwaysEnabledCommand(xfeCmdInsertBreakBelowImage, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {

		fe_EditorLineBreak(view->getContext(), ED_BREAK_BOTH);

	}; 
};

class InsertNonBreakingSpaceCommand : public AlwaysEnabledCommand
{
public:
	InsertNonBreakingSpaceCommand(XFE_EditorView *v) :
		AlwaysEnabledCommand(xfeCmdInsertNonBreakingSpace, v) {};

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
	PR_ASSERT(name != 0);   // we really do want a crash here if we see this
    if (name == 0)
    	return -1;
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
	SetFontSizeCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdSetFontSize, v) {};
	SetFontSizeCommand(char* name, XFE_EditorView *v) : AlwaysEnabledCommand(name, v) {};

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
			ED_FontSize size = (ED_FontSize)(int)set_font_size_params[i].data;
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
			ED_FontSize match = (ED_FontSize)(int)set_font_size_params[i].data;
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
	SetFontFaceCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdSetFontFace, v) {
		m_params = NULL;
	}
	SetFontFaceCommand(char* name, XFE_EditorView *v) : AlwaysEnabledCommand(name, v) {
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

XP_Bool SetFontColorCommand::isDeterminate(XFE_View* view, XFE_CommandInfo*)
{
  return style_is_determinate(view->getContext(), TF_FONT_COLOR);
}

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
	SetParagraphStyleCommand(XFE_EditorView *v) :
		AlwaysEnabledCommand(xfeCmdSetParagraphStyle, v) {};
	SetParagraphStyleCommand(char* name, XFE_EditorView *v) : AlwaysEnabledCommand(name, v) {};

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
	SetListStyleCommand(XFE_EditorView *v) :	AlwaysEnabledCommand(xfeCmdSetListStyle, v) {};
	SetListStyleCommand(char* name, XFE_EditorView *v) : AlwaysEnabledCommand(name, v) {};

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
	SetAlignStyleCommand(XFE_EditorView *v) :	
		AlwaysEnabledCommand(xfeCmdSetAlignmentStyle, v) {};
	SetAlignStyleCommand(char* name, XFE_EditorView *v) : AlwaysEnabledCommand(name, v) {};

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
	SetCharacterStyleCommand(XFE_EditorView *v) :	
		AlwaysEnabledCommand(xfeCmdSetCharacterStyle, v) {};
	SetCharacterStyleCommand(char* name, XFE_EditorView *v) : AlwaysEnabledCommand(name, v) {};

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
	ToggleCharacterStyleCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyle, v) {};

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
	IndentCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdIndent, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorIndent(view->getContext(), False);
	}
};

class OutdentCommand : public AlwaysEnabledCommand
{
public:
	OutdentCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdOutdent, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorIndent(view->getContext(), True);
	}
};

class SetObjectPropertiesCommand : public AlwaysEnabledCommand
{
public:
	SetObjectPropertiesCommand(XFE_EditorView *v) :
		AlwaysEnabledCommand(xfeCmdSetObjectProperties, v) {};

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
	SetPagePropertiesCommand(XFE_EditorView *v) : 
		AlwaysEnabledCommand(xfeCmdSetPageProperties, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorDocumentPropertiesDialogDo(view->getContext(),
								XFE_EDITOR_DOCUMENT_PROPERTIES_APPEARANCE);
	}
};

class SetTablePropertiesCommand : public XFE_EditorViewCommand
{
public:
	SetTablePropertiesCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdSetTableProperties, v) {};

	XP_Bool isEnabled(XFE_View* view, XFE_CommandInfo*) {
		return fe_EditorTableCanDelete(view->getContext());
	}
	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		fe_EditorTablePropertiesDialogDo(view->getContext(),
										 XFE_EDITOR_PROPERTIES_TABLE);
	}
};

HG10297

class SetCharacterColorCommand : public AlwaysEnabledCommand
{
public:
	SetCharacterColorCommand(XFE_EditorView *v) :
		AlwaysEnabledCommand(xfeCmdSetCharacterColor, v) {};

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
	JavaPluginCommand(char* name, int32 cid, int32 pid, XFE_EditorView *v) :
		AlwaysEnabledCommand(name, v) {
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
	BrowsePageCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdBrowsePage, v) {};

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
	EditSourceCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdEditPageSource, v) {};

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
	ViewSourceCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdViewPageSource, v) {};

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
	SetFontSizeMinusTwoCommand(XFE_EditorView *v) :
	   		SetFontSizeCommand(xfeCmdSetFontSizeMinusTwo, v) {};

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
	SetFontSizeMinusOneCommand(XFE_EditorView *v) :
		SetFontSizeCommand(xfeCmdSetFontSizeMinusOne, v) {};

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
	SetFontSizeZeroCommand(XFE_EditorView *v) :
		SetFontSizeCommand(xfeCmdSetFontSizeZero, v) {};

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
	SetFontSizePlusOneCommand(XFE_EditorView *v) :
		SetFontSizeCommand(xfeCmdSetFontSizePlusOne, v) {};

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
	SetFontSizePlusTwoCommand(XFE_EditorView *v) :
		SetFontSizeCommand(xfeCmdSetFontSizePlusTwo, v) {};

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
	SetFontSizePlusThreeCommand(XFE_EditorView *v) :
		SetFontSizeCommand(xfeCmdSetFontSizePlusThree, v) {};

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
	SetFontSizePlusFourCommand(XFE_EditorView *v) :
		SetFontSizeCommand(xfeCmdSetFontSizePlusFour, v) {};

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
	SetParagraphStyleNormalCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleNormal, v) {};

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
	SetParagraphStyleHeadingOneCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingOne, v) {};

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
	SetParagraphStyleHeadingTwoCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingTwo, v) {};

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
	SetParagraphStyleHeadingThreeCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingThree, v) {};

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
	SetParagraphStyleHeadingFourCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingFour, v) {};

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
	SetParagraphStyleHeadingFiveCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingFive, v) {};

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
	SetParagraphStyleHeadingSixCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleHeadingSix, v) {};

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
	SetParagraphStyleAddressCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleAddress, v) {};

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
	SetParagraphStyleFormattedCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleFormatted, v) {};

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
	SetParagraphStyleTitleCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleDescriptionTitle, v) {};

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
	SetParagraphStyleTextCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleDescriptionText, v) {};

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
	SetParagraphStyleBlockQuoteCommand(XFE_EditorView *v) :
		SetParagraphStyleCommand(xfeCmdSetParagraphStyleBlockQuote, v) {};
	
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
	SetListStyleNoneCommand(XFE_EditorView *v) :
		SetListStyleCommand(xfeCmdSetListStyleNone, v) {};

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
	SetListStyleBulletedCommand(XFE_EditorView *v) :
		SetListStyleCommand(xfeCmdSetListStyleBulleted, v) {};

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
	SetListStyleNumberedCommand(XFE_EditorView *v) :
		SetListStyleCommand(xfeCmdSetListStyleNumbered, v) {};

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
	SetAlignStyleLeftCommand(XFE_EditorView *v) :
		SetAlignStyleCommand(xfeCmdSetAlignmentStyleLeft, v) {};

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
	SetAlignStyleCenterCommand(XFE_EditorView *v) :
		SetAlignStyleCommand(xfeCmdSetAlignmentStyleCenter, v) {};

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
	SetAlignStyleRightCommand(XFE_EditorView *v) :
		SetAlignStyleCommand(xfeCmdSetAlignmentStyleRight, v) {};

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
	ClearAllStylesCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdClearAllStyles, v) {};

	void    reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		set(view, TF_NONE);
	};
};

class ToggleCharacterStyleBoldCommand : public SetCharacterStyleCommand
{
public:
	ToggleCharacterStyleBoldCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleBold, v) {};

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
	ToggleCharacterStyleUnderlineCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleUnderline, v) {};

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
	ToggleCharacterStyleItalicCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleItalic, v) {};

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
	ToggleCharacterStyleFixedCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleFixed, v) {};

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
	ToggleCharacterStyleSuperscriptCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleSuperscript, v)
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
	ToggleCharacterStyleSubscriptCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleSubscript, v)
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
	ToggleCharacterStyleStrikethroughCommand(XFE_EditorView *v) :
	SetCharacterStyleCommand(xfeCmdToggleCharacterStyleStrikethrough, v) {
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
	ToggleCharacterStyleBlinkCommand(XFE_EditorView *v) :
		SetCharacterStyleCommand(xfeCmdToggleCharacterStyleBlink, v) {};

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
	MoveCommand(XFE_EditorView *v) :	AlwaysEnabledCommand(xfeCmdMoveCursor, v) {};

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
	KeyInsertCommand(XFE_EditorView *v) :	AlwaysEnabledCommand(xfeCmdKeyInsert, v) {};

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
	InsertReturnCommand(XFE_EditorView *v) :	AlwaysEnabledCommand(xfeCmdInsertReturn, v) {};

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
	InsertLineBreakCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdInsertLineBreak, v) {};
	
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
	TabCommand(XFE_EditorView *v) : AlwaysEnabledCommand(xfeCmdTab, v) {};

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
	SelectCommand(XFE_EditorView *v) : XFE_ViewCommand(xfeCmdSelect, v) {};
	
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
	if (m_view)
		view = m_view;

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
	EditorObjectIsCommand(XFE_EditorView *v) : XFE_ObjectIsCommand(v) {}
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
	DialogCommand(XFE_EditorView *v) : XFE_ViewCommand(xfeCmdDialog, v) {};
	
	void doCommand(XFE_View* view, XFE_CommandInfo* info) {

		if (m_view)
			view = m_view;

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

class EditorPopupCommand : public XFE_EditorViewCommand
{
public:
	EditorPopupCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdShowPopup, v) {};
	
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo* info);
};

void
EditorPopupCommand::reallyDoCommand(XFE_View* v_view, XFE_CommandInfo* info)
{	
	XFE_EditorView* view = (XFE_EditorView*)v_view;

	if (info != NULL) {
		MWContext* context = view->getContext();
		unsigned long x;
		unsigned long y;
		XFE_PopupMenu* popup;
		XFE_Frame *frame;
		
		fe_EventLOCoords(context, info->event, &x, &y);
		
		if (!fe_editor_selection_contains_point(context, x, y))
			EDT_SelectObject(context, x, y);
		
		frame = (XFE_Frame*)view->getToplevel();

#ifdef ENDER
		if (! EDITOR_CONTEXT_DATA(context)->embedded)
#endif /* ENDER */
		context = frame->getContext();

		popup = fe_EditorNewPopupMenu(frame, info->widget, context);

#ifdef ENDER
		if (EDITOR_CONTEXT_DATA(context)->embedded)
			popup->setCommandDispatcher(view);
#endif /* ENDER */

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
	BrowseLinkCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdOpenLinkNew, v) {};
	
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
	EditLinkCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdOpenLinkEdit, v) {};
	
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
	BookmarkLinkCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdAddLinkBookmark, v) {};
	
	void reallyDoCommand(XFE_View* view, XFE_CommandInfo*) {
		MWContext* context = view->getContext();
		EDT_HREFData* href_data = EDT_GetHREFData(context);
    
		if (href_data != NULL) {
			if (href_data->pURL != NULL) {
                HT_AddBookmark (href_data->pURL, NULL /*Title*/);
			}
			EDT_FreeHREFData(href_data);
		}
    }
};

class CopyLinkCommand : public XFE_EditorViewCommand
{
public:
	CopyLinkCommand(XFE_EditorView *v) : XFE_EditorViewCommand(xfeCmdCopyLink, v) {};
	
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

	XFE_EditorView *ev = 0;

#ifdef ENDER

	if (EDITOR_CONTEXT_DATA(context)->embedded)
		ev = this;

#endif /* ENDER */

	m_commands = 0;
	registerCommand(m_commands, new BrowseLinkCommand(ev));
	registerCommand(m_commands, new EditLinkCommand(ev));
	registerCommand(m_commands, new BookmarkLinkCommand(ev));
	registerCommand(m_commands, new CopyLinkCommand(ev));
	registerCommand(m_commands, new EditorPopupCommand(ev));
	registerCommand(m_commands, new UndoCommand(ev));
	registerCommand(m_commands, new RedoCommand(ev));
	registerCommand(m_commands, new CutCommand(ev));
	registerCommand(m_commands, new CopyCommand(ev));
	registerCommand(m_commands, new PasteCommand(ev));
	registerCommand(m_commands, new DeleteCommand(ev));
	registerCommand(m_commands, new SpellCommand(ev));
	registerCommand(m_commands, new SaveCommand(ev));
	registerCommand(m_commands, new SaveAsCommand(ev));
	registerCommand(m_commands, new PublishCommand(ev));
#if defined(MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
	registerCommand(m_commands, new SendPageCommand(ev));
#endif /* MOZ_MAIL_NEWS || MOZ_MAIL_COMPOSE */
	registerCommand(m_commands, new DeleteTableCommand(ev));
	registerCommand(m_commands, new DeleteTableCellCommand(ev));
	registerCommand(m_commands, new DeleteTableRowCommand(ev));
	registerCommand(m_commands, new DeleteTableColumnCommand(ev));
	registerCommand(m_commands, new SelectTableCommand(ev));
	registerCommand(m_commands, new SelectTableCellCommand(ev));
	registerCommand(m_commands, new SelectTableAllCellsCommand(ev));
	registerCommand(m_commands, new SelectTableRowCommand(ev));
	registerCommand(m_commands, new SelectTableColumnCommand(ev));
	registerCommand(m_commands, new TableJoinCommand(ev));
	registerCommand(m_commands, new ConvertTextToTableCommand(ev));
	registerCommand(m_commands, new ConvertTableToTextCommand(ev));
	registerCommand(m_commands, new RemoveLinkCommand(ev));
	registerCommand(m_commands, new SelectAllCommand(ev));
	registerCommand(m_commands, new FindCommand(ev));
	registerCommand(m_commands, new FindAgainCommand(ev));
	registerCommand(m_commands, new ToggleTableBordersCommand(ev));
	registerCommand(m_commands, new ToggleParagraphMarksCommand(ev));
	registerCommand(m_commands, new ReloadCommand(ev));
	registerCommand(m_commands, new RefreshCommand(ev));
	registerCommand(m_commands, new InsertLinkCommand(ev));
	registerCommand(m_commands, new InsertTargetCommand(ev));
	registerCommand(m_commands, new InsertImageCommand(ev));
	registerCommand(m_commands, new InsertHorizontalLineCommand(ev));
	registerCommand(m_commands, new InsertBulletedListCommand(ev));
	registerCommand(m_commands, new InsertNumberedListCommand(ev));
	registerCommand(m_commands, new InsertTableCommand(ev));
	registerCommand(m_commands, new InsertTableRowCommand(ev));
	registerCommand(m_commands, new InsertTableColumnCommand(ev));
	registerCommand(m_commands, new InsertTableCellCommand(ev));
	registerCommand(m_commands, new InsertHtmlCommand(ev));
	registerCommand(m_commands, new InsertLineBreakCommand(ev));
	registerCommand(m_commands, new InsertBreakBelowImageCommand(ev));
	registerCommand(m_commands, new InsertNonBreakingSpaceCommand(ev));
	registerCommand(m_commands, new SetFontSizeCommand(ev));
	registerCommand(m_commands, new SetFontFaceCommand(ev));
	registerCommand(m_commands, new SetFontColorCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleCommand(ev));
	registerCommand(m_commands, new SetListStyleCommand(ev));
	registerCommand(m_commands, new SetAlignStyleCommand(ev));
	registerCommand(m_commands, new SetCharacterStyleCommand(ev));
	registerCommand(m_commands, new ToggleCharacterStyleCommand(ev));
	registerCommand(m_commands, new ClearAllStylesCommand(ev));
	registerCommand(m_commands, new IndentCommand(ev));
	registerCommand(m_commands, new OutdentCommand(ev));
	registerCommand(m_commands, new SetObjectPropertiesCommand(ev));
	registerCommand(m_commands, new SetPagePropertiesCommand(ev));
	registerCommand(m_commands, new SetTablePropertiesCommand(ev));
	HG81272
	registerCommand(m_commands, new SetCharacterColorCommand(ev));
	registerCommand(m_commands, new BrowsePageCommand(ev));
	registerCommand(m_commands, new EditSourceCommand(ev));
	registerCommand(m_commands, new ViewSourceCommand(ev));
	registerCommand(m_commands, new EditorObjectIsCommand(ev));
	registerCommand(m_commands, new DialogCommand(ev));
	registerCommand(m_commands, new TabCommand(ev));

	//    non-paramaterized font size commands.
	registerCommand(m_commands, new SetFontSizeMinusTwoCommand(ev));
	registerCommand(m_commands, new SetFontSizeMinusOneCommand(ev));
	registerCommand(m_commands, new SetFontSizeZeroCommand(ev));
	registerCommand(m_commands, new SetFontSizePlusOneCommand(ev));
	registerCommand(m_commands, new SetFontSizePlusTwoCommand(ev));
	registerCommand(m_commands, new SetFontSizePlusThreeCommand(ev));
	registerCommand(m_commands, new SetFontSizePlusFourCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleNormalCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleHeadingOneCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleHeadingTwoCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleHeadingThreeCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleHeadingFourCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleHeadingFiveCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleHeadingSixCommand(ev));

	registerCommand(m_commands, new SetParagraphStyleAddressCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleFormattedCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleTitleCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleTextCommand(ev));
	registerCommand(m_commands, new SetParagraphStyleBlockQuoteCommand(ev));

	registerCommand(m_commands, new SetListStyleNoneCommand(ev));
	registerCommand(m_commands, new SetListStyleBulletedCommand(ev));
	registerCommand(m_commands, new SetListStyleNumberedCommand(ev));

	registerCommand(m_commands, new SetAlignStyleLeftCommand(ev));
	registerCommand(m_commands, new SetAlignStyleCenterCommand(ev));
	registerCommand(m_commands, new SetAlignStyleRightCommand(ev));

	registerCommand(m_commands, new ToggleCharacterStyleBoldCommand(ev));
	registerCommand(m_commands, new ToggleCharacterStyleItalicCommand(ev));
	registerCommand(m_commands, new ToggleCharacterStyleUnderlineCommand(ev));
	registerCommand(m_commands, new ToggleCharacterStyleFixedCommand(ev));
	registerCommand(m_commands, new ToggleCharacterStyleSuperscriptCommand(ev));
	registerCommand(m_commands, new ToggleCharacterStyleSubscriptCommand(ev));
	registerCommand(m_commands, new ToggleCharacterStyleStrikethroughCommand(ev));
	registerCommand(m_commands, new ToggleCharacterStyleBlinkCommand(ev));

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

			registerCommand(m_commands, new JavaPluginCommand(name, nc, np, ev));
		}
	}

	//
	//    Insert these last, so they are at the head of the command
	//    list, and will get found quickly.
	//
	registerCommand(m_commands, new InsertReturnCommand(ev));
	registerCommand(m_commands, new SelectCommand(ev));
	registerCommand(m_commands, new MoveCommand(ev));
	registerCommand(m_commands, new KeyInsertCommand(ev));
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
	if (m_commands) {
		destroyCommandList(m_commands);
		m_commands = 0;
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
	XFE_Command* command = findCommand(m_commands, cmd);

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
