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

#ifndef _windex_h_
#define _windex_h_ 1

extern TDBWindex* tdbWindexNew(TDBBase* base);

extern void tdbWindexFree(TDBWindex* windex);

extern TDBStatus tdbWindexSync(TDBWindex* windex);

extern TDBStatus tdbWindexAdd(TDBWindex* windex, TDBVector* vector);

extern TDBStatus tdbWindexRemove(TDBWindex* windex, TDBVector* vector);

extern TDBWindexCursor* tdbWindexGetCursor(TDBWindex* windex,
                                           const char* string);

extern TDBStatus tdbWindexCursorFree(TDBWindexCursor* cursor);

extern TDBVector* tdbWindexGetCursorValue(TDBWindexCursor* cursor);


#endif /* _windex_h_ */
