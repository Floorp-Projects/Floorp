/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Originally Created by Spencer Murray <spence@netscape.com>, 15-Sep-95.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

typedef int XP_Bool;

#define TRUE 1
#define FALSE 0

#define LINUX_GLIBC_2

#include <sys/errno.h>
#if !defined(__FreeBSD__) && !defined(MACLINUX) && !defined(LINUX_GLIBC_2)
extern char *sys_errlist[];
extern int sys_nerr;
#endif

int XFE_CANT_MOVE_MAIL = 0;
int XFE_CANT_GET_NEW_MAIL_LOCK_FILE_EXISTS = 0;
int XFE_CANT_GET_NEW_MAIL_UNABLE_TO_CREATE_LOCK_FILE = 0;
int XFE_CANT_GET_NEW_MAIL_SYSTEM_ERROR = 0;
int XFE_CANT_MOVE_MAIL_UNABLE_TO_OPEN = 0;
int XFE_CANT_MOVE_MAIL_UNABLE_TO_READ = 0;
int XFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE = 0;
int XFE_THERE_WERE_PROBLEMS_MOVING_MAIL = 0;
int XFE_THERE_WERE_PROBLEMS_MOVING_MAIL_EXIT_STATUS = 0;
int XFE_THERE_WERE_PROBLEMS_CLEANING_UP = 0;
int XFE_MOVEMAIL_FAILURE_SUFFIX = 0;
int XFE_UNKNOWN_ERROR_CODE = 0;
int XFE_MOVEMAIL_NO_MESSAGES = 0;
int XFE_MOVEMAIL_CANT_DELETE_LOCK = 0;

#define TMP_SUFFIX ".nslock"
#define LOCK_SUFFIX ".lock"

#define NAME_LEN 1024

#ifndef BUFSIZ
#define BUFSIZ 1024
#endif /* BUFSIZ */

char *XP_GetString(int i) {
  return NULL;
}
  
static void
fe_movemail_perror(const char *message)
{
  int e = errno;
  char *es = 0;
  char *buf1 = 0;
  char buf2[512];
  char *suffix;
  int L;

  if ((unsigned)e < (unsigned)sys_nerr)
    {
      es = sys_errlist [e];
    }
  else
    {
      snprintf (buf2, sizeof (buf2), XP_GetString( XFE_UNKNOWN_ERROR_CODE ),
		errno);
	printf("XFE_UNKNOWN_ERROR_CODE\n");
      es = buf2;
    }

  suffix = XP_GetString(XFE_MOVEMAIL_FAILURE_SUFFIX);
  printf("XFE_MOVEMAIL_FAILURE_SUFFIX\n");
  if(!suffix) suffix = "";
  if(!message) message = "";
  L = strlen(message) + strlen(es) + strlen(suffix) + 40;
  buf1 = (char *) malloc(L);
  if(!buf1) return;
  snprintf (buf1, L-1, "%s\n%s\n\n%s", message, es, suffix);
#if 0
  FE_Alert (buf1);
#endif
  free(buf1);
}


int
fe_MoveMail (char *from, char *to)
{
  XP_Bool succeeded = FALSE;
  char tmp_file[NAME_LEN];
  char lock_file[NAME_LEN];
  char spool_dir[NAME_LEN];
  char *bp, buf[BUFSIZ];
  char msgbuf[1024];
  int from_fd = -1;
  int to_fd = -1;
  int tmp_fd = -1;
  int nread, nwritten, nbytes, ntodo;
  struct stat st;

  *lock_file = 0;

  if (!from || !to)
    return FALSE;

  if (strlen(from) > NAME_LEN - 1) return FALSE;

  /* Make spool_dir be "/var/spool/mail" from "/var/spool/mail/$USER". */
  strcpy(spool_dir, from);
  while ((bp = strrchr(spool_dir, '/')) && bp[1] == '\0')
    *bp = '\0';
  if (!bp) return FALSE;
  *bp = 0;

  printf("%s\n", spool_dir);

  /* If we can't write into the directory itself, we can't create files,
     so we lose. */
  if (access(spool_dir, R_OK|W_OK) < 0)
    {
      snprintf(msgbuf, sizeof (msgbuf),
		  XP_GetString(XFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE),
		  from);
	printf("XFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE\n");
      fe_movemail_perror(msgbuf);
      goto FAIL;
    }

  /* If the mail-spool file doesn't exist, or is 0-length, bug out.
   */
  if (stat(from, &st) < 0 ||
      st.st_size == 0)
    {
#if 0
      NET_Progress (XP_GetString(XFE_MOVEMAIL_NO_MESSAGES));
#endif
      goto FAIL;
    }
  
  snprintf(tmp_file, sizeof (tmp_file), "%s%s", from, TMP_SUFFIX);

  if (access(tmp_file, 0) == 0) {
    /* The tmp file exists; try to get rid of it */

    if (unlink(tmp_file) < 0) {
      snprintf(msgbuf, sizeof (msgbuf),
                  XP_GetString( XFE_CANT_GET_NEW_MAIL_LOCK_FILE_EXISTS ),
                  tmp_file);
      fe_movemail_perror(msgbuf);
      goto FAIL;
    }
  }

  snprintf(lock_file, sizeof (lock_file), "%s%s", from, LOCK_SUFFIX);

  while (1) {
    int ret;

    /* First create a real file, $USER.nslock
     */
    tmp_fd = open(tmp_file, O_WRONLY|O_CREAT|O_EXCL, 0666);

    if (tmp_fd < 0) {
      snprintf(msgbuf, sizeof (msgbuf),
              XP_GetString( XFE_CANT_GET_NEW_MAIL_UNABLE_TO_CREATE_LOCK_FILE ),
              tmp_file);
      fe_movemail_perror(msgbuf);
      goto FAIL;
    }
    close(tmp_fd);
    tmp_fd = -1;
  
    /* Then make a hard link from $USER.lock to $USER.nslock -- this is the
       trick to make NFS behave synchronously.
     */
    ret = link(tmp_file, lock_file);

    /* Now we've (attempted to) make the link.  `ret' says whether it was
       successful.  It would have failed if the lock was already being held.

       Regardless of whether it succeeded, we can now delete $USER.nslock
       (the thing to which the hard-link points.)
     */
    if (unlink(tmp_file) < 0) {
      snprintf(msgbuf, sizeof (msgbuf),
		  /* This error message isn't quite right; what it really
		     means is "can't delete $MAIL.nslock", but I guess that
		     could only happen if some other user was the owner of
		     it?  Or if it was already gone?  Weird... */
	XP_GetString(XFE_MOVEMAIL_CANT_DELETE_LOCK), tmp_file);
	printf("XFE_MOVEMAIL_CANT_DELETE_LOCK\n");
      fe_movemail_perror(msgbuf);
      goto FAIL;
    }

    /* If we didn't obtain the lock (== make the hard-link), above, then
       retry getting it until the file is 60 seconds old...   Then get
       Guido to go over and bust its kneecaps.
     */
    if (ret < 0) {
      sleep (1);
      if (stat(lock_file, &st) >= 0) {
	int current = time(NULL);
	if (st.st_ctime < current - 60)
	  unlink (lock_file);
      }
    } else {
      /* Got the lock - done waiting. */
      break;
    }
  }

  /* All of this junk used to happen in a forked process, but that's not
     necessary any more (actually, hasn't been for a long time) since we
     no longer play any setuid/setgid games -- there is no gain to doing
     this in another fork and waiting for it to complete.
   */

  /* Open the mail spool file for input...
   */
  from_fd = open(from, O_RDWR, 0666);
  if (from_fd < 0)
    {
      int err;
      if (access(from, W_OK) < 0)  /* look again, for right error message */
	err = XFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE;
      else if (access(from, 0) == 0)
	err = XFE_CANT_MOVE_MAIL_UNABLE_TO_READ;
      else
	err = XFE_CANT_MOVE_MAIL_UNABLE_TO_OPEN;
      snprintf(msgbuf, sizeof (msgbuf), XP_GetString(err), from);
      fe_movemail_perror(msgbuf);
      goto FAIL;
    }

  /* Open the destination file for output...
   */
  to_fd = open(to, O_WRONLY|O_CREAT|O_EXCL, 0666);
  if (to_fd < 0) {
    snprintf(msgbuf, sizeof (msgbuf),
		XP_GetString( XFE_CANT_MOVE_MAIL_UNABLE_TO_OPEN ),
		to);
    fe_movemail_perror(msgbuf);
    goto FAIL;
  }

  /* copy the file */

  nbytes = BUFSIZ;

  while (1) {
    nread = read(from_fd, buf, nbytes);

    if (nread < 0) {
      /* we're busted */
      snprintf(msgbuf, sizeof (msgbuf),
		  XP_GetString( XFE_CANT_MOVE_MAIL_UNABLE_TO_READ ), from);
      fe_movemail_perror(msgbuf);
      goto FAIL;
    }

    if (!nread) {
      /* we're done */
      break;
    }

    for (ntodo = nread, bp = buf;
	 ntodo > 0;
	 ntodo -= nwritten, bp += nwritten) {
      nwritten = write(to_fd, bp, ntodo);

      if (nwritten < 0) {
	snprintf(msgbuf, sizeof (msgbuf),
		    XP_GetString( XFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE ), to); 
	fe_movemail_perror(msgbuf);
	goto FAIL;
      }
    }
  }

  /* if we made it this far, we're done copying the file contents.
     First, close and sync the output file.
   */

  if (fsync(to_fd) < 0 ||
      close(to_fd) < 0)
    {
      snprintf(msgbuf, sizeof (msgbuf),
		  XP_GetString( XFE_CANT_MOVE_MAIL_UNABLE_TO_WRITE ), to); 
      fe_movemail_perror(msgbuf);
      goto FAIL;
    }
  to_fd = -1;

  /* Now the output file is closed and sync'ed, so we can truncate the
     spool file.
   */
  if (ftruncate(from_fd, (off_t)0) < 0)
    {
      snprintf(msgbuf, sizeof (msgbuf),
		  XP_GetString( XFE_THERE_WERE_PROBLEMS_CLEANING_UP ),
		  from);
      fe_movemail_perror(msgbuf);
      goto FAIL;
    }

  succeeded = TRUE;

 FAIL:

  if (to_fd >= 0) close(to_fd);
  if (from_fd >= 0) close(from_fd);
  if (tmp_fd >= 0) close(tmp_fd);

  /* Unlink the lock file.
     If this fails, but we were otherwise successful, then whine about it.
     Otherwise, we've already presented an error dialog about something else.
   */
  if (*lock_file && unlink(lock_file) < 0 && succeeded)
    {
      snprintf(msgbuf, sizeof (msgbuf),
		  XP_GetString( XFE_THERE_WERE_PROBLEMS_CLEANING_UP ),
		  lock_file);
      fe_movemail_perror(msgbuf);
      succeeded = FALSE;
    }

  return succeeded;
}

main()
{
  fe_MoveMail ("/var/spool/mail/localguy","/home/localguy/mozilla.mail-recovery");
}
