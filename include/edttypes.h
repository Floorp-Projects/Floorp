/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 *  File defines external editor types.
 *
 * These types are remapped internally to the editor.
*/

#ifndef _edt_types_h_
#define _edt_types_h_

#ifndef EDITOR_TYPES
#define ED_Element void
#define ED_Buffer void
#define ED_TagCursor void
#define ED_BitArray void
#endif

#include "xp_core.h"

/* Number of "Netscape Colors" All are in 
 * Color cube 
*/
#define   MAX_NS_COLORS        70

typedef int32  ED_BufferOffset;

/*
 * Handle to Internal structure used for maintaining links.
*/
typedef struct ED_Link* ED_LinkId;
#define ED_LINK_ID_NONE     0

/* this id is passed to FE_GetImageData.. when it returns, we know to pass
 *  the call to EDT_SetImageData
*/
#define ED_IMAGE_LOAD_HACK_ID -10

typedef enum {
    ED_ELEMENT_NONE,            /* Not returned from EDT_GetCurrentElement, needed to have a "not known" value */
    ED_ELEMENT_SELECTION,       /* a selection instead of a single element */
    ED_ELEMENT_TEXT,
    ED_ELEMENT_IMAGE,
    ED_ELEMENT_HRULE,
    ED_ELEMENT_UNKNOWN_TAG,
    ED_ELEMENT_TARGET,
    ED_ELEMENT_TABLE,           /* Keep these at the end so we can use type >= ED_ELEMENT_TABLE to test for any of them */
    ED_ELEMENT_CELL,
    ED_ELEMENT_ROW,             /* May not need these. Currently returning ED_ELEMENT_CELL instead */
    ED_ELEMENT_COL
} ED_ElementType;

typedef enum {
    ED_CARET_BEFORE = 0,
    ED_CARET_AFTER  = 1
} ED_CaretObjectPosition;

#define TF_NONE         0
#define TF_BOLD         1
#define TF_ITALIC       2
#define TF_FIXED        4
#define TF_SUPER        8
#define TF_SUB          0x10
#define TF_STRIKEOUT    0x20
#define TF_BLINK        0x40
#define TF_FONT_COLOR   0x80     /* set if font has color */
#define TF_FONT_SIZE    0x100    /* set if font has size */
#define TF_HREF         0x200
#define TF_SERVER       0x400
#define TF_SCRIPT       0x800
#define TF_STYLE        0x1000
#define TF_UNDERLINE    0x2000
#define TF_FONT_FACE    0x4000
#define TF_NOBREAK      0x8000
#define TF_SPELL        0x10000
#define TF_INLINEINPUT  0x20000
#define TF_INLINEINPUTTHICK  0x40000
#define TF_INLINEINPUTDOTTED  0x80000
#define TF_FONT_WEIGHT 0x100000
#define TF_FONT_POINT_SIZE 0x200000

typedef int32 ED_TextFormat; /* Prefered type for the editor text format. */
typedef ED_TextFormat ED_ETextFormat; /* Alias for old code. Remove when possible. */

/* Similar to ED_TextFormat. Used when multiple cells are represented in EDT_TableCellData */

#define CF_NONE         0
#define CF_ALIGN        1
#define CF_VALIGN       2
#define CF_COLSPAN      4     
#define CF_ROWSPAN      8    
#define CF_HEADER       0x10 
#define CF_NOWRAP       0x20 
#define CF_WIDTH        0x40 
#define CF_HEIGHT       0x80 
#define CF_BACK_COLOR   0x100
#define CF_BACK_IMAGE   0x200
#define CF_BACK_NOSAVE  0x400
#define CF_EXTRA_HTML   0x800

typedef int32 ED_CellFormat; /* Prefered type for the editor cell format. */

/*
 * The names here are confusing, and have a historical basis that is
 * lost in the mists of time. The trouble is that the "ED_ALIGN_CENTER"
 * tag is really "abscenter", while the ED_ALIGN_ABSCENTER tag is
 * really "center". (and the same for the TOP and BOTTOM tags.)
 *
 * Someday, if we have a lot of spare time we could switch the names. 
 */

/* CLM: Swapped bottom and center tags -- should match latest extensions now?
 *      Note: BASELINE is not written out (this is default display mode)
*/
typedef enum {
    ED_ALIGN_DEFAULT        = -1,
    ED_ALIGN_CENTER         = 0,    /* abscenter    */
    ED_ALIGN_LEFT           = 1,    /* left         */
    ED_ALIGN_RIGHT          = 2,    /* right        */
    ED_ALIGN_TOP            = 3,    /* texttop      */
    ED_ALIGN_BOTTOM         = 4,    /* absbottom    */
    ED_ALIGN_BASELINE       = 5,    /* baseline     */
    ED_ALIGN_ABSCENTER      = 6,    /* center       */
    ED_ALIGN_ABSBOTTOM      = 7,    /* bottom       */
    ED_ALIGN_ABSTOP         = 8     /* top          */
} ED_Alignment;


/*------------------- TABLE SIZING AND SELECTION --------------------*/

/*  SizeStyle defines */
#define  ED_SIZE_NONE           0
#define  ED_SIZE_TOP            0x0001
#define  ED_SIZE_RIGHT          0x0002
#define  ED_SIZE_BOTTOM         0x0004
#define  ED_SIZE_LEFT           0x0008
#define  ED_SIZE_ADD_ROWS       0x0010
#define  ED_SIZE_ADD_COLS       0x0020

/*  Hotspot at corners */
#define  ED_SIZE_TOP_RIGHT      (ED_SIZE_TOP | ED_SIZE_RIGHT)
#define  ED_SIZE_BOTTOM_RIGHT   (ED_SIZE_BOTTOM | ED_SIZE_RIGHT)
#define  ED_SIZE_TOP_LEFT       (ED_SIZE_TOP | ED_SIZE_LEFT)
#define  ED_SIZE_BOTTOM_LEFT    (ED_SIZE_BOTTOM | ED_SIZE_LEFT)

typedef intn ED_SizeStyle;

typedef enum {                /* Return value for EDT_GetTableHitRegion) */
    ED_HIT_NONE,
    ED_HIT_SEL_TABLE,         /* Upper left corner */
    ED_HIT_SEL_COL,           /* Near top table border */
    ED_HIT_SEL_ROW,           /* Near left table border */
    ED_HIT_SEL_CELL,          /* Near top cell border */
    ED_HIT_SEL_ALL_CELLS,     /* Upper left corner when Ctrl is pressed */
    ED_HIT_SIZE_TABLE_WIDTH,  /* Near right table border */
    ED_HIT_SIZE_TABLE_HEIGHT, /* Near bottom table border */
    ED_HIT_SIZE_COL,          /* Near right border of a cell and between columns */
    ED_HIT_SIZE_ROW,          /* Near bottom border of a cell and between columns */
    ED_HIT_ADD_ROWS,          /* Lower left corner */
    ED_HIT_ADD_COLS,          /* Lower right corner */
    ED_HIT_DRAG_TABLE,        /* Near bottom border and between rows when table or cell is already selected */
    ED_HIT_CHANGE_COLSPAN,    /* Near Right border of cell having COLSPAN (Not used yet) */
    ED_HIT_CHANGE_ROWSPAN     /* Bottom edge of cell having ROWSPAN (Not used yet) */
} ED_HitType;

typedef enum {             /* Return types for EDT_GetTableDragDropRegion */
    ED_DROP_NONE,               /* Don't allow drop - when pasting wouldn't change anything */
    ED_DROP_NORMAL,             /* No special table behavior - do the same as any HTML drop */
    ED_DROP_INSERT_BEFORE,      /* Between columns - near left border of cell when source = column */
    ED_DROP_INSERT_AFTER,       /* Between columns - near right border of cell when source = column */
    ED_DROP_INSERT_ABOVE,       /* Between rows - near top border of cell when source = row */
    ED_DROP_INSERT_BELOW,       /* Between rows - near bottom border of cell when source = row */
    ED_DROP_REPLACE_CELL,       /* Inside cell - when we want to replace cell contents */
    ED_DROP_APPEND_CONTENTS     /* Inside cell - when we append to existing contents */
} ED_DropType;

struct _EDT_DragTableData {
    ED_HitType  iSourceType;         /* One of the ED_HIT_SEL_... enums */
    ED_DropType iDropType;           /* One of above enum values */
    LO_Element  *pFirstSelectedCell; /* First cell in source being dragged */
    LO_Element  *pDragOverCell;      /* Cell being dragged over */
    intn         iRows;              /* Number of rows */
    intn         iColumns;           /*  and columns in selection */
    int32        X;                  /* Location to place highlighting or make caret */
    int32        Y;                  /*  to show where to drop cells */
    int32        iWidth;
    int32        iHeight;
};
typedef struct _EDT_DragTableData EDT_DragTableData;


typedef enum {             /* Return values from EDT_GetMergeCellsType */
    ED_MERGE_NONE,
    ED_MERGE_NEXT_CELL,
    ED_MERGE_SELECTED_CELLS
} ED_MergeType;

/* Used with EDT_ChangeTableSelection to tell if we should move
 *   to next Cell, Row, or Columns along with changing the selected cells
*/
typedef enum {             
    ED_MOVE_NONE,
    ED_MOVE_PREV,
    ED_MOVE_NEXT
} ED_MoveSelType;

/*--------------------------- HREF --------------------------------*/

struct _EDT_HREFData {
    char *pURL;
    char *pExtra;
};

typedef struct _EDT_HREFData EDT_HREFData;

/*--------------------------- Image --------------------------------*/

struct _EDT_ImageData {
    XP_Bool bIsMap;        
/*    char *pUseMap;  created with XP_ALLOC()    Now in pExtra, hardts */
    ED_Alignment align;
    char *pSrc;         
    char *pLowSrc;
    char *pName;
    char *pAlt;
    int32  iWidth;
    int32  iHeight;
    XP_Bool bWidthPercent;         /* Range: 1 - 100 if TRUE, else = pixels (default) */
    XP_Bool bHeightPercent;
    int32 iHSpace;
    int32 iVSpace;
    int32 iBorder;
/* Added hardts */
    XP_Bool bNoSave;
/* Added by CLM: */
    int32  iOriginalWidth;        /* Width and Height we got on initial loading */
    int32  iOriginalHeight;
    EDT_HREFData *pHREFData;
    char *pExtra;
};

typedef struct _EDT_ImageData EDT_ImageData;

/*--------------------------- Target --------------------------------*/

struct _EDT_TargetData {
    char *pName;
    char *pExtra;
};

typedef struct _EDT_TargetData EDT_TargetData;

/*--------------------------- Character  --------------------------------*/
#define ED_FONT_POINT_SIZE_DEFAULT 0
#define ED_FONT_POINT_SIZE_MIN 1
#define ED_FONT_POINT_SIZE_MAX 1000

#define ED_FONT_WEIGHT_MIN 100
#define ED_FONT_WEIGHT_NORMAL 400
#define ED_FONT_WEIGHT_BOLD 700
#define ED_FONT_WEIGHT_MAX 900

enum {
    ED_FONT_VARIABLE,
    ED_FONT_FIXED,
    ED_FONT_LOCAL
};

struct _EDT_CharacterData {
    ED_TextFormat mask;             /* bits to set or get */
    ED_TextFormat values;           /* values of the bits in the mask */
    LO_Color *pColor;               /* color if mask bit is set */
    int32 iSize;                    /* size if mask bit is set */
    EDT_HREFData *pHREFData;        /* href if mask bit is set */
    ED_LinkId linkId;               /* internal use only */
    char* pFontFace;                /* FontFace name */
    int16  iWeight;                 /* font weight range = 100-900, 400=Normal, 700=Bold*/
    int16  iPointSize;              /* not sure what default is! Use 0 to mean "default" */
};

typedef struct _EDT_CharacterData EDT_CharacterData;

/*--------------------------- Horizonal Rule --------------------------------*/

struct _EDT_HorizRuleData {
    ED_Alignment align;         /* only allows left and right alignment */
    int32  size;                  /* value 1 to 100 indicates line thickness */     
    int32  iWidth;                /* CM: default = 100% */
    XP_Bool bWidthPercent;         /* Range: 1 - 100 if TRUE(default), else = pixels */
    XP_Bool bNoShade;              
    char *pExtra;
};

typedef struct _EDT_HorizRuleData EDT_HorizRuleData;

/*--------------------------- ContainerData --------------------------------*/

struct _EDT_ContainerData {
    ED_Alignment align;         /* only allows left and right alignment */
    char *pExtra;
};

typedef struct _EDT_ContainerData EDT_ContainerData;

/*--------------------------- TableData --------------------------------*/

struct _EDT_TableData {
    ED_Alignment align; /* ED_ALIGN_LEFT, ED_ALIGN_ABSCENTER, ED_ALIGN_RIGHT */
    ED_Alignment malign; /* margin alignment: ED_ALIGN_DEFAULT, ED_ALIGN_LEFT, ED_ALIGN_RIGHT */
    XP_Bool bUseCols; /* TRUE means COLS= will be output, which speeds up layout in 4.0 */
    int32 iRows;
    int32 iColumns;
    XP_Bool bBorderWidthDefined;
    int32 iBorderWidth;
    int32 iCellSpacing;
    int32 iCellPadding;
    int32 iInterCellSpace;
    XP_Bool bWidthDefined;
    XP_Bool bWidthPercent;
    int32 iWidth;
    int32 iWidthPixels;
    XP_Bool bHeightDefined;
    XP_Bool bHeightPercent;
    int32 iHeight;
    int32 iHeightPixels;
    LO_Color *pColorBackground;     /* null in the default case */
    char *pBackgroundImage; /* null in the default case */
    XP_Bool bBackgroundNoSave;
    char *pExtra;
};

typedef struct _EDT_TableData EDT_TableData;

/*--------------------------- TableCaptionData --------------------------------*/

struct _EDT_TableCaptionData {
    ED_Alignment align;
    char *pExtra;
};

typedef struct _EDT_TableCaptionData EDT_TableCaptionData;

/*--------------------------- TableRowData --------------------------------*/

struct _EDT_TableRowData {
    ED_Alignment align;
    ED_Alignment valign;
    LO_Color *pColorBackground;     /* null in the default case */
    char *pBackgroundImage; /* null in the default case */
    XP_Bool bBackgroundNoSave;
    char *pExtra;
};

typedef struct _EDT_TableRowData EDT_TableRowData;

/*--------------------------- TableCellData --------------------------------*/

struct _EDT_TableCellData {
    ED_CellFormat mask;             /* bits to tell us what we know for all cells */
    ED_HitType iSelectionType;      /* Either: ED_HIT_SEL_CELL, ED_HIT_SEL_COL, ED_HIT_SEL_ROW, or ED_HIT_NONE */
    intn iSelectedCount;            /* Number of cells selected. Usually >= 1 */
    ED_Alignment align;
    ED_Alignment valign;
    int32 iColSpan;
    int32 iRowSpan;
    XP_Bool bHeader; /* TRUE == th, FALSE == td */
    XP_Bool bNoWrap;
    int32 X;
    int32 Y;
    intn  iRow;
    XP_Bool bWidthDefined;
    XP_Bool bWidthPercent;
    int32 iWidth;
    int32 iWidthPixels;
    XP_Bool bHeightDefined;
    XP_Bool bHeightPercent;
    int32 iHeight;
    int32 iHeightPixels;
    LO_Color *pColorBackground;     /* null in the default case */
    char *pBackgroundImage; /* null in the default case */
    XP_Bool bBackgroundNoSave;
    char *pExtra;
};

typedef struct _EDT_TableCellData EDT_TableCellData;

/*--------------------------- LayerData --------------------------------*/

struct _EDT_LayerData {
    char *pExtra;
};

typedef struct _EDT_LayerData EDT_LayerData;

/*--------------------------- DivisionData --------------------------------*/

struct _EDT_DivisionData {
    ED_Alignment align;
    char *pExtra;
};

typedef struct _EDT_DivisionData EDT_DivisionData;

/*--------------------------- Page Properties --------------------------------*/
struct _EDT_MetaData {
    XP_Bool bHttpEquiv;        /* true, http-equiv="fdsfds", false name="fdsfds" */
    char *pName;            /* http-equiv's or name's value */
    char *pContent;
};

typedef struct _EDT_MetaData EDT_MetaData;

struct _EDT_PageData {
    LO_Color *pColorBackground;     /* null in the default case */
    LO_Color *pColorLink;
    LO_Color *pColorText;
    LO_Color *pColorFollowedLink;
    LO_Color *pColorActiveLink;
    char *pBackgroundImage;
    XP_Bool bBackgroundNoSave;
    char *pFontDefURL; /* For Web Fonts. */
    XP_Bool bFontDefNoSave;
    char *pTitle;
    XP_Bool bKeepImagesWithDoc;
};

typedef struct _EDT_PageData EDT_PageData;

typedef enum {
    ED_COLOR_BACKGROUND,
    ED_COLOR_LINK,
    ED_COLOR_TEXT,
    ED_COLOR_FOLLOWED_LINK
} ED_EColor;

/*
 * CLM: Java and PlugIn data structures
*/
 struct _EDT_ParamData {
    char *pName;
    char *pValue;
};
typedef struct _EDT_ParamData EDT_ParamData;

typedef int32 EDT_ParamID;

struct _EDT_PlugInData {
    EDT_ParamID   ParamID;               /* Identifies which Param list is associated */
    char          *pSrc;
    XP_Bool          bHidden;
    ED_Alignment  align;
    int32           iWidth;
    int32           iHeight;
    XP_Bool          bWidthPercent;         /* Range: 1 - 100 if TRUE, else = pixels default) */
    XP_Bool          bHeightPercent;
    XP_Bool          bForegroundPalette;    /* PC systems only. For controling 256-color palette wars */
    int32           iHSpace;
    int32           iVSpace;
    int32           iBorder;
};
typedef struct _EDT_PlugInData EDT_PlugInData;

struct _EDT_JavaData {
    EDT_ParamID   ParamID;
    char          *pCode;
    char          *pCodebase;
    char          *pName;
    ED_Alignment  align;
    char          *pSrc;
    int32           iWidth;
    int32           iHeight;
    XP_Bool          bWidthPercent;         /* Range: 1 - 100 if TRUE, else = pixels default) */
    XP_Bool          bHeightPercent;
    int32           iHSpace;
    int32           iVSpace;
    int32           iBorder;
};
typedef struct _EDT_JavaData EDT_JavaData;

/* CLM: Error codes for file writing
 *      Return 0 if no error
 */
typedef enum {
    ED_ERROR_NONE,
    ED_ERROR_READ_ONLY,           /* File is marked read-only */
    ED_ERROR_BLOCKED,             /* Can't write at this time, edit buffer blocked */
    ED_ERROR_BAD_URL,             /* URL was not a "file:" type or no string */
    ED_ERROR_FILE_OPEN,
    ED_ERROR_FILE_WRITE,
    ED_ERROR_CREATE_BAKNAME,
    ED_ERROR_DELETE_BAKFILE,
    ED_ERROR_FILE_RENAME_TO_BAK,
    ED_ERROR_CANCEL,
    ED_ERROR_FILE_EXISTS,         /* We really didn't save -- file existed and no overwrite */
    ED_ERROR_SRC_NOT_FOUND,
    ED_ERROR_FILE_READ,

    /* The following are used internally by the editor and will not be passed to the front end. */
    ED_ERROR_PUBLISHING,          /* When netlib encounters an error http or ftp publishing. */
    ED_ERROR_TAPEFS_COMPLETION    /* The tape file system for saving encountered an error when
                                     the Complete() method was called.  E.g. an error sending a 
                                     mail message. */          
} ED_FileError;

typedef enum {
    ED_TAG_OK,
    ED_TAG_UNOPENED,
    ED_TAG_UNCLOSED,
    ED_TAG_UNTERMINATED_STRING,
    ED_TAG_PREMATURE_CLOSE,
    ED_TAG_TAGNAME_EXPECTED
} ED_TagValidateResult;

typedef enum {
    ED_LIST_TYPE_DEFAULT,
    ED_LIST_TYPE_DIGIT,         
    ED_LIST_TYPE_BIG_ROMAN,
    ED_LIST_TYPE_SMALL_ROMAN,
    ED_LIST_TYPE_BIG_LETTERS,
    ED_LIST_TYPE_SMALL_LETTERS,
    ED_LIST_TYPE_CIRCLE,
    ED_LIST_TYPE_SQUARE,
    ED_LIST_TYPE_DISC,
    ED_LIST_TYPE_CITE /* For Mail Quoting */
} ED_ListType;


struct _EDT_ListData {
    /* This should be TagType, but there are problems with the include file dependencies. */
    int8 iTagType;              /* P_UNUM_LIST, P_NUM_LIST, P_BLOCKQUOTE, */
                                /* P_DIRECTOR, P_MENU, P_DESC_LIST */
    XP_Bool bCompact;
    ED_ListType eType;
    int32 iStart;                /* automatically maps, start is one */
    char *pBaseURL;              /* If an ED_LIST_TYPE_CITE, this is the URL from the enclosing <BASE> tag. 
                                    Don't expose this to users because not actually written out in the HTML. */
    char *pExtra;
};

typedef struct _EDT_ListData EDT_ListData;

typedef enum {
    ED_BREAK_NORMAL,            /* just break the line, ignore images */
    ED_BREAK_LEFT,              /* break so it passes the image on the left */
    ED_BREAK_RIGHT,             /* break past the right image */
    ED_BREAK_BOTH               /* break past both images */
} ED_BreakType;

typedef enum {
    ED_SAVE_OVERWRITE_THIS,
    ED_SAVE_OVERWRITE_ALL,
    ED_SAVE_DONT_OVERWRITE_THIS,
    ED_SAVE_DONT_OVERWRITE_ALL,
    ED_SAVE_CANCEL
} ED_SaveOption;

/* After saving, what to do with the editor buffer. */
typedef enum {
    ED_FINISHED_GOTO_NEW,       /* Point the editor to the location of the 
                                   newly saved document. */
    ED_FINISHED_REVERT_BUFFER,  /* Revert the buffer to the state before
                                   the save operation began. */
    ED_FINISHED_SAVE_DRAFT,     /* Like ED_FINISHED_REVERT_BUFFER, except clears the dirty flag
                                   on success. */
    ED_FINISHED_MAIL_SEND      /*  If we succeed we're going to throw the buffer
                                   away, so don't revert it.  If failure, revert the buffer.
                                   Used for mail compose, we don't 
                                   want the editor to start any operation that 
                                   causes problems when libmsg destroys the editor
                                   context. */
} ED_SaveFinishedOption;


/* For FE_SaveDialogCreate */
typedef enum {
  ED_SAVE_DLG_SAVE_LOCAL,  /* "saving files to local disk" */
  ED_SAVE_DLG_PUBLISH,     /* "uploading files to remote server" */
  ED_SAVE_DLG_PREPARE_PUBLISH /* "preparing files to publish" */
} ED_SaveDialogType;

typedef int32 EDT_ClipboardResult;
#define EDT_COP_OK 0
#define EDT_COP_DOCUMENT_BUSY 1
#define EDT_COP_SELECTION_EMPTY 2
#define EDT_COP_SELECTION_CROSSES_TABLE_DATA_CELL 3
/* For the result EDT_COP_CLIPBOARD_BAD the XP code has already
 * given the user an error dialog. So FE code should not
 * give an additional error dialog. (Only occurs on
 * paste and paste-like operations.)
 */
#define EDT_COP_CLIPBOARD_BAD 4


#ifdef FIND_REPLACE

#define ED_FIND_FIND_ALL_WORDS  1       /* used to enumerate all words in a */
                                        /*  buffer */
#define ED_FIND_MATCH_CASE      2       /* default is to ignore case */
#define ED_FIND_REPLACE         4       /* call back the replace routine */
#define ED_FIND_WHOLE_BUFFER    8       /* start search from the top */
#define ED_FIND_REVERSE         0x10    /* reverse search from this point */

typedef intn ED_FindFlags;

typedef void (*EDT_PFReplaceFunc)( void *pMWContext, 
                                  char *pFoundWord, 
                                  char **pReplaceWord );

struct _EDT_FindAndReplaceData {
    char* pSearchString;
    ED_FindFlags fflags;
    EDT_PFReplaceFunc pfReplace;
};

typedef struct _EDT_FindAndReplaceData EDT_FindAndReplaceData;

#endif /* FIND_REPLACE */

/* Callback function for image encoder */

typedef int32 EDT_ImageEncoderReference;
typedef unsigned char EDT_ImageEncoderStatus;
#define ED_IMAGE_ENCODER_OK 0
#define ED_IMAGE_ENCODER_USER_CANCELED 1
#define ED_IMAGE_ENCODER_EXCEPTION 2

typedef void (*EDT_ImageEncoderCallbackFn)(EDT_ImageEncoderStatus status, void* hook);

#endif
