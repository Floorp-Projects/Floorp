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
   SpellHandler.h -- class definition for the Spell Handler class
   Created: Richard Hess <rhess@netscape.com>, 12-May-97.
 */



#ifndef _xfe_spellhandler_h
#define _xfe_spellhandler_h

#include "structs.h"
#include "mozilla.h"
#include "xfe.h"

class ISpellChecker;

typedef struct PRLibrary PRLibrary;

class XFE_SpellCheck;

struct xfe_lang_tag {
	int  language;
	int  dialect;
};

struct xfe_spell_data {
	ISpellChecker    *spell;
	XFE_SpellCheck   *interface;
	Widget            dialog;
	Widget            list;
	Widget            combo;
	Widget            replace;
	Widget            replaceAll;
	Widget            check;
	Widget            ignore;
	Widget            ignoreAll;
	Widget            learn;
	Widget            stop;
	Widget            status;
	Widget            text;
	char             *nuText;
	char             *xWord;
	xfe_lang_tag     *langTags;
	int               langCount;
	int               langIndx;
	XP_Bool           inList;
	XP_Bool           inCheck;
	XP_Bool           isOk;
	XP_Bool           isDone;
};

struct xfe_spell_tag {
	int               action;
	xfe_spell_data   *data;
};

class XFE_SpellCheck
{
public:
	XFE_SpellCheck(ISpellChecker* spell, MWContext *context);
	virtual ~XFE_SpellCheck();

	XP_Bool    ProcessError(xfe_spell_tag* tag);
	XP_Bool    ProcessDocument(xfe_spell_tag* tag);

	// WARNING... [ caution needs to be applied to this one ]
	//
	void       resetVars();

	virtual void     IgnoreHilitedText(int AllInstances) = 0;
	virtual void     RemoveAllErrorHilites() = 0;
	virtual char    *GetFirstError() = 0;
	virtual char    *GetNextError() = 0;
	virtual char    *GetBuffer() = 0;
	virtual XP_Bool  GetSelection(int32 &SelStart, int32 &SelEnd) = 0;
	virtual void     ReplaceHilitedText(const char *NewText, 
										XP_Bool AllInstances) = 0;
  
protected:
	ISpellChecker   *m_spellChecker;
	int              m_bufferSize;
	int              m_xpDelta;
	MWContext       *m_contextData;
	char            *m_misspelledWord;
	int32            m_selStart;
	int32            m_selEnd;

private:

};

class XFE_HtmlSpellCheck: public XFE_SpellCheck
{
public:
	XFE_HtmlSpellCheck(ISpellChecker *spell, MWContext *context);
	virtual ~XFE_HtmlSpellCheck();

	virtual void     IgnoreHilitedText(int AllInstances);
	virtual void     RemoveAllErrorHilites();
	virtual char    *GetFirstError();
	virtual char    *GetNextError();
	virtual char    *GetBuffer();
	virtual XP_Bool  GetSelection(int32 &SelStart, int32 &SelEnd);
	virtual void     ReplaceHilitedText(const char *NewText, 
										XP_Bool AllInstances);

private:

};

class XFE_TextSpellCheck: public XFE_SpellCheck
{
public:
	XFE_TextSpellCheck(ISpellChecker *spell, MWContext *context);
	virtual ~XFE_TextSpellCheck();
  
	virtual void     IgnoreHilitedText(int AllInstances);
	virtual void     RemoveAllErrorHilites();
	virtual char    *GetFirstError();
	virtual char    *GetNextError();
	virtual char    *GetBuffer();
	virtual XP_Bool  GetSelection(int32 &SelStart, int32 &SelEnd);
	virtual void     ReplaceHilitedText(const char *NewText, 
										XP_Bool AllInstances);

private:
	Widget  m_textWidget;
	int     m_offset;
	int     m_len;
	XP_Bool m_dirty;
};

#define XSP_CACHE_SIZE 8

class XFE_SpellHandler
{
public:
	XFE_SpellHandler(MWContext *context);
	virtual ~XFE_SpellHandler();

	void       PopupDialog(MWContext* context, char* eWord);
	void       UpdateGUI();
	void       DialogDestroyed();

	XP_Bool    ProcessDocument(MWContext* context, XP_Bool isHtml);
	XP_Bool    ReprocessDocument();
	XP_Bool    ProcessError(xfe_spell_tag* tag);
	XP_Bool    IsActive();
	XP_Bool    IsAvailable();

protected:
	void       initLanguageList();
    void       initSpellChecker();
	void       nukeSpellChecker();
	void       updateLang(int indx);
	char      *getSpellCheckerDir();
	char      *getPersonalDicPath();
	char      *getLanguageString(int lang, int dialect);

private:
	static PRLibrary* m_spellCheckerLib;
	static XP_Bool   m_triedToLoad;

	XP_Bool          m_active;
	xfe_spell_data   m_data;
	xfe_spell_tag    m_tags[XSP_CACHE_SIZE];
};

Boolean  xfe_SpellCheckerAvailable(MWContext* context);
Boolean  xfe_EditorSpellCheck(MWContext* context);
Boolean  xfe_TextSpellCheck(MWContext* context);

#endif /* _xfe_spellhandler_h */


