/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

  void ClearBufFill() { mBuf_Fill = 0; }

  static void NilBufBodyError(morkEnv* ev);

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
 
  static void BlobFillOverSizeError(morkEnv* ev);
 
public:
  mork_bool GrowBlob(morkEnv* ev, nsIMdbHeap* ioHeap,
    mork_size inNewSize);

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

/*| Coil: a text with an associated nsIMdbHeap instance that provides
**| all memory management for the space pointed to by mBuf_Body. (This
**| was the hardest type to give a name in this small class hierarchy,
**| because it's hard to characterize self-management of one's space.)
**| A coil is a self-contained blob that knows how to grow itself as
**| necessary to hold more content when necessary.  Coil descends from
**| morkText to include the mText_Form slot, even though this won't be
**| needed always, because we are not as concerned about the overall
**| size of this particular Coil object (if we were concerned about
**| the size of an array of Coil instances, we would not bother with
**| a separate heap pointer for each of them).
**|
**|| A coil makes a good medium in which to stream content as a sink,
**| so we will have a subclass of morkSink called morkCoil that
**| will stream bytes into this self-contained coil object. The name
**| of this morkCoil class derives more from this intended usage than
**| from anything else.  The Mork code to parse db content will use
**| coils with associated sinks to accumulate parsed strings.
**|
**|| Heap: this is the heap used for memory allocation.  This instance
**| is NOT refcounted, since this coil always assumes the heap is held
**| through a reference elsewhere (for example, through the same object
**| that contains or holds the coil itself.  This lack of refcounting
**| is consistent with the fact that morkCoil itself is not refcounted,
**| and is not intended for use as a standalone object.
|*/
class morkCoil : public morkText { // self-managing text blob object

  // void*         mBuf_Body;  // space for holding any binary content
  // mdb_fill      mBuf_Fill;  // logical content in Buf in bytes
  // mdb_size      mBlob_Size;  // physical size of Buf in bytes
  // mdb_cscode    mText_Form;  // charset format encoding
public:
  nsIMdbHeap*      mCoil_Heap;  // storage manager for mBuf_Body pointer

public:
  morkCoil(morkEnv* ev, nsIMdbHeap* ioHeap);
  
  void CloseCoil(morkEnv* ev);

  mork_bool GrowCoil(morkEnv* ev, mork_size inNewSize)
  { return this->GrowBlob(ev, mCoil_Heap, inNewSize); }

private: // copying is not allowed
  morkCoil(const morkCoil& other);
  morkCoil& operator=(const morkCoil& other);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKBLOB_ */
