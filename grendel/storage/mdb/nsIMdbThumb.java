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

/*| nsIMdbThumb: 
  |*/
interface nsIMdbThumb extends nsIMdbObject { // closure for repeating incremental method
  
  // { ===== begin nsIMdbThumb methods =====
  public int GetProgress(nsIMdbEnv ev,
                         int outTotal,    // total somethings to do in operation
                         int outCurrent,  // subportion of total completed so far
                         int outDone,      // is operation finished?
                         int outBroken     // is operation irreparably dead and broken?
                         );
  
  public int DoMore(nsIMdbEnv ev,
                    int outTotal,    // total somethings to do in operation
                    int outCurrent,  // subportion of total completed so far
                    int outDone,      // is operation finished?
                    int outBroken     // is operation irreparably dead and broken?
                    );
  
  public void CancelAndBreakThumb( // cancel pending operation
                                  nsIMdbEnv ev);
  // } ===== end nsIMdbThumb methods =====
};

