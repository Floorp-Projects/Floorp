/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Mauro Botelho <mabotelh@email.com>, 20 Mar 1999.
 */

package grendel.storage.mdb;

/*| nsIMdbBlob: a base class for objects composed mainly of byte sequence state.
**| (This provides a base class for nsIMdbCell, so that cells themselves can
**| be used to set state in another cell, without extracting a buffer.)
|*/
interface nsIMdbBlob extends nsIMdbObject { // a string with associated charset
  
  // { ===== begin nsIMdbBlob methods =====
  
  // { ----- begin attribute methods -----
  public void SetBlob(nsIMdbEnv ev,
                      nsIMdbBlob ioBlob); // reads inBlob slots
  // when inBlob is in the same suite, this might be fastest cell-to-cell
  
  public void ClearBlob( // make empty (so content has zero length)
                        nsIMdbEnv ev);
  // clearing a yarn is like SetYarn() with empty yarn instance content
  
  public int GetBlobFill(nsIMdbEnv ev);
  // size of blob 
  // Same value that would be put into mYarn_Fill, if one called GetYarn()
  // with a yarn instance that had mYarn_Buf==nil and mYarn_Size==0.
  
  public void SetYarn(nsIMdbEnv ev, 
                      final String inYarn);   // reads from yarn slots
  // make this text object contain content from the yarn's buffer
  
  public String GetYarn(nsIMdbEnv ev);
  // writes some yarn slots 
  // copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
  
  public String AliasYarn(nsIMdbEnv ev);
  // writes ALL yarn slots
  // AliasYarn() reveals sensitive internal text buffer state to the caller
  // by setting mYarn_Buf to point into the guts of this text implementation.
  //
  // The caller must take great care to avoid writing on this space, and to
  // avoid calling any method that would cause the state of this text object
  // to change (say by directly or indirectly setting the text to hold more
  // content that might grow the size of the buffer and free the old buffer).
  // In particular, callers should scrupulously avoid making calls into the
  // mdb interface to write any content while using the buffer pointer found
  // in the returned yarn instance.  Best safe usage involves copying content
  // into some other kind of external content representation beyond mdb.
  //
  // (The original design of this method a week earlier included the concept
  // of very fast and efficient cooperative locking via a pointer to some lock
  // member slot.  But let's ignore that complexity in the current design.)
  //
  // AliasYarn() is specifically intended as the first step in transferring
  // content from nsIMdbBlob to a nsString representation, without forcing extra
  // allocations and/or memory copies. (A standard nsIMdbBlob_AsString() utility
  // will use AliasYarn() as the first step in setting a nsString instance.)
  //
  // This is an alternative to the GetYarn() method, which has copy semantics
  // only; AliasYarn() relaxes a robust safety principle only for performance
  // reasons, to accomodate the need for callers to transform text content to
  // some other canonical representation that would necessitate an additional
  // copy and transformation when such is incompatible with the mdbYarn format.
  //
  // The implementation of AliasYarn() should have extremely little overhead
  // besides the public dispatch to the method implementation, and the code
  // necessary to populate all the mdbYarn member slots with internal buffer
  // address and metainformation that describes the buffer content.  Note that
  // mYarn_Grow must always be set to nil to indicate no resizing is allowed.
  
  // } ----- end attribute methods -----
  
  // } ===== end nsIMdbBlob methods =====
};

