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

class morkFile /*d*/ : public morkObject, public nsIMdbFile { /* ````` simple file API ````` */

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
  NS_DECL_ISUPPORTS_INHERITED
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
public:
  virtual mork_pos   Length(morkEnv* ev) const = 0; // eof
  // nsIMdbFile methods
  NS_IMETHOD Tell(nsIMdbEnv* ev, mdb_pos* outPos) const = 0;
  NS_IMETHOD Seek(nsIMdbEnv* ev, mdb_pos inPos, mdb_pos *outPos) = 0;
  NS_IMETHOD Eof(nsIMdbEnv* ev, mdb_pos* outPos);
  // } ----- end pos methods -----

  // { ----- begin read methods -----
  NS_IMETHOD Read(nsIMdbEnv* ev, void* outBuf, mdb_size inSize,
    mdb_size* outActualSize) = 0;
  NS_IMETHOD Get(nsIMdbEnv* ev, void* outBuf, mdb_size inSize,
    mdb_pos inPos, mdb_size* outActualSize);
  // } ----- end read methods -----
    
  // { ----- begin write methods -----
  NS_IMETHOD  Write(nsIMdbEnv* ev, const void* inBuf, mdb_size inSize,
    mdb_size* outActualSize) = 0;
  NS_IMETHOD  Put(nsIMdbEnv* ev, const void* inBuf, mdb_size inSize,
    mdb_pos inPos, mdb_size* outActualSize);
  NS_IMETHOD  Flush(nsIMdbEnv* ev) = 0;
  // } ----- end attribute methods -----
    
  // { ----- begin path methods -----
  NS_IMETHOD  Path(nsIMdbEnv* ev, mdbYarn* outFilePath);
  // } ----- end path methods -----
    
  // { ----- begin replacement methods -----
  NS_IMETHOD  Steal(nsIMdbEnv* ev, nsIMdbFile* ioThief) = 0;
  NS_IMETHOD  Thief(nsIMdbEnv* ev, nsIMdbFile** acqThief);
  // } ----- end replacement methods -----

  // { ----- begin versioning methods -----
  NS_IMETHOD BecomeTrunk(nsIMdbEnv* ev) = 0;

  NS_IMETHOD AcquireBud(nsIMdbEnv* ev, nsIMdbHeap* ioHeap,
    nsIMdbFile** acqBud) = 0; 
  // } ----- end versioning methods -----

// } ===== end nsIMdbFile methods =====

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

  virtual mork_pos   Length(morkEnv* ev) const; // eof

  NS_IMETHOD Tell(nsIMdbEnv* ev, mdb_pos* outPos) const;
  NS_IMETHOD Seek(nsIMdbEnv* ev, mdb_pos inPos, mdb_pos *outPos);
//  NS_IMETHOD Eof(nsIMdbEnv* ev, mdb_pos* outPos);
  // } ----- end pos methods -----

  // { ----- begin read methods -----
  NS_IMETHOD Read(nsIMdbEnv* ev, void* outBuf, mdb_size inSize,
    mdb_size* outActualSize);
    
  // { ----- begin write methods -----
  NS_IMETHOD  Write(nsIMdbEnv* ev, const void* inBuf, mdb_size inSize,
    mdb_size* outActualSize);
//  NS_IMETHOD  Put(nsIMdbEnv* ev, const void* inBuf, mdb_size inSize,
//    mdb_pos inPos, mdb_size* outActualSize);
  NS_IMETHOD  Flush(nsIMdbEnv* ev);
  // } ----- end attribute methods -----
    
  NS_IMETHOD  Steal(nsIMdbEnv* ev, nsIMdbFile* ioThief);
   

  // { ----- begin versioning methods -----
  NS_IMETHOD BecomeTrunk(nsIMdbEnv* ev);

  NS_IMETHOD AcquireBud(nsIMdbEnv* ev, nsIMdbHeap* ioHeap,
    nsIMdbFile** acqBud); 
  // } ----- end versioning methods -----

// } ===== end nsIMdbFile methods =====

// ````` ````` ````` `````   ````` ````` ````` `````  

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
