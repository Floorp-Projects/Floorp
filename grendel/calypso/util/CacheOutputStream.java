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

import calypso.util.TempFile;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.lang.OutOfMemoryError;

/** This class provides buffering for arbitrarily large streams of data.
    It is an <TT>OutputStream</TT>, so you just write the data you want to
    cache to it.  Then, when you've written all of your data (and called
    <TT>close()</TT>) you can get the data back by calling
    <TT>makeInputStream()</TT>, which returns an <TT>InputStream</TT>.

    <P> If you decide you don't want the data at all, you should call
    <TT>discardBuffer()</TT> as early as possible, to avoid having a temporary
    file stay around longer than necessary.  It's not necessary to call
    <TT>discardBuffer()</TT> if you have called <TT>makeInputStream()</TT>;
    it is called automatically when that stream is closed.  However, it is
    ok to call <TT>discardBuffer()</TT> than once.

    <P> So, one conservative way of using this would be:
    <UL>
    <TT>CacheOutputStream output = new CacheOutputStream();</TT><BR>
    <TT>try {</TT><BR>
    <TT>&nbsp;&nbsp;</TT><I>... write bytes to `output' ...</I><BR>
    <TT>&nbsp;&nbsp;output.close();</TT><BR>
    <BR>
    <TT>&nbsp;&nbsp;InputStream input = output.makeInputStream();</TT><BR>
    <TT>&nbsp;&nbsp;try {</TT><BR>
    <TT>&nbsp;&nbsp;&nbsp;&nbsp;</TT><I>... read bytes from `input' ...</I>
    <BR>
    <TT>&nbsp;&nbsp;} finally {</TT><BR>
    <TT>&nbsp;&nbsp;&nbsp;&nbsp;input.close();</TT><BR>
    <TT>&nbsp;&nbsp;}</TT><BR>
    <BR>
    <TT>} finally {</TT><BR>
    <TT>&nbsp;&nbsp;output.discardBuffer();</TT><BR>
    <TT>}</TT></PRE></UL><P>

    <DL><DT>Implementation:</DT><DD>
        <P> The bytes written to the stream are buffered in memory, until
        a size threshold is reached, or a memory-allocation error occurs.
        At that point, a temporary file is created, and buffering continues
        there.  Therefore, files are not used for small objects; this ensures
        that buffering of small objects is fast, but buffering of arbitrarily
        large objects is possible.
    </DL>
 */

public class CacheOutputStream extends OutputStream {

  protected byte   mem_cache[]         = null;
  protected int    mem_cache_fp        = 0;     // fill pointer
  protected int    mem_cache_max_size  = 0;
  protected double mem_cache_increment = 1.3;   // expansion factor

  protected Object       bs_handle     = null;  // backing store
  protected OutputStream bs_stream     = null;


  /** Creates a CacheOutputStream with default buffer sizes. */
  public CacheOutputStream() {
    this(64 * 1024);
  }

  /** Creates a CacheOutputStream with the given maximum memory usage.
      If an attempt is made to buffer more than the specified number of
      bytes, then the memory buffer will be traded for a disk buffer.
    */
  public CacheOutputStream(int max_resident_size) {
    this(0, max_resident_size);
  }

  /** Creates a CacheOutputStream with the given initial memory buffer size,
      and given maximum memory usage.
      If an attempt is made to buffer more than the specified number of
      bytes, then the memory buffer will be traded for a disk buffer.
    */
  public CacheOutputStream(int probable_size, int max_resident_size) {
    if (probable_size > max_resident_size)
      probable_size = 0;
    if (probable_size < 1024)
      probable_size = 1024;
    mem_cache = new byte[probable_size];
    mem_cache_max_size = max_resident_size;
  }

  public void write(byte bytes[]) throws IOException {
    write (bytes, 0, bytes.length);
  }

  public void write(byte bytes[], int start, int length) throws IOException {
    if (mem_cache != null)
      increaseCapacity(length);
    if (mem_cache != null) {
      System.arraycopy(bytes, start, mem_cache, mem_cache_fp, length);
      mem_cache_fp += length;
    } else {
      bs_stream.write(bytes, start, length);
    }
  }

  public void write(int b) throws IOException {
    if (mem_cache != null)
      increaseCapacity(1);
    if (mem_cache != null) {
      mem_cache[mem_cache_fp++] = (byte) b;
    } else {
      bs_stream.write(b);
    }
  }

  /** Indicate (only) that no more data will be written in to the buffer.
      After calling <TT>close()</TT>, one must eventually call either
      <TT>makeInputStream()</TT> or <TT>discardBuffer()</TT>.
   */
  public void close() throws IOException {
    if (bs_stream != null) {
      bs_stream.close();
      bs_stream = null;
    }
  }

  public void flush() throws IOException {
    if (bs_stream != null)
      bs_stream.flush();
  }


  /** Ensure that there is room to write an additonal `count' bytes.
      If this would exceed the memory cache size, set up the disk cache.
    */
  protected void increaseCapacity(int count) throws IOException {
    if (mem_cache == null)              // already going to backing store...
      return;

    int new_fp = mem_cache_fp + count;
    if (new_fp < mem_cache.length)      // it still fits...
      return;
    if (new_fp < mem_cache_max_size) {  // time to grow the array...
      growMemCache(count);
    } else {                            // time to switch to backing store...
      switchToBackingStore();
    }
  }

  /** Assuming the memory cache is in use, expand the mem_cache to be
      able to hold an additional `count' bytes if necessary. */
  protected void growMemCache(int count) throws IOException {
    int new_fp = mem_cache_fp + count;
    int new_size = (int) (mem_cache.length * mem_cache_increment);
    if (new_size < mem_cache_fp + 1024)
      new_size = mem_cache_fp + 1024;
    if (new_size < new_fp)
      new_size = new_fp;

    try {
      byte new_mem_cache[] = new byte[new_size];
      System.arraycopy(mem_cache, 0, new_mem_cache, 0, mem_cache_fp);
      mem_cache = new_mem_cache;
    } catch (OutOfMemoryError e) {
      // Couldn't allocate a new, bigger vector!
      // That's fine, we'll just switch to backing-store mode.
      switchToBackingStore();
    }
  }

  /** Call this when you're done with this object.
      It's important that you call this if you're not planning on calling
      makeInputStream(); if you don't, a temp file could stay around longer
      than necessary.

      <P> You must not use this CacheOutputStream object after having called
      discardBuffer().

      <P> If you call makeInputStream(), you must not call discardBuffer()
      before you are finished with the returned stream.
   */
  public synchronized void discardBuffer() {
    mem_cache = null;
    mem_cache_fp = 0;

    if (bs_stream != null) {
      try {
        bs_stream.close();
      } catch (IOException e) {
        // just ignore it...
      }
      bs_stream = null;
    }

    discardBackingStore();
  }

  /** Returns an InputStream that returns the data previously written
      into this object.  This may only be called once, and only after
      <TT>close()</TT> has been called.  It is also important that the
      returned InputStream be closed when the caller is done with it;
      failure to do so could cause a temp file to stay around longer
      than necessary.
   */
  public InputStream makeInputStream()
    throws IOException, FileNotFoundException {
    close();
    if (mem_cache != null) {
      byte[] v = mem_cache;
      int fp = mem_cache_fp;
      mem_cache = null;
      mem_cache_fp = 0;
      return new ByteArrayInputStream(v, 0, fp);
    } else if (bs_handle != null) {
      return createBackingStoreInputStream();
    } else {
      return null;
    }
  }

  /** Calls discardBuffer(), in case this object was dropped without being
      properly shut down. */
  protected void finalize() {
    discardBuffer();
  }


  /** Assuming the memory cache is in use, switch to using the disk cache. */
  protected void switchToBackingStore() throws IOException {
    TempFile f = TempFile.TempName("");
    OutputStream s = f.create();
    if (mem_cache_fp > 0)
      s.write(mem_cache, 0, mem_cache_fp);

    mem_cache = null;
    mem_cache_fp = 0;

    bs_handle = f;
    bs_stream = s;
  }

  /** Assuming backing-store is in use, delete the underlying temp file. */
  protected void discardBackingStore() {
    if (bs_handle != null) {
      TempFile f = (TempFile) bs_handle;
      f.delete();
      bs_handle = null;
    }
  }

  /** Assuming backing-store is in use, creates an InputStream that reads
      from the underlying TempFile, and will call this.discardBuffer()
      when that InputStream reaches EOF. */
  protected InputStream createBackingStoreInputStream()
    throws FileNotFoundException {
    TempFile f = (TempFile) bs_handle;
    InputStream s = new BufferedInputStream(new FileInputStream(f.getName()));
    return new CacheInputStream(this, s);
  }



/*
  public static final void main(String a[])
    throws FileNotFoundException, IOException {
    String s = "0123456789";
    String ss = "";
    while (s.length() < 1000)
      s = s + s;
    System.err.println("s len " + s.length());

    CacheOutputStream output = new CacheOutputStream(0);
    try {

      for (int ii = 0; ii < (80 * 1024); ii += s.length()) {
        System.err.println("writing " + s.length());
        output.write(s.getBytes());
        ss += s;
      }

      output.close();

      InputStream input = output.makeInputStream();
      try {
        byte b[] = new byte[4000];
        int i;
        String s2 = "";
        do {
          i = input.read(b);
          if (i >= 0) {
            System.err.println("read " + i);
            s2 = s2 + new String(b, 0, i);
          }
        } while (i >= 0);

        if (ss.equals(s2))
          System.err.println("read correct bytes");
        else
          throw new Error("read wrong bytes");

        } finally {
          input.close();
        }

    } finally {
      output.discardBuffer();
    }
  }
 */

}


/** This is the private class that is used for reading data out of the
    file underlying a CacheOutputStream.  This is used only when a file is
    being used for cacheing, not when the memory buffer is being used.
    Mainly this just passes the various InputStream methods along to
    the underlying file stream; but it adds one other thing, which is
    that as soon as the underlying file stream hits eof, the underlying
    file is deleted (by calling CacheOutputStream.discardBuffer().)
 */
class CacheInputStream extends InputStream {

  protected CacheOutputStream stream_cache;
  protected InputStream stream;

  CacheInputStream(CacheOutputStream sc, InputStream s) {
    stream_cache = sc;
    stream = s;
  }

  public int available() throws IOException {
    return stream.available();
  }

  public void close() throws IOException {
    stream.close();
    stream = null;
    discardBackingStore();
  }

  public void mark(int i) {
    stream.mark(i);
  }

  public boolean markSupported() {
    return stream.markSupported();
  }

  public int read() throws IOException {
    int result = stream.read();
    if (result < 0) discardBackingStore();
    return result;
  }

  public int read(byte b[]) throws IOException {
    int result = stream.read(b);
    if (result < 0) discardBackingStore();
    return result;
  }

  public int read(byte b[], int start, int length) throws IOException {
    int result = stream.read(b, start, length);
    if (result < 0) discardBackingStore();
    return result;
  }

  public void reset() throws IOException {
    stream.reset();
  }

  public long skip(long i) throws IOException {
    return stream.skip(i);
  }

  protected synchronized void discardBackingStore() {
    if (stream_cache != null) {
      stream_cache.discardBuffer();
      stream_cache = null;
    }
  }

  protected void finalize() {
    discardBackingStore();
  }
}
