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
/* Name:		<Xfe/Xfe.h>												*/
/* Description:	Xfe widgets private header file.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeXfeP_h_								/* start XfeP.h			*/
#define _XfeXfeP_h_

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

#include <Xm/XmP.h>
#include <Xm/DrawP.h>
#include <Xfe/Xfe.h>
#include <Xfe/GeometryP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* WidgetClassRec declaration macro.									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_WIDGET_CLASS_RECORD(name,Name)		\
externaldef ( xfe##name##classrec )				\
Xfe##Name##ClassRec								\
xfe##Name##ClassRec

/*----------------------------------------------------------------------*/
/*																		*/
/* WidgetClass declaration macro.										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XFE_WIDGET_CLASS(name,Name)			\
externaldef ( xfe##name##widgetclass )			\
WidgetClass										\
xfe##Name##WidgetClass =						\
( WidgetClass ) & xfe##Name##ClassRec

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive & XfeManager class method types							*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void	(*XfeGeometryProc)		(Widget			w,
										 Dimension *	width,
										 Dimension *	height);
/*----------------------------------------------------------------------*/
typedef void	(*XfePrepareProc)		(Widget			w,
										 int			flags);
/*----------------------------------------------------------------------*/
typedef void	(*XfeExposeProc)		(Widget			w,
										 XEvent *		event,
										 Region			region,
										 XRectangle *	clip_rect);
/*----------------------------------------------------------------------*/
typedef void	(*XfeOrientedProc)		(Widget			w,
										 Widget			descendant,
										 int			x,
										 int			y);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Manager child accept / insert / delete function type					*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef Boolean	(*XfeChildFunc)			(Widget			child);

/*----------------------------------------------------------------------*/
/*																		*/
/* GC Access method														*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef GC		(*XfeGetGCFunc)			(Widget			child);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive & XfeManager class bit gravity							*/
/*																		*/
/*----------------------------------------------------------------------*/
#ifdef _ALPHA_
typedef void (*XfeBitGravityType)(void);
#else
typedef int	XfeBitGravityType;
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget method inheritance macros										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeInheritAcceptChild				((XfeChildFunc)			_XtInherit)
#define XfeInheritArmTimeout				((XtTimerCallbackProc)	_XtInherit)
#define XfeInheritBitGravity				((XfeBitGravityType)	_XtInherit)
#define XfeInheritChangeManaged				((XtWidgetProc)			_XtInherit)
#define XfeInheritChildIsLayable			((XfeChildFunc)			_XtInherit)
#define XfeInheritClickTimeout				((XtTimerCallbackProc)	_XtInherit)
#define XfeInheritDeleteChild				((XfeChildFunc)			_XtInherit)
#define XfeInheritDescendantDragEnd			((XfeOrientedProc)		_XtInherit)
#define XfeInheritDescendantDragMotion		((XfeOrientedProc)		_XtInherit)
#define XfeInheritDescendantDragStart		((XfeOrientedProc)		_XtInherit)
#define XfeInheritDescendantEnter			((XfeOrientedProc)		_XtInherit)
#define XfeInheritDescendantLeave			((XfeOrientedProc)		_XtInherit)
#define XfeInheritDescendantMotion			((XfeOrientedProc)		_XtInherit)
#define XfeInheritDragEnd					((XfeOrientedProc)		_XtInherit)
#define XfeInheritDragMotion				((XfeOrientedProc)		_XtInherit)
#define XfeInheritDragStart					((XfeOrientedProc)		_XtInherit)
#define XfeInheritDrawBackground			((XfeExposeProc)		_XtInherit)
#define XfeInheritDrawComponents			((XfeExposeProc)		_XtInherit)
#define XfeInheritDrawHighlight				((XfeExposeProc)		_XtInherit)
#define XfeInheritDrawPixmap				((XfeExposeProc)		_XtInherit)
#define XfeInheritDrawRaiseBorder			((XfeExposeProc)		_XtInherit)
#define XfeInheritDrawSelection				((XfeExposeProc)		_XtInherit)
#define XfeInheritDrawShadow				((XfeExposeProc)		_XtInherit)
#define XfeInheritDrawString				((XfeExposeProc)		_XtInherit)
#define XfeInheritDrawTitleShadow			((XfeExposeProc)		_XtInherit)
#define XfeInheritEnter						((XfeOrientedProc)		_XtInherit)
#define XfeInheritGetBackgroundGC			((XfeGetGCFunc)			_XtInherit)
#define XfeInheritGetLabelGC				((XfeGetGCFunc)			_XtInherit)
#define XfeInheritGetPixmapGC				((XfeGetGCFunc)			_XtInherit)
#define XfeInheritGetSelectionGC			((XfeGetGCFunc)			_XtInherit)
#define XfeInheritInsertChild				((XfeChildFunc)			_XtInherit)
#define XfeInheritLayoutArrow				((XtWidgetProc)			_XtInherit)
#define XfeInheritLayoutChildren			((XtWidgetProc)			_XtInherit)
#define XfeInheritLayoutComponents			((XtWidgetProc)			_XtInherit)
#define XfeInheritLayoutIndicator			((XtWidgetProc)			_XtInherit)
#define XfeInheritLayoutPixmap				((XtWidgetProc)			_XtInherit)
#define XfeInheritLayoutString				((XtWidgetProc)			_XtInherit)
#define XfeInheritLayoutTitle				((XtWidgetProc)			_XtInherit)
#define XfeInheritLeave						((XfeOrientedProc)		_XtInherit)
#define XfeInheritMinimumGeometry			((XfeGeometryProc)		_XtInherit)
#define XfeInheritMotion					((XfeOrientedProc)		_XtInherit)
#define XfeInheritPreferredGeometry			((XfeGeometryProc)		_XtInherit)
#define XfeInheritToggleSelection			((XtWidgetProc)			_XtInherit)
#define XfeInheritCountLayableChildren		((Boolean)				_XtInherit)
#define XfeInheritUpdateRect				((XtWidgetProc)			_XtInherit)

/*----------------------------------------------------------------------*/
/*																		*/
/* Method inheritance resolution macro									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeResolve(wc,sc,class_name,method,flag)		\
if ((wc)->class_name.method == flag)					\
  (wc)->class_name.method = (sc)->class_name.method
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Converter done macro													*/
/*																		*/
/*----------------------------------------------------------------------*/
#define	_XfeConverertDone(value,type)			\
	{											\
	    if (to->addr != NULL) {					\
		if (to->size < sizeof(type)) {			\
		    to->size = sizeof(type);			\
		    return False;						\
		}										\
		*(type*)(to->addr) = (value);			\
	    }										\
	    else {									\
		static type static_val;					\
		static_val = (value);					\
		to->addr = (XPointer)&static_val;		\
	    }										\
	    to->size = sizeof(type);				\
	    return True;							\
	}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* _XfeIsAlive() macro.													*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeIsAlive(_w) \
((_w) && !_XfeBeingDestroyed(_w))

/*----------------------------------------------------------------------*/
/*																		*/
/* _XfeIsRealized() macro.												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeIsRealized(_w) \
(_XfeIsAlive(_w) && _XfeWindowGood(_XfeWindow(_w)))

/*----------------------------------------------------------------------*/
/*																		*/
/* _XfeSetMappedWhenManaged() macro.									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeSetMappedWhenManaged(_w,_v) \
if (_XfeIsAlive(_w) && XtIsRealized(_w)) \
  XtVaSetValues((_w),XmNmappedWhenManaged,(_v),NULL)

/*----------------------------------------------------------------------*/
/*																		*/
/* Xt Core member access												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeAccelerators(_w)		((_w)->core . accelerators)
#define _XfeAncestorSensitive(_w)	((_w)->core . ancestor_sensitive)
#define _XfeBackgroundPixel(_w)		((_w)->core . background_pixel)
#define _XfeBackgroundPixmap(_w)	((_w)->core . background_pixmap)
#define _XfeBeingDestroyed(_w)		((_w)->core . being_destroyed)
#define _XfeBorderPixel(_w)			((_w)->core . border_pixel)
#define _XfeBorderPixmap(_w)		((_w)->core . border_pixmap)
#define _XfeBorderWidth(_w)			((_w)->core . border_width)
#define _XfeClass(_w)				((_w)->core . widget_class)
#define _XfeColormap(_w)			((_w)->core . colormap)
#define _XfeConstraints(_w)			((_w)->core . constraints)
#define _XfeDepth(_w)				((_w)->core . depth)
#define _XfeHeight(_w)				((_w)->core . height)
#define _XfeIsManaged(_w)			((_w)->core . managed)
#define _XfeMappedWhenManaged(_w)	((_w)->core . mapped_when_managed)
#define _XfeNumPopups(_w)			((_w)->core . num_popups)
#define _XfeParent(_w)				((_w)->core . parent)
#define _XfePopupList(_w)			((_w)->core . popup_list)
#define _XfeScreen(_w)				((_w)->core . screen)
#define _XfeSensitive(_w)			((_w)->core . sensitive)
#define _XfeVisible(_w)				((_w)->core . visible)
#define _XfeWidth(_w)				((_w)->core . width)
#define _XfeWindow(_w)				((_w)->core . window)
#define _XfeX(_w)					((_w)->core . x)
#define _XfeXrmName(_w)				((_w)->core . xrm_name)
#define _XfeY(_w)					((_w)->core . y)

/*----------------------------------------------------------------------*/
/*																		*/
/* Popup list indexing macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePopupListIndex(_w,_i)						\
(((_i < _XfeNumPopups(_w)) && _XfePopupList(_w))		\
? _XfePopupList(_w)[_i] : NULL)

/*----------------------------------------------------------------------*/
/*																		*/
/* Xt WidgetClass member access											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeClassName(wc)			((wc)->core_class . class_name)
#define _XfeSuperClass(wc)			((wc)->core_class . superclass)

/*----------------------------------------------------------------------*/
/*																		*/
/* Xm widget part access macros											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeXmLabelPart(w) \
&(((XmLabelWidget) w) -> label)

#define _XfeXmPushButtonPart(w) \
&(((XmPushButtonWidget) w) -> pushbutton)

#define _XfeXmCascadeButtonPart(w) \
&(((XmCascadeButtonWidget) w) -> cascade_button)

/*----------------------------------------------------------------------*/
/*																		*/
/* Xfe configuration propragation flags									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeConfigNone			(0)
#define XfeConfigGeometry		(1 << 0)
#define XfeConfigLayout			(1 << 1)
#define XfeConfigExpose			(1 << 2)
#define XfeConfigDirty			(1 << 3)

#define XfeConfigGLE			(XfeConfigGeometry		| \
								 XfeConfigLayout		| \
								 XfeConfigExpose)

#define XfeConfigGLED			(XfeConfigGeometry		| \
								 XfeConfigLayout		| \
								 XfeConfigExpose		| \
								 XfeConfigDirty)

#define XfeConfigLED			(XfeConfigLayout		| \
								 XfeConfigExpose		| \
								 XfeConfigDirty)

#define XfeConfigED				(XfeConfigExpose		| \
								 XfeConfigDirty)

#define XfeConfigLE				(XfeConfigLayout		| \
								 XfeConfigExpose)

/*----------------------------------------------------------------------*/
/*																		*/
/* Xfe preparation propragation flags									*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfePrepareNone			(0)
#define XfePrepare1				(1 << 0)
#define XfePrepare2				(1 << 1)
#define XfePrepare3				(1 << 2)
#define XfePrepare4				(1 << 3)
#define XfePrepare5				(1 << 4)
#define XfePrepare6				(1 << 5)
#define XfePrepare7				(1 << 6)
#define XfePrepare8				(1 << 7)
#define XfePrepare9				(1 << 8)
#define XfePrepare10			(1 << 9)
#define XfePrepare11			(1 << 10)
#define XfePrepare12			(1 << 11)
#define XfePrepare13			(1 << 12)
#define XfePrepare14			(1 << 13)
#define XfePrepare15			(1 << 14)
#define XfePrepare16			(1 << 15)
#define XfePrepare17			(1 << 16)
#define XfePrepare18			(1 << 17)
#define XfePrepare19			(1 << 18)
#define XfePrepare20			(1 << 19)
#define XfePrepareAll			(~(0))

/*----------------------------------------------------------------------*/
/*																		*/
/* Widget translations													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern char _XfeArrowDefaultTranslations[];
extern char _XfeBmButtonExtraTranslations[];
extern char _XfeButtonDefaultTranslations[];
extern char _XfeButtonMotionAddTranslations[];
extern char _XfeButtonMotionRemoveTranslations[];
extern char _XfeBypassShellDefaultTranslations[];
extern char _XfeComboBoxArrowTranslations[];
extern char _XfeComboBoxExtraTranslations[];
extern char _XfeComboBoxTextEditableTranslations[];
extern char _XfeComboBoxTextReadOnlyTranslations[];
extern char _XfeLabelDefaultTranslations[];
extern char _XfePrimitiveDefaultTranslations[];
extern char _XfeToolBarExtraTranslations[];
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Callbacks															*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeInvokeCallbacks			(Widget				w,
							 XtCallbackList		list,
							 int				reason,
							 XEvent *			event,
							 Boolean			flush_display);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Double Buffer utils													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Pixmap
_XfePixmapBufferAllocate		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
_XfePixmapBufferRelease			(Widget			w,
								 Pixmap			buffer);
/*----------------------------------------------------------------------*/
extern Pixmap
_XfePixmapBufferAccess			(void);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Warnings																*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeSimpleWarning				(Widget			w,
								 String			warning);
/*----------------------------------------------------------------------*/
extern void
_XfeExtraWarning				(Widget			w,
								 String			warning,
								 String			filename,
								 Cardinal		lineno);
/*----------------------------------------------------------------------*/
extern void
_XfeArgumentWarning				(Widget			w,
								 String			format,
								 XtPointer		argument,
								 String			filename,
								 Cardinal		lineno);
/*----------------------------------------------------------------------*/
#define _XfeWarning(w,warning) \
_XfeExtraWarning((w),(warning),__FILE__,__LINE__)
/*----------------------------------------------------------------------*/
#define _XfeArgWarning(w,format,arg) \
_XfeArgumentWarning((w),(format),(XtPointer) (arg),__FILE__,__LINE__)
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Check read only resources in Initialize()							*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeInitializeCheckReadOnly(_w,_np,_v,_d,_m)	\
{														\
	if (_np -> _v)										\
	{													\
		_np -> _v = _d;									\
														\
		_XfeWarning(_w,_m);								\
	}													\
}

/*----------------------------------------------------------------------*/
/*																		*/
/* Check read only resources in SetValues()								*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeSetValuesCheckReadOnly(_w,_np,_op,_v,_m)	\
{														\
	if (_np -> _v != _op -> _v)							\
	{													\
		_np -> _v = _op -> _v;							\
														\
		_XfeWarning(_w,_m);								\
	}													\
}

/*----------------------------------------------------------------------*/
/*																		*/
/* Check for good pixmaps												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfePixmapGood(_p) \
(((_p) != XmUNSPECIFIED_PIXMAP) && ((_p) != None))

/*----------------------------------------------------------------------*/
/*																		*/
/* Check for good cursors												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeCursorGood(_c) \
((_c) != None)

/*----------------------------------------------------------------------*/
/*																		*/
/* Check for good window												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeWindowGood(_window) \
((_window) != None)

/*----------------------------------------------------------------------*/
/*																		*/
/* Resource call procedures												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfeCallProcCopyBackground		(Widget			w,
								 int			offset,
								 XrmValue *		value);
/*----------------------------------------------------------------------*/
extern void
_XfeCallProcCopyForeground		(Widget			w,
								 int			offset,
								 XrmValue *		value);
/*----------------------------------------------------------------------*/
extern void
_XfeCallProcCopyShadowThickness	(Widget			w,
								 int			offset,
								 XrmValue *		value);
/*----------------------------------------------------------------------*/
extern void
_XfeCallProcSelectPixel			(Widget			w,
								 int			offset,
								 XrmValue *		value);

/*----------------------------------------------------------------------*/
extern void
_XfeCallProcOrientationCursor	(Widget			w,
								 int			offset,
								 XrmValue *		value);
/*----------------------------------------------------------------------*/
extern void
_XfeCallProcDefaultLabelFontList(Widget			w,
								 int			offset,
								 XrmValue *		value);
/*----------------------------------------------------------------------*/
extern void
_XfeCallProcDefaultTextFontList	(Widget			w,
								 int			offset,
								 XrmValue *		value);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Synthetic resource import procedures									*/
/*																		*/
/*----------------------------------------------------------------------*/
extern XmImportOperator
_XfeSyntheticSetResourceForChild	(Widget		w,
									 int		offset,
									 XtArgVal *	value);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Actions																*/
/*																		*/
/*----------------------------------------------------------------------*/
extern XtActionProc
_XfeGetActionProc				(WidgetClass	wc,
								 String			name);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Preparation utlities													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
_XfePixmapPrepare				(Widget			w,
								 Pixmap *		pixmap_in_out,
								 Dimension *	width_out,
								 Dimension *	height_out,
								 String			name);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Privat functions not exposed in motif 2.0							*/
/*																		*/
/*----------------------------------------------------------------------*/
#if XmVersion >= 2000
extern void	_XmMenuPopDown				(Widget,XEvent *,Boolean *);
extern void	_XmMoveObject				(Widget,Position,Position);

extern void	_XmPostPopupMenu			(Widget,XEvent *);

extern void	_XmPrimitiveEnter			(Widget,XEvent *,String *,Cardinal *);
extern void	_XmPrimitiveLeave			(Widget,XEvent *,String *,Cardinal *);

extern void	_XmPopdown					(Widget);
extern void	_XmPopupSpringLoaded		(Widget);
extern void	_XmPopup					(Widget,XtGrabKind);

extern void	_XmBottomShadowColorDefault	(Widget,int,XrmValue *);
extern void	_XmTopShadowColorDefault	(Widget,int,XrmValue *);

extern XtGeometryResult	_XmMakeGeometryRequest	(Widget,XtWidgetGeometry *);

#endif /* XmVersion >= 2000 */

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end XfeP.h			*/
