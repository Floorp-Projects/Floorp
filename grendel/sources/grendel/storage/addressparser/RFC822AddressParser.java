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


/*
 * Parse a comma separated list of RFC822 addresses.
 * The important thing here is all the ERROR cases.
 * If you aren't rigidly RFC822 compliant, you will hit
 * one of them.
 */
class RFC822AddressParser
{
  static final int NO_ERROR = 0;
  static final int LIST_OF_ADDRESSES = -1;
  static final int NO_DOMAIN = -2;
  static final int NO_VECTOR = -3;
  static final int COMMENTS_IN_VECTOR = -4;
  static final int EXTRA_TOKENS_AT_START = -5;
  static final int NO_ADDR_SPEC = -6;
  static final int NO_ROUTE_ADDR = -7;
  static final int BAD_ROUTE = -8;
  static final int BAD_ROUTE_DOMAIN = -9;
  static final int EXTRA_TOKENS_IN_ROUTE_ADDR = -10;
  static final int NO_PHRASE = -11;
  static final int EXTRA_TOKENS_IN_MAILBOX_LIST = -12;
  static final int NO_LOCAL_PART = -13;
  static final int BAD_GROUP = -14;
  static final int ILLEGAL_SPECIAL = -15;

  private Vector address_list;
  private Vector tokens;
  private Vector all_tokens;
  private int error_code;

  /*
   * This is the complete vector of tokens
   * straight from the tokenizer.
   */
  public RFC822AddressParser(Vector tokens) throws RFC822ParserException
  {
    AddressToken addr_token = null;
    int num;
    int start, end;

    this.error_code = NO_ERROR;
    this.all_tokens = tokens;
    this.address_list = new Vector();
    this.tokens = stripComments(tokens);

    num = this.tokens.size();

    if (num == 0)
    {
      // ERROR: zero length input vector.
      this.error_code = NO_VECTOR;
      return;
    }

    start = 0;
    end = num - 1;
    /*
     * Arbitrary commas can start and end this list
     */
    start = skipForwardCommas(start, end);
    end = skipCommas(end);
    if (start > end)
    {
      // ERROR: zero length input vector.
      this.error_code = NO_VECTOR;
      return;
    }

    addr_token = collectAddress(start, end);
    if (addr_token == null)
    {
      return;
    }
    addMailAddress(addr_token);
    end = addr_token.getTokenStart() - 1;
    while (end >= start)
    {
      RFC822Token token;

      token = (RFC822Token)this.tokens.elementAt(end);

      /*
       * Skip the comma separator if found, but
       * don't require it.
       */
      if (token.isSpecialChar(',') == true)
      {
        end = skipCommas(end);
      }

      addr_token = collectAddress(start, end);
      if (addr_token == null)
      {
        return;
      }
      addMailAddress(addr_token);
      end = addr_token.getTokenStart() - 1;
    }
    if (addr_token.getTokenStart() != start)
    {
      // ERROR: All tokens in input vector not part
      //       of a legal address list.
      this.error_code = EXTRA_TOKENS_AT_START;
      return;
    }
  }


  public Vector getTokenList()
  {
    return(this.tokens);
  }


  public boolean isError()
  {
    if (this.error_code != NO_ERROR)
    {
      return(true);
    }
    return(false);
  }


  public String getErrorString()
  {
    String err;

    switch(this.error_code)
    {
      case NO_ERROR:
        err = new String("No error.");
        break;
      case LIST_OF_ADDRESSES:
        err = new String("Cannot handle non-comma separated list of addresses!");
        break;
      case NO_DOMAIN:
        err = new String("No valid domain.");
        break;
      case NO_VECTOR:
        err = new String("No input vector.");
        break;
      case COMMENTS_IN_VECTOR:
        err = new String("Comment tokens in input vector.");
        break;
      case EXTRA_TOKENS_AT_START:
        err = new String("Extra tokens at the start of the input vector after parsing address list.");
        break;
      case NO_ADDR_SPEC:
        err = new String("No valid address specification.");
        break;
      case NO_ROUTE_ADDR:
        err = new String("No valid route address.");
        break;
      case BAD_ROUTE:
        err = new String("Bad route format.");
        break;
      case BAD_ROUTE_DOMAIN:
        err = new String("Bad domain in a route list.");
        break;
      case EXTRA_TOKENS_IN_ROUTE_ADDR:
        err = new String("Extra tokens within the bounds of the route address.");
        break;
      case NO_PHRASE:
        err = new String("No valid phrase.");
        break;
      case EXTRA_TOKENS_IN_MAILBOX_LIST:
        err = new String("Extra tokens within the bounds of the mailbox list.");
        break;
      case NO_LOCAL_PART:
        err = new String("No valid local part.");
        break;
      case BAD_GROUP:
        err = new String("Bad group format.");
        break;
      case ILLEGAL_SPECIAL:
        err = new String("Found a special character where another token type was expected.");
        break;
      default:
        err = new String("");
    }
    return(err);
  }


  public Vector getAddressList()
  {
    return(this.address_list);
  }


  /***********************
   ***********************
   **  PRIVATE METHODS  **
   ***********************
   ***********************/


  /*
   * create a new vector from the passed vector with all
   * the comments stripped from it.
   */
  private Vector stripComments(Vector tokens)
  {
    Vector new_vec = new Vector();
    int num = tokens.size();

    for (int i=0; i < num; i++)
    {
      RFC822Token token = (RFC822Token)tokens.elementAt(i);
      if (token.getType() != RFC822Token.COMMENT)
      {
        new_vec.addElement(token);
      }
    }
    return(new_vec);
  }


  /*
   * Add an address (either group or mailbox) to the
   * beginning of an address list vector.
   */
  private void addMailAddress(AddressToken addr_token)
  {
    MailAddressToken address;

    if (addr_token.getType() == AddressToken.GROUP)
    {
      address = new MailAddressToken((GroupToken)addr_token);
    }
    else
    {
      address = new MailAddressToken((MailboxToken)addr_token);
    }
    this.address_list.insertElementAt((Object)address, 0);
  }


  /*
   * Collect an address.  Starts from a rightmost
   * token and works left.
   * In all cases where we return null we set an error code
   */
  private AddressToken collectAddress(int start, int end) throws
                                      RFC822ParserException
  {
    MailboxToken mailbox = null;
    AddressToken addr_token = null;
    RFC822Token token;
    int type;

    token = (RFC822Token)this.tokens.elementAt(end);
    type = token.getType();
    if (type == RFC822Token.SPECIAL_CHAR)
    {
      /*
       * A '>' must be the right edge of a
       * route-addr which must be a mailbox
       * made up of a phrase route-addr pair.
       */
      if (token.isSpecialChar('>') == true)
      {
        mailbox = collectMailbox(end);
        if (mailbox == null)
        {
          return(null);
        }
        return((AddressToken)mailbox);
      }
      /*
       * A ';' must be the right edge of a
       * group which must have a ':' separating
       * a phrase from zero or more mailboxes
       * in a comma separated list.
       */
      else if (token.isSpecialChar(';') == true)
      {
        GroupToken group;

        group = collectGroup(end);
        if (group == null)
        {
          return(null);
        }
        return((AddressToken)group);
      }
      /*
       * No other special character can terminate a
       * legal address.
       */
      else
      {
        this.error_code = ILLEGAL_SPECIAL;
        return(null);
      }
    }
    else if (type == RFC822Token.COMMENT)
    {
      // ERROR: No comments in address vector.
      this.error_code = COMMENTS_IN_VECTOR;
      return(null);
    }
    else
    {
      addr_token = collectAddrSpec(end);
      // if returns null error code already set.
      if (addr_token == null)
      {
        return(null);
      }
      mailbox = new MailboxToken(addr_token);
      return((AddressToken)mailbox);
    }
  }


  /*
   * Collect a group having been passed the rightmost ';'.
   * In all cases where we return null we set an error code.
   */
  private GroupToken collectGroup(int indx2) throws RFC822ParserException
  {
    GroupToken group = null;
    Vector mailboxes = new Vector();
    AddressToken phrase;
    RFC822Token token;
    int colon_at;
    int indx;

    /*
     * A ';' must be the right edge of a group which
     * must have a ':' separating a phrase from zero or
     * more mailboxes in a comma separated list.
     */
    token = (RFC822Token)this.tokens.elementAt(indx2);

    if (token.isSpecialChar(';') == false)
    {
      // ERROR: group must be terminated
      //        by a ';'.
      this.error_code = BAD_GROUP;
      return(null);
    }

    /*
     * Stupid RFC allows extra commas here at the end
     * of the list.
     */
    indx = skipCommas(indx2 - 1);

    token = (RFC822Token)this.tokens.elementAt(indx);

    /*
     * We have one or more mailboxes in a comma
     * separated list.
     */
    if (token.isSpecialChar(':') == false)
    {
      MailboxToken tmp_mbox;
      RFC822Token comma;

      tmp_mbox = collectMailbox(indx);
      // if returns null error code already set.
      if (tmp_mbox == null)
      {
        return(null);
      }
      mailboxes.insertElementAt((Object)tmp_mbox, 0);

      indx = tmp_mbox.getTokenStart();
      comma = (RFC822Token)this.tokens.elementAt(indx - 1);
      while (comma.isSpecialChar(',') == true)
      {
        /*
         * Skip the extra commas the RFC allows here
         */
        indx = skipCommas(indx - 1);

        /*
         * List could end here with commas
         */
        comma = (RFC822Token)this.tokens.elementAt(indx);
        if (comma.isSpecialChar(':') == true)
        {
          indx++;
          break;
        }

        tmp_mbox = collectMailbox(indx);
        // if returns null error code already set.
        if (tmp_mbox == null)
        {
          return(null);
        }
        mailboxes.insertElementAt((Object)tmp_mbox, 0);

        indx = tmp_mbox.getTokenStart();
        comma = (RFC822Token)this.tokens.elementAt(indx - 1);
      }
      if (comma.isSpecialChar(':') == false)
      {
        // ERROR: group contained more than
        //        a comma separated list of
        //        mailboxes with the
        //        ':' to ';' pair.
        this.error_code = EXTRA_TOKENS_IN_MAILBOX_LIST;
        return(null);
      }
      colon_at = indx - 1;
    }
    /*
     * This is the case of an empty mailbox list
     * in the group.
     */
    else
    {
      colon_at = indx;
    }

    phrase = collectPhrase(colon_at);
    // if returns null error code already set.
    if (phrase == null)
    {
      return(null);
    }
    group = new GroupToken(phrase, mailboxes, indx2);
    return(group);
  }


  /*
   * Function to skip arbitrary number of commas.
   * Needed because of the RFC's stupid comma separated list
   * rules.
   * Passed the index of possibly the first comma,
   * returns the index of the first non-comma.
   */
  private int skipCommas(int indx)
  {
    RFC822Token comma;

    if (indx >= this.tokens.size())
    {
      return(indx);
    }

    comma = (RFC822Token)this.tokens.elementAt(indx);
    while(comma.isSpecialChar(',') == true)
    {
      indx--;
      if (indx < 0)
      {
        break;
      }
      comma = (RFC822Token)this.tokens.elementAt(indx);
    }
    return(indx);
  }


  /*
   * Like skipCommas above but skipping forward
   * instead of backwards.
   */
  private int skipForwardCommas(int indx, int end)
  {
    RFC822Token comma;

    comma = (RFC822Token)this.tokens.elementAt(indx);
    while(comma.isSpecialChar(',') == true)
    {
      indx++;
      if (indx > end)
      {
        break;
      }
      comma = (RFC822Token)this.tokens.elementAt(indx);
    }
    return(indx);
  }


  /*
   * Passing in the final '>' token of a route-addr, work backwards
   * through the tokens to the matching opening '<' token.
   */
  private int findBeginRouteAddr(int end)
  {
    int indx = end - 1;

    while (indx >= 0)
    {
      RFC822Token token;

      token = (RFC822Token)this.tokens.elementAt(indx);
      if (token.isSpecialChar('<') == true)
      {
        break;
      }
      indx--;
    }

    if (indx >= 0)
    {
      return(indx);
    }

    // ERROR:  No matching '<' to '>' pair.
    return(NO_ROUTE_ADDR);
  }


  /*
   * Starting at the suspected terminal sub-domain token
   * parse up to the beginning of the domain.
   * All legal domains begin with an '@' symbol.
   * To support the illegal but very common case of a local-part
   * with no domain, need to return a special error for
   * a missing domain.
   */
  private int findBeginDomain(int end)
  {
    int indx = end;
    boolean need_dot = false;

    while (indx >= 0)
    {
      RFC822Token token;

      token = (RFC822Token)this.tokens.elementAt(indx);
      if (token.isSpecialChar('@') == true)
      {
        break;
      }
      /*
       * Must be a '.' separated list of sub-domains.
       */
      else if ((token.isSpecialChar('.') == true)&&
               (need_dot == true))
      {
        need_dot = false;
      }
      /*
       * Between '.' symbols, only atoms or domain-literals
       * make legal sub-domains.
       */
      else if (((token.getType() == RFC822Token.ATOM)||
                (token.getType() == RFC822Token.DOMAIN_LITERAL))&&
               (need_dot == false))
      {
        need_dot = true;
      }
      else
      {
        // ERROR:  Encountered some token out of
        //         position to make a legal domain.
        return(NO_DOMAIN);
      }
      indx--;
    }

    /*
     * Must contain at least one sub-domain after the '@'
     * symbol to be a legal domain.
     */
    if ((indx >= 0)&&(indx < end))
    {
      return(indx);
    }

    // ERROR:  No legal domain.
    return(NO_DOMAIN);
  }


  /*
   * A local-part looks a lot like a domain.  The difference
   * being it can have quoted-strings as well as atoms between the '.'
   * symbols, while the domain can have domain-literals as well
   * as atoms between the '.' symbols.
   * So for a list of just '.' separated atoms, you depend
   * on the '@' symbol to know which you are.
   *
   * It is illegal but common for the domain to be ommitted.
   * We will treat this as legal.
   */
  private int findBeginLocalPart(int end)
  {
    int indx = end;
    boolean need_dot = false;

    while (indx >= 0)
    {
      RFC822Token token;

      token = (RFC822Token)this.tokens.elementAt(indx);
      if ((token.isSpecialChar('.') == true)&&
          (need_dot == true))
      {
        need_dot = false;
      }
      else if (((token.getType() == RFC822Token.QUOTED_STRING)||
                (token.getType() == RFC822Token.ATOM))&&
               (need_dot == false))
      {
        need_dot = true;
      }
      /*
       * Break the local part on the first non-matching
       * token you find.
       */
      else
      {
        break;
      }
      indx--;
    }

    /*
     * Since we broke on the terminating token, increment
     * to get to the start of the local-part.
     */
    indx++;

    /*
     * Must contain at least one word.
     * Cannot start with a '.' symbol.
     */
    if ((need_dot == true)&&(indx >= 0)&&(indx <= end))
    {
      return(indx);
    }

    // ERROR: Not a legal local-part.
    return(NO_LOCAL_PART);
  }


  /*
   * A legal phrase is always followed by a special symbol
   * (either ':' or '<').  The passed index is assumed to be that
   * symbol, and we search back for the start of the phrase.
   */
  private int findBeginPhrase(int end)
  {
    int indx = end - 1;

    while (indx >= 0)
    {
      RFC822Token token;

      token = (RFC822Token)this.tokens.elementAt(indx);
      if ((token.getType() == RFC822Token.QUOTED_STRING)||
          (token.getType() == RFC822Token.ATOM))
      {
        // OK
      }
      else
      {
        break;
      }
      indx--;
    }

    /*
     * Since we broke on the terminating token, increment
     * to get to the start of the phrase.
     */
    indx++;

    /*
     * A legal index must have at least one word in
     * the phrase.
     */
    if ((indx >= 0)&&(indx < end))
    {
      return(indx);
    }

    // ERROR:  Illegal phrase.
    return(NO_PHRASE);
  }


  /*
   * Having been passed the rightmost token, collect the
   * phrase and pass it back as an address token.
   * On an error return null and set an error code.
   */
  private AddressToken collectPhrase(int indx2)
  {
    int indx1 = findBeginPhrase(indx2);
    AddressToken addr_token;

    if (indx1 == NO_PHRASE)
    {
      this.error_code = NO_PHRASE;
      return(null);
    }

    addr_token = new AddressToken(indx1, indx2 - 1, AddressToken.PHRASE);
    return(addr_token);
  }


  /*
   * Having been passed the rightmost token, collect the
   * route-addr and pass it back as an address token.
   * On an error return null and set an error code.
   */
  private AddressToken collectRouteAddr(int indx2)
  {
    AddressToken addr_token;
    RFC822Token token;
    int indx;
    int indx1 = findBeginRouteAddr(indx2);

    if ((indx1 == NO_ROUTE_ADDR)||(indx1 >= (indx2 - 1)))
    {
      // ERROR: Illegal to have a route-addr with
      //        an empty body.
      this.error_code = NO_ROUTE_ADDR;
      return(null);
    }
    addr_token = collectAddrSpec(indx2 - 1);
    // if returns null error code already set.
    if (addr_token == null)
    {
      return(null);
    }
    indx = addr_token.getTokenStart();

    /*
     * The route is optional, look for the closing ':'
     * symbol to see if we have one.
     */
    token = (RFC822Token)this.tokens.elementAt(indx - 1);
    if (token.isSpecialChar(':') == true)
    {
      AddressToken route = collectRoute(indx - 1);

      // if returns null error code already set.
      if (route == null)
      {
        return(null);
      }
      indx = route.getTokenStart();
    }

    token = (RFC822Token)this.tokens.elementAt(indx - 1);
    if (token.isSpecialChar('<') == false)
    {
      // ERROR:  More in the route-addr than a legal
      //         addr-spec and an optional route.
      this.error_code = EXTRA_TOKENS_IN_ROUTE_ADDR;
      return(null);
    }
    addr_token = new AddressToken(indx - 1, indx2, AddressToken.ROUTE_ADDR);
    return(addr_token);
  }


  /*
   * Having been passed the rightmost token, collect the
   * addr-spec and pass it back as an address token.
   * On an error return null and set an error code.
   */
  private AddressToken collectAddrSpec(int indx2)
  {
    int at_sign = findBeginDomain(indx2);
    int indx1;
    AddressToken addr_token;

    if (at_sign == NO_DOMAIN)
    {
      indx1 = findBeginLocalPart(indx2);
    }
    else
    {
      indx1 = findBeginLocalPart(at_sign - 1);
    }

    if (indx1 == NO_LOCAL_PART)
    {
      this.error_code = NO_ADDR_SPEC;
      return(null);
    }

    addr_token = new AddressToken(indx1, indx2, AddressToken.ADDR_SPEC);

    return(addr_token);
  }

  /*
   * Having been passed the rightmost token, collect the
   * route and pass it back as an address token.
   * On an error return null and set an error code.
   */
  private AddressToken collectRoute(int indx2)
  {
    AddressToken addr_token;
    RFC822Token token;
    int at_sign;
    int indx;

    token = (RFC822Token)this.tokens.elementAt(indx2);
    if (token.isSpecialChar(':') == false)
    {
      // ERROR:  Not a legal route, not ':' terminated.
      this.error_code = BAD_ROUTE;
      return(null);
    }

    /*
     * Stupid RFC allows many commas at end of list.
     */
    indx = skipCommas(indx2 - 1);

    /*
     * Must be at least one domain, look for the '@' symbol
     * that starts it.
     */
    at_sign = findBeginDomain(indx);
    if (at_sign < 1)
    {
      // ERROR:  Must always be room to move back from
      //         the start of a route because it is
      //         always enclosed in a route-addr.
      //         So it is always ok to look for the ','
      //         In the optional comma separated list.
      this.error_code = BAD_ROUTE_DOMAIN;
      return(null);
    }

    /*
     * If there is a comma, there are one or more domains to
     * skip over.
     */
    token = (RFC822Token)this.tokens.elementAt(at_sign - 1);
    while (token.isSpecialChar(',') == true)
    {
      /*
       * Again RFC allows many commas
       */
      indx = skipCommas(at_sign - 1);
      /*
       * These commas may have been the end of the list
       */
      token = (RFC822Token)this.tokens.elementAt(indx);
      if (token.isSpecialChar('<') == true)
      {
        at_sign = indx + 1;
        break;
      }

      at_sign = findBeginDomain(indx);
      if (at_sign < 1)
      {
        // ERROR:  Must always be room to move back from
        //         the start of a route because it is
        //         always enclosed in a route-addr.
        //         So it is always ok to look for the','
        //         In the optional comma separated list.
        this.error_code = BAD_ROUTE_DOMAIN;
        return(null);
      }
      token = (RFC822Token)this.tokens.elementAt(at_sign - 1);
    }

    addr_token = new AddressToken(at_sign, indx2, AddressToken.ROUTE);

    return(addr_token);
  }


  /*
   * Having been passed the rightmost token, collect the
   * mailbox and pass it back as a mailbox token.
   * On an error return null and set an error code.
   */
  private MailboxToken collectMailbox(int indx2) throws RFC822ParserException
  {
    MailboxToken mailbox = null;
    AddressToken addr_token = null;
    AddressToken phrase;
    RFC822Token token;
    int indx;

    /*
     * A legal mailbox either ends in a '>'
     * symbol and is a phrase route-addr pair,
     * or it is a simple addr-spec.
     */
    token = (RFC822Token)this.tokens.elementAt(indx2);
    /*
     * Get the phrase route-addr pair.
     */
    if (token.isSpecialChar('>') == true)
    {
      addr_token = collectRouteAddr(indx2);
      // if returns null error code already set.
      if (addr_token == null)
      {
        return(null);
      }
      indx = addr_token.getTokenStart();
      phrase = collectPhrase(indx);
      if (this.error_code == NO_PHRASE)
      {
        throw new RouteAddrNoPhraseException("No valid phrase with this route address.", addr_token, this.tokens);
      }
      // if returns null error code already set.
      if (phrase == null)
      {
        return(null);
      }
      indx = phrase.getTokenStart();
      mailbox = new MailboxToken(phrase, addr_token);
    }
    /*
     * Get the addr-spec.
     */
    else
    {
      addr_token = collectAddrSpec(indx2);
      // if returns null error code already set.
      if (addr_token == null)
      {
        return(null);
      }
      indx = addr_token.getTokenStart();
      mailbox = new MailboxToken(addr_token);
    }

    return(mailbox);
  }
}

