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
 * The Original Code is lineterm.
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

/* lineterm.h: Line terminal (LTERM) public interface header file
 * LINETERM provides a "stream" interface to an XTERM-like terminal,
 * using line-oriented input/output.
 */

#ifndef _LINETERM_H

#define _LINETERM_H   1

#include "unistring.h"

/* Define LTERM read callback function type */
#ifdef USE_GTK_WIDGETS
#include <gtk/gtk.h>
typedef void (*lterm_callback_func_t)(gpointer, gint, GdkInputCondition);
#else
typedef void* lterm_callback_func_t;
#endif

/* Unicode character style information (same type as UNICHAR) */
typedef UNICHAR UNISTYLE;

#ifdef  __cplusplus
extern "C" {
#endif

/* lineterm functions */

/* LTERM module number (used for trace/log operations) */
#define LTERM_TLOG_MODULE  1

/** Initializes all LTERM operations;
 * needs to be called before any calls to lterm_new.
 * @return 0 on success, or -1 on error.
 *
 * MESSAGELEVEL specifies the diagnostic message display level:
 *  0 => (normal) only fatal errors cause diagnostic messages to be printed.
 * -1 => (silent) no diagnostic messages are printed, even for fatal errors.
 *  1 => (warning) print non-fatal warning messages as well as error messages.
 * >9 and <= 99 (debugging)
 *       print debugging messages at selected procedure levels/sublevels
 * (See tracelog.h for more information on messageLevel)
 *
 * Returns 0 on successful initialization, -1 otherwise.
 */

int lterm_init(int messageLevel);


/** Creates a new LTERM object and returns its descriptor index but
 *  does not open it for I/O
 * (documented in the LTERM interface)
 * @return lterm descriptor index (>= 0) on success, or
 *         -1 on error
 */

int lterm_new();

/** Opens line terminal indexed by LTERM for input/output and creates
 * a process attached to it to execute the command line contained in string
 * array ARGV.
 * Called from the adminstrative/output thread of LTERM.
 * @return 0 on success, or -1 on error.
 *
 * COOKIE contains a cookie string used for stream security. If it is null,
 *  or a null string, all streams are considered insecure.
 *  (only MAXCOOKIESTR-1 characters of the cookie string are used for checking)
 *
 * INIT_COMMAND contains an initialization command string, if any (not echoed)
 *
 * PROMPT_REGEXP contains a REGEXP string describing the command prompt.
 * (**NOTE** For the moment, only a list of prompt delimiters is accepted;
 *           a typical list of prompt delimiters would be "#$%>?")
 *
 * OPTIONS is a bitmask controlling the following options:
 * LTERM_NOCANONICAL_FLAG  disable TTY canonical mode
 * LTERM_NOEDIT_FLAG       disable input line editing
 * LTERM_NOCOMPLETION_FLAG disable command line completion
 * LTERM_NOMETA_FLAG       disable meta input
 * LTERM_NOMARKUP_FLAG     disable HTML/XML element processing in command line
 * LTERM_NOECHO_FLAG       disable TTY echo
 * LTERM_NOPTY_FLAG        do not use pseudo-TTY
 * LTERM_NONUL_FLAG        do not process any NUL characters (discard them)
 * LTERM_NOLINEWRAP_FLAG   disable line wrapping
 * LTERM_NOEXPORT_FLAG     disable export of current environment to new process
 * LTERM_STDERR_FLAG       enable use of separate STDERR
 * LTERM_PARTLINE_FLAG     enable returning of partial line output
 *
 * Notes:
 *  -LTERM_STDERR_FLAG, although implemented, does not work properly at all
 *  -LTERM_PARTLINE_FLAG is not yet implemented
 *
 * PROCESS_TYPE specifies the subordinate process type, if set to one
 * of the following:
 *   LTERM_DETERMINE_PROCESS
 *   LTERM_UNKNOWN_PROCESS
 *   LTERM_SH_PROCESS
 *   LTERM_KSH_PROCESS
 *   LTERM_BASH_PROCESS
 *   LTERM_CSH_PROCESS
 *   LTERM_TCSH_PROCESS
 * If it is set to LTERM_DETERMINE_PROCESS, the process type is determined
 * from the path name.
 *
 * ROWS, COLS contain the initial no. of rows/columns.
 * X_PIXELS, Y_PIXELS contain the initial screen size in pixels
 * (may be set to zero if screen size is unknown).
 *
 * CALLBACK_FUNC is a pointer to a GTK-style callback function,
 * or NULL for no callback.
 * The function is called whenever there is data available for
 * LTERM_READ to process, with CALLBACK_DATA as the data argument.
 * The callback function should call LTERM_READ immediately;
 * otherwise the callback function will be called repeatedly.
 * (The type LTERM_CALLBACK_FUNC_T is defined at the top of this include file)
 *
 * In canonical mode, no input line editing is permitted.
 * In editing mode, EMACS-style keyboard line editing commands are allowed.
 * In completion mode, incomplete command/file names are transmitted to
 * the subordinate process for complettion, as in TCSH.
 * Meta input refers to input lines that begin with a colon,
 * or with a "protocol" name followed by a colon, such as
 * "http: ...".
 * Meta input lines are not sent to the subordinate process, but simply
 * echoed as LTERM output through LTERM_READ for further processing.
 * If command line completion is not disabled, incomplete meta input may
 * also be echoed for completion. In this case, the completed meta input
 * should be supplied to the LTERM through a call to LTERM_WRITE as if the
 * user had entered it.
 */

int lterm_open(int lterm, char *const argv[],
               const char* cookie, const char* init_command,
               const UNICHAR* prompt_regexp, int options, int process_type,
               int rows, int cols, int x_pixels, int y_pixels,
               lterm_callback_func_t callback_func, void *callback_data);


/** Closes line terminal indexed by LTERM.
 * The default action is to block until active calls to lterm_write
 * and lterm_read to complete.
 * Called from the administrative/output thread of LTERM.
 * @return 0 on success, or -1 on error.
 */
int lterm_close(int lterm);


/** Deletes an LTERM object, closing it if necessary.
 * @return 0 on success, or -1 on error.
 */

int lterm_delete(int lterm);


/** Closes all LTERMs, but does not delete them.
 * This may be used to free any resources associated with LTERMs for clean up.
 * The closed LTERMs should still be deleted, if possible.
 */
void lterm_close_all(void);


/** Set input echo flag for line terminal indexed by LTERM.
 * Called from the output thread of LTERM.
 * @return 0 on success, or -1 on error.
 */

int lterm_setecho(int lterm, int echo_flag);


/** Resizes the line terminal indexed by LTERM to new row/column count.
 * Called from the output thread of LTERM.
 * @return 0 on success, or -1 on error.
 */

int lterm_resize(int lterm, int rows, int cols);


/** Sets cursor position in line terminal indexed by LTERM.
 * NOT YET IMPLEMENTED
 * Row numbers increase upward, starting from 0.
 * Column numbers increase rightward, starting from 0.
 * Called from the output thread of LTERM.
 * @return 0 on success, or -1 on error.
 */

int lterm_setcursor(int lterm, int row, int col);


/** Writes supplied to Unicode string in BUF of length COUNT to
 * line terminal indexed by LTERM.
 * (May be called from any thread, since it uses a pipe to communicate
 *  with the output thread.)
 * DATATYPE may be set to one of the following values:
 *   LTERM_WRITE_PLAIN_INPUT     Plain text user input
 *   LTERM_WRITE_XML_INPUT       XML element user input
 *   LTERM_WRITE_PLAIN_OUTPUT    Plain text server output
 *   LTERM_WRITE_CLOSE_MESSAGE   End of file message
 * NOTE: This is a non-blocking call
 * Returns the number of characters written.
 * Returns -1 on error, and
 *         -2 if pseudo-TTY has been closed.
 * If the return value is less than COUNT, it usually indicates an error.
 * If the return value is -1, any further operations on the LTERM,
 * other than LTERM_CLOSE, will always fail with an error return value.
 */

int lterm_write(int lterm, const UNICHAR *buf, int count, int dataType);


/** Completes meta input in line terminal indexed by LTERM with the
 * supplied to Unicode string in BUF of length COUNT.
 * Called from the output thread of the LTERM.
 * @return 0 on success, or -1 on error.
 */

int lterm_metacomplete(int lterm, const UNICHAR *buf, int count);


/** reads upto COUNT Unicode characters from a single line of output
 * from line terminal indexed by LTERM into BUF.
 * Called from the output thread of the LTERM.
 * Returns the number of characters read (>=0) on a successful read.
 * Returns -1 if an error occurred while reading,
 *         -2 if pseudo-TTY has been closed,
 *         -3 if more than COUNT characters are present in the line
 *            (in this case the first COUNT characters are returned in BUF,
 *             and the rest are discarded).
 * If the return value is -1, any further operations on the LTERM,
 * other than LTERM_CLOSE, will always fail with an error return value.
 * (If return value is -2, it means that the subordinate process has closed
 *  the pseudo-TTY. In this case, the LTERM still needs to be explicitly
 *  closed by calling LTERM_CLOSE for proper clean-up.)
 *
 * TIMEOUT is the number of platform-dependent time units
 * (usually milliseconds on Unix) to wait to read data.
 * A zero value implies no waiting.
 * A negative TIMEOUT value implies infinite timeout, i.e., a blocking read.
 * Non-zero values of TIMEOUT should be used only when the output thread
 * is allowed to block.
 *
 * STYLE should be an array of same length as BUF, and contains
 * the style bits associated with each character on return (see below).
 *
 * OPCODES contains a bit mask describing the type of output (see below).
 * Using Extended Backus-Naur Form notation:
 *
 * OPCODES ::= STREAMDATA NEWLINE? ERROR?
 *               COOKIESTR? DOCSTREAM? XMLSTREAM? JSSTREAM? WINSTREAM?
 * if StreamMode data is being returned.
 * (NEWLINE, if set, denotes that the stream has terminated;
 *  if ERROR is also set, it means that the stream has terminated abnormally.)
 *
 * OPCODES ::= SCREENDATA BELL? ( OUTPUT | CLEAR | INSERT | DELETE | SCROLL )?
 * if ScreenMode data is being returned.
 * If none of the flags OUTPUT ... SCROLL are set in screen mode,
 * do nothing but position the cursor (and ring the bell, if need be).
 *
 * OPCODES ::= LINEDATA BELL? (  CLEAR
 *                             | ( PROMPT | OUTPUT)? INPUT ( NEWLINE HIDE? )?
 *                             | PROMPT? INPUT META ( COMPLETION | NEWLINE HIDE? )
 *                             | PROMPT? OUTPUT NEWLINE? )
 * if LineMode data is being returned.
 *
 * If OPCODES == 0, then it means that no data has been read.
 *
 * If the returned OPCODES has META and COMPLETION bits set, then the completed
 * version of the meta input should be supplied through a call to
 * LTERM_WRITE, with an input data type, as if the user had typed it.
 *
 * BUF_ROW and BUF_COL denote the row and starting column at which to
 * display the data in BUF.
 * (BUF_ROW and BUF_COL are not used for CLEAR option in screen/line mode)
 * In ScreenMode or LineMode, CURSOR_ROW and CURSOR_COL denote the final
 * cursor position after any data in BUF is displayed.
 *
 * The bottom left corner of the screen corresponds to row 0, column 0.
 * BUF_COL, CURSOR_COL are always >= 0, with 0 denoting the leftmost column.
 * (BUF_COL is always zero if NOPARTLINE flag is set for LTERM.)
 * (CURRENT IMPLEMENTATION: BUF_COL is always zero.)
 * BUF_ROW, CURSOR_ROW are always set to -1 when LTERM is in line mode,
 * BUF_ROW, CURSOR_ROW are always >=0 when LTERM is in screen mode,
 * with 0 denoting the bottom row.
 *
 * In ScreenMode:
 *    - OUTPUT denotes that a modifed row of data is being returned.
 *    - OPVALS contains the no. of lines to be inserted/deleted,
 *      for INSERT/DELETE/SCROLL operations
 *            
 */

int lterm_read(int lterm, int timeout, UNICHAR *buf, int count,
               UNISTYLE *style, int *opcodes, int *opvals,
               int *buf_row, int *buf_col, int *cursor_row, int *cursor_col);

/* opcodes describing terminal operations:
 */
#define LTERM_STREAMDATA_CODE  0x0001U  /* Stream mode */
#define LTERM_SCREENDATA_CODE  0x0002U  /* Screen mode */
#define LTERM_LINEDATA_CODE    0x0004U  /* Line mode */
#define LTERM_BELL_CODE        0x0008U  /* Ring bell */
#define LTERM_CLEAR_CODE       0x0010U  /* Clear screen */
#define LTERM_INSERT_CODE      0x0020U  /* Insert lines above current line */
#define LTERM_DELETE_CODE      0x0040U  /* Delete lines below current line */
#define LTERM_SCROLL_CODE      0x0080U  /* Define scrolling region */
#define LTERM_INPUT_CODE       0x0100U  /* Contains STDIN at end of line */
#define LTERM_PROMPT_CODE      0x0200U  /* Contains prompt at beginning */
#define LTERM_OUTPUT_CODE      0x0400U  /* Contains STDOUT/STDERR/ALTOUT */
#define LTERM_META_CODE        0x0800U  /* Meta input */
#define LTERM_COMPLETION_CODE  0x1000U  /* Completion requested */
#define LTERM_NEWLINE_CODE     0x2000U  /* Complete (new) line */
#define LTERM_HIDE_CODE        0x4000U  /* Hide output */
#define LTERM_ERROR_CODE       0x8000U  /* Error in output */

#define LTERM_COOKIESTR_CODE  0x10000U  /* Stream prefixed with cookie */
#define LTERM_DOCSTREAM_CODE  0x20000U  /* Stream contains complete document */
#define LTERM_XMLSTREAM_CODE  0x40000U  /* Stream contains XML, not HTML */
#define LTERM_JSSTREAM_CODE   0x80000U  /* Stream contains Javascript */
#define LTERM_WINSTREAM_CODE 0x100000U  /* Display stream in entire window */


/* LTERM/XTERM 16-bit style mask:
 * PROMPT, STDIN, STDOUT, STDERR, ALTOUT are mutually exclusive.
 * The markup styles apply to STDIN/STDOUT/ALTOUT data.
 * The highlighting styles only apply to STDOUT data.
 * The VT100 foreground and background styles are not implemented.
 */

#define LTERM_PROMPT_STYLE     0x0001UL /* prompt string    */
#define LTERM_STDIN_STYLE      0x0002UL /* standard input   */
#define LTERM_STDOUT_STYLE     0x0004UL /* standard output  */
#define LTERM_STDERR_STYLE     0x0008UL /* standard error   */
#define LTERM_ALTOUT_STYLE     0x0010UL /* alternate output */

#define LTERM_URI_STYLE        0x0020UL /* URI markup */
#define LTERM_HTML_STYLE       0x0040UL /* HTML markup */
#define LTERM_XML_STYLE        0x0080UL /* XML markup */

#define LTERM_BOLD_STYLE       0x0100UL /* boldface */
#define LTERM_ULINE_STYLE      0x0200UL /* underline */
#define LTERM_BLINK_STYLE      0x0400UL /* blink */
#define LTERM_INVERSE_STYLE    0x0800UL /* inverse video */

/* LTERM option flags */
#define LTERM_NOCANONICAL_FLAG   0x0001U
#define LTERM_NOEDIT_FLAG        0x0002U
#define LTERM_NOCOMPLETION_FLAG  0x0004U
#define LTERM_NOMETA_FLAG        0x0008U
#define LTERM_NOECHO_FLAG        0x0010U
#define LTERM_NOMARKUP_FLAG      0x0020U
#define LTERM_NOPTY_FLAG         0x0040U
#define LTERM_NONUL_FLAG         0x0080U
#define LTERM_NOLINEWRAP_FLAG    0x0100U
#define LTERM_NOEXPORT_FLAG      0x0200U
#define LTERM_STDERR_FLAG        0x0400U
#define LTERM_PARTLINE_FLAG      0x0800U

/* Process type codes */
#define LTERM_DETERMINE_PROCESS -1       /* Determine process type from name */
#define LTERM_UNKNOWN_PROCESS    0       /* Unknown process type */
#define LTERM_SH_PROCESS         1       /* Bourne shell */
#define LTERM_KSH_PROCESS        2       /* Korn shell */
#define LTERM_BASH_PROCESS       3       /* Bourne Again shell */
#define LTERM_CSH_PROCESS        4       /* C shell */
#define LTERM_TCSH_PROCESS       5       /* TC shell */

/* lterm_write data type codes (XML server output not permitted) */
#define LTERM_WRITE_PLAIN_INPUT     0    /* Plain text user input */
#define LTERM_WRITE_XML_INPUT       1    /* XML element user input */
#define LTERM_WRITE_PLAIN_OUTPUT    2    /* Plain text server output */
#define LTERM_WRITE_CLOSE_MESSAGE   3    /* End of file message */

#ifdef  __cplusplus
}
#endif

#endif  /* _LINETERM_H */
