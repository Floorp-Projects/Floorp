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

#ifndef _ORKINENV_
#define _ORKINENV_ 1

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

#ifndef _MORKEnv_
#include "morkEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkMagic_kEnv 0x456E7669 /* ascii 'Envi' */

/*| orkinEnv:
|*/
class orkinEnv : public morkHandle, public nsIMdbEnv { // nsIMdbObject

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinEnv(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinEnv(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkEnv* ioObject); // must not be nil, the object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinEnv(const morkHandle& other);
  orkinEnv& operator=(const morkHandle& other);

// public: // dynamic type identification
  // mork_bool IsHandle() const //
  // { return IsNode() && mNode_Derived == morkDerived_kHandle; }
// } ===== end morkNode methods =====

protected: // morkHandle memory management operators
  void* operator new(size_t inSize, morkPool& ioPool, morkZone& ioZone, morkEnv* ev)
  { return ioPool.NewHandle(ev, inSize, &ioZone); }
  
  void* operator new(size_t inSize, morkPool& ioPool, morkEnv* ev)
  { return ioPool.NewHandle(ev, inSize, (morkZone*) 0); }
  
  void* operator new(size_t inSize, morkHandleFace* ioFace)
  { MORK_USED_1(inSize); return ioFace; }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkHandle instances.  They are collected.
  
public: // construction:

  static orkinEnv* MakeEnv(morkEnv* ev, morkEnv* ioObject);

public: // utilities:

  morkEnv* CanUseEnv(mork_bool inMutable, mdb_err* outErr) const;

public: // type identification
  mork_bool IsOrkinEnv() const
  { return mHandle_Magic == morkMagic_kEnv; }

  mork_bool IsOrkinEnvHandle() const
  { return this->IsHandle() && this->IsOrkinEnv(); }

public:

// { ===== begin nsIMdbISupports methods =====
  virtual mdb_err AddRef(); // add strong ref with no
  virtual mdb_err Release(); // cut strong ref
// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbObject methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err IsFrozenMdbObject(nsIMdbEnv* ev, mdb_bool* outIsReadonly);
  // same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
  // } ----- end attribute methods -----

  // { ----- begin factory methods -----
  virtual mdb_err GetMdbFactory(nsIMdbEnv* ev, nsIMdbFactory** acqFactory); 
  // } ----- end factory methods -----

  // { ----- begin ref counting for well-behaved cyclic graphs -----
  virtual mdb_err GetWeakRefCount(nsIMdbEnv* ev, // weak refs
    mdb_count* outCount);  
  virtual mdb_err GetStrongRefCount(nsIMdbEnv* ev, // strong refs
    mdb_count* outCount);

  virtual mdb_err AddWeakRef(nsIMdbEnv* ev);
  virtual mdb_err AddStrongRef(nsIMdbEnv* ev);

  virtual mdb_err CutWeakRef(nsIMdbEnv* ev);
  virtual mdb_err CutStrongRef(nsIMdbEnv* ev);
  
  virtual mdb_err CloseMdbObject(nsIMdbEnv* ev); // called at strong refs zero
  virtual mdb_err IsOpenMdbObject(nsIMdbEnv* ev, mdb_bool* outOpen);
  // } ----- end ref counting -----
  
// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbEnv methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetErrorCount(mdb_count* outCount,
    mdb_bool* outShouldAbort);
  virtual mdb_err GetWarningCount(mdb_count* outCount,
    mdb_bool* outShouldAbort);
  
  virtual mdb_err GetEnvBeVerbose(mdb_bool* outBeVerbose);
  virtual mdb_err SetEnvBeVerbose(mdb_bool inBeVerbose);
  
  virtual mdb_err GetDoTrace(mdb_bool* outDoTrace);
  virtual mdb_err SetDoTrace(mdb_bool inDoTrace);
  
  virtual mdb_err GetAutoClear(mdb_bool* outAutoClear);
  virtual mdb_err SetAutoClear(mdb_bool inAutoClear);
  
  virtual mdb_err GetErrorHook(nsIMdbErrorHook** acqErrorHook);
  virtual mdb_err SetErrorHook(
    nsIMdbErrorHook* ioErrorHook); // becomes referenced
  
  virtual mdb_err GetHeap(nsIMdbHeap** acqHeap);
  virtual mdb_err SetHeap(
    nsIMdbHeap* ioHeap); // becomes referenced
  // } ----- end attribute methods -----
  
  virtual mdb_err ClearErrors(); // clear errors beore re-entering db API
  virtual mdb_err ClearWarnings(); // clear warnings
  virtual mdb_err ClearErrorsAndWarnings(); // clear both errors & warnings
// } ===== end nsIMdbEnv methods =====
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINENV_ */
