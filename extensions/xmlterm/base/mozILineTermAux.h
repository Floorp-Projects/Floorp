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

// mozILineTermAux.h: auxiliary interface for LineTerm (not XPCONNECTed)
// This adds some XPCOM-only methods to the XPCOM/XPCONNECT interface
// mozILineTerm (unregistered)

#ifndef mozILineTermAux_h___
#define mozILineTermAux_h___

#include "nsISupports.h"

#include "nscore.h"

#include "nsIObserver.h"
#include "mozILineTerm.h"

/* {0eb82b10-43a2-11d3-8e76-006008948af5} */
#define MOZILINETERMAUX_IID_STR "0eb82b10-43a2-11d3-8e76-006008948af5"
#define MOZILINETERMAUX_IID \
  {0x0eb82b10, 0x43a2, 0x11d3, \
    { 0x8e, 0x76, 0x00, 0x60, 0x08, 0x94, 0x8a, 0xf5 }}

class mozILineTermAux : public mozILineTerm {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(MOZILINETERMAUX_IID)

  // mozILineTerm interface
  NS_IMETHOD Open(const PRUnichar *command,
                  const PRUnichar *promptRegexp,
                  PRInt32 options, PRInt32 processType,
                  nsIDOMDocument *domDoc) = 0;

  NS_IMETHOD Close(const PRUnichar* aCookie) = 0;

  NS_IMETHOD Write(const PRUnichar *buf, const PRUnichar* aCookie) = 0;

  NS_IMETHOD Read(PRInt32 *opcodes, PRInt32 *opvals,
                  PRInt32 *buf_row, PRInt32 *buf_col,
                  const PRUnichar* aCookie,
                  PRUnichar **_retval) = 0;

  // mozILineTermAux interface add ons
  // (not scriptable, no authentication cookie required)

  /** Opens LineTerm, a line-oriented terminal interface (without graphics)
   * @param command name of command to be executed; usually a shell,
   *        e.g., "/bin/sh"; if set to null string, the command name is
   *        determined from the environment variable SHELL
   * @param promptRegexp command prompt regular expression (for future use);
   *        at the moment, any string terminated by one of the characters
   *        "#$%>?", followed by a space, is assumed to be a prompt
   * @param options LineTerm option bits (usually 0; see lineterm.h)
   * @param processType command shell type; if set to -1, type is determined
   *        from the command name
   * @param domDoc DOM document object associated with the LineTerm
   *        (document.cookie will be defined for this document on return)
   * @param aCookie (output) cookie associated with LineTerm
   */
  NS_IMETHOD OpenAux(const PRUnichar *command,
                     const PRUnichar *promptRegexp,
                     PRInt32 options, PRInt32 processType,
                     nsIDOMDocument *domDoc,
                     nsIObserver* anObserver,
                     nsString& aCookie) = 0;

  /** Suspend/restores LineTerm operation
   * @param aSuspend suspension state flag
   */
  NS_IMETHOD SuspendAux(PRBool aSuspend) = 0;

  /** Closes LineTerm
   */
  NS_IMETHOD CloseAux(void) = 0;

  /** Close all LineTerms, not just this one
   */
  NS_IMETHOD CloseAllAux(void) = 0;

  /** Resizes XMLterm to match a resized window.
   * @param nRows number of rows
   * @param nCols number of columns
   */
  NS_IMETHOD ResizeAux(PRInt32 nRows, PRInt32 nCols) = 0;

  /** Read output data and style strings and parameters from LineTerm
   * @param opcodes (output) output data descriptor bits (see lineterm.h)
   * @param opvals (output) output data value(s)
   * @param buf_row (output) row number (>=-1)
   *                (-1 denotes line mode and 0 represents bottom row)
   * @param buf_col (output) column number (>=0)
   * @param _retval (output) success code
   * @param retstyle (output) output style string
   * @return output data string from LineTerm
   */
  NS_IMETHOD ReadAux(PRInt32 *opcodes, PRInt32 *opvals,
                     PRInt32 *buf_row, PRInt32 *buf_col,
                     PRUnichar **_retval, PRUnichar **retstyle) = 0;

  NS_IMETHOD GetCookie(nsString& aCookie) = 0;

  NS_IMETHOD GetCursorRow(PRInt32 *aCursorRow) = 0;
  NS_IMETHOD SetCursorRow(PRInt32 aCursorRow) = 0;

  NS_IMETHOD GetCursorColumn(PRInt32 *aCursorColumn) = 0;
  NS_IMETHOD SetCursorColumn(PRInt32 aCursorColumn) = 0;

  NS_IMETHOD GetEchoFlag(PRBool *aEchoFlag) = 0;
  NS_IMETHOD SetEchoFlag(PRBool aEchoFlag) = 0;
};

// Factory for mozILineTermAux

extern nsresult
NS_NewLineTermAux(mozILineTermAux** aLineTermAux);

#endif /* mozILineTermAux_h___ */
