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

#pragma once

#include "CPrefsMediator.h"

//======================================
#pragma mark
class UAssortedPrefMediators
//======================================
{
public:
	static void RegisterViewClasses();
}; // class UAssortedPrefMediators

class LTextColumn;
class CMIMEListPane;
class CDragOrderTextList;
class LPopupButton;

//======================================
#pragma mark
class CAppearanceMainMediator
//======================================
:	public CPrefsMediator
{
	public:
		
		enum { class_ID = PrefPaneID::eAppearance_Main };
		CAppearanceMainMediator(LStream*);

		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();
};

//======================================
#pragma mark
class CAppearanceFontsMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eAppearance_Fonts };
		CAppearanceFontsMediator(LStream*);
		virtual	~CAppearanceFontsMediator()
			{ delete [] mEncodings;	}

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);

		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();

	private:

		struct Encoding
		{
			Encoding():
			mCSID(0),
			mPropFontSize(12),
			mFixedFontSize(10),
			mPropFontLocked(false),
			mFixedFontLocked(false),
			mPropFontSizeLocked(false),
			mFixedFontSizeLocked(false)
			{
			};
			CStr31			mLanguageGroup;
			unsigned short	mCSID;
			CStr31			mPropFont;
			Boolean			mPropFontLocked;
			CStr31			mFixedFont;
			Boolean			mFixedFontLocked;
			unsigned short	mPropFontSize;
			Boolean			mPropFontSizeLocked;
			unsigned short	mFixedFontSize;
			Boolean			mFixedFontSizeLocked;
		};


				void	LoadEncodings();
				void	ReadEncodings(Handle hndl);
				void	SetEncodingWithPref(Encoding& enc);
				void	SetPrefWithEncoding(const Encoding& enc);
				void	UpdateMenus();
				void	UpdateEncoding(PaneIDT changedMenuID);
				void	PopulateEncodingsMenus(PaneIDT menuID);
				void	WriteEncodingPrefs();
				Int16	GetFontSize(LPopupButton* whichPopup);
				int		GetSelectEncMenuItem();
				void	FontMenuChanged(PaneIDT changedMenuID);
				void	SizeMenuChanged(PaneIDT changedMenuID);

		Encoding	*mEncodings;
		int			mEncodingsCount;
};

//======================================
#pragma mark
class CAppearanceColorsMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eAppearance_Colors };
		CAppearanceColorsMediator(LStream*);
		virtual	~CAppearanceColorsMediator() {};
		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);
	private:
				void	UseDefaults();
};

//======================================
#pragma mark
class CBrowserMainMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eBrowser_Main };
		CBrowserMainMediator(LStream*);
		virtual	~CBrowserMainMediator() {if (mCurrentURL) XP_FREE(mCurrentURL);};

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);

		virtual	void	LoadPrefs();
		virtual	void	UpdateFromIC();
		virtual	void	LoadMainPane();
		virtual	void	WritePrefs();

		static Boolean ExpireDaysValidationFunc(CValidEditField *daysTilExpire);

	private:
				void	URLChoosingButtons(Boolean enable);
				void	ExpireNow();
		Boolean	mHomePageURLLocked;
		char	*mCurrentURL;
};

//======================================
#pragma mark
class CBrowserLanguagesMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eBrowser_Languages };
		
		CBrowserLanguagesMediator(LStream*);
		virtual	~CBrowserLanguagesMediator();

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);

		virtual	void	LoadMainPane();
		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();

	private:
				void	LoadBuiltInArrays();
				void	SetLanguageListWithPref(const char *prefName,
												PaneIDT	languageListID,
												Boolean	&locked);
				void	SetPrefWithLanguageList(const char *prefName,
												PaneIDT	languageListID,
												Boolean	locked);
				char	*GetLanguageDisplayString(const char *languageCode);
							// For a code from the prefs, allocates (with XP_ALLOC())
							// the proper string for display.
							//	If languageCode is fr then returns French [fr]
							// 	If the languageCode is not a builtin code, then
							// 		a copy of languageCode is returned.
				void	UpdateButtons(LTextColumn *languageList = nil);
							// OK button.
				Boolean	GetNewLanguage(char *&newLanguage);
				Boolean	GetNewUniqueLanguage(char *&newLanguage);
				void	FillAddList(LTextColumn *list);
				char	*AppendLanguageCode(const char *originalString,
											const char *stringToAdd);
		Boolean	mLanguagesLocked;
		Int32	mBuiltInCount;
		char	**mLanguageStringArray;
		char	**mLangaugeCodeArray;
			// The following two members are only meaningful when the Add Language
			// dialog is up.
		Boolean	mOtherTextEmpty;	// This means that the "Other:" text field is empty.
		LTextColumn	*mAddLanguageList;
};

//======================================
#pragma mark
class CBrowserApplicationsMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eBrowser_Applications };
		CBrowserApplicationsMediator(LStream*);
		virtual	~CBrowserApplicationsMediator() {};

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);

		virtual	void	LoadMainPane();
		virtual	void	WritePrefs();
	private:
		Boolean			mModified;			// Have any MIMEs been modified
		CMIMEListPane*	mMIMETable;			// Scrolling table of MIME types
		CMimeList		mDeletedTypes;

				void	EditMimeEntry();
				void	NewMimeEntry();
				void	DeleteMimeEntry();
};

#ifdef EDITOR
//======================================
#pragma mark
class CEditorMainMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eEditor_Main };
		CEditorMainMediator(LStream*);
		virtual	~CEditorMainMediator() {};

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);

		virtual	void	LoadMainPane();

		static Boolean	SaveIntervalValidationFunc(CValidEditField *saveInterval);

	private:
				void	RestoreDefaultURL();
		Boolean	mNewDocURLLocked;
};
#endif // EDITOR

//======================================
#pragma mark
class CAdvancedCacheMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eAdvanced_Cache };
		CAdvancedCacheMediator(LStream*);
		virtual	~CAdvancedCacheMediator() {};

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);
	private:
				void	ClearDiskCacheNow();
};

//======================================
#pragma mark
class CAdvancedProxiesMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eAdvanced_Proxies };
		CAdvancedProxiesMediator(LStream*);
		virtual	~CAdvancedProxiesMediator() {};

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);
		virtual	void	WritePrefs();
		class Protocol
		{
			public:
				char	*mProtocolServer;
				Boolean	mServerLocked;
				Int32	mProtocolPort;
				Boolean	mPortLocked;

						Protocol();
						Protocol(const Protocol& original);
						~Protocol();
				void	LoadFromPref(const char *serverPrefName, const char *portPrefName);
				void	WriteToPref(const char *serverPrefName, const char *portPrefName);
				void	PreEdit(LView *dialog, ResIDT serverEditID, ResIDT portEditID);
				void	PostEdit(LView *dialog, ResIDT serverEditID, ResIDT portEditID);
		};

		class ManualProxy
		{
			public:
				Protocol	mFTP;
				Protocol	mGopher;
				Protocol	mHTTP;
				Protocol	mSSL;
				Protocol	mWAIS;
				char		*mNoProxyList;
				Boolean		mNoProxyListLocked;
				Protocol	mSOCKS;

						ManualProxy();
						ManualProxy(const ManualProxy& original);
						~ManualProxy();
				void	LoadPrefs();
				void	WritePrefs();
				Boolean	EditPrefs();
		};
	private:
		ManualProxy	*mManualProxy;
};

#ifdef MOZ_MAIL_NEWS
//======================================
#pragma mark
class CAdvancedDiskSpaceMediator : public CPrefsMediator
//======================================
{
	public:

		enum { class_ID = PrefPaneID::eAdvanced_DiskSpace };
		CAdvancedDiskSpaceMediator(LStream*);
		virtual	~CAdvancedDiskSpaceMediator() {};

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);

		virtual	void	LoadMainPane();

		static Boolean	DiskSpaceValidationFunc(CValidEditField *editField);
};
#endif // MOZ_MAIL_NEWS

