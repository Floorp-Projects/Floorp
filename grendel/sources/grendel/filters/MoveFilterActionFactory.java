/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Class MoveFilterActionFactory.
 *
 * Created: David Williams <djw@netscape.com>,  1 Oct 1997.
 *
 * Contributors: Edwin Woudt <edwin@woudt.nl>
 */

package grendel.filters;

import java.io.IOException;

import javax.mail.Message;
import javax.mail.Folder;
import javax.mail.Flags;
import javax.mail.MessagingException;

import grendel.filters.IFilterAction;
import grendel.filters.FilterMaster;
import grendel.filters.FilterRulesParser;
import grendel.filters.FilterSyntaxException;

public class MoveFilterActionFactory extends Object
implements IFilterActionFactory {
  static class MoveFilterAction extends Object implements IFilterAction {
        // slots
        private Folder fTargetFolder;

        // constructors
    protected MoveFilterAction(Folder folder) {
          fTargetFolder = folder;
        }

        // IFilterAction methods:
        public void exorcise(Message message) {
          String subject = null;
          try {
                if (message != null)
                  subject = message.getSubject();
          } catch (MessagingException e) {
                subject = "getSubject() threw exception";
          }
          System.err.println("Message: " + subject +
                                                 "-> fileinto \"" + fTargetFolder.getName() + "\";");
          if (fTargetFolder != null && message != null) {
                boolean set;
                try {
                  set = message.isSet(Flags.Flag.DELETED);
                } catch (MessagingException e) {
                  set = false;
                }

                if (set)
                  System.out.println("MoveFilterAction.exercise() message is deleted");

                Message mlist[] = { message };
                try {
                  fTargetFolder.appendMessages(mlist);
                } catch (MessagingException e) {
                  System.err.println("MoveFilterAction.exercise() move failed: " + e);
                }
                try {
                  message.setFlags(new Flags(Flags.Flag.DELETED), true);
                } catch (MessagingException e) {
                  System.err.println("MoveFilterAction.exercise() delete failed: " +e);
                }
          }
        }
  }

  // IFilterActionFactory methods:
  // first arg is name of folder
  public IFilterAction Make(String[] args) throws FilterSyntaxException {
        if (args == null || args.length != 1) {
          throw new     FilterSyntaxException("Wrong number of args to " +
                                                                          getName() + " action");
        }
        FilterMaster filter_master = FilterMaster.Get();
        Folder folder = filter_master.getFolder(args[0]);

        if (folder == null) {
          throw new     FilterSyntaxException("Folder " + args[0] + "not found in" +
                                                                          getName() + " action");
        }
        return new MoveFilterAction(folder);
  }
  public IFilterAction Make(FilterRulesParser parser)
        throws IOException, FilterSyntaxException {
        // We expect:
        // moveTo "folder"
        // folder is the folder to file the message in
        FilterRulesParser.Token token;

        token = parser.getToken();

        String name = token.toString();
        Folder folder = FilterMaster.Get().getFolder(name);

        if (folder == null) {
          throw new     FilterSyntaxException("Folder " + name + " not found in " +
                                                                          getName() + " action");
        }
        return new MoveFilterAction(folder);
  }

  public String getName() {
        return "fileinto"; // Do not localize, part of save format
  }
  public String toString() {
        return "Move to folder"; // FIXME localize
  }

  /*
  // convenience
  static public IFilterAction Make(Folder folder) {
        return new MoveFilterAction(folder);
  }
  */
}
