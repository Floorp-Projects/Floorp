/*
 * Copyright (c) 2005, 2006, 2007 Henri Sivonen
 * Copyright (c) 2007-2008 Mozilla Foundation
 * Portions of comments Copyright 2004-2007 Apple Computer, Inc., Mozilla 
 * Foundation, and Opera Software ASA.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Please edit Tokenizer.java instead and regenerate.
 */

#ifndef nsHtml5Tokenizer_h__
#define nsHtml5Tokenizer_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5NamedCharacters.h"
#include "nsHtml5Parser.h"
#include "nsHtml5StringLiterals.h"
#include "nsHtml5Atoms.h"

class nsHtml5Parser;

class nsHtml5TreeBuilder;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5HtmlAttributes;
class nsHtml5StackNode;
class nsHtml5UTF16Buffer;
class nsHtml5Portability;

class nsHtml5Tokenizer
{
  private:
    static PRUnichar LT_GT[];
    static PRUnichar LT_SOLIDUS[];
    static PRUnichar RSQB_RSQB[];
    static PRUnichar REPLACEMENT_CHARACTER[];
    static PRUnichar LF[];
    static PRUnichar CDATA_LSQB[];
    static PRUnichar OCTYPE[];
    static PRUnichar UBLIC[];
    static PRUnichar YSTEM[];
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar TITLE_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> TITLE_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar SCRIPT_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> SCRIPT_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar STYLE_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> STYLE_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar PLAINTEXT_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> PLAINTEXT_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar XMP_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> XMP_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar TEXTAREA_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> TEXTAREA_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar IFRAME_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> IFRAME_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar NOEMBED_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> NOEMBED_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar NOSCRIPT_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> NOSCRIPT_ARR;
    #ifdef nsHtml5Tokenizer_cpp__
    static PRUnichar NOFRAMES_ARR_DATA[];
    #endif
    static jArray<PRUnichar,PRInt32> NOFRAMES_ARR;
    nsHtml5TreeBuilder* tokenHandler;
    nsHtml5Parser* encodingDeclarationHandler;
    PRUnichar prev;
    PRInt32 line;
    PRInt32 linePrev;
    PRInt32 col;
    PRInt32 colPrev;
    PRBool nextCharOnNewLine;
    PRInt32 stateSave;
    PRInt32 returnStateSave;
    PRInt32 index;
    PRBool forceQuirks;
    PRUnichar additional;
    PRInt32 entCol;
    PRInt32 lo;
    PRInt32 hi;
    PRInt32 candidate;
    PRInt32 strBufMark;
    PRInt32 prevValue;
    PRInt32 value;
    PRBool seenDigits;
    PRInt32 pos;
    PRInt32 endPos;
    PRUnichar* buf;
    PRInt32 cstart;
    nsString* publicId;
    nsString* systemId;
    jArray<PRUnichar,PRInt32> strBuf;
    PRInt32 strBufLen;
    jArray<PRUnichar,PRInt32> longStrBuf;
    PRInt32 longStrBufLen;
    nsHtml5HtmlAttributes* attributes;
    jArray<PRUnichar,PRInt32> bmpChar;
    jArray<PRUnichar,PRInt32> astralChar;
    PRBool alreadyWarnedAboutPrivateUseCharacters;
    nsHtml5ElementName* contentModelElement;
    jArray<PRUnichar,PRInt32> contentModelElementNameAsArray;
    PRBool endTag;
    nsHtml5ElementName* tagName;
    nsHtml5AttributeName* attributeName;
    PRBool shouldAddAttributes;
    PRBool html4;
    PRBool alreadyComplainedAboutNonAscii;
    PRBool metaBoundaryPassed;
    nsIAtom* doctypeName;
    nsString* publicIdentifier;
    nsString* systemIdentifier;
    PRInt32 mappingLangToXmlLang;
    PRBool shouldSuspend;
    PRBool confident;
  public:
    nsHtml5Tokenizer(nsHtml5TreeBuilder* tokenHandler, nsHtml5Parser* encodingDeclarationHandler);
    void initLocation(nsString* newPublicId, nsString* newSystemId);
    void destructor();
    void setContentModelFlag(PRInt32 contentModelFlag, nsIAtom* contentModelElement);
    void setContentModelFlag(PRInt32 contentModelFlag, nsHtml5ElementName* contentModelElement);
  private:
    void contentModelElementToArray();
  public:
    nsString* getPublicId();
    nsString* getSystemId();
    PRInt32 getLineNumber();
    PRInt32 getColumnNumber();
    void notifyAboutMetaBoundary();
    nsHtml5HtmlAttributes* emptyAttributes();
  private:
    void detachStrBuf();
    void detachLongStrBuf();
    void clearStrBufAndAppendCurrentC(PRUnichar c);
    void clearStrBufAndAppendForceWrite(PRUnichar c);
    void clearStrBufForNextState();
    void appendStrBuf(PRUnichar c);
    void appendStrBufForceWrite(PRUnichar c);
    nsString* strBufToString();
    nsIAtom* strBufToLocal();
    void emitStrBuf();
    void clearLongStrBufForNextState();
    void clearLongStrBuf();
    void clearLongStrBufAndAppendCurrentC();
    void clearLongStrBufAndAppendToComment(PRUnichar c);
    void appendLongStrBuf(PRUnichar c);
    void appendSecondHyphenToBogusComment();
    void adjustDoubleHyphenAndAppendToLongStrBuf(PRUnichar c);
    void appendLongStrBuf(jArray<PRUnichar,PRInt32> buffer, PRInt32 offset, PRInt32 length);
    void appendLongStrBuf(jArray<PRUnichar,PRInt32> arr);
    void appendStrBufToLongStrBuf();
    nsString* longStrBufToString();
    void emitComment(PRInt32 provisionalHyphens);
    PRBool isPrivateUse(PRUnichar c);
    PRBool isAstralPrivateUse(PRInt32 c);
    PRBool isNonCharacter(PRInt32 c);
    void flushChars();
    void resetAttributes();
    nsHtml5ElementName* strBufToElementNameString();
    PRInt32 emitCurrentTagToken(PRBool selfClosing);
    void attributeNameComplete();
    void addAttributeWithoutValue();
    void addAttributeWithValue();
  public:
    void start();
    PRBool tokenizeBuffer(nsHtml5UTF16Buffer* buffer);
  private:
    void stateLoop(PRInt32 state, PRUnichar c, PRBool reconsume, PRInt32 returnState);
    void rememberAmpersandLocation();
    void bogusDoctype();
    void bogusDoctypeWithoutQuirks();
    void emitOrAppendStrBuf(PRInt32 returnState);
    void handleNcrValue(PRInt32 returnState);
  public:
    void eof();
  private:
    PRUnichar read();
  public:
    void internalEncodingDeclaration(nsString* internalCharset);
  private:
    void emitOrAppend(jArray<PRUnichar,PRInt32> val, PRInt32 returnState);
    void emitOrAppendOne(PRUnichar* val, PRInt32 returnState);
  public:
    void end();
    void requestSuspension();
    PRBool isAlreadyComplainedAboutNonAscii();
    void becomeConfident();
    PRBool isNextCharOnNewLine();
    PRBool isPrevCR();
    PRInt32 getLine();
    PRInt32 getCol();
    static void initializeStatics();
    static void releaseStatics();
};

#ifdef nsHtml5Tokenizer_cpp__
PRUnichar nsHtml5Tokenizer::LT_GT[] = { '<', '>' };
PRUnichar nsHtml5Tokenizer::LT_SOLIDUS[] = { '<', '/' };
PRUnichar nsHtml5Tokenizer::RSQB_RSQB[] = { ']', ']' };
PRUnichar nsHtml5Tokenizer::REPLACEMENT_CHARACTER[] = { 0xfffd };
PRUnichar nsHtml5Tokenizer::LF[] = { '\n' };
PRUnichar nsHtml5Tokenizer::CDATA_LSQB[] = { 'C', 'D', 'A', 'T', 'A', '[' };
PRUnichar nsHtml5Tokenizer::OCTYPE[] = { 'o', 'c', 't', 'y', 'p', 'e' };
PRUnichar nsHtml5Tokenizer::UBLIC[] = { 'u', 'b', 'l', 'i', 'c' };
PRUnichar nsHtml5Tokenizer::YSTEM[] = { 'y', 's', 't', 'e', 'm' };
PRUnichar nsHtml5Tokenizer::TITLE_ARR_DATA[] = { 't', 'i', 't', 'l', 'e' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::TITLE_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, TITLE_ARR_DATA);
PRUnichar nsHtml5Tokenizer::SCRIPT_ARR_DATA[] = { 's', 'c', 'r', 'i', 'p', 't' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::SCRIPT_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, SCRIPT_ARR_DATA);
PRUnichar nsHtml5Tokenizer::STYLE_ARR_DATA[] = { 's', 't', 'y', 'l', 'e' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::STYLE_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, STYLE_ARR_DATA);
PRUnichar nsHtml5Tokenizer::PLAINTEXT_ARR_DATA[] = { 'p', 'l', 'a', 'i', 'n', 't', 'e', 'x', 't' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::PLAINTEXT_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, PLAINTEXT_ARR_DATA);
PRUnichar nsHtml5Tokenizer::XMP_ARR_DATA[] = { 'x', 'm', 'p' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::XMP_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, XMP_ARR_DATA);
PRUnichar nsHtml5Tokenizer::TEXTAREA_ARR_DATA[] = { 't', 'e', 'x', 't', 'a', 'r', 'e', 'a' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::TEXTAREA_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, TEXTAREA_ARR_DATA);
PRUnichar nsHtml5Tokenizer::IFRAME_ARR_DATA[] = { 'i', 'f', 'r', 'a', 'm', 'e' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::IFRAME_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, IFRAME_ARR_DATA);
PRUnichar nsHtml5Tokenizer::NOEMBED_ARR_DATA[] = { 'n', 'o', 'e', 'm', 'b', 'e', 'd' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::NOEMBED_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, NOEMBED_ARR_DATA);
PRUnichar nsHtml5Tokenizer::NOSCRIPT_ARR_DATA[] = { 'n', 'o', 's', 'c', 'r', 'i', 'p', 't' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::NOSCRIPT_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, NOSCRIPT_ARR_DATA);
PRUnichar nsHtml5Tokenizer::NOFRAMES_ARR_DATA[] = { 'n', 'o', 'f', 'r', 'a', 'm', 'e', 's' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::NOFRAMES_ARR = J_ARRAY_STATIC(PRUnichar, PRInt32, NOFRAMES_ARR_DATA);
#endif

#define NS_HTML5TOKENIZER_DATA 0
#define NS_HTML5TOKENIZER_RCDATA 1
#define NS_HTML5TOKENIZER_CDATA 2
#define NS_HTML5TOKENIZER_PLAINTEXT 3
#define NS_HTML5TOKENIZER_TAG_OPEN 49
#define NS_HTML5TOKENIZER_CLOSE_TAG_OPEN_PCDATA 50
#define NS_HTML5TOKENIZER_TAG_NAME 58
#define NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME 4
#define NS_HTML5TOKENIZER_ATTRIBUTE_NAME 5
#define NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME 6
#define NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE 7
#define NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED 8
#define NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED 9
#define NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED 10
#define NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED 11
#define NS_HTML5TOKENIZER_BOGUS_COMMENT 12
#define NS_HTML5TOKENIZER_MARKUP_DECLARATION_OPEN 13
#define NS_HTML5TOKENIZER_DOCTYPE 14
#define NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME 15
#define NS_HTML5TOKENIZER_DOCTYPE_NAME 16
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME 17
#define NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER 18
#define NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED 19
#define NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED 20
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER 21
#define NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER 22
#define NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED 23
#define NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED 24
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER 25
#define NS_HTML5TOKENIZER_BOGUS_DOCTYPE 26
#define NS_HTML5TOKENIZER_COMMENT_START 27
#define NS_HTML5TOKENIZER_COMMENT_START_DASH 28
#define NS_HTML5TOKENIZER_COMMENT 29
#define NS_HTML5TOKENIZER_COMMENT_END_DASH 30
#define NS_HTML5TOKENIZER_COMMENT_END 31
#define NS_HTML5TOKENIZER_CLOSE_TAG_OPEN_NOT_PCDATA 32
#define NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN 33
#define NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE 34
#define NS_HTML5TOKENIZER_DOCTYPE_UBLIC 35
#define NS_HTML5TOKENIZER_DOCTYPE_YSTEM 36
#define NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE 37
#define NS_HTML5TOKENIZER_CONSUME_NCR 38
#define NS_HTML5TOKENIZER_CHARACTER_REFERENCE_LOOP 39
#define NS_HTML5TOKENIZER_HEX_NCR_LOOP 41
#define NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP 42
#define NS_HTML5TOKENIZER_HANDLE_NCR_VALUE 43
#define NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG 44
#define NS_HTML5TOKENIZER_CDATA_START 45
#define NS_HTML5TOKENIZER_CDATA_SECTION 46
#define NS_HTML5TOKENIZER_CDATA_RSQB 47
#define NS_HTML5TOKENIZER_CDATA_RSQB_RSQB 48
#define NS_HTML5TOKENIZER_TAG_OPEN_NON_PCDATA 51
#define NS_HTML5TOKENIZER_ESCAPE_EXCLAMATION 52
#define NS_HTML5TOKENIZER_ESCAPE_EXCLAMATION_HYPHEN 53
#define NS_HTML5TOKENIZER_ESCAPE 54
#define NS_HTML5TOKENIZER_ESCAPE_HYPHEN 55
#define NS_HTML5TOKENIZER_ESCAPE_HYPHEN_HYPHEN 56
#define NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN 57
#define NS_HTML5TOKENIZER_LEAD_OFFSET (0xD800 - (0x10000 >> 10))
#define NS_HTML5TOKENIZER_SURROGATE_OFFSET (0x10000 - (0xD800 << 10) - 0xDC00)
#define NS_HTML5TOKENIZER_BUFFER_GROW_BY 1024


#endif

