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


class AddressToken
{
  static final int UNKNOWN = 0;
  static final int SPECIAL_CHAR = 1;
  static final int QUOTED_STRING = 2;
  static final int DOMAIN_LITERAL = 3;
  static final int COMMENT = 4;
  static final int ATOM = 5;
  static final int WORD = 6;
  static final int DOMAIN_REF = 7;
  static final int SUB_DOMAIN = 8;
  static final int ADDR_SPEC = 9;
  static final int ROUTE = 10;
  static final int ROUTE_ADDR = 11;
  static final int PHRASE = 12;
  static final int MAILBOX = 13;
  static final int GROUP = 14;
  static final int MAIL_ADDRESS = 15;

  public int indx1, indx2;
  public int token_type;

  public AddressToken()
  {
    this.indx1 = 0;
    this.indx2 = 0;
    this.token_type = UNKNOWN;
  }

  public AddressToken(int indx1, int indx2, int type)
  {
    this.indx1 = indx1;
    this.indx2 = indx2;
    if (this.indx1 > this.indx2)
    {
      this.indx1 = indx2;
      this.indx2 = indx1;
    }

    if ((type < SPECIAL_CHAR)||(type > ATOM))
    {
      this.token_type = UNKNOWN;
    }
    else
    {
      this.token_type = type;
    }
  }

  public int getTokenStart()
  {
    return(this.indx1);
  }

  public int getTokenEnd()
  {
    return(this.indx2);
  }

  public int getType()
  {
    return(this.token_type);
  }

  public int length()
  {
    return(this.indx2 - this.indx1 + 1);
  }


  public String getTokenString(Vector tokens)
  {
    String token_str;

    if ((this.indx1 < 0)||(this.indx2 < 0))
    {
      return(null);
    }

    token_str = new String("");
    for (int i=this.indx1; i <= this.indx2; i++)
    {
      RFC822Token token = (RFC822Token)tokens.elementAt(i);
      String str = (String)token.getObject();

      token_str = token_str + str;
    }
    return(token_str);
  }


  public void printToken(Vector tokens)
  {
    if ((this.indx1 < 0)||(this.indx2 < 0))
    {
      return;
    }

    for (int i=this.indx1; i <= this.indx2; i++)
    {
      RFC822Token token = (RFC822Token)tokens.elementAt(i);
      String str = (String)token.getObject();
      System.out.print(str);
    }
    System.out.flush();
  }
}

