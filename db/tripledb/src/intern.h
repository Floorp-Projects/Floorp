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

#ifndef _intern_h_
#define _intern_h_ 1

extern TDBIntern* tdbInternInit(TDBBase* base);

extern void tdbInternFree(TDBIntern* intern);

/* tdbIntern() takes a non-interned node and returns an atom representing it.
   The returned node should be free'd with TDBFreeNode(). */

extern TDBNodePtr tdbIntern(TDBBase* base, TDBNodePtr node);


/* tdbUnintern() takes an atom and allocates a new node representing it.  The
   returned node should be free'd with TDBFreeNode(). */

extern TDBNodePtr tdbUnintern(TDBBase* base, TDBNodePtr node);

/* tdbRTypeSync() causes any changes made to intern tables to be written out
   to disk. */

extern TDBStatus tdbInternSync(TDBBase* base);



#endif /* _intern_h_ */
