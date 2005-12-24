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
import javax.mail.internet.InternetAddress;


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
public class RFC822Mailbox extends InternetAddress {
    //private String name, address;
    
    /**
     * Should be created only by the <b>RFC822MailboxList</b> class.
     *
     * @see         RFC822MailboxList
     */
    public RFC822Mailbox(String personal, String address) {
        this.address = address;
        this.personal = personal;
        this.encodedPersonal = null;
    }
    
    public RFC822Mailbox(String address) {
        this("",address);
    }
    
    public String getName() {
        return getPersonal();
    }
    
    /**
     * Creates a valid RFC822 mailbox.  Since we don't like to lose
     * the comment information, this will reform addresses like:
     * <br><tt> (Eric Bina) ebina@netscape.com </tt><br>
     * to addresses like:
     * <br><tt> "Eric Bina" &lt;ebina@netscape.com&gt; </tt><br>
     */
    public String getMailboxString() {
        String mailbox = null;
        
        if ((this.personal != null)&&(this.personal.equalsIgnoreCase("") == false)) {
            mailbox = this.personal + " <" + this.address + ">";
        } else {
            mailbox = this.address;
        }
        return(mailbox);
    }
    
    public String toString() {
        return getMailboxString();
    }
}

