/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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


/**
 * Errors defines the ID's of error messages output
 * by the Debugger verisons of the Player
 */
class Errors
{
        public static const kOutOfMemoryError                 = 1000;
        public static const kNotImplementedError              = 1001;
        public static const kInvalidPrecisionError            = 1002;
        public static const kInvalidRadixError                = 1003;
        public static const kInvokeOnIncompatibleObjectError  = 1004;
        public static const kArrayIndexNotIntegerError        = 1005;
        public static const kCallOfNonFunctionError           = 1006;
        public static const kConstructOfNonFunctionError      = 1007;
        public static const kAmbiguousBindingError            = 1008;
        public static const kConvertNullToObjectError         = 1009;
        public static const kConvertUndefinedToObjectError    = 1010;
        public static const kIllegalOpcodeError               = 1011;
        public static const kLastInstExceedsCodeSizeError     = 1012;
        public static const kFindVarWithNoScopeError          = 1013;
        public static const kClassNotFoundError               = 1014;
        public static const kIllegalSetDxns                   = 1015;
        public static const kDescendentsError                 = 1016;
        public static const kScopeStackOverflowError          = 1017;
        public static const kScopeStackUnderflowError         = 1018;
        public static const kGetScopeObjectBoundsError        = 1019;
        public static const kCannotFallOffMethodError         = 1020;
        public static const kInvalidBranchTargetError         = 1021;
        public static const kIllegalVoidError                 = 1022;
        public static const kStackOverflowError               = 1023;
        public static const kStackUnderflowError              = 1024;
        public static const kInvalidRegisterError             = 1025;
        public static const kSlotExceedsCountError            = 1026;
        public static const kMethodInfoExceedsCountError      = 1027;
        public static const kDispIdExceedsCountError          = 1028;
        public static const kDispIdUndefinedError             = 1029;
        public static const kStackDepthUnbalancedError        = 1030;
        public static const kScopeDepthUnbalancedError        = 1031;
        public static const kCpoolIndexRangeError             = 1032;
        public static const kCpoolEntryWrongTypeError         = 1033;
        public static const kCheckTypeFailedError             = 1034;
        public static const kIllegalSuperCallError            = 1035;
        public static const kCannotAssignToMethodError        = 1037;
        public static const kRedefinedError                   = 1038;
        public static const kCannotVerifyUntilReferencedError = 1039;
        public static const kCantUseInstanceofOnNonObjectError = 1040;
        public static const kIsTypeMustBeClassError           = 1041;
        public static const kInvalidMagicError                = 1042;
        public static const kInvalidCodeLengthError           = 1043;
        public static const kInvalidMethodInfoFlagsError      = 1044;
        public static const kUnsupportedTraitsKindError       = 1045;
        public static const kMethodInfoOrderError             = 1046;
        public static const kMissingEntryPointError           = 1047;
        public static const kPrototypeTypeError               = 1049;
        public static const kConvertToPrimitiveError          = 1050;
        public static const kIllegalEarlyBindingError         = 1051;
        public static const kInvalidURIError                  = 1052;
        public static const kIllegalOverrideError             = 1053;
        public static const kIllegalExceptionHandlerError     = 1054;
        public static const kWriteSealedError                 = 1056;
        public static const kIllegalSlotError                 = 1057;
        public static const kIllegalOperandTypeError          = 1058;
        public static const kClassInfoOrderError              = 1059;
        public static const kClassInfoExceedsCountError       = 1060;
        public static const kNumberOutOfRangeError            = 1061;
        public static const kWrongArgumentCountError          = 1063;
        public static const kCannotCallMethodAsConstructor    = 1064;
        public static const kUndefinedVarError                = 1065;
        public static const kFunctionConstructorError         = 1066;
        public static const kIllegalNativeMethodBodyError     = 1067;
        public static const kCannotMergeTypesError            = 1068;
        public static const kReadSealedError                  = 1069;
        public static const kCallNotFoundError                = 1070;
        public static const kAlreadyBoundError                = 1071;
        public static const kZeroDispIdError                  = 1072;
        public static const kDuplicateDispIdError             = 1073;
        public static const kConstWriteError                  = 1074;
        public static const kMathNotFunctionError             = 1075;
        public static const kMathNotConstructorError          = 1076;
        public static const kWriteOnlyError                   = 1077;
        public static const kIllegalOpMultinameError          = 1078;
        public static const kIllegalNativeMethodError         = 1079;
        public static const kIllegalNamespaceError            = 1080;
        public static const kReadSealedErrorNs                = 1081;
        public static const kNoDefaultNamespaceError          = 1082;
        public static const kXMLPrefixNotBound                = 1083;
        public static const kXMLBadQName                      = 1084;
        public static const kXMLUnterminatedElementTag        = 1085;
        public static const kXMLOnlyWorksWithOneItemLists     = 1086;
        public static const kXMLAssignmentToIndexedXMLNotAllowed = 1087;
        public static const kXMLMarkupMustBeWellFormed        = 1088;
        public static const kXMLAssigmentOneItemLists         = 1089;
        public static const kXMLMalformedElement              = 1090;
        public static const kXMLUnterminatedCData             = 1091;
        public static const kXMLUnterminatedXMLDecl           = 1092;
        public static const kXMLUnterminatedDocTypeDecl       = 1093;
        public static const kXMLUnterminatedComment           = 1094;
        public static const kXMLUnterminatedAttribute         = 1095;
        public static const kXMLUnterminatedElement           = 1096;
        public static const kXMLUnterminatedProcessingInstruction = 1097;
        public static const kXMLNamespaceWithPrefixAndNoURI   = 1098;
        public static const kRegExpFlagsArgumentError         = 1100;
        public static const kNoScopeError                     = 1101;
        public static const kIllegalDefaultValue              = 1102;
        public static const kCannotExtendFinalClass           = 1103;
        public static const kXMLDuplicateAttribute            = 1104;
        public static const kCorruptABCError                  = 1107;
        public static const kInvalidBaseClassError            = 1108;
        public static const kDanglingFunctionError            = 1109;
        public static const kCannotExtendError                = 1110;
        public static const kCannotImplementError             = 1111;
        public static const kCoerceArgumentCountError         = 1112;
        public static const kInvalidNewActivationError        = 1113;
        public static const kNoGlobalScopeError               = 1114;
        public static const kNotConstructorError              = 1115;
        public static const kApplyError                       = 1116;
        public static const kXMLInvalidName                   = 1117;
        public static const kXMLIllegalCyclicalLoop           = 1118;
        public static const kDeleteTypeError                  = 1119;
        public static const kDeleteSealedError                = 1120;
        public static const kDuplicateMethodBodyError         = 1121;
        public static const kIllegalInterfaceMethodBodyError  = 1122;
        public static const kFilterError                      = 1123;
        public static const kInvalidHasNextError              = 1124;
        public static const kFileOpenError                    = 1500;
        public static const kFileWriteError                   = 1501;
        public static const kScriptTimeoutError               = 1502;
        public static const kScriptTerminatedError            = 1503;
        public static const kEndOfFileError                   = 1504;
        public static const kStringIndexOutOfBoundsError      = 1505;
        public static const kInvalidRangeError                = 1506;
        public static const kNullArgumentError                = 1507;
        public static const kInvalidArgumentError             = 1508;
        public static const kShellCompressedDataError         = 1509;
        public static const kArrayFilterNonNullObjectError    = 1510;
};
