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
import java.util.*;


/**
 * Stores a RFC822 mailbox.  Defined as being either a
 * addr-spec (<i>local-part@domain</i>), or a route-addr
 * preceeded by a phrase (<i>phrase &lt;local-part@domain&gt;</i>).
 * <br><br>
 * Mere humans tends to consider this a <i>name</i> and an
 * <i>address</i>.  In the first addr-spec form, the name is usually
 * within a RFC822 comment preceeding the addr-spec.
 * If there is no comment, a mailbox <i>name</i> may be <b>null</b>.
 * <br><br>
 * Normally used only as a data type returned by the <b>RFC822MailboxList</b>
 * class.
 *
 * @see         RFC822MailboxList
 * @author      Eric Bina
 */
public class RFC822Mailbox
{
  private String name, address;

  /**
   * Should be created only by the <b>RFC822MailboxList</b> class.
   *
   * @see         RFC822MailboxList
   */
  public RFC822Mailbox(String name, String address)
  {
    this.name = name;
    this.address = address;
  }


  public String getName()
  {
    return(this.name);
  }


  public String getAddress()
  {
    return(this.address);
  }


  /**
   * Creates a valid RFC822 mailbox.  Since we don't like to lose
   * the comment information, this will reform addresses like:
   * <br><tt> (Eric Bina) ebina@netscape.com </tt><br>
   * to addresses like:
   * <br><tt> "Eric Bina" &lt;ebina@netscape.com&gt; </tt><br>
   */
  public String getMailboxString()
  {
    String mailbox = null;

    if ((this.name != null)&&(this.name.equalsIgnoreCase("") == false))
    {
      mailbox = this.name + " <" + this.address + ">";
    }
    else
    {
      mailbox = this.address;
    }
    return(mailbox);
  }
}

