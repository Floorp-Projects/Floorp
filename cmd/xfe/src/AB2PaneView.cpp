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
   ABListSearchView.cpp -- view of user's mailfilters.
   Created: Tao Cheng <tao@netscape.com>, 14-oct-97
 */

#include "AB2PaneView.h"
#include "ABDirListView.h"
#include "AddrBookView.h"
#include "ABAddrSearchView.h"

#include <Xfe/Pane.h>

XFE_AB2PaneView::XFE_AB2PaneView(XFE_Component *toplevel_component, 
								 Widget         parent, 
								 XFE_View      *parent_view, 
								 MWContext     *context,
								 eABViewMode    mode):
	XFE_View(toplevel_component, parent_view, context)
{
	m_focusedView = NULL;
	// data
	m_expanded = (mode==AB_PICKER?False:True);
	m_dir = 0;
	m_directories = FE_GetDirServers();
	if (m_directories)
		m_nDirs = XP_ListCount(m_directories);
	else
		m_nDirs = 0;

	// UI
	/* let's make a container here
	 */
	Widget paneContainerForm = XtVaCreateManagedWidget("paneContainerForm",
													   xmFormWidgetClass,
													   parent,
													   NULL);

	
	Widget hpane;

	hpane = XtVaCreateWidget("hpane",
							 xfePaneWidgetClass,
							 paneContainerForm,
							 XmNorientation,			XmHORIZONTAL,
							 XmNsashPosition,			200,
							 XmNsashThickness,			10,
							 XmNsashShadowThickness,	1,
							 XmNpaneSashType,			XmPANE_SASH_LIVE,
							 NULL);

	// dir list
	m_dirListView = new XFE_ABDirListView(toplevel_component, 
										  hpane, 
										  this,
										  context,
										  m_directories);

	// register
	m_dirListView->registerInterest(XFE_ABDirListView::dirCollapse,
									this,
									(XFE_FunctionNotification)dirCollapse_cb);

	m_dirListView->registerInterest(XFE_ABDirListView::dirSelect,
									this,
									(XFE_FunctionNotification)dirSelect_cb);

	m_dirListView->registerInterest(XFE_MNListView::changeFocus,
									this,
									(XFE_FunctionNotification)changeFocus_cb);

	// entry list
	if (mode == AB_BOOK)
		m_entriesListView = (XFE_ABListSearchView *) 
			new XFE_AddrBookView(toplevel_component, 
								 hpane,
								 this,
								 context,
								 m_directories);
	else 
		m_entriesListView = (XFE_ABListSearchView *) 
			new XFE_AddrSearchView(toplevel_component, 
								   hpane,
								   this,
								   context,
								   m_directories);


	/* get outliner
	 */
	XFE_Outliner *outliner = m_entriesListView->getOutliner();
	XP_ASSERT(outliner);

	// initialize the icons if they haven't already been
	Pixel bg_pixel;
	
	XtVaGetValues(outliner->getBaseWidget(), XmNbackground, &bg_pixel, 0);
	if (!XFE_ABListSearchView::m_personIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(outliner->getBaseWidget())),
					   bg_pixel,
					   &XFE_ABListSearchView::m_personIcon,
					   NULL, 
					   MN_Person.width, 
					   MN_Person.height,
					   MN_Person.mono_bits, 
					   MN_Person.color_bits, 
					   MN_Person.mask_bits, 
					   FALSE);
  
	if (!XFE_ABListSearchView::m_listIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(outliner->getBaseWidget())),
					   bg_pixel,
					   &XFE_ABListSearchView::m_listIcon,
					   NULL, 
					   MN_People.width, 
					   MN_People.height,
					   MN_People.mono_bits, 
					   MN_People.color_bits, 
					   MN_People.mask_bits, 
					   FALSE);

#if defined(USE_MOTIF_DND)
	outliner->enableDragDrop(m_entriesListView,
							 XFE_ABListSearchView::getDropTargets,
							 XFE_ABListSearchView::getDragTargets,
							 XFE_ABListSearchView::getDragIconData,
							 XFE_ABListSearchView::dragConvert,
							 XFE_ABListSearchView::processTargets);

	outliner = m_dirListView->getOutliner();
	outliner->enableDragDrop(m_dirListView,
							 XFE_ABDirListView::getDropTargets,
							 XFE_ABDirListView::getDragTargets,
							 XFE_ABDirListView::getDragIconData,
							 XFE_ABDirListView::dragConvert,
							 XFE_ABDirListView::processTargets);


#else

 /* to replaced by new Dnd
	   */

	/* enable m_entriesListView drag & drop 
	 */
	outliner->setDragType(FE_DND_ADDRESSBOOK, 
						  &XFE_ABListSearchView::m_personIcon, 
						  m_entriesListView);

	fe_dnd_CreateDrop(outliner->getBaseWidget(), 
					  &XFE_ABListSearchView::entryListDropCallback, 
					  m_entriesListView);

	/* enable m_dirListView drag & drop 
	 */
	outliner = m_dirListView->getOutliner();
	outliner->setDragType(FE_DND_BOOKS_DIRECTORIES,
						  &XFE_ABListSearchView::m_listIcon, 
						  m_dirListView);

	fe_dnd_CreateDrop(outliner->getBaseWidget(), 
					  &XFE_ABDirListView::dirListDropCallback, 
					  m_dirListView);
#endif

	// register
	m_entriesListView->registerInterest(XFE_ABListSearchView::dirExpand,
										this,
										(XFE_FunctionNotification)dirExpand_cb);

	m_entriesListView->registerInterest(XFE_ABListSearchView::dirsChanged,
										this,
										(XFE_FunctionNotification)dirsChanged_cb);

	m_entriesListView->registerInterest(XFE_ABListSearchView::dirSelect,
										this,
										(XFE_FunctionNotification)dirSelect_cb);

	m_entriesListView->registerInterest(XFE_MNListView::changeFocus,
										this,
										(XFE_FunctionNotification)changeFocus_cb);

	// add our subviews to the list of subviews for command dispatching and
	// deletion.
	addView(m_dirListView);
	addView(m_entriesListView);
	m_entriesListView->show();
	
	//
#if defined(USE_ABCOM)
	MSG_Pane *abContainerPane = m_dirListView->getPane();
    m_entriesListView->setContainerPane(abContainerPane);

	/* get all root containers
	 */
	uint32 nDirs = 0;
	int error = AB_GetNumRootContainers(abContainerPane,
										(int32 *) &nDirs);
	m_rootContainers = 0;
	if (nDirs) {
		m_nDirs = nDirs;
		m_rootContainers = 
			(AB_ContainerInfo **) XP_CALLOC(nDirs, 
											sizeof(AB_ContainerInfo *));
		
		error = 
			AB_GetOrderedRootContainers(abContainerPane,
										/* FE Allocated & Freed */
										m_rootContainers, 
										/* in  - # of elements in ctrArray.
										 * out - BE fills with # root 
										 * containers stored in ctrArray */
										(int32 *) &nDirs);
	}/* if */

#endif /* USE_ABCOM */
	Widget filterBoxForm = 
		m_entriesListView->makeFilterBox(paneContainerForm, 
										 (mode==AB_PICKER)?True:False);

	if (m_nDirs)
#if defined(USE_ABCOM)
		m_entriesListView->selectContainer(m_rootContainers[0]);
#else
	    m_entriesListView->changeEntryCount();
#endif
	expandCollapse(m_expanded);
	XtManageChild(hpane);
	setBaseWidget(paneContainerForm);

	//
	XtVaSetValues(hpane,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, filterBoxForm,
				  XmNtopOffset, 4,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);

}

XFE_AB2PaneView::~XFE_AB2PaneView()
{
	// unregister
	m_dirListView->unregisterInterest(XFE_ABDirListView::dirCollapse,
									  this,
									  (XFE_FunctionNotification)dirCollapse_cb);

	m_dirListView->unregisterInterest(XFE_ABDirListView::dirSelect,
									  this,
									  (XFE_FunctionNotification)dirSelect_cb);
	//
	m_entriesListView->unregisterInterest(XFE_ABListSearchView::dirExpand,
										  this,
										  (XFE_FunctionNotification)dirExpand_cb);
	m_entriesListView->unregisterInterest(XFE_ABListSearchView::dirsChanged,
										  this,
										  (XFE_FunctionNotification)dirsChanged_cb);
	m_entriesListView->unregisterInterest(XFE_ABListSearchView::dirSelect,
										  this,
										  (XFE_FunctionNotification)dirSelect_cb);

#if defined(USE_ABCOM)
	// Free heap
	XP_FREEIF(m_rootContainers);
#endif /* USE_ABCOM */

}

//
Boolean 
XFE_AB2PaneView::isCommandEnabled(CommandType command, 
								  void *calldata,
								  XFE_CommandInfo* i)
{
	if (m_focusedView)
		return m_focusedView->isCommandEnabled(command, calldata, i);
	return FALSE;
}

Boolean 
XFE_AB2PaneView::isCommandSelected(CommandType command, 
								   void *calldata,
								   XFE_CommandInfo* i)
{
	if (m_focusedView)
		return m_focusedView->isCommandSelected(command, calldata, i);
	return FALSE;
}

Boolean 
XFE_AB2PaneView::handlesCommand(CommandType command, 
								void *calldata,
								XFE_CommandInfo* i)
{
	if (m_focusedView)
		return m_focusedView->handlesCommand(command, calldata, i);
	return FALSE;
}

void 
XFE_AB2PaneView::doCommand(CommandType command, 
						   void *calldata,
						   XFE_CommandInfo* i)
{
	if (m_focusedView)
		m_focusedView->doCommand(command, calldata, i);
}

//
XFE_CALLBACK_DEFN(XFE_AB2PaneView, dirCollapse)(XFE_NotificationCenter */*obj*/, 
												void */*clientData*/, 
												void */* callData */)
{
	expandCollapse();
}

XFE_CALLBACK_DEFN(XFE_AB2PaneView, dirExpand)(XFE_NotificationCenter */*obj*/, 
											  void */*clientData*/, 
											  void */* callData */)
{
	expandCollapse();
}

XFE_CALLBACK_DEFN(XFE_AB2PaneView, dirSelect)(XFE_NotificationCenter */*obj*/, 
											  void */*clientData*/, 
											  void *callData)
{
#if defined(USE_ABCOM)
	AB_ContainerInfo *containerInfo = (AB_ContainerInfo *) callData;
	if (m_entriesListView) {
		m_entriesListView->selectContainer(containerInfo);		
	}/* if */

	if (m_dirListView) {
		m_dirListView->selectContainer(containerInfo);		
	}/* if */
#else
	DIR_Server *dir = (DIR_Server *) callData;
	if (m_entriesListView)
		m_entriesListView->selectDir(dir);
#endif
}

XFE_CALLBACK_DEFN(XFE_AB2PaneView, dirsChanged)(XFE_NotificationCenter */*obj*/, 
											  void */*clientData*/, 
											  void *callData)
{
	XP_List *dirs = (XP_List *) callData;
	if (m_dirListView)
		m_dirListView->setDirServers(dirs);
}

XFE_CALLBACK_DEFN(XFE_AB2PaneView, changeFocus)(XFE_NotificationCenter *,
												void *, void* calldata)
{	
	XP_ASSERT(calldata != NULL);
	XFE_MNListView *focusedView = (XFE_MNListView *) calldata;

	if (m_focusedView != focusedView) {
	    if (m_focusedView)
			m_focusedView->setInFocus(False);

	    focusedView->setInFocus(True);
	    m_focusedView = focusedView;

		//?? duplicate???
	    getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
	}/* if */
}

// callbacks
void 
XFE_AB2PaneView::propertiesCallback(Widget w, XtPointer clientData, 
									XtPointer callData)
{
  XFE_AB2PaneView *obj = (XFE_AB2PaneView *) clientData;
  obj->propertiesCB(w, callData);
}

void 
XFE_AB2PaneView::propertiesCB(Widget, XtPointer)
{
	if (m_focusedView == m_dirListView)
		m_dirListView->propertiesCB();
	else if (m_focusedView == m_entriesListView)
		m_entriesListView->propertiesCB();
}

// 
void XFE_AB2PaneView::expandCollapse()
{
	m_expanded = m_expanded?False:True;
	expandCollapse(m_expanded);
}

void XFE_AB2PaneView::expandCollapse(XP_Bool expand)
{
	if (m_dirListView)
		m_dirListView->expandCollapse(expand);

	if (m_entriesListView)
		m_entriesListView->expandCollapse(expand);
}

//
void XFE_AB2PaneView::selectLine(int line)
{
	if (m_dirListView)
		m_dirListView->selectLine(line);

	if (m_entriesListView)
		m_entriesListView->selectLine(line);
}

void XFE_AB2PaneView::selectDir(DIR_Server* dir)
{
	if (m_dirListView)
		m_dirListView->selectDir(dir);

	if (m_entriesListView)
		m_entriesListView->selectDir(dir);
}

#if defined(USE_ABCOM)
const AB_ContainerInfo**
XFE_AB2PaneView::getRootContainers(uint32 &count) const {
	count = m_nDirs;
	return m_rootContainers;
}
#endif /* USE_ABCOM */

