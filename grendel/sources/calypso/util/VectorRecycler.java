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
 * Static recycler class for java.util.Vector
 *
 * @author  psl   04-23-97 6:51pm
 * @see
 */
public final class VectorRecycler
{
  static Recycler gRecycler;

  /**
   * Get one
   *
   * @return
   * @exception
   * @author    psl   04-23-97 6:52pm
   */
  public static synchronized Vector Alloc ()
  {
    Vector result = null;

    if (null != gRecycler)
      result = (Vector)gRecycler.getRecycledObject ();
    if (null == result)
      result = new Vector ();
    return result;
  }

  /**
   * Get one with an initial capacity
   * <I>a recycled vector may be bigger than capacity</I>
   *
   * @param aCapacity minimum desired capacity
   * @return
   * @exception
   * @author    psl   04-23-97 6:52pm
   */
  public static synchronized Vector Alloc (int aCapacity)
  {
    Vector result = null;

    if (null != gRecycler)
      result = (Vector)gRecycler.getRecycledObject ();
    if (null != result)
      result.ensureCapacity (aCapacity);
    else
      result = new Vector (aCapacity);
    return result;
  }

  /**
   * Recycle a vector
   * (it will be emptied by the recycler)
   *
   * @param aVector vector to be recycled
   * @return
   * @exception
   * @author    psl   04-23-97 6:52pm
   */
  public static synchronized void Recycle (Vector aVector)
  {
    if (null == gRecycler)
      gRecycler = new Recycler ();
    aVector.removeAllElements ();
    gRecycler.recycle (aVector);
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

