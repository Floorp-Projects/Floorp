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

#ifndef _MORKFILE_
#define _MORKFILE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*=============================================================================
 * morkFile: abstract file interface
 */

#define morkDerived_kFile     /*i*/ 0x4669 /* ascii 'Fi' */

class morkFile /*d*/ : public morkObject { /* ````` simple file API ````` */

// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*    mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs
  
// public: // slots inherited from morkObject (meant to inform only)

  // mork_color   mBead_Color;   // ID for this bead
  // morkHandle*  mObject_Handle;  // weak ref to handle for this object

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected morkFile members (similar to public domain IronDoc)

  mork_u1     mFile_Frozen;   // 'F' => file allows only read access
  mork_u1     mFile_DoTrace;  // 'T' trace if ev->DoTrace()
  mork_u1     mFile_IoOpen;   // 'O' => io stream is open (& needs a close)
  mork_u1     mFile_Active;   // 'A' => file is active and usable
  
  nsIMdbHeap* mFile_SlotHeap; // heap for Name and other allocated slots
  char*       mFile_Name; // can be nil if SetFileName() is never called
  // mFile_Name convention: managed with morkEnv::CopyString()/FreeString()

  nsIMdbFile* mFile_Thief; // from a call to orkinFile::Steal()
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseFile() only if open
  virtual ~morkFile(); // assert that CloseFile() executed earlier
  
public: // morkFile construction & destruction
  morkFile(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap,
    nsIMdbHeap* ioSlotHeap);
  void CloseFile(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkFile(const morkFile& other);
  morkFile& operator=(const morkFile& other);

public: // dynamic type identification
  mork_bool IsFile() const
  { return IsNode() && mNode_Derived == morkDerived_kFile; }
// } ===== end morkNode methods =====
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public static standard file creation entry point

  static morkFile* OpenOldFile(morkEnv* ev, nsIMdbHeap* ioHeap,
    const char* inFilePath, mork_bool inFrozen);
  // Choose some subclass of morkFile to instantiate, in order to read
  // (and write if not frozen) the file known by inFilePath.  The file
  // returned should be open and ready for use, and presumably positioned
  // at the first byte position of the file.  The exact manner in which
  // files must be opened is considered a subclass specific detail, and
  // other portions or Mork source code don't want to know how it's done.

  static morkFile* CreateNewFile(morkEnv* ev, nsIMdbHeap* ioHeap,
    const char* inFilePath);
  // Choose some subclass of morkFile to instantiate, in order to read
  // (and write if not frozen) the file known by inFilePath.  The file
  // returned should be created and ready for use, and presumably positioned
  // at the first byte position of the file.  The exact manner in which
  // files must be opened is considered a subclass specific detail, and
  // other portions or Mork source code don't want to know how it's done.
  
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual morkFile methods

  virtual void Steal(morkEnv* ev, nsIMdbFile* ioThief) = 0;
  // Steal: tell this file to close any associated i/o stream in the file
  // system, because the file ioThief intends to reopen the file in order
  // to provide the MDB implementation with more exotic file access than is
  // offered by the nsIMdbFile alone.  Presumably the thief knows enough
  // from Path() in order to know which file to reopen.  If Steal() is
  // successful, this file should probably delegate all future calls to
  // the nsIMdbFile interface down to the thief files, so that even after
  // the file has been stolen, it can still be read, written, or forcibly
  // closed (by a call to CloseMdbObject()).
  
  virtual void BecomeTrunk(morkEnv* ev);
  // If this file is a file version branch created by calling AcquireBud(),
  // BecomeTrunk() causes this file's content to replace the original
  // file's content, typically by assuming the original file's identity.
  // This default implementation of BecomeTrunk() does nothing, and this
  // is appropriate behavior for files which are not branches, and is
  // also the right behavior for files returned from AcquireBud() which are
  // in fact the original file that has been truncated down to zero length.

  virtual morkFile*  AcquireBud(morkEnv* ev, nsIMdbHeap* ioHeap) = 0;
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
  
  virtual mork_pos   Length(morkEnv* ev) const = 0; // eof
  virtual mork_pos   Tell(morkEnv* ev) const = 0;
  virtual mork_size  Read(morkEnv* ev, void* outBuf, mork_size inSize) = 0;
  virtual mork_pos   Seek(morkEnv* ev, mork_pos inPos) = 0;
  virtual mork_size  Write(morkEnv* ev, const void* inBuf, mork_size inSize) = 0;
  virtual void       Flush(morkEnv* ev) = 0;
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly morkFile methods

  nsIMdbFile* AcquireFileHandle(morkEnv* ev); // mObject_Handle
  
  mork_bool FileFrozen() const  { return mFile_Frozen == 'F'; }
  mork_bool FileDoTrace() const { return mFile_DoTrace == 'T'; }
  mork_bool FileIoOpen() const  { return mFile_IoOpen == 'O'; }
  mork_bool FileActive() const  { return mFile_Active == 'A'; }

  void SetFileFrozen(mork_bool b)  { mFile_Frozen = (mork_u1) ((b)? 'F' : 0); }
  void SetFileDoTrace(mork_bool b) { mFile_DoTrace = (mork_u1) ((b)? 'T' : 0); }
  void SetFileIoOpen(mork_bool b)  { mFile_IoOpen = (mork_u1) ((b)? 'O' : 0); }
  void SetFileActive(mork_bool b)  { mFile_Active = (mork_u1) ((b)? 'A' : 0); }

  
  mork_bool IsOpenActiveAndMutableFile() const
  { return ( IsOpenNode() && FileActive() && !FileFrozen() ); }
    // call IsOpenActiveAndMutableFile() before writing a file
  
  mork_bool IsOpenAndActiveFile() const
  { return ( this->IsOpenNode() && this->FileActive() ); }
    // call IsOpenAndActiveFile() before using a file
    

  nsIMdbFile* GetThief() const { return mFile_Thief; }
  void SetThief(morkEnv* ev, nsIMdbFile* ioThief); // ioThief can be nil
    
  const char* GetFileNameString() const { return mFile_Name; }
  void SetFileName(morkEnv* ev, const char* inName); // inName can be nil
  static void NilSlotHeapError(morkEnv* ev);
  static void NilFileNameError(morkEnv* ev);
  static void NonFileTypeError(morkEnv* ev);
    
  void NewMissingIoError(morkEnv* ev) const;
    
  void NewFileDownError(morkEnv* ev) const;
    // call NewFileDownError() when either IsOpenAndActiveFile()
    // is false, or when IsOpenActiveAndMutableFile() is false.
 
   void NewFileErrnoError(morkEnv* ev) const;
       // call NewFileErrnoError() to convert std C errno into AB fault

  mork_size WriteNewlines(morkEnv* ev, mork_count inNewlines);
  // WriteNewlines() returns the number of bytes written.
         
public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakFile(morkFile* me,
    morkEnv* ev, morkFile** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongFile(morkFile* me,
    morkEnv* ev, morkFile** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

/*=============================================================================
 * morkStdioFile: concrete file using standard C file io
 */

#define morkDerived_kStdioFile     /*i*/ 0x7346 /* ascii 'sF' */

class morkStdioFile /*d*/ : public morkFile { /* `` copied from IronDoc `` */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected morkStdioFile members

  void* mStdioFile_File;
  // actually type FILE*, but using opaque void* type
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseStdioFile() only if open
  virtual ~morkStdioFile(); // assert that CloseStdioFile() executed earlier
  
public: // morkStdioFile construction & destruction
  morkStdioFile(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);
  void CloseStdioFile(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkStdioFile(const morkStdioFile& other);
  morkStdioFile& operator=(const morkStdioFile& other);

public: // dynamic type identification
  mork_bool IsStdioFile() const
  { return IsNode() && mNode_Derived == morkDerived_kStdioFile; }
// } ===== end morkNode methods =====

public: // typing
  static void NonStdioFileTypeError(morkEnv* ev);
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // compatible with the morkFile::OpenOldFile() entry point

  static morkStdioFile* OpenOldStdioFile(morkEnv* ev, nsIMdbHeap* ioHeap,
    const char* inFilePath, mork_bool inFrozen);

  static morkStdioFile* CreateNewStdioFile(morkEnv* ev, nsIMdbHeap* ioHeap,
    const char* inFilePath);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_File methods

  virtual void Steal(morkEnv* ev, nsIMdbFile* ioThief);
  // Steal: tell this file to close any associated i/o stream in the file
  // system, because the file ioThief intends to reopen the file in order
  // to provide the MDB implementation with more exotic file access than is
  // offered by the nsIMdbFile alone.  Presumably the thief knows enough
  // from Path() in order to know which file to reopen.  If Steal() is
  // successful, this file should probably delegate all future calls to
  // the nsIMdbFile interface down to the thief files, so that even after
  // the file has been stolen, it can still be read, written, or forcibly
  // closed (by a call to CloseMdbObject()).

  virtual void BecomeTrunk(morkEnv* ev);
  // If this file is a file version branch created by calling AcquireBud(),
  // BecomeTrunk() causes this file's content to replace the original
  // file's content, typically by assuming the original file's identity.

  virtual morkFile*  AcquireBud(morkEnv* ev, nsIMdbHeap* ioHeap);
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

  virtual mork_pos   Length(morkEnv* ev) const; // eof
  virtual mork_pos   Tell(morkEnv* ev) const;
  virtual mork_size  Read(morkEnv* ev, void* outBuf, mork_size inSize);
  virtual mork_pos   Seek(morkEnv* ev, mork_pos inPos);
  virtual mork_size  Write(morkEnv* ev, const void* inBuf, mork_size inSize);
  virtual void       Flush(morkEnv* ev);
    
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly morkStdioFile methods

  void new_stdio_file_fault(morkEnv* ev) const;
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly morkStdioFile methods
    
  morkStdioFile(morkEnv* ev, const morkUsage& inUsage, 
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap,
    const char* inName, const char* inMode);
    // calls OpenStdio() after construction

  morkStdioFile(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap,
     void* ioFile, const char* inName, mork_bool inFrozen);
    // calls UseStdio() after construction
  
  void OpenStdio(morkEnv* ev, const char* inName, const char* inMode);
    // Open a new FILE with name inName, using mode flags from inMode.
  
  void UseStdio(morkEnv* ev, void* ioFile, const char* inName,
    mork_bool inFrozen);
    // Use an existing file, like stdin/stdout/stderr, which should not
    // have the io stream closed when the file is closed.  The ioFile
    // parameter must actually be of type FILE (but we don't want to make
    // this header file include the stdio.h header file).
    
  void CloseStdio(morkEnv* ev);
    // Close the stream io if both and FileActive() and FileIoOpen(), but
    // this does not close this instances (like CloseStdioFile() does).
    // If stream io was made active by means of calling UseStdio(),
    // then this method does little beyond marking the stream inactive
    // because FileIoOpen() is false.
    
public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakStdioFile(morkStdioFile* me,
    morkEnv* ev, morkStdioFile** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongStdioFile(morkStdioFile* me,
    morkEnv* ev, morkStdioFile** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKFILE_ */
