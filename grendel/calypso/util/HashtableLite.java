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

class ElementEnumeration
    implements Enumeration
{
    private int fIndex;
    private HashtableLite fTable;

    public ElementEnumeration (HashtableLite theTable)
    {
        fTable = theTable;
    }

    public  boolean hasMoreElements ()
    {
        if (fIndex < fTable.fCount) return true;
        else return false;
    }

    public  Object  nextElement ()
    {
        return fTable.fItems[(fIndex++*2) + 1];
    }
}

class KeyEnumeration
    implements Enumeration
{
    private int fIndex;
    private HashtableLite fTable;

    KeyEnumeration (HashtableLite theTable)
    {
        fTable = theTable;
    }

    public  boolean hasMoreElements ()
    {
        if (fIndex < fTable.fCount) return true;
        else return false;
    }

    public  Object  nextElement ()
    {
        return fTable.fItems[fIndex++*2];
    }
}

  /**
   * A utility class to avoid the pain (and memory bloat)
   * of making lots of small hashtables. A real HashTable
   * isn't created until the number of elements grows past
   * a certain number (defined internally to HashtableLite).
   *
   * It sure would have been nice if Hashtables were an interface.
   *
   * @author    thom   08-19-97 2:06am
   */

public class HashtableLite extends Object
    implements java.lang.Cloneable
{
    int fCount;
    Object fItems[];       // [0] = key, [1] = item, etc
    AtomHashtable fRealTable;

    private static final int maxItems = 4;

    public HashtableLite ()
    {
    }

    public void clear()
    {
        fCount = 0;
        fRealTable = null;
        fItems = null;
    }

    public Object clone()
    {
        HashtableLite theClone = new HashtableLite();

        if (fRealTable != null)
        {
            theClone.fRealTable = (AtomHashtable) fRealTable.clone();
        }
        else if (fCount > 0)
        {
            theClone.fItems = (Object[]) fItems.clone();
            theClone.fCount = fCount;
        }
        return theClone;
    }

    public boolean contains (Object item)
    {
        if (fCount > 0)
        {
            for (int i = 0; i < fCount; ++i )
                if (fItems[(i*2) + 1] == item)
                    return true;
            return false;
        }
        if (fRealTable != null)
            return fRealTable.contains (item);

        return false;
    }

    public boolean containsKey (Atom key)
    {
        if (fCount > 0)
        {
            for (int i = 0; i < fCount; ++i )
                if (fItems[i*2] == key)
                    return true;
            return false;
        }
        if (fRealTable != null)
            return fRealTable.containsKey (key);

        return false;
    }

    public int count ()
    {
        if (fRealTable != null) return fRealTable.count();
        return fCount;
    }

    public Enumeration elements()
    {
        if (fCount > 0)
            return new ElementEnumeration (this);

        if (fRealTable != null)
            return fRealTable.elements();

        return NullEnumeration.kInstance;
    }

    public Object[] elementsArray()
    {
        if (fRealTable != null)
            return fRealTable.elementsArray();
        Assert.NotYetImplemented("Arg");
        return null;
    }

    public Vector elementsVector ()
    {
        if (fRealTable != null)
            return fRealTable.elementsVector();
        Assert.NotYetImplemented("Arg");
        return null;
    }

    /*public void decode (Decoder aDecoder)
    {
        if (fRealTable != null)
            fRealTable.decode(aDecoder);

    }

    public void describeClassInfo (ClassInfo theInfo)
    {
        if (fRealTable != null)
            fRealTable.describeClassInfo(theInfo);

    }

    public void encode (Encoder anEncoder)
    {
        if (fRealTable != null)
            fRealTable.encode (anEncoder);

    }

    public void finishDecoding()
    {
        if (fRealTable != null)
            fRealTable.finishDecoding();

    }*/

    public Object get (Atom key)
    {
        if (fCount > 0)
        {
            for (int i = 0; i < fCount; ++i )
                if (fItems[i*2] == key)
                    return fItems[(i*2) + 1];
            return null;
        }
        else if (fRealTable != null)
            return fRealTable.get (key);

        return null;
    }

    public boolean isEmpty ()
    {
        if (fCount == 0 && fRealTable == null)
            return true;

        return false;
    }

    public Object put (Atom key, Object item)
    {
        if (fRealTable != null)
        {
            return fRealTable.put (key, item);
        }
        else
        {
            if (fItems == null) fItems = new Object[maxItems*2];

            // is it already in the table?

            for (int i = 0; i < fCount; ++i )
                if (fItems[i*2] == key)
                {
                    Object oldItem = fItems[(i*2) + 1];
                    fItems[(i*2) + 1] = item;
                    return oldItem;
                }

            // no, must add new key/item pair

            if (fCount == maxItems) // spill over to real hashtable?
            {
                fRealTable = new AtomHashtable();

                for (int i = 0; i < fCount; ++i )
                    fRealTable.put (fItems[i*2], fItems[(i*2) + 1]);

                fItems = null;
                fCount = 0;

                return fRealTable.put (key, item);
            }

            // simple case, add the item

            fItems[fCount*2] = key;
            fItems[(fCount*2) + 1] = item;
            ++fCount;
        }
        return null;
    }

    public Enumeration keys ()
    {
        if (fRealTable != null)
            return fRealTable.keys();

        if (fCount > 0) return new KeyEnumeration (this);

        return NullEnumeration.kInstance;
    }

    public Atom[] keysArray()
    {
        if (fRealTable != null)
            return fRealTable.keysArray();
        Assert.NotYetImplemented("Arg");
        return null;
    }

    public Vector keysVector()
    {
        if (fRealTable != null)
            return fRealTable.keysVector();
        Assert.NotYetImplemented("Arg");
        return null;
    }

    public Object remove (Atom key)
    {
        if (fRealTable != null)
            return fRealTable.remove(key);

        if (fCount > 0)
            for (int i = 0; i < fCount; ++i )
                if (fItems[i*2] == key)
                {
                    Object oldItem = fItems[i*2 + 1];

                    --fCount;
                    for (int j = i; j < fCount - 1; ++j)
                    {
                        fItems[(j*2) + 1] = fItems[((j+1)*2) + 1];
                        fItems[ j*2]      = fItems[ (j+1)*2];
                    }
                    return oldItem;
                }
        return null;
    }

    public int size()
    {
        if (fRealTable != null)
            return fRealTable.size();

        return fCount;
    }

    public String toString ()
    {
        if (fRealTable != null)
            return fRealTable.toString();

        return "Hi, I'm a hack";
    }
}


