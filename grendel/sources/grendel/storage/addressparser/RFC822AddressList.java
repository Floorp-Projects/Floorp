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
 * This class lets you instantiate an address list object
 * from an input string <i>(usually the right hand side to
 * a </i><tt>To: </tt><i>field)</i>.  You can then detect if they were all valid
 * addresses, or there was some error.
 * You can also access the individual address string to
 * send the address list back out again.
 *
 * @author      Eric Bina
 */
public class RFC822AddressList
{
  RFC822AddressParser parser;
  Vector all_tokens;


  /**
   * This constructor seems to be needed in order to subclass this
   * class.  It should never be directly called as it will create a
   * <b>null</b> and basically useless address list.
   */
  RFC822AddressList()
  {
    this.all_tokens = null;
    this.parser = null;
  }


  /**
   * The input string (usually a <tt>To: </tt> field) is first tokenized, and
   * then parsed into a list of addresses.
   */
  public RFC822AddressList(String str)
  {
    RFC822Tokenizer tokenize = new RFC822Tokenizer(str);

    this.all_tokens = tokenize.getTokens();
    try
    {
      this.parser = new RFC822AddressParser(this.all_tokens);
    }
    catch (RFC822ParserException ex)
    {
      this.parser = null;
      System.out.println("Exception caught!  " + ex.getMessage());
    }
  }


  /**
   * @return    Returns the number of addresses in the list.
   *            The string representation for each address
   *            is indexed from 0 to (size - 1).
   *            A return of 0 means an empty address list, probably
   *            a result of an error in parsing.
   */
  public int size()
  {
    if (this.parser == null)
    {
      return(0);
    }
    return(this.parser.getAddressList().size());
  }


  /**
   * @param     indx    an index from 0 to (size - 1).
   * @return    Either a <b>String</b> representation of the address,
   *            or <b>null</b> if the <i>indx</i> is outside the list.
   *            Also returns <b>null</b> is there was an error which
   *            resulted in no address list.
   */
  public String getAddressString(int indx)
  {
    Vector address_list;
    AddressToken address;

    if (this.parser == null)
    {
      return(null);
    }

    address_list = this.parser.getAddressList();

    if ((indx < 0)||(indx >= address_list.size()))
    {
      return(null);
    }

    address = (AddressToken)address_list.elementAt(indx);
    return(address.getTokenString(this.parser.getTokenList()));
  }


  /**
   * Were there errors in tokenizing and parsing this string?
   */
  /*
   * Errors can only happen parsing, so we let the parser
   * handle all this.
   */
  public boolean isError()
  {
    if (this.parser == null)
    {
      return(false);
    }
    return(this.parser.isError());
  }


  /**
   * @return  A string describing the error if there was one.
   *          Some errors can return an empty string.
   *          No error in the address list returns <tt>"No error."</tt>
   */
  /*
   * Errors can only happen parsing, so we let the parser
   * handle all this.
   */
  public String getErrorString()
  {
    if (this.parser == null)
    {
      return("");
    }
    return(this.parser.getErrorString());
  }
}

