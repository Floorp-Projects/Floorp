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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// CAddressBookWindows.h

#pragma once
#include "abcom.H"
#ifdef MOZ_NEWADDR
class CComposeAddressTableView;
/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CMailNewsWindow.h"
#include "CMailNewsContext.h"
#include "LGADialogBox.h"
#include  "MailNewsCallbacks.h"


class CSearchEditField;
class LBroadcasterEditField;
class CNamePropertiesWindow;
class CListPropertiesWindow;
class CMailingListTableView;
class CAddressBookController;
#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/
typedef struct DIR_Server DIR_Server;
typedef struct _XP_List XP_List;
typedef UInt32 ABID;



#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

// Save window status version

static const UInt16 cAddressSaveWindowStatusVersion = 	0x0219;
static const UInt16 cNamePropertiesSaveWindowStatusVersion = 	0x0202;
static const UInt16 cListPropertiesSaveWindowStatusVersion = 	0x0202;

#pragma mark -

extern "C"
{
int MacFe_ShowModelessPropertySheetForAB2( MSG_Pane * pane,  MWContext*  inContext);
int MacFE_ShowPropertySheetForDir( 
			DIR_Server* server, MWContext*  context, MSG_Pane * srcPane, XP_Bool  newDirectory );
}
class CAddressBookWindow;


class CAddressBookManager
{
public:

							// Should be called when the application starts up
	static void				OpenAddressBookManager(void);
							// Should be called when the application closes
	static void				CloseAddressBookManager(void);
	
	static void				ImportLDIF(const FSSpec& inSpec);

	static CAddressBookWindow*	ShowAddressBookWindow(void);
	
	static XP_List			*GetDirServerList(void);
	static void				SetDirServerList(XP_List *inList, Boolean inSavePrefs = true);
	static DIR_Server		*GetPersonalBook(void);
	
	static void 			FailAddressError(Int32 inError);


private:

	static void				RegisterAddressBookClasses(void);
	static int				DirServerListChanged(const char*, void*)
	{
		CAddressBookManager::sDirServerListChanged = true;
		return 0;
	}

	// Instance variables
	
	static XP_List			*sDirServerList;
	static Boolean			sDirServerListChanged;
	
};

class CAddressBookWindow : public CMailNewsWindow
{
private:
	typedef CMailNewsWindow Inherited;
					  
public:

	enum { class_ID = 'AbWn', pane_ID = class_ID, res_ID = 8900 };

	// IDs for panes in associated view, also messages that are broadcast to this object
	enum {
		paneID_DirServers = 'DRSR'			// CDirServersPopupMenu *, 	this
	,	paneID_Search = 'SRCH'				// MSG_Pane *, 	search button
	,	paneID_Stop = 'STOP'				// nil, 		stop button
	,	paneID_AddressBookTable = 'Tabl'	// Address book table
	,	paneID_TypedownName = 'TYPE'		// Typedown name search edit field in window
	,	paneID_SearchEnclosure = 'SCHE'		// Enclosure for search items
	, 	paneID_AddressBookController = 'AbCn'
	};
					
	// Stream creator method

						CAddressBookWindow(LStream *inStream) : 
							 		  	   CMailNewsWindow(inStream, WindowType_Address),
									  	   mAddressBookController(nil)
										   {
							SetPaneID(pane_ID);
							SetRefreshAllWhenResized(false);
						}
	virtual				~CAddressBookWindow();
						
	virtual ResIDT		GetStatusResID() const { return res_ID; }
	
	static MWContext				*GetMailContext();
	virtual CNSContext* 		CreateContext() const;
	virtual CMailFlexTable*			GetActiveTable();
	
	CAddressBookController* 		GetAddressBookController() const { return mAddressBookController; }
protected:

	// Overriden methods

	virtual void		FinishCreateSelf();	
	
	// Utility methods
	
	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
	virtual UInt16		GetValidStatusVersion() const { return cAddressSaveWindowStatusVersion; }

protected:
	CAddressBookController 	*mAddressBookController;
};


class CAddressBookChildWindow : public LGADialogBox
{					  
private:
	typedef LGADialogBox Inherited;

public:
						CAddressBookChildWindow(LStream *inStream) : 
			 		  	   	    Inherited (inStream), mMSGPane( NULL )
			 		  	   	   {
								SetRefreshAllWhenResized(false);
							}

	virtual void		UpdateBackendToUI() = 0L;
	virtual void		UpdateUIToBackend( MSG_Pane* inPane ) = 0L;
												 
protected:
	// Overriden methods
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	virtual void		UpdateTitle()=0;
	// Instance variables
	MSG_Pane*				mMSGPane;			
};										

class CListPropertiesWindow : public CAddressBookChildWindow {
					  
private:
	typedef CAddressBookChildWindow Inherited;

public:
		enum { class_ID = 'LpWn', pane_ID = class_ID, res_ID = 8940 };

		// IDs for panes in associated view, also messages that are broadcast to this object
		enum {
			paneID_Name = 'NAME'
		,	paneID_Nickname = 'NICK'
		,	paneID_Description = 'DESC'
		,	paneID_AddressBookListTable = 'Tabl'	// Address book list table
		};

						CListPropertiesWindow(LStream *inStream);
	virtual				~CListPropertiesWindow();
						
	virtual	void		UpdateBackendToUI();
	virtual void		UpdateUIToBackend( MSG_Pane* inPane );

protected:
	virtual void		FinishCreateSelf();
	virtual void		DrawSelf();
	virtual void		UpdateTitle();
	
	// Instance variables
	CMailingListTableView	*mAddressBookListTable;
	CSearchEditField		*mTitleField;
};

class CMailWindowCallbackListener: public CMailCallbackListener
{
	void CMailWindowCallBackListener( LWindow* inWindow )
	{ 
		mWindow = inWindow;
	}
private:
	virtual void 	PaneChanged(
		MSG_Pane*		inPane,
		MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
		int32 value);
	LWindow *mWindow;
};

//------------------------------------------------------------------------------
//	¥	CNamePropertiesWindow
//------------------------------------------------------------------------------
//
class CNamePropertiesWindow :  public CAddressBookChildWindow
{
private:
	typedef CAddressBookChildWindow Inherited;

public:

		enum { class_ID = 'NpWn', pane_ID = class_ID, res_ID = 8930 };

		// IDs for panes in associated view, also messages that are broadcast to this object
		enum {
			paneID_GeneralView = 'GNVW'			// General preferences view
		,		paneID_FirstName = 'FNAM'
		,		paneID_LastName = 'LNAM'
		,		paneID_DisplayName = 'DNAM'
		,		paneID_EMail = 'EMAL'
		,		paneID_Nickname = 'NICK'
		,		paneID_Notes = 'NOTE'
		,		paneID_PrefersHTML = 'HTML'
		,	paneID_ContactView = 'CNVW'			// Contact preferences view
		,		paneID_Company = 'COMP'
		,		paneID_Title = 'TITL'
		,		paneID_Department = 'DPRT'
		,		paneID_Address = 'ADDR'
		,		paneID_City = 'CITY'
		,		paneID_State = 'STAT'
		,		paneID_ZIP = 'ZIP '
		, 		paneID_Country = 'Coun'
		,		paneID_WorkPhone = 'WPHO'
		,		paneID_FaxPhone = 'FPHO'
		,		paneID_PagerPhone = 'PPHO'
		,		paneID_HomePhone = 'HPHO'
		,		paneID_CelluarPhone = 'CPHO'
		,	paneID_SecurityView = 'SCVW'		// Security preferences view
		,	paneID_CooltalkView = 'CLVW'		// Cooltalk preferences view
		,		paneID_ConferenceAddress = 'CAED'
		,		paneID_ConferenceServer	 = 'CnPu'
		,	paneID_None	= 'NONE'
		};
		
		enum {	// Broadcast messages
				paneID_ConferencePopup ='CoPU'	// conference pop up button
		};

						CNamePropertiesWindow(LStream *inStream);
	virtual void		UpdateBackendToUI();
	void				UpdateUIToBackend( MSG_Pane *inPane );
	void				SetConferenceText( );
	
protected:
	virtual void		FinishCreateSelf();
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	virtual void		UpdateTitle();

private:
	int32 				GetPaneAndAttribID( TableIndexT index, PaneIDT& outPaneID, AB_AttribID &outAttrib );
	PaneIDT				FindPaneForAttribute ( AB_AttribID inAttrib );
protected:	
	CMailWindowCallbackListener mCallBackListener;
	LBroadcasterEditField		*mFirstNameField;
	LBroadcasterEditField		*mLastNameField;
};
#endif // NEWADDR