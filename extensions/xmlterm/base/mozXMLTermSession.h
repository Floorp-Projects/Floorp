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

// mozXMLTermSession.h: class to manage session input/output display
// NOTE: This class is getting too unwieldy. It should be modularized,
//       using a separate class for entries, for example, once the dust
//       settles.

#include "nscore.h"
#include "prlog.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIGenericFactory.h"
#include "nsIPresShell.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"

#include "nsIContent.h"

#include "mozXMLT.h"
#include "mozILineTerm.h"
#include "mozIXMLTerminal.h"
#include "mozIXMLTermStream.h"


class mozXMLTermSession
{
  public:

  mozXMLTermSession();
  virtual ~mozXMLTermSession();

  /** Initializes XMLterm session
   * @param aXMLTerminal containing XMLTerminal object
   * @param aPresShell presentation shell associated with XMLterm
   * @param aDOMDocument DOM document associated with XMLterm
   * @param nRows initial number of rows
   * @param nCols initial number of columns
   */
  NS_IMETHOD Init(mozIXMLTerminal* aXMLTerminal,
                  nsIPresShell* aPresShell,
                  nsIDOMDocument* aDOMDocument,
                  PRInt32 nRows, PRInt32 nCols);

  /** Finalizes (closes) session
   */
  NS_IMETHOD Finalize(void);

  /** Resizes XMLterm to match a resized window.
   * @param lineTermAux LineTermAux object to be resized (may be null)
   */
  NS_IMETHOD Resize(mozILineTermAux* lineTermAux);

  /** Preprocesses user input before it is transmitted to LineTerm
   * @param aString (inout) input data to be preprocessed
   * @param consumed (output) PR_TRUE if input data has been consumed
   * @param checkSize (output) PR_TRUE if terminal size needs to be checked
   */
  NS_IMETHOD Preprocess(const nsString& aString,
                        PRBool& consumed,
                        PRBool& checkSize);

  /** Reads all available data from LineTerm and displays it;
   * returns when no more data is available.
   * @param lineTermAux LineTermAux object to read data from
   * @param processedData (output) PR_TRUE if any data was processed
   */
  NS_IMETHOD ReadAll(mozILineTermAux* lineTermAux, PRBool& processedData);

  /** Exports HTML to file, with META REFRESH, if refreshSeconds is non-zero.
   * Nothing is done if display has not changed since last export, unless
   * forceExport is true. Returns true if export actually takes place.
   * If filename is a null string, HTML is written to STDERR.
   */
  NS_IMETHOD ExportHTML(const PRUnichar* aFilename,
                        PRInt32 permissions,
                        const PRUnichar* style,
                        PRUint32 refreshSeconds,
                        PRBool forceExport,
                        PRBool* exported);

  /** Aborts session by closing LineTerm and displays an error message
   * @param lineTermAux LineTermAux object to be closed
   * @param abortCode abort code string to be displayed
   */
  NS_IMETHOD Abort(mozILineTermAux* lineTermAux,
                   nsString& abortCode);

  /** Gets current entry (command) number
   * @param aNumber (output) current entry number
   */
  NS_IMETHOD GetCurrentEntryNumber(PRInt32 *aNumber);

  /** Gets command history buffer count
   * @param aHistory (output) history buffer count
   */
  NS_IMETHOD GetHistory(PRInt32 *aHistory);

  /** Sets command history buffer count
   * @param aHistory history buffer count
   */
  NS_IMETHOD SetHistory(PRInt32 aHistory);

  /** Gets prompt string
   * @param aPrompt (output) prompt string
   */
  NS_IMETHOD GetPrompt(PRUnichar **aPrompt);

  /** Sets prompt string
   * @param aPrompt prompt string
   */
  NS_IMETHOD SetPrompt(const PRUnichar* aPrompt);

  /** Gets webcast filename
   * @param aFilename (output) webcast filename
   */
  NS_IMETHOD GetScreenMode(PRBool* aFlag);

protected:

  /** ShellML element type (see ShellML documentation),
   * implemented as CLASS attribute of HTML elements
   */
  enum SessionElementType {
    SESSION_ELEMENT = 0,
    ENTRY_ELEMENT,
    INPUT_ELEMENT,
    OUTPUT_ELEMENT,
    PROMPT_ELEMENT,
    COMMAND_ELEMENT,
    STDIN_ELEMENT,
    STDOUT_ELEMENT,
    STDERR_ELEMENT,
    MIXED_ELEMENT,
    WARNING_ELEMENT,
    SESSION_ELEMENT_TYPES
  };

  /** allowed user-generated input event type */
  enum SessionEventType {
    CLICK_EVENT = 0,
    SESSION_EVENT_TYPES
  };

  /** output type */
  enum OutputType {
    LINE_OUTPUT = 0,
    SCREEN_OUTPUT,
    STREAM_OUTPUT,
    OUTPUT_TYPES
  };

  /** display style of output */
  enum OutputDisplayType {
    NO_NODE          = 0,
    PRE_STDOUT_NODE,
    PRE_STDERR_NODE,
    PRE_STDIN_NODE,
    DIV_MIXED_NODE,
    SPAN_DUMMY_NODE,
    OUTPUT_DISPLAY_TYPES
  };

  /** markup style of output */
  enum OutputMarkupType {
    PLAIN_TEXT = 0,
    TEXT_FRAGMENT,
    JS_FRAGMENT,
    HTML_FRAGMENT,
    HTML_DOCUMENT,
    XML_DOCUMENT,
    INSECURE_FRAGMENT,
    OVERFLOW_FRAGMENT,
    INCOMPLETE_FRAGMENT,
    OUTPUT_MARKUP_TYPES
  };

  /** settings for automatic markup detection */
  enum AutoDetectOption {
    NO_MARKUP = 0,
    FIRST_LINE,
    ANY_LINE
  };

  /** type of output flush action */
  enum FlushActionType {
    DISPLAY_INCOMPLETE_FLUSH = 0,
    SPLIT_INCOMPLETE_FLUSH,
    CLEAR_INCOMPLETE_FLUSH,
    CLOSE_INCOMPLETE_FLUSH
  };

  /** type of currently active meta command */
  enum MetaCommandType {
    NO_META_COMMAND    = 0,
    DEFAULT_META_COMMAND,
    HTTP_META_COMMAND,
    JS_META_COMMAND,
    TREE_META_COMMAND,
    LS_META_COMMAND,
    META_COMMAND_TYPES
  };

  /** file type for directory display (TEMPORARY) */
  enum FileType {
    PLAIN_FILE      = 0,
    DIRECTORY_FILE,
    EXECUTABLE_FILE,
    FILE_TYPES
  };

  /** action code for navigating XMLterm DOM document */
  enum TreeActionCode {
    TREE_MOVE_UP      = 1,
    TREE_MOVE_DOWN    = 2,
    TREE_MOVE_LEFT    = 3,
    TREE_MOVE_RIGHT   = 4,
    TREE_PRINT_ATTS   = 5,
    TREE_PRINT_HTML   = 6,
    TREE_ACTION_CODES = 7
  };

  /** Displays ("echoes") input text string with style and positions cursor
   * @param aString string to be displayed
   * @param aStyle style values for string (see lineterm.h)
   * @param cursorCol cursor column
   */
  NS_IMETHOD DisplayInput(const nsString& aString,
                          const nsString& aStyle,
                          PRInt32 cursorCol);

  /** Autodetects markup in current output line
   * @param aString string to be displayed
   * @param firstOutputLine PR_TRUE if this is the first output line
   * @param secure PR_TRUE if output data is secure
   *               (usually PR_TRUE for metacommand output only)
   */
  NS_IMETHOD AutoDetectMarkup(const nsString& aString,
                              PRBool firstOutputLine,
                              PRBool secure);

  /** Initializes display of stream output with specified markup type
   * @param streamURL effective URL of stream output
   * @param streamMarkupType stream markup type
   * @param streamIsSecure PR_TRUE if stream is secure
   */
  NS_IMETHOD InitStream(const nsString& streamURL,
                        OutputMarkupType streamMarkupType,
                        PRBool streamIsSecure);

  /** Breaks output display by flushing and deleting incomplete lines */
  NS_IMETHOD BreakOutput(PRBool positionCursorBelow);

  /** Processes output string with specified style
   * @param aString string to be processed
   * @param aStyle style values for string (see lineterm.h)
   *               (if it is a null string, STDOUT style is assumed)
   * @param newline PR_TRUE if this is a complete line of output
   * @param streamOutput PR_TRUE if string represents stream output
   */
  NS_IMETHOD ProcessOutput(const nsString& aString,
                           const nsString& aStyle,
                           PRBool newline,
                           PRBool streamOutput);

  /** Ensures the total number of output lines stays within a limit
   * by deleting the oldest output line.
   * @param deleteAllOld if PR_TRUE, delete all previous display nodes
   *                     (excluding the current one)
   */
  NS_IMETHOD LimitOutputLines(PRBool deleteAllOld);

  /** Appends text string to output buffer
   *  (appended text may need to be flushed for it to be actually displayed)
   * @param aString string to be processed
   * @param aStyle style values for string (see lineterm.h)
   *               (if it is a null string, STDOUT style is assumed)
   * @param newline PR_TRUE if this is a complete line of output
   */
  NS_IMETHOD AppendOutput(const nsString& aString,
                          const nsString& aStyle,
                          PRBool newline);

  /** Adds markup to LS output (TEMPORARY)
   * @param aString string to be processed
   * @param aStyle style values for string (see lineterm.h)
   *               (if it is a null string, STDOUT style is assumed)
   */
  NS_IMETHOD AppendLineLS(const nsString& aString,
                          const nsString& aStyle);

  /** Inserts HTML fragment string as child of parentNode, before specified
   * child node, or after the last child node
   * @param aString HTML fragment string to be inserted
   * @param parentNode parent node for HTML fragment
   * @param entryNumber entry number (default value = -1)
   *                   (if entryNumber >= 0, all '#' characters in aString
   *                    are substituted by entryNumber)
   * @param beforeNode child node before which to insert fragment;
   *                   if null, insert after last child node
   *                   (default value is null)
   * @param replace if PR_TRUE, replace beforeNode with inserted fragment
   *                (default value is PR_FALSE)
   */
  NS_IMETHOD InsertFragment(const nsString& aString,
                            nsIDOMNode* parentNode,
                            PRInt32 entryNumber = -1,
                            nsIDOMNode* beforeNode = nsnull,
                            PRBool replace = PR_FALSE);

  /** Substitute all occurrences of the '#' character in aString with
   * aNumber, if aNumber >= 0;
   * @ param aString string to be modified
   * @ param aNumber number to substituted
   */
  void SubstituteCommandNumber(nsString& aString,
                               PRInt32 aNumber);

  /** Sanitize event handler attribute values by imposing syntax checks.
   * @param aAttrValue attribute value to be sanitized
   * @param aEventName name of event being handled ("Click", "DblClick", ...)
   */
  void SanitizeAttribute(nsString& aAttrValue,
                         const char* aEventName);

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
  NS_IMETHOD DeepSanitizeFragment(nsCOMPtr<nsIDOMNode>& domNode,
                                  nsIDOMNode* parentNode,
                                  PRInt32 entryNumber);

  /** Deep refresh of selected event handler attributes for DOM elements
   * (WORKAROUND for inserting HTML fragments properly)
   * @param domNode DOM node of branch to be refreshed
   */
  NS_IMETHOD DeepRefreshEventHandlers(nsCOMPtr<nsIDOMNode>& domNode);

  /** Forces display of data in output buffer
   * @param flushAction type of flush action: display, split-off, clear, or
   *                                          close incomplete lines
   */
  NS_IMETHOD FlushOutput(FlushActionType flushAction);

  /** Positions cursor below the last output element */
  void PositionOutputCursor(mozILineTermAux* lineTermAux);

  /** Scrolls document to align bottom and left margin with screen */
  NS_IMETHOD ScrollToBottomLeft(void);

  /** Create a DIV element with attributes NAME="preface", CLASS="preface",
   * and ID="preface0", containing an empty text node, and append it as a
   * child of the main BODY element. Also make it the current display element.
   */
  NS_IMETHOD NewPreface(void);

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
  NS_IMETHOD NewEntry(const nsString& aPrompt);

  /** Create a DIV element with attributes NAME="screen" and CLASS="screen",
   * containing an empty text node, and append it as a
   * child of the main BODY element. Also make it the current display element.
   */
  NS_IMETHOD NewScreen(void);

  /** Returns DOM PRE node corresponding to specified screen row
   */
  NS_IMETHOD GetRow(PRInt32 aRow, nsIDOMNode** aRowNode);

  /** Positions cursor to specified screen row/col position
   */
  NS_IMETHOD PositionScreenCursor(PRInt32 aRow, PRInt32 aCol);

  /** Create a PRE element with attributes NAME="row", CLASS="row",
   * containing an empty text node, and insert it as a
   * child of the SCREEN element before beforeRowNode, or at the
   * end if beforeRowNode is null.
   */
  NS_IMETHOD NewRow(nsIDOMNode* beforeRowNode,
                    nsIDOMNode** resultNode);

  /** Displays screen output string with specified style
   * @param aString string to be processed
   * @param aStyle style values for string (see lineterm.h)
   *               (if it is a null string, STDOUT style is assumed)
   * @param aRow row in which to insert string
   */
  NS_IMETHOD DisplayRow(const nsString& aString, const nsString& aStyle,
                        PRInt32 aRow);

  /** Append a BR element as the next child of specified parent.
   * @param parentNode parent node for BR element
   */
  NS_IMETHOD NewBreak(nsIDOMNode* parentNode);

  /** Creates an empty block element with tag name tagName with attributes
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
  NS_IMETHOD NewElementWithText(const nsString& tagName,
                                const nsString& name, PRInt32 number,
                                nsIDOMNode* parentNode,
                                nsCOMPtr<nsIDOMNode>& blockNode,
                                nsCOMPtr<nsIDOMNode>& textNode,
                                nsIDOMNode* beforeNode = nsnull);

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
  NS_IMETHOD NewAnchor(const nsString& classAttribute,
                       PRInt32 number,
                       nsIDOMNode* parentNode,
                       nsCOMPtr<nsIDOMNode>& anchorNode);

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
  NS_IMETHOD NewElement(const nsString& tagName,
                        const nsString& name, PRInt32 number,
                        nsIDOMNode* parentNode,
                        nsCOMPtr<nsIDOMNode>& blockNode,
                        nsIDOMNode* beforeNode = nsnull);

  /** Creates a new DOM text node, and appends it as a child of the
   * specified parent.
   * @param parentNode parent node for element
   * @param textNode (output) created text DOM node
   */
  NS_IMETHOD NewTextNode( nsIDOMNode* parentNode,
                          nsCOMPtr<nsIDOMNode>& textNode);

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
  NS_IMETHOD NewIFrame(nsIDOMNode* parentNode,
                       PRInt32 number,
                       PRInt32 frameBorder,
                       const nsString& src,
                       const nsString& width,
                       const nsString& height);

  /** Add event attributes (onclick, ondblclick, ...) to DOM node
   * @param name name of DOM node (supplied as argument to the event handler)
   * @param number entry number (supplied as argument to the event handler)
   * @param domNode DOM node to be modified 
   */
  NS_IMETHOD SetEventAttributes(const nsString& name,
                                PRInt32 number,
                                nsCOMPtr<nsIDOMNode>& domNode);

  /** Sets text content of a DOM node to supplied string
   * @param textNode DOM text node to be modified
   * @param aString string to be inserted
   */
  NS_IMETHOD SetDOMText(nsCOMPtr<nsIDOMNode>& textNode,
                        const nsString& aString);

  /** Checks if node is a text node
   * @param aNode DOM node to be checked
   * @return PR_TRUE if node is a text node
   */
  PRBool IsTextNode(nsIDOMNode *aNode);

  /** Checks if node is a text, span, or anchor node
   * (i.e., allowed inside a PRE element)
   * @param aNode DOM node to be checked
   * @return PR_TRUE if node is a text, span or anchor node
   */
  PRBool IsPREInlineNode(nsIDOMNode* aNode);

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
  NS_IMETHOD ToHTMLString(nsIDOMNode* aNode,
                          nsString& indentString,
                          nsString& htmlString,
                          PRBool deepContent = PR_FALSE,
                          PRBool insidePRENode = PR_FALSE);

  /** Implements the "tree:" meta command to traverse DOM tree
   * @param fileStream file stream for displaying tree traversal output
   * @param rootNode root node of DOM tree
   * @param currentNode current node for traversal
   * @param treeActionCode traversal action type
   */
  void TraverseDOMTree(FILE* fileStream,
                       nsIDOMNode* rootNode,
                       nsCOMPtr<nsIDOMNode>& currentNode,
                       TreeActionCode treeActionCode);

  /** names of session elements */
  static const char* const sessionElementNames[SESSION_ELEMENT_TYPES];

  /** names of session events */
  static const char* const sessionEventNames[SESSION_EVENT_TYPES];

  /** names of meta commands */
  static const char* const metaCommandNames[META_COMMAND_TYPES];

  /** names of file types (TEMPORARY) */
  static const char* const fileTypeNames[FILE_TYPES];

  /** names of tree traversal action types */
  static const char* const treeActionNames[TREE_ACTION_CODES];

  /** object initialization flag */
  PRBool mInitialized;


  /** non-owning reference to containing XMLTerminal object */
  mozIXMLTerminal*   mXMLTerminal;               // non-owning reference

  /** non-owning reference to presentation shell associated with XMLterm */
  nsIPresShell*      mPresShell;                 // non-owning reference (??)

  /** non-owning reference to DOM document containing XMLterm */
  nsIDOMDocument*    mDOMDocument;               // non-owning reference (??)


  /** BODY node of document containing XMLterm */
  nsCOMPtr<nsIDOMNode> mBodyNode;

  /** XMLterm menus node */
  nsCOMPtr<nsIDOMNode> mMenusNode;

  /** XMLterm session node */
  nsCOMPtr<nsIDOMNode> mSessionNode;

  /** current debug node (TEMPORARY; used for tree traversal) */
  nsCOMPtr<nsIDOMNode> mCurrentDebugNode;


  /** starting entry node in the history list */
  nsCOMPtr<nsIDOMNode> mStartEntryNode;

  /** current (and last) entry node in the history list */
  nsCOMPtr<nsIDOMNode> mCurrentEntryNode;

  /** maximum number of commands allowed to be saved in history list */
  PRInt32              mMaxHistory;

  /** entry number of first entry in history list */
  PRInt32              mStartEntryNumber;

  /** entry number of current entry */
  PRInt32              mCurrentEntryNumber;

  /** flag indicating whether current entry has output data */
  PRBool               mEntryHasOutput;

  /** text node terminating current command prompt */
  nsCOMPtr<nsIDOMNode> mPromptTextNode;

  /** span node for current command input */
  nsCOMPtr<nsIDOMNode> mCommandSpanNode;

  /** text node for current command input */
  nsCOMPtr<nsIDOMNode> mInputTextNode;


  /** block-level node for current command output */
  nsCOMPtr<nsIDOMNode> mOutputBlockNode;

  /** current display element node for command output */
  nsCOMPtr<nsIDOMNode> mOutputDisplayNode;

  /** current text node for command output */
  nsCOMPtr<nsIDOMNode> mOutputTextNode;

  /** line offset of current text node for command output */
  PRInt32 mOutputTextOffset;

  /** current XMLTerm stream interface for stream display */
  nsCOMPtr<mozIXMLTermStream> mXMLTermStream;


  /** currently active output type */
  OutputType    mOutputType;

  /** currently active display style of output */
  OutputDisplayType    mOutputDisplayType;

  /** currently active markup style of output */
  OutputMarkupType     mOutputMarkupType;


  /** currently active meta command (if any) */
  MetaCommandType      mMetaCommandType;

  /** currently active setting for automatic markup detection */
  AutoDetectOption     mAutoDetect;


  /** flag marking the first line of output */
  PRBool               mFirstOutputLine;


  /** total count of PRE/mixed text lines displayed for this entry */
  PRInt32              mEntryOutputLines;

  /** count of PRE complete text lines in buffer */
  PRInt32              mPreTextBufferLines;

  /** buffer for incomplete line of PRE text */
  nsString             mPreTextIncomplete;

  /** buffer for complete lines of PRE text */
  nsString             mPreTextBuffered;

  /** copy of PRE text already displayed */
  nsString             mPreTextDisplayed;


  /** Screen element */
  nsCOMPtr<nsIDOMNode> mScreenNode;

  /** Number of rows in screen */
  PRInt32 mScreenRows;

  /** Number of columns in screen */
  PRInt32 mScreenCols;

  /** Top scrolling row */
  PRInt32 mTopScrollRow;

  /** Bottom scrolling row */
  PRInt32 mBotScrollRow;


  /** restore input echo flag */
  PRBool               mRestoreInputEcho;


  /** count of exported HTML */
  PRInt32  mCountExportHTML;

  /** last exported HTML */
  nsString mLastExportHTML;


  /** shell prompt string */
  nsString             mShellPrompt;

  /** prompt string (HTML) */
  nsString             mPromptHTML;

  /** buffer for HTML/XML fragment streams */
  nsString             mFragmentBuffer;

};
