/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

// mozIXMLTerminal.h: primary interface for XMLterm operations
//                    (unregistered)

#ifndef mozIXMLTerminal_h___
#define mozIXMLTerminal_h___

#include "nscore.h"

#include "nsISupports.h"
#include "nsIDOMDocument.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIScriptContext.h"

#include "mozIXMLTermShell.h"

/* {0eb82b20-43a2-11d3-8e76-006008948af5} */
#define MOZIXMLTERMINAL_IID_STR "0eb82b20-43a2-11d3-8e76-006008948af5"
#define MOZIXMLTERMINAL_IID \
  {0x0eb82b20, 0x43a2, 0x11d3, \
    { 0x8e, 0x76, 0x00, 0x60, 0x08, 0x94, 0x8a, 0xf5 }}

class mozIXMLTerminal : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(MOZIXMLTERMINAL_IID)

  /** Initializes XMLterm in specified web shell
   * @param aDocShell web shell in which to embed XMLterm
   * @param aXMLTermShell scriptable wrapper shell for XMLterm
   * @param URL URL of document to be loaded in the window
   *            (set to null string if document is already loaded in window)
   * @param args argument string to be passed to XMLterm
   *             (at the moment this just contains any initial input data)
   */
  NS_IMETHOD Init(nsIDocShell* aDocShell,
                  mozIXMLTermShell* aXMLTermShell,
                  const PRUnichar* aURL,
                  const PRUnichar* args) = 0;

  /** Finalizes (closes) XMLterm
   */
  NS_IMETHOD Finalize(void) = 0;

  /** Polls for readable data from XMLterm
   */
  NS_IMETHOD Poll(void) = 0;

  /** Gets current entry (command) number
   * @param aNumber (output) current entry number
   */
  NS_IMETHOD GetCurrentEntryNumber(PRInt32 *aNumber) = 0;

  /** Gets command history buffer count
   * @param aHistory (output) history buffer count
   */
  NS_IMETHOD GetHistory(PRInt32 *aHistory) = 0;

  /** Sets command history buffer count
   * @param aHistory history buffer count
   */
  NS_IMETHOD SetHistory(PRInt32 aHistory) = 0;

  /** Gets prompt string
   * @param aPrompt (output) prompt string
   */
  NS_IMETHOD GetPrompt(PRUnichar **aPrompt) = 0;

  /** Sets prompt string
   * @param aPrompt prompt string
   */
  NS_IMETHOD SetPrompt(const PRUnichar* aPrompt) = 0;

  /** Gets ignore key press flag
   * @param aIgnore ignore key press flag
   */
  NS_IMETHOD GetKeyIgnore(PRBool* aIgnore) = 0;

  /** Sets ignore key press flag
   * @param aIgnore ignore key press flag
   */
  NS_IMETHOD SetKeyIgnore(const PRBool aIgnore) = 0;

  /** Writes string to terminal as if the user had typed it (without authenitcation)
   * @param aString string to be transmitted to terminal
   */
  NS_IMETHOD SendTextAux(const nsString& aString) = 0;

  /** Writes string to terminal as if the user had typed it (command input)
   * @param aString string to be transmitted to terminal
   * @param aCookie document.cookie string for authentication
   */
  NS_IMETHOD SendText(const nsString& aString, const PRUnichar* aCookie) = 0;

  /** Paste data from clipboard into XMLterm at current input line cursor location
   */
  NS_IMETHOD Paste() = 0;

  /** Gets document associated with XMLterm
   * @param aDoc (output) DOM document
   */
  NS_IMETHOD GetDocument(nsIDOMDocument** aDoc) = 0;

  /** Gets web shell associated with XMLterm
   * @param aDocShell (output) web shell
   */
  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell) = 0;

  /** Gets presentation shell associated with XMLterm
   * @param aPresShell (output) presentation shell
   */
  NS_IMETHOD GetPresShell(nsIPresShell** aPresShell) = 0;

  /** Gets DOM document associated with XMLterm
   * @param aDOMDocumentl (output) DOM document
   */
  NS_IMETHOD GetDOMDocument(nsIDOMDocument** aDOMDocument) = 0;

  /** Gets selection controller associated with XMLterm
   * @param aSelectionControllerl (output) DOM document
   */
  NS_IMETHOD GetSelectionController(nsISelectionController** aSelectionController) = 0;

  /** Gets flag denoting whether terminal is in full screen mode
   * @param aFlag (output) screen mode flag
   */
  NS_IMETHOD GetScreenMode(PRBool* aFlag) = 0;

  /** Checks if supplied cookie is valid for XMLTerm
   * @param aCookie supplied cookie string
   * @param _retval PR_TRUE if supplied cookie matches XMLTerm cookie
   */
  NS_IMETHOD MatchesCookie(const PRUnichar* aCookie, PRBool *_retval) = 0;

  /** Resizes XMLterm to match a resized window.
   */
  NS_IMETHOD Resize(void) = 0;

  /** Shows the caret and make it editable.
   */
  NS_IMETHOD ShowCaret(void) = 0;

  /** Returns current screen size in rows/cols and in pixels
   * @param (output) rows
   * @param (output) cols
   * @param (output) xPixels
   * @param (output) yPixels
   */
  NS_IMETHOD ScreenSize(PRInt32& rows, PRInt32& cols,
                        PRInt32& xPixels, PRInt32& yPixels) = 0;
};

#define MOZXMLTERMINAL_CID                       \
{ /* 0eb82b21-43a2-11d3-8e76-006008948af5 */ \
   0x0eb82b21, 0x43a2, 0x11d3,               \
{0x8e, 0x76, 0x00, 0x60, 0x08, 0x94, 0x8a, 0xf5} }
extern nsresult
NS_NewXMLTerminal(mozIXMLTerminal** aXMLTerminal);

#endif /* mozIXMLTerminal_h___ */
