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


class RouteAddrNoPhraseException extends RFC822ParserException
{
  public RouteAddrNoPhraseException(String str, AddressToken addr_token,
                                    Vector tokens)
  {
    super(str, addr_token, tokens);
  }


  public boolean hasRoute()
  {
    int start = getAddrSpecStart();
    RFC822Token token;

    token = (RFC822Token)this.tokens.elementAt(start - 1);
    if (token.isSpecialChar(':') == true)
    {
      return(true);
    }
    return(false);
  }


  public boolean hasComments(Vector orig_tokens)
  {
    int start = this.token.getTokenStart();
    int end = this.token.getTokenEnd();
    int orig_start = matchToken(0, 0, start, orig_tokens, this.tokens);

    /*
     * Comments just before the starting '<' count.
     */
    if (isComment(orig_tokens, (orig_start - 1)) == true)
    {
      return(true);
    }

    /*
     * Look for any comments in the body.
     */
    orig_start++;
    start++;
    while (start < end)
    {
      if (isComment(orig_tokens, orig_start) == true)
      {
        return(true);
      }
      orig_start++;
      start++;
    }

    /*
     * Comments just after the '>' count.
     */
    if (isComment(orig_tokens, (orig_start + 1)) == true)
    {
      return(true);
    }
    return(false);
  }


  public AddressToken getRouteAddr()
  {
    return(this.token);
  }


  public int getOrigRouteAddrStart(Vector orig_tokens)
  {
    int start = this.token.getTokenStart();
    int indx = matchToken(0, 0, start, orig_tokens, this.tokens);

    return(indx);
  }


  public int getOrigRouteAddrEnd(Vector orig_tokens)
  {
    int start = this.token.getTokenStart();
    int end = this.token.getTokenEnd();
    int indx = matchToken(0, 0, start, orig_tokens, this.tokens);

    indx = matchToken(indx, start, end, orig_tokens, this.tokens);
    return(indx);
  }


  public AddressToken getAddrSpec()
  {
    int start = getAddrSpecStart();
    int end = this.token.getTokenEnd();
    AddressToken addr_spec;

    end--;
    addr_spec = new AddressToken(start, end, AddressToken.ADDR_SPEC);
    return(addr_spec);
  }


/*
  public AddressToken makePhrase(Vector orig_tokens)
  {
  }
*/


  /***********************
   ***********************
   **  PRIVATE METHODS  **
   ***********************
   ***********************/


  private int matchToken(int orig_start, int start, int end,
                         Vector orig_tokens, Vector tokens)
  {
    if (start > end)
    {
      return(orig_start);
    }

    while (start <= end)
    {
      while (isComment(orig_tokens, orig_start) == true)
      {
        orig_start++;
      }
      orig_start++;
      start++;
    }
    return(orig_start - 1);
  }


  private boolean isComment(Vector tokens, int indx)
  {
    RFC822Token token;

    if ((indx < 0)||(indx >= tokens.size()))
    {
      return(false);
    }

    token = (RFC822Token)tokens.elementAt(indx);
    if (token.getType() == RFC822Token.COMMENT)
    {
      return(true);
    }
    return(false);
  }


  private int getAddrSpecStart()
  {
    RFC822Token token;
    int end = this.token.getTokenEnd();

    token = (RFC822Token)this.tokens.elementAt(end);
    while (token.isSpecialChar('<') == false)
    {
      if (token.isSpecialChar(':') == true)
      {
        break;
      }
      end--;
      token = (RFC822Token)this.tokens.elementAt(end);
    }
    return(end + 1);
  }
}

