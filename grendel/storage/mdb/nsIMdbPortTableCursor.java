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

/*| nsIMdbPortTableCursor: cursor class for iterating port tables
**|
**|| port: the cursor is associated with a specific port, which can be
**| set to a different port (which resets the position to -1 so the
**| next table acquired is the first in the port.
**|
|*/
interface nsIMdbPortTableCursor extends nsIMdbCursor { // table collection iterator
  
  // { ===== begin nsIMdbPortTableCursor methods =====
  
  // { ----- begin attribute methods -----
  public void SetPort(nsIMdbEnv ev, nsIMdbPort ioPort); // sets pos to -1
  public nsIMdbPort GetPort(nsIMdbEnv ev);
  
  public void SetRowScope(nsIMdbEnv ev, // sets pos to -1
                          int inRowScope);
  public int GetRowScope(nsIMdbEnv ev); 
  // setting row scope to zero iterates over all row scopes in port
  
  public void SetTableKind(nsIMdbEnv ev, // sets pos to -1
                           int inTableKind);
  public int GetTableKind(nsIMdbEnv ev);
  // setting table kind to zero iterates over all table kinds in row scope
  // } ----- end attribute methods -----
  
  // { ----- begin table iteration methods -----
  public nsIMdbTable NextTable( // get table at next position in the db
                               nsIMdbEnv ev); // context
  // the next table in the iteration
  // } ----- end table iteration methods -----
  
  // } ===== end nsIMdbPortTableCursor methods =====
};

