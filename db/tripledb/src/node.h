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

#ifndef _node_h_
#define _node_h_ 1




extern TDBNode* tdbNodeDup(TDBNode* node);
extern TDBInt32 tdbNodeSize(TDBNode* node);
extern TDBNode* tdbGetNode(char** ptr, char* endptr);
extern TDBStatus tdbPutNode(char** ptr, TDBNode* node, char* endptr);


extern TDBNode* tdbGetNodeFromKey(char** ptr, char* endptr);
extern TDBStatus tdbPutNodeToKey(char** ptr, TDBNode* node, char* endptr,
                                 TDBUint8 *flags);

#endif /* _node_h_ */
