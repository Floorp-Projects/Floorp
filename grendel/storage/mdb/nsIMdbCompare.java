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

/*| nsIMdbCompare: a caller-supplied yarn comparison interface.  When two yarns
**| are compared to each other with Order(), this method should return a signed
**| long integer denoting relation R between the 1st and 2nd yarn instances
**| such that (First R Second), where negative is less than, zero is equal to,
**| and positive is greater than.  Note that both yarns are readonly, and the
**| Order() method should make no attempt to modify the yarn content.
|*/
interface nsIMdbCompare { // caller-supplied yarn comparison
  
  // { ===== begin nsIMdbCompare methods =====
  public int Order(nsIMdbEnv ev,      // compare first to second yarn
                   String inFirst,   // first yarn in comparison
                   String inSecond);  // second yarn in comparison
  // negative="<", zero="=", positive=">"
  // } ===== end nsIMdbCompare methods =====
  
};

