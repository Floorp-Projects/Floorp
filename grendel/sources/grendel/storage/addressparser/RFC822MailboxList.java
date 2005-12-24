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
import java.util.*;
import javax.mail.Address;
import javax.mail.internet.InternetAddress;


/**
 * A subclass of <b>RFC822AddressList</b>.  Its purpose is to create
 * a more human readable list of all the addresses of this address list.
 * To that end it flattens RFC822 groups in place into mailbox lists.
 * It also attempts to save those occasions when name information
 * is stored in a comment with an addr-spec instead of being put into
 * the more complete form of (phrase route-addr).
 *
 * @see         RFC822AddressList
 * @author      Eric Bina
 */
public class RFC822MailboxList extends RFC822AddressList
{
  private Vector<MailboxToken> mailbox_list;
  private List<RFC822Mailbox> mailboxes = new ArrayList<RFC822Mailbox>();
  
  public RFC822MailboxList()
  {
      
  }
  
  public RFC822MailboxList(RFC822MailboxList base)
  {
      mailboxes = new ArrayList<RFC822Mailbox>(base.mailboxes);
  }
  
  public RFC822MailboxList(String str)
  {
    super(str);
    if (this.parser != null)
    {
      createMailboxList();
      createMailboxArray();
    }
  }


  /**
   * @return     An array of <b>RFC822Mailbox</b> objects.
   *             Methods on the object are used to get strings
   *             to display.
   *
   * @see        RFC822Mailbox
   */
  public RFC822Mailbox[] getMailboxArray()
  {
    return mailboxes.toArray(new RFC822Mailbox[0]);
  }

  /*
   * Size of the mailbox list.
   */
  public int mailboxCount()
  {
    if (this.mailbox_list == null)
    {
      return(0);
    }
    else
    {
      return(this.mailbox_list.size());
    }
  }


  /***********************
   ***********************
   **  PRIVATE METHODS  **
   ***********************
   ***********************/

  /*
   * Make an array of standalone RFC822Mailbox objects
   * which contain all the needed string data.
   */
  private void createMailboxArray()
  {
    int cnt = this.mailbox_list.size();

    this.mailboxes.clear();
    for (int i=0; i<cnt; i++)
    {
      MailboxToken mailbox;
      Vector parser_tokens = this.parser.getTokenList();

      mailbox = (MailboxToken)this.mailbox_list.elementAt(i);
      this.mailboxes.add(new RFC822Mailbox(mailbox.getName(parser_tokens),
                                            mailbox.getAddress(parser_tokens)));
    }
  }

  /*
   * Convert the list of MailAddressTokens to a list
   * of MailboxTokens.  A GroupToken can become
   * zero or more MailboxTokens.
   */
  private void createMailboxList()
  {
    MailAddressToken address;
    Vector address_list;
    int list_size;

    if (this.mailbox_list != null)
    {
      return;
    }
    this.mailbox_list = new Vector();

    address_list = this.parser.getAddressList();
    list_size = address_list.size();

    for (int a=0; a < list_size; a++)
    {
      MailboxToken mailbox;
      Vector<MailboxToken> group_mboxes;
      int mbox_cnt;

      address = (MailAddressToken)address_list.elementAt(a);
      mailbox = address.getMailbox();
      if (mailbox != null)
      {
        this.mailbox_list.addElement(mailbox);
        continue;
      }

      /*
       * If here we have a group.
       */
      group_mboxes = address.getGroup().getMailboxList();
      if (group_mboxes.isEmpty() == true)
      {
        continue;
      }
      mbox_cnt = group_mboxes.size();
      for (int m=0; m < mbox_cnt; m++)
      {
        this.mailbox_list.addElement(
          group_mboxes.elementAt(m));
      }
    }

    /*
     * Pull all the comments associated with each mailbox
     * out of the original token list because we may
     * need to use them to make a name if none was
     * provided.
     */
    collectMailboxComments();
  }


  /*
   * Pull all comments from the original token list from the
   * start to the end of the mailbox and save them to later construct
   * a name for this mailbox if needed.
   */
  private void collectMailboxComments()
  {
    int list_size;
    int start, end;
    int all_indx, sub_indx;

    if (this.mailbox_list == null)
    {
      return;
    }

    all_indx = 0;
    sub_indx = 0;
    list_size = this.mailbox_list.size();
    for (int m=0; m < list_size; m++)
    {
      MailboxToken mailbox;
      int indx;

      mailbox = (MailboxToken)this.mailbox_list.elementAt(m);
      /*
       * Get all comments up top the last token of
       * the mailbox.
       */
      indx = addMailboxComments(mailbox, all_indx, sub_indx,
                                mailbox.getTokenEnd());
      all_indx = indx;
      sub_indx = mailbox.getTokenEnd();
      /*
       * If there are comments before the separating comma
       * or semi-colon (from a group) get those.
       */
      if (isMailboxSpacer(sub_indx + 1))
      {
        all_indx = addMailboxComments(mailbox, all_indx, sub_indx, (sub_indx + 1));
        sub_indx++;
      }
      /*
       * If there is no spacer, and there are currently no
       * comments for this mailbox, take the trailing
       * comment if it exists.
       */
      else if ((mailbox.noComments())&&(isComment(all_indx + 1)))
      {
        RFC822Token token = (RFC822Token)this.all_tokens.elementAt(all_indx + 1);
        mailbox.addComment(token);
        all_indx = all_indx + 2;
        sub_indx++;
      }

      /*
       * Catch trailing comments on last mailbox
       */
      if (m == (list_size - 1))
      {
        while (isComment(all_indx + 1) == true)
        {
          RFC822Token token = (RFC822Token)this.all_tokens.elementAt(all_indx + 1);
          mailbox.addComment(token);
          all_indx++;
        }
      }
    }
  }


  /*
   * Find any comments in the original token list that were
   * stripped from between the passed start and end of
   * the parser's token sublist.  Add any such comments
   * to the passed mailbox.  Return the position in the original
   * token list that corresponds to the passed end in the
   * token sublist.
   */
  private int addMailboxComments(MailboxToken mailbox, int all_indx,
                                 int sub_start, int sub_end)
  {
    while (sub_start <= sub_end)
    {
      if (isComment(all_indx) == true)
      {
        RFC822Token token = (RFC822Token)this.all_tokens.elementAt(all_indx);

        mailbox.addComment(token);
        all_indx++;
      }
      else
      {
        all_indx++;
        sub_start++;
      }
    }
    return(all_indx - 1);
  }


  /*
   * Mailboxes are always in comma separated lists, except for the
   * last mailbox of a group that is terminated by a ';'.
   * Check if the passed token in the token sublist could
   * be a mailbox separator.
   */
  private boolean isMailboxSpacer(int indx)
  {
    RFC822Token token;
    Vector parser_tokens;

    parser_tokens = this.parser.getTokenList();
    if (indx >= parser_tokens.size())
    {
      return(false);
    }

    token = (RFC822Token)parser_tokens.elementAt(indx);

    if ((token.isSpecialChar(',') == true)||(token.isSpecialChar(';') == true))
    {
      return(true);
    }
    return(false);
  }


  /*
   * Is the index passed into the main token list a comment token?
   */
  private boolean isComment(int indx)
  {
    RFC822Token token;

    if (indx >= this.all_tokens.size())
    {
      return(false);
    }
    token = (RFC822Token)this.all_tokens.elementAt(indx);

    if (token.getType() == RFC822Token.COMMENT)
    {
      return(true);
    }
    return(false);
  }
}

