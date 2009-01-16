/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2009 Mozilla Foundation
 * Portions of comments Copyright 2004-2008 Apple Computer, Inc., Mozilla 
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
 * Please edit TreeBuilder.java instead and regenerate.
 */

#ifndef nsHtml5TreeBuilder_h__
#define nsHtml5TreeBuilder_h__

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

class nsHtml5Tokenizer;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5HtmlAttributes;
class nsHtml5StackNode;
class nsHtml5UTF16Buffer;
class nsHtml5Portability;

class nsHtml5TreeBuilder
{
  private:
    static jArray<PRUnichar,PRInt32> ISINDEX_PROMPT;
    static jArray<nsString*,PRInt32> QUIRKY_PUBLIC_IDS;
    nsHtml5StackNode* MARKER;
    static nsIAtom* HTML_LOCAL;
    PRInt32 mode;
    PRInt32 originalMode;
    PRInt32 foreignFlag;
  protected:
    nsHtml5Tokenizer* tokenizer;
  private:
    nsHtml5Parser* documentModeHandler;
    PRBool scriptingEnabled;
    PRBool needToDropLF;
    PRBool fragment;
    nsIAtom* contextName;
    PRInt32 contextNamespace;
    jArray<nsHtml5StackNode*,PRInt32> stack;
    PRInt32 currentPtr;
    jArray<nsHtml5StackNode*,PRInt32> listOfActiveFormattingElements;
    PRInt32 listPtr;
    nsIContent* formPointer;
    nsIContent* headPointer;
    PRBool reportingDoctype;
  public:
    void startTokenization(nsHtml5Tokenizer* self);
    void doctype(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, PRBool forceQuirks);
    void comment(PRUnichar* buf, PRInt32 start, PRInt32 length);
    void characters(PRUnichar* buf, PRInt32 start, PRInt32 length);
    void eof();
    void endTokenization();
    void startTag(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, PRBool selfClosing);
    static nsString* extractCharsetFromContent(nsString* attributeValue);
  private:
    void checkMetaCharset(nsHtml5HtmlAttributes* attributes);
  public:
    void endTag(nsHtml5ElementName* elementName);
  private:
    void endSelect();
    PRInt32 findLastInTableScopeOrRootTbodyTheadTfoot();
    PRInt32 findLast(nsIAtom* name);
    PRInt32 findLastInTableScope(nsIAtom* name);
    PRInt32 findLastInScope(nsIAtom* name);
    PRInt32 findLastInScopeHn();
    PRBool hasForeignInScope();
    void generateImpliedEndTagsExceptFor(nsIAtom* name);
    void generateImpliedEndTags();
    PRBool isSecondOnStackBody();
    void documentModeInternal(nsHtml5DocumentMode m, nsString* publicIdentifier, nsString* systemIdentifier, PRBool html4SpecificAdditionalErrorChecks);
    PRBool isAlmostStandards(nsString* publicIdentifier, nsString* systemIdentifier);
    PRBool isQuirky(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, PRBool forceQuirks);
    void closeTheCell(PRInt32 eltPos);
    PRInt32 findLastInTableScopeTdTh();
    void clearStackBackTo(PRInt32 eltPos);
    void resetTheInsertionMode();
    void implicitlyCloseP();
    PRBool clearLastStackSlot();
    PRBool clearLastListSlot();
    void push(nsHtml5StackNode* node);
    void append(nsHtml5StackNode* node);
    void insertMarker();
    void clearTheListOfActiveFormattingElementsUpToTheLastMarker();
    PRBool isCurrent(nsIAtom* name);
    void removeFromStack(PRInt32 pos);
    void removeFromStack(nsHtml5StackNode* node);
    void removeFromListOfActiveFormattingElements(PRInt32 pos);
    void adoptionAgencyEndTag(nsIAtom* name);
    void insertIntoStack(nsHtml5StackNode* node, PRInt32 position);
    void insertIntoListOfActiveFormattingElements(nsHtml5StackNode* formattingClone, PRInt32 bookmark);
    PRInt32 findInListOfActiveFormattingElements(nsHtml5StackNode* node);
    PRInt32 findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsIAtom* name);
    PRInt32 findLastOrRoot(nsIAtom* name);
    PRInt32 findLastOrRoot(PRInt32 group);
    void addAttributesToBody(nsHtml5HtmlAttributes* attributes);
    void pushHeadPointerOntoStack();
    void reconstructTheActiveFormattingElements();
    void insertIntoFosterParent(nsIContent* child);
    PRBool isInStack(nsHtml5StackNode* node);
    void pop();
    void appendCharMayFoster(PRUnichar* buf, PRInt32 i);
    PRBool isTainted();
    void appendHtmlElementToDocumentAndPush(nsHtml5HtmlAttributes* attributes);
    void appendHtmlElementToDocumentAndPush();
    void appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushBodyElement(nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushBodyElement();
    void appendToCurrentNodeAndPushFormElementMayFoster(nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushFormattingElementMayFoster(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElement(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElementMayFoster(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElementMayFosterNoScoping(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElementMayFosterCamelCase(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElementMayFoster(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, nsIContent* form);
    void appendVoidElementToCurrentMayFoster(PRInt32 ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent* form);
    void appendVoidElementToCurrentMayFoster(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendVoidElementToCurrentMayFosterCamelCase(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendVoidElementToCurrent(PRInt32 ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent* form);
  protected:
    void accumulateCharacters(PRUnichar* buf, PRInt32 start, PRInt32 length);
    void flushCharacters();
    void requestSuspension();
    nsIContent* createElement(PRInt32 ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes);
    nsIContent* createElement(PRInt32 ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent* form);
    nsIContent* createHtmlElementSetAsRoot(nsHtml5HtmlAttributes* attributes);
    void detachFromParent(nsIContent* element);
    PRBool hasChildren(nsIContent* element);
    nsIContent* shallowClone(nsIContent* element);
    void appendElement(nsIContent* child, nsIContent* newParent);
    void appendChildrenToNewParent(nsIContent* oldParent, nsIContent* newParent);
    nsIContent* parentElementFor(nsIContent* child);
    void insertBefore(nsIContent* child, nsIContent* sibling, nsIContent* parent);
    void insertCharactersBefore(PRUnichar* buf, PRInt32 start, PRInt32 length, nsIContent* sibling, nsIContent* parent);
    void appendCharacters(nsIContent* parent, PRUnichar* buf, PRInt32 start, PRInt32 length);
    void appendComment(nsIContent* parent, PRUnichar* buf, PRInt32 start, PRInt32 length);
    void appendCommentToDocument(PRUnichar* buf, PRInt32 start, PRInt32 length);
    void addAttributesToElement(nsIContent* element, nsHtml5HtmlAttributes* attributes);
  public:
    void startCoalescing();
  protected:
    void start(PRBool fragment);
  public:
    void endCoalescing();
  protected:
    void end();
    void appendDoctypeToDocument(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier);
    void elementPushed(PRInt32 ns, nsIAtom* name, nsIContent* node);
    void elementPopped(PRInt32 ns, nsIAtom* name, nsIContent* node);
    nsIContent* currentNode();
  public:
    PRBool isScriptingEnabled();
    void setScriptingEnabled(PRBool scriptingEnabled);
    void setDocumentModeHandler(nsHtml5Parser* documentModeHandler);
    void setReportingDoctype(PRBool reportingDoctype);
    PRBool inForeign();
    static void initializeStatics();
    static void releaseStatics();

#include "nsHtml5TreeBuilderHSupplement.h"
};

#ifdef nsHtml5TreeBuilder_cpp__
jArray<nsString*,PRInt32> nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS = nsnull;
nsIAtom* nsHtml5TreeBuilder::HTML_LOCAL = nsnull;
#endif

#define NS_HTML5TREE_BUILDER_OTHER 0
#define NS_HTML5TREE_BUILDER_A 1
#define NS_HTML5TREE_BUILDER_BASE 2
#define NS_HTML5TREE_BUILDER_BODY 3
#define NS_HTML5TREE_BUILDER_BR 4
#define NS_HTML5TREE_BUILDER_BUTTON 5
#define NS_HTML5TREE_BUILDER_CAPTION 6
#define NS_HTML5TREE_BUILDER_COL 7
#define NS_HTML5TREE_BUILDER_COLGROUP 8
#define NS_HTML5TREE_BUILDER_FORM 9
#define NS_HTML5TREE_BUILDER_FRAME 10
#define NS_HTML5TREE_BUILDER_FRAMESET 11
#define NS_HTML5TREE_BUILDER_IMAGE 12
#define NS_HTML5TREE_BUILDER_INPUT 13
#define NS_HTML5TREE_BUILDER_ISINDEX 14
#define NS_HTML5TREE_BUILDER_LI 15
#define NS_HTML5TREE_BUILDER_LINK 16
#define NS_HTML5TREE_BUILDER_MATH 17
#define NS_HTML5TREE_BUILDER_META 18
#define NS_HTML5TREE_BUILDER_SVG 19
#define NS_HTML5TREE_BUILDER_HEAD 20
#define NS_HTML5TREE_BUILDER_HR 22
#define NS_HTML5TREE_BUILDER_HTML 23
#define NS_HTML5TREE_BUILDER_NOBR 24
#define NS_HTML5TREE_BUILDER_NOFRAMES 25
#define NS_HTML5TREE_BUILDER_NOSCRIPT 26
#define NS_HTML5TREE_BUILDER_OPTGROUP 27
#define NS_HTML5TREE_BUILDER_OPTION 28
#define NS_HTML5TREE_BUILDER_P 29
#define NS_HTML5TREE_BUILDER_PLAINTEXT 30
#define NS_HTML5TREE_BUILDER_SCRIPT 31
#define NS_HTML5TREE_BUILDER_SELECT 32
#define NS_HTML5TREE_BUILDER_STYLE 33
#define NS_HTML5TREE_BUILDER_TABLE 34
#define NS_HTML5TREE_BUILDER_TEXTAREA 35
#define NS_HTML5TREE_BUILDER_TITLE 36
#define NS_HTML5TREE_BUILDER_TR 37
#define NS_HTML5TREE_BUILDER_XMP 38
#define NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT 39
#define NS_HTML5TREE_BUILDER_TD_OR_TH 40
#define NS_HTML5TREE_BUILDER_DD_OR_DT 41
#define NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 42
#define NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET 43
#define NS_HTML5TREE_BUILDER_PRE_OR_LISTING 44
#define NS_HTML5TREE_BUILDER_B_OR_BIG_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U 45
#define NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL 46
#define NS_HTML5TREE_BUILDER_IFRAME 47
#define NS_HTML5TREE_BUILDER_EMBED_OR_IMG 48
#define NS_HTML5TREE_BUILDER_AREA_OR_BASEFONT_OR_BGSOUND_OR_SPACER_OR_WBR 49
#define NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU 50
#define NS_HTML5TREE_BUILDER_ADDRESS_OR_DIR_OR_ARTICLE_OR_ASIDE_OR_DATAGRID_OR_DETAILS_OR_DIALOG_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_NAV_OR_SECTION 51
#define NS_HTML5TREE_BUILDER_CODE_OR_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR 52
#define NS_HTML5TREE_BUILDER_RT_OR_RP 53
#define NS_HTML5TREE_BUILDER_COMMAND_OR_EVENT_SOURCE 54
#define NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE 55
#define NS_HTML5TREE_BUILDER_MGLYPH_OR_MALIGNMARK 56
#define NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT 57
#define NS_HTML5TREE_BUILDER_ANNOTATION_XML 58
#define NS_HTML5TREE_BUILDER_FOREIGNOBJECT_OR_DESC 59
#define NS_HTML5TREE_BUILDER_NOEMBED 60
#define NS_HTML5TREE_BUILDER_FIELDSET 61
#define NS_HTML5TREE_BUILDER_OUTPUT_OR_LABEL 62
#define NS_HTML5TREE_BUILDER_OBJECT 63
#define NS_HTML5TREE_BUILDER_FONT 64
#define NS_HTML5TREE_BUILDER_INITIAL 0
#define NS_HTML5TREE_BUILDER_BEFORE_HTML 1
#define NS_HTML5TREE_BUILDER_BEFORE_HEAD 2
#define NS_HTML5TREE_BUILDER_IN_HEAD 3
#define NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT 4
#define NS_HTML5TREE_BUILDER_AFTER_HEAD 5
#define NS_HTML5TREE_BUILDER_IN_BODY 6
#define NS_HTML5TREE_BUILDER_IN_TABLE 7
#define NS_HTML5TREE_BUILDER_IN_CAPTION 8
#define NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP 9
#define NS_HTML5TREE_BUILDER_IN_TABLE_BODY 10
#define NS_HTML5TREE_BUILDER_IN_ROW 11
#define NS_HTML5TREE_BUILDER_IN_CELL 12
#define NS_HTML5TREE_BUILDER_IN_SELECT 13
#define NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE 14
#define NS_HTML5TREE_BUILDER_AFTER_BODY 15
#define NS_HTML5TREE_BUILDER_IN_FRAMESET 16
#define NS_HTML5TREE_BUILDER_AFTER_FRAMESET 17
#define NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY 18
#define NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET 19
#define NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA 20
#define NS_HTML5TREE_BUILDER_CHARSET_INITIAL 0
#define NS_HTML5TREE_BUILDER_CHARSET_C 1
#define NS_HTML5TREE_BUILDER_CHARSET_H 2
#define NS_HTML5TREE_BUILDER_CHARSET_A 3
#define NS_HTML5TREE_BUILDER_CHARSET_R 4
#define NS_HTML5TREE_BUILDER_CHARSET_S 5
#define NS_HTML5TREE_BUILDER_CHARSET_E 6
#define NS_HTML5TREE_BUILDER_CHARSET_T 7
#define NS_HTML5TREE_BUILDER_CHARSET_EQUALS 8
#define NS_HTML5TREE_BUILDER_CHARSET_SINGLE_QUOTED 9
#define NS_HTML5TREE_BUILDER_CHARSET_DOUBLE_QUOTED 10
#define NS_HTML5TREE_BUILDER_CHARSET_UNQUOTED 11
#define NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK PR_INT32_MAX
#define NS_HTML5TREE_BUILDER_IN_FOREIGN 0
#define NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN 1


#endif

