//	============================================================================
//	¥¥¥	QAP_AssistPP.cp
//	============================================================================
//		QA Partner/Macintosh Driver Assistance Hook for 
//		PowerPlant Class Library applications
//		
//		Copyright © 1993-1997 Segue Software, Inc.
//		All Rights Reserved.
//		
//		QA PARTNER RELEASE VERSION 4.0 BETA 1
//		THIS IS A BETA RELEASE.  THIS SOFTWARE MAY HAVE BUGS.  THIS SOFTWARE MAY CHANGE BEFORE FINAL RELEASE.

/*

To use this assistance hook with your PowerPlant application, do the following:

1. 	#include "QAP_Assist.h" in the source file(s) in which you do steps 2 and 3 below.

2. 	In your main () function, add calls to the QAP_AssistHook before and after your
   	application's "Run" method, as follows

	...
	QAP_AssistHook (kQAPAppToForeground, 0, NULL, 0, 0);
	YourApplication->Run();
	QAP_AssistHook (kQAPAppToBackground, 0, NULL, 0, 0);
	...
	
3. 	Sub-class LCommander::PutOnDuty and LCommander::TakeOffDuty (if you havn't already),
   	and add the following to them:

	void CYourApplication::PutOnDuty ()
	{
		QAP_AssistHook (kQAPAppToForeground, 0, NULL, 0, 0);
		LCommander::PutOnDuty ();
	}
	
	void CYourApplication::TakeOffDuty ()
	{
		LCommander::TakeOffDuty ();
		QAP_AssistHook (kQAPAppToBackground, 0, NULL, 0, 0);
	}
   
4. 	The assist hook now uses RTTI to resolve class id at runtime.  Make sure RTTI 
	is enabled in your compiler preferences.  Also, ensure that the LView static 
	class variable LView::sInFocusView and the instance variable LView::mSubPanes 
	are visible by tweaking 'LView.h' to declare them public.
	
5. 	Add QAP_AssistPP.cp to your project and rebuild.

*/

#include "QAP_Assist.h"
#ifdef QAP_BUILD
#include <string.h>

#include <LButton.h>
#include <LCaption.h>
#include <LCicnButton.h>
#include <LControl.h>
#include <LEditField.h>
#include <LGroupBox.h>
#include <LIconPane.h>
#include <LListBox.h>
#include <LArrayIterator.h>		//¥NETSCAPE: was LListIterator.h
#include <LPicture.h>
#include <LPlaceHolder.h>
#include <LStdControl.h>
#include <LTextButton.h>
#include <LTextEdit.h>
#include <LToggleButton.h>
#include <LView.h>
#include <LWindow.h>

#ifndef PARTNER // Segue internal use
#  include <LGACheckBox.h>
#  include <LGADisclosureTriangle.h>
#  include <LGAPopup.h>
#  include <LGAIconButtonPopup.h>
#  include <LGAIconButton.h>
#  include <LGAPushButton.h>
#  include <LGATextButton.h>
#  include <LGARadioButton.h>
#endif

#ifdef PARTNER // Segue internal use
#  include "ZButton.h"
#endif

#include "CButton.h"		//¥NETSCAPE: custom classes
#include "CPatternButtonPopup.h"

#ifndef FALSE				//¥NETSCAPE: duh?
#define FALSE 0
#endif

							//¥NETSCAPE:
							// When the driver calls back QAP_AssistHook with the
							// 'kQAPGetListContents' selector, it sets a wrong
							// LPane* in 'l_handle'. This enables a workaround:
#define QAP_V4_DRIVER_WORKAROUND

#pragma segment main

static short SetAssistHook (QAPAssistHookUPP assistHookUPP);
static void AddViewItem (LPane * lpanep, PWCINFO wcp, short * sp_count, short s_type, short s_cls, Handle h);
static short GetWindowContents (WindowPtr winp, PWCINFO wcp, short s_max);
static short GetListInfo (LPane * lpanep, PQAPLISTINFO p_buffer);				//¥NETSCAPE: added
static short GetListContents (LPane * lpanep, Ptr p_buffer, short s_val);		//¥NETSCAPE: added
static short LPaneGetContents (LPane * lpanep, PWCINFO wcp, short * sp_count, short s_max);
static short LPaneGetTextInfo (LPane * lpanep, PTEXTINFO textInfop);
static short LPaneGetCustomItemName (LPane * lpanep, char * cp_buf);
static short LPaneGetCustomItemValue (LPane * lpanep, long * l_value);

// NOTE: assumes that <MacHeaders> is automatically included

//	----------------------------------------------------------------------------
//	¥	QAP_AssistHook
//	----------------------------------------------------------------------------
//		QAP_AssistHook is the actual assistance hook callback.
//
//¥NETSCAPE:
//	- The kQAPGetListInfo and kQAPGetListContents cases were out-commented.
//	- A5 was set/restored on each individual case and the GrafPort was not saved at all.
//	  We now do that at the beginning and the end of the function. Saving the GrafPort
//	  is necessary because of LPaneGetContents() => AddViewItem() => LPane::FocusDraw().

pascal short QAP_AssistHook (short s_selector, long l_handle, void * p_buffer, short s_val, long l_inAppA5)
{
	short s_result;
	long l_saveA5;
	GrafPtr savePort;

	// Set my A5 and grafport
	if (l_inAppA5 != nil)
	{
		l_saveA5 = SetA5 (l_inAppA5);	// line order...
		::GetPort(&savePort);			// ...does matter
	}

	/* Dispatch to the appropiate function and return the result. */
	
	switch (s_selector)
	{
	case kQAPAppToForeground:
		s_result = SetAssistHook (NewQAPAssistHook (QAP_AssistHook));
		break;

	case kQAPAppToBackground:
		s_result = SetAssistHook (NULL);
		break;

	case kQAPGetWindowContents:
		s_result = GetWindowContents ((WindowPtr) l_handle, (PWCINFO) p_buffer, s_val);
		break;

/*
	case kQAPGetCustomItemName:
		LPaneGetCustomItemName ((LPane *) l_handle, (char *) p_buffer);
		break;
*/	
	case kQAPGetCustomItemValue:
		LPaneGetCustomItemValue ((LPane *) l_handle, (long *) p_buffer);
		break;
		
	case kQAPGetTextInfo:
		s_result = LPaneGetTextInfo ((LPane *) l_handle, (PTEXTINFO) p_buffer);
		break;

	case kQAPGetListInfo:
		//	PowerPlant LListBox uses the Mac Toolbox ListManager, so QAP gets
		//	the information it needs from the ListHandle.  If you use custom 
		//	ListBoxes, this selector would be called to retrieve information about the ListBox.
		s_result = GetListInfo ((LPane *) l_handle, (PQAPLISTINFO) p_buffer);
		break;

	case kQAPGetListContents:
		//	PowerPlant LListBox uses the Mac Toolbox ListManager, so QAP gets
		//	the information it needs from the ListHandle.  If you use custom 
		//	ListBoxes, this selector would be called to retrieve the contents the ListBox.
		s_result = GetListContents ((LPane *) l_handle, (Ptr) p_buffer, s_val);
		break;

	default:
		s_result = 0;
		break;
	}

	// restore A5 and grafport
	if (l_inAppA5 != nil)
	{
		::SetPort(savePort);			// line order...
		l_saveA5 = SetA5 (l_saveA5);	// ...does matter
	}

	return s_result;
}

//	----------------------------------------------------------------------------
//	¥	SetAssistHook
//	----------------------------------------------------------------------------
//		SetAssistHook makes the appropriate PBControl call to the QA Partner driver to install
//	  	the assistance hook callback

static short SetAssistHook (QAPAssistHookUPP assistHookUPP)
{
	CntrlParam cp;
	OSErr osErr;
	
	if ((osErr = OpenDriver (QAP_DRIVER_NAME, &cp.ioCRefNum)) != 0)
		return osErr;

	cp.ioNamePtr = NULL;
	cp.ioVRefNum = 0;
	cp.csCode = QAP_SET_ASSIST_HOOK;
	* (QAPAssistHookUPP *) & cp.csParam[0] = assistHookUPP;
	* (long *) & cp.csParam[2] = (long) SetCurrentA5 ();

	if ((osErr = PBControlSync ((ParmBlkPtr) & cp)) != 0)
		return osErr;

	return 0;
}

//	----------------------------------------------------------------------------
//	¥	AddViewItem
//	----------------------------------------------------------------------------
//		AddViewItem adds a given item to the window contents list.

static void AddViewItem (LPane * lpanep, PWCINFO wcp, short * sp_count, short s_type, short s_cls, char * cp_name, Handle h)
{
	wcp += * sp_count;
	++ * sp_count;
	wcp->type = s_type;
	wcp->cls = s_cls;
	wcp->handle = h;
	
	if (cp_name)
		strncpy (wcp->str, cp_name, MAC_NAME_SIZE-1);
		
	if (lpanep)
	{
		lpanep->FocusDraw ();
		lpanep->CalcPortFrameRect (wcp->rect);
		wcp->flags = (lpanep->IsEnabled () ? 0 : WCF_DISABLED);
	}
}

//	----------------------------------------------------------------------------
//	¥	GetWindowContents
//	----------------------------------------------------------------------------
//		GetWindowContents is called by the assistance hook to fill in the 
//		window contents structures for all the relevant views in a given window.

static short GetWindowContents (WindowPtr winp, PWCINFO wcp, short s_max)
{
	LWindow * lwindowp;
	short s_count = 0;
	LView * lviewp_saveFocus;
	
	lviewp_saveFocus = LView::GetInFocusView();		//¥NETSCAPE: was LView::sInFocusView;
	
	lwindowp = LWindow::FetchWindowObject (winp);

	if (lwindowp != nil)
		LPaneGetContents (lwindowp, wcp, & s_count, s_max);
	
	//	The following call tells the QAP Agent that the list provided is incomplete.
	//	The Agent will go ahead and perform its usual traversal of toolbox data structures.	
	//	If you do not want to perform this search, comment this line out.
	
	AddViewItem (0, wcp, & s_count, WT_INCOMPLETE, 0, NULL, 0);		
																																
	if (lviewp_saveFocus)
		lviewp_saveFocus->FocusDraw ();
	
	return s_count;
}

//	----------------------------------------------------------------------------
//	¥	GetListInfo			//¥NETSCAPE: added
//	----------------------------------------------------------------------------
static short GetListInfo (LPane * lpanep, PQAPLISTINFO p_buffer)
{
#ifdef QAP_V4_DRIVER_WORKAROUND
	lpanep = (LPane *)(((long*)lpanep)[4]);
#endif //QAP_V4_DRIVER_WORKAROUND
	CQAPartnerTableMixin* qaTable = dynamic_cast <CQAPartnerTableMixin*> (lpanep);
	if (qaTable != NULL)
		qaTable->QapGetListInfo(p_buffer);
	return 0;	
}

//	----------------------------------------------------------------------------
//	¥	GetListContents		//¥NETSCAPE: added
//	----------------------------------------------------------------------------
static short GetListContents (LPane * lpanep, Ptr p_buffer, short s_val)
{
#ifdef QAP_V4_DRIVER_WORKAROUND
	lpanep = (LPane *)(((long*)lpanep)[4]);
#endif //QAP_V4_DRIVER_WORKAROUND
	CQAPartnerTableMixin* qaTable = dynamic_cast <CQAPartnerTableMixin*> (lpanep);
	if (qaTable != NULL)
		return (qaTable->QapGetListContents(p_buffer, s_val));
	return 0;	
}


//	----------------------------------------------------------------------------
//	¥	LPaneGetContents
//	----------------------------------------------------------------------------
//		LPaneGetContents is called by GetWindowContents 
//		and recursively by itself to fill in some window contents structures 
//		for a given LPane.

static short LPaneGetContents (LPane * lpanep, PWCINFO wcp, short * sp_count, short s_max)
{
	LView * lviewp;
	LEditField * leditFieldp;
	LListBox * llistBoxp;
	LStdControl * lstdControlp;
	LTextEdit * ltextEditp;
	PaneIDT id;
	Str255 str;
	char str_name[MAC_NAME_SIZE];

	if (* sp_count == s_max)
		return 0;
		
	if ((llistBoxp = dynamic_cast <LListBox*> (lpanep)) != NULL)
	{
		AddViewItem (lpanep, wcp, sp_count, WT_LIST_BOX, 0, NULL, (Handle) llistBoxp->GetMacListH ());
		// Adjust bounding rect to exclude the scrollbars.
		Rect r_adjusted = wcp [*sp_count-1].rect;
		if ((** llistBoxp->GetMacListH ()).vScroll != NULL)
			r_adjusted.right -= 15;
		if ((** llistBoxp->GetMacListH ()).hScroll != NULL)
			r_adjusted.bottom -= 15;			
		wcp [*sp_count-1].rect = r_adjusted;	
		goto Done;
	}
	
	if ((lstdControlp = dynamic_cast <LStdControl*> (lpanep)) != NULL)
	{
		((LCaption *) lpanep)->GetDescriptor (str);
		p2cstr (str);
		AddViewItem (lpanep, wcp, sp_count, WT_CONTROL, 0, (char *) str, (Handle) lstdControlp->GetMacControl ());
		goto Done;
	}
		
	if ((leditFieldp = dynamic_cast <LEditField*> (lpanep)) != NULL)
	{
		AddViewItem (lpanep, wcp, sp_count, WT_TEXT_FIELD, 0, NULL, (Handle) leditFieldp->GetMacTEH ());
		goto Done;
	}
		
	if ((ltextEditp = dynamic_cast <LTextEdit*> (lpanep)) != NULL)
	{
		AddViewItem (lpanep, wcp, sp_count, WT_TEXT_FIELD, 0, NULL, (Handle) ltextEditp->GetMacTEH ());
		goto Done;
	}
		
	if (dynamic_cast <LGroupBox*> (lpanep) != NULL)
	{
		((LGroupBox *) lpanep)->GetDescriptor (str);
		p2cstr (str);
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_STATIC_TEXT, (char *) str, (Handle) lpanep);
		// Change bounding rect to textbox frame.  To do this, you need to change the access modifier of
		// LGroupBox::CalcTextBoxFrame() from protected to public. 
		((LGroupBox *) lpanep)->CalcTextBoxFrame (wcp [*sp_count-1].rect);	

		goto Done;
	}
	
	if (dynamic_cast <LCaption*> (lpanep) != NULL)
	{
		((LCaption *) lpanep)->GetDescriptor (str);
		p2cstr (str);
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_STATIC_TEXT, (char *) str, (Handle) lpanep);
		goto Done;
	}

#ifdef PARTNER
	if (dynamic_cast <ZButton*> (lpanep) != NULL)
	{
		//	For in-house use, call the ZButton method
		((ZButton *) lpanep)->GetCDescriptor (str_name);
		if (strcmp (str_name, "") == 0)
		{
			id = lpanep->GetPaneID ();
			* (long *) str_name = id;
			str_name[4] = 0;
		}
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_PUSH_BUTTON, str_name, (Handle) lpanep);
		goto Done;
	}
#endif	
	if ((dynamic_cast <LButton*> (lpanep) != NULL) ||
		(dynamic_cast <LCicnButton*> (lpanep) != NULL))
	{
		//	Return the pane id as control name
		
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		//	Modify or override this to return window class WC_PUSH_BUTTON, 
		//	WC_CHECK_BOX or WC_RADIO_BUTTON as appropriate for your use of LButton.
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_PUSH_BUTTON, str_name, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LTextButton*> (lpanep) != NULL)
	{
		((LCaption *) lpanep)->GetDescriptor (str);
		p2cstr (str);
		//	According to doc, LTextButton's default behaviour is that of a radio button.
		//	If this is not your case, change window class below as appropriate.
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_RADIO_BUTTON, (char *) str, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LToggleButton*> (lpanep) != NULL)
	{
		//	Return the pane id as control name
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		//	LToggleButton is essentially a fancy check box.
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_CHECK_BOX, str_name, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LIconPane*> (lpanep) != NULL)
	{
		//	Return the pane id as control name
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_ICON, str_name, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LPlaceHolder*> (lpanep) != NULL)
	{
		//	Return the pane id as control name
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_CUSTOM, str_name, (Handle) lpanep);
		goto Done;
	}

#ifndef PARTNER
	if (dynamic_cast <LGACheckbox *> (lpanep) != NULL)
	{
		((LGACheckbox *) lpanep)->GetDescriptor (str);
		p2cstr (str);
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_CHECK_BOX, (char *) str, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LGADisclosureTriangle *> (lpanep) != NULL)
	{
		//	Return the pane id as control name
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_CHECK_BOX, str_name /*¥NETSCAPE: was NULL*/, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LGAPopup *> (lpanep) != NULL)
	{
		((LGAPopup *) lpanep)->GetDescriptor (str);
		p2cstr (str);
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_POPUP_LIST, (char *) str, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LGAIconButtonPopup *> (lpanep) != NULL)
	{
		//	Return the pane id as control name
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_POPUP_LIST, str_name /*¥NETSCAPE: was NULL*/, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LGAIconButton *> (lpanep) != NULL)
	{
		//	Return the pane id as control name
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;

		switch (((LGAIconButton *) lpanep)->GetControlMode ())
		{
			case controlMode_Button:
				AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_PUSH_BUTTON, (char *) str_name /*¥NETSCAPE: was str*/, (Handle) lpanep);
				break;
			case controlMode_RadioButton:
				AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_RADIO_BUTTON, (char *) str_name /*¥NETSCAPE: was str*/, (Handle) lpanep);
				break;
			case controlMode_Switch:
				AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_CHECK_BOX, (char *) str_name /*¥NETSCAPE: was str*/, (Handle) lpanep);
				break;
		}
		goto Done;
	}
	
	if ((dynamic_cast <LGAPushButton *> (lpanep) != NULL) ||
		(dynamic_cast <LGATextButton *> (lpanep) != NULL))
	{
		((LGAPopup *) lpanep)->GetDescriptor (str);
		p2cstr (str);
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_PUSH_BUTTON, (char *) str, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <LGARadioButton *> (lpanep) != NULL)
	{
		((LGAPopup *) lpanep)->GetDescriptor (str);
		p2cstr (str);
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_RADIO_BUTTON, (char *) str, (Handle) lpanep);
		goto Done;
	}
#endif

	if (dynamic_cast <LPicture*> (lpanep) != NULL)
	{
		//	Return the pane id as control name
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_PICTURE, str_name, (Handle) lpanep);
		// LPicture is subclassed from LView, and can have subViews.  Don't exit.
	}

	//¥ NETSCAPE --- begin
	if (dynamic_cast <CPatternButtonPopup *> (lpanep) != NULL)
	{
		//	Return the pane id as control name
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_POPUP_LIST, str_name, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <CButton*> (lpanep) != NULL)
	{
		((CButton *)lpanep)->GetDescriptor(str);
		p2cstr(str);
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_PUSH_BUTTON, (char *) str, (Handle) lpanep);
		goto Done;

		short s_type = WC_PUSH_BUTTON;	
		if (((CButton *)lpanep)->IsBehaviourRadio())
			s_type = WC_RADIO_BUTTON;
		else
		if (((CButton *)lpanep)->IsBehaviourToggle())
			s_type = WC_CHECK_BOX;

		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, s_type, (char *) str, (Handle) lpanep);
		goto Done;
	}

	if (dynamic_cast <CQAPartnerTableMixin*> (lpanep) != NULL)
	{
		id = lpanep->GetPaneID ();
		* (long *) str_name = id;
		str_name[4] = 0;
		AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_LIST_BOX, (char *) str_name, (Handle) lpanep);
		goto Done;
	}	
	//¥ NETSCAPE --- end


	//¥ NETSCAPE: below is the generic LView handler - put your custom types above
	if ((lviewp = dynamic_cast <LView*> (lpanep)) != NULL)
	{
		LArrayIterator 	iterator (lviewp->GetSubPanes (), LArrayIterator::from_Start);
		LPane 			* lpanep_sub;

		while (iterator.Next (& lpanep_sub)) 
		{
			if (lpanep_sub->IsVisible ())
				LPaneGetContents (lpanep_sub, wcp, sp_count, s_max);
			if (* sp_count == s_max)
				break;
		}
		goto Done;
	}

/*
	The following is a catch-all which will return any other LPane as a CustomWin, including 
	"cosmetic" LPanes with no functionality.  If you don't want this to happen,
	comment the next four lines out.
*/
/*
	id = lpanep->GetPaneID ();
	* (long *) str_name = id;
	str_name[4] = 0;
	AddViewItem (lpanep, wcp, sp_count, WT_ASSIST_ITEM, WC_CUSTOM, str_name, (Handle) lpanep);
*/
Done:
	return * sp_count;
}

//	----------------------------------------------------------------------------
//	¥	LPaneGetTextInfo
//	----------------------------------------------------------------------------

static short LPaneGetTextInfo (LPane * lpanep, PTEXTINFO textInfop)
{
	LCaption * lcaptionp;
	LGroupBox * lgroupBoxp;
	
	memset (textInfop, 0, sizeof (TEXTINFO));
	
	if ((lcaptionp = dynamic_cast <LCaption*> (lpanep)) != NULL)
	{
		lcaptionp->GetDescriptor ((unsigned char *) textInfop->buf);
		textInfop->hTE = NULL;
		textInfop->hasFocus = FALSE;
		textInfop->len = textInfop->buf[0];
		textInfop->handle = NULL;
		textInfop->state = FALSE;
		textInfop->ptr = (char *) & textInfop->buf[1];
	}

	if ((lgroupBoxp = dynamic_cast <LGroupBox*> (lpanep)) != NULL)
	{
		lgroupBoxp->GetDescriptor ((unsigned char *) textInfop->buf);
		textInfop->hTE = NULL;
		textInfop->hasFocus = FALSE;
		textInfop->len = textInfop->buf[0];
		textInfop->handle = NULL;
		textInfop->state = FALSE;
		textInfop->ptr = (char *) & textInfop->buf[1];
	}

	return 0;
}

//	----------------------------------------------------------------------------
//	¥	LPaneGetCustomItemName
//	----------------------------------------------------------------------------
//		If you want the assist hook to return the name of a GUI object, to
//		be used as the tag, make sure this function returns what you need.xxx
//		Name must be a NULL terminated c-string, shorter than 256 characters.
/*
static short LPaneGetCustomItemName (LPane * lpanep, char * cp_buf)
{
	
	* cp_buf = 0;
	
	//	For classes without obvious descriptor, by default we use the 4 character PaneID for tag.
	//	Change this if you want to return a different descriptor.
	LButton * lbuttonp;
	LCicnButton * lcicnButtonp;
	LTextButton * ltextButtonp;
	LToggleButton * ltoggleButtonp;
	LPicture * lpicturep;
	LIconPane * liconPanep;
			
	PaneIDT id;
	
	if ((lbuttonp = dynamic_cast <LButton *> (lpanep)) != NULL)
	{
		id = lbuttonp->GetPaneID ();
		* (long *) cp_buf = id;
		cp_buf[4] = 0;
		goto Done;
	}

	if ((lcicnButtonp = dynamic_cast <LCicnButton *> (lpanep)) != NULL)
	{
		id = lbuttonp->GetPaneID ();
		* (long *) cp_buf = id;
		cp_buf[4] = 0;
		goto Done;
	}

	if ((ltextButtonp = dynamic_cast <LTextButton *> (lpanep)) != NULL)
	{
		ltextButtonp->GetDescriptor ((unsigned char *) cp_buf);
		p2cstr ((StringPtr) cp_buf);
		goto Done;
	}

	if ((ltoggleButtonp = dynamic_cast <LToggleButton *> (lpanep)) != NULL)
	{
		id = ltoggleButtonp->GetPaneID ();
		* (long *) cp_buf = id;
		cp_buf[4] = 0;
		goto Done;
	}

	if ((lpicturep = dynamic_cast <LPicture *> (lpanep)) != NULL)
	{
		id = lpicturep->GetPaneID ();
		* (long *) cp_buf = id;
		cp_buf[4] = 0;
		goto Done;
	}

	if ((liconPanep = dynamic_cast <LIconPane *> (lpanep)) != NULL)
	{
		id = liconPanep->GetPaneID ();
		* (long *) cp_buf = id;
		cp_buf[4] = 0;
		goto Done;
	}


Done:
	return 0;
}
*/
//	----------------------------------------------------------------------------
//	¥	LPaneGetCustomItemValue
//	----------------------------------------------------------------------------
static short LPaneGetCustomItemValue (LPane * lpanep, long * l_value)
{
	
	* l_value = 0;
	
	LControl * lcontrolp;
			
	if ((lcontrolp = dynamic_cast <LControl *> (lpanep)) != NULL)
	{
		* l_value = lcontrolp->GetValue ();
		goto Done;
	}

Done:
	return 0;
}

//	----------------------------------------------------------------------------
//	¥	CQAPartnerTableMixin			//¥NETSCAPE: added
//	----------------------------------------------------------------------------
CQAPartnerTableMixin::CQAPartnerTableMixin(LTableView * inTable)
	: mTableView(inTable)
{
}

CQAPartnerTableMixin::~CQAPartnerTableMixin()
{
}

short	CQAPartnerTableMixin::QapGetListContents(Ptr pBuf, short index)
{
	STableCell	sTblCell;
	short		count = 0;
	Ptr			pLimit;

	if (pBuf == NULL || mTableView == NULL)
		return 0;

	pLimit = pBuf + *(long *)pBuf;

	TableIndexT outRows, outCols;
	mTableView->GetTableSize(outRows, outCols);

	switch (index)
	{
		case QAP_INDEX_ALL:
			mTableView->IndexToCell(1, sTblCell);
			if (mTableView->IsValidCell(sTblCell))
			{
				do
				{
					if ((pBuf = QapAddCellToBuf(pBuf, pLimit, sTblCell)) != NULL)
					{
						count ++;
						sTblCell.SetCell(sTblCell.row, outCols);
					}
					else
						break;
				}
				while (mTableView->GetNextCell(sTblCell));
			}
			break;
			
		case QAP_INDEX_SELECTED:
			sTblCell = mTableView->GetFirstSelectedCell();
			if (mTableView->IsValidCell(sTblCell))
			{
				do
				{
					if ((pBuf = QapAddCellToBuf(pBuf, pLimit, sTblCell)) != NULL)
					{
						count ++;
						sTblCell.SetCell(sTblCell.row, outCols);
					}
					else
						break;
				}
				while (mTableView->GetNextSelectedCell(sTblCell));
			}
			break;
			
		default:
			sTblCell.row = index + 1;
			sTblCell.col = 1;
			if (mTableView->IsValidCell(sTblCell))
			{
				if ((pBuf = QapAddCellToBuf(pBuf, pLimit, sTblCell)) != NULL)
					count ++;
			}
			break;
	}
	return count;
}

#endif // QAP_BUILD
