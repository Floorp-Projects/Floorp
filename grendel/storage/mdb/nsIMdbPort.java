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
/*| nsIMdbPort: a readonly interface to a specific database file. The mutable
**| nsIMdbStore interface is a subclass that includes writing behavior, but
**| most of the needed db methods appear in the readonly nsIMdbPort interface.
**|
**|| mdbYarn: note all nsIMdbPort and nsIMdbStore subclasses must guarantee null
**| termination of all strings written into mdbYarn instances, as long as
**| mYarn_Size and mYarn_Buf are nonzero.  Even truncated string values must
**| be null terminated.  This is more strict behavior than mdbYarn requires,
**| but it is part of the nsIMdbPort and nsIMdbStore interface.
**|
**|| attributes: methods are provided to distinguish a readonly port from a
**| mutable store, and whether a mutable store actually has any dirty content.
**|
**|| filepath: the file path used to open the port from the nsIMdbFactory can be
**| queried and discovered by GetPortFilePath(), which includes format info.
**|
**|| export: a port can write itself in other formats, with perhaps a typical
**| emphasis on text interchange formats used by other systems.  A port can be
**| queried to determine its preferred export interchange format, and a port
**| can be queried to see whether a specific export format is supported.  And
**| actually exporting a port requires a new destination file name and format.
**|
**|| tokens: a port supports queries about atomized strings to map tokens to
**| strings or strings to token integers.  (All atomized strings must be in
**| US-ASCII iso-8859-1 Latin1 charset encoding.)  When a port is actually a
**| mutable store and a string has not yet been atomized, then StringToToken()
**| will actually do so and modify the store.  The QueryToken() method will not
**| atomize a string if it has not already been atomized yet, even in stores.
**|
**|| tables: other than string tokens, all port content is presented through
**| tables, which are ordered collections of rows.  Tables are identified by
**| row scope and table kind, which might or might not be unique in a port,
**| depending on app convention.  When tables are effectively unique, then
**| queries for specific scope and kind pairs will find those tables.  To see
**| all tables that match specific row scope and table kind patterns, even in
**| the presence of duplicates, every port supports a GetPortTableCursor()
**| method that returns an iterator over all matching tables.  Table kind is
**| considered scoped inside row scope, so passing a zero for table kind will
**| find all table kinds for some nonzero row scope.  Passing a zero for row
**| scope will iterate over all tables in the port, in some undefined order.
**| (A new table can be added to a port using nsIMdbStore::NewTable(), even when
**| the requested scope and kind combination is already used by other tables.)
**|
**|| memory: callers can request that a database use less memory footprint in
**| several flavors, from an inconsequential idle flavor to a rather drastic
**| panic flavor. Callers might perform an idle purge very frequently if desired
**| with very little cost, since only normally scheduled memory management will
**| be conducted, such as freeing resources for objects scheduled to be dropped.
**| Callers should perform session memory purges infrequently because they might
**| involve costly scanning of data structures to removed cached content, and
**| session purges are recommended only when a caller experiences memory crunch.
**| Callers should only rarely perform a panic purge, in response to dire memory
**| straits, since this is likely to make db operations much more expensive
**| than they would be otherwise.  A panic purge asks a database to free as much
**| memory as possible while staying effective and operational, because a caller
**| thinks application failure might otherwise occur.  (Apps might better close
**| an open db, so panic purges only make sense when a db is urgently needed.)
|*/
public interface nsIMdbPort extends nsIMdbObject {
  
  // { ===== begin nsIMdbPort methods =====
  
  // { ----- begin attribute methods -----
  public boolean GetIsPortReadonly(nsIMdbEnv ev);
  public boolean GetIsStore(nsIMdbEnv ev);
  public boolean GetIsStoreAndDirty(nsIMdbEnv ev);
  
  public mdbUsagePolicy GetUsagePolicy(nsIMdbEnv ev);
  
  public void SetUsagePolicy(nsIMdbEnv ev, final mdbUsagePolicy inUsagePolicy);
  // } ----- end attribute methods -----
  
  // { ----- begin memory policy methods -----  
  public int IdleMemoryPurge( // do memory management already scheduled
                             nsIMdbEnv ev); // context
  // approximate bytes actually freed
  
  public int SessionMemoryPurge( // request specific footprint decrease
                                nsIMdbEnv ev, // context
                                int inDesiredBytesFreed); // approximate number of bytes wanted
  // approximate bytes actually freed
  
  public int PanicMemoryPurge( // desperately free all possible memory
                              nsIMdbEnv ev); // context
  // approximate bytes actually freed
  // } ----- end memory policy methods -----
  
  // { ----- begin filepath methods -----
  public String GetPortFilePath(
                                nsIMdbEnv ev); // context
  // name of file holding port content
  // file format description
  public String GetFormatVersion(nsIMdbEnv ev);
  // } ----- end filepath methods -----
  
  // { ----- begin export methods -----
  public String BestExportFormat( // determine preferred export format
                                 nsIMdbEnv ev); // context
  // file format description
  
  // some tentative suggested import/export formats
  // "ns:msg:db:port:format:ldif:ns4.0:passthrough" // necessary
  // "ns:msg:db:port:format:ldif:ns4.5:utf8"        // necessary
  // "ns:msg:db:port:format:ldif:ns4.5:tabbed"
  // "ns:msg:db:port:format:ldif:ns4.5:binary"      // necessary
  // "ns:msg:db:port:format:html:ns3.0:addressbook" // necessary
  // "ns:msg:db:port:format:html:display:verbose"
  // "ns:msg:db:port:format:html:display:concise"
  // "ns:msg:db:port:format:mork:zany:verbose"      // necessary
  // "ns:msg:db:port:format:mork:zany:atomized"     // necessary
  // "ns:msg:db:port:format:rdf:xml"
  // "ns:msg:db:port:format:xml:mork"
  // "ns:msg:db:port:format:xml:display:verbose"
  // "ns:msg:db:port:format:xml:display:concise"
  // "ns:msg:db:port:format:xml:print:verbose"      // recommended
  // "ns:msg:db:port:format:xml:print:concise"
  
  public boolean
    CanExportToFormat( // can export content in given specific format?
                      nsIMdbEnv ev, // context
                      final String inFormatVersion); // file format description
  // whether ExportSource() might succeed
  
  public nsIMdbThumb ExportToFormat( // export content in given specific format
                                    nsIMdbEnv ev, // context
                                    final String inFilePath, // the file to receive exported content
                                    final String inFormatVersion); // file format description
  // acquire thumb for incremental export
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the export will be finished.
  
  // } ----- end export methods -----
  
  // { ----- begin token methods -----
  public String TokenToString( // return a string name for an integer token
                              nsIMdbEnv ev, // context
                              int inToken); // token for inTokenName inside this port
  // the type of table to access
  
  public int StringToToken( // return an integer token for scope name
                           nsIMdbEnv ev, // context
                           final String inTokenName); // Latin1 string to tokenize if possible
  // token for inTokenName inside this port
  
  // String token zero is never used and never supported. If the port
  // is a mutable store, then StringToToken() to create a new
  // association of inTokenName with a new integer token if possible.
  // But a readonly port will return zero for an unknown scope name.
  
  public int QueryToken( // like StringToToken(), but without adding
                        nsIMdbEnv ev, // context
                        final String inTokenName); // Latin1 string to tokenize if possible
  // token for inTokenName inside this port
  
  // QueryToken() will return a string token if one already exists,
  // but unlike StringToToken(), will not assign a new token if not
  // already in use.
  
  // } ----- end token methods -----
  
  // { ----- begin row methods -----  
  public boolean HasRow( // contains a row with the specified oid?
                        nsIMdbEnv ev, // context
                        final mdbOid inOid);  // hypothetical row oid
  // whether GetRow() might succeed
  
  public nsIMdbRow GetRow( // access one row with specific oid
                          nsIMdbEnv ev, // context
                          final mdbOid inOid);  // hypothetical row oid
  // acquire specific row (or null)
  
  public int GetRowRefCount( // get number of tables that contain a row 
                            nsIMdbEnv ev, // context
                            final mdbOid inOid);  // hypothetical row oid
  // number of tables containing inRowKey 
  // } ----- end row methods -----
  
  // { ----- begin table methods -----  
  public boolean HasTable( // supports a table with the specified oid?
                          nsIMdbEnv ev, // context
                          final mdbOid inOid);  // hypothetical table oid
  // whether GetTable() might succeed
  
  public nsIMdbTable GetTable( // access one table with specific oid
                              nsIMdbEnv ev, // context
                              final mdbOid inOid);  // hypothetical table oid
  // acquire specific table (or null)
  
  public boolean HasTableKind( // supports a table of the specified type?
                              nsIMdbEnv ev, // context
                              int inRowScope, // rid scope for row ids
                              int inTableKind); // the type of table to access
  // whether GetTableKind() might succeed
  
  public int GetTableKindCount( // supports a table of the specified type?
                               nsIMdbEnv ev, // context
                               int inRowScope, // rid scope for row ids
                               int inTableKind); // the type of table to access
  
  // row scopes to be supported include the following suggestions:
  // "ns:msg:db:row:scope:address:cards:all"
  // "ns:msg:db:row:scope:mail:messages:all"
  // "ns:msg:db:row:scope:news:articles:all"
  
  // table kinds to be supported include the following suggestions:
  // "ns:msg:db:table:kind:address:cards:main"
  // "ns:msg:db:table:kind:address:lists:all" 
  // "ns:msg:db:table:kind:address:list" 
  // "ns:msg:db:table:kind:news:threads:all" 
  // "ns:msg:db:table:kind:news:thread" 
  // "ns:msg:db:table:kind:mail:threads:all"
  // "ns:msg:db:table:kind:mail:thread"
  
  public nsIMdbTable GetTableKind( // access one (random) table of specific type
                                  nsIMdbEnv ev, // context
                                  int inRowScope,      // row scope for row ids
                                  int inTableKind);      // the type of table to access
  // current number of such tables
  //mdb_bool* outMustBeUnique); // whether port can hold only one of these
  // acquire scoped collection of rows
  
  public nsIMdbPortTableCursor
    GetPortTableCursor( // get cursor for all tables of specific type
                       nsIMdbEnv ev, // context
                       int inRowScope, // row scope for row ids
                       int inTableKind); // the type of table to access
  // all such tables in the port
  // } ----- end table methods -----
  
  // } ===== end nsIMdbPort methods =====
};

