/* -*- Mode: C; indent-tabs-mode: nil; -*-
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
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is Geocast Network Systems.
 * Portions created by Geocast are
 * Copyright (C) 2000 Geocast Network Systems. All
 * Rights Reserved.
 *
 * Contributor(s): Terry Weissman <terry@mozilla.org>
 */

#ifndef _vector_h_
#define _vector_h_ 1

extern TDBVector* tdbVectorNewFromNodes(TDBBase* base, TDBNodePtr* nodes,
                                        TDBUint8 layer, TDBUint8 flags,
                                        TDBUint32 recordnum);

extern TDBVector* tdbVectorNewFromRecord(TDBBase* base, TDBUint32 recordnum);

extern TDBStatus tdbVectorPutInNewRecord(TDBVector* vector, TDBUint64 owner);

extern TDBUint32 tdbVectorGetRecordNumber(TDBVector* vector);

extern TDBUint64 tdbVectorGetOwner(TDBVector* vector);

extern void tdbVectorFree(TDBVector* vector);

extern TDBNodePtr tdbVectorGetNonInternedNode(TDBVector* vector,
                                              TDBInt32 which);

extern TDBNodePtr tdbVectorGetInternedNode(TDBVector* vector, TDBInt32 which);

extern TDBUint8 tdbVectorGetFlags(TDBVector* vector);

extern TDBUint8 tdbVectorGetLayer(TDBVector* vector);

extern TDBInt32 tdbVectorGetNumFields(TDBVector* vector);

extern TDBBool tdbVectorEqual(TDBVector* v1, TDBVector* v2);

/* tdbVectorLayerMatches() returns TRUE if the given vector is in one of the
   layers specified by the given TDB. */

extern TDBBool tdbVectorLayerMatches(TDBVector* vector, TDB* tdb);




#endif /* _vector_h_ */
