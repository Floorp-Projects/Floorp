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

/** nsIMdbObject: base class for all message db class interfaces
 **
 ** factory: all nsIMdbObjects from the same code suite have the same factory
 **
 ** refcounting: both strong and weak references, to ensure strong refs are
 ** acyclic, while weak refs can cause cycles.  CloseMdbObject() is
 ** called when (strong) use counts hit zero, but clients can call this close
 ** method early for some reason, if absolutely necessary even though it will
 ** thwart the other uses of the same object.  Note that implementations must
 ** cope with close methods being called arbitrary numbers of times.  The COM
 ** calls to AddRef() and release ref map directly to strong use ref calls,
 ** but the total ref count for COM objects is the sum of weak & strong refs.
 */
public interface nsIMdbObject { // msg db base class
  
  // { ===== begin nsIMdbObject methods =====
  
  // { ----- begin attribute methods -----
  public boolean IsFrozenMdbObject(nsIMdbEnv ev);
  // same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
  // } ----- end attribute methods -----
  
  // { ----- begin factory methods -----
  public nsIMdbFactory GetMdbFactory(nsIMdbEnv ev); 
  // } ----- end factory methods -----
  
  public int CloseMdbObject(nsIMdbEnv ev); // called at strong refs zero
  public boolean IsOpenMdbObject(nsIMdbEnv ev);
  // } ----- end ref counting -----
}
