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

/*| nsIMdbFactory: the main entry points to the abstract db interface.  A DLL
**| that supports this mdb interface need only have a single exported method
**| that will return an instance of nsIMdbFactory, so that further methods in
**| the suite can be accessed from objects returned by nsIMdbFactory methods.
**|
**|| mdbYarn: note all nsIMdbFactory subclasses must guarantee null
**| termination of all strings written into mdbYarn instances, as long as
**| mYarn_Size and mYarn_Buf are nonzero.  Even truncated string values must
**| be null terminated.  This is more strict behavior than mdbYarn requires,
**| but it is part of the nsIMdbFactory interface.
**|
**|| envs: an environment instance is required as per-thread context for
**| most of the db method calls, so nsIMdbFactory creates such instances.
**|
**|| rows: callers must be able to create row instances that are independent
**| of storage space that is part of the db content graph.  Many interfaces
**| for data exchange have strictly copy semantics, so that a row instance
**| has no specific identity inside the db content model, and the text in
**| cells are an independenty copy of unexposed content inside the db model.
**| Callers are expected to maintain one or more row instances as a buffer
**| for staging cell content copied into or out of a table inside the db.
**| Callers are urged to use an instance of nsIMdbRow created by the nsIMdbFactory
**| code suite, because reading and writing might be much more efficient than
**| when using a hand-rolled nsIMdbRow subclass with no relation to the suite.
**|
**|| ports: a port is a readonly interface to a specific database file. Most
**| of the methods to access a db file are suitable for a readonly interface,
**| so a port is the basic minimum for accessing content.  This makes it
**| possible to read other external formats for import purposes, without
**| needing the code or competence necessary to write every such format.  So
**| we can write generic import code just once, as long as every format can
**| show a face based on nsIMdbPort. (However, same suite import can be faster.)
**| Given a file name and the first 512 bytes of a file, a factory can say if
**| a port can be opened by this factory.  Presumably an app maintains chains
**| of factories for different suites, and asks each in turn about opening a
**| a prospective file for reading (as a port) or writing (as a store).  I'm
**| not ready to tackle issues of format fidelity and factory chain ordering.
**|
**|| stores: a store is a mutable interface to a specific database file, and
**| includes the port interface plus any methods particular to writing, which
**| are few in number.  Presumably the set of files that can be opened as
**| stores is a subset of the set of files that can be opened as ports.  A
**| new store can be created with CreateNewFileStore() by supplying a new
**| file name which does not yet exist (callers are always responsible for
**| destroying any existing files before calling this method). 
|*/
public interface nsIMdbFactory extends nsIMdbObject { // suite entry points
  
  // { ===== begin nsIMdbFactory methods =====
  
  // { ----- begin env methods -----
  public nsIMdbEnv MakeEnv(); // acquire new env
  // ioHeap can be nil, causing a MakeHeap() style heap instance to be used
  // } ----- end env methods -----
  
  // { ----- begin heap methods -----
  // public int MakeHeap(nsIMdbEnv ev, nsIMdbHeap acqHeap); // acquire new heap
  // } ----- end heap methods -----
  
  // { ----- begin row methods -----
  public nsIMdbRow MakeRow(nsIMdbEnv ev); // new row
  // ioHeap can be nil, causing the heap associated with ev to be used
  // } ----- end row methods -----
  
  // { ----- begin port methods -----
  public boolean CanOpenFilePort(
                                 nsIMdbEnv ev, // context
                                 final String inFilePath, // the file to investigate
                                 // const mdbYarn* inFirst512Bytes,
                                 String inFirst512Bytes); 
  
  public String getFormatVersion(
                                 nsIMdbEnv ev, // context
                                 final String inFilePath, // the file to investigate
                                 // const mdbYarn* inFirst512Bytes,
                                 String inFirst512Bytes);
  
  public nsIMdbThumb OpenFilePort(
                                  nsIMdbEnv ev, // context
                                  final String inFilePath, // the file to open for readonly import
                                  final mdbOpenPolicy inOpenPolicy); // runtime policies for using db
  
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then call nsIMdbFactory::ThumbToOpenPort() to get the port instance.
  
  public nsIMdbPort ThumbToOpenPort( // redeeming a completed thumb from OpenFilePort()
                                    nsIMdbEnv ev, // context
                                    nsIMdbThumb ioThumb); // thumb from OpenFilePort() with done status
  // } ----- end port methods -----
  
  // { ----- begin store methods -----
  public boolean CanOpenFileStore(
                                  nsIMdbEnv ev, // context
                                  final String inFilePath); // the file to investigate
  //const mdbYarn* inFirst512Bytes,
  // mdb_bool* outCanOpenAsStore, // whether OpenFileStore() might succeed
  // mdb_bool* outCanOpenAsPort, // whether OpenFilePort() might succeed
  // mdbYarn* outFormatVersion); // informal file format description
  
  public nsIMdbThumb OpenFileStore( // open an existing database
                                   nsIMdbEnv ev, // context
                                   final String inFilePath, // the file to open for general db usage
                                   final mdbOpenPolicy inOpenPolicy); // runtime policies for using db
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then call nsIMdbFactory::ThumbToOpenStore() to get the store instance.
  
  public nsIMdbStore ThumbToOpenStore( // redeem completed thumb from OpenFileStore()
                                      nsIMdbEnv ev, // context
                                      nsIMdbThumb ioThumb); // thumb from OpenFileStore() with done status
  
  public nsIMdbStore CreateNewFileStore( // create a new db with minimal content
                                        nsIMdbEnv ev, // context
                                        //nsIMdbHeap ioHeap, // can be nil to cause ev's heap attribute to be used
                                        final String inFilePath, // name of file which should not yet exist
                                        final mdbOpenPolicy inOpenPolicy); // runtime policies for using db
  // } ----- end store methods -----
  
  // } ===== end nsIMdbFactory methods =====
};

