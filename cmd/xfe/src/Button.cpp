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
   Button.cpp -- implementation file for XFE_Button.
   Created: Chris McAfee <mcafee@netscape.com>, Wed Nov  6 11:11:42 PST 1996
 */



#include "Button.h"
#include "Command.h"
#include "mozilla.h"
#include "icons.h"
#include "prefapi.h"

#include <Xfe/Button.h>
#include <Xfe/Cascade.h>
#include <Xfe/BmButton.h>
#include <Xfe/BmCascade.h>
#include <Xm/XmAll.h>

#include "xpgetstr.h"			// for XP_GetString()
extern int XFE_UNTITLED;

const char *
XFE_Button::doCommandCallback = "XFE_Button::doCommandCallback";

#if defined(DEBUG_tao_)
#define XDBG(x) x
#else
#define XDBG(x) 
#endif

void
XFE_Button::tip_cb(Widget w, XtPointer clientData, XtPointer cb_data)
{
	XFE_Button *obj = (XFE_Button *) clientData;
	obj->tipCB(w, cb_data);
}

void
XFE_Button::tipCB(Widget /* w */, XtPointer cb_data)
{
	XFE_TipStringCallbackStruct* cbs = (XFE_TipStringCallbackStruct*)cb_data;

	/* get frame first
	 */
	XFE_Frame * frame = (XFE_Frame *) getToplevel();

	if (!frame) 
	{
		return;
	}/* if */

	XDBG(printf("\n *** XFE_Button::tipCB: parent Frame found for %s! *** \n", 
				XtName(w)));

	char *rtn = 0;

	if (cbs->reason == XFE_DOCSTRING) 
	{
		rtn = frame->getDocString(m_cmd);
	} 
	else if (cbs->reason == XFE_TIPSTRING) 
	{
		rtn = frame->getTipString(m_cmd);
	}
	if (rtn)
	{
		*cbs->string = rtn;
	}
}

// Can't just pass in a XFE_Frame, we might be in a XFE_Dialog.
// Cast to XFE_Component superclass.
//////////////////////////////////////////////////////////////////////////
XFE_Button::XFE_Button(XFE_Frame *		frame,
					   Widget			parent,
                       const char *		name,
                       IconGroup *		iconGroup,
                       IconGroup *		iconGroup2,
                       IconGroup *		iconGroup3,
                       IconGroup *		iconGroup4)
	: XFE_Component(frame)
{
	XP_ASSERT(parent);
	XP_ASSERT(frame);

	m_icons[0] = iconGroup;
	m_icons[1] = iconGroup2;
	m_icons[2] = iconGroup3;
	m_icons[3] = iconGroup4;
	m_currentIconGroup = 0;
	
	m_name = name ? name : XP_GetString(XFE_UNTITLED);
	m_cmd = Command::intern((char *) m_name);
	m_callData = NULL;

	m_widget = createButton(parent,xfeButtonWidgetClass);

	installDestroyHandler();

	// Set the new pixmap group if needed
	if (m_icons[m_currentIconGroup])
	{
		setPixmap(m_icons[m_currentIconGroup]);
	}
}
//////////////////////////////////////////////////////////////////////////
XFE_Button::XFE_Button(XFE_Frame *		frame,
					   Widget			parent,
                       const char *		name,
					   MenuSpec *		spec,
                       IconGroup *		iconGroup,
                       IconGroup *		iconGroup2,
                       IconGroup *		iconGroup3,
                       IconGroup *		iconGroup4)
	: XFE_Component(frame)
{
	XP_ASSERT(parent);
	XP_ASSERT(frame);

	m_icons[0] = iconGroup;
	m_icons[1] = iconGroup2;
	m_icons[2] = iconGroup3;
	m_icons[3] = iconGroup4;
	m_currentIconGroup = 0;

	m_name = name ? name : XP_GetString(XFE_UNTITLED);
	m_cmd = Command::intern((char *) m_name);
	m_callData = NULL;

	m_widget = createButton(parent,xfeCascadeWidgetClass);

	installDestroyHandler();
	
	// Set the new pixmap group if needed
	if (m_icons[m_currentIconGroup])
	{
		setPixmap(m_icons[m_currentIconGroup]);
	}

	setMenuSpec(spec);

}
//////////////////////////////////////////////////////////////////////////
XFE_Button::XFE_Button(XFE_Frame *			frame,
                       Widget				parent,
                       const char *			name,
					   dynamenuCreateProc	generateProc,
					   void *				generateArg,
                       IconGroup *			iconGroup,
                       IconGroup *			iconGroup2,
                       IconGroup *			iconGroup3,
                       IconGroup *			iconGroup4)
	: XFE_Component(frame)
{
	XP_ASSERT(parent);
	XP_ASSERT(frame);

	m_icons[0] = iconGroup;
	m_icons[1] = iconGroup2;
	m_icons[2] = iconGroup3;
	m_icons[3] = iconGroup4;
	m_currentIconGroup = 0;

	m_name = name ? name : XP_GetString(XFE_UNTITLED);
	m_cmd = Command::intern((char *) m_name);
	m_callData = NULL;

	m_widget = createButton(parent,xfeCascadeWidgetClass);

	installDestroyHandler();
	
	// Set the new pixmap group if needed
	if (m_icons[m_currentIconGroup])
	{
		setPixmap(m_icons[m_currentIconGroup]);
	}

	(*generateProc)(m_widget,generateArg,frame);

}
//////////////////////////////////////////////////////////////////////////
XFE_Button::~XFE_Button()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::setPixmap(IconGroup * icons)
{
	fe_buttonSetPixmaps(m_widget,icons);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::setLabel(char *label)
{
  XfeSetXmStringPSZ(m_widget, XmNlabelString, XmFONTLIST_DEFAULT_TAG, label);
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_Button::createButton(Widget parent,WidgetClass wc)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( (wc == xfeButtonWidgetClass) || (wc == xfeCascadeWidgetClass) );

    Widget button;

    button = XtVaCreateWidget(m_name,wc,parent,
							  XmNinstancePointer,	this,
							  NULL);
	
    XtAddCallback(button,
				  XmNactivateCallback,
				  &XFE_Button::activate_cb,
				  (XtPointer) this);

	/* Add tooltip to button */
#if 1
	fe_AddTipStringCallback(button, XFE_Button::tip_cb, this);
#else
	// Add tooltip to button
	fe_WidgetAddToolTips(button);
#endif
	return button;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::activate_cb(Widget		w,
						XtPointer	clientData, 
						XtPointer	callData)
{	
	XFE_Button *				button = (XFE_Button*) clientData;
	XfeButtonCallbackStruct *	cd = (XfeButtonCallbackStruct *) callData;
	CommandType					cmd = button->getCmd();
	void *						cmdCallData = button->getCallData();


	// The command info - only widget and event available
	XFE_CommandInfo				info(XFE_COMMAND_BUTTON_ACTIVATE,w,cd->event);

	// Command arguments
	XFE_DoCommandArgs			cmdArgs(cmd,cmdCallData,&info);

	// Send a message that will perform an action.
	button->notifyInterested(XFE_Button::doCommandCallback,(void *) &cmdArgs);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::sub_menu_cb(Widget		w,
						XtPointer	clientData, 
						XtPointer	callData)
{	
	XFE_Button *			obj = (XFE_Button*) clientData;
	XmAnyCallbackStruct *	cbs = (XmAnyCallbackStruct *) callData;
    CommandType				cmd = Command::intern(XtName(w));

	XP_ASSERT( obj != NULL );

	if (!obj)
	{
		return;
	}

	XFE_Frame * frame = (XFE_Frame *) obj->getToplevel();

	XP_ASSERT( frame != NULL );

	if (!frame)
	{
		return;
	}

	switch(cbs->reason)
	{
	case XmCR_ARM:

		frame->notifyInterested(Command::commandArmedCallback,(void*) cmd);

		break;

	case XmCR_DISARM:

		frame->notifyInterested(Command::commandDisarmedCallback,(void*)cmd);

		break;

	case XmCR_ACTIVATE:
	    {
			XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
								   w,cbs->event,NULL,0);

			MenuSpec * spec = (MenuSpec *) XfeUserData(w);

			if ( spec && spec->tag == CUSTOMBUTTON ) {
				cmd = spec->getCmdName();
				if (frame->handlesCommand(cmd) && frame->isCommandEnabled(cmd))
				{
					xfe_ExecuteCommand(frame, cmd, spec->callData, &e_info);
				}
			} else {
				if (frame->handlesCommand(cmd) && frame->isCommandEnabled(cmd))
				{
					xfe_ExecuteCommand(frame, cmd, NULL, &e_info);
				}
			}
		}
	    break;


	case XmCR_VALUE_CHANGED:
	    {
			XFE_CommandInfo e_info(XFE_COMMAND_BUTTON_ACTIVATE,
								   w,cbs->event,NULL,0);
			
//			if (frame->handlesCommand(cmd) && frame->isCommandSelected(cmd))
			if (frame->handlesCommand(cmd))
			{
				xfe_ExecuteCommand(frame, cmd, NULL, &e_info);
			}
		}
	    break;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::setMenuSpec(MenuSpec * spec)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XfeIsCascade(m_widget) );

	Widget		sub_menu_id;
	MenuSpec *	cur_spec;

	XtVaGetValues(m_widget,XmNsubMenuId,&sub_menu_id,NULL);

	XP_ASSERT( XfeIsAlive(sub_menu_id) );

	// Make sure a menu spec is given
	if (!spec)
	{
		return;
	}
	
	cur_spec = spec;

	// Create toolbar buttons
	while (cur_spec->menuItemName)
    {
		switch(cur_spec->tag)
		{
		case PUSHBUTTON:
		{
			Widget pb=XtVaCreateManagedWidget(cur_spec->menuItemName,
									xmPushButtonGadgetClass,
									sub_menu_id,
                                    XmNuserData, cur_spec,
									NULL);
			
 			XtAddCallback(pb,
						  XmNactivateCallback,
 						  XFE_Button::sub_menu_cb,
 						  (XtPointer) this);

 			XtAddCallback(pb,
						  XmNarmCallback,
 						  XFE_Button::sub_menu_cb,
 						  (XtPointer) this);

 			XtAddCallback(pb,
						  XmNdisarmCallback,
 						  XFE_Button::sub_menu_cb,
 						  (XtPointer) this);
		}
		break;

		case CUSTOMBUTTON:
		{
			char* str;
			XmString xm_str;
			KeySym mnemonic;

			if ( !PREF_GetLabelAndMnemonic((char*) cur_spec->callData, &str, 
									&xm_str, &mnemonic) ) break;

			if ( !strcmp(str, "-") ) {
				XtVaCreateManagedWidget(spec->menuItemName,
										xmSeparatorGadgetClass,
										sub_menu_id,
                                    	XmNshadowThickness, 2,
										NULL);
			} else {
				Widget pb=XtVaCreateManagedWidget(cur_spec->menuItemName,
										xmPushButtonGadgetClass,
										sub_menu_id,
										XmNuserData, cur_spec,
										XmNlabelString, xm_str,
										mnemonic ? XmNmnemonic : NULL, mnemonic,
										NULL);
			
 				XtAddCallback(pb,
							  XmNactivateCallback,
 							  XFE_Button::sub_menu_cb,
 							  (XtPointer) this);

 				XtAddCallback(pb,
							  XmNarmCallback,
 							  XFE_Button::sub_menu_cb,
 							  (XtPointer) this);

 				XtAddCallback(pb,
							  XmNdisarmCallback,
 							  XFE_Button::sub_menu_cb,
 							  (XtPointer) this);
			}

			XmStringFree(xm_str);
		}
		break;

		case TOGGLEBUTTON:
		{
			Widget tb=XtVaCreateManagedWidget(cur_spec->menuItemName,
											  xmToggleButtonGadgetClass,
											  sub_menu_id,
											  XmNuserData, cur_spec,
											  NULL);
			
 			XtAddCallback(tb,
						  XmNvalueChangedCallback,
 						  XFE_Button::sub_menu_cb,
 						  (XtPointer) this);

 			XtAddCallback(tb,
						  XmNarmCallback,
 						  XFE_Button::sub_menu_cb,
 						  (XtPointer) this);

 			XtAddCallback(tb,
						  XmNdisarmCallback,
 						  XFE_Button::sub_menu_cb,
 						  (XtPointer) this);
		}
		break;
		
		case SEPARATOR:

			XtVaCreateManagedWidget(spec->menuItemName,
									xmSeparatorGadgetClass,
									sub_menu_id,
                                    XmNshadowThickness, 2,
									NULL);


			break;

		case DYNA_MENUITEMS:

			XP_ASSERT( cur_spec != NULL );
			XP_ASSERT( cur_spec->generateProc != NULL );

			(*cur_spec->generateProc)(m_widget,
									  cur_spec->callData,
									  (XFE_Frame *) m_toplevel);


			break;

		case DYNA_CASCADEBUTTON:
		case DYNA_FANCY_CASCADEBUTTON:
		{
			XP_ASSERT( cur_spec != NULL );
			XP_ASSERT( cur_spec->generateProc != NULL );

			Widget		top_level = m_toplevel->getBaseWidget();
			Visual *	visual = XfeVisual(top_level);
			Colormap	cmap = XfeColormap(top_level);
			Cardinal	depth = XfeDepth(top_level);
			Arg			av[10];
			Cardinal	ac = 0;
			Widget		pulldown;
			Widget		cascade;
			WidgetClass	wc = NULL;

			ac = 0;
			XtSetArg(av[ac],XmNvisual,		visual);	ac++;
			XtSetArg(av[ac],XmNcolormap,	cmap);		ac++;
			XtSetArg(av[ac],XmNdepth,		depth);		ac++;
			
			pulldown = XmCreatePulldownMenu(sub_menu_id,"pulldown",av,ac);

			if (cur_spec->tag == DYNA_FANCY_CASCADEBUTTON)
			{
				wc = xfeBmCascadeWidgetClass;
			}
			else
			{
				wc = xmCascadeButtonWidgetClass;
			}

			cascade = XtVaCreateManagedWidget(cur_spec->menuItemName,
											  wc,
											  sub_menu_id,
											  XmNsubMenuId, pulldown,
											  NULL);

			
			(*cur_spec->generateProc)(cascade,
									  cur_spec->callData,
									  (XFE_Frame *) m_toplevel);
		}
		break;

		default:
			
			XP_ASSERT(0);
			
			break;
		}
		
		cur_spec++;
    }
}
//////////////////////////////////////////////////////////////////////////
int
XFE_Button::getIconGroupIndex()
{
  return m_currentIconGroup;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::setIconGroups (IconGroup *iconGroup,
                           IconGroup *iconGroup2,
                           IconGroup *iconGroup3,
                           IconGroup *iconGroup4)
{
  m_icons[0] = iconGroup;
  m_icons[1] = iconGroup2;
  m_icons[2] = iconGroup3;
  m_icons[3] = iconGroup4;
  if (m_icons[m_currentIconGroup] == NULL) 
    m_currentIconGroup = 0;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::useIconGroup (int iconGroupIndex)
{
  if (m_currentIconGroup != iconGroupIndex
      && iconGroupIndex >= 0
      && iconGroupIndex < MAX_ICON_GROUPS
      && m_icons[iconGroupIndex] != NULL)
    {
      setPixmap(m_icons[iconGroupIndex]);
      m_currentIconGroup = iconGroupIndex;
    }
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::setPretendSensitive(Boolean state)
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	XfeSetPretendSensitive(m_widget,state);
}
//////////////////////////////////////////////////////////////////////////
Boolean
XFE_Button::isPretendSensitive()
{
	XP_ASSERT( XfeIsAlive(m_widget) );

	return XfeIsPretendSensitive(m_widget);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_Button::setPopupDelay(int delay)
{
	XP_ASSERT( XfeIsAlive(m_widget) );
	XP_ASSERT( XfeIsCascade(m_widget) );

	XtVaSetValues(m_widget,XmNmappingDelay,delay,NULL);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// fe_buttonSetPixmaps()
//
// This function should be used to configure the icons of any widget that
// is a XfeButton (or subclass).
//
// Only the normal pixmap/mask (XmNpixmap,XmNpixmapMask) and the mouse
// over pixmap/mask (XmNraisedPixmap/XmNraisedMask) are created and used.
//
// The XfeButton will render the insensitive effect for the pixmap 
// automatically.  Also, the button will be filled on arm.  This provides
// enough "mouse-down" feedback and thus reduces the XID resource 
// consumption (pixmaps and pixels).
//
//////////////////////////////////////////////////////////////////////////
/* extern */ void
fe_buttonSetPixmaps(Widget button,IconGroup * ig)
{
	XP_ASSERT( ig != NULL );
	XP_ASSERT( XfeIsAlive(button) );
	XP_ASSERT( XfeIsButton(button) );

	if (!ig || !XfeIsAlive(button))
	{
		return;
	}

	// Use the closest ancestor shell's colormap
	Widget	shell_for_colormap = 
		XfeAncestorFindByClass(button,shellWidgetClass,XfeFIND_ANY);

	XP_ASSERT( XfeIsAlive(shell_for_colormap) );

	// Normal pixmap
	if (!XfePixmapGood(ig->pixmap_icon.pixmap))
	{
		IconGroup_createOneIcon(&ig->pixmap_icon,
								ig->pixmap_data,
								shell_for_colormap,
								XfeForeground(button),
								XfeBackground(button));
	}

	// Mouse Over pixmap (raised)
	if (!XfePixmapGood(ig->pixmap_mo_icon.pixmap))
	{
		IconGroup_createOneIcon(&ig->pixmap_mo_icon,
								ig->pixmap_mo_data,
								shell_for_colormap,
								XfeForeground(button),
								XfeBackground(button));
	}

	Arg			av[4];
	Cardinal	ac = 0;

	// XmNpixmap
	if (XfePixmapGood(ig->pixmap_icon.pixmap))
	{
		XtSetArg(av[ac],XmNpixmap,ig->pixmap_icon.pixmap); ac++;
	}

	// XmNpixmapMask
	if (XfePixmapGood(ig->pixmap_icon.mask))
	{
		XtSetArg(av[ac],XmNpixmapMask,ig->pixmap_icon.mask); ac++;
	}

	// XmNraisedPixmap
	if (XfePixmapGood(ig->pixmap_mo_icon.pixmap))
	{
		XtSetArg(av[ac],XmNraisedPixmap,ig->pixmap_mo_icon.pixmap); ac++;
	}

	// XmNraisedPixmapMask
	if (XfePixmapGood(ig->pixmap_mo_icon.mask))
	{
		XtSetArg(av[ac],XmNraisedPixmapMask,ig->pixmap_mo_icon.mask); ac++;
	}

	if (ac)
	{
		XtSetValues(button,av,ac);
	}
}
//////////////////////////////////////////////////////////////////////////
