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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/RepType.h>											*/
/* Description:	Xfe widgets representation types source.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xm/RepType.h>
#include <Xfe/XfeP.h>

/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeRepTypeCheck(Widget				w,
				String				rep_type,
				unsigned char *		address,
				unsigned char		fallback)
/*----------------------------------------------------------------------*/
{
	Boolean result = True;

	assert( address != NULL );

	if (!XmRepTypeValidValue(XmRepTypeGetId(rep_type),*address,w))
	{
		result = False;

		*address = fallback;
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterBoxType()
{
    static String BoxNames[] = 
    { 
		"box_none",
		"box_plain",
		"box_shadow"
    };
    
    XmRepTypeRegister(XmRBoxType,BoxNames,NULL,XtNumber(BoxNames));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterButtonLayout()
{
    static String names[] = 
    { 
		"button_label_only",
		"button_label_on_bottom",
		"button_label_on_left",
		"button_label_on_right",
		"button_label_on_top",
		"button_pixmap_only"
    };
    
    XmRepTypeRegister(XmRButtonLayout,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterButtonTrigger()
{
    static String names[] = 
    { 
		"button_trigger_anywhere",
		"button_trigger_label",
		"button_trigger_pixmap",
		"button_trigger_either",
		"button_trigger_neither"
    };
    
    XmRepTypeRegister(XmRButtonTrigger,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
#ifdef notused
/* extern */ void
XfeRegisterArrowType()
{
    static String ArrowNames[] = 
    { 
		"arrow_pointer",
		"arrow_pointer_base",
		"arrow_triangle",
		"arrow_triangle_base"
    };
    
/*    XmRepTypeRegister(XmRArrowType,ArrowNames,NULL,XtNumber(ArrowNames));*/
}
/*----------------------------------------------------------------------*/
#endif
/* extern */ void
XfeRegisterBufferType()
{
    static String BufferNames[] = 
    { 
		"buffer_shared",
		"buffer_none",
		"buffer_private"
    };
    
    XmRepTypeRegister(XmRBufferType,BufferNames,NULL,XtNumber(BufferNames));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterButtonType()
{
    static String ButtonNames[] = 
    { 
		"button_none",
		"button_push",
		"button_toggle"
    };
    
    XmRepTypeRegister(XmRButtonType,ButtonNames,NULL,XtNumber(ButtonNames));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterAccentType()
{
    static String AccentNames[] = 
    { 
		"accent_box",
		"accent_none",
		"accent_underline",
    };
    
    XmRepTypeRegister(XmRAccentType,AccentNames,NULL,XtNumber(AccentNames));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterRulesType()
{
    static String RulesNames[] = 
    { 
		"rules_date",
		"rules_option",
		"rules_text"
    };

    XmRepTypeRegister(XmRRulesType,RulesNames,NULL,XtNumber(RulesNames));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterLocationType()
{
    static String names[] = 
    { 
		"location_east",
		"location_north",
		"location_north_east",
		"location_north_west",
		"location_south",
		"location_south_east",
		"location_south_west",
		"location_west"
    };

    XmRepTypeRegister(XmRLocationType,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterComboBoxType()
{
    static String names[] = 
    { 
		"combo_box_editable",
		"combo_box_read_only"
    };

    XmRepTypeRegister(XmRComboBoxType,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterToolScrollArrowPlacement()
{
    static String names[] = 
    { 
		"tool_scroll_arrow_placement_both",
		"tool_scroll_arrow_placement_end",
		"tool_scroll_arrow_placement_start"
    };
    
    XmRepTypeRegister(XmRToolScrollArrowPlacement,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterPaneChildType()
{
    static String names[] = 
    { 
		"pane_child_none",
		"pane_child_attachment_one",
		"pane_child_attachment_two",
		"pane_child_work_area_one",
		"pane_child_work_area_two"
    };
    
    XmRepTypeRegister(XmRPaneChildType,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterPaneDragModeType()
{
    static String names[] = 
    { 
		"pane_drag_preserve_one",
		"pane_drag_preserve_two",
		"pane_drag_preserve_ratio"
    };
    
    XmRepTypeRegister(XmRPaneDragMode,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterPaneChildAttachment()
{
    static String names[] = 
    { 
		"pane_child_attach_none",
		"pane_child_attach_bottom",
		"pane_child_attach_left",
		"pane_child_attach_right",
		"pane_child_attach_top",
    };
    
    XmRepTypeRegister(XmRPaneChildAttachment,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterPaneSashType()
{
    static String names[] = 
    { 
		"pane_sash_double_line",
		"pane_sash_filled_rectangle",
		"pane_sash_live",
		"pane_sash_rectangle",
		"pane_sash_single_line"
    };

    XmRepTypeRegister(XmRPaneSashType,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterToolBarSelectionPolicy()
{
    static String names[] = 
    { 
		"tool_bar_select_none",
		"tool_bar_select_single",
		"tool_bar_select_multiple"
    };
	
    XmRepTypeRegister(XmRToolBarSelectionPolicy,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterToolBarIndicatorLocation()
{
    static String names[] = 
    { 
		"indicator_location_none",
		"indicator_location_beginning",
		"indicator_location_end",
		"indicator_location_middle"
    };
	
    XmRepTypeRegister(XmRToolBarIndicatorLocation,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterChromeChildType()
{
    static String names[] = 
    { 
		"chrome_bottom_view",
		"chrome_center_view",
		"chrome_dash_board",
		"chrome_ignore",
		"chrome_left_view",
		"chrome_menu_bar",
		"chrome_right_view",
		"chrome_tool_box",
		"chrome_top_view"
    };
    
    XmRepTypeRegister(XmRChromeChildType,names,NULL,XtNumber(names));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterRepresentationTypes()
{
	XfeRegisterAccentType();
	XfeRegisterBoxType();
	XfeRegisterBufferType();
	XfeRegisterButtonLayout();
	XfeRegisterButtonTrigger();
	XfeRegisterButtonType();
	XfeRegisterChromeChildType();
	XfeRegisterComboBoxType();
	XfeRegisterLocationType();
	XfeRegisterPaneChildAttachment();
	XfeRegisterPaneChildType();
	XfeRegisterPaneDragModeType();
	XfeRegisterPaneSashType();
	XfeRegisterRulesType();
	XfeRegisterToolBarIndicatorLocation();
	XfeRegisterToolBarSelectionPolicy();
	XfeRegisterToolScrollArrowPlacement();

/* 	XfeRegisterArrowType(); */
}
/*----------------------------------------------------------------------*/
