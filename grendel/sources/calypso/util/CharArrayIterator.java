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

import java.text.*;

public final class CharArrayIterator implements CharacterIterator
{
  private char[] fText;
  private int fStart;
  private int fEnd;
  private int fPosition;

  public CharArrayIterator(char[] aText, int aStartOffset, int aLength)
  {
    int endOffset = aStartOffset + aLength;

    if (aStartOffset < 0 || aLength < 0 ||
        (aStartOffset + aLength) > aText.length)
      throw new IllegalArgumentException("parameter out of range");

    fText = aText;
    fStart = aStartOffset;
    fEnd = endOffset;
    fPosition = 0;
  }

  public char first()
  {
    fPosition = fStart;
    return fText[fPosition];
  }

  public char last()
  {
    fPosition = fEnd - 1;
    return fText[fPosition];
  }

  public char current()
  {
    if (fPosition < fStart || fPosition >= fEnd)
      return CharacterIterator.DONE;

    return fText[fPosition];
  }

  public char next()
  {
    fPosition += 1;

    if (fPosition >= fEnd)
      return CharacterIterator.DONE;

    return fText[fPosition];
  }

  public char previous()
  {
    fPosition -= 1;

    if (fPosition < fStart)
      return CharacterIterator.DONE;

    return fText[fPosition];
  }

  public char setIndex(int aPosition)
  {
    if (aPosition < fStart || aPosition >= fEnd)
      throw new IllegalArgumentException("aPosition out of range");

    fPosition = aPosition;
    return fText[fPosition];
  }

  public int getBeginIndex()
  {
    return fStart;
  }

  public int getEndIndex()
  {
    return fEnd;
  }

  public int getIndex()
  {
    return fPosition;
  }

  public Object clone()
  {
    try
    {
      CharArrayIterator other
        = (CharArrayIterator) super.clone();

      return other;
    }
    catch (CloneNotSupportedException e)
    {
      throw new InternalError();
    }
  }
}
