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
   Command.cpp -- command goop
   Created: Chris Toshok <toshok@netscape.com>, 21-Sep-96.
 */


#include "Command.h"
#include "Frame.h"
#include "View.h"
#include "ViewGlue.h"
#include "commands.h"

#include <xpgetstr.h>     /* for XP_GetString() */

extern int XFE_ACTION_SYNTAX_ERROR;

const char * Command::commandArmedCallback = "Command::commandArmedCallback";
const char * Command::commandDispatchedCallback = "Command::commandDispatchedCallback";
const char * Command::commandDisarmedCallback = "Command::commandDisarmedCallback";
const char * Command::doCommandCallback = "Command::doCommandCallback";

XFE_CommandInfo::XFE_CommandInfo(XFE_CommandEventType t,
								 Widget w, XEvent* e,
								 String* p,
								 Cardinal n) {
	type = t;
	widget = w;
	if (e != NULL ) {
		event = e;
	} else {
		if (XtIsSubclass(widget, xmGadgetClass))
			widget = XtParent(widget);
		event_store.type = 0;
		event_store.xany.serial = 0;
		event_store.xany.send_event = False;
		event_store.xany.display = XtDisplay(widget);
		event_store.xany.window = XtWindow(widget);
		event = &event_store;
	}
	params = p;

	if (n == 0) {
		for (; p != NULL && p[n] != NULL; n++)
			;
	}

	nparams_store = n;
	nparams = &nparams_store;
}

//
//    These methods impliment simple defaults, so a subclass needs
//    only impliment doCommand.
//
XFE_Command::XFE_Command(CommandType id)
{
	m_id = id;
}

#if 0
// inlined.
CommandType
XFE_Command::getId()
{
	return m_id;
}
#endif

char*
XFE_Command::getName()
{
	return Command::getString(m_id);
}

XP_Bool
XFE_Command::isDynamic()
{
	return FALSE;
}

XP_Bool
XFE_Command::isSlow()
{
	return TRUE; // of course
}

XP_Bool
XFE_Command::isEnabled(XFE_View*, XFE_CommandInfo*)
{
	return TRUE;
}

XP_Bool
XFE_Command::isSelected(XFE_View*, XFE_CommandInfo*)
{
	return FALSE;
}

XP_Bool
XFE_Command::isDeterminate(XFE_View*, XFE_CommandInfo*)
{
	return TRUE;
}

XFE_CommandParameters*
XFE_Command::getParameters(XFE_View*)
{
	return NULL;
}

int
XFE_Command::getParameterIndex(XFE_View*)
{
	return 0;
}

void
XFE_Command::setParameterIndex(XFE_View*, unsigned)
{
}

char*
XFE_Command::getLabel(XFE_View*, XFE_CommandInfo*)
{
	return NULL; // we could probably impliment this.
}

char*
XFE_Command::getTipString(XFE_View*, XFE_CommandInfo*)
{
	return NULL; // we could probably impliment this.
}

char*
XFE_Command::getDocString(XFE_View*, XFE_CommandInfo*)
{
	return NULL; // we could probably impliment this.
}

XP_Bool
XFE_Command::isEnabled(XFE_Frame*, XFE_CommandInfo*)
{
	return TRUE;
}

XP_Bool
XFE_Command::isSelected(XFE_Frame*, XFE_CommandInfo*)
{
	return FALSE;
}

XP_Bool
XFE_Command::isDeterminate(XFE_Frame*, XFE_CommandInfo*)
{
	return TRUE;
}

XFE_CommandParameters*
XFE_Command::getParameters(XFE_Frame*)
{
	return NULL;
}

int
XFE_Command::getParameterIndex(XFE_Frame*)
{
	return 0;
}

void
XFE_Command::setParameterIndex(XFE_Frame*, unsigned)
{
}

char*
XFE_Command::getLabel(XFE_Frame*, XFE_CommandInfo*)
{
	return NULL;
}

char* 
XFE_Command::getTipString(XFE_Frame*, XFE_CommandInfo*)
{
	return NULL;
}

char*
XFE_Command::getDocString(XFE_Frame*, XFE_CommandInfo*)
{
	return NULL;
}

void
CommandDoSyntaxErrorAlert(MWContext* context, 
						  XFE_Command* command,
						  XFE_CommandInfo* info)
{
    char buf[1024];
    char buf2[1024];

	XP_STRCPY(buf, command->getName());
	XP_STRCAT(buf, "(");
	if (info != NULL) {
		for (unsigned n = 0; n < *info->nparams; n++) {
			if (n != 0)
				XP_STRCAT(buf, ",");
			XP_STRCAT(buf, info->params[n]);
		}
	}
	XP_STRCAT(buf, ")");

    sprintf(buf2, XP_GetString(XFE_ACTION_SYNTAX_ERROR), buf);

    FE_Alert(context, buf2);
}

static XFE_View*
frame_to_view(XFE_Command* command, XFE_Frame* frame, XFE_CommandInfo*)
{
	XFE_View* view = frame->getView();
	XFE_View* sub_view = view->getCommandView(command);

	if (sub_view)
		return sub_view;
	else
		return NULL;
}

XP_Bool
XFE_ViewCommand::isEnabled(XFE_Frame* frame, XFE_CommandInfo* info)
{
	XFE_View* view = frame_to_view(this, frame, info);

	return ((XFE_AbstractCommand*)this)->isEnabled(view, info);
}

XP_Bool
XFE_ViewCommand::isSelected(XFE_Frame* frame, XFE_CommandInfo* info)
{
	XFE_View* view = frame_to_view(this, frame, info);

	return ((XFE_AbstractCommand*)this)->isSelected(view, info);
}

XP_Bool
XFE_ViewCommand::isDeterminate(XFE_Frame* frame, XFE_CommandInfo* info)
{
	XFE_View* view = frame_to_view(this, frame, info);

	return ((XFE_AbstractCommand*)this)->isDeterminate(view, info);
}

XFE_CommandParameters*
XFE_ViewCommand::getParameters(XFE_Frame* frame)
{
	XFE_View* view = frame_to_view(this, frame, NULL);

	return ((XFE_AbstractCommand*)this)->getParameters(view);
}

void
XFE_ViewCommand::setParameterIndex(XFE_Frame* frame, unsigned index)
{
	XFE_View* view = frame_to_view(this, frame, NULL);

	((XFE_AbstractCommand*)this)->setParameterIndex(view, index);
}

int
XFE_ViewCommand::getParameterIndex(XFE_Frame* frame)
{
	XFE_View* view = frame_to_view(this, frame, NULL);

	return ((XFE_AbstractCommand*)this)->getParameterIndex(view);
}

char*
XFE_ViewCommand::getLabel(XFE_Frame* frame, XFE_CommandInfo* info)
{
	XFE_View* view = frame_to_view(this, frame, info);

	return ((XFE_AbstractCommand*)this)->getLabel(view, info);
}

char*
XFE_ViewCommand::getTipString(XFE_Frame* frame, XFE_CommandInfo* info)
{
	XFE_View* view = frame_to_view(this, frame, info);

	return ((XFE_AbstractCommand*)this)->getTipString(view, info);
}

char*
XFE_ViewCommand::getDocString(XFE_Frame* frame, XFE_CommandInfo* info)
{
	XFE_View* view = frame_to_view(this, frame, info);

	return ((XFE_AbstractCommand*)this)->getDocString(view, info);
}

void
XFE_ViewCommand::doCommand(XFE_Frame* frame, XFE_CommandInfo* info)
{
	XFE_View* view = frame_to_view(this, frame, info);

	((XFE_AbstractCommand*)this)->doCommand(view, info);
}

void
XFE_ViewCommand::doSyntaxErrorAlert(XFE_View* view,  XFE_CommandInfo* info)
{
	CommandDoSyntaxErrorAlert(view->getContext(), this, info);
}

XFE_Frame*
XFE_FrameCommand::getFrame(XFE_View* view)
{
	return ViewGlue_getFrame(view->getContext());
}

XP_Bool
XFE_FrameCommand::isEnabled(XFE_View* view, XFE_CommandInfo* info)
{
	return ((XFE_AbstractCommand*)this)->isEnabled(getFrame(view), info);
}

XP_Bool
XFE_FrameCommand::isSelected(XFE_View* view, XFE_CommandInfo* info)
{
	return ((XFE_AbstractCommand*)this)->isSelected(getFrame(view), info);
}

XP_Bool
XFE_FrameCommand::isDeterminate(XFE_View* view, XFE_CommandInfo* info)
{
	return ((XFE_AbstractCommand*)this)->isDeterminate(getFrame(view), info);
}

XFE_CommandParameters*
XFE_FrameCommand::getParameters(XFE_View* view)
{
	return ((XFE_AbstractCommand*)this)->getParameters(getFrame(view));
}

int
XFE_FrameCommand::getParameterIndex(XFE_View* view)
{
	return ((XFE_AbstractCommand*)this)->getParameterIndex(getFrame(view));
}

void
XFE_FrameCommand::setParameterIndex(XFE_View* view, unsigned index)
{
	XFE_Frame* frame = getFrame(view);
	((XFE_AbstractCommand*)this)->setParameterIndex(frame, index);
}

char*
XFE_FrameCommand::getLabel(XFE_View* view, XFE_CommandInfo* info)
{
	return ((XFE_AbstractCommand*)this)->getLabel(getFrame(view), info);
}

char*
XFE_FrameCommand::getTipString(XFE_View* view, XFE_CommandInfo* info)
{
	return ((XFE_AbstractCommand*)this)->getTipString(getFrame(view), info);
}

char*
XFE_FrameCommand::getDocString(XFE_View* view, XFE_CommandInfo* info)
{
	return ((XFE_AbstractCommand*)this)->getDocString(getFrame(view), info);
}

void
XFE_FrameCommand::doCommand(XFE_View* view, XFE_CommandInfo* info)
{
	((XFE_AbstractCommand*)this)->doCommand(getFrame(view), info);
}

void
XFE_FrameCommand::doSyntaxErrorAlert(XFE_Frame* frame,  XFE_CommandInfo* info)
{
	CommandDoSyntaxErrorAlert(frame->getContext(), this, info);
}

XFE_CommandList::XFE_CommandList(XFE_CommandList* list, XFE_Command* command)
{
	m_next = list;
	m_command = command;
}

XFE_CommandList*
registerCommand(XFE_CommandList*& list, XFE_Command* command)
{
	return list = new XFE_CommandList(list, command);
}

XFE_Command*
findCommand(XFE_CommandList* list, CommandType id)
{
	while (list != NULL) {
		if (list->m_command != NULL && list->m_command->getId() == id) {
			return list->m_command;
		}
		list = list->m_next;
	}
	return NULL;
}

XFE_ObjectIsCommand::XFE_ObjectIsCommand() : XFE_ViewCommand(xfeCmdObjectIs)
{
};

void
XFE_ObjectIsCommand::doCommand(XFE_View* view, XFE_CommandInfo* info)
{
	if (info == NULL)
		return;
	
	char*     c_type = getObjectType(view);
	char*     s_type;
	unsigned  depth;
	String*   av = info->params;
	Cardinal* ac = info->nparams;
	unsigned  n;
	unsigned  m;
	
    for (n = 0; n + 1 < *ac; n++) {
		
		s_type = av[n++];
		
		if (XP_STRCMP(av[n], "{") == 0) {
			n++;
		} else {
			// syntax error.
			return;
		}
		
		for (depth = 1, m = n; av[m] != NULL && m < *ac; m++) {
			if (XP_STRCMP(av[m], "{") == 0)
				depth++;
			else if (XP_STRCMP(av[m], "}") == 0)
				depth--;
			
			if (depth == 0)
				break;
		}
		
		if (XP_STRCASECMP(c_type, s_type) == 0 && m > n && av[m] != NULL) {
			XtCallActionProc(info->widget, "xfeDoCommand", info->event,
							 &av[n], m - n);
			break; // done
		}
	    
		n = m;
    }
};

int
XFE_commandMatchParameters(XFE_CommandParameters* params, char* name)
{
	int index;

	if (params != NULL) {
		for (index = 0; params[index].name != NULL; index++) {
			if (XP_STRCMP(name, params[index].name) == 0)
				return index;
		}
	}
	return -1;
}

struct CommandList_t
{
	char*          name;
	CommandList_t* next;
};

static CommandList_t* intern_list;

char *
Command::intern(char *foo)
{
	int middle, first, last;

	if (foo == NULL) return NULL;

	/* do a binary search through the _XfeCommandIndices table, and return
	   the correct string */

	first = 0;
	last = _XfeNumCommands - 1;

	while (first <= last)
		{
			int compare;
			char *command;

			middle = (first + last) / 2;
			
			command = &_XfeCommands[_XfeCommandIndices[middle]];

			compare = strcmp(foo, command);

			if (compare == 0)
				{
					return command;
				}
			else if (compare < 0)
				{
					last = middle - 1;
				}
			else
				{
					first = middle + 1;
				}
		}

	//
	//    Didn't find it in static list, look in dynamic list
	//
	CommandList_t* bar;
	for (bar = intern_list; bar != NULL; bar = bar->next) {
		if (strcmp(foo, bar->name) == 0)
			return bar->name;
	}

	//    Didn't match, add a new dude..
	bar = new CommandList_t;
	bar->name = XP_STRDUP(foo);
	bar->next = intern_list;
	intern_list = bar;

	return bar->name;
}

struct cmd_mapping {
	char *old_remote_cmd;
	char *xfe_do_cmd;
};

static struct cmd_mapping mapping[] = {
	/* bringing up new pages. */
	{ "new",			xfeCmdOpenBrowser },
	{ "openFile",		xfeCmdOpenPageChooseFile },
	{ "saveAs",			xfeCmdSaveAs },
#ifdef MOZ_MAIL_NEW
	{ "mailNew",		xfeCmdNewMessage },
#endif
	{ "docInfo",		xfeCmdViewPageInfo },
	{ "delete",			xfeCmdClose },
	{ "quit",			xfeCmdExit },

	/* edit menu functions. */
	{ "find",			xfeCmdFindInObject },
	{ "findAgain",		xfeCmdFindAgain },

	/* view menu functions. */
	{ "reload",			xfeCmdReload },
	{ "loadImages", 	xfeCmdShowImage },
	{ "source",			xfeCmdViewPageSource },

	/* go menu functions. */
	{ "back",			xfeCmdBack },
	{ "forward",		xfeCmdForward },
	{ "home",			xfeCmdHome },
	{ "abort",			xfeCmdStopLoading },
	{ "viewHistory",	xfeCmdOpenHistory },

	{ "viewBookmark",	xfeCmdOpenBookmarks },
	{ "preferences",	xfeCmdEditPreferences },

#ifdef MOZ_MAIL_NEWS
	{ "openInbox",		xfeCmdOpenInbox },
	{ "inbox",			xfeCmdOpenInbox },
	{ "openAddrBook",	xfeCmdOpenAddressBook },
	{ "addrbk",			xfeCmdOpenAddressBook },
	{ "openNews",		xfeCmdOpenNewsgroups },
	{ "news",			xfeCmdOpenNewsgroups },
#endif
#ifdef EDITOR
	{ "editor",			xfeCmdOpenEditor },
	{ "openEditor",		xfeCmdOpenEditor },
	{ "edit",           xfeCmdNewBlank },
	{ "editURL",        xfeCmdNewBlank },
#endif
#ifdef MOZ_MAIL_NEWS
	{ "openConference", xfeCmdOpenConference }
#endif

};
static int num_old_cmds = sizeof(mapping) / sizeof(mapping[0]);

char *
Command::convertOldRemote(char *foo)
{
	int i;

	for (i = 0; i < num_old_cmds; i ++)
		{
			if (!XP_STRCASECMP(foo, mapping[i].old_remote_cmd))
				return mapping[i].xfe_do_cmd;
		}

	return Command::intern(foo);
}
