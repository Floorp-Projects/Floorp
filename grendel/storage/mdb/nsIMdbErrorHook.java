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

/*| nsIMdbErrorHook: a base class for clients of this API to subclass, in order
**| to provide a callback installable in nsIMdbEnv for error notifications. If
**| apps that subclass nsIMdbErrorHook wish to maintain a reference to the env
**| that contains the hook, then this should be a weak ref to avoid cycles.
**|
**|| OnError: when nsIMdbEnv has an error condition that causes the total count
**| of errors to increase, then nsIMdbEnv should call OnError() to report the
**| error in some fashion when an instance of nsIMdbErrorHook is installed.  The
**| variety of string flavors is currently due to the uncertainty here in the
**| nsIMdbBlob and nsIMdbCell interfaces.  (Note that overloading by using the
**| same method name is not necessary here, and potentially less clear.)
|*/
public interface nsIMdbErrorHook { // env callback handler to report errors
  
  // { ===== begin error methods =====
  public int OnErrorString(nsIMdbEnv ev, String inAscii);
  // public int OnErrorYarn(nsIMdbEnv ev, mdbYarn inYarn);
  // } ===== end error methods =====
  
  // { ===== begin warning methods =====
  public int OnWarningString(nsIMdbEnv ev, String inAscii);
  // public int OnWarningYarn(nsIMdbEnv ev, mdbYarn inYarn);
  // } ===== end warning methods =====
  
  // { ===== begin abort hint methods =====
  public int OnAbortHintString(nsIMdbEnv ev, String inAscii);
  // public int OnAbortHintYarn(nsIMdbEnv ev, mdbYarn inYarn);
  // } ===== end abort hint methods =====
};

