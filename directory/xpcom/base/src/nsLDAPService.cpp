/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsLDAPInternal.h"
#include "nsLDAPService.h"

// constructor
//
nsLDAPService::nsLDAPService()
{
  NS_INIT_ISUPPORTS();
}

// destructor
//
nsLDAPService::~nsLDAPService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPService,nsIRunnable);

// initialize; should only be called by the generic constructor after
// creation.  This is where the thread gets spun up.
//
NS_IMETHODIMP
nsLDAPService::Init(void)
{
  nsresult rv;

  NS_ASSERTION(!mThread, "nsLDAPService already initialized!");

  rv = NS_NewThread(getter_AddRefs(mThread),
		    this, 0, PR_JOINABLE_THREAD);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// for nsIRunnable.  all the processing work happens here.
//
NS_IMETHODIMP
nsLDAPService::Run(void)
{
  PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, ("nsLDAPService::Run() entered\n"));

  // XXX - should use mThreadRunning here
  //	
  while (1) {

  }

  return NS_OK;
}

