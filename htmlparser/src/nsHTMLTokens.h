/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 *  
 * This file contains the declarations for all the 
 * HTML specific token types that our HTML tokenizer
 * delegate understands. 
 *
 * If you want to add a new kind of token, this is 
 * the place to do it. You should also add a bit of glue
 * code to the HTML tokenizer delegate class.
 */

#ifndef HTMLTOKENS_H
#define HTMLTOKENS_H

#include "nsToken.h"
#include <iostream.h>

// If you define these to true then crlf sequences and whitespace come
// through the scanner as seperate tokens.
#undef TOKENIZE_CRLF
#undef TOKENIZE_WHITESPACE

class CScanner;

enum eHTMLTokenTypes {
  eToken_unknown=2000,

  eToken_start,       eToken_end,     eToken_comment, eToken_entity,
  eToken_whitespace,  eToken_newline, eToken_text,    eToken_attribute,
  eToken_script,      eToken_style,   eToken_skippedcontent,  //used in cases like <SCRIPT> where we skip over script content.
  eToken_last
};

//*** This enum is used to define the known universe of HTML tags.
//*** The use of this table doesn't preclude of from using non-standard
//*** tags. It simply makes normal tag handling more efficient.
enum eHTMLTags
{
  eHTMLTag_unknown=0,   eHTMLTag_doctype,     eHTMLTag_a,         eHTMLTag_acronym,
  eHTMLTag_address,     eHTMLTag_applet,      eHTMLTag_area,      eHTMLTag_bold,
  eHTMLTag_base,        eHTMLTag_basefont,    eHTMLTag_bdo,       eHTMLTag_big,         
  eHTMLTag_blink,       eHTMLTag_blockquote,  eHTMLTag_body,      eHTMLTag_br,          
  eHTMLTag_button,      eHTMLTag_caption,     eHTMLTag_center,    
  eHTMLTag_certificate, eHTMLTag_cite,
  eHTMLTag_code,        eHTMLTag_col,         eHTMLTag_colgroup,  eHTMLTag_dd,
  eHTMLTag_del,         eHTMLTag_dfn,         eHTMLTag_div,       eHTMLTag_dir,       
  eHTMLTag_dl,          eHTMLTag_dt,          eHTMLTag_em,        eHTMLTag_embed,
  eHTMLTag_fieldset,    eHTMLTag_font,        eHTMLTag_footer,  
  eHTMLTag_form,        eHTMLTag_frame,       eHTMLTag_frameset,
  eHTMLTag_h1,          eHTMLTag_h2,          eHTMLTag_h3,        eHTMLTag_h4,
  eHTMLTag_h5,          eHTMLTag_h6,          eHTMLTag_head,      eHTMLTag_header,
  eHTMLTag_hr,			    eHTMLTag_html,        eHTMLTag_iframe,    eHTMLTag_ilayer,
  eHTMLTag_italic,      eHTMLTag_img,         eHTMLTag_ins,       eHTMLTag_input,       
  eHTMLTag_isindex,     eHTMLTag_kbd,         eHTMLTag_keygen,
  eHTMLTag_label,       eHTMLTag_layer,       eHTMLTag_legend,    eHTMLTag_listitem,
  eHTMLTag_link,        eHTMLTag_listing,     eHTMLTag_map,       eHTMLTag_marquee,
  eHTMLTag_math,        eHTMLTag_menu,        eHTMLTag_meta,      eHTMLTag_newline,
  eHTMLTag_noembed,     eHTMLTag_noframes,    eHTMLTag_nolayer,   eHTMLTag_noscript,  
  eHTMLTag_note,        eHTMLTag_object,      eHTMLTag_ol,
  eHTMLTag_option,      eHTMLTag_paragraph,   eHTMLTag_param,     eHTMLTag_plaintext,   
  eHTMLTag_pre,         eHTMLTag_quotation,   eHTMLTag_strike,    eHTMLTag_samp,        
  eHTMLTag_script,      eHTMLTag_select,      eHTMLTag_server,    eHTMLTag_small,     
  eHTMLTag_spacer,      eHTMLTag_span,
  eHTMLTag_strong,      eHTMLTag_style,       eHTMLTag_sub,       eHTMLTag_sup,         
  eHTMLTag_table,       eHTMLTag_tbody,       eHTMLTag_td,        eHTMLTag_tfoot,   
  eHTMLTag_thead,       eHTMLTag_th,          eHTMLTag_tr,        eHTMLTag_textarea,    
  eHTMLTag_title,       eHTMLTag_tt,          eHTMLTag_monofont,  eHTMLTag_u,
  eHTMLTag_ul,          eHTMLTag_var,         eHTMLTag_wbr,       eHTMLTag_whitespace,
  eHTMLTag_xmp,         
  eHTMLTag_userdefined
};

//*** This enum is used to define the known universe of HTML attributes.
//*** The use of this table doesn't preclude of from using non-standard
//*** attributes. It simply makes normal tag handling more efficient.
enum eHTMLAttributes {
  eHTMLAttr_abbrev,     eHTMLAttr_above,      eHTMLAttr_alt,      eHTMLAttr_array,
  eHTMLAttr_author,     eHTMLAttr_background, eHTMLAttr_banner,   eHTMLAttr_below,
  eHTMLAttr_bgsound,    eHTMLAttr_box,        eHTMLAttr_bt,       eHTMLAttr_class,
  eHTMLAttr_comment,    eHTMLAttr_credit,     eHTMLAttr_dir,      eHTMLAttr_figure,
  eHTMLAttr_footnote,   eHTMLAttr_height,     eHTMLAttr_id,       eHTMLAttr_lang,
  eHTMLAttr_math,       eHTMLAttr_name,       eHTMLAttr_nextid,   eHTMLAttr_nobreak,
  eHTMLAttr_note,       eHTMLAttr_option,     eHTMLAttr_overlay,  eHTMLAttr_person,
  eHTMLAttr_public,     eHTMLAttr_range,      eHTMLAttr_root,     eHTMLAttr_sgml,
  eHTMLAttr_sqrt,       eHTMLAttr_src,        eHTMLAttr_style,    eHTMLAttr_text,
  eHTMLAttr_title,      eHTMLAttr_wordbreak,  eHTMLAttr_width,    eHTMLAttr_xmp
};

PRInt32         ConsumeQuotedString(PRUnichar aChar,nsString& aString,CScanner* aScanner);
PRInt32         ConsumeAttributeText(PRUnichar aChar,nsString& aString,CScanner* aScanner);
PRInt32         FindEntityIndex(const char* aBuffer,PRInt32 aBufLen=-1);
eHTMLTags       DetermineHTMLTagType(const nsString& aString);
eHTMLTokenTypes DetermineTokenType(const nsString& aString);


class CHTMLToken : public CToken {
  public:
                        CHTMLToken(const nsString& aString);
    virtual eHTMLTags   GetHTMLTag() {return mTagType;}
            void        SetHTMLTag(eHTMLTags aTagType) {mTagType=aTagType; return;}
  protected:
            eHTMLTags   mTagType;
};


class CStartToken: public CHTMLToken {
	public:
                        CStartToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner* aScanner);
    virtual const char*	GetClassName(void) {return "start";}
		virtual PRInt32     GetTokenType(void) {return eToken_start;}
    virtual eHTMLTags   GetHTMLTag();
      		  void        SetAttributed(PRBool aValue) {mAttributed=aValue;}
            PRBool		  IsAttributed(void) {return mAttributed;}
    virtual void        DebugDumpSource(ostream& out);
  
  protected:
            PRBool        mAttributed;      

};


class CEndToken: public CHTMLToken {
	public:
                        CEndToken(const nsString& aString);
   	virtual PRInt32     Consume(PRUnichar aChar,CScanner* aScanner);
    virtual const char*	GetClassName(void) {return "/end";}
		virtual PRInt32	    GetTokenType(void) {return eToken_end;}
    virtual eHTMLTags   GetHTMLTag();
    virtual void        DebugDumpSource(ostream& out);
};


class CCommentToken: public CHTMLToken {
	public:
                        CCommentToken(const nsString& aString);
   	virtual PRInt32     Consume(PRUnichar aChar,CScanner* aScanner);
    virtual const char*	GetClassName(void);
		virtual PRInt32	    GetTokenType(void) {return eToken_comment;}
            char        mLeadingChar;
};


class CEntityToken : public CHTMLToken {
	public:
                        CEntityToken(const nsString& aString);
    virtual const char*	GetClassName(void) {return "entity";}
		virtual PRInt32	    GetTokenType(void) {return eToken_entity;}
            PRInt32     TranslateToUnicode(void);
   	virtual PRInt32     Consume(PRUnichar aChar,CScanner* aScanner);
    static  PRInt32     ConsumeEntity(PRUnichar aChar,nsString& aString,CScanner* aScanner);
    static  PRInt32     TranslateToUnicode(nsString& aString);
    static  PRInt32     FindEntityIndex(const char* aBuffer,PRInt32 aBufLen=-1);
    static  PRBool      VerifyEntityTable(void);
    static  PRInt32     ReduceEntities(nsString& aString);
    virtual	void			  DebugDumpSource(ostream& out);

  private:
    static  PRInt32     mEntityTokenCount;
};


#ifdef TOKENIZE_WHITESPACE
class CWhitespaceToken: public CHTMLToken {
	public:
                        CWhitespaceToken(const nsString& aString);
   	virtual PRInt32     Consume(PRUnichar aChar,CScanner* aScanner);
    virtual const char*	GetClassName(void) {return "ws";}
		virtual PRInt32			GetTokenType(void) {return eToken_whitespace;}
};
#endif

class CTextToken: public CHTMLToken {
	public:
                        CTextToken(const nsString& aString);
   	virtual PRInt32     Consume(PRUnichar aChar,CScanner* aScanner);
    virtual const char*	GetClassName(void) {return "text";}
		virtual PRInt32			GetTokenType(void) {return eToken_text;}
};


class CAttributeToken: public CHTMLToken {
	public:
                          CAttributeToken(const nsString& aString);
   	virtual PRInt32       Consume(PRUnichar aChar,CScanner* aScanner);
    virtual const char*	  GetClassName(void) {return "attr";}
		virtual PRInt32			  GetTokenType(void) {return eToken_attribute;}
    virtual nsString&     GetKey(void) {return mTextKey;}
    virtual void          DebugDumpToken(ostream& out);
    virtual void          DebugDumpSource(ostream& out);
            PRBool        mLastAttribute;

  protected:
   	        nsString mTextKey;
};


#ifdef TOKENIZE_CRLF
class CNewlineToken: public CHTMLToken { 
	public:
                        CNewlineToken(const nsString& aString);
   	virtual PRInt32     Consume(PRUnichar aChar,CScanner* aScanner);
    virtual const char*	GetClassName(void) {return "crlf";}
		virtual PRInt32			GetTokenType(void) {return eToken_newline;}
};
#endif


class CScriptToken: public CHTMLToken {
	public:

                        CScriptToken(const nsString& aString);
    virtual const char*	GetClassName(void) {return "script";}
		virtual PRInt32			GetTokenType(void) {return eToken_script;}
    virtual	void			  DebugDumpSource(ostream& out) {CToken::DebugDumpSource(out);}
  protected:
};


class CStyleToken: public CHTMLToken {
	public:
                        CStyleToken(const nsString& aString);
    virtual const char*	GetClassName(void) {return "style";}
		virtual PRInt32			GetTokenType(void) {return eToken_style;}
    virtual	void			  DebugDumpSource(ostream& out) {CToken::DebugDumpSource(out);}
  protected:
};


class CSkippedContentToken: public CAttributeToken {
	public:
                        CSkippedContentToken(const nsString& aString);
   	virtual PRInt32     Consume(PRUnichar aChar,CScanner* aScanner);
    virtual const char*	GetClassName(void) {return "skipped";}
		virtual PRInt32			GetTokenType(void) {return eToken_skippedcontent;}
  protected:
};


#endif


