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
import java.io.*;

/**
 * A Test program to test the <b>RFC822AddressList</b> and
 * <b>RFC822MailboxList</b> classes.
 *
 * @see      RFC822AddressList
 * @see      RFC822MailboxList
 * @author   Eric Bina
 */
public class P1
{

  public static void main(String args[]) throws IOException
  {
    RFC822AddressList address_list;
    RFC822MailboxList mailbox_list;

    System.out.println("Arg[0] = " + args[0]);
    System.out.println("\n\n");

    System.out.println("Test RFC822AddressList");
    address_list = new RFC822AddressList(args[0]);
    if (address_list.isError())
    {
      System.out.print("ERROR:  ");
      System.out.println(address_list.getErrorString());
    }
    else
    {
      System.out.print("Address:  ");
      for (int i=0; i < address_list.size(); i++)
      {
        System.out.print(address_list.getAddressString(i));
        if (i != (address_list.size() - 1))
        {
          System.out.print(",\n\t");
        }
      }
    }
    System.out.println("\n\n");

    System.out.println("Test RFC822MailboxList");
    mailbox_list = new RFC822MailboxList(args[0]);
    if (mailbox_list.isError())
    {
      System.out.print("ERROR:  ");
      System.out.println(mailbox_list.getErrorString());
    }
    else
    {
      int len;
      RFC822Mailbox mailbox_array[];

      mailbox_array = mailbox_list.getMailboxArray();
      len = mailbox_list.mailboxCount();

      for (int i=0; i < len; i++)
      {
        System.out.print(mailbox_array[i].getMailboxString());
        if (i != (len - 1))
        {
          System.out.print(", ");
        }
      }
    }
    System.out.println("\n\n");
  }
}

