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

import grendel.storage.mdb.*;
import java.util.*;

/**
 * Vector of scopes for use in db opening policy 
 */
class mdbScopeStringSet extends Vector { 
};

/**
 * Class that defines policies affecting db usage for ports and stores
 */
class mdbOpenPolicy {
  /** predeclare scope usage plan */
  private mdbScopeStringSet  ScopePlan; 
  /** nonzero: do least work */
  private byte               MaxLazy;   
  /** nonzero: use least memory */
  private byte               MinMemory; 
  
  public mdbOpenPolicy() {
    super();
  }
  public mdbOpenPolicy(mdbScopeStringSet aScopePlan, byte aMaxLazy, byte aMinMemory) {
    super();
    ScopePlan = aScopePlan;
    MaxLazy = aMaxLazy;
    MinMemory = aMinMemory;
  }
  
  public mdbScopeStringSet getScopePlan(){
    return ScopePlan;
  }
  public void setScopePlan(mdbScopeStringSet aScope) {
    ScopePlan = aScope;
  }
  
  public byte getMaxLazy() {
    return MaxLazy;
  }
  
  public void setMaxLazy(byte aMaxLazy) {
    MaxLazy = aMaxLazy;
  }
  
  public byte getMinMemory() {
    return MinMemory;
  }
  
  public void setMinMemory(byte aMinMemory) {
    MinMemory = aMinMemory;
  }
};

/**
 * array for a set of tokens, and actual slots used
 */
class mdbTokenSet extends Vector {
}

/** 
 * mdbUsagePolicy: another version of mdbOpenPolicy which uses tokens instead
 * of scope strings, because usage policies can be constructed for use with a
 * db that is already open, while an open policy must be constructed before a
 * db has yet been opened.
 * policies affecting db usage for ports and stores
 */
class mdbUsagePolicy {
  /** current scope usage plan */
  private mdbTokenSet  ScopePlan; 
  /** nonzero: do least work */
  private byte         MaxLazy;   
  /** nonzero: use least memory */
  private byte         MinMemory; 
  
  public mdbUsagePolicy() {
    super ();
  }
  
  public mdbUsagePolicy(mdbTokenSet aScopePlan, byte aMaxLazy, byte aMinMemory) {
    ScopePlan = aScopePlan;
    MaxLazy = aMaxLazy;
    MinMemory = aMinMemory;
  }
  
  public mdbTokenSet getScopePlan() {
    return ScopePlan;
  }
  
  public void setScopePlan(mdbTokenSet aScopePlan){
    ScopePlan = aScopePlan;
  }
  
  public byte getMaxLazy() {
    return MaxLazy;
  }
  public void setMaxLazy(byte aMaxLazy) {
    MaxLazy = aMaxLazy;
  }
  
  public byte getMinMemory() {
    return MinMemory;
  }
  
  public void SetMinMemory(byte aMinMemory) {
    MinMemory = aMinMemory;
  }
};

/**
 * identity of some row or table inside a database
 */
class mdbOid { 
  /** scope token for an id's namespace */
  private int   Scope;  
  /** identity of object inside scope namespace */
  private int   Id;
  
  public mdbOid() {
    super();
  }
  
  public mdbOid(int aScope, int aId) {
    Scope = aScope;
    aId = aId;
  }
  
  public int getScope() {
    return Scope;
  }
  
  public void setScope(int aScope) {
    Scope = aScope;
  }
  public int getId() {
    return Id;
  }
  public void setId(int aId) {
    Id = aId;
  }
};

/**
 * range of row positions in a table
 */
class mdbRange {
  /** position of first row */
  private int FirstPos;
  /** position of last row */
  private int LastPos;
  public mdbRange(){
    super();
  }
  public mdbRange(int aFirstPos, int aLastPos){
    FirstPos = aFirstPos;
    LastPos = aLastPos;
  }
  public int getFirstPos() {
    return FirstPos;
  }
  public void setFirstPos(int aFirstPos) {
    FirstPos = aFirstPos;
  }
  
  public int getLastPos(int aLastPos) {
    return LastPos;
  }
  public void setLastPos(int aLastPos) {
    LastPos = aLastPos;
    
  }
}


/** 
 * array of column tokens (just the same as mdbTokenSet)
 */
class mdbColumnSet extends Vector {
  
}

/**
 * parallel in and out arrays for search results
 */
class mdbSearch { 
  /** number of columns and ranges */
  private int     Count;    
  /** count mdb_column instances */
  private Vector  Columns;  
  /** count mdbRange instances */
  private Vector  Ranges;   
  
  public mdbSearch(Vector aColumns, Vector aRanges) {
    Columns = aColumns;
    Ranges = aRanges;
  }
  
  public mdbSearch() {
    super();
  }
  
  public int getCount() {
    return Count;
  }
  
  public void setColumns(Vector aColumns) {
    Columns = aColumns;
  }
  
  public Vector getRanges() {
    return Ranges;
  }
  
  public void setRanges(Vector aRanges) {
    Ranges = aRanges;
  }
};
