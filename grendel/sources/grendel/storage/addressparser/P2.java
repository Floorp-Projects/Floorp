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
 * A Test program to test the <b>AddressCorrector</b> class.
 *
 * @see      AddressCorrector
 * @author   Eric Bina
 */
public class P2
{

  public static void main(String args[]) throws IOException
  {
    AddressCorrector address_corr;

    System.out.println("Arg[0] = " + args[0]);
    System.out.println("\n\n");

    System.out.println("Test AddressCorrector");
    address_corr = new AddressCorrector(args[0]);
    if (address_corr.isError())
    {
      System.out.print("ERROR:  ");
      System.out.println(address_corr.getErrorString());
    }
    else
    {
      System.out.print("Address:  ");
      for (int i=0; i < address_corr.size(); i++)
      {
        System.out.print(address_corr.getAddressString(i));
        if (i != (address_corr.size() - 1))
        {
          System.out.print(",\n\t");
        }
      }
    }
    System.out.println("\n\n");
  }
}

