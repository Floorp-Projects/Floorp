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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/PrefView.c>										*/
/* Description:	XfePrefView widget source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <stdio.h>

#include <Xfe/PrefViewP.h>

/* Possible children classes */
#include <Xfe/Label.h>
#include <Xfe/Button.h>
#include <Xfe/Cascade.h>

#include <XmL/Tree.h>

#define MESSAGE1 "Widget is not an XfePrefView."
#define MESSAGE2 "XmNpanelTree is a read-only resource."
#define MESSAGE3 "XmNprefInfoList is a read-only resource."

#define TREE_NAME "PrefTree"

static void whack_tree(Widget tree);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	Initialize			(Widget,Widget,ArgList,Cardinal *);
static void 	Destroy				(Widget);
static Boolean	SetValues			(Widget,Widget,Widget,ArgList,Cardinal *);
static void		GetValuesHook		(Widget,ArgList,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		ConstraintInitialize(Widget,Widget,ArgList,Cardinal *);
static Boolean	ConstraintSetValues	(Widget,Widget,Widget,ArgList,Cardinal *);
static void 	ConstraintDestroy	(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PreferredGeometry	(Widget,Dimension *,Dimension *);
static Boolean	AcceptStaticChild			(Widget);
static Boolean	InsertStaticChild			(Widget);
static Boolean	DeleteStaticChild			(Widget);
static void		LayoutComponents	(Widget);
static void		LayoutStaticChildren		(Widget);
static void		DrawComponents		(Widget,XEvent *,Region,XRectangle *);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	BtnUp				(Widget,XEvent *,char **,Cardinal *);
static void 	BtnDown				(Widget,XEvent *,char **,Cardinal *);

/*----------------------------------------------------------------------*/
/*																		*/
/* Tree functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void 	TreeCreate			(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Panel list/info functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		PanelInfoFree		(XtPointer,XtPointer);
static void		PanelListFree		(Widget);

/*----------------------------------------------------------------------*/
/*																		*/
/* Panel tree callbacks													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void		TreeSelectCB		(Widget,XtPointer,XtPointer);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView resources												*/
/*																		*/
/*----------------------------------------------------------------------*/
static const XtResource resources[] = 	
{					
	/* Tree resources */
    { 
		XmNpanelTree,
		XmCReadOnly,
		XmRWidget,
		sizeof(Widget),
		XtOffsetOf(XfePrefViewRec , xfe_pref_view . panel_tree),
		XmRImmediate, 
		(XtPointer) NULL
    },
	
	/* Panel info resources */
    { 
		XmNpanelInfoList,
		XmCPanelInfoList,
		XmRLinked,
		sizeof(XfeLinked),
		XtOffsetOf(XfePrefViewRec , xfe_pref_view . panel_info_list),
		XmRImmediate, 
		(XtPointer) NULL
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePref synthetic resources											*/
/*																		*/
/*----------------------------------------------------------------------*/
static const XmSyntheticResource syn_resources[] =
{
    { 
		XmNspacing,
		sizeof(Dimension),
		XtOffsetOf(XfePrefViewRec , xfe_pref_view . spacing),
		_XmFromHorizontalPixels,
		_XmToHorizontalPixels 
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView constraint resources										*/
/*																		*/
/*----------------------------------------------------------------------*/
static const XtResource constraint_resources[] = 
{
    { 
		XmNpanelTitle,
		XmCPanelTitle,
		XmRXmString,
		sizeof(XmString),
		XtOffsetOf(XfePrefViewConstraintRec , xfe_pref_view . panel_title),
		XmRImmediate,
		(XtPointer) NULL
    },
    { 
		XmNpanelSubTitle,
		XmCPanelSubTitle,
		XmRXmString,
		sizeof(XmString),
		XtOffsetOf(XfePrefViewConstraintRec , xfe_pref_view . panel_sub_title),
		XmRImmediate,
		(XtPointer) NULL
    },
    { 
		XmNnumGroups,
		XmCNumGroups,
		XmRCardinal,
		sizeof(Cardinal),
		XtOffsetOf(XfePrefViewConstraintRec , xfe_pref_view . num_groups),
		XmRImmediate,
		(XtPointer) 0
    },
};   

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView actions													*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtActionsRec actions[] = 
{
    { "BtnDown",			BtnDown				},
    { "BtnUp",				BtnUp				},
};

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget Class Record Initialization                                   */
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS_RECORD(prefview,PrefView) =
{
    {
		(WidgetClass) &xfeManagerClassRec,		/* superclass			*/
		"XfePrefView",							/* class_name			*/
		sizeof(XfePrefViewRec),					/* widget_size			*/
		NULL,									/* class_initialize		*/
		NULL,									/* class_part_initialize*/
		FALSE,									/* class_inited			*/
		Initialize,								/* initialize			*/
		NULL,									/* initialize_hook		*/
		XtInheritRealize,						/* realize				*/
		actions,								/* actions            	*/
		XtNumber(actions),						/* num_actions        	*/
		(XtResource *) resources,				/* resources			*/
		XtNumber(resources),                    /* num_resources		*/
		NULLQUARK,                              /* xrm_class			*/
		TRUE,                                   /* compress_motion		*/
		XtExposeCompressMaximal,                /* compress_exposure	*/
		TRUE,                                   /* compress_enterleave	*/
		FALSE,                                  /* visible_interest		*/
		Destroy,								/* destroy				*/
		XtInheritResize,                        /* resize				*/
		XtInheritExpose,						/* expose				*/
		SetValues,                              /* set_values			*/
		NULL,                                   /* set_values_hook		*/
		XtInheritSetValuesAlmost,				/* set_values_almost	*/
		GetValuesHook,							/* get_values_hook		*/
		NULL,                                   /* access_focus			*/
		XtVersion,                              /* version				*/
		NULL,                                   /* callback_private		*/
		XtInheritTranslations,					/* tm_table				*/
		XtInheritQueryGeometry,					/* query_geometry		*/
		XtInheritDisplayAccelerator,            /* display accelerator	*/
		NULL,                                   /* extension			*/
    },
    
    /* Composite Part */
    {
		XtInheritGeometryManager,				/* geometry_manager		*/
		XtInheritChangeManaged,					/* change_managed		*/
		XtInheritInsertChild,					/* insert_child			*/
		XtInheritDeleteChild,					/* delete_child			*/
		NULL									/* extension			*/
    },

    /* Constraint Part */
    {
		(XtResource *) constraint_resources,	/* constraint res		*/
		XtNumber(constraint_resources),			/* num constraint res	*/
		sizeof(XfePrefViewConstraintRec),		/* constraint size		*/
		ConstraintInitialize,					/* init proc			*/
		ConstraintDestroy,						/* destroy proc			*/
		ConstraintSetValues,					/* set values proc		*/
		NULL,                                   /* extension			*/
    },

    /* XmManager Part */
    {
		XtInheritTranslations,					/* tm_table				*/
		(XmSyntheticResource *) syn_resources,	/* syn resources		*/
		XtNumber(syn_resources),				/* num syn_resources	*/
		NULL,                                   /* syn_cont_resources  	*/
		0,                                      /* num_syn_cont_resource*/
		XmInheritParentProcess,                 /* parent_process		*/
		NULL,                                   /* extension			*/
    },
    
    /* XfeManager Part 	*/
	{
		XfeInheritBitGravity,					/* bit_gravity				*/
		PreferredGeometry,						/* preferred_geometry		*/
		XfeInheritUpdateBoundary,				/* update_boundary			*/
		XfeInheritUpdateChildrenInfo,			/* update_children_info		*/
		XfeInheritLayoutWidget,					/* layout_widget			*/
		AcceptStaticChild,						/* accept_static_child		*/
		InsertStaticChild,						/* insert_static_child		*/
		DeleteStaticChild,						/* delete_static_child		*/
		LayoutStaticChildren,					/* layout_static_children	*/
		NULL,									/* change_managed			*/
		NULL,									/* prepare_components		*/
		LayoutComponents,						/* layout_components		*/
		NULL,									/* draw_background			*/
		XfeInheritDrawShadow,					/* draw_shadow				*/
		DrawComponents,							/* draw_components			*/
		NULL,									/* extension				*/
    },

    /* XfePrefView Part */
    {
		NULL,									/* extension			*/
    },
};

/*----------------------------------------------------------------------*/
/*																		*/
/* xfePrefViewWidgetClass declaration.									*/
/*																		*/
/*----------------------------------------------------------------------*/
_XFE_WIDGET_CLASS(prefview,PrefView);

/*----------------------------------------------------------------------*/
/*																		*/
/* Core Class methods													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
Initialize(Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfePrefViewPart *		pp = _XfePrefViewPart(nw);

	/* Create the tree */
	TreeCreate(nw);

	/* Construct the panel list */
	pp->panel_info_list = XfeLinkedConstruct();

    /* Finish of initialization */
    _XfeManagerChainInitialize(rw,nw,xfePrefViewWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
Destroy(Widget w)
{
/*     XfePrefViewPart *		pp = _XfePrefViewPart(w); */

	PanelListFree(w);
}
/*----------------------------------------------------------------------*/
static Boolean
SetValues(Widget ow,Widget rw,Widget nw,ArgList args,Cardinal *nargs)
{
    XfePrefViewPart *		np = _XfePrefViewPart(nw);
    XfePrefViewPart *		op = _XfePrefViewPart(ow);

	/* XmNpanelTree */
	if (np->panel_tree != op->panel_tree)
	{
		np->panel_tree = op->panel_tree;

		_XfeWarning(nw,MESSAGE2);
	}
	
	/* XmNpanelInfoList */
	if (np->panel_info_list != op->panel_info_list)
	{
		np->panel_info_list = op->panel_info_list;

		_XfeWarning(nw,MESSAGE3);
	}
	
    return _XfeManagerChainSetValues(ow,rw,nw,xfePrefViewWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
GetValuesHook(Widget w,ArgList args,Cardinal* nargs)
{
/*     XfePrefViewPart *		pp = _XfePrefViewPart(w); */
    Cardinal				i;
    
    for (i = 0; i < *nargs; i++)
    {
#if 0
		/* label_string */
		if (strcmp(args[i].name,XmNlabelString) == 0)
		{
			*((XtArgVal *) args[i].value) = 
				(XtArgVal) XmStringCopy(lp->label_string);
		}
		/* font_list */
		else if (strcmp(args[i].name,XmNfontList) == 0)
		{
			*((XtArgVal *) args[i].value) = 
				(XtArgVal) XmFontListCopy(lp->font_list);
		}      
#endif
    }
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Constraint class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
ConstraintInitialize(Widget rc,Widget nc,ArgList args,Cardinal *nargs)
{
/*  	Widget						w = _XfeParent(nc); */
/*     XfePrefViewPart *			pp = _XfePrefViewPart(w); */

	/* Finish constraint initialization */
	_XfeConstraintChainInitialize(rc,nc,xfePrefViewWidgetClass);
}
/*----------------------------------------------------------------------*/
static void
ConstraintDestroy(Widget c)
{
/*  	Widget						w = XtParent(c); */
/*     XfePrefViewPart *			pp = _XfePrefViewPart(w); */
}
/*----------------------------------------------------------------------*/
static Boolean
ConstraintSetValues(Widget oc,Widget rc,Widget nc,ArgList args,Cardinal *nargs)
{
/* 	Widget						w = XtParent(nc); */
/*     XfePrefViewPart *			pp = _XfePrefViewPart(w); */

/*  	XfePrefViewConstraintPart *	ncp = _XfePrefViewConstraintPart(nc); */
/*  	XfePrefViewConstraintPart *	ocp = _XfePrefViewConstraintPart(oc); */

	/* Finish constraint set values */
	return _XfeConstraintChainSetValues(oc,rc,nc,xfePrefViewWidgetClass);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class methods												*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PreferredGeometry(Widget w,Dimension * width,Dimension * height)
{
/*     XfePrefViewPart *		pp = _XfePrefViewPart(w); */

}
/*----------------------------------------------------------------------*/
static Boolean
AcceptStaticChild(Widget child)
{
	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
InsertStaticChild(Widget child)
{
/*     Widget					w = XtParent(child); */
/*     XfePrefViewPart *		pp = _XfePrefViewPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static Boolean
DeleteStaticChild(Widget child)
{
/*     Widget					w = XtParent(child); */
/*     XfePrefViewPart *		pp = _XfePrefViewPart(w); */

	return True;
}
/*----------------------------------------------------------------------*/
static void
LayoutComponents(Widget w)
{
    XfePrefViewPart *	pp = _XfePrefViewPart(w);

	/* Configure the tree */
	if (_XfeChildIsShown(pp->panel_tree))
	{
		Dimension width;
		Dimension height;
		
		_XfePreferredGeometry(pp->panel_tree,&width,&height);

		/* Place the panel_tree on the left */
		_XfeConfigureWidget(pp->panel_tree,
							
							_XfemBoundaryX(w),

							_XfemBoundaryY(w),

							_XfeWidth(pp->panel_tree),

							_XfemBoundaryHeight(w));
	}
}
/*----------------------------------------------------------------------*/
static void
LayoutStaticChildren(Widget w)
{
/*     XfePrefViewPart *	pp = _XfePrefViewPart(w); */
	
}
/*----------------------------------------------------------------------*/
static void
DrawComponents(Widget w,XEvent * event,Region region,XRectangle * clip_rect)
{
/*     XfePrefViewPart *	pp = _XfePrefViewPart(w); */

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Tree functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
TreeCreate(Widget w)
{
    XfePrefViewPart *		pp = _XfePrefViewPart(w);

	assert( pp->panel_tree == NULL );
	
    /* Initialize private members */
	pp->panel_tree = 
      XtVaCreateManagedWidget(TREE_NAME,
                              xmlTreeWidgetClass,
                              w,

							  XmNtopFixedMargin,		10,
							  XmNbottomFixedMargin,		10,
							  XmNleftFixedMargin,		10,
							  XmNrightFixedMargin,		10,


							  XmNshadowThickness, 0,

 							  XmNignorePixmaps,			True,
 							  XmNiconSpacing,			0,

							  XmNhighlightThickness, 0,

                              XmNhorizontalSizePolicy,	XmVARIABLE,
							  XmNselectionPolicy,		XmSELECT_SINGLE_ROW,

/*                               XmNglobalPixmapWidth,		1, */
/*                               XmNglobalPixmapHeight,	1, */


                              XmNvisibleRows, 10,
                              XmNshadowThickness, 0,
                              XmNshadowThickness, 0,

/*                               XmNwidth, 250, */

                              NULL);

#if 0
	XtVaSetValues(pp->panel_tree,
				  XmNcellDefaults,			True,
				  XmNcellLeftBorderType,	XmBORDER_NONE,
				  XmNcellRightBorderType,	XmBORDER_NONE,
				  XmNcellTopBorderType,		XmBORDER_NONE,
				  XmNcellBottomBorderType,	XmBORDER_NONE,
				  NULL);
#endif

#if 0
	XtVaSetValues(pp->panel_tree,
				  XmNconnectingLineColor,	_XfeBackgroundPixel(pp->panel_tree),
				  NULL);
#endif

    whack_tree(pp->panel_tree);

	XtVaSetValues(pp->panel_tree,
/* 		XmNcellDefaults, True, */
/* 		XmNcolumn, 0, */
				  XmNcellType, XmICON_CELL,
		NULL);

	/* Add tree callbacks */
    XtAddCallback(pp->panel_tree,XmNselectCallback,TreeSelectCB,(XtPointer) w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView action procedures										*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
BtnDown(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
/*     XfePrefViewPart *	pp = _XfePrefViewPart(w); */
}
/*----------------------------------------------------------------------*/
static void
BtnUp(Widget w,XEvent * event,char ** params,Cardinal * nparams)
{
/*     XfePrefViewPart *	pp = _XfePrefViewPart(w); */
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Panel list/info functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
PanelInfoFree(XtPointer item,XtPointer client_data)
{
	XfePrefPanelInfo	panel_info = (XfePrefPanelInfo) item;

	/* Destroy the panel info if needed */
	if (panel_info != NULL)
	{
		XfePrefPanelInfoDestroy(panel_info);
	}
}
/*----------------------------------------------------------------------*/
static void
PanelListFree(Widget w)
{
    XfePrefViewPart *	pp = _XfePrefViewPart(w);

	/* Destroy the panel list */
	if (pp->panel_info_list != NULL)
	{
 		XfeLinkedDestroy(pp->panel_info_list,PanelInfoFree,NULL);
	}

	pp->panel_info_list = NULL;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Panel tree callbacks													*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
TreeSelectCB(Widget tree,XtPointer client_data,XtPointer call_data)
{
	Widget					w = (Widget) client_data;
    XfePrefViewPart *		pp = _XfePrefViewPart(w);
	XmLGridCallbackStruct *	cbs = (XmLGridCallbackStruct *) call_data;
	XmLGridRow				row = NULL;

	/* Make sure the cell has content */
	if (cbs->rowType != XmCONTENT)
	{
		return;
	}

	row = XmLGridGetRow(pp->panel_tree,XmCONTENT,cbs->row);

	printf("TreeSelectCB(%s,row = %d,col = %d)\n",
		   XtName(w),cbs->row,cbs->column);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrefView Public Methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCreatePrefView(Widget pw,char * name,Arg * av,Cardinal ac)
{
	return XtCreateWidget(name,xfePrefViewWidgetClass,pw,av,ac);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfePrefViewAddPanel(Widget		w,
					String		panel_name,
					XmString	xm_panel_name,
					Boolean		has_sub_panels)
{
    XfePrefViewPart *	pp = _XfePrefViewPart(w);
	Widget				panel = NULL;
	XfePrefPanelInfo	panel_info = NULL;
	Boolean				need_to_free = False;
	
	assert( _XfeIsAlive(w) );
	assert( XfeIsPrefView(w) );

	panel_info = XfePrefPanelInfoConstruct(panel_name);

	XfeLinkedInsertAtTail(pp->panel_info_list,panel_info);

	if (xm_panel_name == NULL)
	{
		xm_panel_name = XmStringCreateLtoR(panel_name,
										   XmFONTLIST_DEFAULT_TAG);
		need_to_free = True;
	}

	XmLTreeAddRow(pp->panel_tree,
				  1,
				  has_sub_panels,
				  True,
				  XmLAST_POSITION,
				  XmUNSPECIFIED_PIXMAP,
				  XmUNSPECIFIED_PIXMAP,
				  xm_panel_name);


	if (need_to_free)
	{
		XmStringFree(xm_panel_name);
	}

	/* Create the panel */
	panel = XtVaCreateWidget("panel",
							 xfePrefPanelWidgetClass,
							 w,
							 XmNmanagerChildType,	XmMANAGER_COMPONENT_CHILD,
							 NULL);

	return panel;
}
/*----------------------------------------------------------------------*/
static Boolean
FindTestFunc(XtPointer item,XtPointer client_data)
{
	XfePrefPanelInfo	info = (XfePrefPanelInfo) item;
	String				target_panel_name = (String) client_data;
	String				test_panel_name;

	test_panel_name = XfePrefPanelInfoGetName(info);

	assert( test_panel_name != NULL );

	return (strcmp(target_panel_name,test_panel_name) == 0);
}
/*----------------------------------------------------------------------*/
/* extern */ XfePrefPanelInfo
XfePaneViewStringtoPanelInfo(Widget w,String panel_name)
{
    XfePrefViewPart *	pp = _XfePrefViewPart(w);
	
	assert( _XfeIsAlive(w) );
	assert( XfeIsPrefView(w) );
	assert( panel_name != NULL );

	return (XfePrefPanelInfo) XfeLinkedFind(pp->panel_info_list,
											FindTestFunc,
											(XtPointer) panel_name);
}
/*----------------------------------------------------------------------*/

static void
add_row(Widget tree,
		int row,
		Boolean expands,
		String str,
		int pos)
{
	XmString xmstr = XmStringCreateSimple(str);

	XmLTreeAddRow(tree,
				  row,
				  expands,
				  True,
				  pos,
				  XmUNSPECIFIED_PIXMAP,
				  XmUNSPECIFIED_PIXMAP,
				  xmstr);

	XmStringFree(xmstr);
}

static void 
whack_tree(Widget tree)
{
	XtVaSetValues(tree,
		XmNlayoutFrozen, True,
		NULL);

	add_row(tree, 1, True , "Appearnace" , XmLAST_POSITION);

 	add_row(tree, 2, False, "Fonts" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Colors" , XmLAST_POSITION);

	add_row(tree, 1, True , "Navigator" , XmLAST_POSITION);

 	add_row(tree, 2, False, "Languages" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Applications" , XmLAST_POSITION);

	add_row(tree, 1, True , "Mail & Groups" , XmLAST_POSITION);

 	add_row(tree, 2, False, "Identity" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Messages" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Mail Server" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Groups Server" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Directory" , XmLAST_POSITION);

	add_row(tree, 1, True , "Composer" , XmLAST_POSITION);

 	add_row(tree, 2, False, "New Page Colors" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Publish" , XmLAST_POSITION);

 	add_row(tree, 3, False, "Row Three" , 3);
 	add_row(tree, 3, False, "Row Three" , 3);

 	add_row(tree, 4, False, "Row Three" , 3);

	add_row(tree, 1, True , "Advanced" , XmLAST_POSITION);

 	add_row(tree, 2, False, "Cache" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Proxies" , XmLAST_POSITION);
 	add_row(tree, 2, False, "Disk Space" , XmLAST_POSITION);

	XtVaSetValues(tree,
		XmNlayoutFrozen, False,
		NULL);
}
