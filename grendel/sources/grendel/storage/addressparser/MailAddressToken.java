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
 * Created: Eric Bina <ebina@netscape.com>, 30 Oct 1997.
 */

package grendel.storage.addressparser;

import java.io.*;


class MailAddressToken extends AddressToken
{
  private MailboxToken mailbox;
  private GroupToken group;

  public MailAddressToken(MailboxToken mailbox)
  {
    this.mailbox = mailbox;
    this.group = null;
    this.indx1 = mailbox.getTokenStart();
    this.indx2 = mailbox.getTokenEnd();
    this.token_type = AddressToken.MAIL_ADDRESS;
  }

  public MailAddressToken(GroupToken group)
  {
    this.mailbox = null;
    this.group = group;
    this.indx1 = group.getTokenStart();
    this.indx2 = group.getTokenEnd();
    this.token_type = AddressToken.MAIL_ADDRESS;
  }

  public MailboxToken getMailbox()
  {
    return(this.mailbox);
  }

  public GroupToken getGroup()
  {
    return(this.group);
  }
}

