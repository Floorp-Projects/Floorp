/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Mauro Botelho <mabotelh@email.com>, 20 Mar 1999.
 */

package grendel.storage.mdb;

/** nsIMdbEnv: a context parameter used when calling most abstract db methods.
 ** The main purpose of such an object is to permit a database implementation
 ** to avoid the use of globals to share information between various parts of
 ** the implementation behind the abstract db interface.  An environment acts
 ** like a session object for a given calling thread, and callers should use
 ** at least one different nsIMdbEnv instance for each thread calling the API.
 ** While the database implementation might not be threaded, it is highly
 ** desirable that the db be thread-safe if calling threads use distinct
 ** instances of nsIMdbEnv.  Callers can stop at one nsIMdbEnv per thread, or they
 ** might decide to make on nsIMdbEnv instance for every nsIMdbPort opened, so that
 ** error information is segregated by database instance.  Callers create
 ** instances of nsIMdbEnv by calling the MakeEnv() method in nsIMdbFactory. 
 **
 ** tracing: an environment might support some kind of tracing, and this
 ** boolean attribute permits such activity to be enabled or disabled.
 **
 ** errors: when a call to the abstract db interface returns, a caller might
 ** check the number of outstanding errors to see whether the operation did
 ** actually succeed. Each nsIMdbEnv should have all its errors cleared by a
 ** call to ClearErrors() before making each call to the abstract db API,
 ** because outstanding errors might disable further database actions.  (This
 ** is not done inside the db interface, because the db cannot in general know
 ** when a call originates from inside or outside -- only the app knows this.)
 **
 ** error hook: callers can install an instance of nsIMdbErrorHook to receive
 ** error notifications whenever the error count increases.  The hook can
 ** be uninstalled by passing a null pointer.
 **
 */
public interface nsIMdbEnv extends nsIMdbObject { // db specific context parameter
  
  // { ===== begin nsIMdbEnv methods =====
  
  // { ----- begin attribute methods -----
  int GetErrorCount(int outCount, boolean outShouldAbort);
  int GetWarningCount(int outCount, boolean outShouldAbort);
  
  int GetDoTrace(boolean outDoTrace);
  int SetDoTrace(boolean inDoTrace);
  
  boolean GetAutoClear();
  void SetAutoClear(boolean inAutoClear);
  
  nsIMdbErrorHook GetErrorHook();
  void SetErrorHook(nsIMdbErrorHook ioErrorHook); // becomes referenced
  
  //virtual mdb_err GetHeap(nsIMdbHeap** acqHeap) = 0;
  //virtual mdb_err SetHeap(
  //nsIMdbHeap* ioHeap) = 0; // becomes referenced
  // } ----- end attribute methods -----
  
  int ClearErrors(); // clear errors beore re-entering db API
  int ClearWarnings(); // clear warnings
  int ClearErrorsAndWarnings(); // clear both errors & warnings
  // } ===== end nsIMdbEnv methods =====
};

