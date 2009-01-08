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

#define nsHtml5TreeBuilder_cpp__

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

#include "nsHtml5Tokenizer.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5HtmlAttributes.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5Portability.h"

#include "nsHtml5TreeBuilder.h"

void 
nsHtml5TreeBuilder::startTokenization(nsHtml5Tokenizer* self)
{
  tokenizer = self;
  stack = jArray<nsHtml5StackNode*,PRInt32>(64);
  listOfActiveFormattingElements = jArray<nsHtml5StackNode*,PRInt32>(64);
  needToDropLF = PR_FALSE;
  originalMode = NS_HTML5TREE_BUILDER_INITIAL;
  currentPtr = -1;
  listPtr = -1;
  formPointer = nsnull;
  start(fragment);
  startCoalescing();
  if (fragment) {
    nsIContent* elt = createHtmlElementSetAsRoot(tokenizer->emptyAttributes());
    nsHtml5StackNode* node = new nsHtml5StackNode(kNameSpaceID_XHTML, nsHtml5ElementName::HTML, elt);
    currentPtr++;
    stack[currentPtr] = node;
    resetTheInsertionMode();
    if (nsHtml5Atoms::title == contextName || nsHtml5Atoms::textarea == contextName) {
      tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_RCDATA, contextName);
    } else if (nsHtml5Atoms::style == contextName || nsHtml5Atoms::script == contextName || nsHtml5Atoms::xmp == contextName || nsHtml5Atoms::iframe == contextName || nsHtml5Atoms::noembed == contextName || nsHtml5Atoms::noframes == contextName || (scriptingEnabled && nsHtml5Atoms::noscript == contextName)) {
      tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, contextName);
    } else if (nsHtml5Atoms::plaintext == contextName) {
      tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_PLAINTEXT, contextName);
    } else {
      tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_DATA, contextName);
    }
    nsHtml5Portability::releaseElement(elt);
  } else {
    mode = NS_HTML5TREE_BUILDER_INITIAL;
    foreignFlag = NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN;
  }
}

void 
nsHtml5TreeBuilder::doctype(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, PRBool forceQuirks)
{
  needToDropLF = PR_FALSE;
  for (; ; ) {
    switch(foreignFlag) {
      case NS_HTML5TREE_BUILDER_IN_FOREIGN: {
        goto doctypeloop_end;
      }
      default: {
        switch(mode) {
          case NS_HTML5TREE_BUILDER_INITIAL: {
            if (reportingDoctype) {
              appendDoctypeToDocument(name, !publicIdentifier ? nsHtml5Portability::newEmptyString() : publicIdentifier, !systemIdentifier ? nsHtml5Portability::newEmptyString() : systemIdentifier);
            }
            if (isQuirky(name, publicIdentifier, systemIdentifier, forceQuirks)) {

              documentModeInternal(QUIRKS_MODE, publicIdentifier, systemIdentifier, PR_FALSE);
            } else if (isAlmostStandards(publicIdentifier, systemIdentifier)) {

              documentModeInternal(ALMOST_STANDARDS_MODE, publicIdentifier, systemIdentifier, PR_FALSE);
            } else {
              if (!((!publicIdentifier || nsHtml5Portability::literalEqualsString(nsHtml5StringLiterals::XSLT_generated, publicIdentifier)) && !systemIdentifier)) {

              }
              documentModeInternal(STANDARDS_MODE, publicIdentifier, systemIdentifier, PR_FALSE);
            }
            mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
            return;
          }
          default: {
            goto doctypeloop_end;
          }
        }
      }
    }
  }
  doctypeloop_end: ;

  return;
}

void 
nsHtml5TreeBuilder::comment(PRUnichar* buf, PRInt32 start, PRInt32 length)
{
  needToDropLF = PR_FALSE;
  for (; ; ) {
    switch(foreignFlag) {
      case NS_HTML5TREE_BUILDER_IN_FOREIGN: {
        goto commentloop_end;
      }
      default: {
        switch(mode) {
          case NS_HTML5TREE_BUILDER_INITIAL:
          case NS_HTML5TREE_BUILDER_BEFORE_HTML:
          case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY:
          case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
            appendCommentToDocument(buf, start, length);
            return;
          }
          case NS_HTML5TREE_BUILDER_AFTER_BODY: {
            flushCharacters();
            appendComment(stack[0]->node, buf, start, length);
            return;
          }
          default: {
            goto commentloop_end;
          }
        }
      }
    }
  }
  commentloop_end: ;
  flushCharacters();
  appendComment(stack[currentPtr]->node, buf, start, length);
  return;
}

void 
nsHtml5TreeBuilder::characters(PRUnichar* buf, PRInt32 start, PRInt32 length)
{
  if (needToDropLF) {
    if (buf[start] == '\n') {
      start++;
      length--;
      if (!length) {
        return;
      }
    }
    needToDropLF = PR_FALSE;
  }
  switch(mode) {
    case NS_HTML5TREE_BUILDER_IN_BODY:
    case NS_HTML5TREE_BUILDER_IN_CELL:
    case NS_HTML5TREE_BUILDER_IN_CAPTION: {
      reconstructTheActiveFormattingElements();
    }
    case NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA: {
      accumulateCharacters(buf, start, length);
      return;
    }
    default: {
      PRInt32 end = start + length;
      for (PRInt32 i = start; i < end; i++) {
        switch(buf[i]) {
          case ' ':
          case '\t':
          case '\n':
          case '\f': {
            switch(mode) {
              case NS_HTML5TREE_BUILDER_INITIAL:
              case NS_HTML5TREE_BUILDER_BEFORE_HTML:
              case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_TABLE:
              case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
              case NS_HTML5TREE_BUILDER_IN_ROW: {
                if (isTainted()) {
                  if (start < i) {
                    accumulateCharacters(buf, start, i - start);
                  }
                  reconstructTheActiveFormattingElements();
                  appendCharMayFoster(buf, i);
                  start = i + 1;
                }
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_HEAD:
              case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT:
              case NS_HTML5TREE_BUILDER_AFTER_HEAD:
              case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP:
              case NS_HTML5TREE_BUILDER_IN_FRAMESET:
              case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_BODY:
              case NS_HTML5TREE_BUILDER_IN_CELL:
              case NS_HTML5TREE_BUILDER_IN_CAPTION: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                reconstructTheActiveFormattingElements();
                goto charactersloop_end;
              }
              case NS_HTML5TREE_BUILDER_IN_SELECT:
              case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE: {
                goto charactersloop_end;
              }
              case NS_HTML5TREE_BUILDER_AFTER_BODY: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                reconstructTheActiveFormattingElements();
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY:
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                reconstructTheActiveFormattingElements();
                continue;
              }
            }
          }
          default: {
            switch(mode) {
              case NS_HTML5TREE_BUILDER_INITIAL: {
                documentModeInternal(QUIRKS_MODE, nsnull, nsnull, PR_FALSE);
                mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_BEFORE_HTML: {
                appendHtmlElementToDocumentAndPush();
                mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                mode = NS_HTML5TREE_BUILDER_IN_HEAD;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_HEAD: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                pop();
                mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }

                pop();
                mode = NS_HTML5TREE_BUILDER_IN_HEAD;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_HEAD: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                appendToCurrentNodeAndPushBodyElement();
                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_BODY:
              case NS_HTML5TREE_BUILDER_IN_CELL:
              case NS_HTML5TREE_BUILDER_IN_CAPTION: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                reconstructTheActiveFormattingElements();
                goto charactersloop_end;
              }
              case NS_HTML5TREE_BUILDER_IN_TABLE:
              case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
              case NS_HTML5TREE_BUILDER_IN_ROW: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                }
                reconstructTheActiveFormattingElements();
                appendCharMayFoster(buf, i);
                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                if (!currentPtr) {

                  start = i + 1;
                  continue;
                }
                pop();
                mode = NS_HTML5TREE_BUILDER_IN_TABLE;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_SELECT:
              case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE: {
                goto charactersloop_end;
              }
              case NS_HTML5TREE_BUILDER_AFTER_BODY: {


                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }

                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }

                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY: {

                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {

                mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
                i--;
                continue;
              }
            }
          }
        }
      }
      charactersloop_end: ;
      if (start < end) {
        accumulateCharacters(buf, start, end - start);
      }
    }
  }
}

void 
nsHtml5TreeBuilder::eof()
{
  flushCharacters();
  switch(foreignFlag) {
    case NS_HTML5TREE_BUILDER_IN_FOREIGN: {

      while (stack[currentPtr]->ns != kNameSpaceID_XHTML) {
        pop();
      }
      foreignFlag = NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN;
    }
    default:
      ; // fall through
  }
  for (; ; ) {
    switch(mode) {
      case NS_HTML5TREE_BUILDER_INITIAL: {
        documentModeInternal(QUIRKS_MODE, nsnull, nsnull, PR_FALSE);
        mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
        continue;
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HTML: {
        appendHtmlElementToDocumentAndPush();
        mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
        appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
        mode = NS_HTML5TREE_BUILDER_IN_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD: {
        if (currentPtr > 1) {

        }
        while (currentPtr > 0) {
          pop();
        }
        mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT: {

        while (currentPtr > 1) {
          pop();
        }
        mode = NS_HTML5TREE_BUILDER_IN_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_AFTER_HEAD: {
        appendToCurrentNodeAndPushBodyElement();
        mode = NS_HTML5TREE_BUILDER_IN_BODY;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
        if (!currentPtr) {

          goto eofloop_end;
        } else {
          pop();
          mode = NS_HTML5TREE_BUILDER_IN_TABLE;
          continue;
        }
      }
      case NS_HTML5TREE_BUILDER_IN_CAPTION:
      case NS_HTML5TREE_BUILDER_IN_CELL:
      case NS_HTML5TREE_BUILDER_IN_BODY: {
        goto eofloop_end;
      }
      case NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA: {

        if (originalMode == NS_HTML5TREE_BUILDER_AFTER_HEAD) {
          pop();
        }
        pop();
        mode = originalMode;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
      case NS_HTML5TREE_BUILDER_IN_ROW:
      case NS_HTML5TREE_BUILDER_IN_TABLE:
      case NS_HTML5TREE_BUILDER_IN_SELECT:
      case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE:
      case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
        if (currentPtr > 0) {

        }
        goto eofloop_end;
      }
      case NS_HTML5TREE_BUILDER_AFTER_BODY:
      case NS_HTML5TREE_BUILDER_AFTER_FRAMESET:
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY:
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET:
      default: {
        goto eofloop_end;
      }
    }
  }
  eofloop_end: ;
  while (currentPtr > 0) {
    pop();
  }
  if (!fragment) {
    pop();
  }
}

void 
nsHtml5TreeBuilder::endTokenization()
{
  stack = nsnull;
  listOfActiveFormattingElements = nsnull;
  endCoalescing();
  end();
}

void 
nsHtml5TreeBuilder::startTag(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, PRBool selfClosing)
{
  PRInt32 eltPos;
  needToDropLF = PR_FALSE;
  PRBool needsPostProcessing = PR_FALSE;
  starttagloop: for (; ; ) {
    PRInt32 group = elementName->group;
    nsIAtom* name = elementName->name;
    switch(foreignFlag) {
      case NS_HTML5TREE_BUILDER_IN_FOREIGN: {
        nsHtml5StackNode* currentNode = stack[currentPtr];
        PRInt32 currNs = currentNode->ns;
        PRInt32 currGroup = currentNode->group;
        if ((kNameSpaceID_XHTML == currNs) || (kNameSpaceID_MathML == currNs && ((NS_HTML5TREE_BUILDER_MGLYPH_OR_MALIGNMARK != group && NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT == currGroup) || (NS_HTML5TREE_BUILDER_SVG == group && NS_HTML5TREE_BUILDER_ANNOTATION_XML == currGroup))) || (kNameSpaceID_SVG == currNs && (NS_HTML5TREE_BUILDER_TITLE == currGroup || (NS_HTML5TREE_BUILDER_FOREIGNOBJECT_OR_DESC == currGroup)))) {
          needsPostProcessing = PR_TRUE;
        } else {
          switch(group) {
            case NS_HTML5TREE_BUILDER_B_OR_BIG_OR_EM_OR_FONT_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
            case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
            case NS_HTML5TREE_BUILDER_BODY:
            case NS_HTML5TREE_BUILDER_BR:
            case NS_HTML5TREE_BUILDER_CODE_OR_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR:
            case NS_HTML5TREE_BUILDER_DD_OR_DT:
            case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
            case NS_HTML5TREE_BUILDER_EMBED_OR_IMG:
            case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
            case NS_HTML5TREE_BUILDER_HEAD:
            case NS_HTML5TREE_BUILDER_HR:
            case NS_HTML5TREE_BUILDER_LI:
            case NS_HTML5TREE_BUILDER_META:
            case NS_HTML5TREE_BUILDER_NOBR:
            case NS_HTML5TREE_BUILDER_P:
            case NS_HTML5TREE_BUILDER_PRE_OR_LISTING:
            case NS_HTML5TREE_BUILDER_TABLE: {

              while (stack[currentPtr]->ns != kNameSpaceID_XHTML) {
                pop();
              }
              foreignFlag = NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN;
              goto starttagloop;
            }
            default: {
              if (kNameSpaceID_SVG == currNs) {
                attributes->adjustForSvg();
                if (selfClosing) {
                  appendVoidElementToCurrentMayFosterCamelCase(currNs, elementName, attributes);
                  selfClosing = PR_FALSE;
                } else {
                  appendToCurrentNodeAndPushElementMayFosterCamelCase(currNs, elementName, attributes);
                }
                goto starttagloop_end;
              } else {
                attributes->adjustForMath();
                if (selfClosing) {
                  appendVoidElementToCurrentMayFoster(currNs, elementName, attributes);
                  selfClosing = PR_FALSE;
                } else {
                  appendToCurrentNodeAndPushElementMayFosterNoScoping(currNs, elementName, attributes);
                }
                goto starttagloop_end;
              }
            }
          }
        }
      }
      default: {
        switch(mode) {
          case NS_HTML5TREE_BUILDER_IN_TABLE_BODY: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_TR: {
                clearStackBackTo(findLastInTableScopeOrRootTbodyTheadTfoot());
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                mode = NS_HTML5TREE_BUILDER_IN_ROW;
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_TD_OR_TH: {

                clearStackBackTo(findLastInTableScopeOrRootTbodyTheadTfoot());
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, nsHtml5ElementName::TR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                mode = NS_HTML5TREE_BUILDER_IN_ROW;
                continue;
              }
              case NS_HTML5TREE_BUILDER_CAPTION:
              case NS_HTML5TREE_BUILDER_COL:
              case NS_HTML5TREE_BUILDER_COLGROUP:
              case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
                eltPos = findLastInTableScopeOrRootTbodyTheadTfoot();
                if (!eltPos) {

                  goto starttagloop_end;
                } else {
                  clearStackBackTo(eltPos);
                  pop();
                  mode = NS_HTML5TREE_BUILDER_IN_TABLE;
                  continue;
                }
              }
              default:
                ; // fall through
            }
          }
          case NS_HTML5TREE_BUILDER_IN_ROW: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_TD_OR_TH: {
                clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TR));
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                mode = NS_HTML5TREE_BUILDER_IN_CELL;
                insertMarker();
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_CAPTION:
              case NS_HTML5TREE_BUILDER_COL:
              case NS_HTML5TREE_BUILDER_COLGROUP:
              case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
              case NS_HTML5TREE_BUILDER_TR: {
                eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
                if (!eltPos) {


                  goto starttagloop_end;
                }
                clearStackBackTo(eltPos);
                pop();
                mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
                continue;
              }
              default:
                ; // fall through
            }
          }
          case NS_HTML5TREE_BUILDER_IN_TABLE: {
            for (; ; ) {
              switch(group) {
                case NS_HTML5TREE_BUILDER_CAPTION: {
                  clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
                  insertMarker();
                  appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                  mode = NS_HTML5TREE_BUILDER_IN_CAPTION;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_COLGROUP: {
                  clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
                  appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                  mode = NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_COL: {
                  clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
                  appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, nsHtml5ElementName::COLGROUP, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                  mode = NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP;
                  goto starttagloop;
                }
                case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
                  clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
                  appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                  mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_TR:
                case NS_HTML5TREE_BUILDER_TD_OR_TH: {
                  clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
                  appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, nsHtml5ElementName::TBODY, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                  mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
                  goto starttagloop;
                }
                case NS_HTML5TREE_BUILDER_TABLE: {

                  eltPos = findLastInTableScope(name);
                  if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

                    goto starttagloop_end;
                  }
                  generateImpliedEndTags();
                  if (!isCurrent(nsHtml5Atoms::table)) {

                  }
                  while (currentPtr >= eltPos) {
                    pop();
                  }
                  resetTheInsertionMode();
                  goto starttagloop;
                }
                case NS_HTML5TREE_BUILDER_SCRIPT:
                case NS_HTML5TREE_BUILDER_STYLE: {
                  if (isTainted()) {
                    goto intableloop_end;
                  }
                  appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                  originalMode = mode;
                  mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                  tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_INPUT: {
                  if (isTainted() || !nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::hidden, attributes->getValue(nsHtml5AttributeName::TYPE))) {
                    goto intableloop_end;
                  }
                  appendVoidElementToCurrent(kNameSpaceID_XHTML, name, attributes, formPointer);
                  selfClosing = PR_FALSE;
                  goto starttagloop_end;
                }
                default: {

                  goto intableloop_end;
                }
              }
            }
            intableloop_end: ;
          }
          case NS_HTML5TREE_BUILDER_IN_CAPTION: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_CAPTION:
              case NS_HTML5TREE_BUILDER_COL:
              case NS_HTML5TREE_BUILDER_COLGROUP:
              case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
              case NS_HTML5TREE_BUILDER_TR:
              case NS_HTML5TREE_BUILDER_TD_OR_TH: {

                eltPos = findLastInTableScope(nsHtml5Atoms::caption);
                if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                  goto starttagloop_end;
                }
                generateImpliedEndTags();
                if (currentPtr != eltPos) {

                }
                while (currentPtr >= eltPos) {
                  pop();
                }
                clearTheListOfActiveFormattingElementsUpToTheLastMarker();
                mode = NS_HTML5TREE_BUILDER_IN_TABLE;
                continue;
              }
              default:
                ; // fall through
            }
          }
          case NS_HTML5TREE_BUILDER_IN_CELL: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_CAPTION:
              case NS_HTML5TREE_BUILDER_COL:
              case NS_HTML5TREE_BUILDER_COLGROUP:
              case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
              case NS_HTML5TREE_BUILDER_TR:
              case NS_HTML5TREE_BUILDER_TD_OR_TH: {
                eltPos = findLastInTableScopeTdTh();
                if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

                  goto starttagloop_end;
                } else {
                  closeTheCell(eltPos);
                  continue;
                }
              }
              default:
                ; // fall through
            }
          }
          case NS_HTML5TREE_BUILDER_IN_BODY: {
            for (; ; ) {
              switch(group) {
                case NS_HTML5TREE_BUILDER_HTML: {

                  addAttributesToElement(stack[0]->node, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_BASE:
                case NS_HTML5TREE_BUILDER_LINK:
                case NS_HTML5TREE_BUILDER_META:
                case NS_HTML5TREE_BUILDER_STYLE:
                case NS_HTML5TREE_BUILDER_SCRIPT:
                case NS_HTML5TREE_BUILDER_TITLE:
                case NS_HTML5TREE_BUILDER_COMMAND_OR_EVENT_SOURCE: {
                  goto inbodyloop_end;
                }
                case NS_HTML5TREE_BUILDER_BODY: {

                  addAttributesToBody(attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_P:
                case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
                case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
                case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
                case NS_HTML5TREE_BUILDER_ADDRESS_OR_DIR_OR_ARTICLE_OR_ASIDE_OR_DATAGRID_OR_DETAILS_OR_DIALOG_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_NAV_OR_SECTION: {
                  implicitlyCloseP();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_FIELDSET: {
                  implicitlyCloseP();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes, formPointer);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_PRE_OR_LISTING: {
                  implicitlyCloseP();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  needToDropLF = PR_TRUE;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_FORM: {
                  if (!!formPointer) {

                    goto starttagloop_end;
                  } else {
                    implicitlyCloseP();
                    appendToCurrentNodeAndPushFormElementMayFoster(attributes);
                    goto starttagloop_end;
                  }
                }
                case NS_HTML5TREE_BUILDER_LI:
                case NS_HTML5TREE_BUILDER_DD_OR_DT: {
                  eltPos = currentPtr;
                  for (; ; ) {
                    nsHtml5StackNode* node = stack[eltPos];
                    if (node->group == group) {
                      generateImpliedEndTagsExceptFor(node->name);
                      if (eltPos != currentPtr) {

                      }
                      while (currentPtr >= eltPos) {
                        pop();
                      }
                      break;
                    }
                    if (node->scoping || (node->special && node->name != nsHtml5Atoms::p && node->name != nsHtml5Atoms::address && node->name != nsHtml5Atoms::div)) {
                      break;
                    }
                    eltPos--;
                  }
                  implicitlyCloseP();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_PLAINTEXT: {
                  implicitlyCloseP();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_PLAINTEXT, elementName);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_A: {
                  PRInt32 activeAPos = findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsHtml5Atoms::a);
                  if (activeAPos != -1) {

                    nsHtml5StackNode* activeA = listOfActiveFormattingElements[activeAPos];
                    adoptionAgencyEndTag(nsHtml5Atoms::a);
                    removeFromStack(activeA);
                    activeAPos = findInListOfActiveFormattingElements(activeA);
                    if (activeAPos != -1) {
                      removeFromListOfActiveFormattingElements(activeAPos);
                    }
                  }
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushFormattingElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_B_OR_BIG_OR_EM_OR_FONT_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U: {
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushFormattingElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_NOBR: {
                  reconstructTheActiveFormattingElements();
                  if (NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK != findLastInScope(nsHtml5Atoms::nobr)) {

                    adoptionAgencyEndTag(nsHtml5Atoms::nobr);
                  }
                  appendToCurrentNodeAndPushFormattingElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_BUTTON: {
                  eltPos = findLastInScope(name);
                  if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

                    generateImpliedEndTags();
                    if (!isCurrent(nsHtml5Atoms::button)) {

                    }
                    while (currentPtr >= eltPos) {
                      pop();
                    }
                    clearTheListOfActiveFormattingElementsUpToTheLastMarker();
                    goto starttagloop;
                  } else {
                    reconstructTheActiveFormattingElements();
                    appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes, formPointer);
                    insertMarker();
                    goto starttagloop_end;
                  }
                }
                case NS_HTML5TREE_BUILDER_OBJECT: {
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes, formPointer);
                  insertMarker();
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET: {
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  insertMarker();
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_XMP: {
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  originalMode = mode;
                  mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                  tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_TABLE: {
                  implicitlyCloseP();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  mode = NS_HTML5TREE_BUILDER_IN_TABLE;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_BR:
                case NS_HTML5TREE_BUILDER_EMBED_OR_IMG:
                case NS_HTML5TREE_BUILDER_AREA_OR_BASEFONT_OR_BGSOUND_OR_SPACER_OR_WBR: {
                  reconstructTheActiveFormattingElements();
                }
                case NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE: {
                  appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  selfClosing = PR_FALSE;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_HR: {
                  implicitlyCloseP();
                  appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  selfClosing = PR_FALSE;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_IMAGE: {

                  elementName = nsHtml5ElementName::IMG;
                  goto starttagloop;
                }
                case NS_HTML5TREE_BUILDER_INPUT: {
                  reconstructTheActiveFormattingElements();
                  appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, name, attributes, formPointer);
                  selfClosing = PR_FALSE;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_ISINDEX: {

                  if (!!formPointer) {
                    goto starttagloop_end;
                  }
                  implicitlyCloseP();
                  nsHtml5HtmlAttributes* formAttrs = tokenizer->emptyAttributes();
                  PRInt32 actionIndex = attributes->getIndex(nsHtml5AttributeName::ACTION);
                  if (actionIndex > -1) {
                    formAttrs->addAttribute(nsHtml5AttributeName::ACTION, attributes->getValue(actionIndex));
                  }
                  appendToCurrentNodeAndPushFormElementMayFoster(formAttrs);
                  appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, nsHtml5ElementName::HR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, nsHtml5ElementName::P, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, nsHtml5ElementName::LABEL, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                  PRInt32 promptIndex = attributes->getIndex(nsHtml5AttributeName::PROMPT);
                  if (promptIndex > -1) {
                    jArray<PRUnichar,PRInt32> prompt = nsHtml5Portability::newCharArrayFromString(attributes->getValue(promptIndex));
                    appendCharacters(stack[currentPtr]->node, prompt, 0, prompt.length);
                    prompt.release();
                  } else {
                    appendCharacters(stack[currentPtr]->node, nsHtml5TreeBuilder::ISINDEX_PROMPT, 0, nsHtml5TreeBuilder::ISINDEX_PROMPT.length);
                  }
                  nsHtml5HtmlAttributes* inputAttributes = tokenizer->emptyAttributes();
                  inputAttributes->addAttribute(nsHtml5AttributeName::NAME, nsHtml5StringLiterals::isindex);
                  for (PRInt32 i = 0; i < attributes->getLength(); i++) {
                    nsHtml5AttributeName* attributeQName = attributes->getAttributeName(i);
                    if (!(nsHtml5AttributeName::NAME == attributeQName || nsHtml5AttributeName::ACTION == attributeQName || nsHtml5AttributeName::PROMPT == attributeQName)) {
                      inputAttributes->addAttribute(attributeQName, attributes->getValue(i));
                    }
                  }
                  appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, nsHtml5Atoms::input, inputAttributes, formPointer);
                  pop();
                  pop();
                  appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, nsHtml5ElementName::HR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                  pop();
                  selfClosing = PR_FALSE;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_TEXTAREA: {
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes, formPointer);
                  tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_RCDATA, elementName);
                  originalMode = mode;
                  mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                  needToDropLF = PR_TRUE;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_NOSCRIPT: {
                  if (!scriptingEnabled) {
                    reconstructTheActiveFormattingElements();
                    appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                    goto starttagloop_end;
                  } else {
                  }
                }
                case NS_HTML5TREE_BUILDER_NOFRAMES:
                case NS_HTML5TREE_BUILDER_IFRAME:
                case NS_HTML5TREE_BUILDER_NOEMBED: {
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  originalMode = mode;
                  mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                  tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_SELECT: {
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes, formPointer);
                  switch(mode) {
                    case NS_HTML5TREE_BUILDER_IN_TABLE:
                    case NS_HTML5TREE_BUILDER_IN_CAPTION:
                    case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP:
                    case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
                    case NS_HTML5TREE_BUILDER_IN_ROW:
                    case NS_HTML5TREE_BUILDER_IN_CELL: {
                      mode = NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE;
                      break;
                    }
                    default: {
                      mode = NS_HTML5TREE_BUILDER_IN_SELECT;
                      break;
                    }
                  }
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_OPTGROUP:
                case NS_HTML5TREE_BUILDER_OPTION: {
                  if (findLastInScope(nsHtml5Atoms::option) != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                    for (; ; ) {
                      if (isCurrent(nsHtml5Atoms::option)) {
                        pop();
                        goto optionendtagloop_end;
                      }
                      eltPos = currentPtr;
                      for (; ; ) {
                        nsHtml5StackNode* node = stack[eltPos];
                        if (node->name == nsHtml5Atoms::option) {
                          generateImpliedEndTags();
                          if (!isCurrent(nsHtml5Atoms::option)) {

                          }
                          while (currentPtr >= eltPos) {
                            pop();
                          }
                          goto optionendtagloop_end;
                        }
                        eltPos--;
                      }
                    }
                    optionendtagloop_end: ;
                  }
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_RT_OR_RP: {
                  eltPos = findLastInScope(nsHtml5Atoms::ruby);
                  if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                    generateImpliedEndTags();
                  }
                  if (eltPos != currentPtr) {

                    while (currentPtr > eltPos) {
                      pop();
                    }
                  }
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_MATH: {
                  reconstructTheActiveFormattingElements();
                  attributes->adjustForMath();
                  if (selfClosing) {
                    appendVoidElementToCurrentMayFoster(kNameSpaceID_MathML, elementName, attributes);
                    selfClosing = PR_FALSE;
                  } else {
                    appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_MathML, elementName, attributes);
                    foreignFlag = NS_HTML5TREE_BUILDER_IN_FOREIGN;
                  }
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_SVG: {
                  reconstructTheActiveFormattingElements();
                  attributes->adjustForSvg();
                  if (selfClosing) {
                    appendVoidElementToCurrentMayFosterCamelCase(kNameSpaceID_SVG, elementName, attributes);
                    selfClosing = PR_FALSE;
                  } else {
                    appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_SVG, elementName, attributes);
                    foreignFlag = NS_HTML5TREE_BUILDER_IN_FOREIGN;
                  }
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_CAPTION:
                case NS_HTML5TREE_BUILDER_COL:
                case NS_HTML5TREE_BUILDER_COLGROUP:
                case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
                case NS_HTML5TREE_BUILDER_TR:
                case NS_HTML5TREE_BUILDER_TD_OR_TH:
                case NS_HTML5TREE_BUILDER_FRAME:
                case NS_HTML5TREE_BUILDER_FRAMESET:
                case NS_HTML5TREE_BUILDER_HEAD: {

                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_OUTPUT_OR_LABEL: {
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes, formPointer);
                  goto starttagloop_end;
                }
                default: {
                  reconstructTheActiveFormattingElements();
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  goto starttagloop_end;
                }
              }
            }
            inbodyloop_end: ;
          }
          case NS_HTML5TREE_BUILDER_IN_HEAD: {
            for (; ; ) {
              switch(group) {
                case NS_HTML5TREE_BUILDER_HTML: {

                  addAttributesToElement(stack[0]->node, attributes);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_BASE:
                case NS_HTML5TREE_BUILDER_COMMAND_OR_EVENT_SOURCE: {
                  appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  selfClosing = PR_FALSE;
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_META:
                case NS_HTML5TREE_BUILDER_LINK: {
                  goto inheadloop_end;
                }
                case NS_HTML5TREE_BUILDER_TITLE: {
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  originalMode = mode;
                  mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                  tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_RCDATA, elementName);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_NOSCRIPT: {
                  if (scriptingEnabled) {
                    appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                    originalMode = mode;
                    mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                    tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                  } else {
                    appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                    mode = NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT;
                  }
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_SCRIPT:
                case NS_HTML5TREE_BUILDER_STYLE:
                case NS_HTML5TREE_BUILDER_NOFRAMES: {
                  appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                  originalMode = mode;
                  mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                  tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                  goto starttagloop_end;
                }
                case NS_HTML5TREE_BUILDER_HEAD: {

                  goto starttagloop_end;
                }
                default: {
                  pop();
                  mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
                  goto starttagloop;
                }
              }
            }
            inheadloop_end: ;
          }
          case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {

                addAttributesToElement(stack[0]->node, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_LINK: {
                appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                selfClosing = PR_FALSE;
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_META: {
                checkMetaCharset(attributes);
                appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                selfClosing = PR_FALSE;
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_STYLE:
              case NS_HTML5TREE_BUILDER_NOFRAMES: {
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                originalMode = mode;
                mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_HEAD: {

                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_NOSCRIPT: {

                goto starttagloop_end;
              }
              default: {

                pop();
                mode = NS_HTML5TREE_BUILDER_IN_HEAD;
                continue;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {

                addAttributesToElement(stack[0]->node, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_COL: {
                appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                selfClosing = PR_FALSE;
                goto starttagloop_end;
              }
              default: {
                if (!currentPtr) {


                  goto starttagloop_end;
                }
                pop();
                mode = NS_HTML5TREE_BUILDER_IN_TABLE;
                continue;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_CAPTION:
              case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
              case NS_HTML5TREE_BUILDER_TR:
              case NS_HTML5TREE_BUILDER_TD_OR_TH:
              case NS_HTML5TREE_BUILDER_TABLE: {

                endSelect();
                continue;
              }
              default:
                ; // fall through
            }
          }
          case NS_HTML5TREE_BUILDER_IN_SELECT: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {

                addAttributesToElement(stack[0]->node, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_OPTION: {
                if (isCurrent(nsHtml5Atoms::option)) {
                  pop();
                }
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_OPTGROUP: {
                if (isCurrent(nsHtml5Atoms::option)) {
                  pop();
                }
                if (isCurrent(nsHtml5Atoms::optgroup)) {
                  pop();
                }
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_SELECT: {

                eltPos = findLastInTableScope(name);
                if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {


                  goto starttagloop_end;
                } else {
                  while (currentPtr >= eltPos) {
                    pop();
                  }
                  resetTheInsertionMode();
                  goto starttagloop_end;
                }
              }
              case NS_HTML5TREE_BUILDER_INPUT:
              case NS_HTML5TREE_BUILDER_TEXTAREA: {

                endSelect();
                continue;
              }
              default: {

                goto starttagloop_end;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_AFTER_BODY: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {

                addAttributesToElement(stack[0]->node, attributes);
                goto starttagloop_end;
              }
              default: {

                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                continue;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_FRAMESET: {
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_FRAME: {
                appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                selfClosing = PR_FALSE;
                goto starttagloop_end;
              }
              default:
                ; // fall through
            }
          }
          case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {

                addAttributesToElement(stack[0]->node, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_NOFRAMES: {
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                originalMode = mode;
                mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                goto starttagloop_end;
              }
              default: {

                goto starttagloop_end;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_INITIAL: {
            documentModeInternal(QUIRKS_MODE, nsnull, nsnull, PR_FALSE);
            mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
            continue;
          }
          case NS_HTML5TREE_BUILDER_BEFORE_HTML: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {
                if (attributes == nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
                  appendHtmlElementToDocumentAndPush();
                } else {
                  appendHtmlElementToDocumentAndPush(attributes);
                }
                mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
                goto starttagloop_end;
              }
              default: {
                appendHtmlElementToDocumentAndPush();
                mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
                continue;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {

                addAttributesToElement(stack[0]->node, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_HEAD: {
                appendToCurrentNodeAndPushHeadElement(attributes);
                mode = NS_HTML5TREE_BUILDER_IN_HEAD;
                goto starttagloop_end;
              }
              default: {
                appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                mode = NS_HTML5TREE_BUILDER_IN_HEAD;
                continue;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_AFTER_HEAD: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {

                addAttributesToElement(stack[0]->node, attributes);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_BODY: {
                if (!attributes->getLength()) {
                  appendToCurrentNodeAndPushBodyElement();
                } else {
                  appendToCurrentNodeAndPushBodyElement(attributes);
                }
                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_FRAMESET: {
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_BASE: {

                pushHeadPointerOntoStack();
                appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                selfClosing = PR_FALSE;
                pop();
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_LINK: {

                pushHeadPointerOntoStack();
                appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                selfClosing = PR_FALSE;
                pop();
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_META: {

                checkMetaCharset(attributes);
                pushHeadPointerOntoStack();
                appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                selfClosing = PR_FALSE;
                pop();
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_SCRIPT: {

                pushHeadPointerOntoStack();
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                originalMode = mode;
                mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_STYLE:
              case NS_HTML5TREE_BUILDER_NOFRAMES: {

                pushHeadPointerOntoStack();
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                originalMode = mode;
                mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_TITLE: {

                pushHeadPointerOntoStack();
                appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, elementName, attributes);
                originalMode = mode;
                mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_RCDATA, elementName);
                goto starttagloop_end;
              }
              case NS_HTML5TREE_BUILDER_HEAD: {

                goto starttagloop_end;
              }
              default: {
                appendToCurrentNodeAndPushBodyElement();
                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                continue;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_HTML: {

                addAttributesToElement(stack[0]->node, attributes);
                goto starttagloop_end;
              }
              default: {


                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                continue;
              }
            }
          }
          case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
            switch(group) {
              case NS_HTML5TREE_BUILDER_NOFRAMES: {
                appendToCurrentNodeAndPushElementMayFoster(kNameSpaceID_XHTML, elementName, attributes);
                originalMode = mode;
                mode = NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA;
                tokenizer->setContentModelFlag(NS_HTML5TOKENIZER_CDATA, elementName);
                goto starttagloop_end;
              }
              default: {

                goto starttagloop_end;
              }
            }
          }
        }
      }
    }
  }
  starttagloop_end: ;
  if (needsPostProcessing && foreignFlag == NS_HTML5TREE_BUILDER_IN_FOREIGN && !hasForeignInScope()) {
    foreignFlag = NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN;
  }
  if (selfClosing) {

  }
}

nsString* 
nsHtml5TreeBuilder::extractCharsetFromContent(nsString* attributeValue)
{
  PRInt32 charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
  PRInt32 start = -1;
  PRInt32 end = -1;
  jArray<PRUnichar,PRInt32> buffer = nsHtml5Portability::newCharArrayFromString(attributeValue);
  for (PRInt32 i = 0; i < buffer.length; i++) {
    PRUnichar c = buffer[i];
    switch(charsetState) {
      case NS_HTML5TREE_BUILDER_CHARSET_INITIAL: {
        switch(c) {
          case 'c':
          case 'C': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_C;
            continue;
          }
          default: {
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_C: {
        switch(c) {
          case 'h':
          case 'H': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_H;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_H: {
        switch(c) {
          case 'a':
          case 'A': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_A;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_A: {
        switch(c) {
          case 'r':
          case 'R': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_R;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_R: {
        switch(c) {
          case 's':
          case 'S': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_S;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_S: {
        switch(c) {
          case 'e':
          case 'E': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_E;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_E: {
        switch(c) {
          case 't':
          case 'T': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_T;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_T: {
        switch(c) {
          case '\t':
          case '\n':
          case '\f':
          case '\r':
          case ' ': {
            continue;
          }
          case '=': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_EQUALS;
            continue;
          }
          default: {
            return nsnull;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_EQUALS: {
        switch(c) {
          case '\t':
          case '\n':
          case '\f':
          case '\r':
          case ' ': {
            continue;
          }
          case '\'': {
            start = i + 1;
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_SINGLE_QUOTED;
            continue;
          }
          case '\"': {
            start = i + 1;
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_DOUBLE_QUOTED;
            continue;
          }
          default: {
            start = i;
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_UNQUOTED;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_SINGLE_QUOTED: {
        switch(c) {
          case '\'': {
            end = i;
            goto charsetloop_end;
          }
          default: {
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_DOUBLE_QUOTED: {
        switch(c) {
          case '\"': {
            end = i;
            goto charsetloop_end;
          }
          default: {
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_UNQUOTED: {
        switch(c) {
          case '\t':
          case '\n':
          case '\f':
          case '\r':
          case ' ':
          case ';': {
            end = i;
            goto charsetloop_end;
          }
          default: {
            continue;
          }
        }
      }
    }
  }
  charsetloop_end: ;
  nsString* rv = nsnull;
  if (start != -1) {
    if (end == -1) {
      end = buffer.length;
    }
    rv = nsHtml5Portability::newStringFromBuffer(buffer, start, end - start);
  }
  buffer.release();
  return rv;
}

void 
nsHtml5TreeBuilder::checkMetaCharset(nsHtml5HtmlAttributes* attributes)
{
  nsString* content = attributes->getValue(nsHtml5AttributeName::CONTENT);
  nsString* internalCharsetLegacy = nsnull;
  if (!!content) {
    internalCharsetLegacy = nsHtml5TreeBuilder::extractCharsetFromContent(content);
  }
  if (!internalCharsetLegacy) {
    nsString* internalCharsetHtml5 = attributes->getValue(nsHtml5AttributeName::CHARSET);
    if (!!internalCharsetHtml5) {
      tokenizer->internalEncodingDeclaration(internalCharsetHtml5);
      requestSuspension();
    }
  } else {
    tokenizer->internalEncodingDeclaration(internalCharsetLegacy);
    nsHtml5Portability::releaseString(internalCharsetLegacy);
    requestSuspension();
  }
}

void 
nsHtml5TreeBuilder::endTag(nsHtml5ElementName* elementName)
{
  needToDropLF = PR_FALSE;
  PRInt32 eltPos;
  for (; ; ) {
    PRInt32 group = elementName->group;
    nsIAtom* name = elementName->name;
    switch(mode) {
      case NS_HTML5TREE_BUILDER_IN_ROW: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TR: {
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {


              goto endtagloop_end;
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_TABLE: {
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {


              goto endtagloop_end;
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
            if (findLastInTableScope(name) == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

              goto endtagloop_end;
            }
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {


              goto endtagloop_end;
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {

            goto endtagloop_end;
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
            eltPos = findLastOrRoot(name);
            if (!eltPos) {

              goto endtagloop_end;
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_TABLE: {
            eltPos = findLastInTableScopeOrRootTbodyTheadTfoot();
            if (!eltPos) {


              goto endtagloop_end;
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_TD_OR_TH:
          case NS_HTML5TREE_BUILDER_TR: {

            goto endtagloop_end;
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TABLE: {
            eltPos = findLast(nsHtml5Atoms::table);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {


              goto endtagloop_end;
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            resetTheInsertionMode();
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TD_OR_TH:
          case NS_HTML5TREE_BUILDER_TR: {

            goto endtagloop_end;
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_CAPTION: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_CAPTION: {
            eltPos = findLastInTableScope(nsHtml5Atoms::caption);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              goto endtagloop_end;
            }
            generateImpliedEndTags();
            if (currentPtr != eltPos) {

            }
            while (currentPtr >= eltPos) {
              pop();
            }
            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_TABLE: {

            eltPos = findLastInTableScope(nsHtml5Atoms::caption);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              goto endtagloop_end;
            }
            generateImpliedEndTags();
            if (currentPtr != eltPos) {

            }
            while (currentPtr >= eltPos) {
              pop();
            }
            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TD_OR_TH:
          case NS_HTML5TREE_BUILDER_TR: {

            goto endtagloop_end;
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_CELL: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            eltPos = findLastInTableScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

              goto endtagloop_end;
            }
            generateImpliedEndTags();
            if (!isCurrent(name)) {

            }
            while (currentPtr >= eltPos) {
              pop();
            }
            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            mode = NS_HTML5TREE_BUILDER_IN_ROW;
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_TABLE:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR: {
            if (findLastInTableScope(name) == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

              goto endtagloop_end;
            }
            closeTheCell(findLastInTableScopeTdTh());
            continue;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML: {

            goto endtagloop_end;
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_BODY: {
            if (!isSecondOnStackBody()) {


              goto endtagloop_end;
            }


            mode = NS_HTML5TREE_BUILDER_AFTER_BODY;
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_HTML: {
            if (!isSecondOnStackBody()) {


              goto endtagloop_end;
            }

            mode = NS_HTML5TREE_BUILDER_AFTER_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
          case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
          case NS_HTML5TREE_BUILDER_PRE_OR_LISTING:
          case NS_HTML5TREE_BUILDER_FIELDSET:
          case NS_HTML5TREE_BUILDER_ADDRESS_OR_DIR_OR_ARTICLE_OR_ASIDE_OR_DATAGRID_OR_DETAILS_OR_DIALOG_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_NAV_OR_SECTION: {
            eltPos = findLastInScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

            } else {
              generateImpliedEndTags();
              if (!isCurrent(name)) {

              }
              while (currentPtr >= eltPos) {
                pop();
              }
            }
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_FORM: {
            formPointer = nsnull;
            eltPos = findLastInScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

            } else {
              generateImpliedEndTags();
              if (!isCurrent(name)) {

              }
              while (currentPtr >= eltPos) {
                pop();
              }
            }
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_P: {
            eltPos = findLastInScope(nsHtml5Atoms::p);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

              if (foreignFlag == NS_HTML5TREE_BUILDER_IN_FOREIGN) {

                while (stack[currentPtr]->ns != kNameSpaceID_XHTML) {
                  pop();
                }
                foreignFlag = NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN;
              }
              appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              goto endtagloop_end;
            }
            generateImpliedEndTagsExceptFor(nsHtml5Atoms::p);

            if (eltPos != currentPtr) {

            }
            while (currentPtr >= eltPos) {
              pop();
            }
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_DD_OR_DT:
          case NS_HTML5TREE_BUILDER_LI: {
            eltPos = findLastInScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

            } else {
              generateImpliedEndTagsExceptFor(name);
              if (eltPos != currentPtr) {

              }
              while (currentPtr >= eltPos) {
                pop();
              }
            }
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6: {
            eltPos = findLastInScopeHn();
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

            } else {
              generateImpliedEndTags();
              if (!isCurrent(name)) {

              }
              while (currentPtr >= eltPos) {
                pop();
              }
            }
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_A:
          case NS_HTML5TREE_BUILDER_B_OR_BIG_OR_EM_OR_FONT_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
          case NS_HTML5TREE_BUILDER_NOBR: {
            adoptionAgencyEndTag(name);
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_BUTTON:
          case NS_HTML5TREE_BUILDER_OBJECT:
          case NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET: {
            eltPos = findLastInScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

            } else {
              generateImpliedEndTags();
              if (!isCurrent(name)) {

              }
              while (currentPtr >= eltPos) {
                pop();
              }
              clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            }
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_BR: {

            if (foreignFlag == NS_HTML5TREE_BUILDER_IN_FOREIGN) {

              while (stack[currentPtr]->ns != kNameSpaceID_XHTML) {
                pop();
              }
              foreignFlag = NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN;
            }
            reconstructTheActiveFormattingElements();
            appendVoidElementToCurrentMayFoster(kNameSpaceID_XHTML, elementName, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_AREA_OR_BASEFONT_OR_BGSOUND_OR_SPACER_OR_WBR:
          case NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE:
          case NS_HTML5TREE_BUILDER_EMBED_OR_IMG:
          case NS_HTML5TREE_BUILDER_IMAGE:
          case NS_HTML5TREE_BUILDER_INPUT:
          case NS_HTML5TREE_BUILDER_HR:
          case NS_HTML5TREE_BUILDER_ISINDEX:
          case NS_HTML5TREE_BUILDER_IFRAME:
          case NS_HTML5TREE_BUILDER_NOEMBED:
          case NS_HTML5TREE_BUILDER_NOFRAMES:
          case NS_HTML5TREE_BUILDER_SELECT:
          case NS_HTML5TREE_BUILDER_TABLE:
          case NS_HTML5TREE_BUILDER_TEXTAREA: {

            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_NOSCRIPT: {
            if (scriptingEnabled) {

              goto endtagloop_end;
            } else {
            }
          }
          default: {
            if (isCurrent(name)) {
              pop();
              goto endtagloop_end;
            }
            eltPos = currentPtr;
            for (; ; ) {
              nsHtml5StackNode* node = stack[eltPos];
              if (node->name == name) {
                generateImpliedEndTags();
                if (!isCurrent(name)) {

                }
                while (currentPtr >= eltPos) {
                  pop();
                }
                goto endtagloop_end;
              }
              if (node->scoping || node->special) {

                goto endtagloop_end;
              }
              eltPos--;
            }
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_COLGROUP: {
            if (!currentPtr) {


              goto endtagloop_end;
            }
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_COL: {

            goto endtagloop_end;
          }
          default: {
            if (!currentPtr) {


              goto endtagloop_end;
            }
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_TABLE:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR:
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {

            if (findLastInTableScope(name) != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              endSelect();
              continue;
            } else {
              goto endtagloop_end;
            }
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_SELECT: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_OPTION: {
            if (isCurrent(nsHtml5Atoms::option)) {
              pop();
              goto endtagloop_end;
            } else {

              goto endtagloop_end;
            }
          }
          case NS_HTML5TREE_BUILDER_OPTGROUP: {
            if (isCurrent(nsHtml5Atoms::option) && nsHtml5Atoms::optgroup == stack[currentPtr - 1]->name) {
              pop();
            }
            if (isCurrent(nsHtml5Atoms::optgroup)) {
              pop();
            } else {

            }
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_SELECT: {
            endSelect();
            goto endtagloop_end;
          }
          default: {

            goto endtagloop_end;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            if (fragment) {

              goto endtagloop_end;
            } else {
              mode = NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY;
              goto endtagloop_end;
            }
          }
          default: {

            mode = NS_HTML5TREE_BUILDER_IN_BODY;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_FRAMESET: {
            if (!currentPtr) {


              goto endtagloop_end;
            }
            pop();
            if ((!fragment) && !isCurrent(nsHtml5Atoms::frameset)) {
              mode = NS_HTML5TREE_BUILDER_AFTER_FRAMESET;
            }
            goto endtagloop_end;
          }
          default: {

            goto endtagloop_end;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            mode = NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET;
            goto endtagloop_end;
          }
          default: {

            goto endtagloop_end;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_INITIAL: {
        documentModeInternal(QUIRKS_MODE, nsnull, nsnull, PR_FALSE);
        mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
        continue;
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HTML: {
        appendHtmlElementToDocumentAndPush();
        mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HEAD:
          case NS_HTML5TREE_BUILDER_BR: {
            appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            continue;
          }
          default: {

            goto endtagloop_end;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HEAD: {
            pop();
            mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_BR: {
            pop();
            mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
            continue;
          }
          default: {

            goto endtagloop_end;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_NOSCRIPT: {
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            goto endtagloop_end;
          }
          case NS_HTML5TREE_BUILDER_BR: {

            pop();
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            continue;
          }
          default: {

            goto endtagloop_end;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_HEAD: {

        goto endtagloop_end;
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY: {

        mode = NS_HTML5TREE_BUILDER_IN_BODY;
        continue;
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {

        mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_CDATA_RCDATA: {
        if (originalMode == NS_HTML5TREE_BUILDER_AFTER_HEAD) {
          pop();
        }
        pop();
        mode = originalMode;
        goto endtagloop_end;
      }
    }
  }
  endtagloop_end: ;
  if (foreignFlag == NS_HTML5TREE_BUILDER_IN_FOREIGN && !hasForeignInScope()) {
    foreignFlag = NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN;
  }
}

void 
nsHtml5TreeBuilder::endSelect()
{
  PRInt32 eltPos = findLastInTableScope(nsHtml5Atoms::select);
  if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {


    return;
  }
  while (currentPtr >= eltPos) {
    pop();
  }
  resetTheInsertionMode();
}

PRInt32 
nsHtml5TreeBuilder::findLastInTableScopeOrRootTbodyTheadTfoot()
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->group == NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT) {
      return i;
    }
  }
  return 0;
}

PRInt32 
nsHtml5TreeBuilder::findLast(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRInt32 
nsHtml5TreeBuilder::findLastInTableScope(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    } else if (stack[i]->name == nsHtml5Atoms::table) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRInt32 
nsHtml5TreeBuilder::findLastInScope(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    } else if (stack[i]->scoping) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRInt32 
nsHtml5TreeBuilder::findLastInScopeHn()
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->group == NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6) {
      return i;
    } else if (stack[i]->scoping) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRBool 
nsHtml5TreeBuilder::hasForeignInScope()
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->ns != kNameSpaceID_XHTML) {
      return PR_TRUE;
    } else if (stack[i]->scoping) {
      return PR_FALSE;
    }
  }
  return PR_FALSE;
}

void 
nsHtml5TreeBuilder::generateImpliedEndTagsExceptFor(nsIAtom* name)
{
  for (; ; ) {
    nsHtml5StackNode* node = stack[currentPtr];
    switch(node->group) {
      case NS_HTML5TREE_BUILDER_P:
      case NS_HTML5TREE_BUILDER_LI:
      case NS_HTML5TREE_BUILDER_DD_OR_DT:
      case NS_HTML5TREE_BUILDER_OPTION:
      case NS_HTML5TREE_BUILDER_OPTGROUP:
      case NS_HTML5TREE_BUILDER_RT_OR_RP: {
        if (node->name == name) {
          return;
        }
        pop();
        continue;
      }
      default: {
        return;
      }
    }
  }
}

void 
nsHtml5TreeBuilder::generateImpliedEndTags()
{
  for (; ; ) {
    switch(stack[currentPtr]->group) {
      case NS_HTML5TREE_BUILDER_P:
      case NS_HTML5TREE_BUILDER_LI:
      case NS_HTML5TREE_BUILDER_DD_OR_DT:
      case NS_HTML5TREE_BUILDER_OPTION:
      case NS_HTML5TREE_BUILDER_OPTGROUP:
      case NS_HTML5TREE_BUILDER_RT_OR_RP: {
        pop();
        continue;
      }
      default: {
        return;
      }
    }
  }
}

PRBool 
nsHtml5TreeBuilder::isSecondOnStackBody()
{
  return currentPtr >= 1 && stack[1]->group == NS_HTML5TREE_BUILDER_BODY;
}

void 
nsHtml5TreeBuilder::documentModeInternal(nsHtml5DocumentMode m, nsString* publicIdentifier, nsString* systemIdentifier, PRBool html4SpecificAdditionalErrorChecks)
{
  if (!!documentModeHandler) {
    documentModeHandler->documentMode(m);
  }
}

PRBool 
nsHtml5TreeBuilder::isAlmostStandards(nsString* publicIdentifier, nsString* systemIdentifier)
{
  if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::___w3c__dtd_xhtml_1_0_transitional__en, publicIdentifier)) {
    return PR_TRUE;
  }
  if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::___w3c__dtd_xhtml_1_0_frameset__en, publicIdentifier)) {
    return PR_TRUE;
  }
  if (!!systemIdentifier) {
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::___w3c__dtd_html_4_01_transitional__en, publicIdentifier)) {
      return PR_TRUE;
    }
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::___w3c__dtd_html_4_01_frameset__en, publicIdentifier)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool 
nsHtml5TreeBuilder::isQuirky(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, PRBool forceQuirks)
{
  if (forceQuirks) {
    return PR_TRUE;
  }
  if (!nsHtml5Portability::localEqualsHtmlIgnoreAsciiCase(name)) {
    return PR_TRUE;
  }
  if (!!publicIdentifier) {
    for (PRInt32 i = 0; i < nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS.length; i++) {
      if (nsHtml5Portability::lowerCaseLiteralIsPrefixOfIgnoreAsciiCaseString(nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS[i], publicIdentifier)) {
        return PR_TRUE;
      }
    }
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::___w3o__dtd_w3_html_strict_3_0__en__, publicIdentifier) || nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::__w3c_dtd_html_4_0_transitional_en, publicIdentifier) || nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::html, publicIdentifier)) {
      return PR_TRUE;
    }
  }
  if (!systemIdentifier) {
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::___w3c__dtd_html_4_01_transitional__en, publicIdentifier)) {
      return PR_TRUE;
    } else if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::___w3c__dtd_html_4_01_frameset__en, publicIdentifier)) {
      return PR_TRUE;
    }
  } else if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsHtml5StringLiterals::http___www_ibm_com_data_dtd_v11_ibmxhtml1_transitional_dtd, systemIdentifier)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

void 
nsHtml5TreeBuilder::closeTheCell(PRInt32 eltPos)
{
  generateImpliedEndTags();
  if (eltPos != currentPtr) {

  }
  while (currentPtr >= eltPos) {
    pop();
  }
  clearTheListOfActiveFormattingElementsUpToTheLastMarker();
  mode = NS_HTML5TREE_BUILDER_IN_ROW;
  return;
}

PRInt32 
nsHtml5TreeBuilder::findLastInTableScopeTdTh()
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    nsIAtom* name = stack[i]->name;
    if (nsHtml5Atoms::td == name || nsHtml5Atoms::th == name) {
      return i;
    } else if (name == nsHtml5Atoms::table) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

void 
nsHtml5TreeBuilder::clearStackBackTo(PRInt32 eltPos)
{
  if (eltPos != currentPtr) {
    while (currentPtr > eltPos) {
      pop();
    }
  }
}

void 
nsHtml5TreeBuilder::resetTheInsertionMode()
{
  foreignFlag = NS_HTML5TREE_BUILDER_NOT_IN_FOREIGN;
  nsHtml5StackNode* node;
  nsIAtom* name;
  for (PRInt32 i = currentPtr; i >= 0; i--) {
    node = stack[i];
    name = node->name;
    if (!i) {
      if (!(contextName == nsHtml5Atoms::td || contextName == nsHtml5Atoms::th)) {
        name = contextName;
      } else {
        mode = NS_HTML5TREE_BUILDER_IN_BODY;
        return;
      }
    }
    if (nsHtml5Atoms::select == name) {
      mode = NS_HTML5TREE_BUILDER_IN_SELECT;
      return;
    } else if (nsHtml5Atoms::td == name || nsHtml5Atoms::th == name) {
      mode = NS_HTML5TREE_BUILDER_IN_CELL;
      return;
    } else if (nsHtml5Atoms::tr == name) {
      mode = NS_HTML5TREE_BUILDER_IN_ROW;
      return;
    } else if (nsHtml5Atoms::tbody == name || nsHtml5Atoms::thead == name || nsHtml5Atoms::tfoot == name) {
      mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
      return;
    } else if (nsHtml5Atoms::caption == name) {
      mode = NS_HTML5TREE_BUILDER_IN_CAPTION;
      return;
    } else if (nsHtml5Atoms::colgroup == name) {
      mode = NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP;
      return;
    } else if (nsHtml5Atoms::table == name) {
      mode = NS_HTML5TREE_BUILDER_IN_TABLE;
      return;
    } else if (kNameSpaceID_XHTML != node->ns) {
      foreignFlag = NS_HTML5TREE_BUILDER_IN_FOREIGN;
      mode = NS_HTML5TREE_BUILDER_IN_BODY;
      return;
    } else if (nsHtml5Atoms::head == name) {
      mode = NS_HTML5TREE_BUILDER_IN_BODY;
      return;
    } else if (nsHtml5Atoms::body == name) {
      mode = NS_HTML5TREE_BUILDER_IN_BODY;
      return;
    } else if (nsHtml5Atoms::frameset == name) {
      mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
      return;
    } else if (nsHtml5Atoms::html == name) {
      if (!headPointer) {
        mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
      } else {
        mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
      }
      return;
    } else if (!i) {
      mode = NS_HTML5TREE_BUILDER_IN_BODY;
      return;
    }
  }
}

void 
nsHtml5TreeBuilder::implicitlyCloseP()
{
  PRInt32 eltPos = findLastInScope(nsHtml5Atoms::p);
  if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
    return;
  }
  generateImpliedEndTagsExceptFor(nsHtml5Atoms::p);
  if (eltPos != currentPtr) {

  }
  while (currentPtr >= eltPos) {
    pop();
  }
}

PRBool 
nsHtml5TreeBuilder::clearLastStackSlot()
{
  stack[currentPtr] = nsnull;
  return PR_TRUE;
}

PRBool 
nsHtml5TreeBuilder::clearLastListSlot()
{
  listOfActiveFormattingElements[listPtr] = nsnull;
  return PR_TRUE;
}

void 
nsHtml5TreeBuilder::push(nsHtml5StackNode* node)
{
  currentPtr++;
  if (currentPtr == stack.length) {
    jArray<nsHtml5StackNode*,PRInt32> newStack = jArray<nsHtml5StackNode*,PRInt32>(stack.length + 64);
    nsHtml5ArrayCopy::arraycopy(stack, newStack, stack.length);
    stack.release();
    stack = newStack;
  }
  stack[currentPtr] = node;
  elementPushed(node->ns, node->popName, node->node);
}

void 
nsHtml5TreeBuilder::append(nsHtml5StackNode* node)
{
  listPtr++;
  if (listPtr == listOfActiveFormattingElements.length) {
    jArray<nsHtml5StackNode*,PRInt32> newList = jArray<nsHtml5StackNode*,PRInt32>(listOfActiveFormattingElements.length + 64);
    nsHtml5ArrayCopy::arraycopy(listOfActiveFormattingElements, newList, listOfActiveFormattingElements.length);
    listOfActiveFormattingElements.release();
    listOfActiveFormattingElements = newList;
  }
  listOfActiveFormattingElements[listPtr] = node;
}

void 
nsHtml5TreeBuilder::insertMarker()
{
  append(MARKER);
}

void 
nsHtml5TreeBuilder::clearTheListOfActiveFormattingElementsUpToTheLastMarker()
{
  while (listPtr > -1) {
    if (listOfActiveFormattingElements[listPtr--] == MARKER) {
      return;
    }
  }
}

PRBool 
nsHtml5TreeBuilder::isCurrent(nsIAtom* name)
{
  return name == stack[currentPtr]->name;
}

void 
nsHtml5TreeBuilder::removeFromStack(PRInt32 pos)
{
  if (currentPtr == pos) {
    pop();
  } else {

    stack[pos]->release();
    nsHtml5ArrayCopy::arraycopy(stack, pos + 1, pos, currentPtr - pos);

    currentPtr--;
  }
}

void 
nsHtml5TreeBuilder::removeFromStack(nsHtml5StackNode* node)
{
  if (stack[currentPtr] == node) {
    pop();
  } else {
    PRInt32 pos = currentPtr - 1;
    while (pos >= 0 && stack[pos] != node) {
      pos--;
    }
    if (pos == -1) {
      return;
    }

    node->release();
    nsHtml5ArrayCopy::arraycopy(stack, pos + 1, pos, currentPtr - pos);
    currentPtr--;
  }
}

void 
nsHtml5TreeBuilder::removeFromListOfActiveFormattingElements(PRInt32 pos)
{
  listOfActiveFormattingElements[pos]->release();
  if (pos == listPtr) {

    listPtr--;
    return;
  }

  nsHtml5ArrayCopy::arraycopy(listOfActiveFormattingElements, pos + 1, pos, listPtr - pos);

  listPtr--;
}

void 
nsHtml5TreeBuilder::adoptionAgencyEndTag(nsIAtom* name)
{
  flushCharacters();
  for (; ; ) {
    PRInt32 formattingEltListPos = listPtr;
    while (formattingEltListPos > -1) {
      nsIAtom* listName = listOfActiveFormattingElements[formattingEltListPos]->name;
      if (listName == name) {
        break;
      } else if (!listName) {
        formattingEltListPos = -1;
        break;
      }
      formattingEltListPos--;
    }
    if (formattingEltListPos == -1) {

      return;
    }
    nsHtml5StackNode* formattingElt = listOfActiveFormattingElements[formattingEltListPos];
    PRInt32 formattingEltStackPos = currentPtr;
    PRBool inScope = PR_TRUE;
    while (formattingEltStackPos > -1) {
      nsHtml5StackNode* node = stack[formattingEltStackPos];
      if (node == formattingElt) {
        break;
      } else if (node->scoping) {
        inScope = PR_FALSE;
      }
      formattingEltStackPos--;
    }
    if (formattingEltStackPos == -1) {

      removeFromListOfActiveFormattingElements(formattingEltListPos);
      return;
    }
    if (!inScope) {

      return;
    }
    if (formattingEltStackPos != currentPtr) {

    }
    PRInt32 furthestBlockPos = formattingEltStackPos + 1;
    while (furthestBlockPos <= currentPtr) {
      nsHtml5StackNode* node = stack[furthestBlockPos];
      if (node->scoping || node->special) {
        break;
      }
      furthestBlockPos++;
    }
    if (furthestBlockPos > currentPtr) {
      while (currentPtr >= formattingEltStackPos) {
        pop();
      }
      removeFromListOfActiveFormattingElements(formattingEltListPos);
      return;
    }
    nsHtml5StackNode* commonAncestor = stack[formattingEltStackPos - 1];
    nsHtml5StackNode* furthestBlock = stack[furthestBlockPos];
    detachFromParent(furthestBlock->node);
    PRInt32 bookmark = formattingEltListPos;
    PRInt32 nodePos = furthestBlockPos;
    nsHtml5StackNode* lastNode = furthestBlock;
    for (; ; ) {
      nodePos--;
      nsHtml5StackNode* node = stack[nodePos];
      PRInt32 nodeListPos = findInListOfActiveFormattingElements(node);
      if (nodeListPos == -1) {



        removeFromStack(nodePos);
        furthestBlockPos--;
        continue;
      }
      if (nodePos == formattingEltStackPos) {
        break;
      }
      if (nodePos == furthestBlockPos) {
        bookmark = nodeListPos + 1;
      }
      if (hasChildren(node->node)) {


        nsIContent* clone = shallowClone(node->node);
        nsHtml5StackNode* newNode = new nsHtml5StackNode(node->group, node->ns, node->name, clone, node->scoping, node->special, node->fosterParenting, node->popName);
        stack[nodePos] = newNode;
        newNode->retain();
        listOfActiveFormattingElements[nodeListPos] = newNode;
        node->release();
        node->release();
        node = newNode;
        nsHtml5Portability::releaseElement(clone);
      }
      detachFromParent(lastNode->node);
      appendElement(lastNode->node, node->node);
      lastNode = node;
    }
    if (commonAncestor->fosterParenting) {

      insertIntoFosterParent(lastNode->node);
    } else {
      detachFromParent(lastNode->node);
      appendElement(lastNode->node, commonAncestor->node);
    }
    nsIContent* clone = shallowClone(formattingElt->node);
    nsHtml5StackNode* formattingClone = new nsHtml5StackNode(formattingElt->group, formattingElt->ns, formattingElt->name, clone, formattingElt->scoping, formattingElt->special, formattingElt->fosterParenting, formattingElt->popName);
    appendChildrenToNewParent(furthestBlock->node, clone);
    appendElement(clone, furthestBlock->node);
    removeFromListOfActiveFormattingElements(formattingEltListPos);
    insertIntoListOfActiveFormattingElements(formattingClone, bookmark);

    removeFromStack(formattingEltStackPos);
    insertIntoStack(formattingClone, furthestBlockPos);
    nsHtml5Portability::releaseElement(clone);
  }
}

void 
nsHtml5TreeBuilder::insertIntoStack(nsHtml5StackNode* node, PRInt32 position)
{


  if (position == currentPtr + 1) {
    flushCharacters();
    push(node);
  } else {
    nsHtml5ArrayCopy::arraycopy(stack, position, position + 1, (currentPtr - position) + 1);
    currentPtr++;
    stack[position] = node;
  }
}

void 
nsHtml5TreeBuilder::insertIntoListOfActiveFormattingElements(nsHtml5StackNode* formattingClone, PRInt32 bookmark)
{
  formattingClone->retain();

  if (bookmark <= listPtr) {
    nsHtml5ArrayCopy::arraycopy(listOfActiveFormattingElements, bookmark, bookmark + 1, (listPtr - bookmark) + 1);
  }
  listPtr++;
  listOfActiveFormattingElements[bookmark] = formattingClone;
}

PRInt32 
nsHtml5TreeBuilder::findInListOfActiveFormattingElements(nsHtml5StackNode* node)
{
  for (PRInt32 i = listPtr; i >= 0; i--) {
    if (node == listOfActiveFormattingElements[i]) {
      return i;
    }
  }
  return -1;
}

PRInt32 
nsHtml5TreeBuilder::findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsIAtom* name)
{
  for (PRInt32 i = listPtr; i >= 0; i--) {
    nsHtml5StackNode* node = listOfActiveFormattingElements[i];
    if (node == MARKER) {
      return -1;
    } else if (node->name == name) {
      return i;
    }
  }
  return -1;
}

PRInt32 
nsHtml5TreeBuilder::findLastOrRoot(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    }
  }
  return 0;
}

PRInt32 
nsHtml5TreeBuilder::findLastOrRoot(PRInt32 group)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->group == group) {
      return i;
    }
  }
  return 0;
}

void 
nsHtml5TreeBuilder::addAttributesToBody(nsHtml5HtmlAttributes* attributes)
{
  if (currentPtr >= 1) {
    nsHtml5StackNode* body = stack[1];
    if (body->group == NS_HTML5TREE_BUILDER_BODY) {
      addAttributesToElement(body->node, attributes);
    }
  }
}

void 
nsHtml5TreeBuilder::pushHeadPointerOntoStack()
{
  flushCharacters();

  if (!headPointer) {

    push(stack[currentPtr]);
  } else {
    push(new nsHtml5StackNode(kNameSpaceID_XHTML, nsHtml5ElementName::HEAD, headPointer));
  }
}

void 
nsHtml5TreeBuilder::reconstructTheActiveFormattingElements()
{
  if (listPtr == -1) {
    return;
  }
  nsHtml5StackNode* mostRecent = listOfActiveFormattingElements[listPtr];
  if (mostRecent == MARKER || isInStack(mostRecent)) {
    return;
  }
  PRInt32 entryPos = listPtr;
  for (; ; ) {
    entryPos--;
    if (entryPos == -1) {
      break;
    }
    if (listOfActiveFormattingElements[entryPos] == MARKER) {
      break;
    }
    if (isInStack(listOfActiveFormattingElements[entryPos])) {
      break;
    }
  }
  if (entryPos < listPtr) {
    flushCharacters();
  }
  while (entryPos < listPtr) {
    entryPos++;
    nsHtml5StackNode* entry = listOfActiveFormattingElements[entryPos];
    nsIContent* clone = shallowClone(entry->node);
    nsHtml5StackNode* entryClone = new nsHtml5StackNode(entry->group, entry->ns, entry->name, clone, entry->scoping, entry->special, entry->fosterParenting, entry->popName);
    nsHtml5StackNode* currentNode = stack[currentPtr];
    if (currentNode->fosterParenting) {
      insertIntoFosterParent(clone);
    } else {
      appendElement(clone, currentNode->node);
    }
    push(entryClone);
    listOfActiveFormattingElements[entryPos] = entryClone;
  }
}

void 
nsHtml5TreeBuilder::insertIntoFosterParent(nsIContent* child)
{
  PRInt32 eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE);
  nsHtml5StackNode* node = stack[eltPos];
  node->tainted = PR_TRUE;
  nsIContent* elt = node->node;
  if (!eltPos) {
    appendElement(child, elt);
    return;
  }
  nsIContent* parent = parentElementFor(elt);
  if (!parent) {
    appendElement(child, stack[eltPos - 1]->node);
  } else {
    insertBefore(child, elt, parent);
  }
}

PRBool 
nsHtml5TreeBuilder::isInStack(nsHtml5StackNode* node)
{
  for (PRInt32 i = currentPtr; i >= 0; i--) {
    if (stack[i] == node) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void 
nsHtml5TreeBuilder::pop()
{
  flushCharacters();
  nsHtml5StackNode* node = stack[currentPtr];

  currentPtr--;
  elementPopped(node->ns, node->popName, node->node);
  node->release();
}

void 
nsHtml5TreeBuilder::appendCharMayFoster(PRUnichar* buf, PRInt32 i)
{
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    PRInt32 eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE);
    nsHtml5StackNode* node = stack[eltPos];
    node->tainted = PR_TRUE;
    nsIContent* elt = node->node;
    if (!eltPos) {
      appendCharacters(elt, buf, i, 1);
      return;
    }
    nsIContent* parent = parentElementFor(elt);
    if (!parent) {
      appendCharacters(stack[eltPos - 1]->node, buf, i, 1);
    } else {
      insertCharactersBefore(buf, i, 1, elt, parent);
    }
  } else {
    accumulateCharacters(buf, i, 1);
  }
}

PRBool 
nsHtml5TreeBuilder::isTainted()
{
  PRInt32 eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE);
  nsHtml5StackNode* node = stack[eltPos];
  return node->tainted;
}

void 
nsHtml5TreeBuilder::appendHtmlElementToDocumentAndPush(nsHtml5HtmlAttributes* attributes)
{
  nsIContent* elt = createHtmlElementSetAsRoot(attributes);
  nsHtml5StackNode* node = new nsHtml5StackNode(kNameSpaceID_XHTML, nsHtml5ElementName::HTML, elt);
  push(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendHtmlElementToDocumentAndPush()
{
  appendHtmlElementToDocumentAndPush(tokenizer->emptyAttributes());
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIContent* elt = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::head, attributes);
  appendElement(elt, stack[currentPtr]->node);
  headPointer = elt;
  nsHtml5StackNode* node = new nsHtml5StackNode(kNameSpaceID_XHTML, nsHtml5ElementName::HEAD, elt);
  push(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushBodyElement(nsHtml5HtmlAttributes* attributes)
{
  appendToCurrentNodeAndPushElement(kNameSpaceID_XHTML, nsHtml5ElementName::BODY, attributes);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushBodyElement()
{
  appendToCurrentNodeAndPushBodyElement(tokenizer->emptyAttributes());
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushFormElementMayFoster(nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIContent* elt = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::form, attributes);
  formPointer = elt;
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(kNameSpaceID_XHTML, nsHtml5ElementName::FORM, elt);
  push(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushFormattingElementMayFoster(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIContent* elt = createElement(ns, elementName->name, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(ns, elementName, elt);
  push(node);
  append(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElement(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIContent* elt = createElement(ns, elementName->name, attributes);
  appendElement(elt, stack[currentPtr]->node);
  nsHtml5StackNode* node = new nsHtml5StackNode(ns, elementName, elt);
  push(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFoster(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIAtom* popName = elementName->name;
  nsIContent* elt = createElement(ns, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(ns, elementName, elt, popName);
  push(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFosterNoScoping(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIAtom* popName = elementName->name;
  nsIContent* elt = createElement(ns, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(ns, elementName, elt, popName, PR_FALSE);
  push(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFosterCamelCase(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIAtom* popName = elementName->camelCaseName;
  nsIContent* elt = createElement(ns, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(ns, elementName, elt, popName, nsHtml5ElementName::FOREIGNOBJECT == elementName);
  push(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFoster(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, nsIContent* form)
{
  flushCharacters();
  nsIContent* elt = createElement(ns, elementName->name, attributes, form);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(ns, elementName, elt);
  push(node);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrentMayFoster(PRInt32 ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent* form)
{
  flushCharacters();
  nsIContent* elt = createElement(ns, name, attributes, form);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  elementPushed(ns, name, elt);
  elementPopped(ns, name, elt);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrentMayFoster(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIAtom* popName = elementName->name;
  nsIContent* elt = createElement(ns, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  elementPushed(ns, popName, elt);
  elementPopped(ns, popName, elt);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrentMayFosterCamelCase(PRInt32 ns, nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  flushCharacters();
  nsIAtom* popName = elementName->camelCaseName;
  nsIContent* elt = createElement(ns, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->fosterParenting) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  elementPushed(ns, popName, elt);
  elementPopped(ns, popName, elt);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrent(PRInt32 ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent* form)
{
  flushCharacters();
  nsIContent* elt = createElement(ns, name, attributes, form);
  nsHtml5StackNode* current = stack[currentPtr];
  appendElement(elt, current->node);
  elementPushed(ns, name, elt);
  elementPopped(ns, name, elt);
  nsHtml5Portability::releaseElement(elt);
}

void 
nsHtml5TreeBuilder::requestSuspension()
{
  tokenizer->requestSuspension();
}

nsIContent* 
nsHtml5TreeBuilder::currentNode()
{
  return stack[currentPtr]->node;
}

PRBool 
nsHtml5TreeBuilder::isScriptingEnabled()
{
  return scriptingEnabled;
}

void 
nsHtml5TreeBuilder::setScriptingEnabled(PRBool scriptingEnabled)
{
  this->scriptingEnabled = scriptingEnabled;
}

void 
nsHtml5TreeBuilder::setDocumentModeHandler(nsHtml5Parser* documentModeHandler)
{
  this->documentModeHandler = documentModeHandler;
}

void 
nsHtml5TreeBuilder::setReportingDoctype(PRBool reportingDoctype)
{
  this->reportingDoctype = reportingDoctype;
}

PRBool 
nsHtml5TreeBuilder::inForeign()
{
  return foreignFlag == NS_HTML5TREE_BUILDER_IN_FOREIGN;
}

void
nsHtml5TreeBuilder::initializeStatics()
{
  ISINDEX_PROMPT = nsHtml5Portability::isIndexPrompt();
  QUIRKY_PUBLIC_IDS = jArray<nsString*,PRInt32>(55);
  QUIRKY_PUBLIC_IDS[0] = nsHtml5StringLiterals::___silmaril__dtd_html_pro_v0r11_19970101__;
  QUIRKY_PUBLIC_IDS[1] = nsHtml5StringLiterals::___advasoft_ltd__dtd_html_3_0_aswedit___extensions__;
  QUIRKY_PUBLIC_IDS[2] = nsHtml5StringLiterals::___as__dtd_html_3_0_aswedit___extensions__;
  QUIRKY_PUBLIC_IDS[3] = nsHtml5StringLiterals::___ietf__dtd_html_2_0_level_1__;
  QUIRKY_PUBLIC_IDS[4] = nsHtml5StringLiterals::___ietf__dtd_html_2_0_level_2__;
  QUIRKY_PUBLIC_IDS[5] = nsHtml5StringLiterals::___ietf__dtd_html_2_0_strict_level_1__;
  QUIRKY_PUBLIC_IDS[6] = nsHtml5StringLiterals::___ietf__dtd_html_2_0_strict_level_2__;
  QUIRKY_PUBLIC_IDS[7] = nsHtml5StringLiterals::___ietf__dtd_html_2_0_strict__;
  QUIRKY_PUBLIC_IDS[8] = nsHtml5StringLiterals::___ietf__dtd_html_2_0__;
  QUIRKY_PUBLIC_IDS[9] = nsHtml5StringLiterals::___ietf__dtd_html_2_1e__;
  QUIRKY_PUBLIC_IDS[10] = nsHtml5StringLiterals::___ietf__dtd_html_3_0__;
  QUIRKY_PUBLIC_IDS[11] = nsHtml5StringLiterals::___ietf__dtd_html_3_2_final__;
  QUIRKY_PUBLIC_IDS[12] = nsHtml5StringLiterals::___ietf__dtd_html_3_2__;
  QUIRKY_PUBLIC_IDS[13] = nsHtml5StringLiterals::___ietf__dtd_html_3__;
  QUIRKY_PUBLIC_IDS[14] = nsHtml5StringLiterals::___ietf__dtd_html_level_0__;
  QUIRKY_PUBLIC_IDS[15] = nsHtml5StringLiterals::___ietf__dtd_html_level_1__;
  QUIRKY_PUBLIC_IDS[16] = nsHtml5StringLiterals::___ietf__dtd_html_level_2__;
  QUIRKY_PUBLIC_IDS[17] = nsHtml5StringLiterals::___ietf__dtd_html_level_3__;
  QUIRKY_PUBLIC_IDS[18] = nsHtml5StringLiterals::___ietf__dtd_html_strict_level_0__;
  QUIRKY_PUBLIC_IDS[19] = nsHtml5StringLiterals::___ietf__dtd_html_strict_level_1__;
  QUIRKY_PUBLIC_IDS[20] = nsHtml5StringLiterals::___ietf__dtd_html_strict_level_2__;
  QUIRKY_PUBLIC_IDS[21] = nsHtml5StringLiterals::___ietf__dtd_html_strict_level_3__;
  QUIRKY_PUBLIC_IDS[22] = nsHtml5StringLiterals::___ietf__dtd_html_strict__;
  QUIRKY_PUBLIC_IDS[23] = nsHtml5StringLiterals::___ietf__dtd_html__;
  QUIRKY_PUBLIC_IDS[24] = nsHtml5StringLiterals::___metrius__dtd_metrius_presentational__;
  QUIRKY_PUBLIC_IDS[25] = nsHtml5StringLiterals::___microsoft__dtd_internet_explorer_2_0_html_strict__;
  QUIRKY_PUBLIC_IDS[26] = nsHtml5StringLiterals::___microsoft__dtd_internet_explorer_2_0_html__;
  QUIRKY_PUBLIC_IDS[27] = nsHtml5StringLiterals::___microsoft__dtd_internet_explorer_2_0_tables__;
  QUIRKY_PUBLIC_IDS[28] = nsHtml5StringLiterals::___microsoft__dtd_internet_explorer_3_0_html_strict__;
  QUIRKY_PUBLIC_IDS[29] = nsHtml5StringLiterals::___microsoft__dtd_internet_explorer_3_0_html__;
  QUIRKY_PUBLIC_IDS[30] = nsHtml5StringLiterals::___microsoft__dtd_internet_explorer_3_0_tables__;
  QUIRKY_PUBLIC_IDS[31] = nsHtml5StringLiterals::___netscape_comm__corp___dtd_html__;
  QUIRKY_PUBLIC_IDS[32] = nsHtml5StringLiterals::___netscape_comm__corp___dtd_strict_html__;
  QUIRKY_PUBLIC_IDS[33] = nsHtml5StringLiterals::___o_reilly_and_associates__dtd_html_2_0__;
  QUIRKY_PUBLIC_IDS[34] = nsHtml5StringLiterals::___o_reilly_and_associates__dtd_html_extended_1_0__;
  QUIRKY_PUBLIC_IDS[35] = nsHtml5StringLiterals::___o_reilly_and_associates__dtd_html_extended_relaxed_1_0__;
  QUIRKY_PUBLIC_IDS[36] = nsHtml5StringLiterals::___softquad_software__dtd_hotmetal_pro_6_0__19990601__extensions_to_html_4_0__;
  QUIRKY_PUBLIC_IDS[37] = nsHtml5StringLiterals::___softquad__dtd_hotmetal_pro_4_0__19971010__extensions_to_html_4_0__;
  QUIRKY_PUBLIC_IDS[38] = nsHtml5StringLiterals::___spyglass__dtd_html_2_0_extended__;
  QUIRKY_PUBLIC_IDS[39] = nsHtml5StringLiterals::___sq__dtd_html_2_0_hotmetal___extensions__;
  QUIRKY_PUBLIC_IDS[40] = nsHtml5StringLiterals::___sun_microsystems_corp___dtd_hotjava_html__;
  QUIRKY_PUBLIC_IDS[41] = nsHtml5StringLiterals::___sun_microsystems_corp___dtd_hotjava_strict_html__;
  QUIRKY_PUBLIC_IDS[42] = nsHtml5StringLiterals::___w3c__dtd_html_3_1995_03_24__;
  QUIRKY_PUBLIC_IDS[43] = nsHtml5StringLiterals::___w3c__dtd_html_3_2_draft__;
  QUIRKY_PUBLIC_IDS[44] = nsHtml5StringLiterals::___w3c__dtd_html_3_2_final__;
  QUIRKY_PUBLIC_IDS[45] = nsHtml5StringLiterals::___w3c__dtd_html_3_2__;
  QUIRKY_PUBLIC_IDS[46] = nsHtml5StringLiterals::___w3c__dtd_html_3_2s_draft__;
  QUIRKY_PUBLIC_IDS[47] = nsHtml5StringLiterals::___w3c__dtd_html_4_0_frameset__;
  QUIRKY_PUBLIC_IDS[48] = nsHtml5StringLiterals::___w3c__dtd_html_4_0_transitional__;
  QUIRKY_PUBLIC_IDS[49] = nsHtml5StringLiterals::___w3c__dtd_html_experimental_19960712__;
  QUIRKY_PUBLIC_IDS[50] = nsHtml5StringLiterals::___w3c__dtd_html_experimental_970421__;
  QUIRKY_PUBLIC_IDS[51] = nsHtml5StringLiterals::___w3c__dtd_w3_html__;
  QUIRKY_PUBLIC_IDS[52] = nsHtml5StringLiterals::___w3o__dtd_w3_html_3_0__;
  QUIRKY_PUBLIC_IDS[53] = nsHtml5StringLiterals::___webtechs__dtd_mozilla_html_2_0__;
  QUIRKY_PUBLIC_IDS[54] = nsHtml5StringLiterals::___webtechs__dtd_mozilla_html__;
}

void
nsHtml5TreeBuilder::releaseStatics()
{
  ISINDEX_PROMPT.release();
  QUIRKY_PUBLIC_IDS.release();
}


#include "nsHtml5TreeBuilderCppSupplement.h"

