/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is
 * Geocast Network Systems.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Weissman <terry@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _rtype_h_
#define _rtype_h_ 1


extern TDBRType* tdbRTypeNew(TDBBase* base, const char* name, TDBUint8 code,
                             TDBInt32 numfields, TDBInt32* fieldnums);



/* tdbRTypeFree() frees the given rtype. */

extern void tdbRTypeFree(TDBRType* rtype);



/* tdbRTypeGetBase() gets the TDB associated with this rtype. */

extern TDBBase* tdbRTypeGetBase(TDBRType* rtype);

/* tdbRTypeGetCode() returns the code associated with the given rtype. */

extern TDBUint8 tdbRTypeGetCode(TDBRType* rtype);

/* tdbRTypeGetName() returns the name associated with the given rtype. */

extern const char* tdbRTypeGetName(TDBRType* rtype);


/* tdbRTypeGetNumFields() returns how many fields this type has.  If it is
   an index, this is the number of fields it indexes; if it is a base type,
   this is the number of different values we are storing in this type; if it
   is an extension type, this is zero. */

extern TDBInt32 tdbRTypeGetNumFields(TDBRType* rtype);



/* tdbRTypeIndexGetField() returns information about one field that this type
   indexes. */

extern TDBStatus tdbRTypeIndexGetField(TDBRType* rtype, TDBInt32 which,
                                       TDBInt32* fieldnum);



/* tdbRTypeFromCode() returns the rtype entry given the code number. */

extern TDBRType* tdbRTypeFromCode(TDBBase* base, TDBUint8 code);



/* tdbRTypeGetNextRType() gets the next rtype from the master list of all
   rtypes. */

extern TDBRType* tdbRTypeGetNextRType(TDBRType* rtype);


extern TDBStatus tdbRTypeAdd(TDBRType* rtype, TDBVector* vector);


extern TDBStatus tdbRTypeRemove(TDBRType* rtype, TDBVector* vector);


/* tdbRTypeGetCursor() gets a DB cursor into this index, and starts it pointing
   at the first node greater than or equal to the given vector. */

extern DBC* tdbRTypeGetCursor(TDBRType* rtype, TDBVector* vector);


/* tdbRTypeGetCursorValue() passes the given flags to the cursor, and retrieves
   the value of the resulting position of the cursor, translating it to
   a TDBVector. */

extern TDBVector* tdbRTypeGetCursorValue(TDBRType* rtype, DBC* cursor,
                                         int flags);


/* tdbRTypeSync() causes any changes made to this object to be written out
   to disk. */

extern TDBStatus tdbRTypeSync(TDBRType* rtype);


/* tdbRTypeProbe() determines whether the given vector seems to be included
   as part of this index. */
extern TDBBool tdbRTypeProbe(TDBRType* rtype, TDBVector* vector);


#endif /* _rtype_h_ */
