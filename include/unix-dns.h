/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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


/*
 * dns.h --- portable nonblocking DNS for Unix
 * Created: Jamie Zawinski <jwz@netscape.com>, 19-Dec-96.
 */

#ifndef __UNIX_DNS_H__
#define __UNIX_DNS_H__

/* Kick off an async DNS lookup;
   The returned value is an id representing this transaction;
    the result_callback will be run (in the main process) when we
    have a result.  Returns negative if something went wrong.
   If `status' is negative,`result' is an error message.
   If `status' is positive, `result' is a 4-character string of
   the IP address.
   If `status' is 0, then the lookup was prematurely aborted
    via a call to DNS_AbortHostLookup().
 */
extern int DNS_AsyncLookupHost(const char *name,
			       int (*result_callback) (void *id,
						       void *closure,
						       int status,
						       const char *result),
			       void *closure,
			       void **id_return);

/* Prematurely give up on the given host-lookup transaction.
   The `id' is what was returned by DNS_AsyncLookupHost.
   This causes the result_callback to be called with a negative
   status.
 */
extern int DNS_AbortHostLookup(void *id);

/* Call this from main() to initialize the async DNS library.
   Returns a file descriptor that should be selected for, or
   negative if something went wrong.  Pass it the argc/argv
   that your `main' was called with (it needs these pointers
   in order to give its forked subprocesses sensible names.)
 */
extern int DNS_SpawnProcess(int argc, char **argv);

/* The main select() loop of your program should call this when the fd
   that was returned by DNS_SpawnProcess comes active.  This may cause
   some of the result_callback functions to run.

   If this returns negative, then a fatal error has happened, and you
   should close `fd' and not select() for it again.  Call gethostbyname()
   in the foreground or something.
 */
extern int DNS_ServiceProcess(int fd);

#endif /* __UNIX_DNS_H__ */
