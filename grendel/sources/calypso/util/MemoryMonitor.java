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

import java.lang.Runtime;
import java.util.logging.*;

/**
 * A utility class to help track memory usage during development.
 * The interface is simple. Just create the monitor, do your business,
 * and then invoke the done method. <p>
 *
 * The figure for the ammount of memory allocated should be accurate
 * but the number for the ammount of garbage can be off by quite a bit
 * if any garbage collection was done between the monitor's creation
 * and the call to done. <p>
 *
 * Currently, it just prints some stuff to the console. I'd like to
 * extend it so it creates a window and displays it's info in that.
 *
 * @author Thom FillABomb :-)
 */
public class MemoryMonitor
{
  Runtime fSystem;

  private static final Logger logger = Logger.getLogger("calypso.util.MemoryMonitor");

  long    fFreeAtStart,
    fFreeAtEnd,
    fGarbageGenerated;

  String  fUserString = null;

  public MemoryMonitor (String monitorDescription)
  {
    fUserString = monitorDescription;
    initialize();
  }

  public MemoryMonitor ()
  {
    initialize();
  }

  protected void initialize ()
  {
    fSystem = Runtime.getRuntime();
    beginMonitor();
  }

  public void done ()
  {
    finishMonitor ();

    if (fUserString != null)
      logger.info(fUserString);

    logger.info("    Starting Free: " + fFreeAtStart + "\n      Ending Free: " + fFreeAtEnd + "\n     Memory Delta: " + (fFreeAtStart - fFreeAtEnd) + "\nGarbage Generated: " + fGarbageGenerated);
  }

  protected void freeUpMemory ()
  {
    // we might want to tickle Kipp's memory Manager here as well...

    fSystem.runFinalization();
    // might want to sleep here
    fSystem.gc();
    fSystem.gc();
  }

  void beginMonitor ()
  {
    freeUpMemory();
    fFreeAtStart = fSystem.freeMemory();
  }

  void finishMonitor ()
  {
    long totalUsed, minusGarbage;

    totalUsed = fSystem.freeMemory();
    freeUpMemory();
    minusGarbage = fSystem.freeMemory();

    // it would also be interesting to purge the recyclers here and
    // see how much memory they're occupying

    fGarbageGenerated = minusGarbage - totalUsed;
    fFreeAtEnd = minusGarbage;
  }
}
