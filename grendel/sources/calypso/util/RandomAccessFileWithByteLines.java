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

/* RandomAccessFileWithByteLines takes a RandomAccessFile, and provides a way
 * to get data a line at a time from it.  The API probably needs rethinking.
 */



package calypso.util;

import calypso.util.Assert;

import java.io.*;

public class RandomAccessFileWithByteLines  {
  static final int INITBUFSIZE = 128;

  protected RandomAccessFile fInput;
  protected byte[] fBuffer = new byte[INITBUFSIZE];
  protected int fOffset;
  protected int fEnd;


  public RandomAccessFileWithByteLines(RandomAccessFile f) {
    fInput = f;
  }

  public boolean readLine(ByteBuf buf) throws IOException {
    buf.setLength(0);
    do {
      for (int i=fOffset ; i<fEnd ; i++) {
        if (fBuffer[i] == '\r' || fBuffer[i] == '\n') {
          buf.append(fBuffer, fOffset, i - fOffset);
          fOffset = i;
          eatNewline();
          return true;
        }
      }
      buf.append(fBuffer, fOffset, fEnd - fOffset);
      fOffset = fEnd;
    } while (fill());
    return (buf.length() > 0);
  }

  /**
   * Eat up one newline if present in the fBuffer. If a newline
   * was eaten up return true otherwise false. This will handle
   * mac (\r), unix (\n) and windows (\r\n) style of newlines
   * transparently.
   */
  public boolean eatNewline() throws IOException {
    boolean eaten = false;
    int amount = fEnd - fOffset;
    if (amount < 2) {
      if (!fillForCapacity(2)) {
        // Couldn't get two characters
        if (fEnd - fOffset == 0) {
          return false;
        }
        if ((fBuffer[fOffset] == '\r') || (fBuffer[fOffset] == '\n')) {
          fOffset++;
          return true;
        }
        return false;
      }
    }
    if (fBuffer[fOffset] == '\r') {
      fOffset++;
      eaten = true;
    }
    if (fBuffer[fOffset] == '\n') {
      eaten = true;
      fOffset++;
    }
    return eaten;
  }


  public long getFilePointer() throws IOException {
    return fInput.getFilePointer() - (fEnd - fOffset);
  }

  public void seek(long loc) throws IOException {
    fInput.seek(loc);
    fOffset = 0;
    fEnd = 0;
  }

  protected boolean fill() throws IOException {
    if (fEnd - fOffset != 0) {
      throw new IOException("fill of non-empty fBuffer");
    }
    fOffset = 0;
    fEnd = fInput.read(fBuffer);
    if (fEnd < 0) {
      fEnd = 0;
      return false;
    }
    return true;
  }

  /**
   * Fill the fBuffer, keeping whatever is unread in the fBuffer and
   * ensuring that "capacity" characters of total filled fBuffer
   * space is available. Return false if the final amount of data
   * in the fBuffer is less than capacity, otherwise return true.
   */
  protected boolean fillForCapacity(int capacity) throws IOException {
    int remainingData = fEnd - fOffset;
    if (remainingData >= capacity) {
      // The fBuffer already holds enough data to satisfy capacity
      return true;
    }

    // The fBuffer needs more data. See if the fBuffer itself must be
    // enlarged to statisfy capacity.
    if (capacity >= fBuffer.length) {
      // Grow the fBuffer to hold enough space
      int newBufLen = fBuffer.length * 2;
      if (newBufLen < capacity) newBufLen = capacity;
      byte newbuf[] = new byte[newBufLen];
      System.arraycopy(fBuffer, fOffset, newbuf, 0, remainingData);
      fOffset = 0;
      fEnd = remainingData;
      fBuffer = newbuf;
    } else {
      if (remainingData != 0) {
        // Slide the data that is currently present in the fBuffer down
        // to the start of the fBuffer
        System.arraycopy(fBuffer, fOffset, fBuffer, 0, remainingData);
        fOffset = 0;
        fEnd = remainingData;
      }
    }

    // Fill up the remainder of the fBuffer
    int mustRead = capacity - remainingData;
    int nb = fInput.read(fBuffer, remainingData,
       fBuffer.length-remainingData);
    if (nb < 0) {
      return false;
    }
    fEnd += nb;
    if (nb < mustRead) {
      return false;
    }
    return true;
  }

  public void readFully(byte[] arr, int off, int length) throws IOException {
    Assert.Assertion(fOffset == fEnd);
    fInput.readFully(arr, off, length);
  }


}

