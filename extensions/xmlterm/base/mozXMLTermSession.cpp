/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is XMLterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// mozXMLTermSession.cpp: implementation of mozXMLTermSession class

#include "nscore.h"
#include "prlog.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"

#include "nsMemory.h"

#include "nsIDocumentViewer.h"

#include "nsILocalFile.h"
#include "nsNetUtil.h"

#include "nsITextContent.h"

#include "nsIDOMElement.h"
#include "nsISelection.h"
#include "nsIDOMText.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"

#include "nsIDOMHTMLDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMNSRange.h"

#include "nsIViewManager.h"
#include "nsIScrollableView.h"

#include "nsIHTMLContent.h"

#include "mozXMLT.h"
#include "mozIXMLTerminal.h"
#include "mozIXMLTermStream.h"
#include "mozXMLTermUtils.h"
#include "mozXMLTermSession.h"
#include "nsISelectionController.h"
#include "nsReadableUtils.h"
#include "nsIDocument.h"

/////////////////////////////////////////////////////////////////////////
// mozXMLTermSession definition
/////////////////////////////////////////////////////////////////////////
static const char* kWhitespace=" \b\t\r\n";
static const PRUnichar kNBSP = 160;

const char* const mozXMLTermSession::sessionElementNames[] = {
  "session",
  "entry",
  "input",
  "output",
  "prompt",
  "command",
  "stdin",
  "stdout",
  "stderr",
  "mixed",
  "warning"
};

// Should HTML event names should always be in lower case for DOM to work?
const char* const mozXMLTermSession::sessionEventNames[] = {
  "click"
};

const char* const mozXMLTermSession::metaCommandNames[] = {
  "",
  "default",
  "http",
  "js",
  "tree",
  "ls"
};

const char* const mozXMLTermSession::fileTypeNames[] = {
  "plainfile",
  "directory",
  "executable"
};

const char* const mozXMLTermSession::treeActionNames[] = {
  "^",
  "v",
  "<",
  ">",
  "A",
  "H"
};

mozXMLTermSession::mozXMLTermSession() :
  mInitialized(PR_FALSE),
  mXMLTerminal(nsnull),

  mBodyNode(nsnull),
  mMenusNode(nsnull),
  mSessionNode(nsnull),
  mCurrentDebugNode(nsnull),

  mStartEntryNode(nsnull),
  mCurrentEntryNode(nsnull),

  mMaxHistory(20),
  mStartEntryNumber(0),
  mCurrentEntryNumber(0),

  mEntryHasOutput(PR_FALSE),

  mPromptTextNode(nsnull),
  mCommandSpanNode(nsnull),
  mInputTextNode(nsnull),

  mOutputBlockNode(nsnull),
  mOutputDisplayNode(nsnull),
  mOutputTextNode(nsnull),

  mXMLTermStream(nsnull),

  mOutputType(LINE_OUTPUT),
  mOutputDisplayType(NO_NODE),
  mOutputMarkupType(PLAIN_TEXT),

  mMetaCommandType(NO_META_COMMAND),
  mAutoDetect(FIRST_LINE),

  mFirstOutputLine(PR_FALSE),

  mEntryOutputLines(0),
  mPreTextBufferLines(0),
  mPreTextIncomplete(),
  mPreTextBuffered(),
  mPreTextDisplayed(),

  mScreenNode(nsnull),
  mScreenRows(0),
  mScreenCols(0),
  mTopScrollRow(0),
  mBotScrollRow(0),

  mRestoreInputEcho(PR_FALSE),

  mCountExportHTML(0),
  mLastExportHTML(),

  mShellPrompt(),
  mPromptHTML(),
  mFragmentBuffer()

{
}


mozXMLTermSession::~mozXMLTermSession()
{
  Finalize();
}


// Initialize XMLTermSession
NS_IMETHODIMP mozXMLTermSession::Init(mozIXMLTerminal* aXMLTerminal,
                                      nsIPresShell* aPresShell,
                                      nsIDOMDocument* aDOMDocument,
                                      PRInt32 nRows, PRInt32 nCols)
{
  nsresult result = NS_OK;

  XMLT_LOG(mozXMLTermSession::Init,30,("\n"));

  if (mInitialized)
    return NS_ERROR_ALREADY_INITIALIZED;

  if (!aXMLTerminal || !aPresShell || !aDOMDocument)
      return NS_ERROR_NULL_POINTER;

  mXMLTerminal = aXMLTerminal;    // containing XMLTerminal; no addref

  mInitialized = PR_TRUE;

  mScreenRows = nRows;
  mScreenCols = nCols;
  mTopScrollRow = mScreenRows - 1;
  mBotScrollRow = 0;

  nsCOMPtr<nsIDOMDocument> domDoc;
  result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(result) || !domDoc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLDocument> vDOMHTMLDocument
                                             (do_QueryInterface(domDoc));
  if (!vDOMHTMLDocument)
    return NS_ERROR_FAILURE;

  // Locate document body node
  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsAutoString bodyTag;
  bodyTag.AssignLiteral("body");
  result = vDOMHTMLDocument->GetElementsByTagName(bodyTag,
                                                  getter_AddRefs(nodeList));
  if (NS_FAILED(result) || !nodeList)
    return NS_ERROR_FAILURE;

  PRUint32 count;
  nodeList->GetLength(&count);
  PR_ASSERT(count==1);

  result = nodeList->Item(0, getter_AddRefs(mBodyNode));
  if (NS_FAILED(result) || !mBodyNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMElement> menusElement;
  nsAutoString menusID( NS_LITERAL_STRING("menus") );
  result = vDOMHTMLDocument->GetElementById(menusID,
                                            getter_AddRefs(menusElement));

  if (NS_SUCCEEDED(result) && menusElement) {
    mMenusNode = do_QueryInterface(menusElement);
  }

  // Use body node as session node by default
  mSessionNode = mBodyNode;

  nsCOMPtr<nsIDOMElement> sessionElement;
  nsAutoString sessionID;
  sessionID.AssignASCII(sessionElementNames[SESSION_ELEMENT]);
  result = vDOMHTMLDocument->GetElementById(sessionID,
                                            getter_AddRefs(sessionElement));

  if (NS_SUCCEEDED(result) && sessionElement) {
    // Specific session node
    mSessionNode = do_QueryInterface(sessionElement);
  }

  mCurrentDebugNode = mSessionNode;

  // Create preface element to display initial output
  result = NewPreface();
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

#if 0
  nsAutoString prefaceText ("Preface");
  result = AppendOutput(prefaceText, EmptyString(), PR_TRUE);
#endif

  XMLT_LOG(mozXMLTermSession::Init,31,("exiting\n"));
  return result;
}


// De-initialize XMLTermSession
NS_IMETHODIMP mozXMLTermSession::Finalize(void)
{

  if (!mInitialized)
    return NS_OK;

  XMLT_LOG(mozXMLTermSession::Finalize,30,("\n"));

  mInitialized = PR_FALSE;

  mScreenNode = nsnull;

  mOutputBlockNode = nsnull;
  mOutputDisplayNode = nsnull;
  mOutputTextNode = nsnull;

  mXMLTermStream = nsnull;

  mPromptTextNode = nsnull;
  mCommandSpanNode = nsnull;
  mInputTextNode = nsnull;

  mStartEntryNode = nsnull;
  mCurrentEntryNode = nsnull;

  mBodyNode = nsnull;
  mMenusNode = nsnull;
  mSessionNode = nsnull;
  mCurrentDebugNode = nsnull;

  mXMLTerminal = nsnull;

  XMLT_LOG(mozXMLTermSession::Finalize,32,("END\n"));

  return NS_OK;
}


/** Resizes XMLterm to match a resized window.
 * @param lineTermAux LineTermAux object to be resized (may be null)
 */
NS_IMETHODIMP mozXMLTermSession::Resize(mozILineTermAux* lineTermAux)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::Resize,70,("\n"));

  // Determine current screen dimensions
  PRInt32 nRows, nCols, xPixels, yPixels;
  result = mXMLTerminal->ScreenSize(&nRows, &nCols, &xPixels, &yPixels);
  if (NS_FAILED(result))
    return result;

  // If dimensions haven't changed, do nothing
  if ((nRows == mScreenRows) && (nCols == mScreenCols))
    return NS_OK;

  mScreenRows = nRows;
  mScreenCols = nCols;

  mTopScrollRow = mScreenRows - 1;
  mBotScrollRow = 0;

  XMLT_LOG(mozXMLTermSession::Resize,72,
       ("Resizing XMLterm, nRows=%d, nCols=%d\n", mScreenRows, mScreenCols));

  if (lineTermAux) {
    // Resize associated LineTerm
    result = lineTermAux->ResizeAux(mScreenRows, mScreenCols);
    if (NS_FAILED(result))
      return result;
  }

  return NS_OK;
}


/** Preprocesses user input before it is transmitted to LineTerm
 * @param aString (inout) input data to be preprocessed
 * @param consumed (output) PR_TRUE if input data has been consumed
 * @param checkSize (output) PR_TRUE if terminal size needs to be checked
 */
NS_IMETHODIMP mozXMLTermSession::Preprocess(const nsString& aString,
                                            PRBool& consumed,
                                            PRBool& checkSize)
{

  XMLT_LOG(mozXMLTermSession::Preprocess,70,("\n"));

  consumed = PR_FALSE;
  checkSize = PR_FALSE;

  if (mMetaCommandType == TREE_META_COMMAND) {
    if (aString.Length() == 1) {
      // Navigate the DOM tree from keyboard
      PRUnichar uch = aString.CharAt(0);

      XMLT_LOG(mozXMLTermSession::Preprocess,60,("char=0x%x\n", uch));

      consumed = PR_TRUE;
      switch (uch) {
      case U_CTL_B:
        TraverseDOMTree(stderr, mBodyNode, mCurrentDebugNode,
                        TREE_MOVE_LEFT);
        break;

      case U_CTL_F:
        TraverseDOMTree(stderr, mBodyNode, mCurrentDebugNode,
                        TREE_MOVE_RIGHT);
        break;

      case U_CTL_N:
        TraverseDOMTree(stderr, mBodyNode, mCurrentDebugNode,
                        TREE_MOVE_DOWN);
        break;

      case U_CTL_P:
        TraverseDOMTree(stderr, mBodyNode, mCurrentDebugNode,
                        TREE_MOVE_UP);
        break;

      case U_A_CHAR:
      case U_a_CHAR:
        TraverseDOMTree(stderr, mBodyNode, mCurrentDebugNode,
                        TREE_PRINT_ATTS);
        break;

      case U_H_CHAR:
      case U_h_CHAR:
        TraverseDOMTree(stderr, mBodyNode, mCurrentDebugNode,
                        TREE_PRINT_HTML);
        break;

      case U_Q_CHAR:
      case U_q_CHAR:
      case U_CTL_C:
        // End of keyboard command sequence; reset debug node to session node
        mCurrentDebugNode = mSessionNode;
        mMetaCommandType = NO_META_COMMAND;
        break;

      default:
        break;
      }
    }
  } else {

    if ((mScreenNode == nsnull) &&
        (aString.FindCharInSet("\r\n\017") >= 0)) {
      // C-Return or Newline or Control-O found in string; not screen mode;
      // resize terminal, if need be
      checkSize = PR_TRUE;
      XMLT_LOG(mozXMLTermSession::Preprocess,72,("checkSize\n"));
    }
  }

  return NS_OK;
}


/** Reads all available data from LineTerm and displays it;
 * returns when no more data is available.
 * @param lineTermAux LineTermAux object to read data from
 * @param processedData (output) PR_TRUE if any data was processed
 */
NS_IMETHODIMP mozXMLTermSession::ReadAll(mozILineTermAux* lineTermAux,
                                         PRBool& processedData)
{
  PRInt32 opcodes, opvals, buf_row, buf_col;
  PRUnichar *buf_str, *buf_style;
  PRBool newline, errorFlag, streamData, screenData;
  nsAutoString bufString, bufStyle;
  nsAutoString abortCode;
  abortCode.SetLength(0);

  XMLT_LOG(mozXMLTermSession::ReadAll,60,("\n"));

  processedData = PR_FALSE;

  if (lineTermAux == nsnull)
    return NS_ERROR_FAILURE;

  nsresult result = NS_OK;
  PRBool flushOutput = PR_FALSE;

  PRBool metaNextCommand = PR_FALSE;

  // NOTE: Do not execute return statements within this loop ;
  //       always break out of the loop after setting result to an error value,
  //       allowing cleanup processing on error
  for (;;) {
    // NOTE: Remember to de-allocate buf_str and buf_style
    //       using nsMemory::Free, if opcodes != 0
    result = lineTermAux->ReadAux(&opcodes, &opvals, &buf_row, &buf_col,
                                  &buf_str, &buf_style);
    if (NS_FAILED(result)) {
      abortCode.AssignLiteral("lineTermReadAux");
      break;
    }

    XMLT_LOG(mozXMLTermSession::ReadAll,62,
           ("opcodes=0x%x,mOutputType=%d,mEntryHasOutput=%d\n",
            opcodes, mOutputType, mEntryHasOutput));

    if (opcodes == 0) break;

    processedData = PR_TRUE;

    screenData = (opcodes & LTERM_SCREENDATA_CODE);
    streamData = (opcodes & LTERM_STREAMDATA_CODE);
    newline =    (opcodes & LTERM_NEWLINE_CODE);
    errorFlag =  (opcodes & LTERM_ERROR_CODE);

    // Copy character/style strings
    bufString = buf_str;
    bufStyle = buf_style;

    // De-allocate buf_str, buf_style using nsMemory::Free
    nsMemory::Free(buf_str);
    nsMemory::Free(buf_style);

    char* temCString = ToNewCString(bufString);
    XMLT_LOG(mozXMLTermSession::ReadAll,68,("bufString=%s\n", temCString));
    nsCRT::free(temCString);

    if (screenData && (mOutputType != SCREEN_OUTPUT)) {
      // Initiate screen mode
      XMLT_LOG(mozXMLTermSession::ReadAll,62,("Initiate SCREEN mode\n"));

      // Break output display
      result = BreakOutput(PR_FALSE);
      if (NS_FAILED(result))
        break;

      // Create screen element
      result = NewScreen();
      if (NS_FAILED(result))
        break;

      mOutputType = SCREEN_OUTPUT;

      // Disable input echo
      lineTermAux->SetEchoFlag(PR_FALSE);
      mRestoreInputEcho = PR_TRUE;
    }

    if (!screenData && (mOutputType == SCREEN_OUTPUT)) {
      // Terminate screen mode
      mOutputType = LINE_OUTPUT;

      XMLT_LOG(mozXMLTermSession::ReadAll,62,
               ("Terminating screen mode\n"));

      // Uncollapse non-screen stuff
      nsAutoString attName(NS_LITERAL_STRING("xmlt-block-collapsed"));

      nsCOMPtr<nsIDOMElement> menusElement = do_QueryInterface(mMenusNode);

      if (NS_SUCCEEDED(result) && menusElement) {
        menusElement->RemoveAttribute(attName);
      }

      nsCOMPtr<nsIDOMElement> sessionElement = do_QueryInterface(mSessionNode);

      if (sessionElement) {
        sessionElement->RemoveAttribute(attName);
      }

      // Delete screen element
      nsCOMPtr<nsIDOMNode> resultNode;
      mBodyNode->RemoveChild(mScreenNode, getter_AddRefs(resultNode));
      if (NS_FAILED(result))
        break;
      mScreenNode = nsnull;

      if (mRestoreInputEcho) {
        lineTermAux->SetEchoFlag(PR_TRUE);
        mRestoreInputEcho = PR_FALSE;
      }

      // Show the caret
      // WORKAROUND for some unknown bug in the full screen implementation.
      // Without this, if you delete a line using "vi" and save the file,
      // the cursor suddenly disappears
      mXMLTerminal->ShowCaret();
    }

    if (streamData) {
      // Process stream data
      if (mOutputType != STREAM_OUTPUT) {
        mOutputType = STREAM_OUTPUT;

        // Disable input echo
        lineTermAux->SetEchoFlag(PR_FALSE);
        mRestoreInputEcho = PR_TRUE;

        // Determine effective stream URL and default markup type
        nsAutoString streamURL;
        OutputMarkupType streamMarkupType;
        PRBool streamIsSecure = (opcodes & LTERM_COOKIESTR_CODE);

        if (streamIsSecure) {
          // Secure stream, i.e., prefixed with cookie; fragments allowed
          streamURL.AssignLiteral("chrome://xmlterm/content/xmltblank.html");

          if (opcodes & LTERM_JSSTREAM_CODE) {
            // Javascript stream 
            streamMarkupType = JS_FRAGMENT;

          } else {
            // HTML/XML stream
            streamMarkupType = HTML_FRAGMENT;
          }

        } else {
          // Insecure stream; do not display
          streamURL.AssignLiteral("http://in.sec.ure");
          streamMarkupType = INSECURE_FRAGMENT;
        }

        if (!(opcodes & LTERM_JSSTREAM_CODE) &&
            (opcodes & LTERM_DOCSTREAM_CODE)) {
          // Stream contains complete document (not Javascript)

          if (opcodes & LTERM_XMLSTREAM_CODE) {
            streamMarkupType = XML_DOCUMENT;
          } else {
            streamMarkupType = HTML_DOCUMENT;
          }
        }

        // Initialize stream output
        result = InitStream(streamURL, streamMarkupType, streamIsSecure);
        if (NS_FAILED(result))
          break;
      }

      // Process stream output
      bufStyle.SetLength(0);
      result = ProcessOutput(bufString, bufStyle, PR_FALSE, PR_TRUE);
      if (NS_FAILED(result))
        break;

      if (newline) {
        if (!mEntryHasOutput) {
          // Start of command output
          mEntryHasOutput = PR_TRUE;
        }

        if (errorFlag) {
          mOutputMarkupType = INCOMPLETE_FRAGMENT;
        }

        // Break stream output display
        result = BreakOutput(PR_TRUE);
        if (NS_FAILED(result))
          break;

        mOutputType = LINE_OUTPUT;
        flushOutput = PR_TRUE;
      }

    } else if (screenData) {
      // Process screen data

      if (opcodes & LTERM_CLEAR_CODE) {
        // Clear screen
        XMLT_LOG(mozXMLTermSession::ReadAll,62,
                 ("Clear screen, opvals=%d, buf_row=%d\n",
                  opvals, buf_row));

        nsCOMPtr<nsIDOMNode> resultNode;
        result = mBodyNode->RemoveChild(mScreenNode,
                                           getter_AddRefs(resultNode));
        if (NS_FAILED(result))
          break;

        mScreenNode = nsnull;

        // Create new screen element
        result = NewScreen();
        if (NS_FAILED(result))
          break;

      } else if (opcodes & LTERM_INSERT_CODE) {
        // Insert rows
        PRInt32 row;
        nsCOMPtr<nsIDOMNode> rowNode, resultNode;

        XMLT_LOG(mozXMLTermSession::ReadAll,62,
                 ("Insert rows, opvals=%d, buf_row=%d\n",
                  opvals, buf_row));

        if (opvals > 0) {
          // Delete row elements below
          for (row=0; row < opvals; row++) {
            result = GetRow(mBotScrollRow+opvals-1, getter_AddRefs(rowNode));
            if (NS_FAILED(result) || !rowNode)
              break;

            result = mScreenNode->RemoveChild(rowNode,
                                              getter_AddRefs(resultNode));
            if (NS_FAILED(result))
              break;
          }
          if (NS_FAILED(result))
            break;

          // Insert individual row elements above
          if (buf_row < opvals) {
            rowNode = nsnull;
          } else {
            result = GetRow(buf_row, getter_AddRefs(rowNode));
            if (NS_FAILED(result))
              break;
          }

          for (row=0; row < opvals; row++)
            NewRow(rowNode, getter_AddRefs(resultNode));
        }

      } else if (opcodes & LTERM_DELETE_CODE) {
        // Delete rows
        PRInt32 row;
        nsCOMPtr<nsIDOMNode> rowNode, resultNode;

        XMLT_LOG(mozXMLTermSession::ReadAll,62,
                 ("Delete rows, opvals=%d, buf_row=%d\n",
                  opvals, buf_row));

        if (opvals > 0) {
          // Delete row elements below
          for (row=0; row < opvals; row++) {
            result = GetRow(buf_row, getter_AddRefs(rowNode));
            if (NS_FAILED(result) || !rowNode)
              break;

            result = mScreenNode->RemoveChild(rowNode,
                                              getter_AddRefs(resultNode));
            if (NS_FAILED(result))
              break;
          }
          if (NS_FAILED(result))
            break;

          // Insert individual row elements above
          if (mBotScrollRow == 0) {
            rowNode = nsnull;
          } else {
            result = GetRow(mBotScrollRow+opvals-1, getter_AddRefs(rowNode));
            if (NS_FAILED(result))
              break;
          }

          for (row=0; row < opvals; row++)
            NewRow(rowNode, getter_AddRefs(resultNode));
        }

      } else if (opcodes & LTERM_SCROLL_CODE) {
        // Set scrolling region
        XMLT_LOG(mozXMLTermSession::ReadAll,62,
                 ("Set scrolling region, opvals=%d, buf_row=%d\n",
                  opvals, buf_row));

        mTopScrollRow = opvals;
        mBotScrollRow = buf_row;

      } else if (opcodes & LTERM_OUTPUT_CODE) {
        // Display row
        XMLT_LOG(mozXMLTermSession::ReadAll,62,
                 ("Display buf_row=%d\n",
                  buf_row));

        result = DisplayRow(bufString, bufStyle, buf_row);
        if (NS_FAILED(result))
          break;
      }

      // Determine cursor position and position cursor
      PRInt32 cursorRow = 0;
      PRInt32 cursorCol = 0;
      result = lineTermAux->GetCursorRow(&cursorRow);
      result = lineTermAux->GetCursorColumn(&cursorCol);

      XMLT_LOG(mozXMLTermSession::ReadAll,62, ("cursorRow=%d, cursorCol=%d\n",
                                               cursorRow, cursorCol));

      result = PositionScreenCursor(cursorRow, cursorCol);

      flushOutput = PR_FALSE;

    } else {
      // Process line data
      PRBool promptLine, inputLine, metaCommand, completionRequested;

      flushOutput = PR_TRUE;

      inputLine =   (opcodes & LTERM_INPUT_CODE);
      promptLine =  (opcodes & LTERM_PROMPT_CODE);
      metaCommand = (opcodes & LTERM_META_CODE);
      completionRequested = (opcodes & LTERM_COMPLETION_CODE);

      nsAutoString promptStr;
      PRInt32 promptLength = 0;
      promptStr.SetLength(0);

      if (promptLine) {
        // Count prompt characters
        const PRUnichar *styleVals = bufStyle.get();
        const PRInt32 bufLength = bufStyle.Length();

        for (promptLength=0; promptLength<bufLength; promptLength++) {
          if (styleVals[promptLength] != LTERM_PROMPT_STYLE)
            break;
        }

        XMLT_LOG(mozXMLTermSession::ReadAll,62,
                 ("bufLength=%d, promptLength=%d, styleVals[0]=0x%x\n",
                  bufLength, promptLength, styleVals[0]));

        PR_ASSERT(promptLength > 0);

        // Extract prompt string
        bufString.Left(promptStr, promptLength);

        if ( (promptLength < bufLength) &&
             !inputLine &&
             !promptStr.Equals(mShellPrompt) ) {
          // Ignore the mismatched prompt in the output line
          int j;
          promptLine = 0;

          for (j=0; j<promptLength; j++)
            bufStyle.SetCharAt((UNICHAR) LTERM_STDOUT_STYLE, j);

        } else {
          // Remove prompt chars/style from buffer strings
          bufString.Cut(0, promptLength);
          bufStyle.Cut(0, promptLength);

          // Save prompt string
          mShellPrompt = promptStr;
        }
      }

      if (!metaCommand && inputLine) {
        if (metaNextCommand) {
          // Echo of transmitted meta command
          metaNextCommand = PR_FALSE;

        } else {
          // No meta command; enable input echo
          mMetaCommandType = NO_META_COMMAND;

          if (mRestoreInputEcho) {
            lineTermAux->SetEchoFlag(PR_TRUE);
            mRestoreInputEcho = PR_FALSE;
          }
        }
      }

      if (metaCommand && !completionRequested) {
        // Identify meta command type

        // Eliminate leading spaces/TABs
        nsAutoString metaLine = bufString;
        metaLine.Trim(kWhitespace, PR_TRUE, PR_FALSE);

        int delimOffset = metaLine.FindChar((PRUnichar) ':');
        PR_ASSERT(delimOffset >= 0);

        XMLT_LOG(mozXMLTermSession::ReadAll,62,
                 ("delimOffset=%d\n", delimOffset));

        if (delimOffset == 0) {
          // Default protocol
          mMetaCommandType = DEFAULT_META_COMMAND;

        } else {
          // Identify meta command type
          mMetaCommandType = NO_META_COMMAND;

          nsAutoString temString;
          metaLine.Left(temString, delimOffset);

          PRInt32 j;
          for (j=NO_META_COMMAND+1; j<META_COMMAND_TYPES; j++) {
            if (temString.EqualsASCII(metaCommandNames[j])) {
              mMetaCommandType = (MetaCommandType) j;
              break;
            }
          }
        }

        XMLT_LOG(mozXMLTermSession::ReadAll,62,("mMetaCommandType=%d\n",
                                               mMetaCommandType));

        // Extract command arguments
        int argChars = metaLine.Length() - delimOffset - 1;
        nsAutoString commandArgs;
        metaLine.Right(commandArgs, argChars);

        // Eliminate leading spaces/TABs
        commandArgs.Trim(kWhitespace, PR_TRUE, PR_FALSE);

        // Display meta command
        if (mEntryHasOutput) {
          // Break previous output display
          result = BreakOutput(PR_FALSE);

          // Create new entry block
          result = NewEntry(promptStr);
          if (NS_FAILED(result))
            break;
        }

        // Display input and position cursor
        PRInt32 cursorCol = 0;
        result = lineTermAux->GetCursorColumn(&cursorCol);

        // Remove prompt offset
        cursorCol -= promptLength;
        if (cursorCol < 0) cursorCol = 0;

        XMLT_LOG(mozXMLTermSession::ReadAll,62,("cursorCol=%d\n", cursorCol));

        result = DisplayInput(bufString, bufStyle, cursorCol);
        if (NS_FAILED(result))
          break;

        if (newline && mXMLTerminal) {
          // Complete meta command; XMLterm instantiated
          nsAutoString metaCommandOutput;
	  metaCommandOutput.SetLength(0);

          nsCOMPtr<nsIDOMDocument> domDoc;
          result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
          if (NS_FAILED(result) || !domDoc)
            break;

          switch (mMetaCommandType) {

          case DEFAULT_META_COMMAND:
            {
              // Construct Javascript command to handle default meta command
              nsAutoString JSCommand;
	      JSCommand.AssignLiteral("MetaDefault(\"");
              JSCommand.Append(commandArgs);
              JSCommand.Append(NS_LITERAL_STRING("\");"));

              // Execute JavaScript command
              result = mozXMLTermUtils::ExecuteScript(domDoc,
                                                      JSCommand,
                                                      metaCommandOutput);
              nsCAutoString cstrout;
              if (NS_SUCCEEDED(result))
                CopyUCS2toASCII(metaCommandOutput, cstrout);
              else
                cstrout = "Error in displaying URL\n";
              XMLT_LOG(mozXMLTermSession::ReadAll,63,
                       ("DEFAULT_META output=%s\n", cstrout.get()));

            }
            break;

          case HTTP_META_COMMAND:
            {
              // Display URL using IFRAME
              nsAutoString url;
	      url.AssignLiteral("http:");
              url.Append(commandArgs);
              nsAutoString width;
	      width.AssignLiteral("100%");
              nsAutoString height;
	      height.AssignLiteral("100");
              result = NewIFrame(mOutputBlockNode, mCurrentEntryNumber,
                                 2, url, width, height);
              if (NS_FAILED(result))
                metaCommandOutput.AssignLiteral("Error in displaying URL\n");

            }
            break;

          case JS_META_COMMAND:
            {
              // Execute JavaScript command
              result = mozXMLTermUtils::ExecuteScript(domDoc,
                                                      commandArgs,
                                                      metaCommandOutput);
              nsCAutoString cstrout;
              if (NS_SUCCEEDED(result))
                CopyUCS2toASCII(metaCommandOutput, cstrout);
              else
                cstrout = "Error in executing JavaScript command\n";
              XMLT_LOG(mozXMLTermSession::ReadAll,63,
                       ("JS output=%s\n", cstrout.get()));

            }
            break;

          case TREE_META_COMMAND:
            XMLT_WARNING("\nTraverseDOMTree: use arrow keys; A for attributes; H for HTML; Q to quit\n");
            break;

          case LS_META_COMMAND:
            {
              // Disable input echo and transmit command
              lineTermAux->SetEchoFlag(PR_FALSE);
              nsAutoString lsCommand;
              lsCommand.SetLength(0);

              if (!commandArgs.IsEmpty()) {
                lsCommand.AppendLiteral("cd ");
                lsCommand.Append(commandArgs);
                lsCommand.AppendLiteral(";");
              }

              lsCommand.AppendLiteral("ls -dF `pwd`/*\n");

              //mXMLTerminal->SendText(lsCommand);

              /* Set flag to recognize transmitted command */
              metaNextCommand = PR_TRUE;
              mRestoreInputEcho = PR_TRUE;
            }
            break;

          default:
            break;
          }

          if ((mMetaCommandType == DEFAULT_META_COMMAND) ||
              (mMetaCommandType == JS_META_COMMAND)) {
            // Display metacommand output
            mEntryHasOutput = PR_TRUE;

            XMLT_LOG(mozXMLTermSession::ReadAll,62,("metaCommandOutput\n"));

            // Ignore the string "false", if that's the only output
            if (metaCommandOutput.EqualsLiteral("false"))
              metaCommandOutput.SetLength(0);

            // Check metacommand output for markup (secure)
            result = AutoDetectMarkup(metaCommandOutput, PR_TRUE, PR_TRUE);
            if (NS_FAILED(result))
              break;

            nsAutoString nullStyle;
            nullStyle.SetLength(0);
            result = ProcessOutput(metaCommandOutput, nullStyle, PR_TRUE,
                                   mOutputMarkupType != PLAIN_TEXT);
            if (NS_FAILED(result))
              break;

            // Break metacommand output display
            result = BreakOutput(PR_FALSE);
          }

          // Reset newline flag
          newline = PR_FALSE;
        }

        // Clear the meta command from the string nuffer
        bufString.SetLength(0);
        bufStyle.SetLength(0);
      }

      if (promptLine) {
        // Prompt line
        if (mEntryHasOutput) {
          // Break previous output display
          result = BreakOutput(PR_FALSE);

          // Create new entry block
          result = NewEntry(promptStr);
          if (NS_FAILED(result))
            break;

          if (mCurrentEntryNumber == mStartEntryNumber) {
            // First entry; resize terminal
            result = Resize(lineTermAux);
            if (NS_FAILED(result))
              break;
          }
        }

        // Display input and position cursor
        PRInt32 cursorCol = 0;
        result = lineTermAux->GetCursorColumn(&cursorCol);

        // Remove prompt offset
        cursorCol -= promptLength;
        if (cursorCol < 0) cursorCol = 0;

        XMLT_LOG(mozXMLTermSession::ReadAll,62,("cursorCol=%d\n", cursorCol));

        result = DisplayInput(bufString, bufStyle, cursorCol);
        if (NS_FAILED(result))
          break;

        if (newline) {
          // Start of command output
          // (this is needed to properly handle commands with no output!)
          mEntryHasOutput = PR_TRUE;
          mFirstOutputLine = PR_TRUE;

        }

      } else {
        // Not prompt line
        if (!mEntryHasOutput) {
          // Start of command output
          mEntryHasOutput = PR_TRUE;
          mFirstOutputLine = PR_TRUE;
        }

        if (newline) {
          // Complete line; check for markup (insecure)
          result = AutoDetectMarkup(bufString, mFirstOutputLine, PR_FALSE);
          if (NS_FAILED(result))
            break;

          // Not first output line anymore
          mFirstOutputLine = PR_FALSE;
        }

        if (mOutputMarkupType == PLAIN_TEXT) {
          // Display plain text output
          result = ProcessOutput(bufString, bufStyle, newline, PR_FALSE);
          if (NS_FAILED(result))
            break;

        } else if (newline) {
          // Process autodetected stream output (complete lines only)
          bufStyle.SetLength(0);
          result = ProcessOutput(bufString, bufStyle, PR_TRUE, PR_TRUE);
          if (NS_FAILED(result))
            break;
        }
      }
    }
  }

  if (NS_FAILED(result)) {
    // Error processing; close LineTerm
    XMLT_LOG(mozXMLTermSession::ReadAll,62,
             ("Aborting on error, result=0x%x\n", result));

    Abort(lineTermAux, abortCode);
    return result;
  }

  if (flushOutput) {
    // Flush output, splitting off incomplete line
    FlushOutput(SPLIT_INCOMPLETE_FLUSH);

    if (mEntryHasOutput)
      PositionOutputCursor(lineTermAux);

    nsCOMPtr<nsISelectionController> selCon;
    result = mXMLTerminal->GetSelectionController(getter_AddRefs(selCon));
    if (NS_FAILED(result) || !selCon)
      return NS_ERROR_FAILURE;

    selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                    nsISelectionController::SELECTION_FOCUS_REGION,
                                    PR_TRUE);

  }

  // Show caret
  mXMLTerminal->ShowCaret();

  // Scroll frame (ignore result)
  ScrollToBottomLeft();

  return NS_OK;
}


/** Exports HTML to file, with META REFRESH, if refreshSeconds is non-zero.
 * Nothing is done if display has not changed since last export, unless
 * forceExport is true. Returns true if export actually takes place.
 * If filename is a null string, HTML is written to STDERR.
 */
NS_IMETHODIMP mozXMLTermSession::ExportHTML(const PRUnichar* aFilename,
                                            PRInt32 permissions,
                                            const PRUnichar* style,
                                            PRUint32 refreshSeconds,
                                            PRBool forceExport,
                                            PRBool* exported)
{
  nsresult result;

  if (!aFilename || !exported)
    return NS_ERROR_NULL_POINTER;

  *exported = PR_FALSE;

  if (forceExport)
    mLastExportHTML.SetLength(0);

  nsAutoString indentString; indentString.SetLength(0);
  nsAutoString htmlString;
  result = ToHTMLString(mBodyNode, indentString, htmlString,
                        PR_TRUE, PR_FALSE );
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  if (htmlString.Equals(mLastExportHTML))
    return NS_OK;

  mLastExportHTML.Assign( htmlString );
  mCountExportHTML++;

  nsAutoString filename( aFilename );

  if (filename.IsEmpty()) {
    // Write to STDERR
    char* htmlCString = ToNewCString(htmlString);
    fprintf(stderr, "mozXMLTermSession::ExportHTML:\n%s\n\n", htmlCString);
    nsCRT::free(htmlCString);

    *exported = PR_TRUE;
    return NS_OK;
  }

  // Copy HTML to local file
  nsCOMPtr<nsILocalFile> localFile = do_CreateInstance( NS_LOCAL_FILE_CONTRACTID, &result);
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  XMLT_LOG(mozXMLTermSession::ExportHTML,0,
           ("Exporting %d\n", mCountExportHTML));

  result = localFile->InitWithPath(filename);
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  PRInt32 ioFlags = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;

  nsCOMPtr<nsIOutputStream> outStream;
  result = NS_NewLocalFileOutputStream(getter_AddRefs(outStream),
                                       localFile, ioFlags, permissions);
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  PRUint32 writeCount;

  nsCAutoString cString( "<html>\n<head>\n" );

  if (refreshSeconds > 0) {
     cString.Append("<META HTTP-EQUIV='refresh' content='");
     cString.AppendInt(refreshSeconds);
     cString.Append("'>");
  }

  cString.Append("<title>xmlterm page</title>\n");
  cString.Append("<link title='defaultstyle' rel='stylesheet' type='text/css' href='xmlt.css'>\n");

  if (style) {
    cString.Append("<style type='text/css'>\n");
    AppendUTF16toUTF8(style, cString);
    cString.Append("</style>\n");
  }

  cString.Append("<script language='JavaScript'>var exportCount=");
  cString.AppendInt(mCountExportHTML);
  cString.Append(";</script>\n");
  cString.Append("<script language='JavaScript' src='xmlt.js'></script>\n</head>");

  AppendUTF16toUTF8(htmlString, cString);

  cString.Append("</html>\n");

  result = outStream->Write(cString.get(), cString.Length(),
                            &writeCount);
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  result = outStream->Flush();

  result = outStream->Close();

  *exported = PR_TRUE;
  return NS_OK;
}


/** Aborts session by closing LineTerm and displays an error message
 * @param lineTermAux LineTermAux object to be closed
 * @param abortCode abort code string to dbe displayed
 */
NS_IMETHODIMP mozXMLTermSession::Abort(mozILineTermAux* lineTermAux,
                                       nsString& abortCode)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::Abort,70,
           ("Aborting session; closing LineTerm\n"));

  // Close LineTerm
  lineTermAux->CloseAux();

  // Display error message using DIV node
  nsCOMPtr<nsIDOMNode> divNode, textNode;
  nsAutoString tagName(NS_LITERAL_STRING("div"));
  nsAutoString elementName(NS_LITERAL_STRING("errmsg"));
  result = NewElementWithText(tagName, elementName, -1,
                              mSessionNode, divNode, textNode);

  if (NS_SUCCEEDED(result) && divNode && textNode) {
    nsAutoString errMsg(NS_LITERAL_STRING("Error in XMLterm (code "));
    errMsg.Append(abortCode);
    errMsg.Append(NS_LITERAL_STRING("); session closed."));
    SetDOMText(textNode, errMsg);

    // Collapse selection and position cursor
    nsCOMPtr<nsISelectionController> selCon;
    result = mXMLTerminal->GetSelectionController(getter_AddRefs(selCon));
    if (NS_FAILED(result) || !selCon)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsISelection> selection;
    result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                      getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection) {
      selection->Collapse(textNode, errMsg.Length());
      if (NS_SUCCEEDED(result)) {
        selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                        nsISelectionController::SELECTION_FOCUS_REGION,
                                        PR_TRUE);
      }
    }
  }

  return NS_OK;
}


/** Displays ("echoes") input text string with style and positions cursor
 * @param aString string to be displayed
 * @param aStyle style values for string (see lineterm.h)
 * @param cursorCol cursor column
 */
NS_IMETHODIMP mozXMLTermSession::DisplayInput(const nsString& aString,
                                              const nsString& aStyle,
                                              PRInt32 cursorCol)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::DisplayInput,70,("cursorCol=%d\n", cursorCol));

  // If string terminates in whitespace, append NBSP for cursor positioning
  nsAutoString tempString( aString );
  if (!aString.IsEmpty() && aString.Last() == PRUnichar(' '))
    tempString += kNBSP;

  // Display string
  result = SetDOMText(mInputTextNode, tempString);

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  char* temCString = ToNewCString(aString);
  XMLT_LOG(mozXMLTermSession::DisplayInput,72,
           ("aString=%s\n", temCString));
  nsCRT::free(temCString);

  // Collapse selection and position cursor
  nsCOMPtr<nsISelectionController> selCon;
  result = mXMLTerminal->GetSelectionController(getter_AddRefs(selCon));
  if (NS_FAILED(result) || !selCon)
      return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelection> selection;

  result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                    getter_AddRefs(selection));
  if (NS_FAILED(result) || !selection)
    return NS_ERROR_FAILURE;

#ifdef NO_WORKAROUND
  // Collapse selection to new cursor location
  result = selection->Collapse(mInputTextNode, cursorCol);
#else
  // WORKAROUND for cursor positioning at end of prompt
  // Without this workaround, the cursor is positioned too close to the prompt
  // (i.e., too far to the left, ignoring the prompt whitespace)

  if ((cursorCol > 0) || !mPromptHTML.IsEmpty()) {
    // Collapse selection to new cursor location
    result = selection->Collapse(mInputTextNode, cursorCol);

  } else {
    // Get the last bit of text in the prompt
    nsCOMPtr<nsIDOMText> domText (do_QueryInterface(mPromptTextNode));

    if (domText) {
      PRUint32 textLength;
      result = domText->GetLength(&textLength);
      if (NS_SUCCEEDED(result)) {
        XMLT_LOG(mozXMLTermSession::DisplayInput,72,
                 ("textLength=%d\n", textLength));
        result = selection->Collapse(mPromptTextNode, textLength);
      }
    }
  }
#endif // !NO_WORKAROUND

  NS_ASSERTION((NS_SUCCEEDED(result)),
                 "selection could not be collapsed after insert.");

  return NS_OK;
}


/** Autodetects markup in current output line
 * @param aString string to be displayed
 * @param firstOutputLine PR_TRUE if this is the first output line
 * @param secure PR_TRUE if output data is secure
 *               (usually PR_TRUE for metacommand output only)
 */
NS_IMETHODIMP mozXMLTermSession::AutoDetectMarkup(const nsString& aString,
                                                  PRBool firstOutputLine,
                                                  PRBool secure)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::AutoDetectMarkup,70,("firstOutputLine=0x%x\n",
                                                   firstOutputLine));

  // If autodetect disabled or not plain text, do nothing
  if ((mAutoDetect == NO_MARKUP) ||
      ((mAutoDetect == FIRST_LINE) && !firstOutputLine) ||
      (mOutputMarkupType != PLAIN_TEXT))
    return NS_OK;

  OutputMarkupType newMarkupType = PLAIN_TEXT;

  // Copy string and trim leading spaces/backspaces/tabs
  nsAutoString str(aString);
  
  str.Trim(kWhitespace, PR_TRUE, PR_FALSE);

  if (str.First() == U_LESSTHAN) {
    // Markup tag detected
    str.CompressWhitespace();
    str.AppendLiteral(" ");

    if ( (str.Find("<!DOCTYPE HTML",PR_TRUE) == 0) ||
         (str.Find("<BASE ",PR_TRUE) == 0) ||
         (str.Find("<HTML>",PR_TRUE) == 0) ) {
      // HTML document
      newMarkupType = HTML_DOCUMENT;

    } else if (str.Find("<?xml ",PR_FALSE) == 0) {
      // XML document
      newMarkupType = XML_DOCUMENT;

    } else {
      // HTML fragment
      if (secure) {
        // Secure HTML fragment
        newMarkupType = HTML_FRAGMENT;
      } else {
        // Insecure; treat as text fragment for security reasons
        newMarkupType = TEXT_FRAGMENT;
      }
    }


  } else if (firstOutputLine && str.Find("Content-Type",PR_TRUE) == 0) {
    // Possible MIME content type header
    str.StripWhitespace();
    if (str.Find("Content-Type:text/html",PR_TRUE) == 0) {
      // MIME content type header for HTML document
      newMarkupType = HTML_DOCUMENT;
    }
  }

  if (newMarkupType != PLAIN_TEXT) {
    // Markup found; initialize (insecure) stream
    nsAutoString streamURL(NS_LITERAL_STRING("http://in.sec.ure"));
    result = InitStream(streamURL, newMarkupType, PR_FALSE);
    if (NS_FAILED(result))
      return result;

  } else {
    // No markup found; assume rest of output is plain text
    mOutputMarkupType = PLAIN_TEXT;
  }

  XMLT_LOG(mozXMLTermSession::AutoDetectMarkup,71,("mOutputMarkupType=%d\n",
                                                   mOutputMarkupType));

  return NS_OK;
}


/** Initializes display of stream output with specified markup type
 * @param streamURL effective URL of stream output
 * @param streamMarkupType stream markup type
 * @param streamIsSecure PR_TRUE if stream is secure
 */
NS_IMETHODIMP mozXMLTermSession::InitStream(const nsString& streamURL,
                                            OutputMarkupType streamMarkupType,
                                            PRBool streamIsSecure)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::InitStream,70,("streamMarkupType=%d\n",
                                             streamMarkupType));

  // Break previous output display
  result = BreakOutput(PR_FALSE);
  if (NS_FAILED(result))
    return result;

  if ((streamMarkupType == TEXT_FRAGMENT)     ||
      (streamMarkupType == JS_FRAGMENT)       ||
      (streamMarkupType == HTML_FRAGMENT)     ||
      (streamMarkupType == INSECURE_FRAGMENT) ||
      (streamMarkupType == OVERFLOW_FRAGMENT) ||
      (streamMarkupType == INCOMPLETE_FRAGMENT)) {
    // Initialize fragment buffer
    mFragmentBuffer.SetLength(0);

  } else {
    // Create IFRAME to display stream document
    nsAutoString src(NS_LITERAL_STRING("about:blank"));
    nsAutoString width(NS_LITERAL_STRING("100%"));
    nsAutoString height(NS_LITERAL_STRING("10"));
    PRInt32 frameBorder = 0;

    if (!streamIsSecure)
      frameBorder = 2;

    result = NewIFrame(mOutputBlockNode, mCurrentEntryNumber,
                       frameBorder, src, width, height);

    if (NS_FAILED(result))
      return result;

    mXMLTermStream = do_CreateInstance( MOZXMLTERMSTREAM_CONTRACTID,
                                        &result);
    if (NS_FAILED(result))
      return result;


    nsCOMPtr<nsIDocShell> docShell;
    result = mXMLTerminal->GetDocShell(getter_AddRefs(docShell));
    if (NS_FAILED(result) || !docShell)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMWindowInternal> outerDOMWindow;
    result = mozXMLTermUtils::ConvertDocShellToDOMWindow(docShell,
                                              getter_AddRefs(outerDOMWindow));

    if (NS_FAILED(result) || !outerDOMWindow) {
      XMLT_ERROR("mozXMLTermSession::InitStream: Failed to convert webshell\n");
      return NS_ERROR_FAILURE;
    }

    // Initialize markup handling
    nsCAutoString iframeName("iframe");
#if 0
    iframeName.Append("t");
#else
    iframeName.AppendInt(mCurrentEntryNumber,10);
#endif

    nsCAutoString contentType;
    switch (streamMarkupType) {

    case HTML_DOCUMENT:
      contentType = "text/html";
      break;

    case XML_DOCUMENT:
      contentType = "text/xml";
      break;

    default:
      PR_ASSERT(0);
      break;
    }

    NS_ConvertUTF16toUTF8 url(streamURL);
    result = mXMLTermStream->Open(outerDOMWindow, iframeName.get(),
                                  url.get(),
                                  contentType.get(), 800);
    if (NS_FAILED(result)) {
      XMLT_ERROR("mozXMLTermSession::InitStream: Failed to open stream\n");
      return result;
    }

  }

  mOutputMarkupType = streamMarkupType;

  return NS_OK;
}


/** Breaks output display by flushing and deleting incomplete lines */
NS_IMETHODIMP mozXMLTermSession::BreakOutput(PRBool positionCursorBelow)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::BreakOutput,70,
           ("positionCursorBelow=%x, mOutputMarkupType=%d\n",
             positionCursorBelow, mOutputMarkupType));

  if (!mEntryHasOutput)
    return NS_OK;

  nsCOMPtr<nsIDOMDocument> domDoc;
  result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(result) || !domDoc)
    return NS_ERROR_FAILURE;

  switch (mOutputMarkupType) {

  case INSECURE_FRAGMENT:
  case OVERFLOW_FRAGMENT:
  case INCOMPLETE_FRAGMENT:
  case TEXT_FRAGMENT:
    {
      // Display text fragment using new SPAN node
      nsCOMPtr<nsIDOMNode> spanNode, textNode;
      nsAutoString tagName(NS_LITERAL_STRING("span"));
      nsAutoString elementName(NS_LITERAL_STRING("stream"));
      result = NewElementWithText(tagName, elementName, -1,
                                  mOutputBlockNode, spanNode, textNode);

      if (NS_FAILED(result) || !spanNode || !textNode)
        return NS_ERROR_FAILURE;

      // Append node
      nsCOMPtr<nsIDOMNode> resultNode;
      result = mOutputBlockNode->AppendChild(spanNode,
                                             getter_AddRefs(resultNode));

      // Handle stream output error messages
      switch (mOutputMarkupType) {
      case INSECURE_FRAGMENT:
        mFragmentBuffer.AssignLiteral("XMLTerm: *Error* Insecure stream data; is LTERM_COOKIE set?");
        break;

      case INCOMPLETE_FRAGMENT:
        mFragmentBuffer.AssignLiteral("XMLTerm: *Error* Incomplete stream data");
        break;

      default:
        break;
      }

      // Display text
      result = SetDOMText(textNode, mFragmentBuffer);
      if (NS_FAILED(result))
        return result;

      mFragmentBuffer.SetLength(0);
      break;
    }

  case JS_FRAGMENT:
    {
      // Execute JS fragment
      nsAutoString jsOutput;
      jsOutput.SetLength(0);
      result = mozXMLTermUtils::ExecuteScript(domDoc,
                                              mFragmentBuffer,
                                              jsOutput);
      if (NS_FAILED(result))
        jsOutput.AssignLiteral("Error in JavaScript execution\n");

      mFragmentBuffer.SetLength(0);

      if (!jsOutput.IsEmpty()) {
        // Display JS output as HTML fragment
        result = InsertFragment(jsOutput, mOutputBlockNode,
                                mCurrentEntryNumber);
        if (NS_FAILED(result))
          return result;
      }
    }

    break;

  case HTML_FRAGMENT:
    // Display HTML fragment
    result = InsertFragment(mFragmentBuffer, mOutputBlockNode,
                            mCurrentEntryNumber);
    if (NS_FAILED(result))
      return result;

    mFragmentBuffer.SetLength(0);
    break;

  case HTML_DOCUMENT:
  case XML_DOCUMENT:
    // Close HTML/XML document
    result = mXMLTermStream->Close();
    if (NS_FAILED(result)) {
      XMLT_ERROR("mozXMLTermSession::BreakOutput: Failed to close stream\n");
      return result;
    }
    mXMLTermStream = nsnull;
    break;

  default:
    // Flush plain text output, clearing any incomplete input line
    result = FlushOutput(CLEAR_INCOMPLETE_FLUSH);
    if (NS_FAILED(result))
      return result;

    mPreTextBufferLines = 0;
    mPreTextBuffered.SetLength(0);
    mPreTextDisplayed.SetLength(0);
    mOutputDisplayNode = nsnull;
    mOutputDisplayType = NO_NODE;
    mOutputTextNode = nsnull;
    break;
  }

  // Revert to plain text type
  mOutputMarkupType = PLAIN_TEXT;

  if (positionCursorBelow) {
    PositionOutputCursor(nsnull);
  }

  return NS_OK;
}


/** Processes output string with specified style
 * @param aString string to be processed
 * @param aStyle style values for string (see lineterm.h)
 *               (if it is a null string, STDOUT style is assumed)
 * @param newline PR_TRUE if this is a complete line of output
 * @param streamOutput PR_TRUE if string represents stream output
 */
NS_IMETHODIMP mozXMLTermSession::ProcessOutput(const nsString& aString,
                                               const nsString& aStyle,
                                               PRBool newline,
                                               PRBool streamOutput)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::ProcessOutput,70,
           ("newline=%d, streamOutput=%d\n", newline, streamOutput));

  if ((mMetaCommandType == LS_META_COMMAND) && newline) {
    // Display hypertext directory listing
    result = AppendLineLS(aString, aStyle);
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

    return NS_OK;

  } else {
    // Not LS meta command

    switch (mOutputMarkupType) {

    case INSECURE_FRAGMENT:
    case OVERFLOW_FRAGMENT:
    case INCOMPLETE_FRAGMENT:
      // Do nothing
      break;

    case TEXT_FRAGMENT:
    case JS_FRAGMENT:
    case HTML_FRAGMENT:
      // Append complete lines to fragment buffer
      if (newline || streamOutput) {
        PRInt32 strLen = mFragmentBuffer.Length()+aString.Length();

        if (strLen < 100000) {
          mFragmentBuffer += aString;
          if (newline)
            mFragmentBuffer += PRUnichar('\n');

        } else {
          mOutputMarkupType = OVERFLOW_FRAGMENT;
          mFragmentBuffer.AssignLiteral("XMLTerm: *Error* Stream data overflow (");
          mFragmentBuffer.AppendInt(strLen,10);
          mFragmentBuffer.Append(NS_LITERAL_STRING(" chars)"));
        break;

        }
      }

      break;

    case HTML_DOCUMENT:
    case XML_DOCUMENT:
      // Write complete lines to document stream

      if (newline || streamOutput) {
        nsAutoString str(aString);
        if (newline)
          str.AppendLiteral("\n");

        result = mXMLTermStream->Write(str.get());
        if (NS_FAILED(result)) {
          XMLT_ERROR("mozXMLTermSession::ProcessOutput: Failed to write to stream\n");
          return result;
        }
      }
      break;

    default:
      // Display plain text output, complete or incomplete lines
      PR_ASSERT(!streamOutput);
      result = AppendOutput(aString, aStyle, newline);
      if (NS_FAILED(result))
        return NS_ERROR_FAILURE;
      break;
    }

    return NS_OK;
  }
}


/** Ensures the total number of output lines stays within a limit
 * by deleting the oldest output line.
 * @param deleteAllOld if PR_TRUE, delete all previous display nodes
 *                     (excluding the current one)
 */
NS_IMETHODIMP mozXMLTermSession::LimitOutputLines(PRBool deleteAllOld)
{
  nsresult result;
  nsAutoString attValue;

  XMLT_LOG(mozXMLTermSession::LimitOutputLines,70,
           ("deleteAllOld=%d, mEntryOutputLines=%d\n",
            deleteAllOld, mEntryOutputLines));

  nsCOMPtr<nsIDOMNode> firstChild;
  result = mOutputBlockNode->GetFirstChild(getter_AddRefs(firstChild));
  if (NS_FAILED(result) || !firstChild)
    return NS_ERROR_FAILURE;

  attValue.SetLength(0);
  result = mozXMLTermUtils::GetNodeAttribute(firstChild, "class", attValue);
  if (NS_FAILED(result))
    return result;

  if (!attValue.EqualsASCII(sessionElementNames[WARNING_ELEMENT])) {
    // Create warning message element
    nsCOMPtr<nsIDOMNode> divNode, textNode;
    nsAutoString tagName(NS_LITERAL_STRING("div"));
    nsAutoString elementName; elementName.AssignASCII(sessionElementNames[WARNING_ELEMENT]);
    result = NewElementWithText(tagName, elementName, -1,
                                mOutputBlockNode, divNode, textNode,
                                firstChild);
    if (NS_FAILED(result) || !divNode || !textNode)
      return NS_ERROR_FAILURE;

    firstChild = divNode;

    nsAutoString warningMsg;
    warningMsg.AssignLiteral("XMLTerm: *WARNING* Command output truncated to ");
    warningMsg.AppendInt(300,10);
    warningMsg.AppendLiteral(" lines");
    result = SetDOMText(textNode, warningMsg);
  }

  PR_ASSERT(mOutputDisplayNode != firstChild);

  nsCOMPtr<nsIDOMNode> nextChild;

  PRInt32 decrementedLineCount = 0;

  for (;;) {
    result = firstChild->GetNextSibling(getter_AddRefs(nextChild));
    PR_ASSERT(NS_SUCCEEDED(result) && nextChild);

    // Do not modify current display node
    if (nextChild.get() == mOutputDisplayNode.get())
      break;

    PRInt32 deleteNode = 0;

    if (deleteAllOld) {
      deleteNode = 1;

    } else {
      attValue.SetLength(0);
      result = mozXMLTermUtils::GetNodeAttribute(nextChild, "class", attValue);

      if (NS_FAILED(result) || attValue.IsEmpty()) {
        deleteNode = 1;

      } else {

        if (attValue.EqualsASCII(sessionElementNames[MIXED_ELEMENT])) {
          // Delete single line containing mixed style output
          deleteNode = 1;
          decrementedLineCount = 1;

          XMLT_LOG(mozXMLTermSession::LimitOutputLines,79,
                   ("deleted mixed line\n"));

        } else if ( (attValue.EqualsASCII(sessionElementNames[STDIN_ELEMENT]))  ||
                    (attValue.EqualsASCII(sessionElementNames[STDOUT_ELEMENT])) ||
                    (attValue.EqualsASCII(sessionElementNames[STDERR_ELEMENT]))) {
          // Delete first line from STDIN/STDOUT/STDERR PRE output

          nsCOMPtr<nsIDOMNode> textNode;
          result = nextChild->GetFirstChild(getter_AddRefs(textNode));
          PR_ASSERT( NS_SUCCEEDED(result) && textNode);

          nsCOMPtr<nsIDOMText> domText (do_QueryInterface(textNode));
          PR_ASSERT(domText);

          // Delete first line from text
          nsAutoString text;
          domText->GetData(text);

          PRInt32 offset = text.FindChar((PRUnichar) U_LINEFEED);

          if (offset < 0) {
            deleteNode = 1;
          } else {
            text.Cut(0,offset+1);
            domText->SetData(text);
          }
          decrementedLineCount = 1;

          XMLT_LOG(mozXMLTermSession::LimitOutputLines,79,
                   ("deleted PRE line\n"));

        } else {
          // Unknown type of DOM element, delete
          deleteNode = 1;
        }
      }
    }

    if (deleteNode) {
      // Delete next child node
      nsCOMPtr<nsIDOMNode> resultNode;
      result = mOutputBlockNode->RemoveChild(nextChild,
                                             getter_AddRefs(resultNode));
      if (NS_FAILED(result))
        return result;
    }

    if (decrementedLineCount || !deleteNode)
      break;
  }

  if (deleteAllOld) {
    mEntryOutputLines = 0;
    return NS_OK;

  } else if (decrementedLineCount) {
    mEntryOutputLines--;
    return NS_OK;

  } else {
    return NS_ERROR_FAILURE;
  }
}


/** Appends text string to output buffer
 *  (appended text may need to be flushed for it to be actually displayed)
 * @param aString string to be processed (may be null string, for dummy line)
 * @param aStyle style values for string (see lineterm.h)
 *               (may be a single Unichar, for uniform style)
 *               (if it is a null string, STDOUT style is assumed)
 * @param newline PR_TRUE if this is a complete line of output
 */
NS_IMETHODIMP mozXMLTermSession::AppendOutput(const nsString& aString,
                                              const nsString& aStyle,
                                              PRBool newline)
{
  nsresult result;

  const PRInt32   strLength   = aString.Length();
  const PRInt32   styleLength = aStyle.Length();
  const PRUnichar *strStyle   = aStyle.get();

  XMLT_LOG(mozXMLTermSession::AppendOutput,70,("strLength=%d\n", strLength));

  // Check if line has uniform style
  PRUnichar uniformStyle = LTERM_STDOUT_STYLE;
  PRInt32 styleChanges = 0;

  if (styleLength > 0) {
    PRInt32 j;
    uniformStyle = strStyle[0];

    PR_ASSERT((styleLength == 1) || (styleLength == strLength));
    for (j=1; j<styleLength; j++) {
      if (strStyle[j] != strStyle[j-1]) {
        uniformStyle = 0;
        styleChanges++;
      }
    }
  }

  XMLT_LOG(mozXMLTermSession::AppendOutput,72,
           ("mOutputDisplayType=%d, uniformStyle=0x%x, newline=%d\n",
            mOutputDisplayType, uniformStyle, newline));

  char* temCString = ToNewCString(aString);
  XMLT_LOG(mozXMLTermSession::AppendOutput,72,
           ("aString=%s\n", temCString));
  nsCRT::free(temCString);

#ifdef NO_WORKAROUND
  // Do not use PRE text
  if (0) {
#else
  if (uniformStyle != 0) {
#endif
    // Uniform style data; display as preformatted block
    OutputDisplayType preDisplayType;
    nsAutoString elementName;
    elementName.SetLength(0);

    if (uniformStyle == LTERM_STDIN_STYLE) {
      preDisplayType = PRE_STDIN_NODE;
      elementName.AssignASCII(sessionElementNames[STDIN_ELEMENT]);
      XMLT_LOG(mozXMLTermSession::AppendOutput,72, ("PRE_STDIN_NODE\n"));

    } else if (uniformStyle == LTERM_STDERR_STYLE) {
      preDisplayType = PRE_STDERR_NODE;
      elementName.AssignASCII(sessionElementNames[STDERR_ELEMENT]);
      XMLT_LOG(mozXMLTermSession::AppendOutput,72, ("PRE_STDERR_NODE\n"));

    } else {
      preDisplayType = PRE_STDOUT_NODE;
      elementName.AssignASCII(sessionElementNames[STDOUT_ELEMENT]);
      XMLT_LOG(mozXMLTermSession::AppendOutput,72, ("PRE_STDOUT_NODE\n"));
    }

    if (mOutputDisplayType != preDisplayType) {
      // Flush incomplete line
      result = FlushOutput(CLEAR_INCOMPLETE_FLUSH);

      // Create PRE display node
      nsCOMPtr<nsIDOMNode> preNode, textNode;
      nsAutoString tagName(NS_LITERAL_STRING("pre"));

      result = NewElementWithText(tagName, elementName, -1,
                                  mOutputBlockNode, preNode, textNode);

      if (NS_FAILED(result) || !preNode || !textNode)
        return NS_ERROR_FAILURE;

      XMLT_LOG(mozXMLTermSession::AppendOutput,72,
               ("Creating new PRE node\n"));

      // Append node
      nsCOMPtr<nsIDOMNode> resultNode;
      result = mOutputBlockNode->AppendChild(preNode,
                                             getter_AddRefs(resultNode));

      mOutputDisplayType = preDisplayType;
      mOutputDisplayNode = preNode;
      mOutputTextNode = textNode;
      mOutputTextOffset = 0;

      // If string terminates in whitespace, append NBSP for cursor positioning
      nsAutoString tempString( aString );
      if (newline || (aString.Last() == PRUnichar(' ')))
        tempString += kNBSP;

      // Display incomplete line
      result = SetDOMText(mOutputTextNode, tempString);
      if (NS_FAILED(result))
        return NS_ERROR_FAILURE;

      // Initialize PRE text string buffers
      mPreTextDisplayed = aString;
      mPreTextBuffered.SetLength(0);
      mPreTextBufferLines = 0;
    }

    // Save incomplete line
    mPreTextIncomplete = aString;

    if (newline) {
      // Complete line; append to buffer
      if (mPreTextBufferLines > 0) {
        mPreTextBuffered += PRUnichar('\n');
      }
      mPreTextBufferLines++;
      mPreTextBuffered += mPreTextIncomplete;
      mPreTextIncomplete.SetLength(0);

      if (mPreTextBufferLines > 300) {
        // Delete all earlier PRE/mixed blocks and first line of current block

        result = LimitOutputLines(PR_TRUE);
        if (NS_FAILED(result))
          return result;

        // Delete first line from PRE text buffer
        PRInt32 offset = mPreTextBuffered.FindChar((PRUnichar) U_LINEFEED);
        if (offset < 0) {
          mPreTextBuffered.SetLength(0);
        } else {
          mPreTextBuffered.Cut(0,offset+1);
        }

        mPreTextBufferLines--;

      } else if (mEntryOutputLines+mPreTextBufferLines > 300) {
        // Delete oldest PRE/mixed line so as to stay within the limit

        result = LimitOutputLines(PR_FALSE);
        if (NS_FAILED(result))
          return result;
      }
    }

    XMLT_LOG(mozXMLTermSession::AppendOutput,72,
             ("mPreTextDisplayed.Length()=%d, mPreTextBufferLines()=%d\n",
              mPreTextDisplayed.Length(), mPreTextBufferLines));

  } else {
    // Create uniform style DIV display node

    XMLT_LOG(mozXMLTermSession::AppendOutput,72,("DIV_MIXED_NODE\n"));

    // Flush buffer, clearing incomplete line
    result = FlushOutput(CLEAR_INCOMPLETE_FLUSH);
    if (NS_FAILED(result))
      return result;

    // Create new DIV node
    nsAutoString elementName; elementName.AssignASCII(sessionElementNames[MIXED_ELEMENT]);
    nsCOMPtr<nsIDOMNode> divNode;
    nsAutoString tagName(NS_LITERAL_STRING("div"));
    result = NewElement(tagName, elementName, -1,
                        mOutputBlockNode, divNode);

    if (NS_FAILED(result) || !divNode)
      return NS_ERROR_FAILURE;

    // Append node
    nsCOMPtr<nsIDOMNode> resultNode;
    result = mOutputBlockNode->AppendChild(divNode,
                                                getter_AddRefs(resultNode));
    if (NS_FAILED(result))
      return result;

    nsCOMPtr<nsIDOMNode> spanNode, textNode;
    nsAutoString subString;
    PRInt32 k;
    PRInt32 passwordPrompt = 0;
    PRUnichar currentStyle = LTERM_STDOUT_STYLE;
    if (styleLength > 0) 
      currentStyle = strStyle[0];

    mOutputTextOffset = 0;
    tagName.AssignLiteral("pre");

    PR_ASSERT(strLength > 0);

    for (k=1; k<strLength+1; k++) {
      if ((k == strLength) || ((k < styleLength) &&
                               (strStyle[k] != currentStyle)) ) {
        // Change of style or end of string
        switch (currentStyle) {
        case LTERM_STDIN_STYLE:
          elementName.AssignASCII(sessionElementNames[STDIN_ELEMENT]);
          break;
        case LTERM_STDERR_STYLE:
          elementName.AssignASCII(sessionElementNames[STDERR_ELEMENT]);
          break;
        default:
          elementName.AssignASCII(sessionElementNames[STDOUT_ELEMENT]);
          break;
        }

        result = NewElementWithText(tagName, elementName, -1,
                                    divNode, spanNode, textNode);

        if (NS_FAILED(result) || !spanNode || !textNode)
          return NS_ERROR_FAILURE;

        aString.Mid(subString, mOutputTextOffset, k-mOutputTextOffset);
        result = SetDOMText(textNode, subString);
        if (NS_FAILED(result))
          return result;

        if (k < styleLength) {
          // Change style
          PRInt32 strLen = subString.Length();
          if ((styleChanges == 1) &&
              (currentStyle == LTERM_STDOUT_STYLE)         &&
              (strStyle[k] == LTERM_STDIN_STYLE)           &&
              ( ((strLen-10) == subString.RFind("password: ",PR_TRUE)) ||
                ((strLen-9) == subString.RFind("password:",PR_TRUE))) ) {
            // Password prompt detected; break loop
            passwordPrompt = 1;
            break;
          }

          currentStyle = strStyle[k];
          mOutputTextOffset = k;
        }
      }
    }

    mOutputDisplayType = DIV_MIXED_NODE;
    mOutputDisplayNode = divNode;
    mOutputTextNode = textNode;

    if (newline) {
      // Increment total output line count for entry
      mEntryOutputLines++;

      if (mEntryOutputLines > 300) {
        // Delete oldest PRE/mixed line so as to stay within the limit
        result = LimitOutputLines(PR_FALSE);
        if (NS_FAILED(result))
          return result;
      }

      if (passwordPrompt) {
        result = mOutputBlockNode->RemoveChild(mOutputDisplayNode,
                                               getter_AddRefs(resultNode));
      }
      mOutputDisplayType = NO_NODE;
      mOutputDisplayNode = nsnull;
      mOutputTextNode = nsnull;

    }
  }

  return NS_OK;
}


/** Adds markup to LS output (TEMPORARY)
 * @param aString string to be processed
 * @param aStyle style values for string (see lineterm.h)
 *               (if it is a null string, STDOUT style is assumed)
 */
NS_IMETHODIMP mozXMLTermSession::AppendLineLS(const nsString& aString,
                                              const nsString& aStyle)
{
  nsresult result;

  const PRInt32   strLength   = aString.Length();
  const PRInt32   styleLength = aStyle.Length();
  const PRUnichar *strStyle   = aStyle.get();

  // Check if line has uniform style
  PRUnichar allStyles = LTERM_STDOUT_STYLE;
  PRUnichar uniformStyle = LTERM_STDOUT_STYLE;

  if (styleLength > 0) {
    PRInt32 j;
    allStyles = strStyle[0];
    uniformStyle = strStyle[0];

    for (j=1; j<strLength; j++) {
      allStyles |= strStyle[j];
      if (strStyle[j] != strStyle[0]) {
        uniformStyle = 0;
      }
    }
  }

  XMLT_LOG(mozXMLTermSession::AppendLineLS,60,
           ("mOutputDisplayType=%d, uniformStyle=0x%x\n",
            mOutputDisplayType, uniformStyle));

  if (uniformStyle != LTERM_STDOUT_STYLE) {
    return AppendOutput(aString, aStyle, PR_TRUE);
  }

  char* temCString = ToNewCString(aString);
  XMLT_LOG(mozXMLTermSession::AppendLineLS,62,("aString=%s\n", temCString));
  nsCRT::free(temCString);

  // Add markup to directory listing
  nsAutoString markupString;
  PRInt32 lineLength = aString.Length();
  PRInt32 wordBegin = 0;
  markupString.SetLength(0);

  while (wordBegin < lineLength) {
    // Consume any leading spaces
    while ( (wordBegin < lineLength) &&
            ((aString[wordBegin] == U_SPACE) ||
             (aString[wordBegin] == U_TAB)) ) {
      markupString += aString[wordBegin];
      wordBegin++;
    }
    if (wordBegin >= lineLength) break;

    // Locate end of word (non-space character)
    PRInt32 wordEnd = aString.FindCharInSet(kWhitespace, wordBegin);
    if (wordEnd < 0) {
      wordEnd = lineLength-1;
    } else {
      wordEnd--;
    }

    PR_ASSERT(wordEnd >= wordBegin);

    // Locate pure filename, with possible type suffix
    PRInt32 nameBegin;
    if (wordEnd > wordBegin) {
      nameBegin = aString.RFindChar(U_SLASH, wordEnd-1);
      if (nameBegin >= wordBegin) {
        nameBegin++;
      } else {
        nameBegin = wordBegin;
      }
    } else {
      nameBegin = wordBegin;
    }

    nsAutoString filename;
    aString.Mid(filename, nameBegin, wordEnd-nameBegin+1);

    FileType fileType = PLAIN_FILE;
    PRUint32 dropSuffix = 0;

    if (wordEnd > wordBegin) {
      // Determine file type from suffix character
      switch (aString[wordEnd]) {
      case U_SLASH:
        fileType = DIRECTORY_FILE;
        break;
      case U_STAR:
        fileType = EXECUTABLE_FILE;
        break;
      default:
        break;
      }

      // Discard any type suffix
      if (fileType != PLAIN_FILE)
        dropSuffix = 1;
    }

    // Extract full pathname (minus any type suffix)
    nsAutoString pathname;
    aString.Mid(pathname, wordBegin, wordEnd-wordBegin+1-dropSuffix);

    // Append to markup string
    markupString.AssignLiteral("<span class=\"");
    markupString.AssignASCII(fileTypeNames[fileType]);
    markupString.AssignLiteral("\"");

    int j;
    for (j=0; j<SESSION_EVENT_TYPES; j++) {
      markupString.AssignLiteral(" on");
      markupString.AssignASCII(sessionEventNames[j]);
      markupString.AssignLiteral("=\"return HandleEvent(event, '");
      markupString.AssignASCII(sessionEventNames[j]);
      markupString.AssignLiteral("','");
      markupString.AssignASCII(fileTypeNames[fileType]);
      markupString.AssignLiteral("',-#,'");
      markupString.Assign(pathname);
      markupString.Assign(NS_LITERAL_STRING("');\""));
    }

    markupString.AssignLiteral(">");
    markupString.Assign(filename);
    markupString.AssignLiteral("</span>");

    // Search for new word
    wordBegin = wordEnd+1;
  }

  if (mOutputDisplayType != PRE_STDOUT_NODE) {
    // Create PRE block
    nsAutoString nullString; nullString.SetLength(0);
    result = AppendOutput(nullString, nullString, PR_FALSE);
  }

  PR_ASSERT(mOutputDisplayNode != nsnull);
  PR_ASSERT(mOutputTextNode != nsnull);

  result = InsertFragment(markupString, mOutputDisplayNode,
                          mCurrentEntryNumber, mOutputTextNode.get());

  nsCOMPtr<nsIDOMDocument> domDoc;
  result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(result) || !domDoc)
    return NS_ERROR_FAILURE;

  // Insert text node containing newline only
  nsCOMPtr<nsIDOMText> newText;
  nsAutoString newlineStr(NS_LITERAL_STRING("\n"));

  result = domDoc->CreateTextNode(newlineStr, getter_AddRefs(newText));
  if (NS_FAILED(result) || !newText)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> newTextNode = do_QueryInterface(newText);
  nsCOMPtr<nsIDOMNode> resultNode;
  result = mOutputDisplayNode->InsertBefore(newTextNode, mOutputTextNode,
                                            getter_AddRefs(resultNode));
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  XMLT_LOG(mozXMLTermSession::AppendLineLS,61,("exiting\n"));

#if 0
  mCurrentDebugNode = mOutputDisplayNode;
  mMetaCommandType = TREE_META_COMMAND;
  XMLT_LOG(mozXMLTermSession::AppendLineLS,62,("tree:\n"));
#endif  /* 0 */

  return NS_OK;
}


/** Inserts HTML fragment string as child of parentNode, before specified
 * child node, or after the last child node
 * @param aString HTML fragment string to be inserted
 * @param parentNode parent node for HTML fragment
 * @param entryNumber entry number (default value = -1)
 *                   (if entryNumber >= 0, all '#' characters in
 *                    id/onclick attribute values are substituted
 *                    with entryNumber)
 * @param beforeNode child node before which to insert fragment;
 *                   if null, insert after last child node
 *                   (default value is null)
 * @param replace if PR_TRUE, replace beforeNode with inserted fragment
 *                (default value is PR_FALSE)
 */
 NS_IMETHODIMP mozXMLTermSession::InsertFragment(const nsString& aString,
                                              nsIDOMNode* parentNode,
                                              PRInt32 entryNumber,
                                              nsIDOMNode* beforeNode,
                                              PRBool replace)
{
  nsresult result;

  char* temCString = ToNewCString(aString);
  XMLT_LOG(mozXMLTermSession::InsertFragment,70,("aString=%s\n", temCString));
  nsCRT::free(temCString);

  // Get selection
  nsCOMPtr<nsISelection> selection;

  nsCOMPtr<nsISelectionController> selCon;
  result = mXMLTerminal->GetSelectionController(getter_AddRefs(selCon));
  if (NS_FAILED(result) || !selCon)
    return NS_ERROR_FAILURE;

  result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                    getter_AddRefs(selection));
  if (NS_FAILED(result) || !selection)
    return NS_ERROR_FAILURE;

  PRUint32 insertOffset = 0;

  nsCOMPtr<nsIDOMNodeList> childNodes;
  result = parentNode->GetChildNodes(getter_AddRefs(childNodes));

  if (NS_SUCCEEDED(result) && childNodes) {
    PRUint32 nChildren = 0;
    childNodes->GetLength(&nChildren);

    if(!beforeNode) {
      // Append child
      insertOffset = nChildren;

    } else {
      // Determine offset of before node
      int j;
      PRInt32 nNodes = nChildren;

      for (j=0; j<nNodes; j++) {
        nsCOMPtr<nsIDOMNode> childNode;
        result = childNodes->Item(j, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(result)) && childNode) {
          if (childNode.get() == beforeNode) {
            insertOffset = j;
            break;
          }
        }
      }
    }
  }

  // Collapse selection to insertion point
  result = selection->Collapse(parentNode, insertOffset);
  if (NS_FAILED(result))
    return result;

  // Get the first range in the selection
  nsCOMPtr<nsIDOMRange> firstRange;
  result = selection->GetRangeAt(0, getter_AddRefs(firstRange));
  if (NS_FAILED(result) || !firstRange)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(firstRange));
  if (!nsrange)
    return NS_ERROR_FAILURE;

  XMLT_LOG(mozXMLTermSession::InsertFragment,62,("Creating Fragment\n"));

  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  result = nsrange->CreateContextualFragment(aString, getter_AddRefs(docfrag));
  if (NS_FAILED(result) || !docfrag)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> docfragNode (do_QueryInterface(docfrag));
  if (!docfragNode)
    return NS_ERROR_FAILURE;

  // Sanitize all nodes in document fragment (deep)
  result = DeepSanitizeFragment(docfragNode, nsnull, entryNumber);
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  // If fragment was deleted during the sanitization process, simply return
  if (!docfragNode)
    return NS_OK;

  // Insert child nodes of document fragment before PRE text node
  nsCOMPtr<nsIDOMNode> childNode;
  result = docfragNode->GetFirstChild(getter_AddRefs(childNode));
  if (NS_FAILED(result) || !childNode)
    return NS_ERROR_FAILURE;

  while (childNode) {
    // Get next sibling prior to insertion
    nsCOMPtr<nsIDOMNode> nextChild;
    result = childNode->GetNextSibling(getter_AddRefs(nextChild));

    XMLT_LOG(mozXMLTermSession::InsertFragment,72,("Inserting child node ...\n"));

    //  nsCOMPtr<nsIContent> childContent (do_QueryInterface(childNode));
    //    if (childContent) childContent->List(stderr);

    // Deep clone child node
    // Note: Not clear why this needs to be done, but like "deep refresh",
    //       seems to be essential for event handlers to work
    nsCOMPtr<nsIDOMNode> cloneNode;
    result = childNode->CloneNode(PR_TRUE, getter_AddRefs(cloneNode));
    if (NS_FAILED(result) || !cloneNode)
      return NS_ERROR_FAILURE;

    // Insert clone of child node
    nsCOMPtr<nsIDOMNode> resultNode;

    PRBool replaceTem = replace;
    if (beforeNode) {
      if (replaceTem) {
        // Replace before node
        result = parentNode->ReplaceChild(cloneNode, beforeNode,
                                          getter_AddRefs(resultNode));

        beforeNode = nsnull;

        nsCOMPtr<nsIDOMNode> newBeforeNode;
        result = resultNode->GetNextSibling(getter_AddRefs(newBeforeNode));

        if (NS_SUCCEEDED(result) && newBeforeNode) {
          beforeNode = newBeforeNode.get();
          replaceTem = PR_FALSE;
        }

      } else {
        // Insert before specified node
        result = parentNode->InsertBefore(cloneNode, beforeNode,
                                          getter_AddRefs(resultNode));
      }
    } else {
      // Append child
      result = parentNode->AppendChild(cloneNode, getter_AddRefs(resultNode));
    }
    if (NS_FAILED(result))
      return result;

    // Refresh attributes of inserted child node (deep)
    DeepRefreshEventHandlers(resultNode);

    childNode = nextChild;
  }

  return NS_OK;

}


/** Substitute all occurrences of the '#' character in aString with
 * aNumber, if aNumber >= 0;
 * @ param aString string to be modified
 * @ param aNumber number to substituted
 */
void mozXMLTermSession::SubstituteCommandNumber(nsString& aString,
                                                PRInt32 aNumber)
{

  if (aNumber < 0)
    return;

  PRInt32 numberOffset;
  nsAutoString numberString;
  numberString.SetLength(0);

  numberString.AppendInt(aNumber,10);

  for (;;) {
    // Search for '#' character
    numberOffset = aString.FindChar((PRUnichar) '#');

    if (numberOffset < 0)
      break;

    // Substitute '#' with supplied number
    aString.Cut(numberOffset,1);
    aString.Insert(numberString, numberOffset);
  }
}


/** Sanitize event handler attribute values by imposing syntax checks.
 * @param aAttrValue attribute value to be sanitized
 * @param aEventName name of event being handled ("click", ...)
 */
void mozXMLTermSession::SanitizeAttribute(nsString& aAttrValue,
                                          const char* aEventName)
{
  // ****************NOTE***************
  // At the moment this method simply prevents the word function and the
  // the character '{' both occurring in the event handler attribute.
  // NEEDS TO BE IMPROVED TO ENFORCE STRICTER REQUIREMENTS
  // such as: the event handler attribute should always be of the form
  // "return EventHandler(str_arg1, num_arg2, str_arg3, str_arg4);"

  if ((aAttrValue.FindChar((PRUnichar)'{') >= 0) &&
      (aAttrValue.Find("function") >= 0)) {
    // Character '{' and string "function" both found in attribute value;
    // set to null string

    char* temCString = ToNewCString(aAttrValue);
    XMLT_WARNING("mozXMLTermSession::SanitizeAttribute: Warning - deleted attribute on%s='%s'\n", aEventName, temCString);
    nsCRT::free(temCString);

    aAttrValue.SetLength(0);
  }

  return;
}


/** Deep sanitizing of event handler attributes ("on*") prior to insertion
 * of HTML fragments, to enfore consistent UI behaviour in XMLTerm and
 * for security. The following actions are carried out:
 * 1. Any SCRIPT tags in the fragment are simply deleted
 * 2. All event handler attributes, except a few selected ones, are deleted.
 * 3. The retained event handler attribute values are subject to strict
 *    checks.
 * 4. If entryNumber >= 0, all '#' characters in the ID attribute and
 *     retained event handler attributes are substituted with entryNumber.
 *     
 * @param domNode DOM node for HTML fragment to be sanitized
 * @param parentNode parent DOM node (needed to delete SCRIPT elements;
 *                                    set to null if root element)
 * @param entryNumber entry number (default value = -1)
 */
NS_IMETHODIMP mozXMLTermSession::DeepSanitizeFragment(
                                  nsCOMPtr<nsIDOMNode>& domNode,
                                  nsIDOMNode* parentNode,
                                  PRInt32 entryNumber)
{
  nsresult result;
  PRInt32 j;

  XMLT_LOG(mozXMLTermSession::DeepSanitizeFragment,72,("entryNumber=%d\n",
                                                       entryNumber));

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(domNode);

  if (domElement) {
    // Check if this is a script element (IGNORE CASE)
    nsAutoString tagName;
    tagName.SetLength(0);
    result = domElement->GetTagName(tagName);

    if (NS_SUCCEEDED(result) && tagName.LowerCaseEqualsLiteral("script")) {
      // Remove script element and return

      XMLT_WARNING("mozXMLTermSession::DeepSanitizeFragment: Warning - rejected SCRIPT element in inserted HTML fragment\n");

      if (parentNode) {
        nsCOMPtr<nsIDOMNode> resultNode;
        result = parentNode->RemoveChild(domNode, getter_AddRefs(resultNode));
        if (NS_FAILED(result))
          return result;

      } else {
        domNode = nsnull;
      }

      return NS_OK;
    }

    nsAutoString eventAttrVals[SESSION_EVENT_TYPES];
    for (j=0; j<SESSION_EVENT_TYPES; j++)
      eventAttrVals[j].SetLength(0);

    nsAutoString attName, attValue;

    for (j=0; j<SESSION_EVENT_TYPES; j++) {
      attName.AssignLiteral("on");
      attName.AppendASCII(sessionEventNames[j]);

      attValue.SetLength(0);
      result = domElement->GetAttribute(attName, attValue);
      if (NS_SUCCEEDED(result) && !attValue.IsEmpty()) {
        // Save allowed event attribute value for re-insertion
        eventAttrVals[j] = attValue;
      }
    }

    nsCOMPtr<nsIDOMNamedNodeMap> namedNodeMap(nsnull);
    result = domNode->GetAttributes(getter_AddRefs(namedNodeMap));

    if (NS_SUCCEEDED(result) && namedNodeMap) {
      // Cycle through all attributes and delete all event attributes ("on*")
      PRUint32 nodeCount;
      result = namedNodeMap->GetLength(&nodeCount);

      if (NS_SUCCEEDED(result)) {
        nsCOMPtr<nsIDOMNode> attrNode;
        PRUint32 k;
        nsAutoString attrName, attrValue, prefix;
        nsAutoString nullStr; nullStr.SetLength(0);

        for (k=0; k<nodeCount; k++) {
          result = namedNodeMap->Item(k, getter_AddRefs(attrNode));

          if (NS_SUCCEEDED(result)) {
            nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(attrNode);

            if (attr) {
              result = attr->GetName(attrName);

              if (NS_SUCCEEDED(result)) {
                result = attr->GetValue(attrValue);
                if (NS_SUCCEEDED(result) && (attrName.Length() >= 2)) {

                  attrName.Left(prefix,2);

                  if (prefix.LowerCaseEqualsLiteral("on")) {
                    // Delete event handler attribute

                    XMLT_LOG(mozXMLTermSession::DeepSanitizeFragment,79,
                             ("Deleting event handler in fragment\n"));

                    result = domElement->SetAttribute(attrName, nullStr);
                    if (NS_FAILED(result))
                    return result;
                  }
                }

              }
            }

          }
        }

      }
    }

    if (entryNumber >= 0) {
      // Process ID attribute
      attName.AssignLiteral("id");

      attValue.SetLength(0);
      result = domElement->GetAttribute(attName, attValue);

      if (NS_SUCCEEDED(result) && !attValue.IsEmpty()) {
        // Modify attribute value
        SubstituteCommandNumber(attValue, entryNumber);
        domElement->SetAttribute(attName, attValue);
      }
    }

    for (j=0; j<SESSION_EVENT_TYPES; j++) {
      // Re-introduce sanitized event attribute values
      attName.AssignLiteral("on");
      attName.AppendASCII(sessionEventNames[j]);
      attValue = eventAttrVals[j];

      if (!attValue.IsEmpty()) {
        SubstituteCommandNumber(attValue, entryNumber);

        // Sanitize attribute value
        SanitizeAttribute(attValue, sessionEventNames[j]);

        // Insert attribute value
        domElement->SetAttribute(attName, attValue);
      }
    }

  }

  // Iterate over all child nodes for deep refresh
  nsCOMPtr<nsIDOMNode> child;
  result = domNode->GetFirstChild(getter_AddRefs(child));
  if (NS_FAILED(result))
    return NS_OK;

  while (child) {
    DeepSanitizeFragment(child, domNode, entryNumber);

    nsCOMPtr<nsIDOMNode> temp = child;
    result = temp->GetNextSibling(getter_AddRefs(child));
    if (NS_FAILED(result))
      break;
  }

  return NS_OK;
}


/** Deep refresh of selected event handler attributes for DOM elements
 * (WORKAROUND for inserting HTML fragments properly)
 * @param domNode DOM node of branch to be refreshed
 */
NS_IMETHODIMP mozXMLTermSession::DeepRefreshEventHandlers(
                                  nsCOMPtr<nsIDOMNode>& domNode)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::DeepRefreshEventHandlers,82,("\n"));

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(domNode);
  if (!domElement)
    return NS_OK;

  int j;
  nsAutoString attName, attValue;

  // Refresh event attributes
  for (j=0; j<SESSION_EVENT_TYPES; j++) {
    attName.AssignLiteral("on");
    attName.AppendASCII(sessionEventNames[j]);

    XMLT_LOG(mozXMLTermSession::DeepRefreshEventHandlers,89,
             ("Refreshing on%s attribute\n",sessionEventNames[j] ));

    attValue.SetLength(0);
    result = domElement->GetAttribute(attName, attValue);

    if (NS_SUCCEEDED(result) && !attValue.IsEmpty()) {
      // Refresh attribute value
      domElement->SetAttribute(attName, attValue);
    }
  }

  // Iterate over all child nodes for deep refresh
  nsCOMPtr<nsIDOMNode> child;
  result = domNode->GetFirstChild(getter_AddRefs(child));
  if (NS_FAILED(result))
    return NS_OK;

  while (child) {
    DeepRefreshEventHandlers(child);

    nsCOMPtr<nsIDOMNode> temp = child;
    result = temp->GetNextSibling(getter_AddRefs(child));
    if (NS_FAILED(result))
      break;
  }

  return NS_OK;
}


/** Forces display of data in output buffer
 * @param flushAction type of flush action: display, split-off, clear, or
 *                                          close incomplete lines
 */
NS_IMETHODIMP mozXMLTermSession::FlushOutput(FlushActionType flushAction)
{
  nsresult result;

  if (!mEntryHasOutput)
    return NS_OK;

  XMLT_LOG(mozXMLTermSession::FlushOutput,70,
          ("flushAction=%d, mOutputDisplayType=%d\n",
           flushAction, mOutputDisplayType));

  PRBool preDisplay = (mOutputDisplayType == PRE_STDOUT_NODE) ||
                      (mOutputDisplayType == PRE_STDERR_NODE) ||
                      (mOutputDisplayType == PRE_STDIN_NODE);

  if (preDisplay) {
    // PRE text display
    OutputDisplayType preDisplayType = mOutputDisplayType;
    nsAutoString preTextSplit; preTextSplit.SetLength(0);

    if (flushAction != DISPLAY_INCOMPLETE_FLUSH) {
      // Split/clear/close incomplete line

      XMLT_LOG(mozXMLTermSession::FlushOutput,72,
               ("mPreTextIncomplete.Length()=%d\n",
                mPreTextIncomplete.Length() ));

      if (flushAction == SPLIT_INCOMPLETE_FLUSH) {
        // Move incomplete text to new PRE element
        preTextSplit = mPreTextIncomplete;

      } else if (flushAction == CLOSE_INCOMPLETE_FLUSH) {
        // Move incomplete text into buffer
        mPreTextBuffered += mPreTextIncomplete;
      }

      // Clear incomplete PRE text
      mPreTextIncomplete.SetLength(0);

      if ((mPreTextBufferLines == 0) && mPreTextBuffered.IsEmpty()) {
        // Remove lone text node
        nsCOMPtr<nsIDOMNode> resultNode;
        result = mOutputDisplayNode->RemoveChild(mOutputTextNode,
                                             getter_AddRefs(resultNode));

        // Check if PRE node has any child nodes
        PRBool hasChildNodes = PR_TRUE;
        result = mOutputDisplayNode->HasChildNodes(&hasChildNodes);

        if (!hasChildNodes) {
          // No child nodes left; Delete PRE node itself
          nsCOMPtr<nsIDOMNode> resultNode2;
          result = mOutputBlockNode->RemoveChild(mOutputDisplayNode,
                                                 getter_AddRefs(resultNode));
        }

        mOutputDisplayNode = nsnull;
        mOutputDisplayType = NO_NODE;
        mOutputTextNode = nsnull;
      }
    }

    if (mOutputDisplayNode != nsnull) {
      // Update displayed PRE text
      nsAutoString outString(mPreTextBuffered);
      outString += mPreTextIncomplete;

      // Increment total output line count for entry
      mEntryOutputLines += mPreTextBufferLines;

      if (outString != mPreTextDisplayed) {
        // Display updated buffer
        mPreTextDisplayed = outString;

        XMLT_LOG(mozXMLTermSession::FlushOutput,72,
                 ("mOutputTextNode=%d\n", (mOutputTextNode != nsnull)));

        result = SetDOMText(mOutputTextNode, mPreTextDisplayed);
        if (NS_FAILED(result))
          return NS_ERROR_FAILURE;

      }
    }

    if (flushAction != DISPLAY_INCOMPLETE_FLUSH) {
      // Split/clear/close incomplete line
      mOutputDisplayNode = nsnull;
      mOutputDisplayType = NO_NODE;
      mOutputTextNode = nsnull;

      if ( (flushAction == SPLIT_INCOMPLETE_FLUSH) &&
           !preTextSplit.IsEmpty() ) {
        // Create new PRE element with incomplete text
        nsAutoString styleStr; styleStr.SetLength(0);

        if (preDisplayType == PRE_STDIN_NODE) {
          styleStr += (PRUnichar) LTERM_STDIN_STYLE;

        } else if (preDisplayType == PRE_STDERR_NODE) {
          styleStr += (PRUnichar) LTERM_STDERR_STYLE;

        } else {
          styleStr += (PRUnichar) LTERM_STDOUT_STYLE;
        }

        XMLT_LOG(mozXMLTermSession::FlushOutput,72,("splitting\n"));

        AppendOutput(preTextSplit, styleStr, PR_FALSE);

        FlushOutput(DISPLAY_INCOMPLETE_FLUSH);
      }
    }

  } else if (mOutputDisplayNode != nsnull) {
    // Non-PRE node
    if (flushAction == CLEAR_INCOMPLETE_FLUSH) {
      // Clear incomplete line info
      nsCOMPtr<nsIDOMNode> resultNode;
      result = mOutputBlockNode->RemoveChild(mOutputDisplayNode,
                                             getter_AddRefs(resultNode));
      mOutputDisplayNode = nsnull;
      mOutputDisplayType = NO_NODE;
      mOutputTextNode = nsnull;

    } else if (flushAction == CLOSE_INCOMPLETE_FLUSH) {
      mOutputDisplayNode = nsnull;
      mOutputDisplayType = NO_NODE;
      mOutputTextNode = nsnull;

    }
  }

  XMLT_LOG(mozXMLTermSession::FlushOutput,71,("returning\n"));

  return NS_OK;
}


/** Positions cursor below the last output element */
void mozXMLTermSession::PositionOutputCursor(mozILineTermAux* lineTermAux)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::PositionOutputCursor,80,("\n"));

  PRBool dummyOutput = PR_FALSE;
  if (!mOutputTextNode) {
    // Append dummy output line
    nsCOMPtr<nsIDOMNode> spanNode, textNode;
    nsAutoString tagName(NS_LITERAL_STRING("span"));
    nsAutoString elementName; elementName.AssignASCII(sessionElementNames[STDOUT_ELEMENT]);
    result = NewElementWithText(tagName, elementName, -1,
                                mOutputBlockNode, spanNode, textNode);

    if (NS_FAILED(result) || !spanNode || !textNode)
      return;

    // Display NBSP for cursor positioning
    nsAutoString tempString;
    tempString += kNBSP;
    SetDOMText(textNode, tempString);
    dummyOutput = PR_TRUE;

    mOutputDisplayType = SPAN_DUMMY_NODE;
    mOutputDisplayNode = spanNode;
    mOutputTextNode = textNode;
    mOutputTextOffset = 0;
  }

  // Get selection
  nsCOMPtr<nsISelection> selection;

  nsCOMPtr<nsISelectionController> selCon;
  result = mXMLTerminal->GetSelectionController(getter_AddRefs(selCon));
  if (NS_FAILED(result) || !selCon)
    return; // NS_ERROR_FAILURE

  result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                    getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection) {
    // Position cursor at end of line
    nsCOMPtr<nsIDOMText> domText( do_QueryInterface(mOutputTextNode) );
    nsAutoString text; text.SetLength(0);
    domText->GetData(text);

    PRInt32 textOffset = text.Length();
    if (textOffset && dummyOutput) textOffset--;

    if (lineTermAux && (mOutputDisplayType == PRE_STDIN_NODE)) {
      // Get cursor column
      PRInt32 cursorCol = 0;
      lineTermAux->GetCursorColumn(&cursorCol);
      textOffset = cursorCol - mOutputTextOffset;
      if (textOffset > (PRInt32)text.Length())
        textOffset = text.Length();
    }
    result = selection->Collapse(mOutputTextNode, textOffset);
  }
}


/** Scrolls document to align bottom and left margin with screen */
NS_IMETHODIMP mozXMLTermSession::ScrollToBottomLeft(void)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::ScrollToBottomLeft,70,("\n"));

  nsCOMPtr<nsIPresShell> presShell;
  result = mXMLTerminal->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(result) || !presShell)
    return NS_ERROR_FAILURE;

  nsIDocument* doc = presShell->GetDocument();
  if (doc) {
    doc->FlushPendingNotifications(Flush_Layout);
  }

  // Get DOM Window
  nsCOMPtr<nsIDocShell> docShell;
  result = mXMLTerminal->GetDocShell(getter_AddRefs(docShell));
  if (NS_FAILED(result) || !docShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindowInternal> domWindow;
  result = mozXMLTermUtils::ConvertDocShellToDOMWindow(docShell,
                                           getter_AddRefs(domWindow));

  if (NS_FAILED(result) || !domWindow)
    return NS_ERROR_FAILURE;

  // Scroll to bottom left of screen
  domWindow->ScrollBy(-99999,99999);

  return NS_OK;
}


/** Gets current entry (command) number
 * @param aNumber (output) current entry number
 */
NS_IMETHODIMP mozXMLTermSession::GetCurrentEntryNumber(PRInt32 *aNumber)
{
  *aNumber = mCurrentEntryNumber;
  return NS_OK;
}


// Get size of entry history buffer
NS_IMETHODIMP mozXMLTermSession::GetHistory(PRInt32 *aHistory)
{
  *aHistory = mMaxHistory;
  return NS_OK;
}


// Set size of entry history buffer
NS_IMETHODIMP mozXMLTermSession::SetHistory(PRInt32 aHistory)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::SetHistory,30,("\n"));

  if (aHistory < 1)
    aHistory = 1;

  if (mInitialized && mStartEntryNode && (aHistory < mMaxHistory)) {
    // Delete any extra entry blocks
    PRInt32 delEntries = (mCurrentEntryNumber-mStartEntryNumber)
                         - aHistory;
    PRInt32 j;
    for (j=0; j<delEntries; j++) {
      nsCOMPtr<nsIDOMNode> newStartNode;
      result = mStartEntryNode->GetNextSibling(getter_AddRefs(newStartNode));
      if (NS_FAILED(result) || !newStartNode) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIDOMNode> resultNode;
      result = mSessionNode->RemoveChild(mStartEntryNode,
                                        getter_AddRefs(resultNode));

      if (NS_FAILED(result)) {
        return NS_ERROR_FAILURE;
      }

      mStartEntryNode = newStartNode;
      mStartEntryNumber++;
    }
  }

  mMaxHistory = aHistory;

  return NS_OK;
}


// Get HTML prompt string
NS_IMETHODIMP mozXMLTermSession::GetPrompt(PRUnichar **_aPrompt)
{
  // NOTE: Need to be sure that this may be freed by nsMemory::Free
  *_aPrompt = ToNewUnicode(mPromptHTML);
  return NS_OK;
}


// Set HTML prompt string
NS_IMETHODIMP mozXMLTermSession::SetPrompt(const PRUnichar* aPrompt)
{
  mPromptHTML = aPrompt;
  return NS_OK;
}


/** Gets flag denoting whether terminal is in full screen mode
 * @param aFlag (output) screen mode flag
 */
NS_IMETHODIMP mozXMLTermSession::GetScreenMode(PRBool* aFlag)
{
  if (!aFlag)
    return NS_ERROR_NULL_POINTER;

  *aFlag = (mScreenNode != nsnull);

  return NS_OK;
}


/** Create a DIV element with attributes NAME="preface", CLASS="preface",
 * and ID="preface0", containing an empty text node, and append it as a
 * child of the main BODY element. Also make it the current display element.
 */
NS_IMETHODIMP mozXMLTermSession::NewPreface(void)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::NewPreface,40,("\n"));

  // Create preface element and append as child of session element
  nsCOMPtr<nsIDOMNode> divNode;
  nsAutoString tagName(NS_LITERAL_STRING("div"));
  nsAutoString name(NS_LITERAL_STRING("preface"));
  result = NewElement(tagName, name, 0,
                      mSessionNode, divNode);

  if (NS_FAILED(result) || !divNode)
    return NS_ERROR_FAILURE;

  mOutputBlockNode = divNode;

  mOutputDisplayType = NO_NODE;
  mOutputDisplayNode = nsnull;
  mOutputTextNode = nsnull;

  // Command output being processed
  mEntryHasOutput = PR_TRUE;

  return NS_OK;
}


/** Create and append a new DIV element with attributes NAME="entry",
 * CLASS="entry", and ID="entry#" as the last child of the main BODY element,
 * where "#" denotes the new entry number obtained by incrementing the
 * current entry number.
 * Inside the entry element, create a DIV element with attributes
 * NAME="input", CLASS="input", and ID="input#" containing two elements,
 * named "prompt" and "command", each containing a text node.
 * Insert the supplied prompt string into the prompt element's text node.
 * @param aPrompt prompt string to be inserted into prompt element
 */
NS_IMETHODIMP mozXMLTermSession::NewEntry(const nsString& aPrompt)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::NewEntry,50,("\n"));

  if (mCurrentEntryNumber == 0) {
    // First entry
    mCurrentEntryNumber = 1;
    mStartEntryNumber = 1;

  } else {
    // Not first entry

    // Add event attributes to current command element
    nsAutoString cmdName; cmdName.AssignASCII(sessionElementNames[COMMAND_ELEMENT]);
    result = SetEventAttributes(cmdName,
                                mCurrentEntryNumber,
                                mCommandSpanNode);
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

    // Increment entry number
    mCurrentEntryNumber++;

    if ((mCurrentEntryNumber - mStartEntryNumber) > mMaxHistory) {
      // Delete oldest displayed entry element

      nsCOMPtr<nsIDOMNode> newStartNode;
      result = mStartEntryNode->GetNextSibling(getter_AddRefs(newStartNode));
      if (NS_FAILED(result) || !newStartNode) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIDOMNode> resultNode;
      result = mSessionNode->RemoveChild(mStartEntryNode,
                                      getter_AddRefs(resultNode));

      if (NS_FAILED(result)) {
        return NS_ERROR_FAILURE;
      }

      mStartEntryNode = newStartNode;
      mStartEntryNumber++;
    }
  }

  XMLT_LOG(mozXMLTermSession::NewEntry,50,
           ("%d (start=%d)\n", mCurrentEntryNumber, mStartEntryNumber));

  nsAutoString tagName, name;

  // Create "entry" element
  nsCOMPtr<nsIDOMNode> entryNode;
  tagName.AssignLiteral("div");
  name.AssignASCII(sessionElementNames[ENTRY_ELEMENT]);
  result = NewElement(tagName, name, mCurrentEntryNumber,
                      mSessionNode, entryNode);
  if (NS_FAILED(result) || !entryNode) {
    return NS_ERROR_FAILURE;
  }

  mCurrentEntryNode = entryNode;

  if (mCurrentEntryNumber == 1) {
    mStartEntryNode = mCurrentEntryNode;
  }

  // Create "input" element containing "prompt" and "command" elements
  nsCOMPtr<nsIDOMNode> inputNode;
  tagName.AssignLiteral("div");
  name.AssignASCII(sessionElementNames[INPUT_ELEMENT]);
  result = NewElement(tagName, name, mCurrentEntryNumber,
                      mCurrentEntryNode, inputNode);
  if (NS_FAILED(result) || !inputNode) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString classAttribute;

  // Create prompt element
  nsCOMPtr<nsIDOMNode> promptSpanNode;
  tagName.AssignLiteral("span");
  name.AssignASCII(sessionElementNames[PROMPT_ELEMENT]);
  result = NewElement(tagName, name, mCurrentEntryNumber,
                      inputNode, promptSpanNode);
  if (NS_FAILED(result) || !promptSpanNode) {
    return NS_ERROR_FAILURE;
  }

  // Add event attributes to prompt element
  result = SetEventAttributes(name, mCurrentEntryNumber,
                                promptSpanNode);

  nsCOMPtr<nsIDOMDocument> domDoc;
  result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(result) || !domDoc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> resultNode;

  if (mPromptHTML.IsEmpty()) {

#define DEFAULT_ICON_PROMPT
#ifdef DEFAULT_ICON_PROMPT    // Experimental code; has scrolling problems
    // Create text node + image node as child of prompt element
    nsCOMPtr<nsIDOMNode> spanNode, textNode;

    tagName.AssignLiteral("span");
    name.AssignLiteral("noicons");
    result = NewElementWithText(tagName, name, -1,
                                promptSpanNode, spanNode, textNode);
    if (NS_FAILED(result) || !spanNode || !textNode) {
      return NS_ERROR_FAILURE;
    }

    // Strip single trailing space, if any, from prompt string
    int spaceOffset = aPrompt.Length();

    if ((spaceOffset > 0) && (aPrompt.Last() == ((PRUnichar) ' ')))
      spaceOffset--;

    nsAutoString promptStr;
    aPrompt.Left(promptStr, spaceOffset);

    // Set prompt text
    result = SetDOMText(textNode, promptStr);
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

    // Create IMG element
    tagName.AssignLiteral("img");
    nsCOMPtr<nsIDOMElement> imgElement;
    result = domDoc->CreateElement(tagName, getter_AddRefs(imgElement));
    if (NS_FAILED(result) || !imgElement)
      return NS_ERROR_FAILURE;

    // Set attributes
    nsAutoString attName(NS_LITERAL_STRING("class"));
    nsAutoString attValue(NS_LITERAL_STRING("icons"));
    imgElement->SetAttribute(attName, attValue);

    attName.AssignLiteral("src");
    attValue.AssignLiteral("chrome://xmlterm/skin/wheel.gif");
    imgElement->SetAttribute(attName, attValue);

    attName.AssignLiteral("align");
    attValue.AssignLiteral("middle");
    imgElement->SetAttribute(attName, attValue);

    // Append IMG element
    nsCOMPtr<nsIDOMNode> imgNode = do_QueryInterface(imgElement);
    result = promptSpanNode->AppendChild(imgNode,
                                          getter_AddRefs(resultNode));
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

#else // !DEFAULT_ICON_PROMPT
    // Create text node as child of prompt element
    nsCOMPtr<nsIDOMNode> textNode;
    result = NewTextNode(promptSpanNode, textNode);

    if (NS_FAILED(result) || !textNode)
      return NS_ERROR_FAILURE;

    // Set prompt text
    result = SetDOMText(textNode, aPrompt);
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;
#endif // !DEFAULT_ICON_PROMPT

  } else {
    // User-specified HTML prompt
    result = InsertFragment(mPromptHTML, promptSpanNode,
                            mCurrentEntryNumber);
  }

  // Append text node containing single NBSP
  nsCOMPtr<nsIDOMText> stubText;
  nsAutoString spaceStr(kNBSP);
  result = domDoc->CreateTextNode(spaceStr, getter_AddRefs(stubText));
  if (NS_FAILED(result) || !stubText)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> stubNode = do_QueryInterface(stubText);
  result = inputNode->AppendChild(stubNode, getter_AddRefs(resultNode));
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  mPromptTextNode = stubNode;

  // Create command element
  nsCOMPtr<nsIDOMNode> newCommandSpanNode;
  tagName.AssignLiteral("span");
  name.AssignASCII(sessionElementNames[COMMAND_ELEMENT]);
  result = NewElement(tagName, name, mCurrentEntryNumber,
                      inputNode, newCommandSpanNode);
  if (NS_FAILED(result) || !newCommandSpanNode) {
    return NS_ERROR_FAILURE;
  }

  mCommandSpanNode = newCommandSpanNode;

  // Create text node as child of command element
  nsCOMPtr<nsIDOMNode> textNode2;
  result = NewTextNode(mCommandSpanNode, textNode2);

  if (NS_FAILED(result) || !textNode2)
    return NS_ERROR_FAILURE;

  mInputTextNode = textNode2;

  // Create output element and append as child of current entry element
  nsCOMPtr<nsIDOMNode> divNode;
  tagName.AssignLiteral("div");
  name.AssignASCII(sessionElementNames[OUTPUT_ELEMENT]);
  result = NewElement(tagName, name, mCurrentEntryNumber,
                      mCurrentEntryNode, divNode);

  if (NS_FAILED(result) || !divNode)
    return NS_ERROR_FAILURE;

  mOutputBlockNode = divNode;

  mOutputDisplayType = NO_NODE;
  mOutputDisplayNode = nsnull;
  mOutputTextNode = nsnull;

  // No command output processed yet
  mEntryHasOutput = PR_FALSE;

  mEntryOutputLines = 0;

  return NS_OK;
}


/** Create a DIV element with attributes NAME="screen" and CLASS="screen",
 * containing an empty text node, and append it as a
 * child of the main BODY element. Also make it the current display element.
 */
NS_IMETHODIMP mozXMLTermSession::NewScreen(void)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::NewScreen,70,("\n"));

  // Create screen element and append as child of session element
  nsCOMPtr<nsIDOMNode> divNode;
  nsAutoString tagName(NS_LITERAL_STRING("div"));
  nsAutoString name(NS_LITERAL_STRING("screen"));
  result = NewElement(tagName, name, 0,
                      mBodyNode, divNode);

  if (NS_FAILED(result) || !divNode)
    return NS_ERROR_FAILURE;

  mScreenNode = divNode;

  // Collapse non-screen stuff
  nsAutoString attName(NS_LITERAL_STRING("xmlt-block-collapsed"));
  nsAutoString attValue(NS_LITERAL_STRING("true"));

  nsCOMPtr<nsIDOMElement> menusElement = do_QueryInterface(mMenusNode);

  if (NS_SUCCEEDED(result) && menusElement) {
    menusElement->SetAttribute(attName, attValue);
  }

  nsCOMPtr<nsIDOMElement> sessionElement = do_QueryInterface(mSessionNode);

  if (sessionElement) {
    sessionElement->SetAttribute(attName, attValue);
  }

  // Create individual row elements
  nsCOMPtr<nsIDOMNode> resultNode;
  PRInt32 row;
  for (row=0; row < mScreenRows; row++) {
    NewRow(nsnull, getter_AddRefs(resultNode));
  }

  // Collapse selection to bottom of screen (for scrolling)
  result = PositionScreenCursor(0, 0);

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsISelectionController> selCon;
    result = mXMLTerminal->GetSelectionController(getter_AddRefs(selCon));
    if (NS_FAILED(result) || !selCon)
      return NS_ERROR_FAILURE;

    result = selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                             nsISelectionController::SELECTION_FOCUS_REGION,
                                             PR_TRUE);
  }

  return NS_OK;
}


/** Returns DOM PRE node corresponding to specified screen row
 */
NS_IMETHODIMP mozXMLTermSession::GetRow(PRInt32 aRow, nsIDOMNode** aRowNode)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::GetRow,60,("aRow=%d\n", aRow));

  if (!aRowNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNodeList> childNodes;
  result = mScreenNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(result) || !childNodes)
    return NS_ERROR_FAILURE;

  PRUint32 nChildren = 0;
  childNodes->GetLength(&nChildren);

  XMLT_LOG(mozXMLTermSession::GetRow,62,("nChildren=%d, mScreenRows=%d\n",
                                         nChildren, mScreenRows));

  PRInt32 rowIndex = mScreenRows - aRow - 1;
  if ((rowIndex < 0) || (rowIndex >= (PRInt32)nChildren))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> childNode;
  result = childNodes->Item(rowIndex, getter_AddRefs(childNode));

  if (NS_FAILED(result) || !childNode)
    return NS_ERROR_FAILURE;

  *aRowNode = childNode.get();
  NS_ADDREF(*aRowNode);

  XMLT_LOG(mozXMLTermSession::GetRow,61,("returning\n"));

  return NS_OK;
}


/** Positions cursor to specified screen row/col position
 */
NS_IMETHODIMP mozXMLTermSession::PositionScreenCursor(PRInt32 aRow,
                                                      PRInt32 aCol)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::PositionScreenCursor,60,
           ("row=%d, col=%d\n",aRow,aCol));

  // Get row node
  nsCOMPtr<nsIDOMNode> rowNode;
  result = GetRow(aRow, getter_AddRefs(rowNode));
  if (NS_FAILED(result) || !rowNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNodeList> childNodes;
  result = rowNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(result) || !childNodes)
    return NS_ERROR_FAILURE;

  PRUint32 nChildren = 0;
  childNodes->GetLength(&nChildren);
  XMLT_LOG(mozXMLTermSession::GetScreenText,60,("children=%d\n",nChildren));

  PRUint16 nodeType;
  PRUint32 j;
  PRInt32 prevCols = 0;
  PRInt32 textOffset = 0;
  nsCOMPtr<nsIDOMNode> textNode = nsnull;
  nsCOMPtr<nsIDOMNode> childNode;
  nsAutoString text; text.SetLength(0);

  for (j=0; j<nChildren; j++) {
    result = childNodes->Item(j, getter_AddRefs(childNode));
    if (NS_FAILED(result) || !childNode)
      return NS_ERROR_FAILURE;

    result = childNode->GetNodeType(&nodeType);
    if (NS_FAILED(result))
      return result;

    XMLT_LOG(mozXMLTermSession::GetScreenText,60,
             ("j=%d, nodeType=%d\n", j, nodeType));
    if (nodeType != nsIDOMNode::TEXT_NODE) {
      nsCOMPtr<nsIDOMNode> temNode;
      result = childNode->GetFirstChild(getter_AddRefs(temNode));
      if (NS_FAILED(result))
        return result;

      childNode = temNode;

      result = childNode->GetNodeType(&nodeType);
      if (NS_FAILED(result))
        return result;
      PR_ASSERT(nodeType == nsIDOMNode::TEXT_NODE);
    }

    nsCOMPtr<nsIDOMText> domText( do_QueryInterface(childNode) );
    result = domText->GetData(text);
    if (NS_FAILED(result))
      return result;

    XMLT_LOG(mozXMLTermSession::GetScreenText,60,("prevCols=%d\n",prevCols));

    if (prevCols+(PRInt32)text.Length() >= aCol) {
      // Determine offset in current text element
      textOffset = aCol - prevCols;
      textNode = childNode;
    } else if (j == nChildren-1) {
      // Position at end of line
      textOffset = text.Length();
      textNode = childNode;
    }
  }

  // Get selection
  nsCOMPtr<nsISelection> selection;

  nsCOMPtr<nsISelectionController> selCon;
  result = mXMLTerminal->GetSelectionController(getter_AddRefs(selCon));
  if (NS_FAILED(result) || !selCon)
    return NS_ERROR_FAILURE;

  result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                    getter_AddRefs(selection));

  if (NS_SUCCEEDED(result) && selection) {
    // Collapse selection to cursor position
    result = selection->Collapse(textNode, textOffset);
  }

  return NS_OK;
}


/** Create a PRE element with attributes NAME="row", CLASS="row",
 * containing an empty text node, and insert it as a
 * child of the SCREEN element before beforeRowNode, or at the
 * end if beforeRowNode is null.
 */
NS_IMETHODIMP mozXMLTermSession::NewRow(nsIDOMNode* beforeRowNode,
                                        nsIDOMNode** resultNode)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::NewRow,60,("\n"));

  // Create PRE display node
  nsCOMPtr<nsIDOMNode> preNode, textNode;
  nsAutoString tagName(NS_LITERAL_STRING("pre"));
  nsAutoString elementName(NS_LITERAL_STRING("row"));

  result = NewElementWithText(tagName, elementName, -1,
                              mScreenNode, preNode, textNode);

  if (NS_FAILED(result) || !preNode || !textNode)
    return NS_ERROR_FAILURE;

  // Set PRE element attributes
  nsCOMPtr<nsIDOMElement> preElement = do_QueryInterface(preNode);
  nsAutoString att(NS_LITERAL_STRING("cols"));
  nsAutoString val; val.SetLength(0);
  val.AppendInt(mScreenCols,10);
  preElement->SetAttribute(att, val);

  att.AssignLiteral("rows");
  val.AssignLiteral("1");
  preElement->SetAttribute(att, val);

  if (beforeRowNode) {
    // Insert row node
    result = mScreenNode->InsertBefore(preNode, beforeRowNode, resultNode);
  } else {
    // Append row node
    result = mScreenNode->AppendChild(preNode, resultNode);
  }

  return NS_OK;
}


/** Displays screen output string with specified style
 * @param aString string to be processed
 * @param aStyle style values for string (see lineterm.h)
 *               (if it is a null string, STDOUT style is assumed)
 * @param aRow row in which to insert string
 */
NS_IMETHODIMP mozXMLTermSession::DisplayRow(const nsString& aString,
                                            const nsString& aStyle,
                                            PRInt32 aRow)
{
  nsresult result;

  const PRInt32   strLength   = aString.Length();
  const PRInt32   styleLength = aStyle.Length();
  const PRUnichar *strStyle   = aStyle.get();

  XMLT_LOG(mozXMLTermSession::DisplayRow,70,
           ("aRow=%d, strLength=%d, styleLength=%d\n",
            aRow, strLength, styleLength));

  // Check if line has uniform style
  PRUnichar uniformStyle = LTERM_STDOUT_STYLE;

  if (styleLength > 0) {
    PRInt32 j;

    PR_ASSERT(styleLength == strLength);

    uniformStyle = strStyle[0];

    for (j=1; j<strLength; j++) {
      if (strStyle[j] != strStyle[0]) {
        uniformStyle = 0;
      }
    }
  }

  nsCOMPtr<nsIDOMNode> rowNode;
  result = GetRow(aRow, getter_AddRefs(rowNode));
  if (NS_FAILED(result) || !rowNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNodeList> childNodes;
  result = rowNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(result) || !childNodes)
    return NS_ERROR_FAILURE;

  PRUint32 nChildren = 0;
  childNodes->GetLength(&nChildren);

  XMLT_LOG(mozXMLTermSession::DisplayRow,79,("nChildren=%d\n", nChildren));

  if ((nChildren == 1) && (uniformStyle == LTERM_STDOUT_STYLE)) {
    // Get child node
    nsCOMPtr<nsIDOMNode> childNode;

    result = rowNode->GetFirstChild(getter_AddRefs(childNode));
    if (NS_FAILED(result) || !childNode)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMText> domText( do_QueryInterface(childNode) );
    if (domText) {
      // Display uniform style
      result = SetDOMText(childNode, aString);
      if (NS_FAILED(result))
        return result;

      return NS_OK;
    }
  }

  // Delete all child nodes for the row
  nsCOMPtr<nsIDOMNode> childNode;
  PRInt32 j;
  for (j=nChildren-1; j>=0; j--) {
    result = childNodes->Item(j, getter_AddRefs(childNode));
    if (NS_FAILED(result) || !childNode)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMNode> resultNode;
    result = rowNode->RemoveChild(childNode, getter_AddRefs(resultNode));
    if (NS_FAILED(result))
      return result;
  }

  nsCOMPtr<nsIDOMNode> spanNode, textNode;
  nsAutoString tagName(NS_LITERAL_STRING("span"));
  nsAutoString elementName;
  nsAutoString subString;
  PRInt32 k;
  PRUnichar currentStyle = LTERM_STDOUT_STYLE;
  if (styleLength > 0) 
    currentStyle = strStyle[0];
  PRInt32 offset = 0;
  offset = 0;

  PR_ASSERT(strLength > 0);

  for (k=1; k<strLength+1; k++) {
    if ((k == strLength) || ((k < styleLength) &&
                             (strStyle[k] != currentStyle)) ) {
      // Change of style or end of string

      if (currentStyle == LTERM_STDOUT_STYLE) {
        // Create text node
        result = NewTextNode(rowNode, textNode);
        if (NS_FAILED(result) || !textNode)
          return NS_ERROR_FAILURE;

      } else {
        // Span Node

        switch (currentStyle) {
        case LTERM_STDOUT_STYLE | LTERM_BOLD_STYLE:
          elementName.AssignLiteral("boldstyle");
          break;
        case LTERM_STDOUT_STYLE | LTERM_ULINE_STYLE:
          elementName.AssignLiteral("underlinestyle");
          break;
        case LTERM_STDOUT_STYLE | LTERM_BLINK_STYLE:
          elementName.AssignLiteral("blinkstyle");
          break;
        case LTERM_STDOUT_STYLE | LTERM_INVERSE_STYLE:
          elementName.AssignLiteral("inversestyle");
          break;
        default:
          elementName.AssignLiteral("boldstyle");
          break;
        }

        result = NewElementWithText(tagName, elementName, -1,
                                    rowNode, spanNode, textNode);

        if (NS_FAILED(result) || !spanNode || !textNode)
          return NS_ERROR_FAILURE;
      }

      aString.Mid(subString, offset, k-offset);
      result = SetDOMText(textNode, subString);
      if (NS_FAILED(result))
        return result;

      if (k < styleLength) {
        // Change style
        currentStyle = strStyle[k];
        offset = k;
      }
    }
  }
    
  return NS_OK;
}


/** Append a BR element as the next child of specified parent.
 * @param parentNode parent node for BR element
 */
NS_IMETHODIMP mozXMLTermSession::NewBreak(nsIDOMNode* parentNode)
{
  nsresult result;
  nsAutoString tagName(NS_LITERAL_STRING("br"));

  XMLT_LOG(mozXMLTermSession::NewBreak,60,("\n"));

  // Create "br" element and append as child of specified parent
  nsCOMPtr<nsIDOMNode> brNode;
  nsAutoString name; name.SetLength(0);
  result = NewElement(tagName, name, -1, parentNode, brNode);

  if (NS_FAILED(result) || !brNode)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


/** Create an empty block element with tag name tagName with attributes
 * NAME="name", CLASS="name", and ID="name#", and appends it as a child of
 * the specified parent. ("#" denotes the specified number)
 * Also create an empty text node inside the new block element.
 * @param tagName tag name of element
 * @param name name and class of element
 *             (If zero-length string, then no attributes are set)
 * @param number numeric suffix for element ID
 *             (If < 0, no ID attribute is defined)
 * @param parentNode parent node for element
 * @param blockNode (output) block-level DOM node for created element
 * @param textNode (output) child text DOM node of element
 * @param beforeNode child node before which to insert new node
 *                   if null, insert after last child node
 *                   (default value is null)
 */
NS_IMETHODIMP mozXMLTermSession::NewElementWithText(const nsString& tagName,
                                      const nsString& name, PRInt32 number,
                                      nsIDOMNode* parentNode,
                                      nsCOMPtr<nsIDOMNode>& blockNode,
                                      nsCOMPtr<nsIDOMNode>& textNode,
                                      nsIDOMNode* beforeNode)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::NewElementWithText,80,("\n"));

  // Create block element
  result = NewElement(tagName, name, number, parentNode, blockNode,
                      beforeNode);
  if (NS_FAILED(result) || !blockNode)
    return NS_ERROR_FAILURE;

  // Create text node as child of block element
  result = NewTextNode(blockNode, textNode);

  if (NS_FAILED(result) || !textNode)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


/** Creates an empty anchor (A) element with tag name tagName with attributes
 * CLASS="classAttribute", and ID="classAttribute#", and appends it as a
 * child of the specified parent. ("#" denotes the specified number)
 * @param classAttribute class attribute of anchor element
 *             (If zero-length string, then no attributes are set)
 * @param number numeric suffix for element ID
 *             (If < 0, no ID attribute is defined)
 * @param parentNode parent node for element
 * @param anchorNode (output) DOM node for created anchor element
 */
NS_IMETHODIMP mozXMLTermSession::NewAnchor(const nsString& classAttribute,
                                           PRInt32 number,
                                           nsIDOMNode* parentNode,
                                           nsCOMPtr<nsIDOMNode>& anchorNode)
{
  nsresult result;
  nsAutoString tagName(NS_LITERAL_STRING("a"));

  XMLT_LOG(mozXMLTermSession::NewAnchor,80,("\n"));

  nsCOMPtr<nsIDOMDocument> domDoc;
  result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(result) || !domDoc)
    return NS_ERROR_FAILURE;

  // Create anchor
  nsCOMPtr<nsIDOMElement> newElement;
  result = domDoc->CreateElement(tagName, getter_AddRefs(newElement));
  if (NS_FAILED(result) || !newElement)
    return NS_ERROR_FAILURE;

  // Set element attributes
  nsAutoString hrefAtt(NS_LITERAL_STRING("href"));
  nsAutoString hrefVal(NS_LITERAL_STRING("#"));
  newElement->SetAttribute(hrefAtt, hrefVal);

  if (!classAttribute.IsEmpty()) {
    nsAutoString classStr(NS_LITERAL_STRING("class"));
    newElement->SetAttribute(classStr, classAttribute);

    if (number >= 0) {
      nsAutoString idAtt(NS_LITERAL_STRING("id"));
      nsAutoString idVal(classAttribute);
      idVal.AppendInt(number,10);
      newElement->SetAttribute(idAtt, idVal);
    }
  }

  // Append child to parent
  nsCOMPtr<nsIDOMNode> newBlockNode = do_QueryInterface(newElement);
  result = parentNode->AppendChild(newBlockNode, getter_AddRefs(anchorNode));
  if (NS_FAILED(result) || !anchorNode)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


/** Creates an empty block element with tag name tagName with attributes
 * NAME="name", CLASS="name", and ID="name#", and appends it as a child of
 * the specified parent. ("#" denotes the specified number)
 * @param tagName tag name of element
 * @param name name and class of element
 *             (If zero-length string, then no attributes are set)
 * @param number numeric suffix for element ID
 *             (If < 0, no ID attribute is defined)
 * @param parentNode parent node for element
 * @param blockNode (output) block-level DOM node for created element
 * @param beforeNode child node before which to insert new node
 *                   if null, insert after last child node
 *                   (default value is null)
 */
NS_IMETHODIMP mozXMLTermSession::NewElement(const nsString& tagName,
                                     const nsString& name, PRInt32 number,
                                     nsIDOMNode* parentNode,
                                     nsCOMPtr<nsIDOMNode>& blockNode,
                                     nsIDOMNode* beforeNode)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::NewElement,80,("\n"));

  nsCOMPtr<nsIDOMDocument> domDoc;
  result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(result) || !domDoc)
    return NS_ERROR_FAILURE;

  // Create element
  nsCOMPtr<nsIDOMElement> newElement;
  result = domDoc->CreateElement(tagName, getter_AddRefs(newElement));
  if (NS_FAILED(result) || !newElement)
    return NS_ERROR_FAILURE;

  if (!name.IsEmpty()) {
    // Set attributes
    nsAutoString classAtt(NS_LITERAL_STRING("class"));
    nsAutoString classVal(name);
    newElement->SetAttribute(classAtt, classVal);

    nsAutoString nameAtt(NS_LITERAL_STRING("name"));
    nsAutoString nameVal(name);
    newElement->SetAttribute(nameAtt, nameVal);

    if (number >= 0) {
      nsAutoString idAtt(NS_LITERAL_STRING("id"));
      nsAutoString idVal(name);
      idVal.AppendInt(number,10);
      newElement->SetAttribute(idAtt, idVal);
    }
  }

  nsCOMPtr<nsIDOMNode> newBlockNode = do_QueryInterface(newElement);

  if (beforeNode) {
    // Insert child
    result = parentNode->InsertBefore(newBlockNode, beforeNode,
                                      getter_AddRefs(blockNode));
    if (NS_FAILED(result) || !blockNode)
      return NS_ERROR_FAILURE;

  } else {
    // Append child
    result = parentNode->AppendChild(newBlockNode, getter_AddRefs(blockNode));
    if (NS_FAILED(result) || !blockNode)
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


/** Creates a new DOM text node, and appends it as a child of the
 * specified parent.
 * @param parentNode parent node for element
 * @param textNode (output) created text DOM node
 */
NS_IMETHODIMP mozXMLTermSession::NewTextNode( nsIDOMNode* parentNode,
                                       nsCOMPtr<nsIDOMNode>& textNode)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::NewTextNode,80,("\n"));

  nsCOMPtr<nsIDOMDocument> domDoc;
  result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(result) || !domDoc)
    return NS_ERROR_FAILURE;

  // Create text node
  nsCOMPtr<nsIDOMText> newText;
  nsAutoString nullStr; nullStr.SetLength(0);
  result = domDoc->CreateTextNode(nullStr, getter_AddRefs(newText));
  if (NS_FAILED(result) || !newText)
    return NS_ERROR_FAILURE;

  // Append child to parent
  nsCOMPtr<nsIDOMNode> newTextNode = do_QueryInterface(newText);
  result = parentNode->AppendChild(newTextNode, getter_AddRefs(textNode));
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  return NS_OK;
}


/** Creates a new IFRAME element with attribute NAME="iframe#",
 * and appends it as a child of the specified parent.
 * ("#" denotes the specified number)
 * @param parentNode parent node for element
 * @param number numeric suffix for element ID
 *             (If < 0, no name attribute is defined)
 * @param frameBorder IFRAME FRAMEBORDER attribute
 * @param src IFRAME SRC attribute
 * @param width IFRAME width attribute
 * @param height IFRAME height attribute
 */
NS_IMETHODIMP mozXMLTermSession::NewIFrame(nsIDOMNode* parentNode,
                                           PRInt32 number,
                                           PRInt32 frameBorder,
                                           const nsString& src,
                                           const nsString& width,
                                           const nsString& height)
                                           
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::NewIFrame,80,("\n"));

  nsCOMPtr<nsIDOMDocument> domDoc;
  result = mXMLTerminal->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(result) || !domDoc)
    return NS_ERROR_FAILURE;

#if 0
  nsAutoString iframeFrag("<iframe name='iframe");
  iframeFrag.Append(number,10);
  iframeFrag.Append("' frameborder=")
  iframeFrag.Append(frameBorder,10);
  iframeFrag.Append(" src='");
  iframeFrag.Append(src)
  iframeFrag.Append("'> </iframe>\n");
  result = InsertFragment(iframeFrag, parentNode, number);
  if (NS_FAILED(result))
    return result;

  return NS_OK;
#else
  // Create IFRAME element
  nsCOMPtr<nsIDOMElement> newElement;
  nsAutoString tagName(NS_LITERAL_STRING("iframe"));
  result = domDoc->CreateElement(tagName, getter_AddRefs(newElement));
  if (NS_FAILED(result) || !newElement)
    return NS_ERROR_FAILURE;

  nsAutoString attName, attValue;

  // Set attributes
  if (number >= 0) {
    attName.AssignLiteral("name");
    attValue.AssignLiteral("iframe");
    attValue.AppendInt(number,10);
    newElement->SetAttribute(attName, attValue);
  }

  attName.AssignLiteral("frameborder");
  attValue.SetLength(0);
  attValue.AppendInt(frameBorder,10);
  newElement->SetAttribute(attName, attValue);

  if (!src.IsEmpty()) {
    // Set SRC attribute
    attName.AssignLiteral("src");
    newElement->SetAttribute(attName, src);
  }

  if (!width.IsEmpty()) {
    // Set WIDTH attribute
    attName.AssignLiteral("width");
    newElement->SetAttribute(attName, width);
  }

  if (!height.IsEmpty()) {
    // Set HEIGHT attribute
    attName.AssignLiteral("height");
    newElement->SetAttribute(attName, height);
  }

  // Append child to parent
  nsCOMPtr<nsIDOMNode> iframeNode;
  nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(newElement);
  result = parentNode->AppendChild(newNode, getter_AddRefs(iframeNode));
  if (NS_FAILED(result) || !iframeNode)
    return NS_ERROR_FAILURE;

  return NS_OK;
#endif
}


/** Add event attributes (onclick, ...) to DOM node
 * @param name name of DOM node (supplied as argument to the event handler)
 * @param number entry number (supplied as argument to the event handler)
 * @param domNode DOM node to be modified 
 */
NS_IMETHODIMP mozXMLTermSession::SetEventAttributes(const nsString& name,
                                                    PRInt32 number,
                                             nsCOMPtr<nsIDOMNode>& domNode)
{
  nsresult result;

  nsCOMPtr <nsIDOMElement> domElement = do_QueryInterface(domNode);
  if (!domElement)
    return NS_ERROR_FAILURE;

  int j;
  for (j=0; j<SESSION_EVENT_TYPES; j++) {
    nsAutoString attName(NS_LITERAL_STRING("on"));
    attName.AppendASCII(sessionEventNames[j]);

    nsAutoString attValue(NS_LITERAL_STRING("return HandleEvent(event, '"));
    attValue.AppendASCII(sessionEventNames[j]);
    attValue.AppendLiteral("','");
    attValue.Append(name);
    attValue.AppendLiteral("','");
    attValue.AppendInt(number,10);
    attValue.Append(NS_LITERAL_STRING("','');"));

    result = domElement->SetAttribute(attName, attValue);
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


/** Sets text content of a DOM node to supplied string
 * @param textNode DOM text node to be modified
 * @param aString string to be inserted
 */
NS_IMETHODIMP mozXMLTermSession::SetDOMText(nsCOMPtr<nsIDOMNode>& textNode,
                                            const nsString& aString)
{
  nsresult result;

  nsCOMPtr<nsIDOMText> domText (do_QueryInterface(textNode));
  if (!domText)
    return NS_ERROR_FAILURE;

  result = domText->SetData(aString);

  return result;
}


/** Checks if node is a text node
 * @param aNode DOM node to be checked
 * @return PR_TRUE if node is a text node
 */
PRBool mozXMLTermSession::IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode) {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return PR_FALSE;
  }

  XMLT_LOG(mozXMLTermSession::IsTextNode,90,("\n"));

  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  if (nodeType == nsIDOMNode::TEXT_NODE)
    return PR_TRUE;
    
  return PR_FALSE;
}


/** Checks if node is a text, span, or anchor node
 * (i.e., allowed inside a PRE element)
 * @param aNode DOM node to be checked
 * @return PR_TRUE if node is a text, span or anchor node
 */
PRBool mozXMLTermSession::IsPREInlineNode(nsIDOMNode* aNode)
{
  nsresult result;
  PRBool isPREInlineNode = PR_FALSE;

  nsCOMPtr<nsIDOMText> domText = do_QueryInterface(aNode);

  if (domText) {
    isPREInlineNode = PR_TRUE;

  } else {
    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(aNode);

    if (domElement) {
      nsAutoString tagName; tagName.SetLength(0);
      result = domElement->GetTagName(tagName);
      if (NS_SUCCEEDED(result)) {
        isPREInlineNode = tagName.LowerCaseEqualsLiteral("span") ||
                          tagName.LowerCaseEqualsLiteral("a");
      }
    }
  }

  return isPREInlineNode;
}


/** Serializes DOM node and its content as an HTML fragment string
 * @param aNode DOM node to be serialized
 * @param indentString indentation prefix string
 * @param htmlString (output) serialized HTML fragment
 * @param deepContent if PR_TRUE, serialize children of node as well
 *                    (defaults to PR_FALSE)
 * @param insidePREnode set to PR_TRUE if aNode is embedded inside a PRE node
 *                      control formatting
 *                      (defaults to PR_FALSE)
 */
NS_IMETHODIMP mozXMLTermSession::ToHTMLString(nsIDOMNode* aNode,
                                              nsString& indentString,
                                              nsString& htmlString,
                                              PRBool deepContent,
                                              PRBool insidePRENode)
{
  nsresult result;

  XMLT_LOG(mozXMLTermSession::ToHTMLString,80,("\n"));

  nsAutoString newIndentString (indentString);
  newIndentString.AppendLiteral("  ");

  htmlString.SetLength(0);

  nsCOMPtr<nsIDOMText> domText( do_QueryInterface(aNode) );

  if (domText) {
    // Text node
    domText->GetData(htmlString);
    htmlString.ReplaceChar(kNBSP, ' ');

  } else {
    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(aNode);

    if (domElement) {
      nsAutoString tagName; tagName.SetLength(0);
      domElement->GetTagName(tagName);

      if (!insidePRENode) {
        htmlString += indentString;
      }
      htmlString.AppendLiteral("<");
      htmlString += tagName;

      PRBool isPRENode = tagName.LowerCaseEqualsLiteral("pre");

      nsCOMPtr<nsIDOMNamedNodeMap> namedNodeMap(nsnull);
      result = aNode->GetAttributes(getter_AddRefs(namedNodeMap));

      if (NS_SUCCEEDED(result) && namedNodeMap) {
        // Print all attributes
        PRUint32 nodeCount, j;
        result = namedNodeMap->GetLength(&nodeCount);

        if (NS_SUCCEEDED(result)) {
          nsCOMPtr<nsIDOMNode> attrNode;

          for (j=0; j<nodeCount; j++) {
            result = namedNodeMap->Item(j, getter_AddRefs(attrNode));

            if (NS_SUCCEEDED(result)) {
              nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(attrNode);

              if (attr) {
                nsAutoString attrName; attrName.SetLength(0);
                nsAutoString attrValue; attrValue.SetLength(0);

                result = attr->GetName(attrName);
                if (NS_SUCCEEDED(result)) {
                  htmlString.AppendLiteral(" ");
                  htmlString.Append(attrName);
                }

                result = attr->GetValue(attrValue);
                if (NS_SUCCEEDED(result) && !attrName.IsEmpty()) {
                  htmlString.AppendLiteral("=\"");
                  htmlString.Append(attrValue);
                  htmlString.AppendLiteral("\"");
                }
              }
            }
          }
        }
      }

      if (!deepContent) {
        htmlString.AppendLiteral(">");

      } else {
        // Iterate over all child nodes to generate deep content
        nsCOMPtr<nsIDOMNode> child;
        result = aNode->GetFirstChild(getter_AddRefs(child));

        nsAutoString htmlInner;
        while (child) {
          nsAutoString innerString;
          ToHTMLString(child, newIndentString, innerString, deepContent,
                       isPRENode);

          htmlInner += innerString;

          nsCOMPtr<nsIDOMNode> temp = child;
          result = temp->GetNextSibling(getter_AddRefs(child));
          if (NS_FAILED(result))
            break;
        }

        if (!htmlInner.IsEmpty()) {
          if (insidePRENode)
            htmlString.AppendLiteral("\n>");
          else
            htmlString.AppendLiteral(">\n");

          htmlString += htmlInner;

          if (!insidePRENode)
            htmlString += indentString;
        } else {
          htmlString.AppendLiteral(">");
        }

        htmlString.AppendLiteral("</");
        htmlString += tagName;

        if (insidePRENode)
          htmlString.AppendLiteral("\n");
        htmlString.AppendLiteral(">");

        if (!insidePRENode)
          htmlString.AppendLiteral("\n");
      }
    }
  }

  return NS_OK;
}


/** Implements the "tree:" meta command to traverse DOM tree
 * @param fileStream file stream for displaying tree traversal output
 * @param rootNode root node of DOM tree
 * @param currentNode current node for traversal
 * @param treeActionCode traversal action type
 */
void mozXMLTermSession::TraverseDOMTree(FILE* fileStream,
                                 nsIDOMNode* rootNode,
                                 nsCOMPtr<nsIDOMNode>& currentNode,
                                 TreeActionCode treeActionCode)
{
  static const PRInt32 NODE_TYPE_NAMES = 12;

  static const char* const nodeTypeNames[NODE_TYPE_NAMES] = {
    "ELEMENT",
    "ATTRIBUTE",
    "TEXT",
    "CDATA_SECTION",
    "ENTITY_REFERENCE",
    "ENTITY_NODE",
    "PROCESSING_INSTRUCTION",
    "COMMENT",
    "DOCUMENT",
    "DOCUMENT_TYPE",
    "DOCUMENT_FRAGMENT",
    "NOTATION_NODE"
  };

  static const PRInt32 PRINT_ATTRIBUTE_NAMES = 2;

  static const char* const printAttributeNames[PRINT_ATTRIBUTE_NAMES] = {
    "class",
    "id"
  };

  nsresult result = NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMNode> moveNode(nsnull);
  nsCOMPtr<nsIDOMNamedNodeMap> namedNodeMap(nsnull);

  switch (treeActionCode) {
  case TREE_MOVE_UP:
    if (currentNode.get() != rootNode) {
      result = currentNode->GetParentNode(getter_AddRefs(moveNode));

      if (NS_SUCCEEDED(result) && moveNode) {
        // Move up to parent node
        currentNode = moveNode;
      }

    } else {
      fprintf(fileStream, "TraverseDOMTree: already at the root node \n");
    }
    break;

  case TREE_MOVE_DOWN:
    result = currentNode->GetFirstChild(getter_AddRefs(moveNode));

    if (NS_SUCCEEDED(result) && moveNode) {
      // Move down to child node
      currentNode = moveNode;
    } else {
      fprintf(fileStream, "TraverseDOMTree: already at a leaf node\n");
    }
    break;

  case TREE_MOVE_LEFT:
    if (currentNode.get() != rootNode) {
      result = currentNode->GetPreviousSibling(getter_AddRefs(moveNode));

      if (NS_SUCCEEDED(result) && moveNode) {
        // Move to previous sibling node
        currentNode = moveNode;
      } else {
        fprintf(fileStream, "TraverseDOMTree: already at leftmost node\n");
      }
    } else {
      fprintf(fileStream, "TraverseDOMTree: already at the root node \n");
    }
    break;

  case TREE_MOVE_RIGHT:
    if (currentNode.get() != rootNode) {
      result = currentNode->GetNextSibling(getter_AddRefs(moveNode));

      if (NS_SUCCEEDED(result) && moveNode) {
        // Move to next sibling node
        currentNode = moveNode;
      } else {
        fprintf(fileStream, "TraverseDOMTree: already at rightmost node\n");
      }
    } else {
      fprintf(fileStream, "TraverseDOMTree: already at the root node \n");
    }
    break;

  case TREE_PRINT_ATTS:
  case TREE_PRINT_HTML:
    if (PR_TRUE) {
      nsAutoString indentString; indentString.SetLength(0);
      nsAutoString htmlString;
      ToHTMLString(currentNode, indentString, htmlString,
                   (PRBool) (treeActionCode == TREE_PRINT_HTML) );

      fprintf(fileStream, "%s:\n", treeActionNames[treeActionCode-1]);

      char* htmlCString = ToNewCString(htmlString);
      fprintf(fileStream, "%s", htmlCString);
      nsCRT::free(htmlCString);

      fprintf(fileStream, "\n");
    }
    break;

  default:
    fprintf(fileStream, "mozXMLTermSession::TraverseDOMTree - unknown action %d\n",
            treeActionCode);
  }

  if (NS_SUCCEEDED(result) && moveNode) {
    PRUint16 nodeType = 0;

    moveNode->GetNodeType(&nodeType);
    fprintf(fileStream, "%s%s: ", treeActionNames[treeActionCode-1],
                                  nodeTypeNames[nodeType-1]);

    nsCOMPtr<nsIDOMElement> domElement;
    domElement = do_QueryInterface(moveNode);
    if (domElement) {
      nsAutoString tagName; tagName.SetLength(0);

      result = domElement->GetTagName(tagName);
      if (NS_SUCCEEDED(result)) {
        char* tagCString = ToNewCString(tagName);
        fprintf(fileStream, "%s", tagCString);
        nsCRT::free(tagCString);

        // Print selected attribute values
        int j;
        for (j=0; j<PRINT_ATTRIBUTE_NAMES; j++) {
          nsAutoString attName; attName.AssignASCII (printAttributeNames[j]);
          nsAutoString attValue;
	  attValue.SetLength(0);

          result = domElement->GetAttribute(attName, attValue);
          if (NS_SUCCEEDED(result) && !attValue.IsEmpty()) {
            // Print attribute value
            char* tagCString2 = ToNewCString(attValue);
            fprintf(fileStream, " %s=%s", printAttributeNames[j], tagCString2);
            nsCRT::free(tagCString2);
          }
        }
      }
    }
    fprintf(fileStream, "\n");
  }
}
