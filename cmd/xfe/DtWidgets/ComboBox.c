/*
 * ComboBox.c, Interleaf, 16aug93 2:37pm Version 1.1.
 */

/***********************************************************
Copyright 1993 Interleaf, Inc.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose without fee is granted,
provided that the above copyright notice appear in all copies
and that both copyright notice and this permission notice appear
in supporting documentation, and that the name of Interleaf not
be used in advertising or publicly pertaining to distribution of
the software without specific written prior permission.

Interleaf makes no representation about the suitability of this
software for any purpose. It is provided "AS IS" without any
express or implied warranty. 
******************************************************************/

/*
 * (C) Copyright 1991,1992, 1993
 * Interleaf, Inc.
 * 9 Hillside Avenue, 
 * Waltham, MA  02154
 *
 * ComboBox.c (DtComboBoxWidget):
 *
 * I wanted a margin around the widget (outside the shadow, like buttons), 
 * so that the combo-box could be made the same size as a 
 * push-button, etc.  The bulletin-board widget always puts the shadow at 
 * the outside edge of the widget, so combo-box is a sublcass of
 * manager, and we do everything ourselves.
 * 
 * One must be carefull when using Dimension (for core width and height).
 * Dimension is an unsigned short.  This causes problems when subtracting
 * and ending up with what should be a negative number (but it doesn't).
 * All child widget positioning is done by the combo_box.  We don't
 * use any heavy-weight forms, etc. to help us out.
 *
 * There is no padding when editable.  If using a label given it a
 * small margin, so it doesn't run up against the side of our
 * shadow or the arrow.
 * 
 * Make some of the ComboBox functions common, so they can be shared
 * with SpinButton.
 *
 * The label-string resource got out of control.  Its role kept getting
 * expanded; now the whole thing is a mess.  Currently it shadows the
 * label's label-string.  If the user sets it explicitly it will 
 * take affect for as long as update-label is false.  If update-label
 * is true, it will take affect until the end-user makes a selection
 * off the list.
 * 
 * Known bugs:
 *	    Changing margin_width or margin_height resources when the
 *	    combo_box has focus will probably result in display glitches.
 *
 */
#include "ComboBoxP.h"
#include <Xm/VendorSP.h>
#include <Xm/DrawP.h>
#include <Xm/MenuUtilP.h>

#include <Xm/DisplayP.h>
#define DisplayUserGrabbed(w) \
((XmDisplayRec*) XmGetXmDisplay(XtDisplay(w)))->display.userGrabbed

static void	ClassInitialize (void);
static void	Initialize (DtComboBoxWidget request, 
			       DtComboBoxWidget new, ArgList given_args, 
			       Cardinal *num_args);
static XmNavigability WidgetNavigable (DtComboBoxWidget combo);
static void	_ComboBoxFocusIn (DtComboBoxWidget combo, XEvent *event, 
				     char **params, Cardinal *num_params);
static void	_ComboBoxFocusOut (DtComboBoxWidget combo, XEvent *event,
				      char **params, Cardinal *num_params);
static void	DrawHighlight (DtComboBoxWidget combo, Boolean clear);
static void	_ComboBoxActivate (Widget w, XEvent *event, char **params,
				      Cardinal *num_params);
static void	_ComboBoxKbdCancel (Widget w, XEvent *event, char **params,
				       Cardinal *num_params);
static void	CheckResources (DtComboBoxWidget combo);
static void	Destroy (DtComboBoxWidget combo);
static void	Resize (DtComboBoxWidget combo);
static void	Redisplay (DtComboBoxWidget w, XEvent *event, 
			      Region region);
static XtGeometryResult GeometryManager (Widget w, 
					    XtWidgetGeometry *request, 
					    XtWidgetGeometry *reply);
static void	SetComboBoxSize (DtComboBoxWidget combo);
static void	ForceChildSizes (DtComboBoxWidget combo);
static void	LayoutChildren (DtComboBoxWidget combo);
static Boolean	SetValues (DtComboBoxWidget current, 
			      DtComboBoxWidget request, DtComboBoxWidget new);
static void	ClearShadow (DtComboBoxWidget w, Boolean all);
static void	DrawShadow (DtComboBoxWidget w);
static char*	GetTextString (XmString xm_string);
static void	SetTextFieldData (DtComboBoxPart *combo_p, XmString item);
static void	SetMaximumLabelSize (DtComboBoxPart *combo_p);
static void	SetLabelData (DtComboBoxPart *combo_p, XmString item,
				 Boolean force_label_string);
static void	select_cb (Widget w, XtPointer client_data, 
			      XtPointer call_data);
static void	shell_event_handler (Widget widget, XtPointer client_data,
					XEvent* event, Boolean *dispatch);
static void	list_event_handler (Widget widget, XtPointer client_data,
				       XEvent* event, Boolean *dispatch);
static void	TextFieldActivate (DtComboBoxPart *combo_p);
static void	activate_cb (Widget w, XtPointer client_data,
				XtPointer call_data);
static void	arrow_expose_cb (Widget w, XtPointer client_data,
				    XtPointer call_data);
static void	text_losing_focus_cb (Widget w, XtPointer client_data,
					 XtPointer call_data);
static void	text_activate_cb (Widget w, XtPointer client_data,
				     XtPointer call_data);
static void	text_focus_cb (Widget w, XtPointer client_data,
				  XtPointer call_data);
static void	SyncWithList (DtComboBoxPart *combo_p);
static XmImportOperator _XmSetSyntheticResForChild (Widget widget,
						       int offset, 
						       XtArgVal * value);

static XmString InitLabel = NULL;
extern void DtComboBoxSelectItemMoveup(DtComboBoxWidget combo, XmString item);

/*
 * DtComboBoxWidget specific defines.
 */
#define COMBO_SHADOW(w)	    w->manager.shadow_thickness
#define COMBO_MARGIN_W(w)   w->combo_box.margin_width
#define COMBO_MARGIN_H(w)   w->combo_box.margin_height
#define COMBO_H_SPACING(w)  w->combo_box.horizontal_spacing
#define COMBO_V_SPACING(w)  w->combo_box.vertical_spacing
#define LIST_EVENTS	    (ButtonPressMask | ButtonReleaseMask | \
			     FocusChangeMask | EnterWindowMask | \
			     LeaveWindowMask)
#define SHELL_EVENTS	    ButtonPressMask
#define INVALID_DIMENSION   (0xFFFF)
#define MAXINT 2147483647  /* Taken from TextF.c */


static char ComboBoxTranslationTable[] = "\
	<FocusIn>:	     ComboBoxFocusIn() \n\
	<FocusOut>:	     ComboBoxFocusOut() \n\
        <Key>osfDown:        ComboBoxActivate() \n\
        <Btn1Down>,<Btn1Up>: ComboBoxActivate() \n\
        <Key>osfSelect:      ComboBoxActivate() \n\
        ~s ~m ~a <Key>space: ComboBoxActivate() \n\
";

static char ComboBoxLabelTranslationTable[] = "\
        <Key>osfDown:        ComboBoxActivate(label) \n\
        <Btn1Down>,<Btn1Up>: ComboBoxActivate(label) \n\
        <Key>osfSelect:      ComboBoxActivate(label) \n\
        ~s ~m ~a <Key>space: ComboBoxActivate(label) \n\
";

static char ComboBoxListTranslationTable[] = "\
        <Key>osfCancel:      ListKbdCancel() ComboBoxKbdCancel() \n\
";

static XtActionsRec ComboBoxActionTable[] = {
       {"ComboBoxFocusIn",   (XtActionProc)_ComboBoxFocusIn},
       {"ComboBoxFocusOut",  (XtActionProc)_ComboBoxFocusOut},
       {"ComboBoxActivate",  (XtActionProc)_ComboBoxActivate},
       {"ComboBoxKbdCancel", (XtActionProc)_ComboBoxKbdCancel},
};


/* 
 * DtComboBoxWidget resources 
 */
#define offset(field) XtOffset(DtComboBoxWidget, field)
static XtResource resources[] = {
    {XmNshadowThickness, XmCShadowThickness, XmRHorizontalDimension, 
	 sizeof(Dimension), offset(manager.shadow_thickness),
	 XmRImmediate, (XtPointer)TEXT_FIELD_SHADOW},

    /* 
     * ComboBox specific resources
     */
    {XmNactivateCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList),
	 offset(combo_box.activate_callback), XmRCallback, 
	 (XtPointer)NULL},
    {XmNalignment, XmCAlignment, XmRAlignment, sizeof(unsigned char),
	 offset(combo_box.alignment), XmRImmediate, 
	 (XtPointer)XmALIGNMENT_BEGINNING},
    {XmNarrowSpacing, XmCArrowSpacing, XmRHorizontalDimension,
	 sizeof(Dimension), offset(combo_box.arrow_spacing),
	 XmRImmediate, (XtPointer)0},
    {XmNarrowType, XmCArrowType, XmRArrowType, sizeof(unsigned char),
	 offset(combo_box.arrow_type), XmRImmediate, (XtPointer)XmWINDOWS},
    {XmNcolumns, XmCColumns, XmRShort, sizeof(short),
	 offset(combo_box.text_columns), XmRImmediate, (XtPointer)20},
    {XmNfocusCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList),
	 offset(combo_box.focus_callback), XmRCallback, 
	 (XtPointer)NULL},
    {XmNhorizontalSpacing, XmCHorizontalSpacing, XmRHorizontalDimension,
	 sizeof(Dimension), offset(combo_box.horizontal_spacing),
	 XmRImmediate, (XtPointer)INVALID_DIMENSION},
    {XmNitemCount, XmCItemCount, XmRInt, sizeof(int), 
	 offset(combo_box.item_count), XmRImmediate, (XtPointer)0},
    /*
     * items is used only for seeing if the user changed the list.  It
     * is only a pointer that reflects the current list's items.
     */
    {XmNitems, XmCItems, XmRXmStringTable, sizeof(XmStringTable),
	 offset(combo_box.items), XmRImmediate, (XtPointer)NULL},
    {XmNlabelString, XmCXmString, XmRXmString, sizeof(XmString),
	 offset(combo_box.label_string), XmRImmediate, (XtPointer)NULL},
    {XmNlist, XmCList, XmRWidget, sizeof(Widget),
	 offset(combo_box.list), XmRImmediate, (XtPointer)NULL},
    {XmNlistFontList, XmCListFontList, XmRFontList, sizeof(XmFontList), 
	 offset(combo_box.list_font_list), XmRImmediate, (XtPointer)NULL},
    {XmNlistMarginHeight, XmCListMarginHeight, XmRVerticalDimension, 
	 sizeof(Dimension), offset(combo_box.list_margin_height),
	 XmRImmediate, (XtPointer)MARGIN},
    {XmNlistMarginWidth, XmCListMarginWidth, XmRHorizontalDimension,
	 sizeof(Dimension), offset(combo_box.list_margin_width),
	 XmRImmediate, (XtPointer)MARGIN},
    {XmNlistSpacing, XmCListSpacing, XmRDimension,sizeof(Dimension), 
	 offset(combo_box.list_spacing), XmRImmediate, (XtPointer)0},
    {XmNlosingFocusCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList),
	 offset(combo_box.losing_focus_callback), XmRCallback, 
	 (XtPointer)NULL},
    {XmNmarginHeight, XmCMarginHeight, XmRVerticalDimension,
	 sizeof(Dimension), offset(combo_box.margin_height),
	 XmRImmediate, (XtPointer)MARGIN},
    {XmNmarginWidth, XmCMarginWidth, XmRHorizontalDimension, sizeof(Dimension),
	 offset(combo_box.margin_width), XmRImmediate, (XtPointer)MARGIN},
    {XmNmaxLength, XmCMaxLength, XmRInt, sizeof(unsigned int),
	 offset(combo_box.text_max_length), XmRImmediate, (XtPointer)MAXINT},
    {XmNmenuPostCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList),
	 offset(combo_box.menu_post_callback), XmRCallback, (XtPointer)NULL},
    {XmNorientation, XmCOrientation, XmROrientation, sizeof(unsigned char),
	 offset(combo_box.orientation), XmRImmediate, (XtPointer)XmRIGHT},
    {XmNpoppedUp, XmCPoppedUp, XmRBoolean, sizeof(Boolean),
	 offset(combo_box.popped_up), XmRImmediate, (XtPointer)FALSE},
    {XmNrecomputeSize, XmCRecomputeSize, XmRBoolean, sizeof(Boolean),
	 offset(combo_box.recompute_size), XmRImmediate, (XtPointer)TRUE},
    {XmNselectedItem, XmCXmString, XmRXmString, sizeof(XmString),
	 offset(combo_box.selected_item), XmRImmediate, (XtPointer)NULL},
    {XmNselectedPosition, XmCSelectedPosition, XmRInt, sizeof(unsigned int),
	 offset(combo_box.selected_position), XmRImmediate, (XtPointer)0},
    {XmNselectionCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList),
	 offset(combo_box.selection_callback), XmRCallback, (XtPointer)NULL},
    {XmNtextField, XmCTextField, XmRWidget, sizeof(Widget),
	 offset(combo_box.text), XmRImmediate, (XtPointer)NULL},
    {XmNtopItemPosition, XmCTopItemPosition, XmRInt, sizeof(int), 
	 offset(combo_box.top_item_position), XmRImmediate, (XtPointer)1},
    {XmNtype, XmCType, XmRType, sizeof(unsigned char),
	 offset(combo_box.type), XmRImmediate,
	 (XtPointer)XmDROP_DOWN_LIST_BOX},
    {XmNupdateLabel, XmCUpdateLabel, XmRBoolean, sizeof(Boolean),
	 offset(combo_box.update_label), XmRImmediate, (XtPointer)TRUE},
    {XmNvisibleItemCount, XmCVisibleItemCount, XmRInt, sizeof(int),
	 offset(combo_box.visible_item_count), XmRImmediate, (XtPointer)10},
    {XmNverticalSpacing, XmCVerticalSpacing, XmRVerticalDimension,
	 sizeof(Dimension), offset(combo_box.vertical_spacing),
	 XmRImmediate, (XtPointer)INVALID_DIMENSION},
    {XmNmoveSelectedItemUp, XmCMoveSelectedItemUp, XmRBoolean, sizeof(Boolean),
         offset(combo_box.move_selecteditem_up), XmRImmediate, (XtPointer)FALSE},
    {XmNnoCallbackForArrow, XmCNoCallbackForArrow, XmRBoolean, sizeof(Boolean),
	 offset(combo_box.no_callback_for_arrow), XmRImmediate, (XtPointer)FALSE}
};

/*
 * List resources (used for GetValues).
 */
static XmSyntheticResource syn_resources[] = {
    {XmNarrowSize, sizeof(Dimension), offset(combo_box.arrow_size), 
	 _DtComboBoxGetArrowSize, _XmSetSyntheticResForChild},
    {XmNlabelString, sizeof(XmString), offset(combo_box.label_string), 
	 _DtComboBoxGetLabelString, _XmSetSyntheticResForChild},
    {XmNitemCount, sizeof(int), offset(combo_box.item_count), 
	 _DtComboBoxGetListItemCount, _XmSetSyntheticResForChild},
    {XmNitems, sizeof(XmStringTable), offset(combo_box.items), 
	 _DtComboBoxGetListItems, _XmSetSyntheticResForChild},
    {XmNlistFontList, sizeof(XmFontList), offset(combo_box.list_font_list), 
	 _DtComboBoxGetListFontList, _XmSetSyntheticResForChild},
    {XmNlistMarginHeight, sizeof(Dimension), 
	 offset(combo_box.list_margin_height),
	 _DtComboBoxGetListMarginHeight, _XmSetSyntheticResForChild},
    {XmNlistMarginWidth, sizeof(Dimension),offset(combo_box.list_margin_width),
	 _DtComboBoxGetListMarginWidth, _XmSetSyntheticResForChild},
    {XmNlistSpacing, sizeof(Dimension), offset(combo_box.list_spacing),
	 _DtComboBoxGetListSpacing, _XmSetSyntheticResForChild},
    {XmNtopItemPosition, sizeof(int), offset(combo_box.top_item_position),
	 _DtComboBoxGetListTopItemPosition, _XmSetSyntheticResForChild},
    {XmNvisibleItemCount, sizeof(int), offset(combo_box.visible_item_count),
	 _DtComboBoxGetListVisibleItemCount, _XmSetSyntheticResForChild},
};
#undef offset

/* Need Class Extension for widget navigation */
static XmBaseClassExtRec baseClassExtRec = {
    NULL,
    NULLQUARK,
    XmBaseClassExtVersion,
    sizeof(XmBaseClassExtRec),
    (XtInitProc)NULL,			/* InitializePrehook	*/
    (XtSetValuesFunc)NULL,		/* SetValuesPrehook	*/
    (XtInitProc)NULL,			/* InitializePosthook	*/
    (XtSetValuesFunc)NULL,		/* SetValuesPosthook	*/
    NULL,				/* secondaryObjectClass	*/
    (XtInitProc)NULL,			/* secondaryCreate	*/
    (XmGetSecResDataFunc)NULL, 		/* getSecRes data	*/
    { 0 },      			/* fastSubclass flags	*/
    (XtArgsProc)NULL,			/* getValuesPrehook	*/
    (XtArgsProc)NULL,			/* getValuesPosthook	*/
    (XtWidgetClassProc)NULL,            /* classPartInitPrehook */
    (XtWidgetClassProc)NULL,            /* classPartInitPosthook*/
    NULL,                               /* ext_resources        */
    NULL,                               /* compiled_ext_resources*/
    0,                                  /* num_ext_resources    */
    FALSE,                              /* use_sub_resources    */
    (XmWidgetNavigableProc) WidgetNavigable,                    /* widgetNavigable      */
    (XmFocusChangeProc)NULL,            /* focusChange          */
    (XmWrapperData)NULL			/* wrapperData 		*/
};

/*
 * Define Class Record.
 */
DtComboBoxClassRec dtComboBoxClassRec =
{
    {		/* core_class fields      */
    (WidgetClass)&(xmManagerClassRec),		/* superclass         */    
    (String)"DtComboBox",			/* class_name         */    
    (Cardinal)sizeof(DtComboBoxRec),		/* widget_size        */    
    (XtProc)ClassInitialize,			/* class_initialize   */    
    (XtWidgetClassProc)NULL,			/* class_part_init    */    
    (XtEnum)FALSE,				/* class_inited       */    
    (XtInitProc)Initialize,			/* initialize         */    
    (XtArgsProc)NULL,				/* initialize_hook    */    
    (XtRealizeProc)XtInheritRealize,		/* realize            */    
    (XtActionList)ComboBoxActionTable,		/* actions	       */    
    (Cardinal)XtNumber(ComboBoxActionTable),	/* num_actions        */    
    (XtResourceList)resources,			/* resources          */    
    (Cardinal)XtNumber(resources),		/* num_resources      */    
    (XrmClass)NULLQUARK,			/* xrm_class          */    
    (Boolean)TRUE,				/* compress_motion    */    
    (XtEnum)XtExposeCompressMaximal,		/* compress_exposure  */    
    (Boolean)TRUE,				/* compress_enterleave*/    
    (Boolean)FALSE,				/* visible_interest   */    
    (XtWidgetProc)Destroy,			/* destroy            */    
    (XtWidgetProc)Resize,			/* resize             */    
    (XtExposeProc)Redisplay,			/* expose             */    
    (XtSetValuesFunc)SetValues,			/* set_values         */    
    (XtArgsFunc)NULL,				/* set values hook    */    
    (XtAlmostProc)XtInheritSetValuesAlmost,	/* set values almost  */    
    (XtArgsProc)NULL,				/* get values hook    */    
    (XtAcceptFocusProc)NULL,			/* accept_focus       */    
    (XtVersionType)XtVersion,			/* Version            */    
    (XtPointer)NULL,				/* PRIVATE cb list    */
    (String)XtInheritTranslations,		/* tm_table           */
    (XtGeometryHandler)XtInheritQueryGeometry,	/* query_geom         */
    (XtStringProc)XtInheritDisplayAccelerator,	/* display_accelerator*/
    (XtPointer)&baseClassExtRec			/* extension	      */
    },
    {		/* composite_class fields */
    (XtGeometryHandler)GeometryManager,		/* geometry_manager   */     
    (XtWidgetProc)XtInheritChangeManaged,	/* change_managed     */     
    (XtWidgetProc)XtInheritInsertChild,		/* insert_child	      */     
    (XtWidgetProc)XtInheritDeleteChild,		/* delete_child	      */     
    (XtPointer)NULL				/* extension	      */     
    },
    {		/* constraint_class fields */
    (XtResourceList)NULL,			/* resources	      */     
    (Cardinal)0,				/* num_resources      */     
    (Cardinal)0,				/* constraint_size    */     
    (XtInitProc)NULL,				/* initialize	      */     
    (XtWidgetProc)NULL,				/* destroy	      */     
    (XtSetValuesFunc)NULL,			/* set_values	      */     
    (XtPointer)NULL				/* extension          */     
    },
    {		/* manager class     */
    (String)XtInheritTranslations,		/* translations       */     
    (XmSyntheticResource*)syn_resources,	/* syn resources      */     
    (int)XtNumber(syn_resources),		/* num syn_resources  */     
    (XmSyntheticResource*)NULL,			/* get_cont_resources */     
    (int)0,					/* num_get_cont_resources */ 
    (XmParentProcessProc)XmInheritParentProcess,/* parent_process     */     
    (XtPointer)NULL				/* extension          */     
    },
    {		/* combo_box_class fields */     
    (Boolean)0,
    }
};

WidgetClass dtComboBoxWidgetClass = (WidgetClass)&dtComboBoxClassRec;

/* 
 * Must set up the record type for the class extensions to work.
 */
static void
ClassInitialize(void)
{
    baseClassExtRec.record_type = XmQmotif;
}

/*
 * ComboBox initialization function.  This builds the widgets inside
 * our widget, to get the correct layout.  If the type resource
 * is XmDROP_DOWN_COMBO_BOX, we create a textField; if FALSE, we create a
 * label.  If the user changes this resource later, we will create the
 * other widget (textField or Label).  We don't want to carry backage from
 * both widgets if the user never changes the type resource.
 */
static void
Initialize(DtComboBoxWidget request,
	   DtComboBoxWidget new,
	   ArgList given_args,
	   Cardinal *num_args)
{
    int k;
    XtTranslations trans = XtParseTranslationTable(ComboBoxTranslationTable);
    XtTranslations list_trans = 
	XtParseTranslationTable(ComboBoxListTranslationTable);
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(new->combo_box);
    Boolean force_label_string = FALSE;
    Arg args[15];
    int n;

    /* Overwrite the manager's focusIn and focusOut translations */
    XtOverrideTranslations((Widget)new, trans);

    if (InitLabel == NULL)
	InitLabel = XmStringCreateSimple("");

    /* 
     * force_label_string usage if it is specified and items is not.
     * This will be the perminant label string only if update-label
     * is false, else it is only used until the user picks something
     * new off the list.
     */
    if (combo_p->label_string == NULL)
	combo_p->label_string = InitLabel;
    else if (!combo_p->items)
	force_label_string = TRUE;


    combo_p->text = (Widget)NULL;
    combo_p->label = (Widget)NULL;
    combo_p->sep = (Widget)NULL;
    combo_p->old_width = 0;
    combo_p->old_height = 0;

    CheckResources(new);

    /*
     * Create the text or label depending on the type resource.
     * When part of X-Designer, we create both at initialization to
     * avoid later crashes.
     */
#ifndef XDESIGNER
    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
#endif
    {
	n = 0;
	XtSetArg(args[n], XmNcolumns, combo_p->text_columns); n++;
	XtSetArg(args[n], XmNmaxLength, combo_p->text_max_length); n++;
	XtSetArg(args[n], XmNmarginWidth, 2); n++;
	XtSetArg(args[n], XmNmarginHeight, 2); n++;
	combo_p->text = XtCreateManagedWidget("Text", 
					      xmTextFieldWidgetClass,
					      (Widget)new, args, n);
	XtAddCallback(combo_p->text, XmNlosingFocusCallback, 
		      text_losing_focus_cb, (XtPointer)new);
	XtAddCallback(combo_p->text, XmNactivateCallback, 
		      text_activate_cb, (XtPointer)new);
	XtAddCallback(combo_p->text, XmNfocusCallback, 
		      text_focus_cb, (XtPointer)new);
	if (combo_p->horizontal_spacing == INVALID_DIMENSION)
	    combo_p->horizontal_spacing = 0;
	if (combo_p->vertical_spacing == INVALID_DIMENSION)
	    combo_p->vertical_spacing = 0;
    }
#ifndef XDESIGNER
    else
#endif
    {
	XtTranslations label_trans = 
	    XtParseTranslationTable(ComboBoxLabelTranslationTable);

	COMBO_SHADOW(new) = LABEL_SHADOW;
	n = 0;
	XtSetArg(args[n], XmNalignment, combo_p->alignment); n++;
	XtSetArg(args[n], XmNrecomputeSize, FALSE); n++;
	XtSetArg(args[n], XmNlabelString, InitLabel); n++;
	XtSetArg(args[n], XmNmarginLeft, LABEL_PADDING); n++;
	XtSetArg(args[n], XmNmarginRight, LABEL_PADDING); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNstringDirection,
		 new->manager.string_direction); n++;
	combo_p->label = XtCreateManagedWidget("Label", 
					       xmLabelWidgetClass,
					       (Widget)new, args, n);
	XtOverrideTranslations((Widget)combo_p->label, label_trans);
	if (combo_p->horizontal_spacing == INVALID_DIMENSION)
	    combo_p->horizontal_spacing = 1;
	if (combo_p->vertical_spacing == INVALID_DIMENSION)
	    combo_p->vertical_spacing = 2;
    }

#ifdef XDESIGNER
    /* Unmanage the one we don't use */
    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	XtUnmanageChild(combo_p->label);
    else
	XtUnmanageChild(combo_p->text);
#endif

    /*
     * Create the separator used if non-editable combo-box.
     */
#ifndef XDESIGNER
    if (combo_p->type == XmDROP_DOWN_LIST_BOX) 
#endif
    {
	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	combo_p->sep = XtCreateManagedWidget("ComboBoxSeparator", 
					     xmSeparatorWidgetClass,
					     (Widget)new, args, n);
    }

    /*
     * Create the ArrowWidget.
     */
    n = 0;
    XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
    XtSetArg(args[n], XmNhighlightThickness, 0); n++;
    XtSetArg(args[n], XmNshadowThickness, 0); n++;
    if (combo_p->arrow_type == XmMOTIF) {
	XtSetArg(args[n], XmNarrowDirection, XmARROW_DOWN); n++;
	XtSetArg(args[n], XmNforeground, new->core.background_pixel); n++;
	combo_p->arrow = XtCreateManagedWidget("ComboBoxArrow", 
					       xmArrowButtonWidgetClass,
					       (Widget)new, args, n);
    }
    else {
	combo_p->arrow = XtCreateManagedWidget("ComboBoxArrow", 
					       xmDrawnButtonWidgetClass,
					       (Widget)new, args, n);
	XtAddCallback(combo_p->arrow, XmNexposeCallback, arrow_expose_cb, 
		      (XtPointer)new);
    }
    XtAddCallback(combo_p->arrow, XmNactivateCallback, activate_cb, 
		  (XtPointer)new);

    /*
     *  Create the shell and associated list widgets.
     */
    n = 0;
    for ( k = 0; k < *num_args; k++ )
    {
             if ( (strcmp(given_args[k].name, XmNvisual) == 0 ) ||
                  (strcmp(given_args[k].name, XmNcolormap) == 0 ) ||
                  (strcmp(given_args[k].name, XmNdepth) == 0) )
             {
                    args[n].name  = given_args[k].name;
                    args[n].value = given_args[k].value;
                    n++;
              }
    }

    XtSetArg(args[n], XtNoverrideRedirect, TRUE); n++;
    XtSetArg(args[n], XtNallowShellResize, TRUE); n++;
    XtSetArg(args[n], XtNsaveUnder, TRUE); n++;
    combo_p->shell = XtCreatePopupShell("ComboBoxMenuShell", 
					topLevelShellWidgetClass,
					(Widget)new, args, n);
    
    n = 0;
    combo_p->frame = XtCreateManagedWidget("ComboBoxRowColumn",
					   xmFrameWidgetClass,
					   combo_p->shell, args, n);

    n = 0;
    /* Store combo widget in list for later use */
    XtSetArg(args[n], XmNuserData, (XtPointer)new); n++;
    if (combo_p->list_font_list) {
	XtSetArg(args[n], XmNfontList, combo_p->list_font_list); n++;
    }
    XtSetArg(args[n], XmNitemCount, combo_p->item_count); n++;
    XtSetArg(args[n], XmNitems, combo_p->items); n++;
    XtSetArg(args[n], XmNlistMarginHeight, combo_p->list_margin_height); n++;
    XtSetArg(args[n], XmNlistMarginWidth, combo_p->list_margin_width); n++;
    XtSetArg(args[n], XmNlistSpacing, combo_p->list_spacing); n++;
    XtSetArg(args[n], XmNstringDirection, new->manager.string_direction); n++;
    XtSetArg(args[n], XmNtopItemPosition, combo_p->top_item_position); n++;
    XtSetArg(args[n], XmNvisibleItemCount, combo_p->visible_item_count); n++;
    XtSetArg(args[n], XmNlistSizePolicy, XmRESIZE_IF_POSSIBLE); n++;
    combo_p->list = XmCreateScrolledList(combo_p->frame, "List",
					 args, n);
    XtOverrideTranslations((Widget)combo_p->list, list_trans);

    /* selected_item resource used before selected_position */
    if (combo_p->selected_item)
	DtComboBoxSelectItem((Widget)new, combo_p->selected_item);
    else
	XmListSelectPos(combo_p->list, combo_p->selected_position + 1, FALSE);

    SyncWithList(combo_p);
    XtManageChild(combo_p->list);
    XtRealizeWidget(combo_p->shell);

    combo_p->max_shell_width = combo_p->shell->core.width;
    combo_p->max_shell_height = combo_p->shell->core.height;

    XtAddCallback(combo_p->list, XmNdefaultActionCallback, select_cb, new);
    XtAddCallback(combo_p->list, XmNbrowseSelectionCallback, select_cb, new);

    /*
     * Set up event handlers needed for handling grab states.
     */
    XtInsertEventHandler(combo_p->list, LIST_EVENTS, TRUE,
			 (XtEventHandler)list_event_handler, 
			 (XtPointer)new, XtListHead);
    XtInsertEventHandler(combo_p->shell, SHELL_EVENTS, TRUE,
			 (XtEventHandler)shell_event_handler, 
			 (XtPointer)new, XtListHead);

    /*
     * Set initial value in text or label if items was specified
     * Copy given label-string.
     */
    if (combo_p->label_string)
	combo_p->label_string = XmStringCopy(combo_p->label_string);
    if (combo_p->type == XmDROP_DOWN_LIST_BOX) {
	SetMaximumLabelSize(combo_p);
	SetLabelData(combo_p, NULL, force_label_string);
    }
    else
	SetTextFieldData(combo_p, NULL);

    SetComboBoxSize(new);
    LayoutChildren(new);
}


/*
 * Allow the manager to gain focus if not editable.  If editable (using
 * text-field), then let the toolkit give focus to the text-field.
 */
static XmNavigability
WidgetNavigable(DtComboBoxWidget combo)
{   
    XmNavigationType nav_type = ((XmManagerWidget)combo)->manager.navigation_type;

    if (combo->core.sensitive &&  combo->core.ancestor_sensitive &&
	((XmManagerWidget)combo)->manager.traversal_on) {
	if ((nav_type == XmSTICKY_TAB_GROUP) ||
	    (nav_type == XmEXCLUSIVE_TAB_GROUP) ||
	    ((nav_type == XmTAB_GROUP) && 
	     !_XmShellIsExclusive((Widget)combo))) {
	    if (combo->combo_box.type == XmDROP_DOWN_COMBO_BOX)
		return(XmDESCENDANTS_TAB_NAVIGABLE);
	    else
		return(XmTAB_NAVIGABLE);
	}
	return(XmDESCENDANTS_NAVIGABLE);
    }
    return(XmNOT_NAVIGABLE);
}

/* 
 * The combo_box gets focus.
 */
static void 
_ComboBoxFocusIn(DtComboBoxWidget combo,
		 XEvent *event,
		 String *params,
		 Cardinal *num_params)
{
    DrawHighlight(combo, FALSE);
}

/* 
 * The combo_box loses focus. Only happens if not editable.
 */
static void 
_ComboBoxFocusOut(DtComboBoxWidget combo,
		  XEvent *event,
		  String *params,
		  Cardinal *num_params)
{
    DrawHighlight(combo, TRUE);
}

/*
 * This function gets called whenever we draw or clear the shadow (to
 * redraw highlight during resize, etc), as well as during focus_in
 * and focus_out events.
 */
static void
DrawHighlight(DtComboBoxWidget combo,
	      Boolean clear)
{
    XRectangle rect[4] ;

    if (XtIsRealized(combo)) {
	if (clear) {
	    rect[0].x = rect[1].x = rect[2].x = 0;
	    rect[3].x = combo->combo_box.old_width - COMBO_MARGIN_W(combo);
	    rect[0].y = rect[2].y = rect[3].y = 0 ;
	    rect[1].y = combo->combo_box.old_height - COMBO_MARGIN_H(combo);
	    rect[0].width = rect[1].width = combo->combo_box.old_width;
	    rect[2].width = rect[3].width = COMBO_MARGIN_W(combo);
	    rect[0].height = rect[1].height = COMBO_MARGIN_H(combo);
	    rect[2].height = rect[3].height = combo->combo_box.old_height;
	    XFillRectangles(XtDisplayOfObject((Widget)combo),
			    XtWindowOfObject((Widget)combo), 
			    combo->manager.background_GC, rect, 4);
	}
	else if (XmGetFocusWidget((Widget)combo) == (Widget)combo) {
	    rect[0].x = rect[1].x = rect[2].x = 0;
	    rect[3].x = XtWidth(combo) - COMBO_MARGIN_W(combo);
	    rect[0].y = rect[2].y = rect[3].y = 0 ;
	    rect[1].y = XtHeight(combo) - COMBO_MARGIN_H(combo);
	    rect[0].width = rect[1].width = XtWidth(combo);
	    rect[2].width = rect[3].width = COMBO_MARGIN_W(combo);
	    rect[0].height = rect[1].height = COMBO_MARGIN_H(combo);
	    rect[2].height = rect[3].height = XtHeight(combo);
	    XFillRectangles(XtDisplayOfObject((Widget)combo),
			    XtWindowOfObject((Widget)combo), 
			    combo->manager.highlight_GC, rect, 4);
	}
    }
}

/*
 * osfSelect virtual key hit.  Simulate hitting the arrow.
 */
static void 
_ComboBoxActivate(Widget w,
		  XEvent *event,
		  String *params,
		  Cardinal *num_params)
{
    DtComboBoxWidget combo;

    if (*num_params == 0) /* no params means combo */
	combo = (DtComboBoxWidget)w;
    else /* params means label */
	combo = (DtComboBoxWidget)XtParent(w);

    activate_cb((Widget)combo->combo_box.arrow, (XtPointer)combo, NULL);
}

/*
 * osfCancel virtual key hit.
 */
static void 
_ComboBoxKbdCancel(Widget w,
		   XEvent *event,
		   String *params,
		   Cardinal *num_params)
{
    DtComboBoxWidget combo;
    DtComboBoxPart *combo_p;
    XtPointer data;
    Arg args[1];

    /* Get combo-box off list data */
    XtSetArg(args[0], XmNuserData, &data);
    XtGetValues(w, args, 1);
    
    combo = (DtComboBoxWidget)data;
    combo_p = (DtComboBoxPart*)&(combo->combo_box);

    combo_p->popped_up = FALSE;
    XtPopdown(combo_p->shell);
    XtUngrabPointer(combo_p->shell, CurrentTime);
    XtUngrabKeyboard(combo_p->list, CurrentTime);
	/*
	 * _XmRemoveGrab() was causing trouble on irix and linux.  The navigator
	 * stopped responding to clicks cause of it.
	 *
	 */
#if 0
    _XmRemoveGrab(combo_p->shell);
#else
    XtRemoveGrab(combo_p->shell);
	
	DisplayUserGrabbed(w) = False;
#endif
    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	XmProcessTraversal(combo_p->text, XmTRAVERSE_CURRENT);
}

/*
 * This function goes through most of the resources and makes sure 
 * they have legal values.
 */
static void
CheckResources(DtComboBoxWidget combo)
{
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);

    if ((combo_p->alignment != XmALIGNMENT_CENTER) && 
	(combo_p->alignment != XmALIGNMENT_BEGINNING) &&
	(combo_p->alignment != XmALIGNMENT_END)) {
	XtWarning(COMBO_ALIGNMENT);
	combo_p->alignment = XmALIGNMENT_CENTER;
    }
    if (combo_p->margin_height < 0.0) {
	XtWarning(COMBO_MARGIN_HEIGHT);
	combo_p->margin_height = 2;
    }
    if (combo_p->margin_width < 0.0) {
	XtWarning(COMBO_MARGIN_WIDTH);
	combo_p->margin_width = 2;
    }
    if (combo_p->horizontal_spacing < 0.0) {
	XtWarning(COMBO_HORIZONTAL_SPACING);
	combo_p->horizontal_spacing = 0;
    }
    if (combo_p->vertical_spacing < 0.0) {
	XtWarning(COMBO_VERTICAL_SPACING);
	combo_p->vertical_spacing = 0;
    }
    if ((combo_p->orientation != XmLEFT) &&
	(combo_p->orientation != XmRIGHT)) {
	XtWarning(COMBO_ORIENTATION);
	combo_p->orientation = XmRIGHT;
    }
    if (combo_p->item_count < 0) {
	XtWarning(COMBO_ITEM_COUNT);
	combo_p->item_count = 0;
    }
    if ((combo_p->selected_position < 0) ||
	((combo_p->selected_position >= combo_p->item_count) && 
	 (combo_p->item_count > 0))) {
	XtWarning(COMBO_VISIBLE_ITEM);
	combo_p->selected_position = 0;
    }
}

/*
 * Destroy procedure called by the toolkit.  Remove all callbacks and
 * event-handlers. 
 */
static void 
Destroy(DtComboBoxWidget combo)
{
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);

    if (combo_p->label_string)
	XmStringFree(combo_p->label_string);

	DisplayUserGrabbed(combo) = False;

#if whydotheydothis
    if (combo_p->text) {
	XtRemoveCallback(combo_p->text, XmNlosingFocusCallback, 
			 text_losing_focus_cb, (XtPointer)combo);
	XtRemoveCallback(combo_p->text, XmNactivateCallback, 
			 text_activate_cb, (XtPointer)combo);
	XtRemoveCallback(combo_p->text, XmNfocusCallback, 
			 text_focus_cb, (XtPointer)combo);
    }

#if i_hope_it_has_been_ungrabbed_cuz_this_memory_has_already_been_freed
    if (((ShellWidget)(combo_p->shell))->shell.popped_up) {
	combo_p->popped_up = FALSE;
	XtPopdown(combo_p->shell);
	XtUngrabPointer(combo_p->shell, CurrentTime);
	XtUngrabKeyboard(combo_p->list, CurrentTime);
	_XmRemoveGrab(combo_p->shell);
    }
#endif /* i_hope_it_has_been_ungrabbed_cuz_this_memory_has_already_been_freed */

    XtRemoveCallback(combo_p->arrow, XmNactivateCallback, activate_cb, 
		     (XtPointer)combo);
    if (combo_p->arrow_type == XmWINDOWS)
	XtRemoveCallback(combo_p->arrow, XmNexposeCallback,
			 arrow_expose_cb, (XtPointer)combo);
    XtRemoveCallback(combo_p->list, XmNdefaultActionCallback, 
		     select_cb, combo);
    XtRemoveCallback(combo_p->list, XmNbrowseSelectionCallback, 
		     select_cb, combo);

    XtRemoveEventHandler(combo_p->list, LIST_EVENTS, TRUE,
			 (XtEventHandler)list_event_handler, 
			 (XtPointer)combo);

    XtRemoveEventHandler(combo_p->shell, SHELL_EVENTS, TRUE,
			 (XtEventHandler)shell_event_handler, 
			 (XtPointer)combo);
#endif /* whydotheydothis */
}


/*
 * Resize function called by toolkit.  The size of our combo-box
 * has already been changed.  That is why we must store 
 * old_width and old_height.
 */
static void
Resize(DtComboBoxWidget combo)
{
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);

    ClearShadow(combo, TRUE);
    LayoutChildren(combo);
    DrawShadow(combo);
    combo_p->old_width = combo->core.width;
    combo_p->old_height = combo->core.height;
}


/*
 * Redisplay function called by toolkit. The widget didn't change size, 
 * so just redisplay the shadow.
 */
static void
Redisplay(DtComboBoxWidget w,
	  XEvent *event,
	  Region region)
{
  /* Wow..  These guys _released_ this shit. */
#if 0
    DrawShadow(w, COMBO_MARGIN_W(w), COMBO_MARGIN_H(w), COMBO_SHADOW(w));
#else
    DrawShadow(w);
#endif
}


/*
 * GeometryManager function called by toolkit when a child resizes/moves.
 * We are not allowing any changes but width/height of the text-field.
 * this is because the user can retrieve the text-field and make changes
 * that we want to honor.  If they mess around with the label or arrow,
 * then we won't honor the request.
 * If the text-field requests a change, then make the change, and allow
 * our SetComboBoxSize() and LayoutChildren() figure out what size will
 * be allowed.  
 * Returning GeometryDone was suppose to tell the toolkit
 * that we resized the child ourselves, but the text-field had trouble
 * with this (its' geometry_manager wasn't called or working right?), so
 * we return GeometryYes.
 */
static XtGeometryResult
GeometryManager(Widget w,
		XtWidgetGeometry *request,
		XtWidgetGeometry *reply)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)(w->core.parent);

    /* Ignore everything but text-field */
    if (w != combo->combo_box.text)
	return(XtGeometryNo);

    /* Only allow width/height changes */
    if (!(request->request_mode & (CWWidth | CWHeight)))
	return(XtGeometryNo);
    
    /* Set the text-field to the requested size */
    if (request->request_mode & CWWidth)
	w->core.width = request->width;
    if (request->request_mode & CWHeight)
	w->core.height = request->height;
    XtResizeWidget(w, w->core.width, w->core.height, w->core.border_width);
    
    ClearShadow(combo, TRUE);
    if (combo->combo_box.recompute_size)
	SetComboBoxSize(combo);
    LayoutChildren(combo);
    DrawShadow(combo);
    return(XtGeometryYes);
}

/* 
 * This function sets the size of the combo_box widget based on the
 * current size of the children.  Don't worry if it doesn't work, the
 * children will be squeezed in later.
 */
static void
SetComboBoxSize(DtComboBoxWidget combo)
{
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    Widget text_holder = ((combo_p->type == XmDROP_DOWN_COMBO_BOX) ? 
			  combo_p->text : combo_p->label);
    Dimension shadow = COMBO_SHADOW(combo) * 2;
    Dimension h_spacing = COMBO_H_SPACING(combo) * 2;
    Dimension v_spacing = COMBO_V_SPACING(combo) * 2;
    short arrow_width;
    short sep_width = 0;

    /* 
     * Find out how big the arrow can be (needed to get 
     * available_width for text_holder).
     */
    arrow_width = (Dimension)(text_holder->core.height * ARROW_MULT);
    arrow_width = (arrow_width < ARROW_MIN) ? ARROW_MIN : arrow_width;

    if (combo_p->type == XmDROP_DOWN_LIST_BOX)
	sep_width = combo_p->sep->core.width;

    (void)XtMakeResizeRequest((Widget)combo, arrow_width + sep_width +
			      combo_p->arrow_spacing +
			      text_holder->core.width + shadow + h_spacing +
			      (COMBO_MARGIN_W(combo) * 2), 
			      text_holder->core.height + shadow + v_spacing +
			      (COMBO_MARGIN_H(combo) * 2), 
			      NULL, NULL);
    combo_p->old_width = combo->core.width;
    combo_p->old_height = combo->core.height;
}

/*
 * This function makes the text_holder (label or text-field) smaller
 * if the combo_box couldn't grow to the needed full size.  It will
 * also make the text_holder grow if there is space.  The textfield will
 * grow with the combo_box, but the label will only grow to its' 
 * maximum size.  The label will also shrink down to nothing, but the
 * text-field will always keep its' core height.
 */
static void
ForceChildSizes(DtComboBoxWidget combo)
{
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    Dimension full_available_height, available_height, available_width;
    Dimension arrow_width;
    Dimension sep_width = 0;

    /* Calculate available height for children */
    if ((available_height = combo->core.height - (COMBO_SHADOW(combo) * 2) - 
	 (COMBO_MARGIN_H(combo) * 2) - (COMBO_V_SPACING(combo) * 2)) <= 0) {
	full_available_height = available_height = 1;
    }
    else {
	/* Seperator need available_height plus the vertical_spacing */
	full_available_height = (available_height + 
				 (COMBO_V_SPACING(combo) * 2));
    }

    /* Get initial available width for children */
    available_width = (combo->core.width - (COMBO_SHADOW(combo) * 2) - 
		       (COMBO_MARGIN_W(combo) * 2) - 
		       (COMBO_H_SPACING(combo) * 2));
    
    /* label only grows to maximum width needed */
    if ((combo_p->type == XmDROP_DOWN_LIST_BOX) && 
	((int)available_height > (int)combo_p->label_max_height))
	available_height = combo_p->label_max_height;
    else if (combo_p->type == XmDROP_DOWN_COMBO_BOX) 
	available_height = combo_p->text->core.height;
    
    /* 
     * Find out how big the arrow can be (needed to get 
     * available_width for text_holder).
     */
    arrow_width = (Dimension)(available_height * ARROW_MULT);
    arrow_width = (arrow_width < ARROW_MIN) ? ARROW_MIN : arrow_width;
	
    if (combo_p->type == XmDROP_DOWN_LIST_BOX)
	sep_width = combo_p->sep->core.width;

    /* Make sure width isn't too small or too big */
    if ((int)(available_width -= 
	      (arrow_width + sep_width + combo_p->arrow_spacing)) <= 0)
	available_width = 1;

    if (combo_p->type == XmDROP_DOWN_LIST_BOX) {  /** label **/
	if ((int)available_width > (int)combo_p->label_max_length)
	    available_width = combo_p->label_max_length;

	if ((available_width != combo_p->label->core.width) ||
	    (available_height != combo_p->label->core.height))
	    XtResizeWidget(combo_p->label, available_width, available_height,
			   combo_p->label->core.border_width);

	if (full_available_height != combo_p->sep->core.height)
	    XtResizeWidget(combo_p->sep, combo_p->sep->core.width,
			   full_available_height,
			   combo_p->sep->core.border_width);
    }
    else if (combo_p->text->core.width != available_width)  /** TextField **/
	XtResizeWidget(combo_p->text, available_width,
		       combo_p->text->core.height,
		       combo_p->text->core.border_width);
    if ((arrow_width != combo_p->arrow->core.width) ||
	(combo_p->arrow->core.height != available_height)) {
	available_height = (available_height < ARROW_MIN) ? ARROW_MIN : 
                            available_height;
	XtResizeWidget(combo_p->arrow, arrow_width, available_height,
		       combo_p->arrow->core.border_width);
    }
}

/*
 * This function positions the children within the combo_box widget.
 * It calls ForceChildSizes() to make sure the children fit within the
 * combo_box widget, but it will not try to resize the combo_box widget.
 */
static void
LayoutChildren(DtComboBoxWidget combo)
{
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    Widget text_holder = ((combo_p->type == XmDROP_DOWN_COMBO_BOX)
			  ? combo_p->text : combo_p->label);
    Position start_x = (COMBO_SHADOW(combo) + COMBO_MARGIN_W(combo) +
			COMBO_H_SPACING(combo));
    Position start_y = (COMBO_SHADOW(combo) + COMBO_MARGIN_H(combo) +
			COMBO_V_SPACING(combo));
    short available_height = combo->core.height - (start_y * 2);
    Position y, arrow_y;
    
    ForceChildSizes(combo);

    /* Center text_holder within combo_box */
    y = available_height - text_holder->core.height;
    y = ((y < 0) ? 0 : y)/2 + start_y;
 
    /* Center arrow within combo_box */
    arrow_y = available_height - combo_p->arrow->core.height;
    arrow_y = ((arrow_y < 0) ? 0 : arrow_y)/2 + start_y;

    if (combo_p->orientation == XmLEFT) {
	XtMoveWidget(combo_p->arrow, start_x, arrow_y);
	start_x += combo_p->arrow->core.width;
	if (combo_p->type == XmDROP_DOWN_LIST_BOX) {
	    XtMoveWidget(combo_p->sep, start_x, start_y - 
			 COMBO_V_SPACING(combo));
	    start_x += combo_p->sep->core.width;
	}
	start_x += combo_p->arrow_spacing;
	XtMoveWidget(text_holder, start_x, y);
    }
    else {
	XtMoveWidget(text_holder, start_x, y);
	/*  
	 * We want the arrow at the end of the combo_box, so
	 * the user can use recompute_size more effectively.
	 */
	start_x = combo->core.width - start_x - combo_p->arrow->core.width;
	if (combo_p->type == XmDROP_DOWN_LIST_BOX) {
	    start_x -= combo_p->sep->core.width;
	    XtMoveWidget(combo_p->sep, start_x, start_y -
			 COMBO_V_SPACING(combo));
	    start_x += combo_p->sep->core.width;
	}
	XtMoveWidget(combo_p->arrow, start_x, arrow_y);
    }
}

/*
 * SetValues() routine for ComboBox widget. 
 */
static Boolean
SetValues(DtComboBoxWidget current,
	  DtComboBoxWidget request,
	  DtComboBoxWidget new)
{
    DtComboBoxPart *new_p = (DtComboBoxPart*)&(new->combo_box);
    DtComboBoxPart *cur_p = (DtComboBoxPart*)&(current->combo_box);
    Boolean label_size_changed = FALSE;
    Boolean force_label_string = FALSE;
    Arg args[10];
    int n;

    CheckResources(new);

    if (new_p->text != cur_p->text) {
	XtWarning(COMBO_TEXT);
	new_p->text = cur_p->text;
    }

    /*
     * Pass any list specific resources on to our List Widget.
     * Check each one, since it's too costly to always set them.
     */
    n = 0;
    if (new_p->item_count != cur_p->item_count){
	if (new_p->items && (new_p->item_count < 0)) {
	    XtWarning(COMBO_ITEM_COUNT);
	    new_p->item_count = 0;
	}
	XtSetArg(args[n], XmNitemCount, new_p->item_count); n++;
    }
    if (new_p->items != cur_p->items) {
	XtSetArg(args[n], XmNitems, new_p->items); n++;
	/* Make sure itemCount will get sent to list */
	if (new_p->item_count == cur_p->item_count) {
	    XtSetArg(args[n], XmNitemCount, new_p->item_count); n++;
	}
    }
    if (new_p->list_font_list != cur_p->list_font_list) {
	XtSetArg(args[n], XmNfontList, new_p->list_font_list); n++;
    }
    if (new_p->list_margin_height != cur_p->list_margin_height) {
	XtSetArg(args[n], XmNlistMarginHeight, new_p->list_margin_height); n++; 
    }
    if (new_p->list_margin_width != cur_p->list_margin_width) {
	XtSetArg(args[n], XmNlistMarginWidth, new_p->list_margin_width); n++;
    }
    if (new_p->list_spacing != cur_p->list_spacing) {
	XtSetArg(args[n], XmNlistSpacing, new_p->list_spacing); n++;
    }
    if (new->manager.string_direction != current->manager.string_direction) {
	XtSetArg(args[n], XmNstringDirection, new->manager.string_direction); n++;
    }
    if (new_p->top_item_position != cur_p->top_item_position) {
	XtSetArg(args[n], XmNtopItemPosition, new_p->top_item_position); n++;
    }
    if (new_p->visible_item_count != cur_p->visible_item_count) {
	XtSetArg(args[n], XmNvisibleItemCount, new_p->visible_item_count); n++;
    }
    if (n > 0) {
	XtSetValues(new_p->list, args, n);
	new_p->max_shell_width = new_p->shell->core.width;
	new_p->max_shell_height = new_p->shell->core.height;
    }

    /* If arrow type changes delete the old one and create the new one */
    if (new_p->arrow_type != cur_p->arrow_type) {
	XtRemoveCallback(new_p->arrow, XmNactivateCallback, activate_cb, 
			 (XtPointer)new);
	if (cur_p->arrow_type == XmWINDOWS)
	    XtRemoveCallback(new_p->arrow, XmNexposeCallback,
			     arrow_expose_cb, (XtPointer)new);
	XtDestroyWidget(new_p->arrow);

	n = 0;
	XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
	XtSetArg(args[n], XmNhighlightThickness, 0); n++;
	XtSetArg(args[n], XmNshadowThickness, 0); n++;
	if (new_p->arrow_type == XmMOTIF) {
	    XtSetArg(args[n], XmNarrowDirection, XmARROW_DOWN); n++;
	    XtSetArg(args[n], XmNforeground, new->core.background_pixel); n++;
	    new_p->arrow = XtCreateManagedWidget("ComboBoxArrow", 
						 xmArrowButtonWidgetClass,
						 (Widget)new, args, n);
	}
	else {
	    new_p->arrow = XtCreateManagedWidget("ComboBoxArrow", 
						 xmDrawnButtonWidgetClass,
						 (Widget)new, args, n);
	    XtAddCallback(new_p->arrow, XmNexposeCallback, arrow_expose_cb, 
			  (XtPointer)new);
	}
	XtAddCallback(new_p->arrow, XmNactivateCallback, activate_cb, 
		      (XtPointer)new);
    }

    /*
     * Type resource changed.  If the widget (textField or Label)
     * doesn't exist, then create it.  Always reset orientation
     * constraint resources when type changes; otherwise, the
     * text_holder widget positioning could be screwed up.  We don't
     * reset both widgets if the orientation changes (because we might
     * not have created both widgets).
     * If label must be created, also create the separator widget.
     */
    if (new_p->type != cur_p->type) {
	if (new_p->type == XmDROP_DOWN_COMBO_BOX) {
	    if (new_p->text == NULL) {
		n = 0;
		XtSetArg(args[n], XmNcolumns, new_p->text_columns); n++;
		XtSetArg(args[n], XmNmaxLength, new_p->text_max_length); n++;
		XtSetArg(args[n], XmNmarginWidth, 2); n++;
		XtSetArg(args[n], XmNmarginHeight, 2); n++;
		new_p->text = XtCreateWidget("ComboBoxTextField", 
					     xmTextFieldWidgetClass,
					     (Widget)new, args, n);
		XtAddCallback(new_p->text, XmNlosingFocusCallback, 
			      text_losing_focus_cb, (XtPointer)new);
		XtAddCallback(new_p->text, XmNactivateCallback, 
			      text_activate_cb, (XtPointer)new);
		XtAddCallback(new_p->text, XmNfocusCallback, 
			      text_focus_cb, (XtPointer)new);
		if (new_p->horizontal_spacing == cur_p->horizontal_spacing)
		    new_p->horizontal_spacing = 0;
		if (new_p->vertical_spacing == cur_p->vertical_spacing)
		    new_p->vertical_spacing = 0;
	    }
	    XtUnmanageChild(new_p->sep);
	    XtUnmanageChild(new_p->label);
	    XtManageChild(new_p->text);
	}
	else {
	    if (new_p->label == NULL) {
		XtTranslations label_trans = 
		    XtParseTranslationTable(ComboBoxLabelTranslationTable);

		n = 0;
		XtSetArg(args[n], XmNalignment, new_p->alignment); n++;
		XtSetArg(args[n], XmNrecomputeSize, FALSE); n++;
		XtSetArg(args[n], XmNmarginLeft, LABEL_PADDING); n++;
		XtSetArg(args[n], XmNmarginRight, LABEL_PADDING); n++;
		XtSetArg(args[n], XmNmarginWidth, 0); n++;
		XtSetArg(args[n], XmNmarginHeight, 0); n++;
		XtSetArg(args[n], XmNstringDirection, 
			 new->manager.string_direction); n++;
		new_p->label = XtCreateWidget("ComboBoxLabel", 
					      xmLabelWidgetClass,
					      (Widget)new, args, n);
		XtOverrideTranslations((Widget)new_p->label, label_trans);
		if (new_p->horizontal_spacing == cur_p->horizontal_spacing)
		    new_p->horizontal_spacing = 1;
		if (new_p->vertical_spacing == cur_p->vertical_spacing)
		    new_p->vertical_spacing = 2;

		n = 0;
		XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
		new_p->sep = XtCreateWidget("ComboBoxSeparator", 
					    xmSeparatorWidgetClass,
					    (Widget)new, args, n);
	    }
	    else if (new->manager.string_direction != 
		     current->manager.string_direction) {
		XtSetArg(args[0], XmNstringDirection, 
			 new->manager.string_direction);
		XtSetValues(new_p->label, args, 1);
	    }
	    XtUnmanageChild(new_p->text);
	    XtManageChild(new_p->label);
	    XtManageChild(new_p->sep);
	}
	/* 
	 * Text-fields and labels have different shadows.  Only
	 * change if user didn't change the shadow resource.
	 */
	if (COMBO_SHADOW(new) == COMBO_SHADOW(current))
	    COMBO_SHADOW(new) = ((new_p->type == XmDROP_DOWN_COMBO_BOX) ?
				 TEXT_FIELD_SHADOW : LABEL_SHADOW);
    }

    if (new_p->text && (new_p->text == cur_p->text)) {
	n = 0;
	if (new_p->text_columns != cur_p->text_columns) {
	    XtSetArg(args[n], XmNcolumns, new_p->text_columns); n++;
	}
	if (new_p->text_max_length != cur_p->text_max_length) {
	    XtSetArg(args[n], XmNmaxLength, new_p->text_max_length); n++;
	}
	if (n > 0)
	    XtSetValues(new_p->text, args, n);
    }

    /*
     * LabelWidget alignment has changed.
     */
    if (new_p->label && (new_p->alignment != cur_p->alignment)) {
	XtSetArg(args[0], XmNalignment, new_p->alignment);
	XtSetValues(new_p->label, args, 1);
    }

    if (new_p->label && ((new_p->items != cur_p->items) || 
			 (new_p->item_count != cur_p->item_count) ||
			 (new_p->label != cur_p->label))) {
	SetMaximumLabelSize(new_p);
	label_size_changed = TRUE;
    }

    /* Copy and free label-string */
    if (new_p->label_string != cur_p->label_string) {
	if (new_p->label_string)
	    new_p->label_string = XmStringCopy(new_p->label_string);
	if (cur_p->label_string)
	    XmStringFree(cur_p->label_string);	
	/* 
	 * force_label_string usage if it is specified and items is not.
	 * This will be the perminant label string only if update-label
	 * is false, else it is only used until the user picks something
	 * new off the list.
	 */
	if (new_p->items == cur_p->items)
	    force_label_string = TRUE;
    }

    if ((new_p->items != cur_p->items) ||
	(new_p->alignment != cur_p->alignment) ||
	(new_p->type != cur_p->type) ||
	(new_p->item_count != cur_p->item_count) ||
	(new_p->selected_position != cur_p->selected_position) ||
	(new_p->selected_item != cur_p->selected_item) ||
	(new_p->label != cur_p->label) ||
	(new_p->update_label != cur_p->update_label) ||
	(new_p->label_string != cur_p->label_string)) {

	/* selected_item resource used before selected_position */
	if (new_p->selected_item &&
	    (new_p->selected_item != cur_p->selected_item))
	    DtComboBoxSelectItem((Widget)new, new_p->selected_item);
	else
	    XmListSelectPos(new_p->list, new_p->selected_position + 1, FALSE);


	if (new_p->type == XmDROP_DOWN_COMBO_BOX)
	    SetTextFieldData(new_p, NULL);
	else
	    SetLabelData(new_p, NULL, force_label_string);
    }
   
    /*
     * Must recalculate the combo_box and re-layout the children.
     * If this is not editable, then set the label to its' maximum
     * size; it will get chopped if it is too big.  This is needed 
     * because we shrink the label down, and SetComboBoxSize() uses
     * the label's core sizes to figure what size to become.
     */
    if ((new_p->type != cur_p->type) ||
	(new_p->arrow_type != cur_p->arrow_type) ||
	(COMBO_MARGIN_W(new) != COMBO_MARGIN_W(current)) ||
	(COMBO_MARGIN_H(new) != COMBO_MARGIN_H(current)) ||
	(COMBO_H_SPACING(new) != COMBO_H_SPACING(current)) ||
	(COMBO_V_SPACING(new) != COMBO_V_SPACING(current)) ||
	(COMBO_SHADOW(new) != COMBO_SHADOW(current)) ||
	(new_p->orientation != cur_p->orientation) ||
	(new_p->arrow_spacing != cur_p->arrow_spacing) ||
	((new_p->type == XmDROP_DOWN_LIST_BOX) && label_size_changed)) {
	ClearShadow(current, TRUE);
	if (new_p->recompute_size)
	    SetComboBoxSize(new);
	LayoutChildren(new);
	DrawShadow(new);
    }

    SyncWithList(new_p);
    return(FALSE);
}


/*
 * This function clears the shadow around our widget.  If all is TRUE,
 * then clear all 4 sides; otherwise, only clear the right and bottom
 * sides (during resize). 
 */ 
static void
ClearShadow(DtComboBoxWidget w,
	    Boolean all)
{
    Dimension shadow = COMBO_SHADOW(w);
    Dimension margin_w = COMBO_MARGIN_W(w);
    Dimension margin_h = COMBO_MARGIN_H(w);

    if ((shadow > 0) && XtIsRealized(w)) {
	if (all) {
	    XClearArea(XtDisplayOfObject((Widget)w),
		       XtWindowOfObject((Widget)w), 
		       margin_w, margin_h,
		       w->combo_box.old_width - (margin_w * 2),
		       shadow, FALSE);
	    XClearArea(XtDisplayOfObject((Widget)w),
		       XtWindowOfObject((Widget)w), 
		       margin_w, margin_h, shadow, 
		       w->combo_box.old_height - (margin_h * 2), FALSE);
	}
	XClearArea(XtDisplayOfObject((Widget)w), 
		   XtWindowOfObject((Widget)w), margin_w,
		   w->combo_box.old_height - margin_h - shadow,
		   w->combo_box.old_width - (margin_w * 2), shadow, FALSE);
	XClearArea(XtDisplayOfObject((Widget)w), XtWindowOfObject((Widget)w),
		   w->combo_box.old_width - margin_w - shadow,
		   margin_h, shadow, 
		   w->combo_box.old_height - (margin_h * 2), FALSE);
    }
    DrawHighlight(w, TRUE);
}

/* 
 * This functions draws the shadow around our combo-box.
 */
static void
DrawShadow(DtComboBoxWidget w)
{
    Dimension shadow = COMBO_SHADOW(w);
    Dimension margin_w = COMBO_MARGIN_W(w);
    Dimension margin_h = COMBO_MARGIN_H(w);
    
    if ((shadow > 0) && XtIsRealized(w)) {
	_XmDrawShadows(XtDisplayOfObject((Widget)w),
		       XtWindowOfObject((Widget)w),
		       w->manager.top_shadow_GC,
		       w->manager.bottom_shadow_GC, 
		       margin_w, margin_h,
		       w->core.width - (margin_w * 2),
		       w->core.height - (margin_h * 2),
		       shadow, XmSHADOW_OUT);
    }
    DrawHighlight(w, FALSE);
}

/*
 * Take the string out of the list and put it into the text-field.
 * text-fields don't handle xm-strings, so we must get the char*
 * out of it (only getting the first segment).  This is slower than
 * storing the text-strings (char*) ourselves, but that would take
 * up a lot of memory.  Since this setting happens during a user
 * action, speed isn't a problem.
 */
static void
SetTextFieldData(DtComboBoxPart *combo_p,
		 XmString item)
{
    XmListWidget list = (XmListWidget)combo_p->list;
    Arg arg;


    if (!item && list->list.itemCount)
		item = list->list.items[combo_p->selected_position];

    if (!item) 
	{
		combo_p->selected_item = NULL;
		XtSetArg(arg, XmNvalue, "");
		XtSetValues(combo_p->text, &arg, 1);
    }
    else 
	{
		XmStringContext			context = NULL;
		char *					text = NULL;
		XmStringCharSet			charset = NULL;

		XmStringDirection		direction;
		XmStringComponentType	unknown_tag;
		XmStringComponentType	type;

		unsigned short			ul = 0;
		unsigned char *			uv = NULL;

		Boolean					done = FALSE;

		combo_p->selected_item = item;

		XmStringInitContext(&context, item);
	
		/* Loop until 1st char* found */
		while (!done) 
		{
			type = XmStringGetNextComponent(context, 
											&text, 
											&charset,
											&direction, 
											&unknown_tag, 
											&ul, 
											&uv);
			switch (type) 
			{
			case XmSTRING_COMPONENT_END:
				done = TRUE;
				break;

			case XmSTRING_COMPONENT_TEXT:
			case XmSTRING_COMPONENT_LOCALE_TEXT:
				XtSetArg(arg, XmNvalue, text);
				XtSetValues(combo_p->text, &arg, 1);

			if (text)
				XtFree((char *) text);

				done = TRUE;
				break;

			case XmSTRING_COMPONENT_CHARSET:
				if (charset)
					XtFree((char *) charset);
				break;

			default:
				if (uv)
					XtFree((char *) uv);
				break;
			}



		}

		if (context)
			XmStringFreeContext(context);
    }

}


/*
 * Set the maximum size of the label, depending on the
 * characteristics of the list of items.
 */
static void
SetMaximumLabelSize(DtComboBoxPart *combo_p)
{
    XmListWidget list = (XmListWidget)combo_p->list;
    XmFontList font_list;
    Dimension width, height;
    Dimension longest = 0;
    Dimension highest = 0;
    Arg args[5];
    int i;

    /* Get font info from the widget */
    XtSetArg(args[0], XmNfontList, &font_list);
    XtGetValues(combo_p->label, args, 1);

    if ( list->list.itemCount && combo_p->update_label) {
	/*
	 * Loop through all the items to find the biggest dimensions
	 */
	for (i = 0; i < combo_p->item_count; i++) {
	    XmStringExtent(font_list, list->list.items[i], &width, &height);
	    longest = (width > longest) ? width : longest;
	    highest = (height > highest) ? height : highest;
	}
    }
    else {
	XmStringExtent(font_list, combo_p->label_string, &longest, &highest);
    }
	
    combo_p->label_max_length = longest + (LABEL_PADDING * 2);
    combo_p->label_max_height = highest;
    XtResizeWidget(combo_p->label, combo_p->label_max_length, highest, 
		   combo_p->label->core.border_width);
}


/*
 * Put the current list item into the label.
 * This could probably be faster if we see if the label is the
 * same as the new item?
 */
static void
SetLabelData(DtComboBoxPart *combo_p,
	     XmString item,
	     Boolean force_label_string)
{
    XmListWidget list = (XmListWidget)combo_p->list;
    int index = combo_p->selected_position;
    Arg arg;

    /*
     * If the item is empty, get the current item from the list, or
     * use label_string if update_label is FALSE.  If that is empty, 
     * use InitLabel.
     */
    if (force_label_string || (combo_p->update_label == FALSE))
	item = combo_p->label_string ? combo_p->label_string : InitLabel;
    else {
	if (!item) {
	    if (list->list.itemCount)
		item = list->list.items[index];
	    else
		item = InitLabel;
	}
	
	/* Keep label_string in sync with item picked */
	if (combo_p->label_string)
	    XmStringFree(combo_p->label_string);
	combo_p->label_string = XmStringCopy(item);
    }

    combo_p->selected_item = item;
    XtSetArg(arg, XmNlabelString, item);
    XtSetValues(combo_p->label, &arg, 1);
}

/*
 * This is the browseSelect and defaultAction callback handler for the
 * ListWidget.  If using the textWidget, we only take the first 
 * segment of the XmString (TextWidgets don't handle XmStrings).  If we
 * are using a label, then just set the labelString resource.
 */
static void
select_cb(Widget w,
	  XtPointer client_data,
	  XtPointer call_data)
{
    DtComboBoxWidget combo_w = (DtComboBoxWidget)client_data;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo_w->combo_box);
    XmListCallbackStruct *info = (XmListCallbackStruct*)call_data;
    DtComboBoxCallbackStruct cb;
    
    combo_p->selected_position = info->item_position - 1;
    if (combo_p->type == XmDROP_DOWN_COMBO_BOX) {
	SetTextFieldData(combo_p, info->item);
    }
    else {	/* Set the labelWidget string */
	SetLabelData(combo_p, info->item, FALSE);
    }
    
    /*
     * Only popdown if this is the defaultAction callback.  We don't
     * want to popdown with browseSelect callback; that would cause the
     * menu to popdown when the user moved selection with the keyboard.
     * Doing it this way, allows the menu to stay up during 
     * keyboard navigation.  When menu goes away, make sure input
     * focus goes back into the textField (if editable).
     */
    if (info->reason == XmCR_DEFAULT_ACTION) {
	combo_p->popped_up = FALSE;
	XtPopdown(combo_p->shell);
	XtUngrabPointer(combo_p->shell, CurrentTime);
	XtUngrabKeyboard(combo_p->list, CurrentTime);
	/*
	 * _XmRemoveGrab() was causing trouble on irix and linux.  The navigator
	 * stopped responding to clicks cause of it.
	 *
	 */
#if 0
    _XmRemoveGrab(combo_p->shell);
#else
    XtRemoveGrab(combo_p->shell);

	DisplayUserGrabbed(w) = False;
#endif
	XFlush(XtDisplay(combo_p->shell));
	if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	    XmProcessTraversal(combo_p->text, XmTRAVERSE_CURRENT);
    }

    if (((ShellWidget)(combo_p->shell))->shell.popped_up == FALSE) {
	/* The list will free info->item */
	cb.reason = XmCR_SELECT;
	cb.event = info->event;
	cb.item_or_text = info->item;
	cb.item_position = combo_p->selected_position;
	XtCallCallbackList((Widget)w, combo_p->selection_callback, 
			   (XtPointer)&cb);
    }
}


/*
 * This is the event_handler for our shell widget.  The grab happens
 * on the shell while the user is not doing anything inside the list.
 * This allows us to know if the user pressed a button outside our
 * application.  If the user pressed a button anywhere but inside
 * the shell, then popdown the menu and ungrab everything.
 */
static void
shell_event_handler(Widget widget,
		    XtPointer client_data,
		    XEvent *event,
		    Boolean *dispatch)
{
    DtComboBoxWidget combo_w = (DtComboBoxWidget)client_data;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo_w->combo_box);
    XmScrolledWindowWidget sw = (XmScrolledWindowWidget)XtParent(combo_p->list);
    XmScrollBarWidget v_scrollbar = sw->swindow.vScrollBar;
    XmScrollBarWidget h_scrollbar = sw->swindow.hScrollBar;
    Window window = event->xbutton.window;

    if (((ShellWidget)(combo_p->shell))->shell.popped_up &&
	(window != XtWindowOfObject(combo_p->list)) &&
	(h_scrollbar && (window != XtWindowOfObject((Widget)h_scrollbar))) &&
	(v_scrollbar && (window != XtWindowOfObject((Widget)v_scrollbar)))) {
	combo_p->popped_up = FALSE;
	XtPopdown(combo_p->shell);
	XtUngrabPointer(combo_p->shell, CurrentTime);
	XtUngrabKeyboard(combo_p->list, CurrentTime);
	/*
	 * _XmRemoveGrab() was causing trouble on irix and linux.  The navigator
	 * stopped responding to clicks cause of it.
	 *
	 */
#if 0
    _XmRemoveGrab(combo_p->shell);
#else
    XtRemoveGrab(combo_p->shell);

	DisplayUserGrabbed(widget) = False;
#endif
	if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	    XmProcessTraversal(combo_p->text, XmTRAVERSE_CURRENT);
    }
}


/*
 * This is the event_handler for our list widget.  Getting the pointer
 * grabbing to work correctly was not very easy.  In order for everything
 * to work correctly, we only do grab-pointer, for the shell, while not
 * doing anything inside the list.  If doing something inside the list
 * we remove the grab on the shell.  This is the only way that the 
 * list will get the ButtonRelease if doing browse while outside the
 * list.  The toolkit automatically does a grab in between ButtonPress
 * and ButtonRelease; therefore, the shell ungrab can't be done inside
 * the ButtonPress event.
 */
static void
list_event_handler(Widget widget,
		   XtPointer client_data,
		   XEvent *event,
		   Boolean *dispatch)
{
    DtComboBoxWidget combo_w = (DtComboBoxWidget)client_data;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo_w->combo_box);
    static int btn_down = False;

    switch (event->type) {
    case ButtonPress:
	/* 
	 * Can't ungrab shell because of toolkit interaction (the toolkit
	 * automatically does a grab in between ButtonPress and
	 * ButtonRelease).
	 */
	btn_down = True;
	break;
    case ButtonRelease:
	/*
	 * Get rid of menu when Button is released.
	 */
	combo_p->popped_up = FALSE;
	XtPopdown(combo_p->shell);
	XtUngrabKeyboard(combo_p->list, CurrentTime);
	/*
	 * _XmRemoveGrab() was causing trouble on irix and linux.  The navigator
	 * stopped responding to clicks cause of it.
	 *
	 */
#if 0
    _XmRemoveGrab(combo_p->shell);
#else
    XtRemoveGrab(combo_p->shell);

	DisplayUserGrabbed(widget) = False;

#endif
	if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	    XmProcessTraversal(combo_p->text, XmTRAVERSE_CURRENT);
	btn_down = False;
	break;
    case FocusOut:
	/*
	 * There are interaction problems between the list and the
	 * scrollbars in terms of Focus.  We always want our list
	 * to have focus, so grab it back if we lose it.
	 */
	if (((ShellWidget)(combo_p->shell))->shell.popped_up) {
	    _XmGrabKeyboard(widget, False, GrabModeAsync, GrabModeAsync, 
			    CurrentTime);
	    XtSetKeyboardFocus(combo_p->list, RevertToNone);
	}
	break;
    case EnterNotify:
	/*
	 * Don't let the shell have the pointer grabbed while
	 * we are inside the list.
	 */
	if (btn_down == False) {
	    XtUngrabPointer(combo_p->shell, CurrentTime);
	}
	break;
    case LeaveNotify:
	/*
	 * Only let the shell have the grab if the menu is up
	 * and the btn isn't down.  We don't want the shell to
	 * have the grab while the user is interacting in any way
	 * with the list.
	 */
	if ((((ShellWidget)(combo_p->shell))->shell.popped_up) &&
	    (btn_down == FALSE)) {
	    _XmGrabPointer(combo_p->shell, True, ButtonPressMask,
			   GrabModeAsync, GrabModeAsync, None, 
			   XmGetMenuCursor(XtDisplayOfObject(combo_p->shell)),
			   CurrentTime);
	}
	break;
    }
}


/* Caller must free string */
static char*
GetTextString(XmString xm_string)
{
    XmStringContext context;
    XmStringComponentType type;
    XmStringCharSet charset;
    XmStringDirection direction;
    XmStringComponentType unknown_tag;
    unsigned short ul;
    unsigned char *uv;
    char *text = NULL;
    Boolean done = FALSE;

    XmStringInitContext(&context, xm_string);
    
    /* Loop until 1st char* found */
    while (!done) {
	type = XmStringGetNextComponent(context, &text, &charset,
					&direction, &unknown_tag, 
					&ul, &uv);
	switch (type) {
	case XmSTRING_COMPONENT_END:
	    done = TRUE;
	    break;
	case XmSTRING_COMPONENT_TEXT:
	case XmSTRING_COMPONENT_LOCALE_TEXT:
	    done = TRUE;
	    break;
	default:
	    break;
	}
    }
    XmStringFreeContext(context);
    return(text);
}

static void
TextFieldActivate(DtComboBoxPart *combo_p)
{
    XmTextFieldWidget w = (XmTextFieldWidget)(combo_p->text);
    XmListWidget list = (XmListWidget)combo_p->list;
    XmAnyCallbackStruct cb;
    char *data = NULL;
    char *text = NULL;
    Arg arg;
    
    XtSetArg(arg, XmNvalue, &data);
    XtGetValues((Widget)w, &arg, 1);

    if ( list->list.itemCount)
	text = GetTextString(list->list.items[combo_p->selected_position]);

    if (text && data && (strcmp(text, data) == 0)) {
	XtFree(text);
	return;
    }
    /* Only send callback if both are not NULL */
    else if (!((text == NULL) && (data == NULL))) {
	cb.reason = XmCR_ACTIVATE;
	cb.event  = NULL;
	XtCallCallbackList((Widget)w, w->text.activate_callback,
			   (XtPointer) &cb);
	if (text)   
	    XtFree(text);
    }
}

/*
 * This is the activate callback for the arrow button.  This 
 * sets the shell position and width, does the correct grabs, and
 * puts the shell on the screen.
 */
static void
activate_cb(Widget w,
	    XtPointer client_data,
	    XtPointer call_data)
{
    DtComboBoxWidget combo_w = (DtComboBoxWidget)client_data;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo_w->combo_box);
    Display *disp = XtDisplayOfObject((Widget)combo_w);
    int screen;
    Dimension width, height;
    Dimension disp_width, disp_height;
    Position root_x, root_y;
    Arg args[5];
    int n;
    char *text;
    XmString item;

    if ((combo_p->type == XmDROP_DOWN_COMBO_BOX) && 
		!combo_p->no_callback_for_arrow)
	{
		TextFieldActivate(combo_p);
	}


    /*
     * Don't popup if no items in the list.
     */
    if (!((XmListWidget)combo_p->list)->list.itemCount)
	return;


    if ( combo_p->move_selecteditem_up )
    {
      if ( combo_p->selected_item )
      {
      text = GetTextString(((XmListWidget)combo_p->list)->
				list.items[combo_p->selected_position]);

      item = ((XmListWidget)combo_p->list)->
		list.items[combo_p->selected_position];

      DtComboBoxSelectItemMoveup(combo_w, item);
      }
      else 
      {
	XmListSelectPos(combo_p->list, 1, False);
	XmListSetPos (combo_p->list, 1 );
      }
    }

    screen = DefaultScreen(disp);
    disp_width = DisplayWidth(disp, screen);
    disp_height = DisplayHeight(disp, screen);

    /*
     * Call the menu-post callback if requested.  This allows the
     * user to change the items, instead of using the losing-focus callback.
     * If the user used the losing-focus callback to change the items, the
     * size of the list/shell will change while it is popped up.  We
     * could disallow SetValues while the menu is posted, but let's see
     * how things go first.
     */
    if (combo_p->menu_post_callback) {
	XmAnyCallbackStruct info;

	info.reason = XmCR_MENU_POST;
	info.event = (XEvent*)NULL;
	XtCallCallbackList((Widget)combo_w, combo_p->menu_post_callback, 
			   (XtPointer)&info);
    }

    width = combo_p->max_shell_width;
    height = combo_p->max_shell_height;

    /* Get root coords of ComboBox */
    XtTranslateCoords((Widget)combo_w, combo_w->core.x, combo_w->core.y,
		      &root_x, &root_y);

    /*
     * Make necessary adjustments for offset of our widget 
     * inside its' parent.  Calculate the width of the shell.
     * This must be done every time the shell gets popped up, because 
     * the x/y can change as well as the width (from list's visibleItemCount 
     * or geometry management changes).
     */
    root_x -= combo_w->core.x;
    root_y -= combo_w->core.y;
    root_y += (combo_w->core.height - COMBO_MARGIN_H(combo_w));

    /*
     * Make sure the shell is at least as big as our combo-box, and
     * make sure it stays on the screen.
     */
    if (width < combo_w->core.width)
	width = combo_w->core.width;
    if ((int)(root_x + width) > (int)disp_width)
	width = (disp_width - root_x);
    if ((int)(root_y + height) > (int)disp_height)
	height = (disp_height - root_y);

    n = 0;
    XtSetArg(args[n], XmNx, root_x); n++;
    XtSetArg(args[n], XmNy, root_y); n++;
    XtSetArg(args[n], XmNwidth, width); n++;
    XtSetArg(args[n], XmNheight, height); n++;
    XtSetValues(combo_p->shell, args, n);

    combo_p->popped_up = TRUE;
    XtPopup(combo_p->shell, XtGrabNone);

    /*
     * Set up the grab for the shell and list.  The shell gets the
     * pointer grab so that events will go into the list and scrollbars
     * correctly, but events outside the shell will go to the shell.
     * See shell and list event handlers for details about grabs.
     */
    _XmGrabPointer(combo_p->shell, True, ButtonPressMask,
		   GrabModeAsync, GrabModeAsync, None, 
		   XmGetMenuCursor(disp), CurrentTime);
    _XmGrabKeyboard(combo_p->list, False, GrabModeAsync, GrabModeAsync, 
		   CurrentTime);

	/*
	 * _XmAddGrab() was causing trouble on irix and linux.  The navigator
	 * stopped responding to clicks cause of it.
	 *
	 */
#if 0
    _XmAddGrab(combo_p->shell, True, True);
#else
    XtAddGrab(combo_p->shell, True, True);

	DisplayUserGrabbed(w) = True;
#endif

    /*
     * Where to define the cursor for the list widget.  It would be
     * nice to do it in the list's realize function, but that's not
     * possible.  We can't use the ComboBox realize function, because
     * the list isn't realized at that point.  This is the simpliest
     * way to get this done.  This is needed to make sure the list has the
     * correct menu during browse scrolling, etc.
     */
    XDefineCursor(disp, XtWindowOfObject(combo_p->shell), 
		  XmGetMenuCursor(disp));
}


/*
 * Make sure arrow is symetrical. 45 degree angle.  I'm not sure how
 * inefficient get/releasing the GC every time is (they are cached by
 * the toolkit)?
 */
static void
arrow_expose_cb(Widget w,
		XtPointer client_data,
		XtPointer call_data)
{
    Display *disp = XtDisplayOfObject(w);
    Window win = XtWindowOfObject(w);
    XGCValues values;
    short center_w = w->core.width/2;
    short center_h = ((int)(w->core.height - 3))/2;
    XPoint points[10];
    Arg arg;
    GC gc;

    XtSetArg(arg, XmNforeground, &(values.foreground));
    XtGetValues(w, &arg, 1);

    values.line_width = 0;
    values.line_style = LineSolid;
    values.fill_style = FillSolid;
    gc = XtGetGC(w, GCForeground | GCFillStyle | GCLineStyle | GCLineWidth,
		 &values);

    XDrawLine(disp, win, gc, 1, center_h + center_w + 1, w->core.width - 2,
	      center_h + center_w + 1);

    /* A - bottom point */
    points[0].x = center_w;
    points[0].y = center_h + (int)(center_w * .8);

    /* B - far left point */
    points[1].x = center_w - (int)(center_w * .8);
    points[1].y = center_h;

    /* C  - inner left point */
    points[2].x = center_w - (int)(center_w * .3);
    points[2].y = points[1].y;

    /* D - top left point */
    points[3].x = points[2].x;
    points[3].y = center_h - (int)(center_w * .8);

    /* E - top right point */
    points[4].x = center_w + (int)(center_w * .3);
    points[4].y = points[3].y;

    /* F - inner right point */
    points[5].x = points[4].x;
    points[5].y = points[1].y;

    /* G - far right point */
    points[6].x = center_w + (int)(center_w * .8);
    points[6].y = points[1].y;

    /* A - bottom point */
    points[7].x = points[0].x;
    points[7].y = points[0].y;

    XDrawLines(disp, win, gc, points, 8, CoordModeOrigin);
    XFillPolygon(disp, win, gc, points, 8, Convex, CoordModeOrigin);
    XtReleaseGC(w, gc);
}

/*
 * We get the text-field losing-focus callback, so pass it on to

 * the user if they requested it.  Our losing-focus callback 
 * is just a convenience callback, so that the user doesn't
 * have to get the text-field first.  This make our integration
 * with XDesigner a little easier.
 */
static void
text_losing_focus_cb(Widget w,
		     XtPointer client_data,
		     XtPointer call_data)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)client_data;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);

    if (combo_p->losing_focus_callback)
	XtCallCallbackList((Widget)combo, combo_p->losing_focus_callback, 
			   (XtPointer)call_data);
}

/*
 * We get the text-field activate callback, so pass it on to
 * the user if they requested it.  Our activate callback 
 * is just a convenience callback, so that the user doesn't
 * have to get the text-field first.  This make our integration
 * with XDesigner a little easier.
 */
static void
text_activate_cb(Widget w,
		 XtPointer client_data,
		 XtPointer call_data)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)client_data;

    if (combo->combo_box.activate_callback)
	XtCallCallbackList((Widget)combo, 
			   combo->combo_box.activate_callback, 
			   (XtPointer)call_data);
}


/*
 * We get the text-field focus callback, so pass it on to
 * the user if they requested it.  Our focus callback 
 * is just a convenience callback, so that the user doesn't
 * have to get the text-field first.  This make our integration
 * with XDesigner a little easier.
 */
static void
text_focus_cb(Widget w,
	      XtPointer client_data,
	      XtPointer call_data)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)client_data;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);

    if (combo_p->focus_callback)
	XtCallCallbackList((Widget)combo, combo_p->focus_callback, 
			   (XtPointer)call_data);
}

/*
 * Try and keep our list related rsources in sync with the list widget.
 * This is not always possible, depending on if the programmer makes
 * list widget calls directly.  If we get out of sync with the
 * list widget, our SetValues() may not work correctly (when the
 * comparisons are done).  Should do get values in case list widget
 * names are changed?
 */
static void
SyncWithList(DtComboBoxPart *combo_p)
{
    XmListWidget list = (XmListWidget)combo_p->list;

    combo_p->items = list->list.items;
    combo_p->item_count = list->list.itemCount;
    combo_p->list_font_list = list->list.font;
    combo_p->list_margin_height = list->list.margin_height;
    combo_p->list_margin_width = list->list.margin_width;
    combo_p->list_spacing = list->list.ItemSpacing;
    combo_p->top_item_position = list->list.top_position;
    combo_p->visible_item_count = list->list.visibleItemCount;
	/* selected position must be into the items */
	if (!(combo_p->selected_position < combo_p->item_count))
		combo_p->selected_position = 0;
}

/*
 * Routines which manipulate the ComboBox list.  These are external
 * for use by users of our widget.
 */
Widget 
DtCreateComboBox(Widget parent,
		 char *name,
		 Arg *arglist,
		 int num_args)
{
    return(XtCreateWidget(name, dtComboBoxWidgetClass, parent,
			  arglist, num_args));
}

void
DtComboBoxAddItem(Widget combo_w,
		  XmString item,
		  int pos,
		  Boolean unique)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)combo_w;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    XmStringTable list_items = ((XmListWidget)combo_p->list)->list.items;
    int i;

    if (item && ((XmListWidget)combo_p->list)->list.itemCount) {
	for (i = 0; i < combo_p->item_count; i++)
	    if (XmStringCompare(item, list_items[i]))
		break;
	if ((i < combo_p->item_count) && unique)
	    return;
    }

    XmListAddItem(combo_p->list, item, pos);
    SyncWithList(combo_p);

    if (combo_p->label) {
	SetMaximumLabelSize(combo_p);
	if (combo_p->type == XmDROP_DOWN_LIST_BOX) {
	    ClearShadow(combo, TRUE);
	    if (combo_p->recompute_size)
		SetComboBoxSize(combo);
	    LayoutChildren(combo);
	    DrawShadow(combo);
	}
    }
    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	SetTextFieldData(combo_p, NULL);
    else
	SetLabelData(combo_p, NULL, FALSE);
}

void
DtComboBoxAddItemSelected(Widget combo_w,
			  XmString item,
			  int pos,
			  Boolean unique)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)combo_w;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    XmStringTable list_items = ((XmListWidget)combo_p->list)->list.items;
    int i;

    if (item && ((XmListWidget)combo_p->list)->list.itemCount) {
	for (i = 0; i < combo_p->item_count; i++)
	    if (XmStringCompare(item, list_items[i]))
		break;
	if ((i < combo_p->item_count) && unique)
	    return;
    }

    XmListAddItem(combo_p->list, item, pos);
    XmListSelectPos(combo_p->list, pos, False);
    SyncWithList(combo_p);

    if (combo_p->label) {
	SetMaximumLabelSize(combo_p);
	if (combo_p->type == XmDROP_DOWN_LIST_BOX) {
	    ClearShadow(combo, TRUE);
	    if (combo_p->recompute_size)
		SetComboBoxSize(combo);
	    LayoutChildren(combo);
	    DrawShadow(combo);
	}
    }
    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	SetTextFieldData(combo_p, NULL);
    else
	SetLabelData(combo_p, NULL, FALSE);
}


void
DtComboBoxDeleteAllItems(Widget combo_w)
{
  DtComboBoxWidget combo = (DtComboBoxWidget)combo_w;
  DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);

  XmListDeleteAllItems(combo_p->list);
  SyncWithList(combo_p);
  
  if (combo_p->label) {
    SetMaximumLabelSize(combo_p);
    if (combo_p->type == XmDROP_DOWN_LIST_BOX) {
      ClearShadow(combo, TRUE);
      if (combo_p->recompute_size)
	SetComboBoxSize(combo);
      LayoutChildren(combo);
      DrawShadow(combo);
    }
  }
  if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
    SetTextFieldData(combo_p, NULL);
  else
    SetLabelData(combo_p, NULL, FALSE);
}


void
DtComboBoxDeletePos(Widget combo_w,
		    int pos)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)combo_w;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);

    XmListDeletePos(combo_p->list, pos);
    SyncWithList(combo_p);

    if (combo_p->label) {
	SetMaximumLabelSize(combo_p);
	if (combo_p->type == XmDROP_DOWN_LIST_BOX) {
	    ClearShadow(combo, TRUE);
	    if (combo_p->recompute_size)
		SetComboBoxSize(combo);
	    LayoutChildren(combo);
	    DrawShadow(combo);
	}
    }
    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
	SetTextFieldData(combo_p, NULL);
    else
	SetLabelData(combo_p, NULL, FALSE);
}

void
DtComboBoxSetItem(Widget combo_w,
		  XmString item)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)combo_w;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    XmStringTable list_items = ((XmListWidget)combo_p->list)->list.items;
    int i;

    if (item && ((XmListWidget)combo_p->list)->list.itemCount) {
	for (i = 0; i < combo_p->item_count; i++)
	    if (XmStringCompare(item, list_items[i]))
		break;
	if (i < combo_p->item_count) {
	    combo_p->selected_position = i;
	    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
		SetTextFieldData(combo_p, NULL);
	    else
		SetLabelData(combo_p, NULL, FALSE);
	    XmListSetItem(combo_p->list, item);
	    SyncWithList(combo_p);
	}
	else
	    XtWarning(COMBO_SET_ITEM);
    }
    else
	XtWarning(COMBO_SET_ITEM);
}

void
DtComboBoxSelectItem(Widget combo_w,
		     XmString item)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)combo_w;
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    XmStringTable list_items = ((XmListWidget)combo_p->list)->list.items;
    int i;

    if (item && ((XmListWidget)combo_p->list)->list.itemCount) {
	for (i = 0; i < combo_p->item_count; i++)
	    if (XmStringCompare(item, list_items[i]))
		break;
	if (i < combo_p->item_count) {
	    combo_p->selected_position = i;
	    if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
		SetTextFieldData(combo_p, NULL);
	    else
		SetLabelData(combo_p, NULL, FALSE);
	    XmListDeselectAllItems(combo_p->list);
	    XmListSelectItem(combo_p->list, item, FALSE);
	    SyncWithList(combo_p);
	}
	else
	    XtWarning(COMBO_SELECT_ITEM);
    }
    else
	XtWarning(COMBO_SELECT_ITEM);
}

void
DtComboBoxSelectItemMoveup(DtComboBoxWidget combo,
			   XmString item)
{
    DtComboBoxPart *combo_p = (DtComboBoxPart*)&(combo->combo_box);
    XmStringTable list_items = ((XmListWidget)combo_p->list)->list.items;
    int i;
 
    if (item && ((XmListWidget)combo_p->list)->list.itemCount) {
        for (i = 0; i < combo_p->item_count; i++)
            if (XmStringCompare(item, list_items[i]))
                break;
        if (i < combo_p->item_count) {
            combo_p->selected_position = i;
            if (combo_p->type == XmDROP_DOWN_COMBO_BOX)
                SetTextFieldData(combo_p, NULL);
            else
                SetLabelData(combo_p, NULL, FALSE);
            XmListDeselectAllItems(combo_p->list);
	    XmListAddItem(combo_p->list, item, 1);
            XmListSelectItem(combo_p->list, item, FALSE);
	    XmListDeletePos(combo_p->list, combo_p->selected_position+2);
	    combo_p->selected_position = 0;
	    XmListSetPos(combo_p->list, 1);
            SyncWithList(combo_p);
        }
        else
            XtWarning(COMBO_SELECT_ITEM);
    }
    else
        XtWarning(COMBO_SELECT_ITEM);
}


/*
 * Synthetic GetValues for List resources.
 */

static XmImportOperator
_XmSetSyntheticResForChild(Widget widget,
			   int offset,
			   XtArgVal *value)
{ 
    return(XmSYNTHETIC_LOAD);
}

void
_DtComboBoxGetArrowSize(Widget w,
			int resource_offset,
			XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;

    *value = (XtArgVal)combo->combo_box.arrow->core.height;
}

void
_DtComboBoxGetLabelString(Widget w,
			  int resource_offset,
			  XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;

    if (combo->combo_box.label_string)
	*value = (XtArgVal)XmStringCopy(combo->combo_box.label_string);
    else
	*value = (XtArgVal)NULL;
}

void
_DtComboBoxGetListItemCount(Widget w,
			    int resource_offset,
			    XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;
    int data;
    Arg arg;

    XtSetArg(arg, XmNitemCount, &data);
    XtGetValues(combo->combo_box.list, &arg, 1);
    *value = (XtArgVal)data;
}

void
_DtComboBoxGetListItems(Widget w, 
			int resource_offset,
			XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;
    XmStringTable data;
    Arg arg;

    XtSetArg(arg, XmNitems, &data);
    XtGetValues(combo->combo_box.list, &arg, 1);
    *value = (XtArgVal)data;
}

void
_DtComboBoxGetListFontList(Widget w,
			   int resource_offset,
			   XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;
    XmFontList data;
    Arg arg;

    XtSetArg(arg, XmNfontList, &data);
    XtGetValues(combo->combo_box.list, &arg, 1);
    *value = (XtArgVal)data;
}

void
_DtComboBoxGetListMarginHeight(Widget w,
			       int resource_offset,
			       XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;
    Dimension data;
    Arg arg;

    XtSetArg(arg, XmNmarginHeight, &data);
    XtGetValues(combo->combo_box.list, &arg, 1);
    *value = (XtArgVal)data;
}

void
_DtComboBoxGetListMarginWidth(Widget w,
			      int resource_offset,
			      XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;
    Dimension data;
    Arg arg;

    XtSetArg(arg, XmNmarginWidth, &data);
    XtGetValues(combo->combo_box.list, &arg, 1);
    *value = (XtArgVal)data;
}

void
_DtComboBoxGetListSpacing(Widget w,
			  int resource_offset,
			  XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;
    Dimension data;
    Arg arg;

    XtSetArg(arg, XmNspacing, &data);
    XtGetValues(combo->combo_box.list, &arg, 1);
    *value = (XtArgVal)data;
}

void
_DtComboBoxGetListTopItemPosition(Widget w,
				  int resource_offset,
				  XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;
    int data;
    Arg arg;

    XtSetArg(arg, XmNtopItemPosition, &data);
    XtGetValues(combo->combo_box.list, &arg, 1);
    *value = (XtArgVal)data;
}

void
_DtComboBoxGetListVisibleItemCount(Widget w,
				   int resource_offset,
				   XtArgVal *value)
{
    DtComboBoxWidget combo = (DtComboBoxWidget)w;
    int data;
    Arg arg;

    XtSetArg(arg, XmNvisibleItemCount, &data);
    XtGetValues(combo->combo_box.list, &arg, 1);
    *value = (XtArgVal)data;
}
