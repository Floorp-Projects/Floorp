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

#include "CAssortedMediators.h"


extern "C"
{
#include "xp_help.h"
}

#include "prefapi.h"
#ifdef MOZ_MAIL_NEWS
#include "CMailNewsContext.h"
#endif
#include "CBrowserWindow.h"
#include "CBrowserContext.h"
#include "InternetConfig.h"
#ifdef MOZ_MAIL_NEWS
#include "CMessageFolder.h"
#endif
#include "UGraphicGizmos.h"
#include "CToolTipAttachment.h"
#include "CValidEditField.h"

#include "MPreference.h"
#include "PrefControls.h"

//#include "prefwutil.h"
#include "glhist.h"
#include "proto.h"
//#include "meditdlg.h"
#include "macgui.h"
#include "resgui.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "libi18n.h"
#include "abdefn.h"
#include "addrbook.h"

#include "np.h"
#include "uapp.h"

#include "macutil.h"

#include "CSizePopup.h"

#include <Sound.h>
#include <UModalDialogs.h>
#include <UNewTextDrawing.h>
#include <LTextColumn.h>
#include <UDrawingUtils.h>
#include <UGAColorRamp.h>
#include <LControl.h>


#include <LGADialogBox.h>
#include <LGACheckbox.h>
#include <LGARadioButton.h>

//#include <QAP_Assist.h>


//#include <ICAPI.h>
//#include <ICKeys.h>
#include <UCursor.h>
#ifdef MOZ_MAIL_NEWS
#define DEFAULT_NEWS_SERVER_KLUDGE
#ifdef DEFAULT_NEWS_SERVER_KLUDGE
	#include "CMailNewsContext.h"
	#include "CMailProgressWindow.h"
#endif
#endif


#pragma mark ---CEditFieldControl---
//======================================
class CEditFieldControl : public LGAEditField
//======================================
{
	// Note: This is not derived from LControl! It has control in the
	// name because it broadcasts when the user changes its contents.

	public:
		enum
		{
			class_ID = 'edtC',
			msg_ChangedText = 'TxtC'
		};
		virtual				~CEditFieldControl() {}
							CEditFieldControl(LStream *inStream) :
											  LGAEditField(inStream)
											  {}
		virtual void		UserChangedText();
}; // class CEditFieldControl

//-----------------------------------
void CEditFieldControl::UserChangedText()
//-----------------------------------
{
	BroadcastMessage(msg_ChangedText, this);
}

//======================================
#pragma mark --CAppearanceMainMediator---
//======================================

enum
{
	eBrowserBox = 12001,
	eMailBox,
	eNewsBox,
	eEditorBox,
	eConferenceBox,
	eCalendarBox,
	eNetcasterBox,
	ePicturesAndTextRButton,
	eShowToolTipsBox,
	ePicturesOnlyRButton,
	eTextOnlyRButton,
	eDesktopPatternBox
};

//-----------------------------------
CAppearanceMainMediator::CAppearanceMainMediator(LStream*)
//-----------------------------------
:	CPrefsMediator(class_ID)
{
}

//-----------------------------------
void CAppearanceMainMediator::LoadPrefs()
//-----------------------------------
{
	const OSType kConferenceAppSig = 'Ncq¹';
	FSSpec fspec;
	if (CFileMgr::FindApplication(kConferenceAppSig, fspec) != noErr)
	{
		LGACheckbox* checkbox = (LGACheckbox *)FindPaneByID(eConferenceBox);
		XP_ASSERT(checkbox);
		checkbox->SetValue(false);
		// disable the control
		checkbox->Disable();	
	}

	const OSType kCalendarAppSig = 'NScl';
	if (CFileMgr::FindApplication(kCalendarAppSig, fspec) != noErr)
	{
		LGACheckbox* checkbox = (LGACheckbox *)FindPaneByID(eCalendarBox);
		XP_ASSERT(checkbox);
		checkbox->SetValue(false);
		// disable the control
		checkbox->Disable();	
	}

	if (!FE_IsNetcasterInstalled())
	{
		LGACheckbox* checkbox = (LGACheckbox *)FindPaneByID(eNetcasterBox);
		XP_ASSERT(checkbox);
		checkbox->SetValue(false);
		// disable the control
		checkbox->Disable();
	}
}

void CAppearanceMainMediator::WritePrefs()
{
	// this pref will not take effect immediately unless we tell the CToolTipAttachment
	// class to make it so
	LGACheckbox	*theBox = (LGACheckbox *)FindPaneByID(eShowToolTipsBox);
	XP_ASSERT(theBox);
	CToolTipAttachment::Enable(theBox->GetValue());
}

enum
{
	eCharSetMenu = 12101,
	ePropFontMenu,
	ePropSizeMenu,
	eFixedFontMenu,
	eFixedSizeMenu,
	eAlwaysOverrideRButton,
	eQuickOverrideRButton,
	eNeverOverrideRButton
};


//-----------------------------------
CAppearanceFontsMediator::CAppearanceFontsMediator(LStream*)
//-----------------------------------
:	CPrefsMediator(class_ID)
,	mEncodings(nil)
{
}

int
CAppearanceFontsMediator::GetSelectEncMenuItem()
{
	LGAPopup *encMenu = (LGAPopup *)FindPaneByID(eCharSetMenu);
	XP_ASSERT(encMenu);
	return encMenu->GetValue();
}

void
CAppearanceFontsMediator::UpdateEncoding(PaneIDT changedMenuID)
{
	Int32	selectedEncMenuItem = GetSelectEncMenuItem();
	XP_ASSERT(selectedEncMenuItem <= mEncodingsCount);

	if (changedMenuID == ePropFontMenu || changedMenuID == eFixedFontMenu)
	{
		LGAPopup *changedMenu = (LGAPopup *)FindPaneByID(changedMenuID);
		XP_ASSERT(changedMenu);
		int	changedMenuValue = changedMenu->GetValue();
		CStr255	itemString;
		::GetMenuItemText(changedMenu->GetMacMenuH(), changedMenuValue, itemString);
		if (changedMenuID == ePropFontMenu)
		{
			mEncodings[selectedEncMenuItem - 1].mPropFont = itemString;
		}
		else
		{
			mEncodings[selectedEncMenuItem - 1].mFixedFont = itemString;
		}
	}
	else if (changedMenuID == ePropSizeMenu || changedMenuID == eFixedSizeMenu)
	{
		CSizePopup *sizeMenu = (CSizePopup *)FindPaneByID(changedMenuID);
		XP_ASSERT(sizeMenu);
		if (changedMenuID == ePropSizeMenu)
		{
			mEncodings[selectedEncMenuItem - 1].mPropFontSize = sizeMenu->GetFontSize();
		}
		else
		{
			mEncodings[selectedEncMenuItem - 1].mFixedFontSize = sizeMenu->GetFontSize();
		}
	}
}


void
CAppearanceFontsMediator::UpdateMenus()
{
	Int32	selectedEncMenuItem = GetSelectEncMenuItem();
	XP_ASSERT(selectedEncMenuItem <= mEncodingsCount);

	CSizePopup *propSizeMenu = (CSizePopup *)FindPaneByID(ePropSizeMenu);
	XP_ASSERT(propSizeMenu);
	propSizeMenu->SetFontSize(mEncodings[selectedEncMenuItem - 1].mPropFontSize);
	if (mEncodings[selectedEncMenuItem - 1].mPropFontSizeLocked)
	{
		propSizeMenu->Disable();
	}
	else
	{
		propSizeMenu->Enable();
	}
	CSizePopup *fixedSizeMenu = (CSizePopup *)FindPaneByID(eFixedSizeMenu);
	XP_ASSERT(fixedSizeMenu);
	fixedSizeMenu->SetFontSize(mEncodings[selectedEncMenuItem - 1].mFixedFontSize);
	if (mEncodings[selectedEncMenuItem - 1].mFixedFontSizeLocked)
	{
		fixedSizeMenu->Disable();
	}
	else
	{
		fixedSizeMenu->Enable();
	}

	Str255	fontName;
	LGAPopup *propFontMenu = (LGAPopup *)FindPaneByID(ePropFontMenu);
	XP_ASSERT(propFontMenu);
	if (!SetLGAPopupToNamedItem(propFontMenu, mEncodings[selectedEncMenuItem - 1].mPropFont))
	{
		GetFontName(applFont, fontName);
		if (!SetLGAPopupToNamedItem(propFontMenu, fontName))
		{
			propFontMenu->SetValue(1);
		}
	}
	propSizeMenu->MarkRealFontSizes(propFontMenu);
	if (mEncodings[selectedEncMenuItem - 1].mPropFontLocked)
	{
		propFontMenu->Disable();
	}
	else
	{
		propFontMenu->Enable();
	}

	LGAPopup *fixedFontMenu = (LGAPopup *)FindPaneByID(eFixedFontMenu);
	XP_ASSERT(fixedFontMenu);
	if (!SetLGAPopupToNamedItem(fixedFontMenu, mEncodings[selectedEncMenuItem - 1].mFixedFont))
	{
		GetFontName(applFont, fontName);
		if (!SetLGAPopupToNamedItem(fixedFontMenu, fontName))
		{
			fixedFontMenu->SetValue(1);
		}
	}
	fixedSizeMenu->MarkRealFontSizes(fixedFontMenu);
	if (mEncodings[selectedEncMenuItem - 1].mFixedFontLocked)
	{
		fixedFontMenu->Disable();
	}
	else
	{
		fixedFontMenu->Enable();
	}
}


void
CAppearanceFontsMediator::SetEncodingWithPref(Encoding& enc)
{
	char *propFontTemplate = "intl.font%hu.prop_font";
	char *fixedFontTemplate = "intl.font%hu.fixed_font";
	char *propFontSizeTemplate = "intl.font%hu.prop_size";
	char *fixedFontSizeTemplate = "intl.font%hu.fixed_size";
	char prefBuffer[255];
	char fontNameBuffer[32];
	int	bufferLength;
	int32	fontSize;

	sprintf(prefBuffer, propFontTemplate, enc.mCSID);
	bufferLength = 32;
	PREF_GetCharPref(prefBuffer, fontNameBuffer, &bufferLength);
	enc.mPropFont = fontNameBuffer;
	enc.mPropFontLocked = PREF_PrefIsLocked(prefBuffer);

	sprintf(prefBuffer, fixedFontTemplate, enc.mCSID);
	bufferLength = 32;
	PREF_GetCharPref(prefBuffer, fontNameBuffer, &bufferLength);
	enc.mFixedFont = fontNameBuffer;
	enc.mFixedFontLocked = PREF_PrefIsLocked(prefBuffer);

	sprintf(prefBuffer, propFontSizeTemplate, enc.mCSID);
	PREF_GetIntPref(prefBuffer, &fontSize);
	enc.mPropFontSize = fontSize;
	enc.mPropFontSizeLocked = PREF_PrefIsLocked(prefBuffer);

	sprintf(prefBuffer, fixedFontSizeTemplate, enc.mCSID);
	PREF_GetIntPref(prefBuffer, &fontSize);
	enc.mFixedFontSize = fontSize;
	enc.mFixedFontSizeLocked = PREF_PrefIsLocked(prefBuffer);
}

void
CAppearanceFontsMediator::ReadEncodings(Handle hndl)
{
	unsigned short	numberEncodings;
	
	LHandleStream	stream(hndl);
				
	stream.ReadData(&numberEncodings, sizeof(unsigned short));
	XP_ASSERT(numberEncodings > 0);
	mEncodingsCount = numberEncodings;
	mEncodings	= new Encoding[numberEncodings];
	XP_ASSERT(mEncodings != nil);
	for(int i = 0; i < numberEncodings; i++)
	{
		CStr31			EncodingName;
		CStr31			PropFont;
		CStr31			FixedFont;
		unsigned short	PropFontSize;
		unsigned short	FixedFontSize;
		unsigned short	CSID;
		unsigned short  FallbackFontScriptID;
		unsigned short	TxtrButtonResID;
		unsigned short	TxtrTextFieldResID;	
		
		stream.ReadData(&EncodingName,	sizeof( CStr31));
		stream.ReadData(&PropFont, 		sizeof( CStr31));
		stream.ReadData(&FixedFont, 		sizeof( CStr31));
		stream.ReadData(&PropFontSize, 	sizeof( unsigned short));
		stream.ReadData(&FixedFontSize, 	sizeof( unsigned short));
		stream.ReadData(&CSID, 			sizeof( unsigned short));
		stream.ReadData(&FallbackFontScriptID, sizeof( unsigned short));
		stream.ReadData(&TxtrButtonResID, sizeof( unsigned short));
		stream.ReadData(&TxtrTextFieldResID, sizeof( unsigned short));

		mEncodings[i].mLanguageGroup = EncodingName;
		mEncodings[i].mCSID = CSID;
		mEncodings[i].mPropFont = PropFont;
		mEncodings[i].mFixedFont = FixedFont;
		mEncodings[i].mPropFontSize = PropFontSize;
		mEncodings[i].mFixedFontSize = FixedFontSize;
		SetEncodingWithPref(mEncodings[i]);
	}
}

void
CAppearanceFontsMediator::LoadEncodings()
{
	Handle hndl;
	::UseResFile(LMGetCurApRefNum());
	hndl = ::Get1Resource(FENC_RESTYPE,FNEC_RESID);
	if (hndl)
	{
		if (!*hndl)
		{
			::LoadResource(hndl);
		}
		if (*hndl)
		{
			::DetachResource(hndl);
			ThrowIfResError_();
			ReadEncodings(hndl);
			DisposeHandle(hndl);
		}
	}
}


void
CAppearanceFontsMediator::PopulateEncodingsMenus(PaneIDT menuID)
{
	if (!mEncodings)
	{
		LoadEncodings();
	}
	LGAPopup *theMenu = (LGAPopup *)FindPaneByID(menuID);
	XP_ASSERT(theMenu);
	for (int i = 0; i < mEncodingsCount; ++i)
	{
		MenuHandle	menuH = theMenu->GetMacMenuH();
		AppendMenu(menuH, "\pabc");
		SetMenuItemText(menuH, i + 1, mEncodings[i].mLanguageGroup);
	}
	theMenu->SetMaxValue(mEncodingsCount);
	theMenu->SetValue(1);
}

void
CAppearanceFontsMediator::SetPrefWithEncoding(const Encoding& enc)
{
	char *propFontTemplate = "intl.font%hu.prop_font";
	char *fixedFontTemplate = "intl.font%hu.fixed_font";
	char *propFontSizeTemplate = "intl.font%hu.prop_size";
	char *fixedFontSizeTemplate = "intl.font%hu.fixed_size";
	char prefBuffer[255];

	if (!enc.mPropFontLocked)
	{
		sprintf(prefBuffer, propFontTemplate, enc.mCSID);
		PREF_SetCharPref(prefBuffer, (char *)enc.mPropFont);
	}

	if (!enc.mFixedFontLocked)
	{
		sprintf(prefBuffer, fixedFontTemplate, enc.mCSID);
		PREF_SetCharPref(prefBuffer, (char *)enc.mFixedFont);
	}

	if (!enc.mPropFontSizeLocked)
	{
		sprintf(prefBuffer, propFontSizeTemplate, enc.mCSID);
		PREF_SetIntPref(prefBuffer, enc.mPropFontSize);
	}

	if (!enc.mFixedFontSizeLocked)
	{
		sprintf(prefBuffer, fixedFontSizeTemplate, enc.mCSID);
		PREF_SetIntPref(prefBuffer, enc.mFixedFontSize);
	}
}

void
CAppearanceFontsMediator::WriteEncodingPrefs()
{
	for(int i = 0; i < mEncodingsCount; i++)
	{
		SetPrefWithEncoding(mEncodings[i]);
	}
}


Int16
CAppearanceFontsMediator::GetFontSize(LGAPopup* whichPopup)
{
	Str255		sizeString;
	Int32		fontSize = 12;
	MenuHandle	sizeMenu;
	short		menuSize;
	short		inMenuItem;
	
	sizeMenu = whichPopup->GetMacMenuH();
	menuSize = CountMItems(sizeMenu);
	inMenuItem = whichPopup->GetValue();
	
	GetMenuItemText(sizeMenu, inMenuItem, sizeString);
	
	myStringToNum(sizeString, &fontSize);
	return fontSize;
}

void
CAppearanceFontsMediator::FontMenuChanged(PaneIDT changedMenuID)
{
	CSizePopup	*sizePopup =
			(CSizePopup *)FindPaneByID(ePropFontMenu == changedMenuID ?
											ePropSizeMenu :
											eFixedSizeMenu);
	XP_ASSERT(sizePopup);
	LGAPopup	*fontPopup = (LGAPopup *)FindPaneByID(changedMenuID);
	XP_ASSERT(fontPopup);
	sizePopup->MarkRealFontSizes(fontPopup);
	UpdateEncoding(changedMenuID);
}

void
CAppearanceFontsMediator::SizeMenuChanged(PaneIDT changedMenuID)
{
	UpdateEncoding(changedMenuID);
}

void
CAppearanceFontsMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eCharSetMenu:
			UpdateMenus();
			break;
		case msg_ChangeFontSize:
			break;
		case ePropFontMenu:
		case eFixedFontMenu:
			FontMenuChanged(inMessage);
			break;
		case ePropSizeMenu:
		case eFixedSizeMenu:
			SizeMenuChanged(inMessage);
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CAppearanceFontsMediator::LoadPrefs()
{
	PopulateEncodingsMenus(eCharSetMenu);
	UpdateMenus();								
}

void
CAppearanceFontsMediator::WritePrefs()
{
	WriteEncodingPrefs();
}

enum
{
	eForegroundColorButton = 12201,
	eBackgroundColorButton,
	eUnvisitedColorButton,
	eVisitedColorButton,
	eUnderlineLinksBox,
	eUseDefaultButton,
	eOverrideDocColors
};

CAppearanceColorsMediator::CAppearanceColorsMediator(LStream*)
:	CPrefsMediator(class_ID)
{
}

void
CAppearanceColorsMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eUseDefaultButton:
			UseDefaults();
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CAppearanceColorsMediator::UseDefaults()
{
	ReadDefaultPref(eForegroundColorButton);
	ReadDefaultPref(eUnvisitedColorButton);
	ReadDefaultPref(eVisitedColorButton);
	ReadDefaultPref(eBackgroundColorButton);
}

enum
{
	eBlankPageRButton = 12301,
	eHomePageRButton,
	eLastPageRButton,
	eHomePageTextEditBox,
	eHomePageChooseButton,
	eExpireAfterTextEditBox,
	eExpireNowButton,
	eUseCurrentButton
};

enum 
{
	eExpireLinksDialog = 1060,
	eClearDiskCacheDialog = 1065
};

CBrowserMainMediator::CBrowserMainMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mHomePageURLLocked(false)
,	mCurrentURL(nil)
{
}

void
CBrowserMainMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
//		case msg_ControlClicked:
//			break;
		case eHomePageChooseButton:
			SetEditFieldWithLocalFileURL(eHomePageTextEditBox, mHomePageURLLocked);
			// do whatever it takes to find a home page file
			break;
		case eExpireNowButton:
			// do whatever it takes to expire links
			ExpireNow();
			break;
		case eUseCurrentButton:
			// do whatever it takes to find the current page
			if (mCurrentURL && *mCurrentURL)
			{
				LStr255	pURL(mCurrentURL);
				LEditField	*theField =
					(LEditField *)FindPaneByID(eHomePageTextEditBox);
				XP_ASSERT(theField);
				theField->SetDescriptor(pURL);
				theField->SelectAll();
			}
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CBrowserMainMediator::ExpireNow()
{
	StPrepareForDialog	prepare;
	short	response = ::CautionAlert(eExpireLinksDialog, NULL);
	if (1 == response)
	{
		GH_ClearGlobalHistory();
		XP_RefreshAnchors();
	}
}

Boolean
CBrowserMainMediator::ExpireDaysValidationFunc(CValidEditField *daysTilExpire)
{
	Boolean		result;
	result = ConstrainEditField(daysTilExpire, EXPIRE_MIN, EXPIRE_MAX);
	if (!result)
	{
		StPrepareForDialog	prepare;
		::StopAlert(1064, NULL);
	}
	return result;
}

void
CBrowserMainMediator::LoadMainPane()
{
	CPrefsMediator::LoadMainPane();
	SetValidationFunction(eExpireAfterTextEditBox, ExpireDaysValidationFunc);
}

void
CBrowserMainMediator::UpdateFromIC()
{
	SetEditFieldsWithICPref(kICWWWHomePage, eHomePageTextEditBox);
	URLChoosingButtons(!UseIC() && !mHomePageURLLocked);
}

void
CBrowserMainMediator::URLChoosingButtons(Boolean enable)
{
	LButton	*currentURLButton = (LButton *)FindPaneByID(eUseCurrentButton);
	XP_ASSERT(currentURLButton);
	LButton	*chooseURLButton = (LButton *)FindPaneByID(eHomePageChooseButton);
	XP_ASSERT(chooseURLButton);
	if (enable)
	{
		currentURLButton->Enable();
		chooseURLButton->Enable();
	}
	else
	{
		currentURLButton->Disable();
		chooseURLButton->Disable();
	}
}

//-----------------------------------
void CBrowserMainMediator::LoadPrefs()
//-----------------------------------
{
	mHomePageURLLocked = PaneHasLockedPref(eHomePageTextEditBox);
	if (!mHomePageURLLocked)	// If locked we don't need the current url.
	{
		URLChoosingButtons(true);
		CWindowMediator	*mediator = CWindowMediator::GetWindowMediator();
		CBrowserWindow	*browserWindow =
			(CBrowserWindow	*)mediator->FetchTopWindow(WindowType_Browser);
		if (browserWindow)
		{
			CNSContext	*context = browserWindow->GetWindowContext();
			mCurrentURL = XP_STRDUP(context->GetCurrentURL());
		}
	}
	else
	{
		URLChoosingButtons(false);
	}
	LButton	*currentURLButton = (LButton *)FindPaneByID(eUseCurrentButton);
	XP_ASSERT(currentURLButton);
	if (mHomePageURLLocked || (!mCurrentURL) || (!(*mCurrentURL)))
		currentURLButton->Disable();
	else
		currentURLButton->Enable();
}

//-----------------------------------
void CBrowserMainMediator::WritePrefs()
//-----------------------------------
{
}

enum
{
	eBuiltInLanguageStringResID = 5028,
	eBuiltInLanguageCodeResID = 5029,
	eLanguageList = 12401,
	eAddLanguageButton,
	eDeleteLanguageButton,
	eAddLanguageList,
	eAddLanguageOtherEditField,
	eAddLanguageDialogID = 12004
};



CBrowserLanguagesMediator::CBrowserLanguagesMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mLanguagesLocked(false)
,	mBuiltInCount(0)
,	mLanguageStringArray(nil)
,	mLangaugeCodeArray(nil)
{
}

CBrowserLanguagesMediator::~CBrowserLanguagesMediator()
{
	int i;
	if (mLanguageStringArray)
	{
		for (i = 0; i < mBuiltInCount; ++i)
		{
			if (mLanguageStringArray[i])
			{
				XP_FREE(mLanguageStringArray[i]);
			}
		}
		XP_FREE(mLanguageStringArray);
	}
	if (mLangaugeCodeArray)
	{
		for (i = 0; i < mBuiltInCount; ++i)
		{
			if (mLangaugeCodeArray[i])
			{
				XP_FREE(mLangaugeCodeArray[i]);
			}
		}
		XP_FREE(mLangaugeCodeArray);
	}
}

char *
CBrowserLanguagesMediator::GetLanguageDisplayString(	const char *languageCode)
{
	char	*result;
	int		i;
	for (i = 0; i < mBuiltInCount; ++i)
	{
		if (!strcmp(languageCode, mLangaugeCodeArray[i]))
		{
			const	char *	const	formatString = "%s [%s]";
			int	newStringLength =	strlen(mLangaugeCodeArray[i]) +
									strlen(mLanguageStringArray[i]) +
									strlen(formatString) -
									4;	// the two "%s"s in the format string
			result = (char *)XP_ALLOC(newStringLength + 1);
			sprintf(result, formatString, mLanguageStringArray[i], mLangaugeCodeArray[i]);
			break;
		}
	}
	if (i == mBuiltInCount)	// then we didn't find a match
	{
		result = (char *)XP_ALLOC(strlen(languageCode) + 1);
		strcpy(result, languageCode);
	}
	return result;
}

void
CBrowserLanguagesMediator::SetLanguageListWithPref(	const char *prefName,
														PaneIDT	languageListID,
														Boolean	&locked)
{
	locked = PREF_PrefIsLocked(prefName);
	CDragOrderTextList	*languageList =
				(CDragOrderTextList *)FindPaneByID(languageListID);
	XP_ASSERT(languageList);

	char	*theString;
	int		prefResult = PREF_CopyCharPref(prefName, &theString);
	if (prefResult == PREF_NOERROR && theString != nil)
	{
		TableIndexT	rowNumber = 0;
		char	*languageCode;
		char	*strtokFirstParam = theString;
		#pragma warn_possunwant off
		while (languageCode = strtok(strtokFirstParam, ", "))
		#pragma warn_possunwant reset
		{
			char	*display = GetLanguageDisplayString(languageCode);
			Str255	pDisplay;
			strtokFirstParam = nil;
			BlockMoveData(display, &pDisplay[1], pDisplay[0] = strlen(display));
			languageList->InsertRows(1, rowNumber++, pDisplay, sizeof(Str255), false);
			XP_FREE(display);
		}
		XP_FREE(theString);
		languageList->Refresh();
	}
	if (locked)
	{
		LButton	*button =
				(LButton *)FindPaneByID(eAddLanguageButton);
		XP_ASSERT(button);
		button->Disable();
		button = (LButton *)FindPaneByID(eDeleteLanguageButton);
		XP_ASSERT(button);
		button->Disable();
		languageList->LockOrder();
	}
}

char *
CBrowserLanguagesMediator::AppendLanguageCode(	const char *originalString,
													const char *stringToAdd)
{
	int		originalLength = originalString ? strlen(originalString) : 0;
	int		lengthToAdd;
	char	*occur = strchr(stringToAdd, '[');
	char	*occurRight;
	if (occur)
	{
		++occur;
		occurRight = strchr(occur, ']');
		if (occurRight)
		{
			lengthToAdd = occurRight - occur;
		}
	}
	else
	{
		lengthToAdd = strlen(stringToAdd);
	}
	char	*result = nil;
	if (originalLength || lengthToAdd)
	{
		const	char *	const junctionString = ", ";
		result = (char *)XP_ALLOC(	originalLength +
									lengthToAdd +
									strlen(junctionString) + 1);
		result[0] = '\0';
		if (originalLength)
		{
			strcpy(result, originalString);
			strcat(result, junctionString);
		}
		if (lengthToAdd)
		{
			const char	*source = occur ? occur : stringToAdd;
			strncat(result, source, lengthToAdd);
		}
	}
	return result;
}

void CBrowserLanguagesMediator::SetPrefWithLanguageList(const char *prefName,
														PaneIDT	/*languageListID*/,
														Boolean	locked)
{
	if (!locked)
	{
		char	*prefString = nil;
		CDragOrderTextList *languageList =
				(CDragOrderTextList *)FindPaneByID(eLanguageList);
		XP_ASSERT(languageList);
		TableIndexT	rows, cols;
		languageList->GetTableSize(rows, cols);

		STableCell	iCell(1, 1);
		for (int i = 0; i < rows; ++i)
		{
			Str255	languageStr;
			Uint32	dataSize = sizeof(Str255);
			languageList->GetCellData(iCell, languageStr, dataSize);
			++iCell.row;
			languageStr[languageStr[0] + 1] = '\0';	// &languageStr[1] is now a C string
			char	*temp = prefString;
			prefString = AppendLanguageCode(prefString, (char *)&languageStr[1]);
			if (temp)
			{
				XP_FREE(temp);
			}
		}

		// only set if different than current
		char	terminator = '\0';
		char	*currentValue;

		int		prefResult = PREF_CopyCharPref(prefName, &currentValue);
		if (prefResult != PREF_NOERROR)
		{
			currentValue = &terminator;
		}
		if (prefResult != PREF_NOERROR || strcmp(currentValue, prefString))
		{
			PREF_SetCharPref(prefName, prefString);
			XP_FREE(prefString);
		}
		if (currentValue && (currentValue != &terminator))
		{
			XP_FREE(currentValue);
		}
	}
}

void
CBrowserLanguagesMediator::UpdateButtons(LTextColumn *languageList)
{
	if (!mLanguagesLocked)
	{
		if (!languageList)
		{
			languageList = (CDragOrderTextList *)FindPaneByID(eLanguageList);
			XP_ASSERT(languageList);
		}
		STableCell		currentCell;
		currentCell = languageList->GetFirstSelectedCell();
		LButton	*button =
				(LButton *)FindPaneByID(eDeleteLanguageButton);
		XP_ASSERT(button);
		if (currentCell.row)
		{
			button->Enable();
		}
		else
		{
			button->Disable();
		}
	}
}

void
CBrowserLanguagesMediator::FillAddList(LTextColumn *list)
{
	int i;
	for (i = 0; i < mBuiltInCount; ++i)
	{
		char	*displayString;
		const	char *	const	formatString = "%s [%s]";
		int	newStringLength =	strlen(mLangaugeCodeArray[i]) +
								strlen(mLanguageStringArray[i]) +
								strlen(formatString) -
								4;	// the two "%s"s in the format string
		displayString = (char *)XP_ALLOC(newStringLength + 1);
		sprintf(displayString, formatString, mLanguageStringArray[i], mLangaugeCodeArray[i]);
		Str255		pDisplayString;
		BlockMoveData(displayString, &pDisplayString[1], newStringLength);
		XP_FREE(displayString);
		pDisplayString[0] = newStringLength;
		list->InsertRows(1, i, &pDisplayString, sizeof(pDisplayString), false);
	}
}


Boolean
CBrowserLanguagesMediator::GetNewLanguage(char *&newLanguage)
{
	Boolean	result;

	StDialogHandler	handler(eAddLanguageDialogID, sWindow->GetSuperCommander());
	LWindow			*dialog = handler.GetDialog();

	mAddLanguageList =
		(LTextColumn *)dialog->FindPaneByID(eAddLanguageList);
	XP_ASSERT(mAddLanguageList);
	CEditFieldControl	*theField =
		(CEditFieldControl *)dialog->FindPaneByID(eAddLanguageOtherEditField);
	XP_ASSERT(theField);

	mOtherTextEmpty = true;

	mAddLanguageList->AddListener(&handler);
	theField->AddListener(this);

	FillAddList(mAddLanguageList);
	MessageT message = msg_Nothing;
	do
	{
		message = handler.DoDialog();
		if ('2sel' == message)
		{
			theField->SetDescriptor("\p");
			mOtherTextEmpty = true;
			message = msg_OK;
		}
		if (msg_Nothing != message &&
			msg_OK != message  &&
			msg_Cancel != message)
		{
			message = msg_Nothing;
		}
	} while (msg_Nothing == message);

	if (msg_OK == message)
	{
		Str255	newLanguageName;
		theField->GetDescriptor(newLanguageName);
		int		newLanguageNameSize = newLanguageName[0];
		if (newLanguageNameSize)
		{
			newLanguage = (char *)XP_ALLOC(newLanguageNameSize + 1);
			BlockMoveData(&newLanguageName[1], newLanguage, newLanguageNameSize);
			newLanguage[newLanguageNameSize] = '\0';
			result = true;
		}
		else
		{
			STableCell	selectedCell = mAddLanguageList->GetFirstSelectedCell();
			Str255		selectedLanguage;
			if (selectedCell.row)
			{
				long	dataSize = sizeof(selectedLanguage);
				mAddLanguageList->GetCellData(selectedCell, selectedLanguage, dataSize);
				newLanguage = (char *)XP_ALLOC(selectedLanguage[0] + 1);
				BlockMoveData(&selectedLanguage[1], newLanguage, selectedLanguage[0]);
				newLanguage[selectedLanguage[0]] = '\0';
				result = true;
			}
			else
			{
				result = false;
			}
		}
	}
	else
	{
		result = false;
	}
	return result;
}

Boolean
CBrowserLanguagesMediator::GetNewUniqueLanguage(char *&newLanguage)
{
	Boolean	result = GetNewLanguage(newLanguage);
	if (result)
	{
		char	*tempString = newLanguage;
		newLanguage = GetLanguageDisplayString(tempString);
		XP_FREE(tempString);

		CDragOrderTextList *languageList =
				(CDragOrderTextList *)FindPaneByID(eLanguageList);
		XP_ASSERT(languageList);
		TableIndexT	rows, cols;
		languageList->GetTableSize(rows, cols);

		STableCell	iCell(1, 1);
		for (int i = 0; i < rows; ++i)
		{
			Str255	languageStr;
			Uint32	dataSize = sizeof(Str255);
			languageList->GetCellData(iCell, languageStr, dataSize);
			++iCell.row;
			LStr255	pNewLangage(newLanguage);
			if (!pNewLangage.CompareTo(languageStr))	// we already have this lanuage
			{
				result = false;
				XP_FREE(newLanguage);
				break;
			}
		}
	}
	return result;
}


void
CBrowserLanguagesMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case CEditFieldControl::msg_ChangedText:
			Str255	languageStr;
			((CEditFieldControl *)ioParam)->GetDescriptor(languageStr);
			if ((mOtherTextEmpty && languageStr[0]) ||	// The value of mOtherTextEmpty
				(!mOtherTextEmpty && !languageStr[0]))	// needs to change.
			{
//				STableCell	selected = mAddLanguageList->GetFirstSelectedCell();
//				if (selected.row)
//				{
					if (mOtherTextEmpty)
					{
						mOtherTextEmpty = false;
						mAddLanguageList->Deactivate();
					}
					else
					{
						mOtherTextEmpty = true;
						mAddLanguageList->Activate();
					}
//				}
			}
			break;
		case eSelectionChanged:
			UpdateButtons((CDragOrderTextList *)ioParam);
			break;
		case eAddLanguageButton:
			XP_ASSERT(!mLanguagesLocked);
			char	*newLanguage;
			if (GetNewUniqueLanguage(newLanguage))
			{
				CDragOrderTextList *languageList =
						(CDragOrderTextList *)FindPaneByID(eLanguageList);
				XP_ASSERT(languageList);
				STableCell		currentCell;
				currentCell = languageList->GetFirstSelectedCell();
				TableIndexT	insertAfter = currentCell.row;
				if (!insertAfter)
				{
					// add it at the end
					TableIndexT	cols;
					languageList->GetTableSize(insertAfter, cols);
				}
				LStr255	pNewLanguage(newLanguage);
				languageList->InsertRows(	1,
											insertAfter,
											pNewLanguage,
											sizeof(Str255),
											true);
				if (newLanguage)
				{
					XP_FREE(newLanguage);
				}
				UpdateButtons(languageList);
			}
			break;
		case eDeleteLanguageButton:
			XP_ASSERT(!mLanguagesLocked);
			CDragOrderTextList *languageList =
					(CDragOrderTextList *)FindPaneByID(eLanguageList);
			XP_ASSERT(languageList);
			STableCell		currentCell;
			currentCell = languageList->GetFirstSelectedCell();
			languageList->RemoveRows(1, currentCell.row, true);
			UpdateButtons(languageList);
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}


void
CBrowserLanguagesMediator::LoadBuiltInArrays()
{
	// I guess we can tell how many strings are in a string list from the first
	// two bytes of the resource. Is this documented?
	short	**countHandle = (short **)GetResource('STR#', eBuiltInLanguageStringResID);
	XP_ASSERT(countHandle);
	mBuiltInCount = **countHandle;
	mLanguageStringArray = (char **)XP_ALLOC(mBuiltInCount * sizeof(char *));
	mLangaugeCodeArray = (char **)XP_ALLOC(mBuiltInCount * sizeof(char *));
	int i;
	for (i = 0; i < mBuiltInCount; ++i)
	{
		Str255	builtInString; Str255	builtInCode;

		::GetIndString(builtInString, eBuiltInLanguageStringResID, i + 1);
		::GetIndString(builtInCode, eBuiltInLanguageCodeResID, i + 1);
		mLanguageStringArray[i] = (char *)XP_ALLOC(builtInString[0] + 1);
		mLangaugeCodeArray[i] = (char *)XP_ALLOC(builtInCode[0] + 1);
		::BlockMoveData(&builtInString[1], mLanguageStringArray[i], (Size)builtInString[0]);
		::BlockMoveData(&builtInCode[1], mLangaugeCodeArray[i], (Size)builtInCode[0]);
		mLanguageStringArray[i][builtInString[0]] = '\0';
		mLangaugeCodeArray[i][builtInCode[0]] = '\0';
	}
}

//-----------------------------------
void CBrowserLanguagesMediator::LoadMainPane()
//-----------------------------------
{
	CPrefsMediator::LoadMainPane();
	CDragOrderTextList *languageList =
			(CDragOrderTextList *)FindPaneByID(eLanguageList);
	XP_ASSERT(languageList);
	languageList->AddListener(this);
	UpdateButtons(languageList);
}

//-----------------------------------
void CBrowserLanguagesMediator::LoadPrefs()
//-----------------------------------
{
	LoadBuiltInArrays();
	SetLanguageListWithPref("intl.accept_languages",
							eLanguageList,
							mLanguagesLocked);
}

//-----------------------------------
void CBrowserLanguagesMediator::WritePrefs()
//-----------------------------------
{
	SetPrefWithLanguageList("intl.accept_languages",
							eLanguageList,
							mLanguagesLocked);
}




enum
{
	eAuthorNameEditField = 13201,
	eAutoSaveCheckBox,
	eAutoSaveIntervalEditField,
	eNewDocTemplateURLEditField,
	eRestoreDefaultButton,
	eChooseLocalFileButton,
	eUseHTMLSourceEditorBox,
	eHTMLSourceEditorFilePicker,
	eUseImageEditorBox,
	eImageEditorFilePicker
};

#ifdef EDITOR

CEditorMainMediator::CEditorMainMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mNewDocURLLocked(false)
{
}

void
CEditorMainMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eUseHTMLSourceEditorBox:
			CFilePicker *fPicker =
					(CFilePicker *)FindPaneByID(eHTMLSourceEditorFilePicker);
			XP_ASSERT(fPicker);
			if (!fPicker->WasSet() && *(Int32 *)ioParam)
			{	// If the user has clicked the checkbox and the file picker is not set
				// then we may want to trigger the file picker
				if (mNeedsPrefs)
				{
					// If mNeedsPrefs, then we are setting up the pane. If the picker
					// is not set (can happen if the app file was physically deleted),
					// then we need to unset the "use" check box.
					LGACheckbox *checkbox =
							(LGACheckbox *)FindPaneByID(inMessage);
					XP_ASSERT(checkbox);
					checkbox->SetValue(false);
				}
				else
				{
					fPicker->ListenToMessage(msg_Browse, nil);
					if (!fPicker->WasSet())
					{	// If the file picker is still unset, that means that the user
						// cancelled the file browse so we don't want the checkbox set.
						LGACheckbox *checkbox =
								(LGACheckbox *)FindPaneByID(inMessage);
						XP_ASSERT(checkbox);
						checkbox->SetValue(false);
					}
				}
			}
			break;
		case eUseImageEditorBox:
			fPicker = (CFilePicker *)FindPaneByID(eImageEditorFilePicker);
			XP_ASSERT(fPicker);
			if (!fPicker->WasSet() && *(Int32 *)ioParam)
			{	// If the user has clicked the checkbox and the file picker is not set
				// then we may want to trigger the file picker
				if (mNeedsPrefs)
				{
					// If mNeedsPrefs, then we are setting up the pane. If the picker
					// is not set (can happen if the app file was physically deleted),
					// then we need to unset the "use" check box.
					LGACheckbox *checkbox =
							(LGACheckbox *)FindPaneByID(inMessage);
					XP_ASSERT(checkbox);
					checkbox->SetValue(false);
				}
				else
				{
					fPicker->ListenToMessage(msg_Browse, nil);
					if (!fPicker->WasSet())
					{	// If the file picker is still unset, that means that the user
						// cancelled the file browse so we don't want the checkbox set.
						LGACheckbox *checkbox =
								(LGACheckbox *)FindPaneByID(inMessage);
						XP_ASSERT(checkbox);
						checkbox->SetValue(false);
					}
				}
			}
			break;
		case msg_FolderChanged:
			PaneIDT	checkBoxID;
			switch (((CFilePicker *)ioParam)->GetPaneID())
			{
				case eHTMLSourceEditorFilePicker:
					checkBoxID = eUseHTMLSourceEditorBox;
					break;
				case eImageEditorFilePicker:
					checkBoxID = eUseImageEditorBox;
					break;
			}
			LGACheckbox *checkbox = (LGACheckbox *)FindPaneByID(checkBoxID);
			XP_ASSERT(checkbox);
			checkbox->SetValue(true);
			break;
		case eChooseLocalFileButton:
			SetEditFieldWithLocalFileURL(	eNewDocTemplateURLEditField,
											mNewDocURLLocked);
			break;
		case eRestoreDefaultButton:
			RestoreDefaultURL();
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void
CEditorMainMediator::RestoreDefaultURL()
{
	ReadDefaultPref(eNewDocTemplateURLEditField);
}

Boolean
CEditorMainMediator::SaveIntervalValidationFunc(CValidEditField *saveInterval)
{
	// If the checkbox isn't set then this value is really
	// ignored, so I will only put up the alert if the checkbox
	// is set, but I will force a valid value in any case.

	Boolean		result = true;

	// force valid value
	if (1 > saveInterval->GetValue())
	{
		int32	newInterval = 10;
		PREF_GetDefaultIntPref("editor.auto_save_delay", &newInterval);
		saveInterval->SetValue(newInterval);
		saveInterval->SelectAll();
		result = false;
	}
	if (!result)	// if the value is within the range, who cares
	{
		// Check for the check box...
		// We are assuming that the checkbox is a sub of the field's superview.
		LView	*superView = saveInterval->GetSuperView();
		XP_ASSERT(superView);
		LGACheckbox	*checkbox =
				(LGACheckbox *)superView->FindPaneByID(eAutoSaveCheckBox);
		XP_ASSERT(checkbox);
		if (checkbox->GetValue())
		{
			StPrepareForDialog	prepare;
			::StopAlert(1067, NULL);
		}
		else
		{
			result = true;	// go ahead and let them switch (after correcting the value)
							// if the checkbox isn't set
		}
	}
	return result;
}

//-----------------------------------
void CEditorMainMediator::LoadMainPane()
//-----------------------------------
{
	CPrefsMediator::LoadMainPane();
	SetValidationFunction(eAutoSaveIntervalEditField, SaveIntervalValidationFunc);
}

#endif // EDITOR

enum
{
	eMaintainLinksBox = 13401,
	eKeepImagesBox,
	ePublishLocationField,
	eBrowseLocationField
};

enum
{
	eOnlineRButton = 13501,
	eOfflineRButton,
	eAskMeRButton
};

enum
{
	eImagesBox = 13901,
	eJaveBox,
	eJavaScriptBox,
	eStyleSheetBox,
	eAutoInstallBox,
	eFTPPasswordBox,
	eCookieAlwaysRButton,
	eCookieOriginatingServerRButton,
	eCookieNeverRButton,
	eCookieWarningBox
};

enum
{
	eDiskCacheEditField = 14101,
	eClearDiskCacheButton,
	eOncePerSessionRButton,
	eEveryUseRButton,
	eNeverRButton,
	eCacheLocationFilePicker
};

CAdvancedCacheMediator::CAdvancedCacheMediator(LStream*)
:	CPrefsMediator(class_ID)
{
}

void CAdvancedCacheMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case eClearDiskCacheButton:
			ClearDiskCacheNow();
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void CAdvancedCacheMediator::ClearDiskCacheNow()
{
	int32 originalDiskCacheSize;	// in Kbytes
	PREF_GetIntPref("browser.cache.disk_cache_size", &originalDiskCacheSize);
	if (originalDiskCacheSize)	// if the current cache size is zero then do nothing
	{
		UDesktop::Deactivate();
		short	response = ::CautionAlert(eClearDiskCacheDialog, NULL);	
		UDesktop::Activate();
		if (1 == response)
		{
			TrySetCursor(watchCursor);
			
			// This is the approved way to clear the cache
	        PREF_SetIntPref("browser.cache.disk_cache_size", 0);
	        PREF_SetIntPref("browser.cache.disk_cache_size", originalDiskCacheSize);
	        
			::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );
		}
	}
}

enum
{
	eManualProxyDialogResID = 12006,
	eDirectConnectRButton = 14201,
	eManualRButton,
	eViewProxyConfigButton,
	eAutoProxyConfigRButton,
	eConfigURLTextBox,
	eReloadButton,
	eFTPEditField,
	eFTPPortEditField,
	eGopherEditField,
	eGopherPortEditField,
	eHTTPEditField,
	eHTTPPortEditField,
	eSSLEditField,
	eSSLPortEditField,
	eWAISEditField,
	eWAISPortEditField,
	eNoProxyEditField,
	eSOCKSEditField,
	eSOCKSPortEditField
};

CAdvancedProxiesMediator::Protocol::Protocol():
mProtocolServer(nil),
mServerLocked(false),
mProtocolPort(0),
mPortLocked(false)
{
}

CAdvancedProxiesMediator::Protocol::Protocol(const Protocol& original):
mProtocolServer(nil),
mServerLocked(original.mServerLocked),
mProtocolPort(original.mProtocolPort),
mPortLocked(original.mPortLocked)
{
	if (original.mProtocolServer)
	{
		mProtocolServer = XP_STRDUP(original.mProtocolServer);
	}
	// else set to nil above
}

CAdvancedProxiesMediator::Protocol::~Protocol()
{
	if (mProtocolServer)
	{
		XP_FREE(mProtocolServer);
	}
}

void
CAdvancedProxiesMediator::Protocol::LoadFromPref(const char *serverPrefName,
													const char *portPrefName)
{
	mServerLocked	= PREF_PrefIsLocked(serverPrefName);
	mPortLocked		= PREF_PrefIsLocked(portPrefName);
	PREF_CopyCharPref(serverPrefName, &mProtocolServer);
	PREF_GetIntPref(portPrefName, &mProtocolPort);
}

void
CAdvancedProxiesMediator::Protocol::WriteToPref(	const char *serverPrefName,
													const char *portPrefName)
{
	if (!mServerLocked)
	{
		char	*currentStringValue = nil;
		PREF_CopyCharPref(serverPrefName, &currentStringValue);
		if (!currentStringValue || strcmp(currentStringValue, mProtocolServer))
		{
			PREF_SetCharPref(serverPrefName, mProtocolServer);
		}
		if (currentStringValue)
		{
			XP_FREE(currentStringValue);
		}
	}
	if (!mPortLocked)
	{
		int32	currentIntValue;
		currentIntValue = mProtocolPort + 1;	// anything != mCheckInterval
		PREF_GetIntPref(portPrefName, &currentIntValue);
		if (currentIntValue != mProtocolPort)
		{
			PREF_SetIntPref(portPrefName, mProtocolPort);
		}
	}
}

void
CAdvancedProxiesMediator::Protocol::PreEdit(	LView *dialog,
												ResIDT serverEditID,
												ResIDT portEditID)
{
	LEditField		*server =
						(LEditField *)dialog->FindPaneByID(serverEditID);
	XP_ASSERT(server);
	if (mProtocolServer)
	{
		LStr255	protocolServerPStr(mProtocolServer);
		server->SetDescriptor(protocolServerPStr);
		server->SelectAll();
	}
	if (mServerLocked)
	{
		server->Disable();
	}
	LEditField *port =
					(LEditField *)dialog->FindPaneByID(portEditID);
	XP_ASSERT(port);
	port->SetValue(mProtocolPort);
	port->SelectAll();
	if (mPortLocked)
	{
		port->Disable();
	}
}

void
CAdvancedProxiesMediator::Protocol::PostEdit(LView *dialog,
												ResIDT serverEditID,
												ResIDT portEditID)
{
	LEditField		*server =
						(LEditField *)dialog->FindPaneByID(serverEditID);
	XP_ASSERT(server);
	Str255	fieldValue;
	if (!mServerLocked)
	{
		server->GetDescriptor(fieldValue);
		int		stringLength = fieldValue[0];
		char	*serverString = (char *)XP_ALLOC(stringLength + 1);
		if (serverString)
		{
			strncpy(serverString, (char *)&fieldValue[1], stringLength + 1);
			serverString[stringLength] = '\0';
			if (mProtocolServer)
			{
				XP_FREE(mProtocolServer);
			}
			mProtocolServer = serverString;
		}
	}
	LEditField *port =
					(LEditField *)dialog->FindPaneByID(portEditID);
	XP_ASSERT(port);
	if (!mPortLocked)
	{
		mProtocolPort = port->GetValue();
	}
}



CAdvancedProxiesMediator::ManualProxy::ManualProxy():
mNoProxyList(nil),
mNoProxyListLocked(false)
{
}

CAdvancedProxiesMediator::ManualProxy::ManualProxy(const ManualProxy& original):
mFTP(original.mFTP),
mGopher(original.mGopher),
mHTTP(original.mHTTP),
mSSL(original.mSSL),
mWAIS(original.mWAIS),
mNoProxyList(nil),
mNoProxyListLocked(original.mNoProxyListLocked),
mSOCKS(original.mSOCKS)
{
	if (original.mNoProxyList)
	{
		mNoProxyList = XP_STRDUP(original.mNoProxyList);
	}
	// else set to nil above
}

CAdvancedProxiesMediator::ManualProxy::~ManualProxy()
{
	XP_FREEIF(mNoProxyList);
}

void
CAdvancedProxiesMediator::ManualProxy::LoadPrefs()
{
	mFTP.LoadFromPref("network.proxy.ftp", "network.proxy.ftp_port");
	mGopher.LoadFromPref("network.proxy.gopher", "network.proxy.gopher_port");
	mHTTP.LoadFromPref("network.proxy.http", "network.proxy.http_port");
	mWAIS.LoadFromPref("network.proxy.wais", "network.proxy.wais_port");
	mSSL.LoadFromPref("network.proxy.ssl", "network.proxy.ssl_port");

	mNoProxyListLocked		= PREF_PrefIsLocked("network.proxy.no_proxies_on");
	PREF_CopyCharPref("network.proxy.no_proxies_on", &mNoProxyList);

	mSOCKS.LoadFromPref("network.hosts.socks_server", "network.hosts.socks_serverport");
}

void
CAdvancedProxiesMediator::ManualProxy::WritePrefs()
{
	mFTP.WriteToPref("network.proxy.ftp", "network.proxy.ftp_port");
	mGopher.WriteToPref("network.proxy.gopher", "network.proxy.gopher_port");
	mHTTP.WriteToPref("network.proxy.http", "network.proxy.http_port");
	mWAIS.WriteToPref("network.proxy.wais", "network.proxy.wais_port");
	mSSL.WriteToPref("network.proxy.ssl", "network.proxy.ssl_port");

	char	*currentStringValue = nil;
	if (!mNoProxyListLocked)
	{
		PREF_CopyCharPref("network.proxy.no_proxies_on", &currentStringValue);
		if (!currentStringValue || strcmp(currentStringValue, mNoProxyList))
		{
			PREF_SetCharPref("network.proxy.no_proxies_on", mNoProxyList);
		}
		if (currentStringValue)
		{
			XP_FREE(currentStringValue);
		}
	}
	mSOCKS.WriteToPref("network.hosts.socks_server", "network.hosts.socks_serverport");
}


Boolean
CAdvancedProxiesMediator::ManualProxy::EditPrefs()
{
	Boolean	result = false;

	StDialogHandler	handler(eManualProxyDialogResID, nil);
	LWindow			*dialog = handler.GetDialog();

	mFTP.PreEdit(dialog, eFTPEditField, eFTPPortEditField);
	mGopher.PreEdit(dialog, eGopherEditField, eGopherPortEditField);
	mHTTP.PreEdit(dialog, eHTTPEditField, eHTTPPortEditField);
	mSSL.PreEdit(dialog, eSSLEditField, eSSLPortEditField);
	mWAIS.PreEdit(dialog, eWAISEditField, eWAISPortEditField);

	LEditField		*noProxy =
						(LEditField *)dialog->FindPaneByID(eNoProxyEditField);
	XP_ASSERT(noProxy);
	if (mNoProxyList)
	{
		LStr255	noProxyPStr(mNoProxyList);
		noProxy->SetDescriptor(noProxyPStr);
		noProxy->SelectAll();
	}
	if (mNoProxyListLocked)
	{
		noProxy->Disable();
	}

	mSOCKS.PreEdit(dialog, eSOCKSEditField, eSOCKSPortEditField);

	// Run the dialog
	MessageT message = msg_Nothing;
	do
	{
		message = handler.DoDialog();
		if (msg_Nothing != message && msg_OK != message  && msg_Cancel != message)
		{
			message = msg_Nothing;
		}
	} while (msg_Nothing == message);

	if (msg_OK == message)
	{
		result = true;
		// read all the controls back to the struct

		mFTP.PostEdit(dialog, eFTPEditField, eFTPPortEditField);
		mGopher.PostEdit(dialog, eGopherEditField, eGopherPortEditField);
		mHTTP.PostEdit(dialog, eHTTPEditField, eHTTPPortEditField);
		mSSL.PostEdit(dialog, eSSLEditField, eSSLPortEditField);
		mWAIS.PostEdit(dialog, eWAISEditField, eWAISPortEditField);

		Str255	fieldValue;
		if (!mNoProxyListLocked)
		{
			noProxy->GetDescriptor(fieldValue);
			int		stringLength = fieldValue[0];
			char	*noProxyString = (char *)XP_ALLOC(stringLength + 1);
			if (noProxyString)
			{
				strncpy(noProxyString, (char *)&fieldValue[1], stringLength + 1);
				noProxyString[stringLength] = '\0';
				if (mNoProxyList)
				{
					XP_FREE(mNoProxyList);
				}
				mNoProxyList = noProxyString;
			}
		}
		mSOCKS.PostEdit(dialog, eSOCKSEditField, eSOCKSPortEditField);
	}
	else
	{
		result = false;
	}
	return result;
}


CAdvancedProxiesMediator::CAdvancedProxiesMediator(LStream*)
:	CPrefsMediator(class_ID)
,	mManualProxy(nil)
{
}

void
CAdvancedProxiesMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
		case msg_ControlClicked:
			break;
		case eViewProxyConfigButton:
			LGARadioButton	*theButton = (LGARadioButton *)FindPaneByID(eManualRButton);
			XP_ASSERT(theButton);
			if (!theButton->GetValue())
			{
				theButton->SetValue(true);
			}
			ManualProxy	*tempManualProxy;
			if (!mManualProxy)
			{
				tempManualProxy = new ManualProxy;
				tempManualProxy->LoadPrefs();
			}
			else
			{
				tempManualProxy = new ManualProxy(*mManualProxy);
			}
			if (tempManualProxy->EditPrefs())
			{
				if (mManualProxy)
				{
					delete mManualProxy;
				}
				mManualProxy = tempManualProxy;
			}
			else
			{
				delete tempManualProxy;
			}
			break;
		case eReloadButton:
			LEditField	*theField = (LEditField *)FindPaneByID(eConfigURLTextBox);
			XP_ASSERT(theField);
			Str255	fieldValue;
			theField->GetDescriptor(fieldValue);
			int		stringLength = fieldValue[0];
			char	*prefString = (char *)XP_ALLOC(stringLength + 1);
			XP_ASSERT(prefString);
			strncpy(prefString, (char *)&fieldValue[1], stringLength);
			prefString[stringLength] = '\0';
			NET_SetProxyServer(PROXY_AUTOCONF_URL, prefString);
			NET_ReloadProxyConfig(nil);	// The context is unused in the function.
			XP_FREE(prefString);
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

//-----------------------------------
void CAdvancedProxiesMediator::WritePrefs()
//-----------------------------------
{
	if (mManualProxy)
		mManualProxy->WritePrefs();
}

#ifdef MOZ_MAIL_NEWS
enum
{
	eMessageSizeLimitBox = 13801,
	eMessageSizeLimitEditField,
	eKeepByDaysRButton,
	eKeepDaysEditField,
	eKeepAllRButton,
	eKeepByCountRButton,
	eKeepCountEditField,
	eKeepOnlyUnreadBox,
	eCompactingBox,
	eCompactingEditField
};

CAdvancedDiskSpaceMediator::CAdvancedDiskSpaceMediator(LStream*)
:	CPrefsMediator(class_ID)
{
}

void
CAdvancedDiskSpaceMediator::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch (inMessage)
	{
//		case eNewsMessagesMoreButton:
//			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

// static
Boolean
CAdvancedDiskSpaceMediator::DiskSpaceValidationFunc(CValidEditField *editField)
{
	Boolean okeyDokey = true;

	// force valid value
	if (1 > editField->GetValue())
	{
		const char *prefName = nil;
		PaneIDT	controllingPaneID;
		switch (editField->GetPaneID())
		{
			case eMessageSizeLimitEditField:
				controllingPaneID = eMessageSizeLimitBox;
				prefName = "mail.max_size";
				break;
			case eKeepDaysEditField:
				controllingPaneID = eKeepByDaysRButton;
				prefName = "news.keep.days";
				break;
			case eKeepCountEditField:
				controllingPaneID = eKeepByCountRButton;
				prefName = "news.keep.count";
				break;
			case eCompactingEditField:
				controllingPaneID = eCompactingBox;
				prefName = "mail.purge_threshhold";
				break;
			default:
				break;	// XP_ASSERT()?
		}
		LControl* controllingPane = nil;
		if (controllingPaneID)
			controllingPane =
			(LControl *)((editField->GetSuperView())->FindPaneByID(controllingPaneID));
		XP_ASSERT(controllingPane);
		int32 newValue;
		PREF_GetDefaultIntPref(prefName, &newValue);
		editField->SetValue(newValue);
		editField->SelectAll();
		okeyDokey = (controllingPane->GetValue() == 0);
	}
	if (!okeyDokey)
	{											// If the value is within the range, or
		StPrepareForDialog prepare;				// the control is not on, who cares?
		::StopAlert(1067, NULL);
	}
	return okeyDokey;
}

//-----------------------------------
void CAdvancedDiskSpaceMediator::LoadMainPane()
//-----------------------------------
{
	CPrefsMediator::LoadMainPane();
	SetValidationFunction(eMessageSizeLimitEditField, DiskSpaceValidationFunc);
	SetValidationFunction(eKeepDaysEditField, DiskSpaceValidationFunc);
	SetValidationFunction(eKeepCountEditField, DiskSpaceValidationFunc);
	SetValidationFunction(eCompactingEditField, DiskSpaceValidationFunc);
}

#endif // MOZ_MAIL_NEWS

//-----------------------------------
void UAssortedPrefMediators::RegisterViewClasses()
//-----------------------------------
{
	RegisterClass_(CEditFieldControl);


	RegisterClass_(CColorButton);
	RegisterClass_( CFilePicker);

	RegisterClass_(COtherSizeDialog);
	RegisterClass_(CSizePopup);
	
	RegisterClass_( CGAEditBroadcaster);
	RegisterClass_(CValidEditField);
	RegisterClass_( LCicnButton);
//	RegisterClass_( 'sbox', (ClassCreatorFunc)OneClickLListBox::CreateOneClickLListBox );
	RegisterClass_(OneRowLListBox);	// added by ftang
	UPrefControls::RegisterPrefControlViews();
} // CPrefsDialog::RegisterViewClasses

