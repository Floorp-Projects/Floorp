/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 */

package calypso.util;

import java.util.*;

/**
 *
 *
 * @author  gess   04-16-97 1:44pm
 * @version $Revision: 1.1 $
 * @notes   This is a general purpose recycler.
 *          It contains objects of a specified capacity.
 *          If you ask for a recycled object and we have
 *          one, then it's returned as object as you must typecast.
 *          If we don't have a recycled object, we return null.
 */

public class Recycler implements MemoryPressure
{
  Object[]          fBuffer;
  int               fCount;
  int               fCapacity;
  final static  int gDefaultCapacity = 100;

 /******************************************
  * @author   gess   04-16-97 1:40pm
  * @param
  * @return
  * @notes    default constructor, assumes
              buffer capacity of 100
  *******************************************/
  public Recycler()
  {
    MemoryManager.Add(this);
    fCapacity=gDefaultCapacity;
    reset();
  }

 /********************************************************
  * @author   gess   04-16-97 1:40pm
  * @param    aGivenCapacity -- size of underlying buffer
  * @return
  * @notes    constructor with capacity argument
  ********************************************************/
  public Recycler(int aGivenCapacity)
  {
    MemoryManager.Add(this);
    fCapacity=aGivenCapacity;
    reset();
  }

 /********************************************************
  * @author   gess   04-16-97 1:43pm
  * @param    none
  * @return   none
  * @notes    discards original buffer (if present) and creates
              a new buffer of size fCapacity
  *********************************************************/
  public void reset()
  {
    fBuffer= new Object[fCapacity];
    fCount = 0;
  }

 /********************************************************
  * @author   gess   04-16-97 1:43pm
  * @param    anObject -- object to be recycled
  * @return   none
  * @notes    always drop on end of list; when capacity is
              reached, replaced last entry
  *********************************************************/
  public void recycle(Object anObject)
  {
    if (null!=anObject)
    {
      if (null==fBuffer)
      {
        reset();
        fBuffer[fCount++] = anObject;
      }
      else
      {
        if (fCount==fCapacity)
        {
            // Replace last entry with the new one,
            // to improve memory locality
            fBuffer[fCount-1] = anObject;
        }
        else
          fBuffer[fCount++] = anObject;
      }
    }
  }

 /**
  * Recycles the entire contents of the given list.
  *
  * @param
  * @return
  * @exception
  * @author     gess   04-16-97 4:22pm
  **/
  public  void recycle(Object anObjectArray[])
  {
    int count = anObjectArray.length;
    int index;

    for (index = 0; index < count; index++)
    {
      recycle (anObjectArray[index]);
      anObjectArray[index] = null;
    }
  }

 /********************************************************
  * @author   gess   04-16-97 1:43pm
  * @param    none
  * @return   object or null
  * @notes    if we don't have any recycled objects, we
              return null. Objects we DO return must be
              type cast by you to the appropriate type.
  *********************************************************/
  public Object getRecycledObject()
  {
    Object result=null;
    if (fCount>0)
    {
      result=fBuffer[--fCount];
      fBuffer[fCount]=null;
    }
    return result;
  }

 /******************************************
  * @author   gess   04-16-97 1:40pm
  * @param    none
  * @return   none
  * @notes    causes buffer to be released and
              count to be reset to 0.
  *******************************************/
  public void empty()
  {
    fCount = 0;
    fBuffer = null;
  }

 /******************************************
  * @author   gess   04-16-97 1:40pm
  * @param
  * @return
  * @notes    copied from kipps implementation
  *******************************************/
  public void preGC(long aCurrentHeapSpace, long aMaximumHeapSpace)
  {
    // Discard our array of buffers so that the GC can
    // compact the heap nicely.
    empty();
  }

 /******************************************
  * @author   gess   04-16-97 1:41pm
  * @param
  * @return
  * @notes    copied from kipps implementation
  *******************************************/
  public void postGC(long aCurrentHeapSpace, long aMaximumHeapSpace)
  {
  }

 /******************************************
  * @author   gess   04-16-97 1:40pm
  * @param
  * @return
  * @notes
  *******************************************/
  public void panic()
  {
    // Get rid of everything, including ourselves
    empty();
  }

}

