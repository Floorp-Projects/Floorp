/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */


#ifndef __avmplus_ErrorConstants__
#define __avmplus_ErrorConstants__
	
namespace avmplus
{ 
    namespace ErrorConstants
    {
        enum LangID
        {
            LANG_en    = 0,
            LANG_de    = 1,
            LANG_es    = 2,
            LANG_fr    = 3,
            LANG_it    = 4,
            LANG_ja    = 5,
            LANG_ko    = 6,
            LANG_zh_CN = 7,
            LANG_zh_TW = 8,
            LANG_count = 9
        };

		typedef struct _LangName
		{
			const char*	str;
			LangID		id;
		}
		LangName;

        enum
        {
            kOutOfMemoryError                               = 1000,
            kNotImplementedError                            = 1001,
            kInvalidPrecisionError                          = 1002,
            kInvalidRadixError                              = 1003,
            kInvokeOnIncompatibleObjectError                = 1004,
            kArrayIndexNotIntegerError                      = 1005,
            kCallOfNonFunctionError                         = 1006,
            kConstructOfNonFunctionError                    = 1007,
            kAmbiguousBindingError                          = 1008,
            kConvertNullToObjectError                       = 1009,
            kConvertUndefinedToObjectError                  = 1010,
            kIllegalOpcodeError                             = 1011,
            kLastInstExceedsCodeSizeError                   = 1012,
            kFindVarWithNoScopeError                        = 1013,
            kClassNotFoundError                             = 1014,
            kIllegalSetDxns                                 = 1015,
            kDescendentsError                               = 1016,
            kScopeStackOverflowError                        = 1017,
            kScopeStackUnderflowError                       = 1018,
            kGetScopeObjectBoundsError                      = 1019,
            kCannotFallOffMethodError                       = 1020,
            kInvalidBranchTargetError                       = 1021,
            kIllegalVoidError                               = 1022,
            kStackOverflowError                             = 1023,
            kStackUnderflowError                            = 1024,
            kInvalidRegisterError                           = 1025,
            kSlotExceedsCountError                          = 1026,
            kMethodInfoExceedsCountError                    = 1027,
            kDispIdExceedsCountError                        = 1028,
            kDispIdUndefinedError                           = 1029,
            kStackDepthUnbalancedError                      = 1030,
            kScopeDepthUnbalancedError                      = 1031,
            kCpoolIndexRangeError                           = 1032,
            kCpoolEntryWrongTypeError                       = 1033,
            kCheckTypeFailedError                           = 1034,
            kIllegalSuperCallError                          = 1035,
            kCannotAssignToMethodError                      = 1037,
            kRedefinedError                                 = 1038,
            kCannotVerifyUntilReferencedError               = 1039,
            kCantUseInstanceofOnNonObjectError              = 1040,
            kIsTypeMustBeClassError                         = 1041,
            kInvalidMagicError                              = 1042,
            kInvalidCodeLengthError                         = 1043,
            kInvalidMethodInfoFlagsError                    = 1044,
            kUnsupportedTraitsKindError                     = 1045,
            kMethodInfoOrderError                           = 1046,
            kMissingEntryPointError                         = 1047,
            kPrototypeTypeError                             = 1049,
            kConvertToPrimitiveError                        = 1050,
            kIllegalEarlyBindingError                       = 1051,
            kInvalidURIError                                = 1052,
            kIllegalOverrideError                           = 1053,
            kIllegalExceptionHandlerError                   = 1054,
            kWriteSealedError                               = 1056,
            kIllegalSlotError                               = 1057,
            kIllegalOperandTypeError                        = 1058,
            kClassInfoOrderError                            = 1059,
            kClassInfoExceedsCountError                     = 1060,
            kNumberOutOfRangeError                          = 1061,
            kWrongArgumentCountError                        = 1063,
            kCannotCallMethodAsConstructor                  = 1064,
            kUndefinedVarError                              = 1065,
            kFunctionConstructorError                       = 1066,
            kIllegalNativeMethodBodyError                   = 1067,
            kCannotMergeTypesError                          = 1068,
            kReadSealedError                                = 1069,
            kCallNotFoundError                              = 1070,
            kAlreadyBoundError                              = 1071,
            kZeroDispIdError                                = 1072,
            kDuplicateDispIdError                           = 1073,
            kConstWriteError                                = 1074,
            kMathNotFunctionError                           = 1075,
            kMathNotConstructorError                        = 1076,
            kWriteOnlyError                                 = 1077,
            kIllegalOpMultinameError                        = 1078,
            kIllegalNativeMethodError                       = 1079,
            kIllegalNamespaceError                          = 1080,
            kReadSealedErrorNs                              = 1081,
            kNoDefaultNamespaceError                        = 1082,
            kXMLPrefixNotBound                              = 1083,
            kXMLBadQName                                    = 1084,
            kXMLUnterminatedElementTag                      = 1085,
            kXMLOnlyWorksWithOneItemLists                   = 1086,
            kXMLAssignmentToIndexedXMLNotAllowed            = 1087,
            kXMLMarkupMustBeWellFormed                      = 1088,
            kXMLAssigmentOneItemLists                       = 1089,
            kXMLMalformedElement                            = 1090,
            kXMLUnterminatedCData                           = 1091,
            kXMLUnterminatedXMLDecl                         = 1092,
            kXMLUnterminatedDocTypeDecl                     = 1093,
            kXMLUnterminatedComment                         = 1094,
            kXMLUnterminatedAttribute                       = 1095,
            kXMLUnterminatedElement                         = 1096,
            kXMLUnterminatedProcessingInstruction           = 1097,
            kXMLNamespaceWithPrefixAndNoURI                 = 1098,
            kRegExpFlagsArgumentError                       = 1100,
            kNoScopeError                                   = 1101,
            kIllegalDefaultValue                            = 1102,
            kCannotExtendFinalClass                         = 1103,
            kXMLDuplicateAttribute                          = 1104,
            kCorruptABCError                                = 1107,
            kInvalidBaseClassError                          = 1108,
            kDanglingFunctionError                          = 1109,
            kCannotExtendError                              = 1110,
            kCannotImplementError                           = 1111,
            kCoerceArgumentCountError                       = 1112,
            kInvalidNewActivationError                      = 1113,
            kNoGlobalScopeError                             = 1114,
            kNotConstructorError                            = 1115,
            kApplyError                                     = 1116,
            kXMLInvalidName                                 = 1117,
            kXMLIllegalCyclicalLoop                         = 1118,
            kDeleteTypeError                                = 1119,
            kDeleteSealedError                              = 1120,
            kDuplicateMethodBodyError                       = 1121,
            kIllegalInterfaceMethodBodyError                = 1122,
            kFilterError                                    = 1123,
            kInvalidHasNextError                            = 1124,
            kFileOpenError                                  = 1500,
            kFileWriteError                                 = 1501,
            kScriptTimeoutError                             = 1502,
            kScriptTerminatedError                          = 1503,
            kEndOfFileError                                 = 1504,
            kStringIndexOutOfBoundsError                    = 1505,
            kInvalidRangeError                              = 1506,
            kNullArgumentError                              = 1507,
            kInvalidArgumentError                           = 1508,
            kShellCompressedDataError                       = 1509,
            kArrayFilterNonNullObjectError                  = 1510
        };

        // Error message strings only in DEBUGGER builds.
        #ifdef DEBUGGER
		const int kLanguages = LANG_count;
        const int kNumErrorConstants = 129;
        extern const char* errorConstants[kLanguages][kNumErrorConstants];
        extern int errorMappingTable[2*kNumErrorConstants];
		extern LangName languageNames[kLanguages];
        #endif
    }
}

#endif /*__avmplus_ErrorConstants__*/
