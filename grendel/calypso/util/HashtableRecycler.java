/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 */

package calypso.util;

import java.util.*;

/**
 * Static recycler class for java.util.Hashtable
 *
 * @author  psl   04-23-97 6:48pm
 */
public final class HashtableRecycler
{
  static Recycler gRecycler;

  /**
   * Get one
   *
   * @return
   * @exception
   * @author    psl   04-23-97 6:49pm
   */
  public static synchronized Hashtable Alloc ()
  {
    Hashtable result = null;

    if (null != gRecycler)
      result = (Hashtable)gRecycler.getRecycledObject ();
    if (null == result)
      result = new Hashtable ();
    return result;
  }

  /**
   * Get one with an initial size
   * <I>since the growth mechanism is private (for now) a recycled
   * table won't really grow to capacity</I>
   *
   * @param aCapacity desired initial capacity
   * @return
   * @exception
   * @author    psl   04-23-97 6:49pm
   */
  public static synchronized Hashtable Alloc (int aCapacity)
  {
    Hashtable result = null;

    if (null != gRecycler)
      result = (Hashtable)gRecycler.getRecycledObject ();
    if (null == result)
      result = new Hashtable (aCapacity);
    return result;
  }

  /**
   * Recycle a hashtable
   * (it will be cleared by the recycler)
   *
   * @param aTable  table to be recycled
   * @return
   * @exception
   * @author    psl   04-23-97 6:50pm
   */
  public static synchronized void Recycle (Hashtable aTable)
  {
    if (null == gRecycler)
      gRecycler = new Recycler ();
    aTable.clear ();
    gRecycler.recycle (aTable);
  }

  /**
   * Empty the recycler, it's earth day
   *
   * @param
   * @return
   * @exception
   * @author    psl   04-21-97 4:29pm
   */
  static synchronized public void EmptyRecycler ()
  {
    if (null != gRecycler)
      gRecycler.empty ();
  }

  /**
   * finalize class for unloading
   *
   * @param
   * @return
   * @exception
   * @author    psl   04-21-97 4:29pm
   */
  static void classFinalize () throws Throwable
  {
    // Poof! Now we are unloadable even though we have a static
    // variable.
  }
}

