/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _MORK_
#define _MORK_ 1

#ifndef _MDB_
#include "mdb.h"
#endif

#include "nscore.h"
//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


// { %%%%% begin disable unused param warnings %%%%%
#define MORK_USED_1(x) (void)(&x)
#define MORK_USED_2(x,y) (void)(&x);(void)(&y);
#define MORK_USED_3(x,y,z) (void)(&x);(void)(&y);(void)(&z);
#define MORK_USED_4(w,x,y,z) (void)(&w);(void)(&x);(void)(&y);(void)(&z);

// } %%%%% end disable unused param warnings %%%%%

// { %%%%% begin macro for finding class member offset %%%%%

/*| OffsetOf: the unsigned integer offset of a class or struct
**| field from the beginning of that class or struct.  This is
**| the same as the similarly named public domain IronDoc macro,
**| and is also the same as another macro appearing in stdlib.h.
**| We want these offsets so we can correctly convert pointers
**| to member slots back into pointers to enclosing objects, and
**| have this exactly match what the compiler thinks is true.
**|
**|| Bascially we are asking the compiler to determine the offset at
**| compile time, and we use the definition of address artithmetic
**| to do this.  By casting integer zero to a pointer of type obj*,
**| we can reference the address of a slot in such an object that
**| is hypothetically physically placed at address zero, but without
**| actually dereferencing a memory location.  The absolute address
**| of slot is the same as offset of that slot, when the object is
**| placed at address zero.
|*/
#define mork_OffsetOf(obj,slot) ((unsigned int)&((obj*) 0)->slot)

// } %%%%% end macro for finding class member offset %%%%%

// { %%%%% begin specific-size integer scalar typedefs %%%%%
typedef unsigned char  mork_u1;  // make sure this is one byte
typedef unsigned short mork_u2;  // make sure this is two bytes
typedef short          mork_i2;  // make sure this is two bytes
typedef PRUint32       mork_u4;  // make sure this is four bytes
typedef PRInt32        mork_i4;  // make sure this is four bytes
typedef long           mork_ip;  // make sure sizeof(mork_ip) == sizeof(void*)

typedef mork_u1 mork_ch;    // small byte-sized character (never wide)
typedef mork_u1 mork_flags;  // one byte's worth of predicate bit flags

typedef mork_u2 mork_base;    // 2-byte magic class signature slot in object
typedef mork_u2 mork_derived; // 2-byte magic class signature slot in object
typedef mork_u2 mork_uses;    // 2-byte strong uses count
typedef mork_u2 mork_refs;    // 2-byte actual reference count

typedef mork_u4 mork_token;      // unsigned token for atomized string
typedef mork_token mork_scope;   // token used to id scope for rows
typedef mork_token mork_kind;    // token used to id kind for tables
typedef mork_token mork_cscode;  // token used to id charset names
typedef mork_token mork_aid;     // token used to id atomize cell values

typedef mork_token mork_column;  // token used to id columns for rows
typedef mork_column mork_delta;  // mork_column plus mork_change 

typedef mork_token mork_color;   // bead ID
#define morkColor_kNone ((mork_color) 0)

typedef mork_u4 mork_magic;      // unsigned magic signature

typedef mork_u4 mork_seed;       // unsigned collection change counter
typedef mork_u4 mork_count;      // unsigned collection member count
typedef mork_count mork_num;     // synonym for count
typedef mork_u4 mork_size;       // unsigned physical media size
typedef mork_u4 mork_fill;       // unsigned logical content size
typedef mork_u4 mork_more;       // more available bytes for larger buffer

typedef mdb_u4 mork_percent; // 0..100, with values >100 same as 100

typedef mork_i4 mork_pos; // negative means "before first" (at zero pos)
typedef mork_i4 mork_line; // negative means "before first line in file"

typedef mork_u1 mork_usage;   // 1-byte magic usage signature slot in object
typedef mork_u1 mork_access;  // 1-byte magic access signature slot in object

typedef mork_u1 mork_change; // add, cut, put, set, nil
typedef mork_u1 mork_priority; // 0..9, for a total of ten different values

typedef mork_u1 mork_able; // on, off, asleep (clone IronDoc's fe_able)
typedef mork_u1 mork_load; // dirty or clean (clone IronDoc's fe_load)
// } %%%%% end specific-size integer scalar typedefs %%%%%

// 'test' is a public domain Mithril for key equality tests in probe maps
typedef mork_i2 mork_test; /* neg=>kVoid, zero=>kHit, pos=>kMiss */

#define morkTest_kVoid ((mork_test) -1) /* -1: nil key slot, no key order */
#define morkTest_kHit  ((mork_test) 0)  /*  0: keys are equal, a map hit */
#define morkTest_kMiss ((mork_test) 1)  /*  1: keys not equal, a map miss */

// { %%%%% begin constants for Mork scalar types %%%%%
#define morkPriority_kHi  ((mork_priority) 0) /* best priority */
#define morkPriority_kMin ((mork_priority) 0) /* best priority is smallest */

#define morkPriority_kLo  ((mork_priority) 9) /* worst priority */
#define morkPriority_kMax ((mork_priority) 9) /* worst priority is biggest */

#define morkPriority_kCount 10 /* number of distinct priority values */

#define morkAble_kEnabled  ((mork_able) 0x55) /* same as IronDoc constant */
#define morkAble_kDisabled ((mork_able) 0xAA) /* same as IronDoc constant */
#define morkAble_kAsleep   ((mork_able) 0x5A) /* same as IronDoc constant */

#define morkChange_kAdd 'a' /* add member */
#define morkChange_kCut 'c' /* cut member */
#define morkChange_kPut 'p' /* put member */
#define morkChange_kSet 's' /* set all members */
#define morkChange_kNil 0   /* no change in this member */
#define morkChange_kDup 'd' /* duplicate changes have no effect */
// kDup is intended to replace another change constant in an object as a
// conclusion about change feasibility while staging intended alterations.

#define morkLoad_kDirty ((mork_load) 0xDD) /* same as IronDoc constant */
#define morkLoad_kClean ((mork_load) 0x22) /* same as IronDoc constant */

#define morkAccess_kOpen    'o'
#define morkAccess_kClosing 'c'
#define morkAccess_kShut    's'
#define morkAccess_kDead    'd'
// } %%%%% end constants for Mork scalar types %%%%%

// { %%%%% begin non-specific-size integer scalar typedefs %%%%%
typedef int mork_char; // nominal type for ints used to hold input byte
#define morkChar_IsWhite(c) \
  ((c) == 0xA || (c) == 0x9 || (c) == 0xD || (c) == ' ')
// } %%%%% end non-specific-size integer scalar typedefs %%%%%

// { %%%%% begin mdb-driven scalar typedefs %%%%%
// easier to define bool exactly the same as mdb:
typedef mdb_bool mork_bool; // unsigned byte with zero=false, nonzero=true

/* canonical boolean constants provided only for code clarity: */
#define morkBool_kTrue  ((mork_bool) 1) /* actually any nonzero means true */
#define morkBool_kFalse ((mork_bool) 0) /* only zero means false */

// mdb clients can assign these, so we cannot pick maximum size:
typedef mdb_id mork_id;    // unsigned object identity in a scope
typedef mork_id mork_rid;  // unsigned row identity inside scope
typedef mork_id mork_tid;  // unsigned table identity inside scope
typedef mork_id mork_gid;  // unsigned group identity without any scope

// we only care about neg, zero, pos -- so we don't care about size:
typedef mdb_order mork_order; // neg:lessthan, zero:equalto, pos:greaterthan 
// } %%%%% end mdb-driven scalar typedefs %%%%%

#define morkId_kMinusOne ((mdb_id) -1)

// { %%%%% begin class forward defines %%%%%
// try to put these in alphabetical order for easier examination:
class morkMid;
class morkAtom;
class morkAtomSpace;
class morkBookAtom;
class morkBuf;
class morkBuilder;
class morkCell;
class morkCellObject;
class morkCursor;
class morkEnv;
class morkFactory;
class morkFile;
class morkHandle;
class morkHandleFace; // just an opaque cookie type
class morkHandleFrame;
class morkHashArrays;
class morkMap;
class morkNode;
class morkObject;
class morkOidAtom;
class morkParser;
class morkPool;
class morkPlace;
class morkPort;
class morkPortTableCursor;
class morkProbeMap;
class morkRow;
class morkRowCellCursor;
class morkRowObject;
class morkRowSpace;
class morkSorting;
class morkSortingRowCursor;
class morkSpace;
class morkSpan;
class morkStore;
class morkStream;
class morkTable;
class morkTableChange;
class morkTableRowCursor;
class morkThumb;
class morkWriter;
class morkZone;
// } %%%%% end class forward defines %%%%%

// include this config file last for platform & environment specific stuff:
#ifndef _MORKCONFIG_
#include "morkConfig.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORK_ */
