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
 * A subclass of <b>RFC822AddressList</b>.  Its purpose is to
 * catch exceptions created by known correctable RFC822 parsing
 * error.  It will respond to those exceptions by modifying the
 * input str until it can be parsed into a valid list of addresses.
 * <br><blink><b>A WORK IN PROGRESS</b></blink>
 *
 * @see         RFC822AddressList
 * @author      Eric Bina
 */
public class AddressCorrector extends RFC822AddressList
{
  private Vector mailbox_list;
  private RFC822Mailbox mailboxes[];

  public AddressCorrector(String str)
  {
    RFC822Tokenizer tokenize = new RFC822Tokenizer(str);
    boolean retry = true;

    this.all_tokens = tokenize.getTokens();

    while (retry)
    {
      retry = false;
      try
      {
        this.parser = new RFC822AddressParser(this.all_tokens);
      }
      catch (RouteAddrNoPhraseException ex)
      {
        if (ex.hasRoute() == false)
        {
          int start, end;

          System.out.println("Convert route-addr to addr-spec.");

          start = ex.getOrigRouteAddrStart(this.all_tokens);
          end = ex.getOrigRouteAddrEnd(this.all_tokens);
          this.all_tokens.removeElementAt(end);
          this.all_tokens.removeElementAt(start);

          retry = true;
        }
        else
        {
          System.out.println("Exception caught!  " + ex.getMessage());
        }
      }
      catch (RFC822ParserException ex)
      {
        System.out.println("Other exception caught!  " + ex.getMessage());
      }
    }
  }


  /***********************
   ***********************
   **  PRIVATE METHODS  **
   ***********************
   ***********************/
}

