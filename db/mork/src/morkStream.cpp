/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKFILE_
#include "morkFile.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKSTREAM_
#include "morkStream.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkStream::CloseMorkNode(morkEnv* ev) // CloseStream() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseStream(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkStream::~morkStream() // assert CloseStream() executed earlier
{
  MORK_ASSERT(mStream_ContentFile==0);
  MORK_ASSERT(mStream_Buf==0);
}

/*public non-poly*/
morkStream::morkStream(morkEnv* ev, const morkUsage& inUsage,
  nsIMdbHeap* ioHeap,
  nsIMdbFile* ioContentFile, mork_size inBufSize, mork_bool inFrozen)
: morkFile(ev, inUsage, ioHeap, ioHeap)
, mStream_At( 0 )
, mStream_ReadEnd( 0 )
, mStream_WriteEnd( 0 )

, mStream_ContentFile( 0 )

, mStream_Buf( 0 )
, mStream_BufSize( inBufSize )
, mStream_BufPos( 0 )
, mStream_Dirty( morkBool_kFalse )
, mStream_HitEof( morkBool_kFalse )
{
  if ( ev->Good() )
  {
    if ( inBufSize < morkStream_kMinBufSize )
      mStream_BufSize = inBufSize = morkStream_kMinBufSize;
    else if ( inBufSize > morkStream_kMaxBufSize )
      mStream_BufSize = inBufSize = morkStream_kMaxBufSize;
    
    if ( ioContentFile && ioHeap )
    {
      // if ( ioContentFile->FileFrozen() ) // forced to be readonly?
      //   inFrozen = morkBool_kTrue; // override the input value
        
      nsIMdbFile_SlotStrongFile(ioContentFile, ev, &mStream_ContentFile);
      if ( ev->Good() )
      {
        mork_u1* buf = 0;
        ioHeap->Alloc(ev->AsMdbEnv(), inBufSize, (void**) &buf);
        if ( buf )
        {
          mStream_At = mStream_Buf = buf;
          
          if ( !inFrozen )
          {
            // physical buffer end never moves:
            mStream_WriteEnd = buf + inBufSize;
          }
          else
            mStream_WriteEnd = 0; // no writing is allowed
          
          if ( inFrozen )
          {
            // logical buffer end starts at Buf with no content:
            mStream_ReadEnd = buf;
            this->SetFileFrozen(inFrozen);
          }
          else
            mStream_ReadEnd = 0; // no reading is allowed
          
          this->SetFileActive(morkBool_kTrue);
          this->SetFileIoOpen(morkBool_kTrue);
        }
        if ( ev->Good() )
          mNode_Derived = morkDerived_kStream;
      }
    }
    else ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkStream::CloseStream(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      nsIMdbFile_SlotStrongFile((nsIMdbFile*) 0, ev, &mStream_ContentFile);
      nsIMdbHeap* heap = mFile_SlotHeap;
      mork_u1* buf = mStream_Buf;
      mStream_Buf = 0;
      
      if ( heap && buf )
        heap->Free(ev->AsMdbEnv(), buf);

      this->CloseFile(ev);
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 
  
#define morkStream_kSpacesPerIndent 1 /* one space per indent */
#define morkStream_kMaxIndentDepth 70 /* max indent of 70 space bytes */
static const char* morkStream_kSpaces // next line to ease length perception
= "                                                                        ";
// 123456789_123456789_123456789_123456789_123456789_123456789_123456789_
// morkStream_kSpaces above must contain (at least) 70 spaces (ASCII 0x20)
 
mork_size
morkStream::PutIndent(morkEnv* ev, mork_count inDepth)
  // PutIndent() puts a linebreak, and then
  // "indents" by inDepth, and returns the line length after indentation.
{
  mork_size outLength = 0;

  if ( ev->Good() )
  {
    this->PutLineBreak(ev);
    if ( ev->Good() )
    {
      outLength = inDepth;
      if ( inDepth )
        this->Write(ev, morkStream_kSpaces, inDepth);
    }
  }
  return outLength;
}

mork_size
morkStream::PutByteThenIndent(morkEnv* ev, int inByte, mork_count inDepth)
  // PutByteThenIndent() puts the byte, then a linebreak, and then
  // "indents" by inDepth, and returns the line length after indentation.
{
  mork_size outLength = 0;
  
  if ( inDepth > morkStream_kMaxIndentDepth )
    inDepth = morkStream_kMaxIndentDepth;
  
  this->Putc(ev, inByte);
  if ( ev->Good() )
  {
    this->PutLineBreak(ev);
    if ( ev->Good() )
    {
      outLength = inDepth;
      if ( inDepth )
        this->Write(ev, morkStream_kSpaces, inDepth);
    }
  }
  return outLength;
}
  
mork_size
morkStream::PutStringThenIndent(morkEnv* ev,
  const char* inString, mork_count inDepth)
// PutStringThenIndent() puts the string, then a linebreak, and then
// "indents" by inDepth, and returns the line length after indentation.
{
  mork_size outLength = 0;
  
  if ( inDepth > morkStream_kMaxIndentDepth )
    inDepth = morkStream_kMaxIndentDepth;
  
  if ( inString )
  {
    mork_size length = MORK_STRLEN(inString);
    if ( length && ev->Good() ) // any bytes to write?
      this->Write(ev, inString, length);
  }
  
  if ( ev->Good() )
  {
    this->PutLineBreak(ev);
    if ( ev->Good() )
    {
      outLength = inDepth;
      if ( inDepth )
        this->Write(ev, morkStream_kSpaces, inDepth);
    }
  }
  return outLength;
}

mork_size
morkStream::PutString(morkEnv* ev, const char* inString)
{
  mork_size outSize = 0;
  if ( inString )
  {
    outSize = MORK_STRLEN(inString);
    if ( outSize && ev->Good() ) // any bytes to write?
    {
      this->Write(ev, inString, outSize);
    }
  }
  return outSize;
}

mork_size
morkStream::PutStringThenNewline(morkEnv* ev, const char* inString)
  // PutStringThenNewline() returns total number of bytes written.
{
  mork_size outSize = 0;
  if ( inString )
  {
    outSize = MORK_STRLEN(inString);
    if ( outSize && ev->Good() ) // any bytes to write?
    {
      this->Write(ev, inString, outSize);
      if ( ev->Good() )
        outSize += this->PutLineBreak(ev);
    }
  }
  return outSize;
}

mork_size
morkStream::PutByteThenNewline(morkEnv* ev, int inByte)
  // PutByteThenNewline() returns total number of bytes written.
{
  mork_size outSize = 1; // one for the following byte
  this->Putc(ev, inByte);
  if ( ev->Good() )
    outSize += this->PutLineBreak(ev);
  return outSize;
}

mork_size
morkStream::PutLineBreak(morkEnv* ev)
{
#if defined(MORK_MAC) || defined(MORK_OBSOLETE)

  this->Putc(ev, mork_kCR);
  return 1;
  
#else
#  if defined(MORK_WIN) || defined(MORK_OS2)
  
  this->Putc(ev, mork_kCR);
  this->Putc(ev, mork_kLF);
  return 2;
  
#  else
#    if defined(MORK_UNIX) || defined(MORK_BEOS)
  
  this->Putc(ev, mork_kLF);
  return 1;
  
#    endif /* MORK_UNIX || MORK_BEOS */
#  endif /* MORK_WIN */
#endif /* MORK_MAC */
}
// ````` ````` ````` `````   ````` ````` ````` `````  
// public: // virtual morkFile methods


/*public virtual*/ void
morkStream::Steal(morkEnv* ev, nsIMdbFile* ioThief)
  // Steal: tell this file to close any associated i/o stream in the file
  // system, because the file ioThief intends to reopen the file in order
  // to provide the MDB implementation with more exotic file access than is
  // offered by the nsIMdbFile alone.  Presumably the thief knows enough
  // from Path() in order to know which file to reopen.  If Steal() is
  // successful, this file should probably delegate all future calls to
  // the nsIMdbFile interface down to the thief files, so that even after
  // the file has been stolen, it can still be read, written, or forcibly
  // closed (by a call to CloseMdbObject()).
{
  MORK_USED_1(ioThief);
  ev->StubMethodOnlyError();
}

/*public virtual*/ void
morkStream::BecomeTrunk(morkEnv* ev)
  // If this file is a file version branch created by calling AcquireBud(),
  // BecomeTrunk() causes this file's content to replace the original
  // file's content, typically by assuming the original file's identity.
{
  ev->StubMethodOnlyError();
}

/*public virtual*/ morkFile*
morkStream::AcquireBud(morkEnv* ev, nsIMdbHeap* ioHeap)
  // AcquireBud() starts a new "branch" version of the file, empty of content,
  // so that a new version of the file can be written.  This new file
  // can later be told to BecomeTrunk() the original file, so the branch
  // created by budding the file will replace the original file.  Some
  // file subclasses might initially take the unsafe but expedient
  // approach of simply truncating this file down to zero length, and
  // then returning the same morkFile pointer as this, with an extra
  // reference count increment.  Note that the caller of AcquireBud() is
  // expected to eventually call CutStrongRef() on the returned file
  // in order to release the strong reference.  High quality versions
  // of morkFile subclasses will create entirely new files which later
  // are renamed to become the old file, so that better transactional
  // behavior is exhibited by the file, so crashes protect old files.
  // Note that AcquireBud() is an illegal operation on readonly files.
{
  MORK_USED_1(ioHeap);
  morkFile* outFile = 0;
  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenAndActiveFile() && file )
  {
    // figure out how this interacts with buffering and mStream_WriteEnd:
    ev->StubMethodOnlyError();
  }
  else this->NewFileDownError(ev);
  
  return outFile;
}

/*public virtual*/ mork_pos
morkStream::Length(morkEnv* ev) const // eof
{
  mork_pos outPos = 0;
  
  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenAndActiveFile() && file )
  {
    mork_pos contentEof = 0;
    nsIMdbEnv* menv = ev->AsMdbEnv();
    file->Eof(menv, &contentEof);
    if ( ev->Good() )
    {
      if ( mStream_WriteEnd ) // this stream supports writing?
      {
        // the local buffer might have buffered content past content eof
        if ( ev->Good() ) // no error happened during Length() above?
        {
          mork_u1* at = mStream_At;
          mork_u1* buf = mStream_Buf;
          if ( at >= buf ) // expected cursor order?
          {
            mork_pos localContent = mStream_BufPos + (at - buf);
            if ( localContent > contentEof ) // buffered past eof?
              contentEof = localContent; // return new logical eof

            outPos = contentEof;
          }
          else this->NewBadCursorOrderError(ev);
        }
      }
      else
        outPos = contentEof; // frozen files get length from content file
    }
  }
  else this->NewFileDownError(ev);

  return outPos;
}

void morkStream::NewBadCursorSlotsError(morkEnv* ev) const
{ ev->NewError("bad stream cursor slots"); }

void morkStream::NewNullStreamBufferError(morkEnv* ev) const
{ ev->NewError("null stream buffer"); }

void morkStream::NewCantReadSinkError(morkEnv* ev) const
{ ev->NewError("cant read stream sink"); }

void morkStream::NewCantWriteSourceError(morkEnv* ev) const
{ ev->NewError("cant write stream source"); }

void morkStream::NewPosBeyondEofError(morkEnv* ev) const
{ ev->NewError("stream pos beyond eof"); }

void morkStream::NewBadCursorOrderError(morkEnv* ev) const
{ ev->NewError("bad stream cursor order"); }

/*public virtual*/ mork_pos   
morkStream::Tell(morkEnv* ev) const
{
  mork_pos outPos = 0;
  
  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenAndActiveFile() && file )
  {
    mork_u1* buf = mStream_Buf;
    mork_u1* at = mStream_At;
    
    mork_u1* readEnd = mStream_ReadEnd;   // nonzero only if readonly
    mork_u1* writeEnd = mStream_WriteEnd; // nonzero only if writeonly
    
    if ( writeEnd )
    {
      if ( buf && at >= buf && at <= writeEnd ) 
      {
        outPos = mStream_BufPos + (at - buf);
      }
      else this->NewBadCursorOrderError(ev);
    }
    else if ( readEnd )
    {
      if ( buf && at >= buf && at <= readEnd ) 
      {
        outPos = mStream_BufPos + (at - buf);
      }
      else this->NewBadCursorOrderError(ev);
    }
  }
  else this->NewFileDownError(ev);

  return outPos;
}

/*public virtual*/ mork_size  
morkStream::Read(morkEnv* ev, void* outBuf, mork_size inSize)
{
  // First we satisfy the request from buffered bytes, if any.  Then
  // if additional bytes are needed, we satisfy these by direct reads
  // from the content file without any local buffering (but we still need
  // to adjust the buffer position to reflect the current i/o point).

  mork_pos outActual = 0;

  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenAndActiveFile() && file )
  {
    mork_u1* end = mStream_ReadEnd; // byte after last buffered byte
    if ( end ) // file is open for read access?
    {
      if ( inSize ) // caller wants any output?
      {
        mork_u1* sink = (mork_u1*) outBuf; // where we plan to write bytes
        if ( sink ) // caller passed good buffer address?
        {
          mork_u1* at = mStream_At;
          mork_u1* buf = mStream_Buf;
          if ( at >= buf && at <= end ) // expected cursor order?
          {
            mork_num remaining = (mork_num) (end - at); // bytes left in buffer
            
            mork_num quantum = inSize; // number of bytes to copy
            if ( quantum > remaining ) // more than buffer content?
              quantum = remaining; // restrict to buffered bytes
              
            if ( quantum ) // any bytes left in the buffer?
            {
              MORK_MEMCPY(sink, at, quantum); // from buffer bytes
              
              at += quantum; // advance past read bytes
              mStream_At = at;
              outActual += quantum;  // this much copied so far

              sink += quantum;   // in case we need to copy more
              inSize -= quantum; // filled this much of request
              mStream_HitEof = morkBool_kFalse;
            }
            
            if ( inSize ) // we still need to read more content?
            {
              // We need to read more bytes directly from the
              // content file, without local buffering.  We have
              // exhausted the local buffer, so we need to show
              // it is now empty, and adjust the current buf pos.
              
              mork_num posDelta = (mork_num) (at - buf); // old buf content
              mStream_BufPos += posDelta;   // past now empty buf
              
              mStream_At = mStream_ReadEnd = buf; // empty buffer
              
              // file->Seek(ev, mStream_BufPos); // set file pos
              // if ( ev->Good() ) // no seek error?
              // {
              // }
              
              mork_num actual = 0;
              nsIMdbEnv* menv = ev->AsMdbEnv();
              file->Get(menv, sink, inSize, mStream_BufPos, &actual);
              if ( ev->Good() ) // no read error?
              {
                if ( actual )
                {
                  outActual += actual;
                  mStream_BufPos += actual;
                  mStream_HitEof = morkBool_kFalse;
                }
                else if ( !outActual )
                  mStream_HitEof = morkBool_kTrue;
              }
            }
          }
          else this->NewBadCursorOrderError(ev);
        }
        else this->NewNullStreamBufferError(ev);
      }
    }
    else this->NewCantReadSinkError(ev);
  }
  else this->NewFileDownError(ev);
  
  if ( ev->Bad() )
    outActual = 0;

  return (mork_size) outActual;
}

/*public virtual*/ mork_pos   
morkStream::Seek(morkEnv* ev, mork_pos inPos)
{
  mork_pos outPos = 0;

  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenOrClosingNode() && this->FileActive() && file )
  {
    mork_u1* at = mStream_At;             // current position in buffer
    mork_u1* buf = mStream_Buf;           // beginning of buffer 
    mork_u1* readEnd = mStream_ReadEnd;   // nonzero only if readonly
    mork_u1* writeEnd = mStream_WriteEnd; // nonzero only if writeonly
    
    if ( writeEnd ) // file is mutable/writeonly?
    {
      if ( mStream_Dirty ) // need to commit buffer changes?
        this->Flush(ev);

      if ( ev->Good() ) // no errors during flush or earlier?
      {
        if ( at == buf ) // expected post flush cursor value?
        {
          if ( mStream_BufPos != inPos ) // need to change pos?
          {
            mork_pos eof = 0;
            nsIMdbEnv* menv = ev->AsMdbEnv();
            file->Eof(menv, &eof);
            if ( ev->Good() ) // no errors getting length?
            {
              if ( inPos <= eof ) // acceptable new position?
              {
                mStream_BufPos = inPos; // new stream position
                outPos = inPos;
              }
              else this->NewPosBeyondEofError(ev);
            }
          }
        }
        else this->NewBadCursorOrderError(ev);
      }
    }
    else if ( readEnd ) // file is frozen/readonly?
    {
      if ( at >= buf && at <= readEnd ) // expected cursor order?
      {
        mork_pos eof = 0;
        nsIMdbEnv* menv = ev->AsMdbEnv();
        file->Eof(menv, &eof);
        if ( ev->Good() ) // no errors getting length?
        {
          if ( inPos <= eof ) // acceptable new position?
          {
            outPos = inPos;
            mStream_BufPos = inPos; // new stream position
            mStream_At = mStream_ReadEnd = buf; // empty buffer
            if ( inPos == eof ) // notice eof reached?
              mStream_HitEof = morkBool_kTrue;
          }
          else this->NewPosBeyondEofError(ev);
        }
      }
      else this->NewBadCursorOrderError(ev);
    }
      
  }
  else this->NewFileDownError(ev);

  return outPos;
}

/*public virtual*/ mork_size  
morkStream::Write(morkEnv* ev, const void* inBuf, mork_size inSize)
{
  mork_num outActual = 0;

  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenActiveAndMutableFile() && file )
  {
    mork_u1* end = mStream_WriteEnd; // byte after last buffered byte
    if ( end ) // file is open for write access?
    {
      if ( inSize ) // caller provided any input?
      {
        const mork_u1* source = (const mork_u1*) inBuf; // from where
        if ( source ) // caller passed good buffer address?
        {
          mork_u1* at = mStream_At;
          mork_u1* buf = mStream_Buf;
          if ( at >= buf && at <= end ) // expected cursor order?
          {
            mork_num space = (mork_num) (end - at); // space left in buffer
            
            mork_num quantum = inSize; // number of bytes to write
            if ( quantum > space ) // more than buffer size?
              quantum = space; // restrict to avail space
              
            if ( quantum ) // any space left in the buffer?
            {
              mStream_Dirty = morkBool_kTrue; // to ensure later flush
              MORK_MEMCPY(at, source, quantum); // into buffer
              
              mStream_At += quantum; // advance past written bytes
              outActual += quantum;  // this much written so far

              source += quantum; // in case we need to write more
              inSize -= quantum; // filled this much of request
            }
            
            if ( inSize ) // we still need to write more content?
            {
              // We need to write more bytes directly to the
              // content file, without local buffering.  We have
              // exhausted the local buffer, so we need to flush
              // it and empty it, and adjust the current buf pos.
              // After flushing, if the rest of the write fits
              // inside the buffer, we will put bytes into the
              // buffer rather than write them to content file.
              
              if ( mStream_Dirty )
                this->Flush(ev); // will update mStream_BufPos

              at = mStream_At;
              if ( at < buf || at > end ) // bad cursor?
                this->NewBadCursorOrderError(ev);
                
              if ( ev->Good() ) // no errors?
              {
                space = (mork_num) (end - at); // space left in buffer
                if ( space > inSize ) // write to buffer?
                {
                  mStream_Dirty = morkBool_kTrue; // ensure flush
                  MORK_MEMCPY(at, source, inSize); // copy
                  
                  mStream_At += inSize; // past written bytes
                  outActual += inSize;  // this much written
                }
                else // directly to content file instead
                {
                  // file->Seek(ev, mStream_BufPos); // set pos
                  // if ( ev->Good() ) // no seek error?
                  // {
                  // }

                  nsIMdbEnv* menv = ev->AsMdbEnv();
                  mork_num actual = 0;
                  file->Put(menv, source, inSize, mStream_BufPos, &actual);
                  if ( ev->Good() ) // no write error?
                  {
                    outActual += actual;
                    mStream_BufPos += actual;
                  }
                }
              }
            }
          }
          else this->NewBadCursorOrderError(ev);
        }
        else this->NewNullStreamBufferError(ev);
      }
    }
    else this->NewCantWriteSourceError(ev);
  }
  else this->NewFileDownError(ev);
  
  if ( ev->Bad() )
    outActual = 0;

  return outActual;
}

/*public virtual*/ void       
morkStream::Flush(morkEnv* ev)
{
  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenOrClosingNode() && this->FileActive() && file )
  {
    if ( mStream_Dirty )
      this->spill_buf(ev);

    file->Flush(ev->AsMdbEnv());
  }
  else this->NewFileDownError(ev);
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// protected: // protected non-poly morkStream methods (for char io)

int
morkStream::fill_getc(morkEnv* ev)
{
  int c = EOF;
  
  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenAndActiveFile() && file )
  {
    mork_u1* buf = mStream_Buf;
    mork_u1* end = mStream_ReadEnd; // beyond buf after earlier read
    if ( end > buf ) // any earlier read bytes buffered?
    {
      mStream_BufPos += ( end - buf ); // advance past old read
    }
      
    if ( ev->Good() ) // no errors yet?
    {
      // file->Seek(ev, mStream_BufPos); // set file pos
      // if ( ev->Good() ) // no seek error?
      // {
      // }

      nsIMdbEnv* menv = ev->AsMdbEnv();
      mork_num actual = 0;
      file->Get(menv, buf, mStream_BufSize, mStream_BufPos, &actual);
      if ( ev->Good() ) // no read errors?
      {
        if ( actual > mStream_BufSize ) // more than asked for??
          actual = mStream_BufSize;
        
        mStream_At = buf;
        mStream_ReadEnd = buf + actual;
        if ( actual ) // any bytes actually read?
        {
          c = *mStream_At++; // return first byte from buffer
          mStream_HitEof = morkBool_kFalse;
        }
        else
          mStream_HitEof = morkBool_kTrue;
      }
    }
  }
  else this->NewFileDownError(ev);
  
  return c;
}

void
morkStream::spill_putc(morkEnv* ev, int c)
{
  this->spill_buf(ev);
  if ( ev->Good() && mStream_At < mStream_WriteEnd )
    this->Putc(ev, c);
}

void
morkStream::spill_buf(morkEnv* ev) // spill/flush from buffer to file
{
  nsIMdbFile* file = mStream_ContentFile;
  if ( this->IsOpenOrClosingNode() && this->FileActive() && file )
  {
    mork_u1* buf = mStream_Buf;
    if ( mStream_Dirty )
    {
      mork_u1* at = mStream_At;
      if ( at >= buf && at <= mStream_WriteEnd ) // order?
      {
        mork_num count = (mork_num) (at - buf); // bytes buffered
        if ( count ) // anything to write to the string?
        {
          if ( count > mStream_BufSize ) // no more than max?
          {
            count = mStream_BufSize;
            mStream_WriteEnd = buf + mStream_BufSize;
            this->NewBadCursorSlotsError(ev);
          }
          if ( ev->Good() )
          {
            // file->Seek(ev, mStream_BufPos);
            // if ( ev->Good() )
            // {
            // }
            nsIMdbEnv* menv = ev->AsMdbEnv();
            mork_num actual = 0;
            
            file->Put(menv, buf, count, mStream_BufPos, &actual);
            if ( ev->Good() )
            {
              mStream_BufPos += actual; // past bytes written
              mStream_At = buf; // reset buffer cursor
              mStream_Dirty = morkBool_kFalse;
            }
          }
        }
      }
      else this->NewBadCursorOrderError(ev);
    }
    else
    {
#ifdef MORK_DEBUG
      ev->NewWarning("stream:spill:not:dirty");
#endif /*MORK_DEBUG*/
    }
  }
  else this->NewFileDownError(ev);

}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789
