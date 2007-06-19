/* Automatically generated.  Do not edit */
/* See the mkopcodeh.awk script for details */
#define OP_ContextPop                           1
#define OP_IntegrityCk                          2
#define OP_DropTrigger                          3
#define OP_DropIndex                            4
#define OP_IdxInsert                            5
#define OP_Delete                               6
#define OP_MoveGt                               7
#define OP_VerifyCookie                         8
#define OP_Push                                 9
#define OP_Dup                                 10
#define OP_Blob                                11
#define OP_FifoWrite                           12
#define OP_IdxGT                               13
#define OP_RowKey                              14
#define OP_IsUnique                            15
#define OP_SetNumColumns                       17
#define OP_Eq                                  67   /* same as TK_EQ       */
#define OP_Expire                              18
#define OP_IdxIsNull                           19
#define OP_NullRow                             20
#define OP_OpenPseudo                          21
#define OP_OpenWrite                           22
#define OP_OpenRead                            23
#define OP_Transaction                         24
#define OP_AutoCommit                          25
#define OP_Negative                            83   /* same as TK_UMINUS   */
#define OP_Pop                                 26
#define OP_Halt                                27
#define OP_Vacuum                              28
#define OP_IfMemNeg                            29
#define OP_RowData                             30
#define OP_NotExists                           31
#define OP_MoveLe                              32
#define OP_OpenVirtual                         33
#define OP_SetCookie                           34
#define OP_Variable                            35
#define OP_TableLock                           36
#define OP_MemMove                             37
#define OP_LoadAnalysis                        38
#define OP_IdxDelete                           39
#define OP_Sort                                40
#define OP_ResetCount                          41
#define OP_NotNull                             65   /* same as TK_NOTNULL  */
#define OP_Ge                                  71   /* same as TK_GE       */
#define OP_Remainder                           81   /* same as TK_REM      */
#define OP_Divide                              80   /* same as TK_SLASH    */
#define OP_Integer                             42
#define OP_AggStep                             43
#define OP_CreateIndex                         44
#define OP_NewRowid                            45
#define OP_MoveLt                              46
#define OP_Explain                             47
#define OP_And                                 60   /* same as TK_AND      */
#define OP_ShiftLeft                           75   /* same as TK_LSHIFT   */
#define OP_Real                               124   /* same as TK_FLOAT    */
#define OP_Return                              48
#define OP_MemLoad                             49
#define OP_IdxLT                               50
#define OP_Rewind                              51
#define OP_MakeIdxRec                          52
#define OP_Gt                                  68   /* same as TK_GT       */
#define OP_AddImm                              53
#define OP_Subtract                            78   /* same as TK_MINUS    */
#define OP_Null                                54
#define OP_MemNull                             55
#define OP_MemIncr                             56
#define OP_Clear                               57
#define OP_IsNull                              64   /* same as TK_ISNULL   */
#define OP_If                                  58
#define OP_ToBlob                             138   /* same as TK_TO_BLOB  */
#define OP_RealAffinity                        61
#define OP_Callback                            62
#define OP_AggFinal                            63
#define OP_IfMemZero                           72
#define OP_Last                                84
#define OP_Rowid                               87
#define OP_Sequence                            88
#define OP_NotFound                            89
#define OP_MakeRecord                          90
#define OP_ToText                             137   /* same as TK_TO_TEXT  */
#define OP_BitAnd                              73   /* same as TK_BITAND   */
#define OP_Add                                 77   /* same as TK_PLUS     */
#define OP_HexBlob                            125   /* same as TK_BLOB     */
#define OP_String                              91
#define OP_Goto                                92
#define OP_MemInt                              93
#define OP_IfMemPos                            94
#define OP_DropTable                           95
#define OP_IdxRowid                            96
#define OP_Insert                              97
#define OP_Column                              98
#define OP_Noop                                99
#define OP_Not                                 16   /* same as TK_NOT      */
#define OP_Le                                  69   /* same as TK_LE       */
#define OP_BitOr                               74   /* same as TK_BITOR    */
#define OP_Multiply                            79   /* same as TK_STAR     */
#define OP_String8                             86   /* same as TK_STRING   */
#define OP_CreateTable                        100
#define OP_Found                              101
#define OP_Distinct                           102
#define OP_Close                              103
#define OP_Statement                          104
#define OP_IfNot                              105
#define OP_ToInt                              140   /* same as TK_TO_INT   */
#define OP_Pull                               106
#define OP_MemMax                             107
#define OP_MemStore                           108
#define OP_Next                               109
#define OP_Prev                               110
#define OP_MoveGe                             111
#define OP_Lt                                  70   /* same as TK_LT       */
#define OP_Ne                                  66   /* same as TK_NE       */
#define OP_MustBeInt                          112
#define OP_ForceInt                           113
#define OP_ShiftRight                          76   /* same as TK_RSHIFT   */
#define OP_CollSeq                            114
#define OP_Gosub                              115
#define OP_ContextPush                        116
#define OP_FifoRead                           117
#define OP_ParseSchema                        118
#define OP_Destroy                            119
#define OP_IdxGE                              120
#define OP_ReadCookie                         121
#define OP_BitNot                              85   /* same as TK_BITNOT   */
#define OP_AbsValue                           122
#define OP_Or                                  59   /* same as TK_OR       */
#define OP_ToReal                             141   /* same as TK_TO_REAL  */
#define OP_ToNumeric                          139   /* same as TK_TO_NUMERIC*/
#define OP_Function                           123
#define OP_Concat                              82   /* same as TK_CONCAT   */
#define OP_Int64                              126

/* The following opcode values are never used */
#define OP_NotUsed_127                        127
#define OP_NotUsed_128                        128
#define OP_NotUsed_129                        129
#define OP_NotUsed_130                        130
#define OP_NotUsed_131                        131
#define OP_NotUsed_132                        132
#define OP_NotUsed_133                        133
#define OP_NotUsed_134                        134
#define OP_NotUsed_135                        135
#define OP_NotUsed_136                        136

/* Opcodes that are guaranteed to never push a value onto the stack
** contain a 1 their corresponding position of the following mask
** set.  See the opcodeNoPush() function in vdbeaux.c  */
#define NOPUSH_MASK_0 0xb3fa
#define NOPUSH_MASK_1 0xbfff
#define NOPUSH_MASK_2 0x4bd7
#define NOPUSH_MASK_3 0xff2d
#define NOPUSH_MASK_4 0xffff
#define NOPUSH_MASK_5 0xd23b
#define NOPUSH_MASK_6 0xffea
#define NOPUSH_MASK_7 0x015f
#define NOPUSH_MASK_8 0x3e00
#define NOPUSH_MASK_9 0x0000
