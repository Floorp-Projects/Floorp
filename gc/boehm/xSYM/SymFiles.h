/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#include <MacTypes.h>

#ifndef __SYM_FILES__
#define __SYM_FILES__

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif

/* Note:  these structures are for .xSYM file format v3.3. */

struct DiskTableInfo {
	UInt16			dti_first_page;				/* table's first page */
	UInt16			dti_page_count;				/* # pages in the table */
	UInt32			dti_object_count;			/* # objects in the table */
};
typedef struct DiskTableInfo DiskTableInfo;

struct DiskSymbolHeaderBlock {
	Str31			dshb_id; 					/* Version information */
	UInt16			dshb_page_size;				/* Size of the pages/blocks */
	UInt16			dshb_hash_page;				/* Disk page for the hash table */
	UInt16			dshb_root_mte;				/* MTE index of the program root */
	UInt32			dshb_mod_date;				/* executable's mod date */

	DiskTableInfo	dshb_frte;					/* Per TABLE information */
	DiskTableInfo	dshb_rte;
	DiskTableInfo	dshb_mte;
	DiskTableInfo	dshb_cmte;
	DiskTableInfo	dshb_cvte;
	DiskTableInfo	dshb_csnte;
	DiskTableInfo	dshb_clte;
	DiskTableInfo	dshb_ctte;
	DiskTableInfo	dshb_tte;
	DiskTableInfo	dshb_nte;
	DiskTableInfo	dshb_tinfo;
	DiskTableInfo	dshb_fite;
	DiskTableInfo	dshb_const;

	OSType			dshb_file_creator;			/* executable's creator */
	OSType			dshb_file_type;				/* executable's file type */
};
typedef struct DiskSymbolHeaderBlock DiskSymbolHeaderBlock;

/*
RESOURCE_TABLE_ENTRY = RECORD
	rte_ResType: ResType; { resource type }
	rte_res_number: INTEGER; { resource id }
	rte_nte_index: LONGINT; { the resource's name }
	rte_mte_first: INTEGER; { first MTE contained in the resource }
	rte_mte_last: INTEGER; { last MTE contained in the resource }
	rte_res_size: LONGINT; { the size of the resource }
END;
 */

struct ResourceTableEntry {
	ResType			rte_ResType;				/* resource type */
	SInt16			rte_res_number;				/* resource id */
	UInt32			rte_nte_index;				/* resource name */
	UInt16			rte_mte_first;				/* first MTE contained in the resource */
	UInt16			rte_mte_last;				/* last MTE contained in the resource */
	UInt32			rte_res_size;				/* the size of the resource */
};
typedef struct ResourceTableEntry ResourceTableEntry;

/*
FILE_REFERENCE = RECORD
	fref_frte_index: LONGINT; { File Reference Table index }
	fref_offset: LONGINT; { absolute offset into the source file }
END;
*/

struct FileReference {
	UInt16			fref_frte_index;			/* File Reference Table index */
	UInt32			fref_offset;				/* absolute offset into the source file */
};
typedef struct FileReference FileReference;

/*
MODULES_TABLE_ENTRY = RECORD
	mte_rte_index: UNSIGNED; { which resource the MTE is in }
	mte_res_offset: LONGINT; { offset into the resource }
	mte_size: LONGINT; { size of the MTE }
	mte_kind: SignedByte; { kind of MTE }
	mte_scope: SignedByte; { MTE's visibility }
	mte_parent: INTEGER; { index of parent MTE }
	mte_imp_fref: FILE_REFERENCE; { implementation source location }
	mte_imp_end: LONGINT; { end of implementation }
	mte_nte_index: LONGINT; { MTE's name }
	mte_cmte_index: INTEGER; { MTEs contained by this MTE }
	mte_cvte_index: LONGINT; { variables contained by this MTE}
	mte_clte_index: INTEGER; { labels contained by this MTE }
	mte_ctte_index: INTEGER; { types contained by this MTE }
	mte_csnte_idx_1: LONGINT; { CSNTE index of first statement }
	mte_csnte_idx_2: LONGINT; { CSNTE index of last statement }
END;
 */

struct ModulesTableEntry {
	UInt16 			mte_rte_index; 				/* which resource the MTE is in */
	UInt32			mte_res_offset;				/* offset into the resource */
	UInt32			mte_size;					/* size of the MTE */
	SInt8			mte_kind;					/* kind of MTE */
	SInt8			mte_scope;					/* MTE's visibility */
	UInt16			mte_parent;					/* index of parent MTE */
	FileReference	mte_imp_fref;				/* implementation source location */
	UInt32			mte_imp_end;				/* end of implementation */
	UInt32			mte_nte_index;				/* MTE's name */
	UInt16			mte_cmte_index;				/* MTEs contained by this MTE */
	UInt32			mte_cvte_index;				/* variables contained by this MTE */
	UInt16			mte_clte_index;				/* labels contained by this MTE */
	UInt16			mte_ctte_index;				/* types contained by this MTE */
	UInt32			mte_csnte_idx_1;			/* CSNTE index of first statement */
	UInt32			mte_csnte_idx_2;			/* CSNTE index of last statement */
};
typedef struct ModulesTableEntry ModulesTableEntry;

/*
 * The type of module.  Taken from the OMF document
 */
enum {
	kModuleKindNone					= 0,
	kModuleKindProgram				= 1,
	kModuleKindUnit					= 2,
	kModuleKindProcedure			= 3,
	kModuleKindFunction				= 4,
	kModuleKindData					= 5,
	kModuleKindBlock				= 6				/* The module is an internal block */
};

/*
FILE_REFERENCE_TABLE_ENTRY = RECORD
	CASE INTEGER OF
	FILE_NAME_INDEX:
	(
		frte_name_entry: INTEGER; { = FILE_NAME_INDEX }
		frte_nte_index: LONGINT; { name of the source file }
		frte_mod_date: LONGINT; { the source file's mod date }
	);
	0: {¥ FILE_NAME_INDEX and ¥ END_OF_LIST, a FRTE entry: }
	(
		frte_mte_index: INTEGER; { the MTE's index }
		frte_file_offset: LONGINT; { the MTE's source file offset}
	);
	END_OF_LIST:
	(
		frte_end_of_list: INTEGER; { = END_OF_LIST }
	);
END;
*/

enum {
	kEndOfList = 0xFFFF,
	kFileNameIndex = kEndOfList - 1
};

union FileReferenceTableEntry {
	struct {
		UInt16			name_entry;				/* = kFileNameIndex	*/
		UInt32			nte_index;				/* Name table entry		*/
		UInt32			mod_date;				/* When file was last modified */
	} frte_fn;

	struct {
		UInt16			mte_index;				/* Module table entry < kFileNameIndex */
		UInt32			file_offset;			/* Absolute offset into the source file */
	} frte_mte;

	UInt16				frte_end_of_list;		/* = kEndOfList */
};
typedef union FileReferenceTableEntry FileReferenceTableEntry;

/*
CONTAINED_STATEMENTS_TABLE_ENTRY = RECORD
	CASE INTEGER OF
	SOURCE_FILE_CHANGE:
	(
		csnte_file_change: INTEGER; { = SOURCE_FILE_CHANGE }
		csnte_fref: FILE_REFERENCE; { the new source file }
	);
	0: {¥ to SOURCE_FILE_CHANGE or END_OF_LIST, a statement entry:}
	(
		csnte_mte_index: INTEGER; { the MTE the statement is in }
		csnte_file_delta: INTEGER; { delta from previous src loc }
		csnte_mte_offset: LONGINT; { code location, offset into MTE }
	);
	END_OF_LIST:
	(
		csnte_end_of_list: INTEGER; { indicates end of stmt list }
	);
END;
 */

enum {
	kSourceFileChange = kEndOfList - 1
};

union ContainedStatementsTableEntry {
	struct {
		UInt16			change;					/* = kSourceFileChange	*/
		FileReference	fref;					/* the new source file */
	} csnte_file;

	struct {
		UInt16			mte_index;				/* the MTE the statement is in */
		UInt16			file_delta;				/* delta from previous src loc */
		UInt32			mte_offset;				/* code location, offset into MTE */
	} csnte;

	UInt16		csnte_end_of_list;				/* = END_OF_LIST */
};
typedef union ContainedStatementsTableEntry ContainedStatementsTableEntry;

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
	#pragma pack()
#endif

#endif /* __SYM_FILES__ */
