/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _MORKSINK_
#define _MORKSINK_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKBLOB_
#include "morkBlob.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*| morkSink is intended to be a very cheap buffered i/o sink which
**| writes to bufs and other strings a single byte at a time.  The
**| basic idea is that writing a single byte has a very cheap average
**| cost, because a polymophic function call need only occur when the
**| space between At and End is exhausted.  The rest of the time a
**| very cheap inline method will write a byte, and then bump a pointer.
**|
**|| At: the current position in some sequence of bytes at which to
**| write the next byte put into the sink.  Presumably At points into
**| the private storage of some space which is not yet filled (except
**| when At reaches End, and the overflow must then spill).  Note both
**| At and End are zeroed in the destructor to help show that a sink
**| is no longer usable; this is safe because At==End causes the case
**| where SpillPutc() is called to handled an exhausted buffer space.
**|
**|| End: an address one byte past the last byte which can be written
**| without needing to make a buffer larger than previously.  When At
**| and End are equal, this means there is no space to write a byte,
**| and that some underlying buffer space must be grown before another
**| byte can be written.  Note At must always be less than or equal to
**| End, and otherwise an important invariant has failed severely.
**|
**|| Buf: this original class slot has been commented out in the new
**| and more abstract version of this sink class, but the general idea
**| behind this slot should be explained to help design subclasses.
**| Each subclass should provide space into which At and End can point,
**| where End is beyond the last writable byte, and At is less than or
**| equal to this point inside the same buffer.  With some kinds of
**| medium, such as writing to an instance of morkBlob, it is feasible
**| to point directly into the final resting place for all the content
**| written to the medium.  Other mediums such as files, which write
**| only through function calls, will typically need a local buffer
**| to efficiently accumulate many bytes between such function calls.
**|
**|| FlushSink: this flush method should move any buffered content to 
**| it's final destination.  For example, for buffered writes to a
**| string medium, where string methods are function calls and not just
**| inline macros, it is faster to accumulate many bytes in a small
**| local buffer and then move these en masse later in a single call.
**|
**|| SpillPutc: when At is greater than or equal to End, this means an
**| underlying buffer has become full, so the buffer must be flushed
**| before a new byte can be written.  The intention is that SpillPutc()
**| will be equivalent to calling FlushSink() followed by another call
**| to Putc(), where the flush is expected to make At less then End once
**| again.  Except that FlushSink() need not make the underlying buffer
**| any larger, and SpillPutc() typically must make room for more bytes.
**| Note subclasses might want to guard against the case that both At
**| and End are null, which happens when a sink is destroyed, which sets
**| both these pointers to null as an indication the sink is disabled.
|*/
class morkSink {
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public sink virtual methods

  virtual void FlushSink(morkEnv* ev) = 0;
  virtual void SpillPutc(morkEnv* ev, int c) = 0;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // member variables

  mork_u1*     mSink_At;     // pointer into mSink_Buf
  mork_u1*     mSink_End;    // one byte past last content byte

// define morkSink_kBufSize 256 /* small enough to go on stack */

  // mork_u1      mSink_Buf[ morkSink_kBufSize + 4 ];
  // want plus one for any needed end null byte; use plus 4 for alignment
   
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly morkSink methods

  virtual ~morkSink(); // zero both At and End; virtual for subclasses
  morkSink() { } // does nothing; subclasses must set At and End suitably

  void Putc(morkEnv* ev, int c)
  { 
    if ( mSink_At < mSink_End )
      *mSink_At++ = (mork_u1) c;
    else
      this->SpillPutc(ev, c);
  }
};

/*| morkSpool: an output sink that efficiently writes individual bytes
**| or entire byte sequences to a coil instance, which grows as needed by
**| using the heap instance in the coil to grow the internal buffer.
**|
**|| Note we do not "own" the coil referenced by mSpool_Coil, and
**| the lifetime of the coil is expected to equal or exceed that of this
**| sink by some external means.  Typical usage might involve keeping an
**| instance of morkCoil and an instance of morkSpool in the same
**| owning parent object, which uses the spool with the associated coil.
|*/
class morkSpool : public morkSink { // for buffered i/o to a morkCoil

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public sink virtual methods

  // when morkSink::Putc() moves mSink_At, mSpool_Coil->mBuf_Fill is wrong:

  virtual void FlushSink(morkEnv* ev); // sync mSpool_Coil->mBuf_Fill
  virtual void SpillPutc(morkEnv* ev, int c); // grow coil and write byte

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // member variables
  morkCoil*   mSpool_Coil; // destination medium for written bytes
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly morkSink methods

  static void BadSpoolCursorOrderError(morkEnv* ev);
  static void NilSpoolCoilError(morkEnv* ev);

  virtual ~morkSpool();
  // Zero all slots to show this sink is disabled, but destroy no memory.
  // Note it is typically unnecessary to flush this coil sink, since all
  // content is written directly to the coil without any buffering.
  
  morkSpool(morkEnv* ev, morkCoil* ioCoil);
  // After installing the coil, calls Seek(ev, 0) to prepare for writing.
  
  // ----- All boolean return values below are equal to ev->Good(): -----

  mork_bool Seek(morkEnv* ev, mork_pos inPos);
  // Changed the current write position in coil's buffer to inPos.
  // For example, to start writing the coil from scratch, use inPos==0.

  mork_bool Write(morkEnv* ev, const void* inBuf, mork_size inSize);
  // write inSize bytes of inBuf to current position inside coil's buffer

  mork_bool PutBuf(morkEnv* ev, const morkBuf& inBuffer)
  { return this->Write(ev, inBuffer.mBuf_Body, inBuffer.mBuf_Fill); }
  
  mork_bool PutString(morkEnv* ev, const char* inString);
  // call Write() with inBuf=inString and inSize=strlen(inString),
  // unless inString is null, in which case we then do nothing at all.
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKSINK_ */
