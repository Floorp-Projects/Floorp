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


class MailboxToken extends AddressToken
{
  private AddressToken addr_spec, phrase, route_addr;
  private Vector comments;
  private String name, address;

  public MailboxToken(AddressToken addr_spec)
  {
    this.name = null;
    this.address = null;
    this.comments = null;
    this.addr_spec = addr_spec;
    this.phrase = null;
    this.route_addr = null;
    this.indx1 = addr_spec.getTokenStart();
    this.indx2 = addr_spec.getTokenEnd();
    this.token_type = AddressToken.MAILBOX;
  }


  public MailboxToken(AddressToken phrase, AddressToken route_addr)
  {
    this.name = null;
    this.address = null;
    this.comments = null;
    this.addr_spec = null;
    this.phrase = phrase;
    this.route_addr = route_addr;

    // Phrase and route addr must be contiguous.
    if (route_addr.getTokenStart() != (phrase.getTokenEnd() + 1))
    {
      // ERROR
    }

    this.indx1 = phrase.getTokenStart();
    this.indx2 = route_addr.getTokenEnd();
    this.token_type = AddressToken.MAILBOX;
  }


  /*
   * Get the address as a string from the list of parsed tokens.
   */
  public String getAddress(Vector tokens)
  {
    String address = null;

    if (this.address != null)
    {
      return(this.address);
    }

    address = new String("");
    /*
     * Here we strip any optional route from the route-addr.
     */
    if (this.addr_spec == null)
    {
      int start, end;

      start = this.route_addr.getTokenStart();
      end = this.route_addr.getTokenEnd();
      for (int i=(end - 1); i >= start; i--)
      {
        RFC822Token token = (RFC822Token)tokens.elementAt(i);
        String str;

        if ((token.isSpecialChar('<') == true)||
            (token.isSpecialChar(':') == true))
        {
          break;
        }

        str = (String)token.getObject();
        address = str + address;
      }
    }
    /*
     * Just compose the addr-spec as a string.
     */
    else
    {
      int start, end;

      start = this.addr_spec.getTokenStart();
      end = this.addr_spec.getTokenEnd();
      for (int i=start; i <= end; i++)
      {
        RFC822Token token = (RFC822Token)tokens.elementAt(i);
        String str = (String)token.getObject();

        address = address + str;
      }
    }
    this.address = address;

    return(address);
  }


  /*
   * Get the name as a string from the list of parsed tokens.
   */
  public String getName(Vector tokens)
  {
    String name = null;

    if (this.name != null)
    {
      return(this.name);
    }

    name = new String("");
    /*
     * Here there was no phrase, but there were some
     * comments.  Compose a name from the comments,
     * replace characters not allowed in the name,
     * and enclose the new name as a quoted string.
     * when combining multiple comments, use a comma
     * separator.
     */
    if ((this.phrase == null)&&(this.comments != null)&&
        (this.comments.isEmpty() == false))
    {
      int num_comments = this.comments.size();

      for (int c=0; c < num_comments; c++)
      {
        RFC822Token comment = (RFC822Token)this.comments.elementAt(c);
        String str = (String)comment.getObject();
        int len = str.length();

        if (len < 3)
        {
          continue;
        }

        str = str.substring(1, len - 1);

        /*
         * Replace all double quotes with
         * single quotes, and replace nested
         * comment parens with square brackets.
         */
        str = str.replace('\"', '\'');
        str = str.replace('(', '[');
        str = str.replace(')', ']');

        name = name + str;
        if (c != (num_comments - 1))
        {
          name = name + ", ";
        }
      }
      if (name.equalsIgnoreCase("") == false)
      {
        name = "\"" + name + "\"";
      }
    }
    /*
     * Else just compose the phrase as a string
     */
    else if (this.phrase != null)
    {
      int start, end;

      start = this.phrase.getTokenStart();
      end = this.phrase.getTokenEnd();
      for (int i=start; i <= end; i++)
      {
        RFC822Token token = (RFC822Token)tokens.elementAt(i);
        String str = (String)token.getObject();

        name = name + str;
        if (i != end)
        {
          name = name + " ";
        }
      }
    }

    /*
     * If there was no phrase and no comments, the name
     * will become the empty string ""
     */

    this.name = name;
    return(name);
  }


  public void addComment(RFC822Token comment)
  {
    if (this.comments == null)
    {
      this.comments = new Vector();
    }
    this.comments.addElement((Object)comment);
  }


  public Vector GetComments()
  {
    return(this.comments);
  }


  public boolean noComments()
  {
    if (this.comments == null)
    {
      return(true);
    }
    else if (this.comments.isEmpty() == true)
    {
      return(true);
    }
    return(false);
  }
}

