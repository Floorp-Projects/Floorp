/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

#ifndef _MORKBLOB_
#define _MORKBLOB_ 1

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*| Buf: the minimum needed to describe location and content length.
**| This is typically only enough to read from this buffer, since
**| one cannot write effectively without knowing the size of a buf.
|*/
class morkBuf { // subset of nsIMdbYarn slots
public:
  void*         mBuf_Body;  // space for holding any binary content
  mork_fill     mBuf_Fill;  // logical content in Buf in bytes

public:
  morkBuf() { }
  morkBuf(const void* ioBuf, mork_fill inFill)
  : mBuf_Body((void*) ioBuf), mBuf_Fill(inFill) { }

private: // copying is not allowed
  morkBuf(const morkBuf& other);
  morkBuf& operator=(const morkBuf& other);
};

/*| Blob: a buffer with an associated size, to increase known buf info
**| to include max capacity in addition to buf location and content.
**| This form factor allows us to allocate a vector of such blobs,
**| which can share the same managing heap stored elsewhere, and that
**| is why we don't include a pointer to a heap in this blob class.
|*/
class morkBlob : public morkBuf { // greater subset of nsIMdbYarn slots

  // void*         mBuf_Body;  // space for holding any binary content
  // mdb_fill      mBuf_Fill;  // logical content in Buf in bytes
public:
  mork_size      mBlob_Size;  // physical size of Buf in bytes

public:
  morkBlob() { }
  morkBlob(const void* ioBuf, mork_fill inFill, mork_size inSize)
  : morkBuf(ioBuf, inFill), mBlob_Size(inSize) { }
  
public:
  mork_bool Grow(morkEnv* ev, nsIMdbHeap* ioHeap, mork_size inNewSize);

private: // copying is not allowed
  morkBlob(const morkBlob& other);
  morkBlob& operator=(const morkBlob& other);
  
};

/*| Text: a blob with an associated charset annotation, where the
**| charset actually includes the general notion of typing, and not
**| just a specification of character set alone; we want to permit
**| arbitrary charset annotations for ad hoc binary types as well.
**| (We avoid including a nsIMdbHeap pointer in morkText for the same
**| reason morkBlob does: we want minimal size vectors of morkText.)
|*/
class morkText : public morkBlob { // greater subset of nsIMdbYarn slots

  // void*         mBuf_Body;  // space for holding any binary content
  // mdb_fill      mBuf_Fill;  // logical content in Buf in bytes
  // mdb_size      mBlob_Size;  // physical size of Buf in bytes

public:
  mork_cscode    mText_Form;  // charset format encoding

  morkText() { }

private: // copying is not allowed
  morkText(const morkText& other);
  morkText& operator=(const morkText& other);
};

/*| Spool: a text with an associated nsIMdbHeap instance that provides
**| all memory management for the space pointed to by mBuf_Body. (This
**| was the hardest type to give a name in this small class hierarchy,
**| because it's hard to characterize self-management of one's space.)
**| A spool is a self-contained blob that knows how to grow itself as
**| necessary to hold more content when necessary.  Spool descends from
**| morkText to include the mText_Form slot, even though this won't be
**| needed always, because we are not as concerned about the overall
**| size of this particular Spool object (if we were concerned about
**| the size of an array of Spool instances, we would not bother with
**| a separate heap pointer for each of them).
**|
**|| A spool makes a good medium in which to stream content as a sink,
**| so we will have a subclass of morkSink called morkSpoolSink that
**| will stream bytes into this self-contained spool object. The name
**| of this morkSpool class derives more from this intended usage than
**| from anything else.  The Mork code to parse db content will use
**| spools with associated sinks to accumulate parsed strings.
**|
**|| Heap: this is the heap used for memory allocation.  This instance
**| is NOT refcounted, since this spool always assumes the heap is held
**| through a reference elsewhere (for example, through the same object
**| that contains or holds the spool itself.  This lack of refcounting
**| is consistent with the fact that morkSpool itself is not refcounted,
**| and is not intended for use as a standalone object.
|*/
class morkSpool : public morkText { // self-managing text blob object

  // void*         mBuf_Body;  // space for holding any binary content
  // mdb_fill      mBuf_Fill;  // logical content in Buf in bytes
  // mdb_size      mBlob_Size;  // physical size of Buf in bytes
  // mdb_cscode    mText_Form;  // charset format encoding
public:
  nsIMdbHeap*      mSpool_Heap;  // storage manager for mBuf_Body pointer

public:
  morkSpool(morkEnv* ev, nsIMdbHeap* ioHeap);
  
  void CloseSpool(morkEnv* ev);

private: // copying is not allowed
  morkSpool(const morkSpool& other);
  morkSpool& operator=(const morkSpool& other);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKBLOB_ */
