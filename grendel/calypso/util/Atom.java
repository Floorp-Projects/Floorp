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
 * Contributor(s): Edwin Woudt <edwin@woudt.nl>
 */

package calypso.util;

/**
 * Atom's are unique objects that you can use the object address
 * to perform equality tests. This class accepts variants on
 * String's to create the Atom's from.
 */
public final class Atom {
  String fString;
  int fHashCode;

  private static final AtomTable gAtoms = new AtomTable();

  /**
   * Private constructor used by static allocators.
   */
  protected Atom(String aString, int aHashCode) {
    fString = aString;
    fHashCode = aHashCode;
  }

  public String toString() {
    return fString;
  }

  public int hashCode() {
    return fHashCode;
  }

  public boolean equals(Object aObject) {
    return aObject == this;
  }

  protected void finalize() throws Throwable {
    synchronized (gAtoms) {
      gAtoms.remove(fString, fHashCode);
    }
  }

  // XXX deprecated entry point. Use Find instead
  static private int gNoisyUsageCount = 10;
  static public Atom ForObject(Object aObject) {
    synchronized (gAtoms) {
      if (gNoisyUsageCount > 0) {
        try {
          throw new RuntimeException("Atom.ForObject: deprecated method called! Use Atom.Find instead. Object:" + aObject.toString());
        }
        catch(RuntimeException e){
          e.printStackTrace();
        }
        gNoisyUsageCount--;
      }
      return gAtoms.find(aObject.toString());
    }
  }

  /**
   * Find the atom for the given argument string. If the atom doesn't
   * already exist then it is created.
   */
  static public Atom Find(String aString) {
    synchronized (gAtoms) {
      return gAtoms.find(aString);
    }
  }

  /**
   * Find the atom for the given argument string. If the atom doesn't
   * already exist then it is created.
   */
  static public Atom Find(StringBuf aString) {
    synchronized (gAtoms) {
      return gAtoms.find(aString);
    }
  }

  private static int gNextAtomID;
  static public Atom UniqueAtom(String aPrefix) {
    synchronized (gAtoms) {
      StringBuf sbuf = StringBuf.Alloc();
      Atom atom = null;
      for (;;) {
        int id = ++gNextAtomID;
        sbuf.setLength(0);
        sbuf.append(aPrefix);
        sbuf.append(id);
        if (!gAtoms.contains(sbuf)) {
          atom = gAtoms.find(sbuf);
          break;
        }
        // Hmmm. Somebody else recorded an atom with this name. Try
        // again with the next id
      }
      StringBuf.Recycle(sbuf);
      return atom;
    }
  }

  //----------------------------------------------------------------------

  // key's are String's/StringBuf's
  // value's are WeakLink's which hold a weak reference to Atom's

  static final class AtomTable extends HashtableBase {
    Atom find(String aKey) {
      return find(aKey, HashCodeFor(aKey));
    }

    Atom find(StringBuf aKey) {
      return find(aKey, HashCodeFor(aKey));
    }

    private Atom find(Object aKey, int aHash) {
      if (hashCodes == null) {
        // create tables
        grow();
      }
      int index = tableIndexFor(aKey, aHash);
      Atom atom = null;
      WeakLink link = (WeakLink) elements[index];
      if (link == null) {
        if (hashCodes[index] == EMPTY) {
          // If the total number of occupied slots (either with a real
          // element or a removed marker) gets too big, grow the table.
          if (totalCount >= capacity) {
            grow();
            return find(aKey, aHash);
          }
          totalCount++;
        }
        count++;

        hashCodes[index] = aHash;
        keys[index] = aKey.toString();
        elements[index] = link = new WeakLink();
      } else {
        atom = (Atom) link.get();
      }

      // Create atom if necessary
      if (atom == null) {
        atom = new Atom(aKey.toString(), aHash);
        link.setThing(atom);
      }
      return atom;
    }

    // XXX replace with a faster better function
    static final int HashCodeFor(String aString) {
      int h = 0;
      int len = aString.length();
      if (len < 16) {
        for (int i = 0; i < len; i++) {
          h = (h * 37) + aString.charAt(i);
        }
      } else {
        // only sample some characters
        int skip = len / 8;
        for (int i = len, off = 0 ; i > 0; i -= skip, off += skip) {
          h = (h * 39) + aString.charAt(off);
        }
      }
      return h;
    }

    static final int HashCodeFor(StringBuf aString) {
      int h = 0;
      int len = aString.length();
      if (len < 16) {
        for (int i = 0; i < len; i++) {
          h = (h * 37) + aString.charAt(i);
        }
      } else {
        // only sample some characters
        int skip = len / 8;
        for (int i = len, off = 0 ; i > 0; i -= skip, off += skip) {
          h = (h * 39) + aString.charAt(off);
        }
      }
      return h;
    }

    boolean contains(StringBuf aKey) {
      if (hashCodes == null) {
        return false;
      }
      int index = tableIndexFor(aKey, HashCodeFor(aKey));
      WeakLink link = (WeakLink) elements[index];
      if (link != null) {
        return link.get() != null;
      }
      return false;
    }

    void remove(String aKey, int aHashCode) {
      int index = tableIndexFor(aKey, aHashCode);
      count--;
      hashCodes[index] = REMOVED;
      keys[index] = null;
      elements[index] = null;
    }
  }
}
