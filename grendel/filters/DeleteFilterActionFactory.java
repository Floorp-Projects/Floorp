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
 * Class DeleteFilterActionFactory.
 *
 * Created: David Williams <djw@netscape.com>,  1 Oct 1997.
 */

package grendel.filters;

import java.io.Reader;
import javax.mail.Message;
import javax.mail.Flags;
import javax.mail.Flags.Flag;
import javax.mail.MessagingException;

import grendel.filters.IFilterAction;

public class DeleteFilterActionFactory extends Object
                                       implements IFilterActionFactory {

  static private class DeleteFilterAction extends Object
                                          implements IFilterAction {
        // IFilterAction methods:
        public void exorcise(Message message) {
          String subject = null;
          try {
                if (message != null)
                  subject = message.getSubject();
          } catch (MessagingException e) {
                subject = "getSubject() threw exception";
          }
          System.err.println("Message: " + subject + " -> discard;");
          try {
                message.setFlag(Flags.Flag.DELETED, true);
          } catch (MessagingException e) {
                System.err.println("DeleteFilterAction.exercise() failed: " + e);
          }
        }
  }

  // IFilterActionFactory methods:
  public IFilterAction Make(String[] notused) {
        return new DeleteFilterAction();
  }
  public IFilterAction Make(FilterRulesParser notused) {
        return new DeleteFilterAction();
  }
  public String getName() {
        return "discard"; // Do not localize, part of save format
  }
  public String toString() {
        return "Delete"; // FIXME localize
  }
  static public IFilterAction Make() {
        return new DeleteFilterAction();
  }
}
