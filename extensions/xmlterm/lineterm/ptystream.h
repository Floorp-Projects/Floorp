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
 * The Original Code is lineterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of
 * those above. If you wish to allow use of your version of this
 * file only under the terms of the GPL and not to allow
 * others to use your version of this file under the MPL, indicate
 * your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* ptystream.h: pseudo-TTY stream header
 * (used by ltermPrivate.h)
 */

#ifndef _PTYSTREAM_H

#define _PTYSTREAM_H   1

#ifdef  __cplusplus
extern "C" {
#endif

#define PTYNAMELEN 10         /* Length of PTY/TTY device name: /dev/pty?? */

struct ptys {                 /* PTY structure */

  int ptyFD;                  /* PTY file descriptor (bi-directional) */
  int errpipeFD;              /* stderr pipe file descriptor (-1, if none) */

  long pid;                   /* PTY child PID */

  int debug;                  /* Debugging flag */

  char ptydev[PTYNAMELEN+1];  /* PTY (master) name */
  char ttydev[PTYNAMELEN+1];  /* TTY (slave) name */
};

/* creates a new pseudo-TTY (PTY) and also a new process attached to
 * it to execute the command line contained in array ARGV.
 * The PTY details are stored in the PTY structure PTYP.
 * ROWS, COLS contain the initial no. of rows/columns.
 * X_PIXELS, Y_PIXELS contain the initial screen size in pixels
 * (may be set to zero if screen size is unknown).
 * ERRFD is the file descriptor to which the STDERR output of the
 * child process is directed.
 * If ERRFD == -1, then the STDERR output is redirected to STDOUT.
 * If ERRFD == -2, then a new pipe is created and STDERR is redirected
 * through it.
 * If NOBLOCK is true, enable non-blocking I/O on PTY.
 * If NOECHO is true, tty echoing is turned off.
 * If NOEXPORT is true, then the current environment is not exported
 * to the new process.
 * If DEBUG_LTERM is true, debugging messages are printed to STDERR.
 * Returns 0 on success and -1 on error.
 */
int pty_create(struct ptys *ptyp, char *const argv[],
               int rows, int cols, int x_pixels, int y_pixels,
               int errfd, int noblock, int noecho, int noexport, int debug);

/* resizes a PTY; if ptyp is null, resizes file desciptor 0,
 * returning 0 on success and -1 on error.
 */
int pty_resize(struct ptys *ptyp, int rows, int cols,
                                  int xpix, int ypix);

/* closes the PTY and kills the associated child process, if still alive.
 * Also close STDERR pipe, if open.
 * Returns 0 on success and -1 on error.
 */
int pty_close(struct ptys *ptyp);

#ifdef  __cplusplus
}
#endif

#endif  /* _PTYSTREAM_H */
