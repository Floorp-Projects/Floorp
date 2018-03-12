(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory();
	else if(typeof define === 'function' && define.amd)
		define([], factory);
	else {
		var a = factory();
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(typeof self !== 'undefined' ? self : this, function() {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, {
/******/ 				configurable: false,
/******/ 				enumerable: true,
/******/ 				get: getter
/******/ 			});
/******/ 		}
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "/assets/build";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 662);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */,
/* 1 */,
/* 2 */,
/* 3 */,
/* 4 */,
/* 5 */,
/* 6 */,
/* 7 */,
/* 8 */,
/* 9 */,
/* 10 */,
/* 11 */,
/* 12 */
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId
} = __webpack_require__(149);

const { workerUtils: { WorkerDispatcher } } = __webpack_require__(27);

const dispatcher = new WorkerDispatcher();

const getOriginalURLs = dispatcher.task("getOriginalURLs");
const getGeneratedLocation = dispatcher.task("getGeneratedLocation");
const getOriginalLocation = dispatcher.task("getOriginalLocation");
const getLocationScopes = dispatcher.task("getLocationScopes");
const getOriginalSourceText = dispatcher.task("getOriginalSourceText");
const applySourceMap = dispatcher.task("applySourceMap");
const clearSourceMaps = dispatcher.task("clearSourceMaps");
const hasMappedSource = dispatcher.task("hasMappedSource");

module.exports = {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
  hasMappedSource,
  getOriginalURLs,
  getGeneratedLocation,
  getOriginalLocation,
  getLocationScopes,
  getOriginalSourceText,
  applySourceMap,
  clearSourceMaps,
  startSourceMapWorker: dispatcher.start.bind(dispatcher),
  stopSourceMapWorker: dispatcher.stop.bind(dispatcher)
};

/***/ }),
/* 13 */,
/* 14 */,
/* 15 */,
/* 16 */
/***/ (function(module, exports) {

/**
 * Checks if `value` is classified as an `Array` object.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is an array, else `false`.
 * @example
 *
 * _.isArray([1, 2, 3]);
 * // => true
 *
 * _.isArray(document.body.children);
 * // => false
 *
 * _.isArray('abc');
 * // => false
 *
 * _.isArray(_.noop);
 * // => false
 */
var isArray = Array.isArray;

module.exports = isArray;


/***/ }),
/* 17 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.isArrayExpression = isArrayExpression;
exports.isAssignmentExpression = isAssignmentExpression;
exports.isBinaryExpression = isBinaryExpression;
exports.isDirective = isDirective;
exports.isDirectiveLiteral = isDirectiveLiteral;
exports.isBlockStatement = isBlockStatement;
exports.isBreakStatement = isBreakStatement;
exports.isCallExpression = isCallExpression;
exports.isCatchClause = isCatchClause;
exports.isConditionalExpression = isConditionalExpression;
exports.isContinueStatement = isContinueStatement;
exports.isDebuggerStatement = isDebuggerStatement;
exports.isDoWhileStatement = isDoWhileStatement;
exports.isEmptyStatement = isEmptyStatement;
exports.isExpressionStatement = isExpressionStatement;
exports.isFile = isFile;
exports.isForInStatement = isForInStatement;
exports.isForStatement = isForStatement;
exports.isFunctionDeclaration = isFunctionDeclaration;
exports.isFunctionExpression = isFunctionExpression;
exports.isIdentifier = isIdentifier;
exports.isIfStatement = isIfStatement;
exports.isLabeledStatement = isLabeledStatement;
exports.isStringLiteral = isStringLiteral;
exports.isNumericLiteral = isNumericLiteral;
exports.isNullLiteral = isNullLiteral;
exports.isBooleanLiteral = isBooleanLiteral;
exports.isRegExpLiteral = isRegExpLiteral;
exports.isLogicalExpression = isLogicalExpression;
exports.isMemberExpression = isMemberExpression;
exports.isNewExpression = isNewExpression;
exports.isProgram = isProgram;
exports.isObjectExpression = isObjectExpression;
exports.isObjectMethod = isObjectMethod;
exports.isObjectProperty = isObjectProperty;
exports.isRestElement = isRestElement;
exports.isReturnStatement = isReturnStatement;
exports.isSequenceExpression = isSequenceExpression;
exports.isSwitchCase = isSwitchCase;
exports.isSwitchStatement = isSwitchStatement;
exports.isThisExpression = isThisExpression;
exports.isThrowStatement = isThrowStatement;
exports.isTryStatement = isTryStatement;
exports.isUnaryExpression = isUnaryExpression;
exports.isUpdateExpression = isUpdateExpression;
exports.isVariableDeclaration = isVariableDeclaration;
exports.isVariableDeclarator = isVariableDeclarator;
exports.isWhileStatement = isWhileStatement;
exports.isWithStatement = isWithStatement;
exports.isAssignmentPattern = isAssignmentPattern;
exports.isArrayPattern = isArrayPattern;
exports.isArrowFunctionExpression = isArrowFunctionExpression;
exports.isClassBody = isClassBody;
exports.isClassDeclaration = isClassDeclaration;
exports.isClassExpression = isClassExpression;
exports.isExportAllDeclaration = isExportAllDeclaration;
exports.isExportDefaultDeclaration = isExportDefaultDeclaration;
exports.isExportNamedDeclaration = isExportNamedDeclaration;
exports.isExportSpecifier = isExportSpecifier;
exports.isForOfStatement = isForOfStatement;
exports.isImportDeclaration = isImportDeclaration;
exports.isImportDefaultSpecifier = isImportDefaultSpecifier;
exports.isImportNamespaceSpecifier = isImportNamespaceSpecifier;
exports.isImportSpecifier = isImportSpecifier;
exports.isMetaProperty = isMetaProperty;
exports.isClassMethod = isClassMethod;
exports.isObjectPattern = isObjectPattern;
exports.isSpreadElement = isSpreadElement;
exports.isSuper = isSuper;
exports.isTaggedTemplateExpression = isTaggedTemplateExpression;
exports.isTemplateElement = isTemplateElement;
exports.isTemplateLiteral = isTemplateLiteral;
exports.isYieldExpression = isYieldExpression;
exports.isAnyTypeAnnotation = isAnyTypeAnnotation;
exports.isArrayTypeAnnotation = isArrayTypeAnnotation;
exports.isBooleanTypeAnnotation = isBooleanTypeAnnotation;
exports.isBooleanLiteralTypeAnnotation = isBooleanLiteralTypeAnnotation;
exports.isNullLiteralTypeAnnotation = isNullLiteralTypeAnnotation;
exports.isClassImplements = isClassImplements;
exports.isDeclareClass = isDeclareClass;
exports.isDeclareFunction = isDeclareFunction;
exports.isDeclareInterface = isDeclareInterface;
exports.isDeclareModule = isDeclareModule;
exports.isDeclareModuleExports = isDeclareModuleExports;
exports.isDeclareTypeAlias = isDeclareTypeAlias;
exports.isDeclareOpaqueType = isDeclareOpaqueType;
exports.isDeclareVariable = isDeclareVariable;
exports.isDeclareExportDeclaration = isDeclareExportDeclaration;
exports.isDeclareExportAllDeclaration = isDeclareExportAllDeclaration;
exports.isDeclaredPredicate = isDeclaredPredicate;
exports.isExistsTypeAnnotation = isExistsTypeAnnotation;
exports.isFunctionTypeAnnotation = isFunctionTypeAnnotation;
exports.isFunctionTypeParam = isFunctionTypeParam;
exports.isGenericTypeAnnotation = isGenericTypeAnnotation;
exports.isInferredPredicate = isInferredPredicate;
exports.isInterfaceExtends = isInterfaceExtends;
exports.isInterfaceDeclaration = isInterfaceDeclaration;
exports.isIntersectionTypeAnnotation = isIntersectionTypeAnnotation;
exports.isMixedTypeAnnotation = isMixedTypeAnnotation;
exports.isEmptyTypeAnnotation = isEmptyTypeAnnotation;
exports.isNullableTypeAnnotation = isNullableTypeAnnotation;
exports.isNumberLiteralTypeAnnotation = isNumberLiteralTypeAnnotation;
exports.isNumberTypeAnnotation = isNumberTypeAnnotation;
exports.isObjectTypeAnnotation = isObjectTypeAnnotation;
exports.isObjectTypeCallProperty = isObjectTypeCallProperty;
exports.isObjectTypeIndexer = isObjectTypeIndexer;
exports.isObjectTypeProperty = isObjectTypeProperty;
exports.isObjectTypeSpreadProperty = isObjectTypeSpreadProperty;
exports.isOpaqueType = isOpaqueType;
exports.isQualifiedTypeIdentifier = isQualifiedTypeIdentifier;
exports.isStringLiteralTypeAnnotation = isStringLiteralTypeAnnotation;
exports.isStringTypeAnnotation = isStringTypeAnnotation;
exports.isThisTypeAnnotation = isThisTypeAnnotation;
exports.isTupleTypeAnnotation = isTupleTypeAnnotation;
exports.isTypeofTypeAnnotation = isTypeofTypeAnnotation;
exports.isTypeAlias = isTypeAlias;
exports.isTypeAnnotation = isTypeAnnotation;
exports.isTypeCastExpression = isTypeCastExpression;
exports.isTypeParameter = isTypeParameter;
exports.isTypeParameterDeclaration = isTypeParameterDeclaration;
exports.isTypeParameterInstantiation = isTypeParameterInstantiation;
exports.isUnionTypeAnnotation = isUnionTypeAnnotation;
exports.isVariance = isVariance;
exports.isVoidTypeAnnotation = isVoidTypeAnnotation;
exports.isJSXAttribute = isJSXAttribute;
exports.isJSXClosingElement = isJSXClosingElement;
exports.isJSXElement = isJSXElement;
exports.isJSXEmptyExpression = isJSXEmptyExpression;
exports.isJSXExpressionContainer = isJSXExpressionContainer;
exports.isJSXSpreadChild = isJSXSpreadChild;
exports.isJSXIdentifier = isJSXIdentifier;
exports.isJSXMemberExpression = isJSXMemberExpression;
exports.isJSXNamespacedName = isJSXNamespacedName;
exports.isJSXOpeningElement = isJSXOpeningElement;
exports.isJSXSpreadAttribute = isJSXSpreadAttribute;
exports.isJSXText = isJSXText;
exports.isJSXFragment = isJSXFragment;
exports.isJSXOpeningFragment = isJSXOpeningFragment;
exports.isJSXClosingFragment = isJSXClosingFragment;
exports.isNoop = isNoop;
exports.isParenthesizedExpression = isParenthesizedExpression;
exports.isAwaitExpression = isAwaitExpression;
exports.isBindExpression = isBindExpression;
exports.isClassProperty = isClassProperty;
exports.isImport = isImport;
exports.isDecorator = isDecorator;
exports.isDoExpression = isDoExpression;
exports.isExportDefaultSpecifier = isExportDefaultSpecifier;
exports.isExportNamespaceSpecifier = isExportNamespaceSpecifier;
exports.isTSParameterProperty = isTSParameterProperty;
exports.isTSDeclareFunction = isTSDeclareFunction;
exports.isTSDeclareMethod = isTSDeclareMethod;
exports.isTSQualifiedName = isTSQualifiedName;
exports.isTSCallSignatureDeclaration = isTSCallSignatureDeclaration;
exports.isTSConstructSignatureDeclaration = isTSConstructSignatureDeclaration;
exports.isTSPropertySignature = isTSPropertySignature;
exports.isTSMethodSignature = isTSMethodSignature;
exports.isTSIndexSignature = isTSIndexSignature;
exports.isTSAnyKeyword = isTSAnyKeyword;
exports.isTSNumberKeyword = isTSNumberKeyword;
exports.isTSObjectKeyword = isTSObjectKeyword;
exports.isTSBooleanKeyword = isTSBooleanKeyword;
exports.isTSStringKeyword = isTSStringKeyword;
exports.isTSSymbolKeyword = isTSSymbolKeyword;
exports.isTSVoidKeyword = isTSVoidKeyword;
exports.isTSUndefinedKeyword = isTSUndefinedKeyword;
exports.isTSNullKeyword = isTSNullKeyword;
exports.isTSNeverKeyword = isTSNeverKeyword;
exports.isTSThisType = isTSThisType;
exports.isTSFunctionType = isTSFunctionType;
exports.isTSConstructorType = isTSConstructorType;
exports.isTSTypeReference = isTSTypeReference;
exports.isTSTypePredicate = isTSTypePredicate;
exports.isTSTypeQuery = isTSTypeQuery;
exports.isTSTypeLiteral = isTSTypeLiteral;
exports.isTSArrayType = isTSArrayType;
exports.isTSTupleType = isTSTupleType;
exports.isTSUnionType = isTSUnionType;
exports.isTSIntersectionType = isTSIntersectionType;
exports.isTSParenthesizedType = isTSParenthesizedType;
exports.isTSTypeOperator = isTSTypeOperator;
exports.isTSIndexedAccessType = isTSIndexedAccessType;
exports.isTSMappedType = isTSMappedType;
exports.isTSLiteralType = isTSLiteralType;
exports.isTSExpressionWithTypeArguments = isTSExpressionWithTypeArguments;
exports.isTSInterfaceDeclaration = isTSInterfaceDeclaration;
exports.isTSInterfaceBody = isTSInterfaceBody;
exports.isTSTypeAliasDeclaration = isTSTypeAliasDeclaration;
exports.isTSAsExpression = isTSAsExpression;
exports.isTSTypeAssertion = isTSTypeAssertion;
exports.isTSEnumDeclaration = isTSEnumDeclaration;
exports.isTSEnumMember = isTSEnumMember;
exports.isTSModuleDeclaration = isTSModuleDeclaration;
exports.isTSModuleBlock = isTSModuleBlock;
exports.isTSImportEqualsDeclaration = isTSImportEqualsDeclaration;
exports.isTSExternalModuleReference = isTSExternalModuleReference;
exports.isTSNonNullExpression = isTSNonNullExpression;
exports.isTSExportAssignment = isTSExportAssignment;
exports.isTSNamespaceExportDeclaration = isTSNamespaceExportDeclaration;
exports.isTSTypeAnnotation = isTSTypeAnnotation;
exports.isTSTypeParameterInstantiation = isTSTypeParameterInstantiation;
exports.isTSTypeParameterDeclaration = isTSTypeParameterDeclaration;
exports.isTSTypeParameter = isTSTypeParameter;
exports.isExpression = isExpression;
exports.isBinary = isBinary;
exports.isScopable = isScopable;
exports.isBlockParent = isBlockParent;
exports.isBlock = isBlock;
exports.isStatement = isStatement;
exports.isTerminatorless = isTerminatorless;
exports.isCompletionStatement = isCompletionStatement;
exports.isConditional = isConditional;
exports.isLoop = isLoop;
exports.isWhile = isWhile;
exports.isExpressionWrapper = isExpressionWrapper;
exports.isFor = isFor;
exports.isForXStatement = isForXStatement;
exports.isFunction = isFunction;
exports.isFunctionParent = isFunctionParent;
exports.isPureish = isPureish;
exports.isDeclaration = isDeclaration;
exports.isPatternLike = isPatternLike;
exports.isLVal = isLVal;
exports.isTSEntityName = isTSEntityName;
exports.isLiteral = isLiteral;
exports.isImmutable = isImmutable;
exports.isUserWhitespacable = isUserWhitespacable;
exports.isMethod = isMethod;
exports.isObjectMember = isObjectMember;
exports.isProperty = isProperty;
exports.isUnaryLike = isUnaryLike;
exports.isPattern = isPattern;
exports.isClass = isClass;
exports.isModuleDeclaration = isModuleDeclaration;
exports.isExportDeclaration = isExportDeclaration;
exports.isModuleSpecifier = isModuleSpecifier;
exports.isFlow = isFlow;
exports.isFlowType = isFlowType;
exports.isFlowBaseAnnotation = isFlowBaseAnnotation;
exports.isFlowDeclaration = isFlowDeclaration;
exports.isFlowPredicate = isFlowPredicate;
exports.isJSX = isJSX;
exports.isTSTypeElement = isTSTypeElement;
exports.isTSType = isTSType;
exports.isNumberLiteral = isNumberLiteral;
exports.isRegexLiteral = isRegexLiteral;
exports.isRestProperty = isRestProperty;
exports.isSpreadProperty = isSpreadProperty;

var _is = _interopRequireDefault(__webpack_require__(110));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function isArrayExpression(node, opts) {
  return (0, _is.default)("ArrayExpression", node, opts);
}

function isAssignmentExpression(node, opts) {
  return (0, _is.default)("AssignmentExpression", node, opts);
}

function isBinaryExpression(node, opts) {
  return (0, _is.default)("BinaryExpression", node, opts);
}

function isDirective(node, opts) {
  return (0, _is.default)("Directive", node, opts);
}

function isDirectiveLiteral(node, opts) {
  return (0, _is.default)("DirectiveLiteral", node, opts);
}

function isBlockStatement(node, opts) {
  return (0, _is.default)("BlockStatement", node, opts);
}

function isBreakStatement(node, opts) {
  return (0, _is.default)("BreakStatement", node, opts);
}

function isCallExpression(node, opts) {
  return (0, _is.default)("CallExpression", node, opts);
}

function isCatchClause(node, opts) {
  return (0, _is.default)("CatchClause", node, opts);
}

function isConditionalExpression(node, opts) {
  return (0, _is.default)("ConditionalExpression", node, opts);
}

function isContinueStatement(node, opts) {
  return (0, _is.default)("ContinueStatement", node, opts);
}

function isDebuggerStatement(node, opts) {
  return (0, _is.default)("DebuggerStatement", node, opts);
}

function isDoWhileStatement(node, opts) {
  return (0, _is.default)("DoWhileStatement", node, opts);
}

function isEmptyStatement(node, opts) {
  return (0, _is.default)("EmptyStatement", node, opts);
}

function isExpressionStatement(node, opts) {
  return (0, _is.default)("ExpressionStatement", node, opts);
}

function isFile(node, opts) {
  return (0, _is.default)("File", node, opts);
}

function isForInStatement(node, opts) {
  return (0, _is.default)("ForInStatement", node, opts);
}

function isForStatement(node, opts) {
  return (0, _is.default)("ForStatement", node, opts);
}

function isFunctionDeclaration(node, opts) {
  return (0, _is.default)("FunctionDeclaration", node, opts);
}

function isFunctionExpression(node, opts) {
  return (0, _is.default)("FunctionExpression", node, opts);
}

function isIdentifier(node, opts) {
  return (0, _is.default)("Identifier", node, opts);
}

function isIfStatement(node, opts) {
  return (0, _is.default)("IfStatement", node, opts);
}

function isLabeledStatement(node, opts) {
  return (0, _is.default)("LabeledStatement", node, opts);
}

function isStringLiteral(node, opts) {
  return (0, _is.default)("StringLiteral", node, opts);
}

function isNumericLiteral(node, opts) {
  return (0, _is.default)("NumericLiteral", node, opts);
}

function isNullLiteral(node, opts) {
  return (0, _is.default)("NullLiteral", node, opts);
}

function isBooleanLiteral(node, opts) {
  return (0, _is.default)("BooleanLiteral", node, opts);
}

function isRegExpLiteral(node, opts) {
  return (0, _is.default)("RegExpLiteral", node, opts);
}

function isLogicalExpression(node, opts) {
  return (0, _is.default)("LogicalExpression", node, opts);
}

function isMemberExpression(node, opts) {
  return (0, _is.default)("MemberExpression", node, opts);
}

function isNewExpression(node, opts) {
  return (0, _is.default)("NewExpression", node, opts);
}

function isProgram(node, opts) {
  return (0, _is.default)("Program", node, opts);
}

function isObjectExpression(node, opts) {
  return (0, _is.default)("ObjectExpression", node, opts);
}

function isObjectMethod(node, opts) {
  return (0, _is.default)("ObjectMethod", node, opts);
}

function isObjectProperty(node, opts) {
  return (0, _is.default)("ObjectProperty", node, opts);
}

function isRestElement(node, opts) {
  return (0, _is.default)("RestElement", node, opts);
}

function isReturnStatement(node, opts) {
  return (0, _is.default)("ReturnStatement", node, opts);
}

function isSequenceExpression(node, opts) {
  return (0, _is.default)("SequenceExpression", node, opts);
}

function isSwitchCase(node, opts) {
  return (0, _is.default)("SwitchCase", node, opts);
}

function isSwitchStatement(node, opts) {
  return (0, _is.default)("SwitchStatement", node, opts);
}

function isThisExpression(node, opts) {
  return (0, _is.default)("ThisExpression", node, opts);
}

function isThrowStatement(node, opts) {
  return (0, _is.default)("ThrowStatement", node, opts);
}

function isTryStatement(node, opts) {
  return (0, _is.default)("TryStatement", node, opts);
}

function isUnaryExpression(node, opts) {
  return (0, _is.default)("UnaryExpression", node, opts);
}

function isUpdateExpression(node, opts) {
  return (0, _is.default)("UpdateExpression", node, opts);
}

function isVariableDeclaration(node, opts) {
  return (0, _is.default)("VariableDeclaration", node, opts);
}

function isVariableDeclarator(node, opts) {
  return (0, _is.default)("VariableDeclarator", node, opts);
}

function isWhileStatement(node, opts) {
  return (0, _is.default)("WhileStatement", node, opts);
}

function isWithStatement(node, opts) {
  return (0, _is.default)("WithStatement", node, opts);
}

function isAssignmentPattern(node, opts) {
  return (0, _is.default)("AssignmentPattern", node, opts);
}

function isArrayPattern(node, opts) {
  return (0, _is.default)("ArrayPattern", node, opts);
}

function isArrowFunctionExpression(node, opts) {
  return (0, _is.default)("ArrowFunctionExpression", node, opts);
}

function isClassBody(node, opts) {
  return (0, _is.default)("ClassBody", node, opts);
}

function isClassDeclaration(node, opts) {
  return (0, _is.default)("ClassDeclaration", node, opts);
}

function isClassExpression(node, opts) {
  return (0, _is.default)("ClassExpression", node, opts);
}

function isExportAllDeclaration(node, opts) {
  return (0, _is.default)("ExportAllDeclaration", node, opts);
}

function isExportDefaultDeclaration(node, opts) {
  return (0, _is.default)("ExportDefaultDeclaration", node, opts);
}

function isExportNamedDeclaration(node, opts) {
  return (0, _is.default)("ExportNamedDeclaration", node, opts);
}

function isExportSpecifier(node, opts) {
  return (0, _is.default)("ExportSpecifier", node, opts);
}

function isForOfStatement(node, opts) {
  return (0, _is.default)("ForOfStatement", node, opts);
}

function isImportDeclaration(node, opts) {
  return (0, _is.default)("ImportDeclaration", node, opts);
}

function isImportDefaultSpecifier(node, opts) {
  return (0, _is.default)("ImportDefaultSpecifier", node, opts);
}

function isImportNamespaceSpecifier(node, opts) {
  return (0, _is.default)("ImportNamespaceSpecifier", node, opts);
}

function isImportSpecifier(node, opts) {
  return (0, _is.default)("ImportSpecifier", node, opts);
}

function isMetaProperty(node, opts) {
  return (0, _is.default)("MetaProperty", node, opts);
}

function isClassMethod(node, opts) {
  return (0, _is.default)("ClassMethod", node, opts);
}

function isObjectPattern(node, opts) {
  return (0, _is.default)("ObjectPattern", node, opts);
}

function isSpreadElement(node, opts) {
  return (0, _is.default)("SpreadElement", node, opts);
}

function isSuper(node, opts) {
  return (0, _is.default)("Super", node, opts);
}

function isTaggedTemplateExpression(node, opts) {
  return (0, _is.default)("TaggedTemplateExpression", node, opts);
}

function isTemplateElement(node, opts) {
  return (0, _is.default)("TemplateElement", node, opts);
}

function isTemplateLiteral(node, opts) {
  return (0, _is.default)("TemplateLiteral", node, opts);
}

function isYieldExpression(node, opts) {
  return (0, _is.default)("YieldExpression", node, opts);
}

function isAnyTypeAnnotation(node, opts) {
  return (0, _is.default)("AnyTypeAnnotation", node, opts);
}

function isArrayTypeAnnotation(node, opts) {
  return (0, _is.default)("ArrayTypeAnnotation", node, opts);
}

function isBooleanTypeAnnotation(node, opts) {
  return (0, _is.default)("BooleanTypeAnnotation", node, opts);
}

function isBooleanLiteralTypeAnnotation(node, opts) {
  return (0, _is.default)("BooleanLiteralTypeAnnotation", node, opts);
}

function isNullLiteralTypeAnnotation(node, opts) {
  return (0, _is.default)("NullLiteralTypeAnnotation", node, opts);
}

function isClassImplements(node, opts) {
  return (0, _is.default)("ClassImplements", node, opts);
}

function isDeclareClass(node, opts) {
  return (0, _is.default)("DeclareClass", node, opts);
}

function isDeclareFunction(node, opts) {
  return (0, _is.default)("DeclareFunction", node, opts);
}

function isDeclareInterface(node, opts) {
  return (0, _is.default)("DeclareInterface", node, opts);
}

function isDeclareModule(node, opts) {
  return (0, _is.default)("DeclareModule", node, opts);
}

function isDeclareModuleExports(node, opts) {
  return (0, _is.default)("DeclareModuleExports", node, opts);
}

function isDeclareTypeAlias(node, opts) {
  return (0, _is.default)("DeclareTypeAlias", node, opts);
}

function isDeclareOpaqueType(node, opts) {
  return (0, _is.default)("DeclareOpaqueType", node, opts);
}

function isDeclareVariable(node, opts) {
  return (0, _is.default)("DeclareVariable", node, opts);
}

function isDeclareExportDeclaration(node, opts) {
  return (0, _is.default)("DeclareExportDeclaration", node, opts);
}

function isDeclareExportAllDeclaration(node, opts) {
  return (0, _is.default)("DeclareExportAllDeclaration", node, opts);
}

function isDeclaredPredicate(node, opts) {
  return (0, _is.default)("DeclaredPredicate", node, opts);
}

function isExistsTypeAnnotation(node, opts) {
  return (0, _is.default)("ExistsTypeAnnotation", node, opts);
}

function isFunctionTypeAnnotation(node, opts) {
  return (0, _is.default)("FunctionTypeAnnotation", node, opts);
}

function isFunctionTypeParam(node, opts) {
  return (0, _is.default)("FunctionTypeParam", node, opts);
}

function isGenericTypeAnnotation(node, opts) {
  return (0, _is.default)("GenericTypeAnnotation", node, opts);
}

function isInferredPredicate(node, opts) {
  return (0, _is.default)("InferredPredicate", node, opts);
}

function isInterfaceExtends(node, opts) {
  return (0, _is.default)("InterfaceExtends", node, opts);
}

function isInterfaceDeclaration(node, opts) {
  return (0, _is.default)("InterfaceDeclaration", node, opts);
}

function isIntersectionTypeAnnotation(node, opts) {
  return (0, _is.default)("IntersectionTypeAnnotation", node, opts);
}

function isMixedTypeAnnotation(node, opts) {
  return (0, _is.default)("MixedTypeAnnotation", node, opts);
}

function isEmptyTypeAnnotation(node, opts) {
  return (0, _is.default)("EmptyTypeAnnotation", node, opts);
}

function isNullableTypeAnnotation(node, opts) {
  return (0, _is.default)("NullableTypeAnnotation", node, opts);
}

function isNumberLiteralTypeAnnotation(node, opts) {
  return (0, _is.default)("NumberLiteralTypeAnnotation", node, opts);
}

function isNumberTypeAnnotation(node, opts) {
  return (0, _is.default)("NumberTypeAnnotation", node, opts);
}

function isObjectTypeAnnotation(node, opts) {
  return (0, _is.default)("ObjectTypeAnnotation", node, opts);
}

function isObjectTypeCallProperty(node, opts) {
  return (0, _is.default)("ObjectTypeCallProperty", node, opts);
}

function isObjectTypeIndexer(node, opts) {
  return (0, _is.default)("ObjectTypeIndexer", node, opts);
}

function isObjectTypeProperty(node, opts) {
  return (0, _is.default)("ObjectTypeProperty", node, opts);
}

function isObjectTypeSpreadProperty(node, opts) {
  return (0, _is.default)("ObjectTypeSpreadProperty", node, opts);
}

function isOpaqueType(node, opts) {
  return (0, _is.default)("OpaqueType", node, opts);
}

function isQualifiedTypeIdentifier(node, opts) {
  return (0, _is.default)("QualifiedTypeIdentifier", node, opts);
}

function isStringLiteralTypeAnnotation(node, opts) {
  return (0, _is.default)("StringLiteralTypeAnnotation", node, opts);
}

function isStringTypeAnnotation(node, opts) {
  return (0, _is.default)("StringTypeAnnotation", node, opts);
}

function isThisTypeAnnotation(node, opts) {
  return (0, _is.default)("ThisTypeAnnotation", node, opts);
}

function isTupleTypeAnnotation(node, opts) {
  return (0, _is.default)("TupleTypeAnnotation", node, opts);
}

function isTypeofTypeAnnotation(node, opts) {
  return (0, _is.default)("TypeofTypeAnnotation", node, opts);
}

function isTypeAlias(node, opts) {
  return (0, _is.default)("TypeAlias", node, opts);
}

function isTypeAnnotation(node, opts) {
  return (0, _is.default)("TypeAnnotation", node, opts);
}

function isTypeCastExpression(node, opts) {
  return (0, _is.default)("TypeCastExpression", node, opts);
}

function isTypeParameter(node, opts) {
  return (0, _is.default)("TypeParameter", node, opts);
}

function isTypeParameterDeclaration(node, opts) {
  return (0, _is.default)("TypeParameterDeclaration", node, opts);
}

function isTypeParameterInstantiation(node, opts) {
  return (0, _is.default)("TypeParameterInstantiation", node, opts);
}

function isUnionTypeAnnotation(node, opts) {
  return (0, _is.default)("UnionTypeAnnotation", node, opts);
}

function isVariance(node, opts) {
  return (0, _is.default)("Variance", node, opts);
}

function isVoidTypeAnnotation(node, opts) {
  return (0, _is.default)("VoidTypeAnnotation", node, opts);
}

function isJSXAttribute(node, opts) {
  return (0, _is.default)("JSXAttribute", node, opts);
}

function isJSXClosingElement(node, opts) {
  return (0, _is.default)("JSXClosingElement", node, opts);
}

function isJSXElement(node, opts) {
  return (0, _is.default)("JSXElement", node, opts);
}

function isJSXEmptyExpression(node, opts) {
  return (0, _is.default)("JSXEmptyExpression", node, opts);
}

function isJSXExpressionContainer(node, opts) {
  return (0, _is.default)("JSXExpressionContainer", node, opts);
}

function isJSXSpreadChild(node, opts) {
  return (0, _is.default)("JSXSpreadChild", node, opts);
}

function isJSXIdentifier(node, opts) {
  return (0, _is.default)("JSXIdentifier", node, opts);
}

function isJSXMemberExpression(node, opts) {
  return (0, _is.default)("JSXMemberExpression", node, opts);
}

function isJSXNamespacedName(node, opts) {
  return (0, _is.default)("JSXNamespacedName", node, opts);
}

function isJSXOpeningElement(node, opts) {
  return (0, _is.default)("JSXOpeningElement", node, opts);
}

function isJSXSpreadAttribute(node, opts) {
  return (0, _is.default)("JSXSpreadAttribute", node, opts);
}

function isJSXText(node, opts) {
  return (0, _is.default)("JSXText", node, opts);
}

function isJSXFragment(node, opts) {
  return (0, _is.default)("JSXFragment", node, opts);
}

function isJSXOpeningFragment(node, opts) {
  return (0, _is.default)("JSXOpeningFragment", node, opts);
}

function isJSXClosingFragment(node, opts) {
  return (0, _is.default)("JSXClosingFragment", node, opts);
}

function isNoop(node, opts) {
  return (0, _is.default)("Noop", node, opts);
}

function isParenthesizedExpression(node, opts) {
  return (0, _is.default)("ParenthesizedExpression", node, opts);
}

function isAwaitExpression(node, opts) {
  return (0, _is.default)("AwaitExpression", node, opts);
}

function isBindExpression(node, opts) {
  return (0, _is.default)("BindExpression", node, opts);
}

function isClassProperty(node, opts) {
  return (0, _is.default)("ClassProperty", node, opts);
}

function isImport(node, opts) {
  return (0, _is.default)("Import", node, opts);
}

function isDecorator(node, opts) {
  return (0, _is.default)("Decorator", node, opts);
}

function isDoExpression(node, opts) {
  return (0, _is.default)("DoExpression", node, opts);
}

function isExportDefaultSpecifier(node, opts) {
  return (0, _is.default)("ExportDefaultSpecifier", node, opts);
}

function isExportNamespaceSpecifier(node, opts) {
  return (0, _is.default)("ExportNamespaceSpecifier", node, opts);
}

function isTSParameterProperty(node, opts) {
  return (0, _is.default)("TSParameterProperty", node, opts);
}

function isTSDeclareFunction(node, opts) {
  return (0, _is.default)("TSDeclareFunction", node, opts);
}

function isTSDeclareMethod(node, opts) {
  return (0, _is.default)("TSDeclareMethod", node, opts);
}

function isTSQualifiedName(node, opts) {
  return (0, _is.default)("TSQualifiedName", node, opts);
}

function isTSCallSignatureDeclaration(node, opts) {
  return (0, _is.default)("TSCallSignatureDeclaration", node, opts);
}

function isTSConstructSignatureDeclaration(node, opts) {
  return (0, _is.default)("TSConstructSignatureDeclaration", node, opts);
}

function isTSPropertySignature(node, opts) {
  return (0, _is.default)("TSPropertySignature", node, opts);
}

function isTSMethodSignature(node, opts) {
  return (0, _is.default)("TSMethodSignature", node, opts);
}

function isTSIndexSignature(node, opts) {
  return (0, _is.default)("TSIndexSignature", node, opts);
}

function isTSAnyKeyword(node, opts) {
  return (0, _is.default)("TSAnyKeyword", node, opts);
}

function isTSNumberKeyword(node, opts) {
  return (0, _is.default)("TSNumberKeyword", node, opts);
}

function isTSObjectKeyword(node, opts) {
  return (0, _is.default)("TSObjectKeyword", node, opts);
}

function isTSBooleanKeyword(node, opts) {
  return (0, _is.default)("TSBooleanKeyword", node, opts);
}

function isTSStringKeyword(node, opts) {
  return (0, _is.default)("TSStringKeyword", node, opts);
}

function isTSSymbolKeyword(node, opts) {
  return (0, _is.default)("TSSymbolKeyword", node, opts);
}

function isTSVoidKeyword(node, opts) {
  return (0, _is.default)("TSVoidKeyword", node, opts);
}

function isTSUndefinedKeyword(node, opts) {
  return (0, _is.default)("TSUndefinedKeyword", node, opts);
}

function isTSNullKeyword(node, opts) {
  return (0, _is.default)("TSNullKeyword", node, opts);
}

function isTSNeverKeyword(node, opts) {
  return (0, _is.default)("TSNeverKeyword", node, opts);
}

function isTSThisType(node, opts) {
  return (0, _is.default)("TSThisType", node, opts);
}

function isTSFunctionType(node, opts) {
  return (0, _is.default)("TSFunctionType", node, opts);
}

function isTSConstructorType(node, opts) {
  return (0, _is.default)("TSConstructorType", node, opts);
}

function isTSTypeReference(node, opts) {
  return (0, _is.default)("TSTypeReference", node, opts);
}

function isTSTypePredicate(node, opts) {
  return (0, _is.default)("TSTypePredicate", node, opts);
}

function isTSTypeQuery(node, opts) {
  return (0, _is.default)("TSTypeQuery", node, opts);
}

function isTSTypeLiteral(node, opts) {
  return (0, _is.default)("TSTypeLiteral", node, opts);
}

function isTSArrayType(node, opts) {
  return (0, _is.default)("TSArrayType", node, opts);
}

function isTSTupleType(node, opts) {
  return (0, _is.default)("TSTupleType", node, opts);
}

function isTSUnionType(node, opts) {
  return (0, _is.default)("TSUnionType", node, opts);
}

function isTSIntersectionType(node, opts) {
  return (0, _is.default)("TSIntersectionType", node, opts);
}

function isTSParenthesizedType(node, opts) {
  return (0, _is.default)("TSParenthesizedType", node, opts);
}

function isTSTypeOperator(node, opts) {
  return (0, _is.default)("TSTypeOperator", node, opts);
}

function isTSIndexedAccessType(node, opts) {
  return (0, _is.default)("TSIndexedAccessType", node, opts);
}

function isTSMappedType(node, opts) {
  return (0, _is.default)("TSMappedType", node, opts);
}

function isTSLiteralType(node, opts) {
  return (0, _is.default)("TSLiteralType", node, opts);
}

function isTSExpressionWithTypeArguments(node, opts) {
  return (0, _is.default)("TSExpressionWithTypeArguments", node, opts);
}

function isTSInterfaceDeclaration(node, opts) {
  return (0, _is.default)("TSInterfaceDeclaration", node, opts);
}

function isTSInterfaceBody(node, opts) {
  return (0, _is.default)("TSInterfaceBody", node, opts);
}

function isTSTypeAliasDeclaration(node, opts) {
  return (0, _is.default)("TSTypeAliasDeclaration", node, opts);
}

function isTSAsExpression(node, opts) {
  return (0, _is.default)("TSAsExpression", node, opts);
}

function isTSTypeAssertion(node, opts) {
  return (0, _is.default)("TSTypeAssertion", node, opts);
}

function isTSEnumDeclaration(node, opts) {
  return (0, _is.default)("TSEnumDeclaration", node, opts);
}

function isTSEnumMember(node, opts) {
  return (0, _is.default)("TSEnumMember", node, opts);
}

function isTSModuleDeclaration(node, opts) {
  return (0, _is.default)("TSModuleDeclaration", node, opts);
}

function isTSModuleBlock(node, opts) {
  return (0, _is.default)("TSModuleBlock", node, opts);
}

function isTSImportEqualsDeclaration(node, opts) {
  return (0, _is.default)("TSImportEqualsDeclaration", node, opts);
}

function isTSExternalModuleReference(node, opts) {
  return (0, _is.default)("TSExternalModuleReference", node, opts);
}

function isTSNonNullExpression(node, opts) {
  return (0, _is.default)("TSNonNullExpression", node, opts);
}

function isTSExportAssignment(node, opts) {
  return (0, _is.default)("TSExportAssignment", node, opts);
}

function isTSNamespaceExportDeclaration(node, opts) {
  return (0, _is.default)("TSNamespaceExportDeclaration", node, opts);
}

function isTSTypeAnnotation(node, opts) {
  return (0, _is.default)("TSTypeAnnotation", node, opts);
}

function isTSTypeParameterInstantiation(node, opts) {
  return (0, _is.default)("TSTypeParameterInstantiation", node, opts);
}

function isTSTypeParameterDeclaration(node, opts) {
  return (0, _is.default)("TSTypeParameterDeclaration", node, opts);
}

function isTSTypeParameter(node, opts) {
  return (0, _is.default)("TSTypeParameter", node, opts);
}

function isExpression(node, opts) {
  return (0, _is.default)("Expression", node, opts);
}

function isBinary(node, opts) {
  return (0, _is.default)("Binary", node, opts);
}

function isScopable(node, opts) {
  return (0, _is.default)("Scopable", node, opts);
}

function isBlockParent(node, opts) {
  return (0, _is.default)("BlockParent", node, opts);
}

function isBlock(node, opts) {
  return (0, _is.default)("Block", node, opts);
}

function isStatement(node, opts) {
  return (0, _is.default)("Statement", node, opts);
}

function isTerminatorless(node, opts) {
  return (0, _is.default)("Terminatorless", node, opts);
}

function isCompletionStatement(node, opts) {
  return (0, _is.default)("CompletionStatement", node, opts);
}

function isConditional(node, opts) {
  return (0, _is.default)("Conditional", node, opts);
}

function isLoop(node, opts) {
  return (0, _is.default)("Loop", node, opts);
}

function isWhile(node, opts) {
  return (0, _is.default)("While", node, opts);
}

function isExpressionWrapper(node, opts) {
  return (0, _is.default)("ExpressionWrapper", node, opts);
}

function isFor(node, opts) {
  return (0, _is.default)("For", node, opts);
}

function isForXStatement(node, opts) {
  return (0, _is.default)("ForXStatement", node, opts);
}

function isFunction(node, opts) {
  return (0, _is.default)("Function", node, opts);
}

function isFunctionParent(node, opts) {
  return (0, _is.default)("FunctionParent", node, opts);
}

function isPureish(node, opts) {
  return (0, _is.default)("Pureish", node, opts);
}

function isDeclaration(node, opts) {
  return (0, _is.default)("Declaration", node, opts);
}

function isPatternLike(node, opts) {
  return (0, _is.default)("PatternLike", node, opts);
}

function isLVal(node, opts) {
  return (0, _is.default)("LVal", node, opts);
}

function isTSEntityName(node, opts) {
  return (0, _is.default)("TSEntityName", node, opts);
}

function isLiteral(node, opts) {
  return (0, _is.default)("Literal", node, opts);
}

function isImmutable(node, opts) {
  return (0, _is.default)("Immutable", node, opts);
}

function isUserWhitespacable(node, opts) {
  return (0, _is.default)("UserWhitespacable", node, opts);
}

function isMethod(node, opts) {
  return (0, _is.default)("Method", node, opts);
}

function isObjectMember(node, opts) {
  return (0, _is.default)("ObjectMember", node, opts);
}

function isProperty(node, opts) {
  return (0, _is.default)("Property", node, opts);
}

function isUnaryLike(node, opts) {
  return (0, _is.default)("UnaryLike", node, opts);
}

function isPattern(node, opts) {
  return (0, _is.default)("Pattern", node, opts);
}

function isClass(node, opts) {
  return (0, _is.default)("Class", node, opts);
}

function isModuleDeclaration(node, opts) {
  return (0, _is.default)("ModuleDeclaration", node, opts);
}

function isExportDeclaration(node, opts) {
  return (0, _is.default)("ExportDeclaration", node, opts);
}

function isModuleSpecifier(node, opts) {
  return (0, _is.default)("ModuleSpecifier", node, opts);
}

function isFlow(node, opts) {
  return (0, _is.default)("Flow", node, opts);
}

function isFlowType(node, opts) {
  return (0, _is.default)("FlowType", node, opts);
}

function isFlowBaseAnnotation(node, opts) {
  return (0, _is.default)("FlowBaseAnnotation", node, opts);
}

function isFlowDeclaration(node, opts) {
  return (0, _is.default)("FlowDeclaration", node, opts);
}

function isFlowPredicate(node, opts) {
  return (0, _is.default)("FlowPredicate", node, opts);
}

function isJSX(node, opts) {
  return (0, _is.default)("JSX", node, opts);
}

function isTSTypeElement(node, opts) {
  return (0, _is.default)("TSTypeElement", node, opts);
}

function isTSType(node, opts) {
  return (0, _is.default)("TSType", node, opts);
}

function isNumberLiteral(node, opts) {
  console.trace("The node type NumberLiteral has been renamed to NumericLiteral");
  return (0, _is.default)("NumberLiteral", node, opts);
}

function isRegexLiteral(node, opts) {
  console.trace("The node type RegexLiteral has been renamed to RegExpLiteral");
  return (0, _is.default)("RegexLiteral", node, opts);
}

function isRestProperty(node, opts) {
  console.trace("The node type RestProperty has been renamed to RestElement");
  return (0, _is.default)("RestProperty", node, opts);
}

function isSpreadProperty(node, opts) {
  console.trace("The node type SpreadProperty has been renamed to SpreadElement");
  return (0, _is.default)("SpreadProperty", node, opts);
}

/***/ }),
/* 18 */
/***/ (function(module, exports, __webpack_require__) {

var freeGlobal = __webpack_require__(52);

/** Detect free variable `self`. */
var freeSelf = typeof self == 'object' && self && self.Object === Object && self;

/** Used as a reference to the global object. */
var root = freeGlobal || freeSelf || Function('return this')();

module.exports = root;


/***/ }),
/* 19 */,
/* 20 */
/***/ (function(module, exports, __webpack_require__) {

var root = __webpack_require__(18);

/** Built-in value references. */
var Symbol = root.Symbol;

module.exports = Symbol;


/***/ }),
/* 21 */
/***/ (function(module, exports) {

/**
 * Checks if `value` is object-like. A value is object-like if it's not `null`
 * and has a `typeof` result of "object".
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is object-like, else `false`.
 * @example
 *
 * _.isObjectLike({});
 * // => true
 *
 * _.isObjectLike([1, 2, 3]);
 * // => true
 *
 * _.isObjectLike(_.noop);
 * // => false
 *
 * _.isObjectLike(null);
 * // => false
 */
function isObjectLike(value) {
  return value != null && typeof value == 'object';
}

module.exports = isObjectLike;


/***/ }),
/* 22 */,
/* 23 */
/***/ (function(module, exports, __webpack_require__) {

var Symbol = __webpack_require__(20),
    getRawTag = __webpack_require__(63),
    objectToString = __webpack_require__(64);

/** `Object#toString` result references. */
var nullTag = '[object Null]',
    undefinedTag = '[object Undefined]';

/** Built-in value references. */
var symToStringTag = Symbol ? Symbol.toStringTag : undefined;

/**
 * The base implementation of `getTag` without fallbacks for buggy environments.
 *
 * @private
 * @param {*} value The value to query.
 * @returns {string} Returns the `toStringTag`.
 */
function baseGetTag(value) {
  if (value == null) {
    return value === undefined ? undefinedTag : nullTag;
  }
  return (symToStringTag && symToStringTag in Object(value))
    ? getRawTag(value)
    : objectToString(value);
}

module.exports = baseGetTag;


/***/ }),
/* 24 */,
/* 25 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsNative = __webpack_require__(127),
    getValue = __webpack_require__(130);

/**
 * Gets the native function at `key` of `object`.
 *
 * @private
 * @param {Object} object The object to query.
 * @param {string} key The key of the method to get.
 * @returns {*} Returns the function if it's native, else `undefined`.
 */
function getNative(object, key) {
  var value = getValue(object, key);
  return baseIsNative(value) ? value : undefined;
}

module.exports = getNative;


/***/ }),
/* 26 */
/***/ (function(module, exports) {

/**
 * Checks if `value` is the
 * [language type](http://www.ecma-international.org/ecma-262/7.0/#sec-ecmascript-language-types)
 * of `Object`. (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
 * @example
 *
 * _.isObject({});
 * // => true
 *
 * _.isObject([1, 2, 3]);
 * // => true
 *
 * _.isObject(_.noop);
 * // => true
 *
 * _.isObject(null);
 * // => false
 */
function isObject(value) {
  var type = typeof value;
  return value != null && (type == 'object' || type == 'function');
}

module.exports = isObject;


/***/ }),
/* 27 */
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const networkRequest = __webpack_require__(46);
const workerUtils = __webpack_require__(47);

module.exports = {
  networkRequest,
  workerUtils
};

/***/ }),
/* 28 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
var _exportNames = {
  assertNode: true,
  createTypeAnnotationBasedOnTypeof: true,
  createUnionTypeAnnotation: true,
  cloneNode: true,
  clone: true,
  cloneDeep: true,
  cloneWithoutLoc: true,
  addComment: true,
  addComments: true,
  inheritInnerComments: true,
  inheritLeadingComments: true,
  inheritsComments: true,
  inheritTrailingComments: true,
  removeComments: true,
  ensureBlock: true,
  toBindingIdentifierName: true,
  toBlock: true,
  toComputedKey: true,
  toExpression: true,
  toIdentifier: true,
  toKeyAlias: true,
  toSequenceExpression: true,
  toStatement: true,
  valueToNode: true,
  appendToMemberExpression: true,
  inherits: true,
  prependToMemberExpression: true,
  removeProperties: true,
  removePropertiesDeep: true,
  removeTypeDuplicates: true,
  getBindingIdentifiers: true,
  getOuterBindingIdentifiers: true,
  traverse: true,
  traverseFast: true,
  shallowEqual: true,
  is: true,
  isBinding: true,
  isBlockScoped: true,
  isImmutable: true,
  isLet: true,
  isNode: true,
  isNodesEquivalent: true,
  isReferenced: true,
  isScope: true,
  isSpecifierDefault: true,
  isType: true,
  isValidES3Identifier: true,
  isValidIdentifier: true,
  isVar: true,
  matchesPattern: true,
  validate: true,
  buildMatchMemberExpression: true,
  react: true
};
exports.react = exports.buildMatchMemberExpression = exports.validate = exports.matchesPattern = exports.isVar = exports.isValidIdentifier = exports.isValidES3Identifier = exports.isType = exports.isSpecifierDefault = exports.isScope = exports.isReferenced = exports.isNodesEquivalent = exports.isNode = exports.isLet = exports.isImmutable = exports.isBlockScoped = exports.isBinding = exports.is = exports.shallowEqual = exports.traverseFast = exports.traverse = exports.getOuterBindingIdentifiers = exports.getBindingIdentifiers = exports.removeTypeDuplicates = exports.removePropertiesDeep = exports.removeProperties = exports.prependToMemberExpression = exports.inherits = exports.appendToMemberExpression = exports.valueToNode = exports.toStatement = exports.toSequenceExpression = exports.toKeyAlias = exports.toIdentifier = exports.toExpression = exports.toComputedKey = exports.toBlock = exports.toBindingIdentifierName = exports.ensureBlock = exports.removeComments = exports.inheritTrailingComments = exports.inheritsComments = exports.inheritLeadingComments = exports.inheritInnerComments = exports.addComments = exports.addComment = exports.cloneWithoutLoc = exports.cloneDeep = exports.clone = exports.cloneNode = exports.createUnionTypeAnnotation = exports.createTypeAnnotationBasedOnTypeof = exports.assertNode = void 0;

var _isReactComponent = _interopRequireDefault(__webpack_require__(664));

var _isCompatTag = _interopRequireDefault(__webpack_require__(674));

var _buildChildren = _interopRequireDefault(__webpack_require__(675));

var _assertNode = _interopRequireDefault(__webpack_require__(716));

exports.assertNode = _assertNode.default;

var _generated = __webpack_require__(717);

Object.keys(_generated).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _generated[key];
});

var _createTypeAnnotationBasedOnTypeof = _interopRequireDefault(__webpack_require__(718));

exports.createTypeAnnotationBasedOnTypeof = _createTypeAnnotationBasedOnTypeof.default;

var _createUnionTypeAnnotation = _interopRequireDefault(__webpack_require__(719));

exports.createUnionTypeAnnotation = _createUnionTypeAnnotation.default;

var _generated2 = __webpack_require__(29);

Object.keys(_generated2).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _generated2[key];
});

var _cloneNode = _interopRequireDefault(__webpack_require__(86));

exports.cloneNode = _cloneNode.default;

var _clone = _interopRequireDefault(__webpack_require__(273));

exports.clone = _clone.default;

var _cloneDeep = _interopRequireDefault(__webpack_require__(720));

exports.cloneDeep = _cloneDeep.default;

var _cloneWithoutLoc = _interopRequireDefault(__webpack_require__(721));

exports.cloneWithoutLoc = _cloneWithoutLoc.default;

var _addComment = _interopRequireDefault(__webpack_require__(722));

exports.addComment = _addComment.default;

var _addComments = _interopRequireDefault(__webpack_require__(274));

exports.addComments = _addComments.default;

var _inheritInnerComments = _interopRequireDefault(__webpack_require__(275));

exports.inheritInnerComments = _inheritInnerComments.default;

var _inheritLeadingComments = _interopRequireDefault(__webpack_require__(279));

exports.inheritLeadingComments = _inheritLeadingComments.default;

var _inheritsComments = _interopRequireDefault(__webpack_require__(280));

exports.inheritsComments = _inheritsComments.default;

var _inheritTrailingComments = _interopRequireDefault(__webpack_require__(281));

exports.inheritTrailingComments = _inheritTrailingComments.default;

var _removeComments = _interopRequireDefault(__webpack_require__(731));

exports.removeComments = _removeComments.default;

var _generated3 = __webpack_require__(732);

Object.keys(_generated3).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _generated3[key];
});

var _constants = __webpack_require__(50);

Object.keys(_constants).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _constants[key];
});

var _ensureBlock = _interopRequireDefault(__webpack_require__(733));

exports.ensureBlock = _ensureBlock.default;

var _toBindingIdentifierName = _interopRequireDefault(__webpack_require__(734));

exports.toBindingIdentifierName = _toBindingIdentifierName.default;

var _toBlock = _interopRequireDefault(__webpack_require__(282));

exports.toBlock = _toBlock.default;

var _toComputedKey = _interopRequireDefault(__webpack_require__(735));

exports.toComputedKey = _toComputedKey.default;

var _toExpression = _interopRequireDefault(__webpack_require__(736));

exports.toExpression = _toExpression.default;

var _toIdentifier = _interopRequireDefault(__webpack_require__(283));

exports.toIdentifier = _toIdentifier.default;

var _toKeyAlias = _interopRequireDefault(__webpack_require__(737));

exports.toKeyAlias = _toKeyAlias.default;

var _toSequenceExpression = _interopRequireDefault(__webpack_require__(738));

exports.toSequenceExpression = _toSequenceExpression.default;

var _toStatement = _interopRequireDefault(__webpack_require__(740));

exports.toStatement = _toStatement.default;

var _valueToNode = _interopRequireDefault(__webpack_require__(741));

exports.valueToNode = _valueToNode.default;

var _definitions = __webpack_require__(35);

Object.keys(_definitions).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _definitions[key];
});

var _appendToMemberExpression = _interopRequireDefault(__webpack_require__(745));

exports.appendToMemberExpression = _appendToMemberExpression.default;

var _inherits = _interopRequireDefault(__webpack_require__(746));

exports.inherits = _inherits.default;

var _prependToMemberExpression = _interopRequireDefault(__webpack_require__(747));

exports.prependToMemberExpression = _prependToMemberExpression.default;

var _removeProperties = _interopRequireDefault(__webpack_require__(286));

exports.removeProperties = _removeProperties.default;

var _removePropertiesDeep = _interopRequireDefault(__webpack_require__(284));

exports.removePropertiesDeep = _removePropertiesDeep.default;

var _removeTypeDuplicates = _interopRequireDefault(__webpack_require__(272));

exports.removeTypeDuplicates = _removeTypeDuplicates.default;

var _getBindingIdentifiers = _interopRequireDefault(__webpack_require__(118));

exports.getBindingIdentifiers = _getBindingIdentifiers.default;

var _getOuterBindingIdentifiers = _interopRequireDefault(__webpack_require__(748));

exports.getOuterBindingIdentifiers = _getOuterBindingIdentifiers.default;

var _traverse = _interopRequireDefault(__webpack_require__(749));

exports.traverse = _traverse.default;

var _traverseFast = _interopRequireDefault(__webpack_require__(285));

exports.traverseFast = _traverseFast.default;

var _shallowEqual = _interopRequireDefault(__webpack_require__(258));

exports.shallowEqual = _shallowEqual.default;

var _is = _interopRequireDefault(__webpack_require__(110));

exports.is = _is.default;

var _isBinding = _interopRequireDefault(__webpack_require__(750));

exports.isBinding = _isBinding.default;

var _isBlockScoped = _interopRequireDefault(__webpack_require__(751));

exports.isBlockScoped = _isBlockScoped.default;

var _isImmutable = _interopRequireDefault(__webpack_require__(752));

exports.isImmutable = _isImmutable.default;

var _isLet = _interopRequireDefault(__webpack_require__(287));

exports.isLet = _isLet.default;

var _isNode = _interopRequireDefault(__webpack_require__(271));

exports.isNode = _isNode.default;

var _isNodesEquivalent = _interopRequireDefault(__webpack_require__(753));

exports.isNodesEquivalent = _isNodesEquivalent.default;

var _isReferenced = _interopRequireDefault(__webpack_require__(754));

exports.isReferenced = _isReferenced.default;

var _isScope = _interopRequireDefault(__webpack_require__(755));

exports.isScope = _isScope.default;

var _isSpecifierDefault = _interopRequireDefault(__webpack_require__(756));

exports.isSpecifierDefault = _isSpecifierDefault.default;

var _isType = _interopRequireDefault(__webpack_require__(177));

exports.isType = _isType.default;

var _isValidES3Identifier = _interopRequireDefault(__webpack_require__(757));

exports.isValidES3Identifier = _isValidES3Identifier.default;

var _isValidIdentifier = _interopRequireDefault(__webpack_require__(83));

exports.isValidIdentifier = _isValidIdentifier.default;

var _isVar = _interopRequireDefault(__webpack_require__(758));

exports.isVar = _isVar.default;

var _matchesPattern = _interopRequireDefault(__webpack_require__(257));

exports.matchesPattern = _matchesPattern.default;

var _validate = _interopRequireDefault(__webpack_require__(270));

exports.validate = _validate.default;

var _buildMatchMemberExpression = _interopRequireDefault(__webpack_require__(256));

exports.buildMatchMemberExpression = _buildMatchMemberExpression.default;

var _generated4 = __webpack_require__(17);

Object.keys(_generated4).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  exports[key] = _generated4[key];
});

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var react = {
  isReactComponent: _isReactComponent.default,
  isCompatTag: _isCompatTag.default,
  buildChildren: _buildChildren.default
};
exports.react = react;

/***/ }),
/* 29 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.arrayExpression = exports.ArrayExpression = ArrayExpression;
exports.assignmentExpression = exports.AssignmentExpression = AssignmentExpression;
exports.binaryExpression = exports.BinaryExpression = BinaryExpression;
exports.directive = exports.Directive = Directive;
exports.directiveLiteral = exports.DirectiveLiteral = DirectiveLiteral;
exports.blockStatement = exports.BlockStatement = BlockStatement;
exports.breakStatement = exports.BreakStatement = BreakStatement;
exports.callExpression = exports.CallExpression = CallExpression;
exports.catchClause = exports.CatchClause = CatchClause;
exports.conditionalExpression = exports.ConditionalExpression = ConditionalExpression;
exports.continueStatement = exports.ContinueStatement = ContinueStatement;
exports.debuggerStatement = exports.DebuggerStatement = DebuggerStatement;
exports.doWhileStatement = exports.DoWhileStatement = DoWhileStatement;
exports.emptyStatement = exports.EmptyStatement = EmptyStatement;
exports.expressionStatement = exports.ExpressionStatement = ExpressionStatement;
exports.file = exports.File = File;
exports.forInStatement = exports.ForInStatement = ForInStatement;
exports.forStatement = exports.ForStatement = ForStatement;
exports.functionDeclaration = exports.FunctionDeclaration = FunctionDeclaration;
exports.functionExpression = exports.FunctionExpression = FunctionExpression;
exports.identifier = exports.Identifier = Identifier;
exports.ifStatement = exports.IfStatement = IfStatement;
exports.labeledStatement = exports.LabeledStatement = LabeledStatement;
exports.stringLiteral = exports.StringLiteral = StringLiteral;
exports.numericLiteral = exports.NumericLiteral = NumericLiteral;
exports.nullLiteral = exports.NullLiteral = NullLiteral;
exports.booleanLiteral = exports.BooleanLiteral = BooleanLiteral;
exports.regExpLiteral = exports.RegExpLiteral = RegExpLiteral;
exports.logicalExpression = exports.LogicalExpression = LogicalExpression;
exports.memberExpression = exports.MemberExpression = MemberExpression;
exports.newExpression = exports.NewExpression = NewExpression;
exports.program = exports.Program = Program;
exports.objectExpression = exports.ObjectExpression = ObjectExpression;
exports.objectMethod = exports.ObjectMethod = ObjectMethod;
exports.objectProperty = exports.ObjectProperty = ObjectProperty;
exports.restElement = exports.RestElement = RestElement;
exports.returnStatement = exports.ReturnStatement = ReturnStatement;
exports.sequenceExpression = exports.SequenceExpression = SequenceExpression;
exports.switchCase = exports.SwitchCase = SwitchCase;
exports.switchStatement = exports.SwitchStatement = SwitchStatement;
exports.thisExpression = exports.ThisExpression = ThisExpression;
exports.throwStatement = exports.ThrowStatement = ThrowStatement;
exports.tryStatement = exports.TryStatement = TryStatement;
exports.unaryExpression = exports.UnaryExpression = UnaryExpression;
exports.updateExpression = exports.UpdateExpression = UpdateExpression;
exports.variableDeclaration = exports.VariableDeclaration = VariableDeclaration;
exports.variableDeclarator = exports.VariableDeclarator = VariableDeclarator;
exports.whileStatement = exports.WhileStatement = WhileStatement;
exports.withStatement = exports.WithStatement = WithStatement;
exports.assignmentPattern = exports.AssignmentPattern = AssignmentPattern;
exports.arrayPattern = exports.ArrayPattern = ArrayPattern;
exports.arrowFunctionExpression = exports.ArrowFunctionExpression = ArrowFunctionExpression;
exports.classBody = exports.ClassBody = ClassBody;
exports.classDeclaration = exports.ClassDeclaration = ClassDeclaration;
exports.classExpression = exports.ClassExpression = ClassExpression;
exports.exportAllDeclaration = exports.ExportAllDeclaration = ExportAllDeclaration;
exports.exportDefaultDeclaration = exports.ExportDefaultDeclaration = ExportDefaultDeclaration;
exports.exportNamedDeclaration = exports.ExportNamedDeclaration = ExportNamedDeclaration;
exports.exportSpecifier = exports.ExportSpecifier = ExportSpecifier;
exports.forOfStatement = exports.ForOfStatement = ForOfStatement;
exports.importDeclaration = exports.ImportDeclaration = ImportDeclaration;
exports.importDefaultSpecifier = exports.ImportDefaultSpecifier = ImportDefaultSpecifier;
exports.importNamespaceSpecifier = exports.ImportNamespaceSpecifier = ImportNamespaceSpecifier;
exports.importSpecifier = exports.ImportSpecifier = ImportSpecifier;
exports.metaProperty = exports.MetaProperty = MetaProperty;
exports.classMethod = exports.ClassMethod = ClassMethod;
exports.objectPattern = exports.ObjectPattern = ObjectPattern;
exports.spreadElement = exports.SpreadElement = SpreadElement;
exports.super = exports.Super = Super;
exports.taggedTemplateExpression = exports.TaggedTemplateExpression = TaggedTemplateExpression;
exports.templateElement = exports.TemplateElement = TemplateElement;
exports.templateLiteral = exports.TemplateLiteral = TemplateLiteral;
exports.yieldExpression = exports.YieldExpression = YieldExpression;
exports.anyTypeAnnotation = exports.AnyTypeAnnotation = AnyTypeAnnotation;
exports.arrayTypeAnnotation = exports.ArrayTypeAnnotation = ArrayTypeAnnotation;
exports.booleanTypeAnnotation = exports.BooleanTypeAnnotation = BooleanTypeAnnotation;
exports.booleanLiteralTypeAnnotation = exports.BooleanLiteralTypeAnnotation = BooleanLiteralTypeAnnotation;
exports.nullLiteralTypeAnnotation = exports.NullLiteralTypeAnnotation = NullLiteralTypeAnnotation;
exports.classImplements = exports.ClassImplements = ClassImplements;
exports.declareClass = exports.DeclareClass = DeclareClass;
exports.declareFunction = exports.DeclareFunction = DeclareFunction;
exports.declareInterface = exports.DeclareInterface = DeclareInterface;
exports.declareModule = exports.DeclareModule = DeclareModule;
exports.declareModuleExports = exports.DeclareModuleExports = DeclareModuleExports;
exports.declareTypeAlias = exports.DeclareTypeAlias = DeclareTypeAlias;
exports.declareOpaqueType = exports.DeclareOpaqueType = DeclareOpaqueType;
exports.declareVariable = exports.DeclareVariable = DeclareVariable;
exports.declareExportDeclaration = exports.DeclareExportDeclaration = DeclareExportDeclaration;
exports.declareExportAllDeclaration = exports.DeclareExportAllDeclaration = DeclareExportAllDeclaration;
exports.declaredPredicate = exports.DeclaredPredicate = DeclaredPredicate;
exports.existsTypeAnnotation = exports.ExistsTypeAnnotation = ExistsTypeAnnotation;
exports.functionTypeAnnotation = exports.FunctionTypeAnnotation = FunctionTypeAnnotation;
exports.functionTypeParam = exports.FunctionTypeParam = FunctionTypeParam;
exports.genericTypeAnnotation = exports.GenericTypeAnnotation = GenericTypeAnnotation;
exports.inferredPredicate = exports.InferredPredicate = InferredPredicate;
exports.interfaceExtends = exports.InterfaceExtends = InterfaceExtends;
exports.interfaceDeclaration = exports.InterfaceDeclaration = InterfaceDeclaration;
exports.intersectionTypeAnnotation = exports.IntersectionTypeAnnotation = IntersectionTypeAnnotation;
exports.mixedTypeAnnotation = exports.MixedTypeAnnotation = MixedTypeAnnotation;
exports.emptyTypeAnnotation = exports.EmptyTypeAnnotation = EmptyTypeAnnotation;
exports.nullableTypeAnnotation = exports.NullableTypeAnnotation = NullableTypeAnnotation;
exports.numberLiteralTypeAnnotation = exports.NumberLiteralTypeAnnotation = NumberLiteralTypeAnnotation;
exports.numberTypeAnnotation = exports.NumberTypeAnnotation = NumberTypeAnnotation;
exports.objectTypeAnnotation = exports.ObjectTypeAnnotation = ObjectTypeAnnotation;
exports.objectTypeCallProperty = exports.ObjectTypeCallProperty = ObjectTypeCallProperty;
exports.objectTypeIndexer = exports.ObjectTypeIndexer = ObjectTypeIndexer;
exports.objectTypeProperty = exports.ObjectTypeProperty = ObjectTypeProperty;
exports.objectTypeSpreadProperty = exports.ObjectTypeSpreadProperty = ObjectTypeSpreadProperty;
exports.opaqueType = exports.OpaqueType = OpaqueType;
exports.qualifiedTypeIdentifier = exports.QualifiedTypeIdentifier = QualifiedTypeIdentifier;
exports.stringLiteralTypeAnnotation = exports.StringLiteralTypeAnnotation = StringLiteralTypeAnnotation;
exports.stringTypeAnnotation = exports.StringTypeAnnotation = StringTypeAnnotation;
exports.thisTypeAnnotation = exports.ThisTypeAnnotation = ThisTypeAnnotation;
exports.tupleTypeAnnotation = exports.TupleTypeAnnotation = TupleTypeAnnotation;
exports.typeofTypeAnnotation = exports.TypeofTypeAnnotation = TypeofTypeAnnotation;
exports.typeAlias = exports.TypeAlias = TypeAlias;
exports.typeAnnotation = exports.TypeAnnotation = TypeAnnotation;
exports.typeCastExpression = exports.TypeCastExpression = TypeCastExpression;
exports.typeParameter = exports.TypeParameter = TypeParameter;
exports.typeParameterDeclaration = exports.TypeParameterDeclaration = TypeParameterDeclaration;
exports.typeParameterInstantiation = exports.TypeParameterInstantiation = TypeParameterInstantiation;
exports.unionTypeAnnotation = exports.UnionTypeAnnotation = UnionTypeAnnotation;
exports.variance = exports.Variance = Variance;
exports.voidTypeAnnotation = exports.VoidTypeAnnotation = VoidTypeAnnotation;
exports.jSXAttribute = exports.jsxAttribute = exports.JSXAttribute = JSXAttribute;
exports.jSXClosingElement = exports.jsxClosingElement = exports.JSXClosingElement = JSXClosingElement;
exports.jSXElement = exports.jsxElement = exports.JSXElement = JSXElement;
exports.jSXEmptyExpression = exports.jsxEmptyExpression = exports.JSXEmptyExpression = JSXEmptyExpression;
exports.jSXExpressionContainer = exports.jsxExpressionContainer = exports.JSXExpressionContainer = JSXExpressionContainer;
exports.jSXSpreadChild = exports.jsxSpreadChild = exports.JSXSpreadChild = JSXSpreadChild;
exports.jSXIdentifier = exports.jsxIdentifier = exports.JSXIdentifier = JSXIdentifier;
exports.jSXMemberExpression = exports.jsxMemberExpression = exports.JSXMemberExpression = JSXMemberExpression;
exports.jSXNamespacedName = exports.jsxNamespacedName = exports.JSXNamespacedName = JSXNamespacedName;
exports.jSXOpeningElement = exports.jsxOpeningElement = exports.JSXOpeningElement = JSXOpeningElement;
exports.jSXSpreadAttribute = exports.jsxSpreadAttribute = exports.JSXSpreadAttribute = JSXSpreadAttribute;
exports.jSXText = exports.jsxText = exports.JSXText = JSXText;
exports.jSXFragment = exports.jsxFragment = exports.JSXFragment = JSXFragment;
exports.jSXOpeningFragment = exports.jsxOpeningFragment = exports.JSXOpeningFragment = JSXOpeningFragment;
exports.jSXClosingFragment = exports.jsxClosingFragment = exports.JSXClosingFragment = JSXClosingFragment;
exports.noop = exports.Noop = Noop;
exports.parenthesizedExpression = exports.ParenthesizedExpression = ParenthesizedExpression;
exports.awaitExpression = exports.AwaitExpression = AwaitExpression;
exports.bindExpression = exports.BindExpression = BindExpression;
exports.classProperty = exports.ClassProperty = ClassProperty;
exports.import = exports.Import = Import;
exports.decorator = exports.Decorator = Decorator;
exports.doExpression = exports.DoExpression = DoExpression;
exports.exportDefaultSpecifier = exports.ExportDefaultSpecifier = ExportDefaultSpecifier;
exports.exportNamespaceSpecifier = exports.ExportNamespaceSpecifier = ExportNamespaceSpecifier;
exports.tSParameterProperty = exports.tsParameterProperty = exports.TSParameterProperty = TSParameterProperty;
exports.tSDeclareFunction = exports.tsDeclareFunction = exports.TSDeclareFunction = TSDeclareFunction;
exports.tSDeclareMethod = exports.tsDeclareMethod = exports.TSDeclareMethod = TSDeclareMethod;
exports.tSQualifiedName = exports.tsQualifiedName = exports.TSQualifiedName = TSQualifiedName;
exports.tSCallSignatureDeclaration = exports.tsCallSignatureDeclaration = exports.TSCallSignatureDeclaration = TSCallSignatureDeclaration;
exports.tSConstructSignatureDeclaration = exports.tsConstructSignatureDeclaration = exports.TSConstructSignatureDeclaration = TSConstructSignatureDeclaration;
exports.tSPropertySignature = exports.tsPropertySignature = exports.TSPropertySignature = TSPropertySignature;
exports.tSMethodSignature = exports.tsMethodSignature = exports.TSMethodSignature = TSMethodSignature;
exports.tSIndexSignature = exports.tsIndexSignature = exports.TSIndexSignature = TSIndexSignature;
exports.tSAnyKeyword = exports.tsAnyKeyword = exports.TSAnyKeyword = TSAnyKeyword;
exports.tSNumberKeyword = exports.tsNumberKeyword = exports.TSNumberKeyword = TSNumberKeyword;
exports.tSObjectKeyword = exports.tsObjectKeyword = exports.TSObjectKeyword = TSObjectKeyword;
exports.tSBooleanKeyword = exports.tsBooleanKeyword = exports.TSBooleanKeyword = TSBooleanKeyword;
exports.tSStringKeyword = exports.tsStringKeyword = exports.TSStringKeyword = TSStringKeyword;
exports.tSSymbolKeyword = exports.tsSymbolKeyword = exports.TSSymbolKeyword = TSSymbolKeyword;
exports.tSVoidKeyword = exports.tsVoidKeyword = exports.TSVoidKeyword = TSVoidKeyword;
exports.tSUndefinedKeyword = exports.tsUndefinedKeyword = exports.TSUndefinedKeyword = TSUndefinedKeyword;
exports.tSNullKeyword = exports.tsNullKeyword = exports.TSNullKeyword = TSNullKeyword;
exports.tSNeverKeyword = exports.tsNeverKeyword = exports.TSNeverKeyword = TSNeverKeyword;
exports.tSThisType = exports.tsThisType = exports.TSThisType = TSThisType;
exports.tSFunctionType = exports.tsFunctionType = exports.TSFunctionType = TSFunctionType;
exports.tSConstructorType = exports.tsConstructorType = exports.TSConstructorType = TSConstructorType;
exports.tSTypeReference = exports.tsTypeReference = exports.TSTypeReference = TSTypeReference;
exports.tSTypePredicate = exports.tsTypePredicate = exports.TSTypePredicate = TSTypePredicate;
exports.tSTypeQuery = exports.tsTypeQuery = exports.TSTypeQuery = TSTypeQuery;
exports.tSTypeLiteral = exports.tsTypeLiteral = exports.TSTypeLiteral = TSTypeLiteral;
exports.tSArrayType = exports.tsArrayType = exports.TSArrayType = TSArrayType;
exports.tSTupleType = exports.tsTupleType = exports.TSTupleType = TSTupleType;
exports.tSUnionType = exports.tsUnionType = exports.TSUnionType = TSUnionType;
exports.tSIntersectionType = exports.tsIntersectionType = exports.TSIntersectionType = TSIntersectionType;
exports.tSParenthesizedType = exports.tsParenthesizedType = exports.TSParenthesizedType = TSParenthesizedType;
exports.tSTypeOperator = exports.tsTypeOperator = exports.TSTypeOperator = TSTypeOperator;
exports.tSIndexedAccessType = exports.tsIndexedAccessType = exports.TSIndexedAccessType = TSIndexedAccessType;
exports.tSMappedType = exports.tsMappedType = exports.TSMappedType = TSMappedType;
exports.tSLiteralType = exports.tsLiteralType = exports.TSLiteralType = TSLiteralType;
exports.tSExpressionWithTypeArguments = exports.tsExpressionWithTypeArguments = exports.TSExpressionWithTypeArguments = TSExpressionWithTypeArguments;
exports.tSInterfaceDeclaration = exports.tsInterfaceDeclaration = exports.TSInterfaceDeclaration = TSInterfaceDeclaration;
exports.tSInterfaceBody = exports.tsInterfaceBody = exports.TSInterfaceBody = TSInterfaceBody;
exports.tSTypeAliasDeclaration = exports.tsTypeAliasDeclaration = exports.TSTypeAliasDeclaration = TSTypeAliasDeclaration;
exports.tSAsExpression = exports.tsAsExpression = exports.TSAsExpression = TSAsExpression;
exports.tSTypeAssertion = exports.tsTypeAssertion = exports.TSTypeAssertion = TSTypeAssertion;
exports.tSEnumDeclaration = exports.tsEnumDeclaration = exports.TSEnumDeclaration = TSEnumDeclaration;
exports.tSEnumMember = exports.tsEnumMember = exports.TSEnumMember = TSEnumMember;
exports.tSModuleDeclaration = exports.tsModuleDeclaration = exports.TSModuleDeclaration = TSModuleDeclaration;
exports.tSModuleBlock = exports.tsModuleBlock = exports.TSModuleBlock = TSModuleBlock;
exports.tSImportEqualsDeclaration = exports.tsImportEqualsDeclaration = exports.TSImportEqualsDeclaration = TSImportEqualsDeclaration;
exports.tSExternalModuleReference = exports.tsExternalModuleReference = exports.TSExternalModuleReference = TSExternalModuleReference;
exports.tSNonNullExpression = exports.tsNonNullExpression = exports.TSNonNullExpression = TSNonNullExpression;
exports.tSExportAssignment = exports.tsExportAssignment = exports.TSExportAssignment = TSExportAssignment;
exports.tSNamespaceExportDeclaration = exports.tsNamespaceExportDeclaration = exports.TSNamespaceExportDeclaration = TSNamespaceExportDeclaration;
exports.tSTypeAnnotation = exports.tsTypeAnnotation = exports.TSTypeAnnotation = TSTypeAnnotation;
exports.tSTypeParameterInstantiation = exports.tsTypeParameterInstantiation = exports.TSTypeParameterInstantiation = TSTypeParameterInstantiation;
exports.tSTypeParameterDeclaration = exports.tsTypeParameterDeclaration = exports.TSTypeParameterDeclaration = TSTypeParameterDeclaration;
exports.tSTypeParameter = exports.tsTypeParameter = exports.TSTypeParameter = TSTypeParameter;
exports.numberLiteral = exports.NumberLiteral = NumberLiteral;
exports.regexLiteral = exports.RegexLiteral = RegexLiteral;
exports.restProperty = exports.RestProperty = RestProperty;
exports.spreadProperty = exports.SpreadProperty = SpreadProperty;

var _builder = _interopRequireDefault(__webpack_require__(677));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function ArrayExpression() {
  for (var _len = arguments.length, args = new Array(_len), _key = 0; _key < _len; _key++) {
    args[_key] = arguments[_key];
  }

  return _builder.default.apply(void 0, ["ArrayExpression"].concat(args));
}

function AssignmentExpression() {
  for (var _len2 = arguments.length, args = new Array(_len2), _key2 = 0; _key2 < _len2; _key2++) {
    args[_key2] = arguments[_key2];
  }

  return _builder.default.apply(void 0, ["AssignmentExpression"].concat(args));
}

function BinaryExpression() {
  for (var _len3 = arguments.length, args = new Array(_len3), _key3 = 0; _key3 < _len3; _key3++) {
    args[_key3] = arguments[_key3];
  }

  return _builder.default.apply(void 0, ["BinaryExpression"].concat(args));
}

function Directive() {
  for (var _len4 = arguments.length, args = new Array(_len4), _key4 = 0; _key4 < _len4; _key4++) {
    args[_key4] = arguments[_key4];
  }

  return _builder.default.apply(void 0, ["Directive"].concat(args));
}

function DirectiveLiteral() {
  for (var _len5 = arguments.length, args = new Array(_len5), _key5 = 0; _key5 < _len5; _key5++) {
    args[_key5] = arguments[_key5];
  }

  return _builder.default.apply(void 0, ["DirectiveLiteral"].concat(args));
}

function BlockStatement() {
  for (var _len6 = arguments.length, args = new Array(_len6), _key6 = 0; _key6 < _len6; _key6++) {
    args[_key6] = arguments[_key6];
  }

  return _builder.default.apply(void 0, ["BlockStatement"].concat(args));
}

function BreakStatement() {
  for (var _len7 = arguments.length, args = new Array(_len7), _key7 = 0; _key7 < _len7; _key7++) {
    args[_key7] = arguments[_key7];
  }

  return _builder.default.apply(void 0, ["BreakStatement"].concat(args));
}

function CallExpression() {
  for (var _len8 = arguments.length, args = new Array(_len8), _key8 = 0; _key8 < _len8; _key8++) {
    args[_key8] = arguments[_key8];
  }

  return _builder.default.apply(void 0, ["CallExpression"].concat(args));
}

function CatchClause() {
  for (var _len9 = arguments.length, args = new Array(_len9), _key9 = 0; _key9 < _len9; _key9++) {
    args[_key9] = arguments[_key9];
  }

  return _builder.default.apply(void 0, ["CatchClause"].concat(args));
}

function ConditionalExpression() {
  for (var _len10 = arguments.length, args = new Array(_len10), _key10 = 0; _key10 < _len10; _key10++) {
    args[_key10] = arguments[_key10];
  }

  return _builder.default.apply(void 0, ["ConditionalExpression"].concat(args));
}

function ContinueStatement() {
  for (var _len11 = arguments.length, args = new Array(_len11), _key11 = 0; _key11 < _len11; _key11++) {
    args[_key11] = arguments[_key11];
  }

  return _builder.default.apply(void 0, ["ContinueStatement"].concat(args));
}

function DebuggerStatement() {
  for (var _len12 = arguments.length, args = new Array(_len12), _key12 = 0; _key12 < _len12; _key12++) {
    args[_key12] = arguments[_key12];
  }

  return _builder.default.apply(void 0, ["DebuggerStatement"].concat(args));
}

function DoWhileStatement() {
  for (var _len13 = arguments.length, args = new Array(_len13), _key13 = 0; _key13 < _len13; _key13++) {
    args[_key13] = arguments[_key13];
  }

  return _builder.default.apply(void 0, ["DoWhileStatement"].concat(args));
}

function EmptyStatement() {
  for (var _len14 = arguments.length, args = new Array(_len14), _key14 = 0; _key14 < _len14; _key14++) {
    args[_key14] = arguments[_key14];
  }

  return _builder.default.apply(void 0, ["EmptyStatement"].concat(args));
}

function ExpressionStatement() {
  for (var _len15 = arguments.length, args = new Array(_len15), _key15 = 0; _key15 < _len15; _key15++) {
    args[_key15] = arguments[_key15];
  }

  return _builder.default.apply(void 0, ["ExpressionStatement"].concat(args));
}

function File() {
  for (var _len16 = arguments.length, args = new Array(_len16), _key16 = 0; _key16 < _len16; _key16++) {
    args[_key16] = arguments[_key16];
  }

  return _builder.default.apply(void 0, ["File"].concat(args));
}

function ForInStatement() {
  for (var _len17 = arguments.length, args = new Array(_len17), _key17 = 0; _key17 < _len17; _key17++) {
    args[_key17] = arguments[_key17];
  }

  return _builder.default.apply(void 0, ["ForInStatement"].concat(args));
}

function ForStatement() {
  for (var _len18 = arguments.length, args = new Array(_len18), _key18 = 0; _key18 < _len18; _key18++) {
    args[_key18] = arguments[_key18];
  }

  return _builder.default.apply(void 0, ["ForStatement"].concat(args));
}

function FunctionDeclaration() {
  for (var _len19 = arguments.length, args = new Array(_len19), _key19 = 0; _key19 < _len19; _key19++) {
    args[_key19] = arguments[_key19];
  }

  return _builder.default.apply(void 0, ["FunctionDeclaration"].concat(args));
}

function FunctionExpression() {
  for (var _len20 = arguments.length, args = new Array(_len20), _key20 = 0; _key20 < _len20; _key20++) {
    args[_key20] = arguments[_key20];
  }

  return _builder.default.apply(void 0, ["FunctionExpression"].concat(args));
}

function Identifier() {
  for (var _len21 = arguments.length, args = new Array(_len21), _key21 = 0; _key21 < _len21; _key21++) {
    args[_key21] = arguments[_key21];
  }

  return _builder.default.apply(void 0, ["Identifier"].concat(args));
}

function IfStatement() {
  for (var _len22 = arguments.length, args = new Array(_len22), _key22 = 0; _key22 < _len22; _key22++) {
    args[_key22] = arguments[_key22];
  }

  return _builder.default.apply(void 0, ["IfStatement"].concat(args));
}

function LabeledStatement() {
  for (var _len23 = arguments.length, args = new Array(_len23), _key23 = 0; _key23 < _len23; _key23++) {
    args[_key23] = arguments[_key23];
  }

  return _builder.default.apply(void 0, ["LabeledStatement"].concat(args));
}

function StringLiteral() {
  for (var _len24 = arguments.length, args = new Array(_len24), _key24 = 0; _key24 < _len24; _key24++) {
    args[_key24] = arguments[_key24];
  }

  return _builder.default.apply(void 0, ["StringLiteral"].concat(args));
}

function NumericLiteral() {
  for (var _len25 = arguments.length, args = new Array(_len25), _key25 = 0; _key25 < _len25; _key25++) {
    args[_key25] = arguments[_key25];
  }

  return _builder.default.apply(void 0, ["NumericLiteral"].concat(args));
}

function NullLiteral() {
  for (var _len26 = arguments.length, args = new Array(_len26), _key26 = 0; _key26 < _len26; _key26++) {
    args[_key26] = arguments[_key26];
  }

  return _builder.default.apply(void 0, ["NullLiteral"].concat(args));
}

function BooleanLiteral() {
  for (var _len27 = arguments.length, args = new Array(_len27), _key27 = 0; _key27 < _len27; _key27++) {
    args[_key27] = arguments[_key27];
  }

  return _builder.default.apply(void 0, ["BooleanLiteral"].concat(args));
}

function RegExpLiteral() {
  for (var _len28 = arguments.length, args = new Array(_len28), _key28 = 0; _key28 < _len28; _key28++) {
    args[_key28] = arguments[_key28];
  }

  return _builder.default.apply(void 0, ["RegExpLiteral"].concat(args));
}

function LogicalExpression() {
  for (var _len29 = arguments.length, args = new Array(_len29), _key29 = 0; _key29 < _len29; _key29++) {
    args[_key29] = arguments[_key29];
  }

  return _builder.default.apply(void 0, ["LogicalExpression"].concat(args));
}

function MemberExpression() {
  for (var _len30 = arguments.length, args = new Array(_len30), _key30 = 0; _key30 < _len30; _key30++) {
    args[_key30] = arguments[_key30];
  }

  return _builder.default.apply(void 0, ["MemberExpression"].concat(args));
}

function NewExpression() {
  for (var _len31 = arguments.length, args = new Array(_len31), _key31 = 0; _key31 < _len31; _key31++) {
    args[_key31] = arguments[_key31];
  }

  return _builder.default.apply(void 0, ["NewExpression"].concat(args));
}

function Program() {
  for (var _len32 = arguments.length, args = new Array(_len32), _key32 = 0; _key32 < _len32; _key32++) {
    args[_key32] = arguments[_key32];
  }

  return _builder.default.apply(void 0, ["Program"].concat(args));
}

function ObjectExpression() {
  for (var _len33 = arguments.length, args = new Array(_len33), _key33 = 0; _key33 < _len33; _key33++) {
    args[_key33] = arguments[_key33];
  }

  return _builder.default.apply(void 0, ["ObjectExpression"].concat(args));
}

function ObjectMethod() {
  for (var _len34 = arguments.length, args = new Array(_len34), _key34 = 0; _key34 < _len34; _key34++) {
    args[_key34] = arguments[_key34];
  }

  return _builder.default.apply(void 0, ["ObjectMethod"].concat(args));
}

function ObjectProperty() {
  for (var _len35 = arguments.length, args = new Array(_len35), _key35 = 0; _key35 < _len35; _key35++) {
    args[_key35] = arguments[_key35];
  }

  return _builder.default.apply(void 0, ["ObjectProperty"].concat(args));
}

function RestElement() {
  for (var _len36 = arguments.length, args = new Array(_len36), _key36 = 0; _key36 < _len36; _key36++) {
    args[_key36] = arguments[_key36];
  }

  return _builder.default.apply(void 0, ["RestElement"].concat(args));
}

function ReturnStatement() {
  for (var _len37 = arguments.length, args = new Array(_len37), _key37 = 0; _key37 < _len37; _key37++) {
    args[_key37] = arguments[_key37];
  }

  return _builder.default.apply(void 0, ["ReturnStatement"].concat(args));
}

function SequenceExpression() {
  for (var _len38 = arguments.length, args = new Array(_len38), _key38 = 0; _key38 < _len38; _key38++) {
    args[_key38] = arguments[_key38];
  }

  return _builder.default.apply(void 0, ["SequenceExpression"].concat(args));
}

function SwitchCase() {
  for (var _len39 = arguments.length, args = new Array(_len39), _key39 = 0; _key39 < _len39; _key39++) {
    args[_key39] = arguments[_key39];
  }

  return _builder.default.apply(void 0, ["SwitchCase"].concat(args));
}

function SwitchStatement() {
  for (var _len40 = arguments.length, args = new Array(_len40), _key40 = 0; _key40 < _len40; _key40++) {
    args[_key40] = arguments[_key40];
  }

  return _builder.default.apply(void 0, ["SwitchStatement"].concat(args));
}

function ThisExpression() {
  for (var _len41 = arguments.length, args = new Array(_len41), _key41 = 0; _key41 < _len41; _key41++) {
    args[_key41] = arguments[_key41];
  }

  return _builder.default.apply(void 0, ["ThisExpression"].concat(args));
}

function ThrowStatement() {
  for (var _len42 = arguments.length, args = new Array(_len42), _key42 = 0; _key42 < _len42; _key42++) {
    args[_key42] = arguments[_key42];
  }

  return _builder.default.apply(void 0, ["ThrowStatement"].concat(args));
}

function TryStatement() {
  for (var _len43 = arguments.length, args = new Array(_len43), _key43 = 0; _key43 < _len43; _key43++) {
    args[_key43] = arguments[_key43];
  }

  return _builder.default.apply(void 0, ["TryStatement"].concat(args));
}

function UnaryExpression() {
  for (var _len44 = arguments.length, args = new Array(_len44), _key44 = 0; _key44 < _len44; _key44++) {
    args[_key44] = arguments[_key44];
  }

  return _builder.default.apply(void 0, ["UnaryExpression"].concat(args));
}

function UpdateExpression() {
  for (var _len45 = arguments.length, args = new Array(_len45), _key45 = 0; _key45 < _len45; _key45++) {
    args[_key45] = arguments[_key45];
  }

  return _builder.default.apply(void 0, ["UpdateExpression"].concat(args));
}

function VariableDeclaration() {
  for (var _len46 = arguments.length, args = new Array(_len46), _key46 = 0; _key46 < _len46; _key46++) {
    args[_key46] = arguments[_key46];
  }

  return _builder.default.apply(void 0, ["VariableDeclaration"].concat(args));
}

function VariableDeclarator() {
  for (var _len47 = arguments.length, args = new Array(_len47), _key47 = 0; _key47 < _len47; _key47++) {
    args[_key47] = arguments[_key47];
  }

  return _builder.default.apply(void 0, ["VariableDeclarator"].concat(args));
}

function WhileStatement() {
  for (var _len48 = arguments.length, args = new Array(_len48), _key48 = 0; _key48 < _len48; _key48++) {
    args[_key48] = arguments[_key48];
  }

  return _builder.default.apply(void 0, ["WhileStatement"].concat(args));
}

function WithStatement() {
  for (var _len49 = arguments.length, args = new Array(_len49), _key49 = 0; _key49 < _len49; _key49++) {
    args[_key49] = arguments[_key49];
  }

  return _builder.default.apply(void 0, ["WithStatement"].concat(args));
}

function AssignmentPattern() {
  for (var _len50 = arguments.length, args = new Array(_len50), _key50 = 0; _key50 < _len50; _key50++) {
    args[_key50] = arguments[_key50];
  }

  return _builder.default.apply(void 0, ["AssignmentPattern"].concat(args));
}

function ArrayPattern() {
  for (var _len51 = arguments.length, args = new Array(_len51), _key51 = 0; _key51 < _len51; _key51++) {
    args[_key51] = arguments[_key51];
  }

  return _builder.default.apply(void 0, ["ArrayPattern"].concat(args));
}

function ArrowFunctionExpression() {
  for (var _len52 = arguments.length, args = new Array(_len52), _key52 = 0; _key52 < _len52; _key52++) {
    args[_key52] = arguments[_key52];
  }

  return _builder.default.apply(void 0, ["ArrowFunctionExpression"].concat(args));
}

function ClassBody() {
  for (var _len53 = arguments.length, args = new Array(_len53), _key53 = 0; _key53 < _len53; _key53++) {
    args[_key53] = arguments[_key53];
  }

  return _builder.default.apply(void 0, ["ClassBody"].concat(args));
}

function ClassDeclaration() {
  for (var _len54 = arguments.length, args = new Array(_len54), _key54 = 0; _key54 < _len54; _key54++) {
    args[_key54] = arguments[_key54];
  }

  return _builder.default.apply(void 0, ["ClassDeclaration"].concat(args));
}

function ClassExpression() {
  for (var _len55 = arguments.length, args = new Array(_len55), _key55 = 0; _key55 < _len55; _key55++) {
    args[_key55] = arguments[_key55];
  }

  return _builder.default.apply(void 0, ["ClassExpression"].concat(args));
}

function ExportAllDeclaration() {
  for (var _len56 = arguments.length, args = new Array(_len56), _key56 = 0; _key56 < _len56; _key56++) {
    args[_key56] = arguments[_key56];
  }

  return _builder.default.apply(void 0, ["ExportAllDeclaration"].concat(args));
}

function ExportDefaultDeclaration() {
  for (var _len57 = arguments.length, args = new Array(_len57), _key57 = 0; _key57 < _len57; _key57++) {
    args[_key57] = arguments[_key57];
  }

  return _builder.default.apply(void 0, ["ExportDefaultDeclaration"].concat(args));
}

function ExportNamedDeclaration() {
  for (var _len58 = arguments.length, args = new Array(_len58), _key58 = 0; _key58 < _len58; _key58++) {
    args[_key58] = arguments[_key58];
  }

  return _builder.default.apply(void 0, ["ExportNamedDeclaration"].concat(args));
}

function ExportSpecifier() {
  for (var _len59 = arguments.length, args = new Array(_len59), _key59 = 0; _key59 < _len59; _key59++) {
    args[_key59] = arguments[_key59];
  }

  return _builder.default.apply(void 0, ["ExportSpecifier"].concat(args));
}

function ForOfStatement() {
  for (var _len60 = arguments.length, args = new Array(_len60), _key60 = 0; _key60 < _len60; _key60++) {
    args[_key60] = arguments[_key60];
  }

  return _builder.default.apply(void 0, ["ForOfStatement"].concat(args));
}

function ImportDeclaration() {
  for (var _len61 = arguments.length, args = new Array(_len61), _key61 = 0; _key61 < _len61; _key61++) {
    args[_key61] = arguments[_key61];
  }

  return _builder.default.apply(void 0, ["ImportDeclaration"].concat(args));
}

function ImportDefaultSpecifier() {
  for (var _len62 = arguments.length, args = new Array(_len62), _key62 = 0; _key62 < _len62; _key62++) {
    args[_key62] = arguments[_key62];
  }

  return _builder.default.apply(void 0, ["ImportDefaultSpecifier"].concat(args));
}

function ImportNamespaceSpecifier() {
  for (var _len63 = arguments.length, args = new Array(_len63), _key63 = 0; _key63 < _len63; _key63++) {
    args[_key63] = arguments[_key63];
  }

  return _builder.default.apply(void 0, ["ImportNamespaceSpecifier"].concat(args));
}

function ImportSpecifier() {
  for (var _len64 = arguments.length, args = new Array(_len64), _key64 = 0; _key64 < _len64; _key64++) {
    args[_key64] = arguments[_key64];
  }

  return _builder.default.apply(void 0, ["ImportSpecifier"].concat(args));
}

function MetaProperty() {
  for (var _len65 = arguments.length, args = new Array(_len65), _key65 = 0; _key65 < _len65; _key65++) {
    args[_key65] = arguments[_key65];
  }

  return _builder.default.apply(void 0, ["MetaProperty"].concat(args));
}

function ClassMethod() {
  for (var _len66 = arguments.length, args = new Array(_len66), _key66 = 0; _key66 < _len66; _key66++) {
    args[_key66] = arguments[_key66];
  }

  return _builder.default.apply(void 0, ["ClassMethod"].concat(args));
}

function ObjectPattern() {
  for (var _len67 = arguments.length, args = new Array(_len67), _key67 = 0; _key67 < _len67; _key67++) {
    args[_key67] = arguments[_key67];
  }

  return _builder.default.apply(void 0, ["ObjectPattern"].concat(args));
}

function SpreadElement() {
  for (var _len68 = arguments.length, args = new Array(_len68), _key68 = 0; _key68 < _len68; _key68++) {
    args[_key68] = arguments[_key68];
  }

  return _builder.default.apply(void 0, ["SpreadElement"].concat(args));
}

function Super() {
  for (var _len69 = arguments.length, args = new Array(_len69), _key69 = 0; _key69 < _len69; _key69++) {
    args[_key69] = arguments[_key69];
  }

  return _builder.default.apply(void 0, ["Super"].concat(args));
}

function TaggedTemplateExpression() {
  for (var _len70 = arguments.length, args = new Array(_len70), _key70 = 0; _key70 < _len70; _key70++) {
    args[_key70] = arguments[_key70];
  }

  return _builder.default.apply(void 0, ["TaggedTemplateExpression"].concat(args));
}

function TemplateElement() {
  for (var _len71 = arguments.length, args = new Array(_len71), _key71 = 0; _key71 < _len71; _key71++) {
    args[_key71] = arguments[_key71];
  }

  return _builder.default.apply(void 0, ["TemplateElement"].concat(args));
}

function TemplateLiteral() {
  for (var _len72 = arguments.length, args = new Array(_len72), _key72 = 0; _key72 < _len72; _key72++) {
    args[_key72] = arguments[_key72];
  }

  return _builder.default.apply(void 0, ["TemplateLiteral"].concat(args));
}

function YieldExpression() {
  for (var _len73 = arguments.length, args = new Array(_len73), _key73 = 0; _key73 < _len73; _key73++) {
    args[_key73] = arguments[_key73];
  }

  return _builder.default.apply(void 0, ["YieldExpression"].concat(args));
}

function AnyTypeAnnotation() {
  for (var _len74 = arguments.length, args = new Array(_len74), _key74 = 0; _key74 < _len74; _key74++) {
    args[_key74] = arguments[_key74];
  }

  return _builder.default.apply(void 0, ["AnyTypeAnnotation"].concat(args));
}

function ArrayTypeAnnotation() {
  for (var _len75 = arguments.length, args = new Array(_len75), _key75 = 0; _key75 < _len75; _key75++) {
    args[_key75] = arguments[_key75];
  }

  return _builder.default.apply(void 0, ["ArrayTypeAnnotation"].concat(args));
}

function BooleanTypeAnnotation() {
  for (var _len76 = arguments.length, args = new Array(_len76), _key76 = 0; _key76 < _len76; _key76++) {
    args[_key76] = arguments[_key76];
  }

  return _builder.default.apply(void 0, ["BooleanTypeAnnotation"].concat(args));
}

function BooleanLiteralTypeAnnotation() {
  for (var _len77 = arguments.length, args = new Array(_len77), _key77 = 0; _key77 < _len77; _key77++) {
    args[_key77] = arguments[_key77];
  }

  return _builder.default.apply(void 0, ["BooleanLiteralTypeAnnotation"].concat(args));
}

function NullLiteralTypeAnnotation() {
  for (var _len78 = arguments.length, args = new Array(_len78), _key78 = 0; _key78 < _len78; _key78++) {
    args[_key78] = arguments[_key78];
  }

  return _builder.default.apply(void 0, ["NullLiteralTypeAnnotation"].concat(args));
}

function ClassImplements() {
  for (var _len79 = arguments.length, args = new Array(_len79), _key79 = 0; _key79 < _len79; _key79++) {
    args[_key79] = arguments[_key79];
  }

  return _builder.default.apply(void 0, ["ClassImplements"].concat(args));
}

function DeclareClass() {
  for (var _len80 = arguments.length, args = new Array(_len80), _key80 = 0; _key80 < _len80; _key80++) {
    args[_key80] = arguments[_key80];
  }

  return _builder.default.apply(void 0, ["DeclareClass"].concat(args));
}

function DeclareFunction() {
  for (var _len81 = arguments.length, args = new Array(_len81), _key81 = 0; _key81 < _len81; _key81++) {
    args[_key81] = arguments[_key81];
  }

  return _builder.default.apply(void 0, ["DeclareFunction"].concat(args));
}

function DeclareInterface() {
  for (var _len82 = arguments.length, args = new Array(_len82), _key82 = 0; _key82 < _len82; _key82++) {
    args[_key82] = arguments[_key82];
  }

  return _builder.default.apply(void 0, ["DeclareInterface"].concat(args));
}

function DeclareModule() {
  for (var _len83 = arguments.length, args = new Array(_len83), _key83 = 0; _key83 < _len83; _key83++) {
    args[_key83] = arguments[_key83];
  }

  return _builder.default.apply(void 0, ["DeclareModule"].concat(args));
}

function DeclareModuleExports() {
  for (var _len84 = arguments.length, args = new Array(_len84), _key84 = 0; _key84 < _len84; _key84++) {
    args[_key84] = arguments[_key84];
  }

  return _builder.default.apply(void 0, ["DeclareModuleExports"].concat(args));
}

function DeclareTypeAlias() {
  for (var _len85 = arguments.length, args = new Array(_len85), _key85 = 0; _key85 < _len85; _key85++) {
    args[_key85] = arguments[_key85];
  }

  return _builder.default.apply(void 0, ["DeclareTypeAlias"].concat(args));
}

function DeclareOpaqueType() {
  for (var _len86 = arguments.length, args = new Array(_len86), _key86 = 0; _key86 < _len86; _key86++) {
    args[_key86] = arguments[_key86];
  }

  return _builder.default.apply(void 0, ["DeclareOpaqueType"].concat(args));
}

function DeclareVariable() {
  for (var _len87 = arguments.length, args = new Array(_len87), _key87 = 0; _key87 < _len87; _key87++) {
    args[_key87] = arguments[_key87];
  }

  return _builder.default.apply(void 0, ["DeclareVariable"].concat(args));
}

function DeclareExportDeclaration() {
  for (var _len88 = arguments.length, args = new Array(_len88), _key88 = 0; _key88 < _len88; _key88++) {
    args[_key88] = arguments[_key88];
  }

  return _builder.default.apply(void 0, ["DeclareExportDeclaration"].concat(args));
}

function DeclareExportAllDeclaration() {
  for (var _len89 = arguments.length, args = new Array(_len89), _key89 = 0; _key89 < _len89; _key89++) {
    args[_key89] = arguments[_key89];
  }

  return _builder.default.apply(void 0, ["DeclareExportAllDeclaration"].concat(args));
}

function DeclaredPredicate() {
  for (var _len90 = arguments.length, args = new Array(_len90), _key90 = 0; _key90 < _len90; _key90++) {
    args[_key90] = arguments[_key90];
  }

  return _builder.default.apply(void 0, ["DeclaredPredicate"].concat(args));
}

function ExistsTypeAnnotation() {
  for (var _len91 = arguments.length, args = new Array(_len91), _key91 = 0; _key91 < _len91; _key91++) {
    args[_key91] = arguments[_key91];
  }

  return _builder.default.apply(void 0, ["ExistsTypeAnnotation"].concat(args));
}

function FunctionTypeAnnotation() {
  for (var _len92 = arguments.length, args = new Array(_len92), _key92 = 0; _key92 < _len92; _key92++) {
    args[_key92] = arguments[_key92];
  }

  return _builder.default.apply(void 0, ["FunctionTypeAnnotation"].concat(args));
}

function FunctionTypeParam() {
  for (var _len93 = arguments.length, args = new Array(_len93), _key93 = 0; _key93 < _len93; _key93++) {
    args[_key93] = arguments[_key93];
  }

  return _builder.default.apply(void 0, ["FunctionTypeParam"].concat(args));
}

function GenericTypeAnnotation() {
  for (var _len94 = arguments.length, args = new Array(_len94), _key94 = 0; _key94 < _len94; _key94++) {
    args[_key94] = arguments[_key94];
  }

  return _builder.default.apply(void 0, ["GenericTypeAnnotation"].concat(args));
}

function InferredPredicate() {
  for (var _len95 = arguments.length, args = new Array(_len95), _key95 = 0; _key95 < _len95; _key95++) {
    args[_key95] = arguments[_key95];
  }

  return _builder.default.apply(void 0, ["InferredPredicate"].concat(args));
}

function InterfaceExtends() {
  for (var _len96 = arguments.length, args = new Array(_len96), _key96 = 0; _key96 < _len96; _key96++) {
    args[_key96] = arguments[_key96];
  }

  return _builder.default.apply(void 0, ["InterfaceExtends"].concat(args));
}

function InterfaceDeclaration() {
  for (var _len97 = arguments.length, args = new Array(_len97), _key97 = 0; _key97 < _len97; _key97++) {
    args[_key97] = arguments[_key97];
  }

  return _builder.default.apply(void 0, ["InterfaceDeclaration"].concat(args));
}

function IntersectionTypeAnnotation() {
  for (var _len98 = arguments.length, args = new Array(_len98), _key98 = 0; _key98 < _len98; _key98++) {
    args[_key98] = arguments[_key98];
  }

  return _builder.default.apply(void 0, ["IntersectionTypeAnnotation"].concat(args));
}

function MixedTypeAnnotation() {
  for (var _len99 = arguments.length, args = new Array(_len99), _key99 = 0; _key99 < _len99; _key99++) {
    args[_key99] = arguments[_key99];
  }

  return _builder.default.apply(void 0, ["MixedTypeAnnotation"].concat(args));
}

function EmptyTypeAnnotation() {
  for (var _len100 = arguments.length, args = new Array(_len100), _key100 = 0; _key100 < _len100; _key100++) {
    args[_key100] = arguments[_key100];
  }

  return _builder.default.apply(void 0, ["EmptyTypeAnnotation"].concat(args));
}

function NullableTypeAnnotation() {
  for (var _len101 = arguments.length, args = new Array(_len101), _key101 = 0; _key101 < _len101; _key101++) {
    args[_key101] = arguments[_key101];
  }

  return _builder.default.apply(void 0, ["NullableTypeAnnotation"].concat(args));
}

function NumberLiteralTypeAnnotation() {
  for (var _len102 = arguments.length, args = new Array(_len102), _key102 = 0; _key102 < _len102; _key102++) {
    args[_key102] = arguments[_key102];
  }

  return _builder.default.apply(void 0, ["NumberLiteralTypeAnnotation"].concat(args));
}

function NumberTypeAnnotation() {
  for (var _len103 = arguments.length, args = new Array(_len103), _key103 = 0; _key103 < _len103; _key103++) {
    args[_key103] = arguments[_key103];
  }

  return _builder.default.apply(void 0, ["NumberTypeAnnotation"].concat(args));
}

function ObjectTypeAnnotation() {
  for (var _len104 = arguments.length, args = new Array(_len104), _key104 = 0; _key104 < _len104; _key104++) {
    args[_key104] = arguments[_key104];
  }

  return _builder.default.apply(void 0, ["ObjectTypeAnnotation"].concat(args));
}

function ObjectTypeCallProperty() {
  for (var _len105 = arguments.length, args = new Array(_len105), _key105 = 0; _key105 < _len105; _key105++) {
    args[_key105] = arguments[_key105];
  }

  return _builder.default.apply(void 0, ["ObjectTypeCallProperty"].concat(args));
}

function ObjectTypeIndexer() {
  for (var _len106 = arguments.length, args = new Array(_len106), _key106 = 0; _key106 < _len106; _key106++) {
    args[_key106] = arguments[_key106];
  }

  return _builder.default.apply(void 0, ["ObjectTypeIndexer"].concat(args));
}

function ObjectTypeProperty() {
  for (var _len107 = arguments.length, args = new Array(_len107), _key107 = 0; _key107 < _len107; _key107++) {
    args[_key107] = arguments[_key107];
  }

  return _builder.default.apply(void 0, ["ObjectTypeProperty"].concat(args));
}

function ObjectTypeSpreadProperty() {
  for (var _len108 = arguments.length, args = new Array(_len108), _key108 = 0; _key108 < _len108; _key108++) {
    args[_key108] = arguments[_key108];
  }

  return _builder.default.apply(void 0, ["ObjectTypeSpreadProperty"].concat(args));
}

function OpaqueType() {
  for (var _len109 = arguments.length, args = new Array(_len109), _key109 = 0; _key109 < _len109; _key109++) {
    args[_key109] = arguments[_key109];
  }

  return _builder.default.apply(void 0, ["OpaqueType"].concat(args));
}

function QualifiedTypeIdentifier() {
  for (var _len110 = arguments.length, args = new Array(_len110), _key110 = 0; _key110 < _len110; _key110++) {
    args[_key110] = arguments[_key110];
  }

  return _builder.default.apply(void 0, ["QualifiedTypeIdentifier"].concat(args));
}

function StringLiteralTypeAnnotation() {
  for (var _len111 = arguments.length, args = new Array(_len111), _key111 = 0; _key111 < _len111; _key111++) {
    args[_key111] = arguments[_key111];
  }

  return _builder.default.apply(void 0, ["StringLiteralTypeAnnotation"].concat(args));
}

function StringTypeAnnotation() {
  for (var _len112 = arguments.length, args = new Array(_len112), _key112 = 0; _key112 < _len112; _key112++) {
    args[_key112] = arguments[_key112];
  }

  return _builder.default.apply(void 0, ["StringTypeAnnotation"].concat(args));
}

function ThisTypeAnnotation() {
  for (var _len113 = arguments.length, args = new Array(_len113), _key113 = 0; _key113 < _len113; _key113++) {
    args[_key113] = arguments[_key113];
  }

  return _builder.default.apply(void 0, ["ThisTypeAnnotation"].concat(args));
}

function TupleTypeAnnotation() {
  for (var _len114 = arguments.length, args = new Array(_len114), _key114 = 0; _key114 < _len114; _key114++) {
    args[_key114] = arguments[_key114];
  }

  return _builder.default.apply(void 0, ["TupleTypeAnnotation"].concat(args));
}

function TypeofTypeAnnotation() {
  for (var _len115 = arguments.length, args = new Array(_len115), _key115 = 0; _key115 < _len115; _key115++) {
    args[_key115] = arguments[_key115];
  }

  return _builder.default.apply(void 0, ["TypeofTypeAnnotation"].concat(args));
}

function TypeAlias() {
  for (var _len116 = arguments.length, args = new Array(_len116), _key116 = 0; _key116 < _len116; _key116++) {
    args[_key116] = arguments[_key116];
  }

  return _builder.default.apply(void 0, ["TypeAlias"].concat(args));
}

function TypeAnnotation() {
  for (var _len117 = arguments.length, args = new Array(_len117), _key117 = 0; _key117 < _len117; _key117++) {
    args[_key117] = arguments[_key117];
  }

  return _builder.default.apply(void 0, ["TypeAnnotation"].concat(args));
}

function TypeCastExpression() {
  for (var _len118 = arguments.length, args = new Array(_len118), _key118 = 0; _key118 < _len118; _key118++) {
    args[_key118] = arguments[_key118];
  }

  return _builder.default.apply(void 0, ["TypeCastExpression"].concat(args));
}

function TypeParameter() {
  for (var _len119 = arguments.length, args = new Array(_len119), _key119 = 0; _key119 < _len119; _key119++) {
    args[_key119] = arguments[_key119];
  }

  return _builder.default.apply(void 0, ["TypeParameter"].concat(args));
}

function TypeParameterDeclaration() {
  for (var _len120 = arguments.length, args = new Array(_len120), _key120 = 0; _key120 < _len120; _key120++) {
    args[_key120] = arguments[_key120];
  }

  return _builder.default.apply(void 0, ["TypeParameterDeclaration"].concat(args));
}

function TypeParameterInstantiation() {
  for (var _len121 = arguments.length, args = new Array(_len121), _key121 = 0; _key121 < _len121; _key121++) {
    args[_key121] = arguments[_key121];
  }

  return _builder.default.apply(void 0, ["TypeParameterInstantiation"].concat(args));
}

function UnionTypeAnnotation() {
  for (var _len122 = arguments.length, args = new Array(_len122), _key122 = 0; _key122 < _len122; _key122++) {
    args[_key122] = arguments[_key122];
  }

  return _builder.default.apply(void 0, ["UnionTypeAnnotation"].concat(args));
}

function Variance() {
  for (var _len123 = arguments.length, args = new Array(_len123), _key123 = 0; _key123 < _len123; _key123++) {
    args[_key123] = arguments[_key123];
  }

  return _builder.default.apply(void 0, ["Variance"].concat(args));
}

function VoidTypeAnnotation() {
  for (var _len124 = arguments.length, args = new Array(_len124), _key124 = 0; _key124 < _len124; _key124++) {
    args[_key124] = arguments[_key124];
  }

  return _builder.default.apply(void 0, ["VoidTypeAnnotation"].concat(args));
}

function JSXAttribute() {
  for (var _len125 = arguments.length, args = new Array(_len125), _key125 = 0; _key125 < _len125; _key125++) {
    args[_key125] = arguments[_key125];
  }

  return _builder.default.apply(void 0, ["JSXAttribute"].concat(args));
}

function JSXClosingElement() {
  for (var _len126 = arguments.length, args = new Array(_len126), _key126 = 0; _key126 < _len126; _key126++) {
    args[_key126] = arguments[_key126];
  }

  return _builder.default.apply(void 0, ["JSXClosingElement"].concat(args));
}

function JSXElement() {
  for (var _len127 = arguments.length, args = new Array(_len127), _key127 = 0; _key127 < _len127; _key127++) {
    args[_key127] = arguments[_key127];
  }

  return _builder.default.apply(void 0, ["JSXElement"].concat(args));
}

function JSXEmptyExpression() {
  for (var _len128 = arguments.length, args = new Array(_len128), _key128 = 0; _key128 < _len128; _key128++) {
    args[_key128] = arguments[_key128];
  }

  return _builder.default.apply(void 0, ["JSXEmptyExpression"].concat(args));
}

function JSXExpressionContainer() {
  for (var _len129 = arguments.length, args = new Array(_len129), _key129 = 0; _key129 < _len129; _key129++) {
    args[_key129] = arguments[_key129];
  }

  return _builder.default.apply(void 0, ["JSXExpressionContainer"].concat(args));
}

function JSXSpreadChild() {
  for (var _len130 = arguments.length, args = new Array(_len130), _key130 = 0; _key130 < _len130; _key130++) {
    args[_key130] = arguments[_key130];
  }

  return _builder.default.apply(void 0, ["JSXSpreadChild"].concat(args));
}

function JSXIdentifier() {
  for (var _len131 = arguments.length, args = new Array(_len131), _key131 = 0; _key131 < _len131; _key131++) {
    args[_key131] = arguments[_key131];
  }

  return _builder.default.apply(void 0, ["JSXIdentifier"].concat(args));
}

function JSXMemberExpression() {
  for (var _len132 = arguments.length, args = new Array(_len132), _key132 = 0; _key132 < _len132; _key132++) {
    args[_key132] = arguments[_key132];
  }

  return _builder.default.apply(void 0, ["JSXMemberExpression"].concat(args));
}

function JSXNamespacedName() {
  for (var _len133 = arguments.length, args = new Array(_len133), _key133 = 0; _key133 < _len133; _key133++) {
    args[_key133] = arguments[_key133];
  }

  return _builder.default.apply(void 0, ["JSXNamespacedName"].concat(args));
}

function JSXOpeningElement() {
  for (var _len134 = arguments.length, args = new Array(_len134), _key134 = 0; _key134 < _len134; _key134++) {
    args[_key134] = arguments[_key134];
  }

  return _builder.default.apply(void 0, ["JSXOpeningElement"].concat(args));
}

function JSXSpreadAttribute() {
  for (var _len135 = arguments.length, args = new Array(_len135), _key135 = 0; _key135 < _len135; _key135++) {
    args[_key135] = arguments[_key135];
  }

  return _builder.default.apply(void 0, ["JSXSpreadAttribute"].concat(args));
}

function JSXText() {
  for (var _len136 = arguments.length, args = new Array(_len136), _key136 = 0; _key136 < _len136; _key136++) {
    args[_key136] = arguments[_key136];
  }

  return _builder.default.apply(void 0, ["JSXText"].concat(args));
}

function JSXFragment() {
  for (var _len137 = arguments.length, args = new Array(_len137), _key137 = 0; _key137 < _len137; _key137++) {
    args[_key137] = arguments[_key137];
  }

  return _builder.default.apply(void 0, ["JSXFragment"].concat(args));
}

function JSXOpeningFragment() {
  for (var _len138 = arguments.length, args = new Array(_len138), _key138 = 0; _key138 < _len138; _key138++) {
    args[_key138] = arguments[_key138];
  }

  return _builder.default.apply(void 0, ["JSXOpeningFragment"].concat(args));
}

function JSXClosingFragment() {
  for (var _len139 = arguments.length, args = new Array(_len139), _key139 = 0; _key139 < _len139; _key139++) {
    args[_key139] = arguments[_key139];
  }

  return _builder.default.apply(void 0, ["JSXClosingFragment"].concat(args));
}

function Noop() {
  for (var _len140 = arguments.length, args = new Array(_len140), _key140 = 0; _key140 < _len140; _key140++) {
    args[_key140] = arguments[_key140];
  }

  return _builder.default.apply(void 0, ["Noop"].concat(args));
}

function ParenthesizedExpression() {
  for (var _len141 = arguments.length, args = new Array(_len141), _key141 = 0; _key141 < _len141; _key141++) {
    args[_key141] = arguments[_key141];
  }

  return _builder.default.apply(void 0, ["ParenthesizedExpression"].concat(args));
}

function AwaitExpression() {
  for (var _len142 = arguments.length, args = new Array(_len142), _key142 = 0; _key142 < _len142; _key142++) {
    args[_key142] = arguments[_key142];
  }

  return _builder.default.apply(void 0, ["AwaitExpression"].concat(args));
}

function BindExpression() {
  for (var _len143 = arguments.length, args = new Array(_len143), _key143 = 0; _key143 < _len143; _key143++) {
    args[_key143] = arguments[_key143];
  }

  return _builder.default.apply(void 0, ["BindExpression"].concat(args));
}

function ClassProperty() {
  for (var _len144 = arguments.length, args = new Array(_len144), _key144 = 0; _key144 < _len144; _key144++) {
    args[_key144] = arguments[_key144];
  }

  return _builder.default.apply(void 0, ["ClassProperty"].concat(args));
}

function Import() {
  for (var _len145 = arguments.length, args = new Array(_len145), _key145 = 0; _key145 < _len145; _key145++) {
    args[_key145] = arguments[_key145];
  }

  return _builder.default.apply(void 0, ["Import"].concat(args));
}

function Decorator() {
  for (var _len146 = arguments.length, args = new Array(_len146), _key146 = 0; _key146 < _len146; _key146++) {
    args[_key146] = arguments[_key146];
  }

  return _builder.default.apply(void 0, ["Decorator"].concat(args));
}

function DoExpression() {
  for (var _len147 = arguments.length, args = new Array(_len147), _key147 = 0; _key147 < _len147; _key147++) {
    args[_key147] = arguments[_key147];
  }

  return _builder.default.apply(void 0, ["DoExpression"].concat(args));
}

function ExportDefaultSpecifier() {
  for (var _len148 = arguments.length, args = new Array(_len148), _key148 = 0; _key148 < _len148; _key148++) {
    args[_key148] = arguments[_key148];
  }

  return _builder.default.apply(void 0, ["ExportDefaultSpecifier"].concat(args));
}

function ExportNamespaceSpecifier() {
  for (var _len149 = arguments.length, args = new Array(_len149), _key149 = 0; _key149 < _len149; _key149++) {
    args[_key149] = arguments[_key149];
  }

  return _builder.default.apply(void 0, ["ExportNamespaceSpecifier"].concat(args));
}

function TSParameterProperty() {
  for (var _len150 = arguments.length, args = new Array(_len150), _key150 = 0; _key150 < _len150; _key150++) {
    args[_key150] = arguments[_key150];
  }

  return _builder.default.apply(void 0, ["TSParameterProperty"].concat(args));
}

function TSDeclareFunction() {
  for (var _len151 = arguments.length, args = new Array(_len151), _key151 = 0; _key151 < _len151; _key151++) {
    args[_key151] = arguments[_key151];
  }

  return _builder.default.apply(void 0, ["TSDeclareFunction"].concat(args));
}

function TSDeclareMethod() {
  for (var _len152 = arguments.length, args = new Array(_len152), _key152 = 0; _key152 < _len152; _key152++) {
    args[_key152] = arguments[_key152];
  }

  return _builder.default.apply(void 0, ["TSDeclareMethod"].concat(args));
}

function TSQualifiedName() {
  for (var _len153 = arguments.length, args = new Array(_len153), _key153 = 0; _key153 < _len153; _key153++) {
    args[_key153] = arguments[_key153];
  }

  return _builder.default.apply(void 0, ["TSQualifiedName"].concat(args));
}

function TSCallSignatureDeclaration() {
  for (var _len154 = arguments.length, args = new Array(_len154), _key154 = 0; _key154 < _len154; _key154++) {
    args[_key154] = arguments[_key154];
  }

  return _builder.default.apply(void 0, ["TSCallSignatureDeclaration"].concat(args));
}

function TSConstructSignatureDeclaration() {
  for (var _len155 = arguments.length, args = new Array(_len155), _key155 = 0; _key155 < _len155; _key155++) {
    args[_key155] = arguments[_key155];
  }

  return _builder.default.apply(void 0, ["TSConstructSignatureDeclaration"].concat(args));
}

function TSPropertySignature() {
  for (var _len156 = arguments.length, args = new Array(_len156), _key156 = 0; _key156 < _len156; _key156++) {
    args[_key156] = arguments[_key156];
  }

  return _builder.default.apply(void 0, ["TSPropertySignature"].concat(args));
}

function TSMethodSignature() {
  for (var _len157 = arguments.length, args = new Array(_len157), _key157 = 0; _key157 < _len157; _key157++) {
    args[_key157] = arguments[_key157];
  }

  return _builder.default.apply(void 0, ["TSMethodSignature"].concat(args));
}

function TSIndexSignature() {
  for (var _len158 = arguments.length, args = new Array(_len158), _key158 = 0; _key158 < _len158; _key158++) {
    args[_key158] = arguments[_key158];
  }

  return _builder.default.apply(void 0, ["TSIndexSignature"].concat(args));
}

function TSAnyKeyword() {
  for (var _len159 = arguments.length, args = new Array(_len159), _key159 = 0; _key159 < _len159; _key159++) {
    args[_key159] = arguments[_key159];
  }

  return _builder.default.apply(void 0, ["TSAnyKeyword"].concat(args));
}

function TSNumberKeyword() {
  for (var _len160 = arguments.length, args = new Array(_len160), _key160 = 0; _key160 < _len160; _key160++) {
    args[_key160] = arguments[_key160];
  }

  return _builder.default.apply(void 0, ["TSNumberKeyword"].concat(args));
}

function TSObjectKeyword() {
  for (var _len161 = arguments.length, args = new Array(_len161), _key161 = 0; _key161 < _len161; _key161++) {
    args[_key161] = arguments[_key161];
  }

  return _builder.default.apply(void 0, ["TSObjectKeyword"].concat(args));
}

function TSBooleanKeyword() {
  for (var _len162 = arguments.length, args = new Array(_len162), _key162 = 0; _key162 < _len162; _key162++) {
    args[_key162] = arguments[_key162];
  }

  return _builder.default.apply(void 0, ["TSBooleanKeyword"].concat(args));
}

function TSStringKeyword() {
  for (var _len163 = arguments.length, args = new Array(_len163), _key163 = 0; _key163 < _len163; _key163++) {
    args[_key163] = arguments[_key163];
  }

  return _builder.default.apply(void 0, ["TSStringKeyword"].concat(args));
}

function TSSymbolKeyword() {
  for (var _len164 = arguments.length, args = new Array(_len164), _key164 = 0; _key164 < _len164; _key164++) {
    args[_key164] = arguments[_key164];
  }

  return _builder.default.apply(void 0, ["TSSymbolKeyword"].concat(args));
}

function TSVoidKeyword() {
  for (var _len165 = arguments.length, args = new Array(_len165), _key165 = 0; _key165 < _len165; _key165++) {
    args[_key165] = arguments[_key165];
  }

  return _builder.default.apply(void 0, ["TSVoidKeyword"].concat(args));
}

function TSUndefinedKeyword() {
  for (var _len166 = arguments.length, args = new Array(_len166), _key166 = 0; _key166 < _len166; _key166++) {
    args[_key166] = arguments[_key166];
  }

  return _builder.default.apply(void 0, ["TSUndefinedKeyword"].concat(args));
}

function TSNullKeyword() {
  for (var _len167 = arguments.length, args = new Array(_len167), _key167 = 0; _key167 < _len167; _key167++) {
    args[_key167] = arguments[_key167];
  }

  return _builder.default.apply(void 0, ["TSNullKeyword"].concat(args));
}

function TSNeverKeyword() {
  for (var _len168 = arguments.length, args = new Array(_len168), _key168 = 0; _key168 < _len168; _key168++) {
    args[_key168] = arguments[_key168];
  }

  return _builder.default.apply(void 0, ["TSNeverKeyword"].concat(args));
}

function TSThisType() {
  for (var _len169 = arguments.length, args = new Array(_len169), _key169 = 0; _key169 < _len169; _key169++) {
    args[_key169] = arguments[_key169];
  }

  return _builder.default.apply(void 0, ["TSThisType"].concat(args));
}

function TSFunctionType() {
  for (var _len170 = arguments.length, args = new Array(_len170), _key170 = 0; _key170 < _len170; _key170++) {
    args[_key170] = arguments[_key170];
  }

  return _builder.default.apply(void 0, ["TSFunctionType"].concat(args));
}

function TSConstructorType() {
  for (var _len171 = arguments.length, args = new Array(_len171), _key171 = 0; _key171 < _len171; _key171++) {
    args[_key171] = arguments[_key171];
  }

  return _builder.default.apply(void 0, ["TSConstructorType"].concat(args));
}

function TSTypeReference() {
  for (var _len172 = arguments.length, args = new Array(_len172), _key172 = 0; _key172 < _len172; _key172++) {
    args[_key172] = arguments[_key172];
  }

  return _builder.default.apply(void 0, ["TSTypeReference"].concat(args));
}

function TSTypePredicate() {
  for (var _len173 = arguments.length, args = new Array(_len173), _key173 = 0; _key173 < _len173; _key173++) {
    args[_key173] = arguments[_key173];
  }

  return _builder.default.apply(void 0, ["TSTypePredicate"].concat(args));
}

function TSTypeQuery() {
  for (var _len174 = arguments.length, args = new Array(_len174), _key174 = 0; _key174 < _len174; _key174++) {
    args[_key174] = arguments[_key174];
  }

  return _builder.default.apply(void 0, ["TSTypeQuery"].concat(args));
}

function TSTypeLiteral() {
  for (var _len175 = arguments.length, args = new Array(_len175), _key175 = 0; _key175 < _len175; _key175++) {
    args[_key175] = arguments[_key175];
  }

  return _builder.default.apply(void 0, ["TSTypeLiteral"].concat(args));
}

function TSArrayType() {
  for (var _len176 = arguments.length, args = new Array(_len176), _key176 = 0; _key176 < _len176; _key176++) {
    args[_key176] = arguments[_key176];
  }

  return _builder.default.apply(void 0, ["TSArrayType"].concat(args));
}

function TSTupleType() {
  for (var _len177 = arguments.length, args = new Array(_len177), _key177 = 0; _key177 < _len177; _key177++) {
    args[_key177] = arguments[_key177];
  }

  return _builder.default.apply(void 0, ["TSTupleType"].concat(args));
}

function TSUnionType() {
  for (var _len178 = arguments.length, args = new Array(_len178), _key178 = 0; _key178 < _len178; _key178++) {
    args[_key178] = arguments[_key178];
  }

  return _builder.default.apply(void 0, ["TSUnionType"].concat(args));
}

function TSIntersectionType() {
  for (var _len179 = arguments.length, args = new Array(_len179), _key179 = 0; _key179 < _len179; _key179++) {
    args[_key179] = arguments[_key179];
  }

  return _builder.default.apply(void 0, ["TSIntersectionType"].concat(args));
}

function TSParenthesizedType() {
  for (var _len180 = arguments.length, args = new Array(_len180), _key180 = 0; _key180 < _len180; _key180++) {
    args[_key180] = arguments[_key180];
  }

  return _builder.default.apply(void 0, ["TSParenthesizedType"].concat(args));
}

function TSTypeOperator() {
  for (var _len181 = arguments.length, args = new Array(_len181), _key181 = 0; _key181 < _len181; _key181++) {
    args[_key181] = arguments[_key181];
  }

  return _builder.default.apply(void 0, ["TSTypeOperator"].concat(args));
}

function TSIndexedAccessType() {
  for (var _len182 = arguments.length, args = new Array(_len182), _key182 = 0; _key182 < _len182; _key182++) {
    args[_key182] = arguments[_key182];
  }

  return _builder.default.apply(void 0, ["TSIndexedAccessType"].concat(args));
}

function TSMappedType() {
  for (var _len183 = arguments.length, args = new Array(_len183), _key183 = 0; _key183 < _len183; _key183++) {
    args[_key183] = arguments[_key183];
  }

  return _builder.default.apply(void 0, ["TSMappedType"].concat(args));
}

function TSLiteralType() {
  for (var _len184 = arguments.length, args = new Array(_len184), _key184 = 0; _key184 < _len184; _key184++) {
    args[_key184] = arguments[_key184];
  }

  return _builder.default.apply(void 0, ["TSLiteralType"].concat(args));
}

function TSExpressionWithTypeArguments() {
  for (var _len185 = arguments.length, args = new Array(_len185), _key185 = 0; _key185 < _len185; _key185++) {
    args[_key185] = arguments[_key185];
  }

  return _builder.default.apply(void 0, ["TSExpressionWithTypeArguments"].concat(args));
}

function TSInterfaceDeclaration() {
  for (var _len186 = arguments.length, args = new Array(_len186), _key186 = 0; _key186 < _len186; _key186++) {
    args[_key186] = arguments[_key186];
  }

  return _builder.default.apply(void 0, ["TSInterfaceDeclaration"].concat(args));
}

function TSInterfaceBody() {
  for (var _len187 = arguments.length, args = new Array(_len187), _key187 = 0; _key187 < _len187; _key187++) {
    args[_key187] = arguments[_key187];
  }

  return _builder.default.apply(void 0, ["TSInterfaceBody"].concat(args));
}

function TSTypeAliasDeclaration() {
  for (var _len188 = arguments.length, args = new Array(_len188), _key188 = 0; _key188 < _len188; _key188++) {
    args[_key188] = arguments[_key188];
  }

  return _builder.default.apply(void 0, ["TSTypeAliasDeclaration"].concat(args));
}

function TSAsExpression() {
  for (var _len189 = arguments.length, args = new Array(_len189), _key189 = 0; _key189 < _len189; _key189++) {
    args[_key189] = arguments[_key189];
  }

  return _builder.default.apply(void 0, ["TSAsExpression"].concat(args));
}

function TSTypeAssertion() {
  for (var _len190 = arguments.length, args = new Array(_len190), _key190 = 0; _key190 < _len190; _key190++) {
    args[_key190] = arguments[_key190];
  }

  return _builder.default.apply(void 0, ["TSTypeAssertion"].concat(args));
}

function TSEnumDeclaration() {
  for (var _len191 = arguments.length, args = new Array(_len191), _key191 = 0; _key191 < _len191; _key191++) {
    args[_key191] = arguments[_key191];
  }

  return _builder.default.apply(void 0, ["TSEnumDeclaration"].concat(args));
}

function TSEnumMember() {
  for (var _len192 = arguments.length, args = new Array(_len192), _key192 = 0; _key192 < _len192; _key192++) {
    args[_key192] = arguments[_key192];
  }

  return _builder.default.apply(void 0, ["TSEnumMember"].concat(args));
}

function TSModuleDeclaration() {
  for (var _len193 = arguments.length, args = new Array(_len193), _key193 = 0; _key193 < _len193; _key193++) {
    args[_key193] = arguments[_key193];
  }

  return _builder.default.apply(void 0, ["TSModuleDeclaration"].concat(args));
}

function TSModuleBlock() {
  for (var _len194 = arguments.length, args = new Array(_len194), _key194 = 0; _key194 < _len194; _key194++) {
    args[_key194] = arguments[_key194];
  }

  return _builder.default.apply(void 0, ["TSModuleBlock"].concat(args));
}

function TSImportEqualsDeclaration() {
  for (var _len195 = arguments.length, args = new Array(_len195), _key195 = 0; _key195 < _len195; _key195++) {
    args[_key195] = arguments[_key195];
  }

  return _builder.default.apply(void 0, ["TSImportEqualsDeclaration"].concat(args));
}

function TSExternalModuleReference() {
  for (var _len196 = arguments.length, args = new Array(_len196), _key196 = 0; _key196 < _len196; _key196++) {
    args[_key196] = arguments[_key196];
  }

  return _builder.default.apply(void 0, ["TSExternalModuleReference"].concat(args));
}

function TSNonNullExpression() {
  for (var _len197 = arguments.length, args = new Array(_len197), _key197 = 0; _key197 < _len197; _key197++) {
    args[_key197] = arguments[_key197];
  }

  return _builder.default.apply(void 0, ["TSNonNullExpression"].concat(args));
}

function TSExportAssignment() {
  for (var _len198 = arguments.length, args = new Array(_len198), _key198 = 0; _key198 < _len198; _key198++) {
    args[_key198] = arguments[_key198];
  }

  return _builder.default.apply(void 0, ["TSExportAssignment"].concat(args));
}

function TSNamespaceExportDeclaration() {
  for (var _len199 = arguments.length, args = new Array(_len199), _key199 = 0; _key199 < _len199; _key199++) {
    args[_key199] = arguments[_key199];
  }

  return _builder.default.apply(void 0, ["TSNamespaceExportDeclaration"].concat(args));
}

function TSTypeAnnotation() {
  for (var _len200 = arguments.length, args = new Array(_len200), _key200 = 0; _key200 < _len200; _key200++) {
    args[_key200] = arguments[_key200];
  }

  return _builder.default.apply(void 0, ["TSTypeAnnotation"].concat(args));
}

function TSTypeParameterInstantiation() {
  for (var _len201 = arguments.length, args = new Array(_len201), _key201 = 0; _key201 < _len201; _key201++) {
    args[_key201] = arguments[_key201];
  }

  return _builder.default.apply(void 0, ["TSTypeParameterInstantiation"].concat(args));
}

function TSTypeParameterDeclaration() {
  for (var _len202 = arguments.length, args = new Array(_len202), _key202 = 0; _key202 < _len202; _key202++) {
    args[_key202] = arguments[_key202];
  }

  return _builder.default.apply(void 0, ["TSTypeParameterDeclaration"].concat(args));
}

function TSTypeParameter() {
  for (var _len203 = arguments.length, args = new Array(_len203), _key203 = 0; _key203 < _len203; _key203++) {
    args[_key203] = arguments[_key203];
  }

  return _builder.default.apply(void 0, ["TSTypeParameter"].concat(args));
}

function NumberLiteral() {
  console.trace("The node type NumberLiteral has been renamed to NumericLiteral");

  for (var _len204 = arguments.length, args = new Array(_len204), _key204 = 0; _key204 < _len204; _key204++) {
    args[_key204] = arguments[_key204];
  }

  return NumberLiteral.apply(void 0, ["NumberLiteral"].concat(args));
}

function RegexLiteral() {
  console.trace("The node type RegexLiteral has been renamed to RegExpLiteral");

  for (var _len205 = arguments.length, args = new Array(_len205), _key205 = 0; _key205 < _len205; _key205++) {
    args[_key205] = arguments[_key205];
  }

  return RegexLiteral.apply(void 0, ["RegexLiteral"].concat(args));
}

function RestProperty() {
  console.trace("The node type RestProperty has been renamed to RestElement");

  for (var _len206 = arguments.length, args = new Array(_len206), _key206 = 0; _key206 < _len206; _key206++) {
    args[_key206] = arguments[_key206];
  }

  return RestProperty.apply(void 0, ["RestProperty"].concat(args));
}

function SpreadProperty() {
  console.trace("The node type SpreadProperty has been renamed to SpreadElement");

  for (var _len207 = arguments.length, args = new Array(_len207), _key207 = 0; _key207 < _len207; _key207++) {
    args[_key207] = arguments[_key207];
  }

  return SpreadProperty.apply(void 0, ["SpreadProperty"].concat(args));
}

/***/ }),
/* 30 */
/***/ (function(module, exports, __webpack_require__) {

var baseGetTag = __webpack_require__(23),
    isObjectLike = __webpack_require__(21);

/** `Object#toString` result references. */
var symbolTag = '[object Symbol]';

/**
 * Checks if `value` is classified as a `Symbol` primitive or object.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a symbol, else `false`.
 * @example
 *
 * _.isSymbol(Symbol.iterator);
 * // => true
 *
 * _.isSymbol('abc');
 * // => false
 */
function isSymbol(value) {
  return typeof value == 'symbol' ||
    (isObjectLike(value) && baseGetTag(value) == symbolTag);
}

module.exports = isSymbol;


/***/ }),
/* 31 */,
/* 32 */,
/* 33 */,
/* 34 */,
/* 35 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.TYPES = void 0;

var _toFastProperties = _interopRequireDefault(__webpack_require__(665));

__webpack_require__(178);

__webpack_require__(179);

__webpack_require__(669);

__webpack_require__(670);

__webpack_require__(671);

__webpack_require__(672);

__webpack_require__(673);

var _utils = __webpack_require__(43);

exports.VISITOR_KEYS = _utils.VISITOR_KEYS;
exports.ALIAS_KEYS = _utils.ALIAS_KEYS;
exports.FLIPPED_ALIAS_KEYS = _utils.FLIPPED_ALIAS_KEYS;
exports.NODE_FIELDS = _utils.NODE_FIELDS;
exports.BUILDER_KEYS = _utils.BUILDER_KEYS;
exports.DEPRECATED_KEYS = _utils.DEPRECATED_KEYS;

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

(0, _toFastProperties.default)(_utils.VISITOR_KEYS);
(0, _toFastProperties.default)(_utils.ALIAS_KEYS);
(0, _toFastProperties.default)(_utils.FLIPPED_ALIAS_KEYS);
(0, _toFastProperties.default)(_utils.NODE_FIELDS);
(0, _toFastProperties.default)(_utils.BUILDER_KEYS);
(0, _toFastProperties.default)(_utils.DEPRECATED_KEYS);
var TYPES = Object.keys(_utils.VISITOR_KEYS).concat(Object.keys(_utils.FLIPPED_ALIAS_KEYS)).concat(Object.keys(_utils.DEPRECATED_KEYS));
exports.TYPES = TYPES;

/***/ }),
/* 36 */
/***/ (function(module, exports, __webpack_require__) {

var getNative = __webpack_require__(25);

/* Built-in method references that are verified to be native. */
var nativeCreate = getNative(Object, 'create');

module.exports = nativeCreate;


/***/ }),
/* 37 */
/***/ (function(module, exports, __webpack_require__) {

var eq = __webpack_require__(54);

/**
 * Gets the index at which the `key` is found in `array` of key-value pairs.
 *
 * @private
 * @param {Array} array The array to inspect.
 * @param {*} key The key to search for.
 * @returns {number} Returns the index of the matched value, else `-1`.
 */
function assocIndexOf(array, key) {
  var length = array.length;
  while (length--) {
    if (eq(array[length][0], key)) {
      return length;
    }
  }
  return -1;
}

module.exports = assocIndexOf;


/***/ }),
/* 38 */
/***/ (function(module, exports, __webpack_require__) {

var isKeyable = __webpack_require__(141);

/**
 * Gets the data for `map`.
 *
 * @private
 * @param {Object} map The map to query.
 * @param {string} key The reference key.
 * @returns {*} Returns the map data.
 */
function getMapData(map, key) {
  var data = map.__data__;
  return isKeyable(key)
    ? data[typeof key == 'string' ? 'string' : 'hash']
    : data.map;
}

module.exports = getMapData;


/***/ }),
/* 39 */,
/* 40 */
/***/ (function(module, exports) {

var g;

// This works in non-strict mode
g = (function() {
	return this;
})();

try {
	// This works if eval is allowed (see CSP)
	g = g || Function("return this")() || (1,eval)("this");
} catch(e) {
	// This works if the window reference is available
	if(typeof window === "object")
		g = window;
}

// g can still be undefined, but nothing to do about it...
// We return undefined, instead of nothing here, so it's
// easier to handle this case. if(!global) { ...}

module.exports = g;


/***/ }),
/* 41 */,
/* 42 */,
/* 43 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.validate = validate;
exports.typeIs = typeIs;
exports.validateType = validateType;
exports.validateOptional = validateOptional;
exports.validateOptionalType = validateOptionalType;
exports.arrayOf = arrayOf;
exports.arrayOfType = arrayOfType;
exports.validateArrayOfType = validateArrayOfType;
exports.assertEach = assertEach;
exports.assertOneOf = assertOneOf;
exports.assertNodeType = assertNodeType;
exports.assertNodeOrValueType = assertNodeOrValueType;
exports.assertValueType = assertValueType;
exports.chain = chain;
exports.default = defineType;
exports.DEPRECATED_KEYS = exports.BUILDER_KEYS = exports.NODE_FIELDS = exports.FLIPPED_ALIAS_KEYS = exports.ALIAS_KEYS = exports.VISITOR_KEYS = void 0;

var _is = _interopRequireDefault(__webpack_require__(110));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var VISITOR_KEYS = {};
exports.VISITOR_KEYS = VISITOR_KEYS;
var ALIAS_KEYS = {};
exports.ALIAS_KEYS = ALIAS_KEYS;
var FLIPPED_ALIAS_KEYS = {};
exports.FLIPPED_ALIAS_KEYS = FLIPPED_ALIAS_KEYS;
var NODE_FIELDS = {};
exports.NODE_FIELDS = NODE_FIELDS;
var BUILDER_KEYS = {};
exports.BUILDER_KEYS = BUILDER_KEYS;
var DEPRECATED_KEYS = {};
exports.DEPRECATED_KEYS = DEPRECATED_KEYS;

function getType(val) {
  if (Array.isArray(val)) {
    return "array";
  } else if (val === null) {
    return "null";
  } else if (val === undefined) {
    return "undefined";
  } else {
    return typeof val;
  }
}

function validate(validate) {
  return {
    validate: validate
  };
}

function typeIs(typeName) {
  return typeof typeName === "string" ? assertNodeType(typeName) : assertNodeType.apply(void 0, typeName);
}

function validateType(typeName) {
  return validate(typeIs(typeName));
}

function validateOptional(validate) {
  return {
    validate: validate,
    optional: true
  };
}

function validateOptionalType(typeName) {
  return {
    validate: typeIs(typeName),
    optional: true
  };
}

function arrayOf(elementType) {
  return chain(assertValueType("array"), assertEach(elementType));
}

function arrayOfType(typeName) {
  return arrayOf(typeIs(typeName));
}

function validateArrayOfType(typeName) {
  return validate(arrayOfType(typeName));
}

function assertEach(callback) {
  function validator(node, key, val) {
    if (!Array.isArray(val)) return;

    for (var i = 0; i < val.length; i++) {
      callback(node, key + "[" + i + "]", val[i]);
    }
  }

  validator.each = callback;
  return validator;
}

function assertOneOf() {
  for (var _len = arguments.length, values = new Array(_len), _key = 0; _key < _len; _key++) {
    values[_key] = arguments[_key];
  }

  function validate(node, key, val) {
    if (values.indexOf(val) < 0) {
      throw new TypeError("Property " + key + " expected value to be one of " + JSON.stringify(values) + " but got " + JSON.stringify(val));
    }
  }

  validate.oneOf = values;
  return validate;
}

function assertNodeType() {
  for (var _len2 = arguments.length, types = new Array(_len2), _key2 = 0; _key2 < _len2; _key2++) {
    types[_key2] = arguments[_key2];
  }

  function validate(node, key, val) {
    var valid = false;

    for (var _i = 0; _i < types.length; _i++) {
      var type = types[_i];

      if ((0, _is.default)(type, val)) {
        valid = true;
        break;
      }
    }

    if (!valid) {
      throw new TypeError("Property " + key + " of " + node.type + " expected node to be of a type " + JSON.stringify(types) + " " + ("but instead got " + JSON.stringify(val && val.type)));
    }
  }

  validate.oneOfNodeTypes = types;
  return validate;
}

function assertNodeOrValueType() {
  for (var _len3 = arguments.length, types = new Array(_len3), _key3 = 0; _key3 < _len3; _key3++) {
    types[_key3] = arguments[_key3];
  }

  function validate(node, key, val) {
    var valid = false;

    for (var _i2 = 0; _i2 < types.length; _i2++) {
      var type = types[_i2];

      if (getType(val) === type || (0, _is.default)(type, val)) {
        valid = true;
        break;
      }
    }

    if (!valid) {
      throw new TypeError("Property " + key + " of " + node.type + " expected node to be of a type " + JSON.stringify(types) + " " + ("but instead got " + JSON.stringify(val && val.type)));
    }
  }

  validate.oneOfNodeOrValueTypes = types;
  return validate;
}

function assertValueType(type) {
  function validate(node, key, val) {
    var valid = getType(val) === type;

    if (!valid) {
      throw new TypeError("Property " + key + " expected type of " + type + " but got " + getType(val));
    }
  }

  validate.type = type;
  return validate;
}

function chain() {
  for (var _len4 = arguments.length, fns = new Array(_len4), _key4 = 0; _key4 < _len4; _key4++) {
    fns[_key4] = arguments[_key4];
  }

  function validate() {
    for (var _i3 = 0; _i3 < fns.length; _i3++) {
      var fn = fns[_i3];
      fn.apply(void 0, arguments);
    }
  }

  validate.chainOf = fns;
  return validate;
}

function defineType(type, opts) {
  if (opts === void 0) {
    opts = {};
  }

  var inherits = opts.inherits && store[opts.inherits] || {};
  var fields = opts.fields || inherits.fields || {};
  var visitor = opts.visitor || inherits.visitor || [];
  var aliases = opts.aliases || inherits.aliases || [];
  var builder = opts.builder || inherits.builder || opts.visitor || [];

  if (opts.deprecatedAlias) {
    DEPRECATED_KEYS[opts.deprecatedAlias] = type;
  }

  var _arr = visitor.concat(builder);

  for (var _i4 = 0; _i4 < _arr.length; _i4++) {
    var key = _arr[_i4];
    fields[key] = fields[key] || {};
  }

  for (var _key5 in fields) {
    var field = fields[_key5];

    if (builder.indexOf(_key5) === -1) {
      field.optional = true;
    }

    if (field.default === undefined) {
      field.default = null;
    } else if (!field.validate) {
      field.validate = assertValueType(getType(field.default));
    }
  }

  VISITOR_KEYS[type] = opts.visitor = visitor;
  BUILDER_KEYS[type] = opts.builder = builder;
  NODE_FIELDS[type] = opts.fields = fields;
  ALIAS_KEYS[type] = opts.aliases = aliases;
  aliases.forEach(function (alias) {
    FLIPPED_ALIAS_KEYS[alias] = FLIPPED_ALIAS_KEYS[alias] || [];
    FLIPPED_ALIAS_KEYS[alias].push(type);
  });
  store[type] = opts;
}

var store = {};

/***/ }),
/* 44 */
/***/ (function(module, exports, __webpack_require__) {

var isSymbol = __webpack_require__(30);

/** Used as references for various `Number` constants. */
var INFINITY = 1 / 0;

/**
 * Converts `value` to a string key if it's not a string or symbol.
 *
 * @private
 * @param {*} value The value to inspect.
 * @returns {string|symbol} Returns the key.
 */
function toKey(value) {
  if (typeof value == 'string' || isSymbol(value)) {
    return value;
  }
  var result = (value + '');
  return (result == '0' && (1 / value) == -INFINITY) ? '-0' : result;
}

module.exports = toKey;


/***/ }),
/* 45 */,
/* 46 */
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function networkRequest(url, opts) {
  return fetch(url, {
    cache: opts.loadFromCache ? "default" : "no-cache"
  }).then(res => {
    if (res.status >= 200 && res.status < 300) {
      return res.text().then(text => ({ content: text }));
    }
    return Promise.reject(`request failed with status ${res.status}`);
  });
}

module.exports = networkRequest;

/***/ }),
/* 47 */
/***/ (function(module, exports) {

function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

function WorkerDispatcher() {
  this.msgId = 1;
  this.worker = null;
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

WorkerDispatcher.prototype = {
  start(url) {
    this.worker = new Worker(url);
    this.worker.onerror = () => {
      console.error(`Error in worker ${url}`);
    };
  },

  stop() {
    if (!this.worker) {
      return;
    }

    this.worker.terminate();
    this.worker = null;
  },

  task(method) {
    return (...args) => {
      return new Promise((resolve, reject) => {
        const id = this.msgId++;
        this.worker.postMessage({ id, method, args });

        const listener = ({ data: result }) => {
          if (result.id !== id) {
            return;
          }

          if (!this.worker) {
            return;
          }

          this.worker.removeEventListener("message", listener);
          if (result.error) {
            reject(result.error);
          } else {
            resolve(result.response);
          }
        };

        this.worker.addEventListener("message", listener);
      });
    };
  }
};

function workerHandler(publicInterface) {
  return function (msg) {
    const { id, method, args } = msg.data;
    try {
      const response = publicInterface[method].apply(undefined, args);
      if (response instanceof Promise) {
        response.then(val => self.postMessage({ id, response: val }),
        // Error can't be sent via postMessage, so be sure to
        // convert to string.
        err => self.postMessage({ id, error: err.toString() }));
      } else {
        self.postMessage({ id, response });
      }
    } catch (error) {
      // Error can't be sent via postMessage, so be sure to convert to
      // string.
      self.postMessage({ id, error: error.toString() });
    }
  };
}

function streamingWorkerHandler(publicInterface, { timeout = 100 } = {}, worker = self) {
  let streamingWorker = (() => {
    var _ref = _asyncToGenerator(function* (id, tasks) {
      let isWorking = true;

      const intervalId = setTimeout(function () {
        isWorking = false;
      }, timeout);

      const results = [];
      while (tasks.length !== 0 && isWorking) {
        const { callback, context, args } = tasks.shift();
        const result = yield callback.call(context, args);
        results.push(result);
      }
      worker.postMessage({ id, status: "pending", data: results });
      clearInterval(intervalId);

      if (tasks.length !== 0) {
        yield streamingWorker(id, tasks);
      }
    });

    return function streamingWorker(_x, _x2) {
      return _ref.apply(this, arguments);
    };
  })();

  return (() => {
    var _ref2 = _asyncToGenerator(function* (msg) {
      const { id, method, args } = msg.data;
      const workerMethod = publicInterface[method];
      if (!workerMethod) {
        console.error(`Could not find ${method} defined in worker.`);
      }
      worker.postMessage({ id, status: "start" });

      try {
        const tasks = workerMethod(args);
        yield streamingWorker(id, tasks);
        worker.postMessage({ id, status: "done" });
      } catch (error) {
        worker.postMessage({ id, status: "error", error });
      }
    });

    return function (_x3) {
      return _ref2.apply(this, arguments);
    };
  })();
}

module.exports = {
  WorkerDispatcher,
  workerHandler,
  streamingWorkerHandler
};

/***/ }),
/* 48 */,
/* 49 */,
/* 50 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.NOT_LOCAL_BINDING = exports.BLOCK_SCOPED_SYMBOL = exports.INHERIT_KEYS = exports.UNARY_OPERATORS = exports.STRING_UNARY_OPERATORS = exports.NUMBER_UNARY_OPERATORS = exports.BOOLEAN_UNARY_OPERATORS = exports.BINARY_OPERATORS = exports.NUMBER_BINARY_OPERATORS = exports.BOOLEAN_BINARY_OPERATORS = exports.COMPARISON_BINARY_OPERATORS = exports.EQUALITY_BINARY_OPERATORS = exports.BOOLEAN_NUMBER_BINARY_OPERATORS = exports.UPDATE_OPERATORS = exports.LOGICAL_OPERATORS = exports.COMMENT_KEYS = exports.FOR_INIT_KEYS = exports.FLATTENABLE_KEYS = exports.STATEMENT_OR_BLOCK_KEYS = void 0;
var STATEMENT_OR_BLOCK_KEYS = ["consequent", "body", "alternate"];
exports.STATEMENT_OR_BLOCK_KEYS = STATEMENT_OR_BLOCK_KEYS;
var FLATTENABLE_KEYS = ["body", "expressions"];
exports.FLATTENABLE_KEYS = FLATTENABLE_KEYS;
var FOR_INIT_KEYS = ["left", "init"];
exports.FOR_INIT_KEYS = FOR_INIT_KEYS;
var COMMENT_KEYS = ["leadingComments", "trailingComments", "innerComments"];
exports.COMMENT_KEYS = COMMENT_KEYS;
var LOGICAL_OPERATORS = ["||", "&&", "??"];
exports.LOGICAL_OPERATORS = LOGICAL_OPERATORS;
var UPDATE_OPERATORS = ["++", "--"];
exports.UPDATE_OPERATORS = UPDATE_OPERATORS;
var BOOLEAN_NUMBER_BINARY_OPERATORS = [">", "<", ">=", "<="];
exports.BOOLEAN_NUMBER_BINARY_OPERATORS = BOOLEAN_NUMBER_BINARY_OPERATORS;
var EQUALITY_BINARY_OPERATORS = ["==", "===", "!=", "!=="];
exports.EQUALITY_BINARY_OPERATORS = EQUALITY_BINARY_OPERATORS;
var COMPARISON_BINARY_OPERATORS = EQUALITY_BINARY_OPERATORS.concat(["in", "instanceof"]);
exports.COMPARISON_BINARY_OPERATORS = COMPARISON_BINARY_OPERATORS;
var BOOLEAN_BINARY_OPERATORS = COMPARISON_BINARY_OPERATORS.concat(BOOLEAN_NUMBER_BINARY_OPERATORS);
exports.BOOLEAN_BINARY_OPERATORS = BOOLEAN_BINARY_OPERATORS;
var NUMBER_BINARY_OPERATORS = ["-", "/", "%", "*", "**", "&", "|", ">>", ">>>", "<<", "^"];
exports.NUMBER_BINARY_OPERATORS = NUMBER_BINARY_OPERATORS;
var BINARY_OPERATORS = ["+"].concat(NUMBER_BINARY_OPERATORS, BOOLEAN_BINARY_OPERATORS);
exports.BINARY_OPERATORS = BINARY_OPERATORS;
var BOOLEAN_UNARY_OPERATORS = ["delete", "!"];
exports.BOOLEAN_UNARY_OPERATORS = BOOLEAN_UNARY_OPERATORS;
var NUMBER_UNARY_OPERATORS = ["+", "-", "~"];
exports.NUMBER_UNARY_OPERATORS = NUMBER_UNARY_OPERATORS;
var STRING_UNARY_OPERATORS = ["typeof"];
exports.STRING_UNARY_OPERATORS = STRING_UNARY_OPERATORS;
var UNARY_OPERATORS = ["void", "throw"].concat(BOOLEAN_UNARY_OPERATORS, NUMBER_UNARY_OPERATORS, STRING_UNARY_OPERATORS);
exports.UNARY_OPERATORS = UNARY_OPERATORS;
var INHERIT_KEYS = {
  optional: ["typeAnnotation", "typeParameters", "returnType"],
  force: ["start", "loc", "end"]
};
exports.INHERIT_KEYS = INHERIT_KEYS;
var BLOCK_SCOPED_SYMBOL = Symbol.for("var used to be block scoped");
exports.BLOCK_SCOPED_SYMBOL = BLOCK_SCOPED_SYMBOL;
var NOT_LOCAL_BINDING = Symbol.for("should not be considered a local binding");
exports.NOT_LOCAL_BINDING = NOT_LOCAL_BINDING;

/***/ }),
/* 51 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; /* This Source Code Form is subject to the terms of the Mozilla Public
                                                                                                                                                                                                                                                                   * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                                                                                                                                                                                                                                   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

exports.parseScript = parseScript;
exports.getAst = getAst;
exports.clearASTs = clearASTs;
exports.traverseAst = traverseAst;

var _parseScriptTags = __webpack_require__(759);

var _parseScriptTags2 = _interopRequireDefault(_parseScriptTags);

var _babylon = __webpack_require__(289);

var babylon = _interopRequireWildcard(_babylon);

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

var _isEmpty = __webpack_require__(290);

var _isEmpty2 = _interopRequireDefault(_isEmpty);

var _sources = __webpack_require__(291);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

let ASTs = new Map();

function _parse(code, opts) {
  return babylon.parse(code, _extends({}, opts, {
    tokens: true
  }));
}

const sourceOptions = {
  generated: {
    tokens: true
  },
  original: {
    sourceType: "unambiguous",
    tokens: true,
    plugins: ["jsx", "flow", "doExpressions", "objectRestSpread", "classProperties", "exportDefaultFrom", "exportNamespaceFrom", "asyncGenerators", "functionBind", "functionSent", "dynamicImport"]
  }
};

function parse(text, opts) {
  let ast;
  if (!text) {
    return;
  }

  try {
    ast = _parse(text, opts);
  } catch (error) {
    ast = {};
  }

  return ast;
}

// Custom parser for parse-script-tags that adapts its input structure to
// our parser's signature
function htmlParser({ source, line }) {
  return parse(source, { startLine: line });
}

function parseScript(text, opts) {
  return _parse(text, opts);
}

function getAst(sourceId) {
  if (ASTs.has(sourceId)) {
    return ASTs.get(sourceId);
  }

  const source = (0, _sources.getSource)(sourceId);

  let ast = {};
  const { contentType } = source;
  if (contentType == "text/html") {
    ast = (0, _parseScriptTags2.default)(source.text, htmlParser) || {};
  } else if (contentType && contentType.match(/(javascript|jsx)/)) {
    const type = source.id.includes("original") ? "original" : "generated";
    const options = sourceOptions[type];
    ast = parse(source.text, options);
  }

  ASTs.set(source.id, ast);
  return ast;
}

function clearASTs() {
  ASTs = new Map();
}

function traverseAst(sourceId, visitor, state) {
  const ast = getAst(sourceId);
  if ((0, _isEmpty2.default)(ast)) {
    return null;
  }

  t.traverse(ast, visitor, state);
  // t.fastTraverse(ast, visitor);
  return ast;
}

/***/ }),
/* 52 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(global) {/** Detect free variable `global` from Node.js. */
var freeGlobal = typeof global == 'object' && global && global.Object === Object && global;

module.exports = freeGlobal;

/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(40)))

/***/ }),
/* 53 */
/***/ (function(module, exports, __webpack_require__) {

var listCacheClear = __webpack_require__(135),
    listCacheDelete = __webpack_require__(136),
    listCacheGet = __webpack_require__(137),
    listCacheHas = __webpack_require__(138),
    listCacheSet = __webpack_require__(139);

/**
 * Creates an list cache object.
 *
 * @private
 * @constructor
 * @param {Array} [entries] The key-value pairs to cache.
 */
function ListCache(entries) {
  var index = -1,
      length = entries == null ? 0 : entries.length;

  this.clear();
  while (++index < length) {
    var entry = entries[index];
    this.set(entry[0], entry[1]);
  }
}

// Add methods to `ListCache`.
ListCache.prototype.clear = listCacheClear;
ListCache.prototype['delete'] = listCacheDelete;
ListCache.prototype.get = listCacheGet;
ListCache.prototype.has = listCacheHas;
ListCache.prototype.set = listCacheSet;

module.exports = ListCache;


/***/ }),
/* 54 */
/***/ (function(module, exports) {

/**
 * Performs a
 * [`SameValueZero`](http://ecma-international.org/ecma-262/7.0/#sec-samevaluezero)
 * comparison between two values to determine if they are equivalent.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to compare.
 * @param {*} other The other value to compare.
 * @returns {boolean} Returns `true` if the values are equivalent, else `false`.
 * @example
 *
 * var object = { 'a': 1 };
 * var other = { 'a': 1 };
 *
 * _.eq(object, object);
 * // => true
 *
 * _.eq(object, other);
 * // => false
 *
 * _.eq('a', 'a');
 * // => true
 *
 * _.eq('a', Object('a'));
 * // => false
 *
 * _.eq(NaN, NaN);
 * // => true
 */
function eq(value, other) {
  return value === other || (value !== value && other !== other);
}

module.exports = eq;


/***/ }),
/* 55 */
/***/ (function(module, exports, __webpack_require__) {

var baseToString = __webpack_require__(67);

/**
 * Converts `value` to a string. An empty string is returned for `null`
 * and `undefined` values. The sign of `-0` is preserved.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to convert.
 * @returns {string} Returns the converted string.
 * @example
 *
 * _.toString(null);
 * // => ''
 *
 * _.toString(-0);
 * // => '-0'
 *
 * _.toString([1, 2, 3]);
 * // => '1,2,3'
 */
function toString(value) {
  return value == null ? '' : baseToString(value);
}

module.exports = toString;


/***/ }),
/* 56 */
/***/ (function(module, exports) {

/**
 * A specialized version of `_.map` for arrays without support for iteratee
 * shorthands.
 *
 * @private
 * @param {Array} [array] The array to iterate over.
 * @param {Function} iteratee The function invoked per iteration.
 * @returns {Array} Returns the new mapped array.
 */
function arrayMap(array, iteratee) {
  var index = -1,
      length = array == null ? 0 : array.length,
      result = Array(length);

  while (++index < length) {
    result[index] = iteratee(array[index], index, array);
  }
  return result;
}

module.exports = arrayMap;


/***/ }),
/* 57 */,
/* 58 */,
/* 59 */,
/* 60 */,
/* 61 */
/***/ (function(module, exports, __webpack_require__) {

var isArray = __webpack_require__(16),
    isKey = __webpack_require__(62),
    stringToPath = __webpack_require__(121),
    toString = __webpack_require__(55);

/**
 * Casts `value` to a path array if it's not one.
 *
 * @private
 * @param {*} value The value to inspect.
 * @param {Object} [object] The object to query keys on.
 * @returns {Array} Returns the cast property path array.
 */
function castPath(value, object) {
  if (isArray(value)) {
    return value;
  }
  return isKey(value, object) ? [value] : stringToPath(toString(value));
}

module.exports = castPath;


/***/ }),
/* 62 */
/***/ (function(module, exports, __webpack_require__) {

var isArray = __webpack_require__(16),
    isSymbol = __webpack_require__(30);

/** Used to match property names within property paths. */
var reIsDeepProp = /\.|\[(?:[^[\]]*|(["'])(?:(?!\1)[^\\]|\\.)*?\1)\]/,
    reIsPlainProp = /^\w*$/;

/**
 * Checks if `value` is a property name and not a property path.
 *
 * @private
 * @param {*} value The value to check.
 * @param {Object} [object] The object to query keys on.
 * @returns {boolean} Returns `true` if `value` is a property name, else `false`.
 */
function isKey(value, object) {
  if (isArray(value)) {
    return false;
  }
  var type = typeof value;
  if (type == 'number' || type == 'symbol' || type == 'boolean' ||
      value == null || isSymbol(value)) {
    return true;
  }
  return reIsPlainProp.test(value) || !reIsDeepProp.test(value) ||
    (object != null && value in Object(object));
}

module.exports = isKey;


/***/ }),
/* 63 */
/***/ (function(module, exports, __webpack_require__) {

var Symbol = __webpack_require__(20);

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * Used to resolve the
 * [`toStringTag`](http://ecma-international.org/ecma-262/7.0/#sec-object.prototype.tostring)
 * of values.
 */
var nativeObjectToString = objectProto.toString;

/** Built-in value references. */
var symToStringTag = Symbol ? Symbol.toStringTag : undefined;

/**
 * A specialized version of `baseGetTag` which ignores `Symbol.toStringTag` values.
 *
 * @private
 * @param {*} value The value to query.
 * @returns {string} Returns the raw `toStringTag`.
 */
function getRawTag(value) {
  var isOwn = hasOwnProperty.call(value, symToStringTag),
      tag = value[symToStringTag];

  try {
    value[symToStringTag] = undefined;
    var unmasked = true;
  } catch (e) {}

  var result = nativeObjectToString.call(value);
  if (unmasked) {
    if (isOwn) {
      value[symToStringTag] = tag;
    } else {
      delete value[symToStringTag];
    }
  }
  return result;
}

module.exports = getRawTag;


/***/ }),
/* 64 */
/***/ (function(module, exports) {

/** Used for built-in method references. */
var objectProto = Object.prototype;

/**
 * Used to resolve the
 * [`toStringTag`](http://ecma-international.org/ecma-262/7.0/#sec-object.prototype.tostring)
 * of values.
 */
var nativeObjectToString = objectProto.toString;

/**
 * Converts `value` to a string using `Object.prototype.toString`.
 *
 * @private
 * @param {*} value The value to convert.
 * @returns {string} Returns the converted string.
 */
function objectToString(value) {
  return nativeObjectToString.call(value);
}

module.exports = objectToString;


/***/ }),
/* 65 */
/***/ (function(module, exports, __webpack_require__) {

var mapCacheClear = __webpack_require__(124),
    mapCacheDelete = __webpack_require__(140),
    mapCacheGet = __webpack_require__(142),
    mapCacheHas = __webpack_require__(143),
    mapCacheSet = __webpack_require__(144);

/**
 * Creates a map cache object to store key-value pairs.
 *
 * @private
 * @constructor
 * @param {Array} [entries] The key-value pairs to cache.
 */
function MapCache(entries) {
  var index = -1,
      length = entries == null ? 0 : entries.length;

  this.clear();
  while (++index < length) {
    var entry = entries[index];
    this.set(entry[0], entry[1]);
  }
}

// Add methods to `MapCache`.
MapCache.prototype.clear = mapCacheClear;
MapCache.prototype['delete'] = mapCacheDelete;
MapCache.prototype.get = mapCacheGet;
MapCache.prototype.has = mapCacheHas;
MapCache.prototype.set = mapCacheSet;

module.exports = MapCache;


/***/ }),
/* 66 */
/***/ (function(module, exports, __webpack_require__) {

var getNative = __webpack_require__(25),
    root = __webpack_require__(18);

/* Built-in method references that are verified to be native. */
var Map = getNative(root, 'Map');

module.exports = Map;


/***/ }),
/* 67 */
/***/ (function(module, exports, __webpack_require__) {

var Symbol = __webpack_require__(20),
    arrayMap = __webpack_require__(56),
    isArray = __webpack_require__(16),
    isSymbol = __webpack_require__(30);

/** Used as references for various `Number` constants. */
var INFINITY = 1 / 0;

/** Used to convert symbols to primitives and strings. */
var symbolProto = Symbol ? Symbol.prototype : undefined,
    symbolToString = symbolProto ? symbolProto.toString : undefined;

/**
 * The base implementation of `_.toString` which doesn't convert nullish
 * values to empty strings.
 *
 * @private
 * @param {*} value The value to process.
 * @returns {string} Returns the string.
 */
function baseToString(value) {
  // Exit early for strings to avoid a performance hit in some environments.
  if (typeof value == 'string') {
    return value;
  }
  if (isArray(value)) {
    // Recursively convert values (susceptible to call stack limits).
    return arrayMap(value, baseToString) + '';
  }
  if (isSymbol(value)) {
    return symbolToString ? symbolToString.call(value) : '';
  }
  var result = (value + '');
  return (result == '0' && (1 / value) == -INFINITY) ? '-0' : result;
}

module.exports = baseToString;


/***/ }),
/* 68 */,
/* 69 */,
/* 70 */,
/* 71 */
/***/ (function(module, exports) {

var charenc = {
  // UTF-8 encoding
  utf8: {
    // Convert a string to a byte array
    stringToBytes: function(str) {
      return charenc.bin.stringToBytes(unescape(encodeURIComponent(str)));
    },

    // Convert a byte array to a string
    bytesToString: function(bytes) {
      return decodeURIComponent(escape(charenc.bin.bytesToString(bytes)));
    }
  },

  // Binary encoding
  bin: {
    // Convert a string to a byte array
    stringToBytes: function(str) {
      for (var bytes = [], i = 0; i < str.length; i++)
        bytes.push(str.charCodeAt(i) & 0xFF);
      return bytes;
    },

    // Convert a byte array to a string
    bytesToString: function(bytes) {
      for (var str = [], i = 0; i < bytes.length; i++)
        str.push(String.fromCharCode(bytes[i]));
      return str.join('');
    }
  }
};

module.exports = charenc;


/***/ }),
/* 72 */,
/* 73 */
/***/ (function(module, exports) {

module.exports = function(module) {
	if(!module.webpackPolyfill) {
		module.deprecate = function() {};
		module.paths = [];
		// module.parent = undefined by default
		if(!module.children) module.children = [];
		Object.defineProperty(module, "loaded", {
			enumerable: true,
			get: function() {
				return module.l;
			}
		});
		Object.defineProperty(module, "id", {
			enumerable: true,
			get: function() {
				return module.i;
			}
		});
		module.webpackPolyfill = 1;
	}
	return module;
};


/***/ }),
/* 74 */,
/* 75 */,
/* 76 */,
/* 77 */,
/* 78 */,
/* 79 */,
/* 80 */,
/* 81 */,
/* 82 */,
/* 83 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isValidIdentifier;

var _esutils = _interopRequireDefault(__webpack_require__(666));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function isValidIdentifier(name) {
  if (typeof name !== "string" || _esutils.default.keyword.isReservedWordES6(name, true)) {
    return false;
  } else if (name === "await") {
    return false;
  } else {
    return _esutils.default.keyword.isIdentifierNameES6(name);
  }
}

/***/ }),
/* 84 */
/***/ (function(module, exports) {

/**
 * The base implementation of `_.unary` without support for storing metadata.
 *
 * @private
 * @param {Function} func The function to cap arguments for.
 * @returns {Function} Returns the new capped function.
 */
function baseUnary(func) {
  return function(value) {
    return func(value);
  };
}

module.exports = baseUnary;


/***/ }),
/* 85 */
/***/ (function(module, exports, __webpack_require__) {

var DataView = __webpack_require__(701),
    Map = __webpack_require__(66),
    Promise = __webpack_require__(702),
    Set = __webpack_require__(268),
    WeakMap = __webpack_require__(703),
    baseGetTag = __webpack_require__(23),
    toSource = __webpack_require__(91);

/** `Object#toString` result references. */
var mapTag = '[object Map]',
    objectTag = '[object Object]',
    promiseTag = '[object Promise]',
    setTag = '[object Set]',
    weakMapTag = '[object WeakMap]';

var dataViewTag = '[object DataView]';

/** Used to detect maps, sets, and weakmaps. */
var dataViewCtorString = toSource(DataView),
    mapCtorString = toSource(Map),
    promiseCtorString = toSource(Promise),
    setCtorString = toSource(Set),
    weakMapCtorString = toSource(WeakMap);

/**
 * Gets the `toStringTag` of `value`.
 *
 * @private
 * @param {*} value The value to query.
 * @returns {string} Returns the `toStringTag`.
 */
var getTag = baseGetTag;

// Fallback for data views, maps, sets, and weak maps in IE 11 and promises in Node.js < 6.
if ((DataView && getTag(new DataView(new ArrayBuffer(1))) != dataViewTag) ||
    (Map && getTag(new Map) != mapTag) ||
    (Promise && getTag(Promise.resolve()) != promiseTag) ||
    (Set && getTag(new Set) != setTag) ||
    (WeakMap && getTag(new WeakMap) != weakMapTag)) {
  getTag = function(value) {
    var result = baseGetTag(value),
        Ctor = result == objectTag ? value.constructor : undefined,
        ctorString = Ctor ? toSource(Ctor) : '';

    if (ctorString) {
      switch (ctorString) {
        case dataViewCtorString: return dataViewTag;
        case mapCtorString: return mapTag;
        case promiseCtorString: return promiseTag;
        case setCtorString: return setTag;
        case weakMapCtorString: return weakMapTag;
      }
    }
    return result;
  };
}

module.exports = getTag;


/***/ }),
/* 86 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = cloneNode;

var _definitions = __webpack_require__(35);

var has = Function.call.bind(Object.prototype.hasOwnProperty);

function cloneIfNode(obj, deep) {
  if (obj && typeof obj.type === "string" && obj.type !== "CommentLine" && obj.type !== "CommentBlock") {
    return cloneNode(obj, deep);
  }

  return obj;
}

function cloneIfNodeOrArray(obj, deep) {
  if (Array.isArray(obj)) {
    return obj.map(function (node) {
      return cloneIfNode(node, deep);
    });
  }

  return cloneIfNode(obj, deep);
}

function cloneNode(node, deep) {
  if (deep === void 0) {
    deep = true;
  }

  if (!node) return node;
  var type = node.type;
  var newNode = {
    type: type
  };

  if (type === "Identifier") {
    newNode.name = node.name;
  } else if (!has(_definitions.NODE_FIELDS, type)) {
    throw new Error("Unknown node type: \"" + type + "\"");
  } else {
    var _arr = Object.keys(_definitions.NODE_FIELDS[type]);

    for (var _i = 0; _i < _arr.length; _i++) {
      var field = _arr[_i];

      if (has(node, field)) {
        newNode[field] = deep ? cloneIfNodeOrArray(node[field], true) : node[field];
      }
    }
  }

  if (has(node, "loc")) {
    newNode.loc = node.loc;
  }

  if (has(node, "leadingComments")) {
    newNode.leadingComments = node.leadingComments;
  }

  if (has(node, "innerComments")) {
    newNode.innerComments = node.innerCmments;
  }

  if (has(node, "trailingComments")) {
    newNode.trailingComments = node.trailingComments;
  }

  if (has(node, "extra")) {
    newNode.extra = Object.assign({}, node.extra);
  }

  return newNode;
}

/***/ }),
/* 87 */,
/* 88 */
/***/ (function(module, exports, __webpack_require__) {

var baseGet = __webpack_require__(89);

/**
 * Gets the value at `path` of `object`. If the resolved value is
 * `undefined`, the `defaultValue` is returned in its place.
 *
 * @static
 * @memberOf _
 * @since 3.7.0
 * @category Object
 * @param {Object} object The object to query.
 * @param {Array|string} path The path of the property to get.
 * @param {*} [defaultValue] The value returned for `undefined` resolved values.
 * @returns {*} Returns the resolved value.
 * @example
 *
 * var object = { 'a': [{ 'b': { 'c': 3 } }] };
 *
 * _.get(object, 'a[0].b.c');
 * // => 3
 *
 * _.get(object, ['a', '0', 'b', 'c']);
 * // => 3
 *
 * _.get(object, 'a.b.c', 'default');
 * // => 'default'
 */
function get(object, path, defaultValue) {
  var result = object == null ? undefined : baseGet(object, path);
  return result === undefined ? defaultValue : result;
}

module.exports = get;


/***/ }),
/* 89 */
/***/ (function(module, exports, __webpack_require__) {

var castPath = __webpack_require__(61),
    toKey = __webpack_require__(44);

/**
 * The base implementation of `_.get` without support for default values.
 *
 * @private
 * @param {Object} object The object to query.
 * @param {Array|string} path The path of the property to get.
 * @returns {*} Returns the resolved value.
 */
function baseGet(object, path) {
  path = castPath(path, object);

  var index = 0,
      length = path.length;

  while (object != null && index < length) {
    object = object[toKey(path[index++])];
  }
  return (index && index == length) ? object : undefined;
}

module.exports = baseGet;


/***/ }),
/* 90 */
/***/ (function(module, exports, __webpack_require__) {

var baseGetTag = __webpack_require__(23),
    isObject = __webpack_require__(26);

/** `Object#toString` result references. */
var asyncTag = '[object AsyncFunction]',
    funcTag = '[object Function]',
    genTag = '[object GeneratorFunction]',
    proxyTag = '[object Proxy]';

/**
 * Checks if `value` is classified as a `Function` object.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a function, else `false`.
 * @example
 *
 * _.isFunction(_);
 * // => true
 *
 * _.isFunction(/abc/);
 * // => false
 */
function isFunction(value) {
  if (!isObject(value)) {
    return false;
  }
  // The use of `Object#toString` avoids issues with the `typeof` operator
  // in Safari 9 which returns 'object' for typed arrays and other constructors.
  var tag = baseGetTag(value);
  return tag == funcTag || tag == genTag || tag == asyncTag || tag == proxyTag;
}

module.exports = isFunction;


/***/ }),
/* 91 */
/***/ (function(module, exports) {

/** Used for built-in method references. */
var funcProto = Function.prototype;

/** Used to resolve the decompiled source of functions. */
var funcToString = funcProto.toString;

/**
 * Converts `func` to its source code.
 *
 * @private
 * @param {Function} func The function to convert.
 * @returns {string} Returns the source code.
 */
function toSource(func) {
  if (func != null) {
    try {
      return funcToString.call(func);
    } catch (e) {}
    try {
      return (func + '');
    } catch (e) {}
  }
  return '';
}

module.exports = toSource;


/***/ }),
/* 92 */
/***/ (function(module, exports, __webpack_require__) {

var baseAssignValue = __webpack_require__(93),
    eq = __webpack_require__(54);

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * Assigns `value` to `key` of `object` if the existing value is not equivalent
 * using [`SameValueZero`](http://ecma-international.org/ecma-262/7.0/#sec-samevaluezero)
 * for equality comparisons.
 *
 * @private
 * @param {Object} object The object to modify.
 * @param {string} key The key of the property to assign.
 * @param {*} value The value to assign.
 */
function assignValue(object, key, value) {
  var objValue = object[key];
  if (!(hasOwnProperty.call(object, key) && eq(objValue, value)) ||
      (value === undefined && !(key in object))) {
    baseAssignValue(object, key, value);
  }
}

module.exports = assignValue;


/***/ }),
/* 93 */
/***/ (function(module, exports, __webpack_require__) {

var defineProperty = __webpack_require__(94);

/**
 * The base implementation of `assignValue` and `assignMergeValue` without
 * value checks.
 *
 * @private
 * @param {Object} object The object to modify.
 * @param {string} key The key of the property to assign.
 * @param {*} value The value to assign.
 */
function baseAssignValue(object, key, value) {
  if (key == '__proto__' && defineProperty) {
    defineProperty(object, key, {
      'configurable': true,
      'enumerable': true,
      'value': value,
      'writable': true
    });
  } else {
    object[key] = value;
  }
}

module.exports = baseAssignValue;


/***/ }),
/* 94 */
/***/ (function(module, exports, __webpack_require__) {

var getNative = __webpack_require__(25);

var defineProperty = (function() {
  try {
    var func = getNative(Object, 'defineProperty');
    func({}, '', {});
    return func;
  } catch (e) {}
}());

module.exports = defineProperty;


/***/ }),
/* 95 */
/***/ (function(module, exports) {

/** Used as references for various `Number` constants. */
var MAX_SAFE_INTEGER = 9007199254740991;

/** Used to detect unsigned integer values. */
var reIsUint = /^(?:0|[1-9]\d*)$/;

/**
 * Checks if `value` is a valid array-like index.
 *
 * @private
 * @param {*} value The value to check.
 * @param {number} [length=MAX_SAFE_INTEGER] The upper bounds of a valid index.
 * @returns {boolean} Returns `true` if `value` is a valid index, else `false`.
 */
function isIndex(value, length) {
  var type = typeof value;
  length = length == null ? MAX_SAFE_INTEGER : length;

  return !!length &&
    (type == 'number' ||
      (type != 'symbol' && reIsUint.test(value))) &&
        (value > -1 && value % 1 == 0 && value < length);
}

module.exports = isIndex;


/***/ }),
/* 96 */,
/* 97 */,
/* 98 */,
/* 99 */,
/* 100 */,
/* 101 */,
/* 102 */,
/* 103 */,
/* 104 */,
/* 105 */,
/* 106 */,
/* 107 */,
/* 108 */,
/* 109 */,
/* 110 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = is;

var _shallowEqual = _interopRequireDefault(__webpack_require__(258));

var _isType = _interopRequireDefault(__webpack_require__(177));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function is(type, node, opts) {
  if (!node) return false;
  var matches = (0, _isType.default)(node.type, type);
  if (!matches) return false;

  if (typeof opts === "undefined") {
    return true;
  } else {
    return (0, _shallowEqual.default)(node, opts);
  }
}

/***/ }),
/* 111 */
/***/ (function(module, exports, __webpack_require__) {

var assignValue = __webpack_require__(92),
    baseAssignValue = __webpack_require__(93);

/**
 * Copies properties of `source` to `object`.
 *
 * @private
 * @param {Object} source The object to copy properties from.
 * @param {Array} props The property identifiers to copy.
 * @param {Object} [object={}] The object to copy properties to.
 * @param {Function} [customizer] The function to customize copied values.
 * @returns {Object} Returns `object`.
 */
function copyObject(source, props, object, customizer) {
  var isNew = !object;
  object || (object = {});

  var index = -1,
      length = props.length;

  while (++index < length) {
    var key = props[index];

    var newValue = customizer
      ? customizer(object[key], source[key], key, object, source)
      : undefined;

    if (newValue === undefined) {
      newValue = source[key];
    }
    if (isNew) {
      baseAssignValue(object, key, newValue);
    } else {
      assignValue(object, key, newValue);
    }
  }
  return object;
}

module.exports = copyObject;


/***/ }),
/* 112 */
/***/ (function(module, exports, __webpack_require__) {

var arrayLikeKeys = __webpack_require__(260),
    baseKeys = __webpack_require__(261),
    isArrayLike = __webpack_require__(117);

/**
 * Creates an array of the own enumerable property names of `object`.
 *
 * **Note:** Non-object values are coerced to objects. See the
 * [ES spec](http://ecma-international.org/ecma-262/7.0/#sec-object.keys)
 * for more details.
 *
 * @static
 * @since 0.1.0
 * @memberOf _
 * @category Object
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of property names.
 * @example
 *
 * function Foo() {
 *   this.a = 1;
 *   this.b = 2;
 * }
 *
 * Foo.prototype.c = 3;
 *
 * _.keys(new Foo);
 * // => ['a', 'b'] (iteration order is not guaranteed)
 *
 * _.keys('hi');
 * // => ['0', '1']
 */
function keys(object) {
  return isArrayLike(object) ? arrayLikeKeys(object) : baseKeys(object);
}

module.exports = keys;


/***/ }),
/* 113 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsArguments = __webpack_require__(688),
    isObjectLike = __webpack_require__(21);

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/** Built-in value references. */
var propertyIsEnumerable = objectProto.propertyIsEnumerable;

/**
 * Checks if `value` is likely an `arguments` object.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is an `arguments` object,
 *  else `false`.
 * @example
 *
 * _.isArguments(function() { return arguments; }());
 * // => true
 *
 * _.isArguments([1, 2, 3]);
 * // => false
 */
var isArguments = baseIsArguments(function() { return arguments; }()) ? baseIsArguments : function(value) {
  return isObjectLike(value) && hasOwnProperty.call(value, 'callee') &&
    !propertyIsEnumerable.call(value, 'callee');
};

module.exports = isArguments;


/***/ }),
/* 114 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(module) {var root = __webpack_require__(18),
    stubFalse = __webpack_require__(689);

/** Detect free variable `exports`. */
var freeExports = typeof exports == 'object' && exports && !exports.nodeType && exports;

/** Detect free variable `module`. */
var freeModule = freeExports && typeof module == 'object' && module && !module.nodeType && module;

/** Detect the popular CommonJS extension `module.exports`. */
var moduleExports = freeModule && freeModule.exports === freeExports;

/** Built-in value references. */
var Buffer = moduleExports ? root.Buffer : undefined;

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeIsBuffer = Buffer ? Buffer.isBuffer : undefined;

/**
 * Checks if `value` is a buffer.
 *
 * @static
 * @memberOf _
 * @since 4.3.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a buffer, else `false`.
 * @example
 *
 * _.isBuffer(new Buffer(2));
 * // => true
 *
 * _.isBuffer(new Uint8Array(2));
 * // => false
 */
var isBuffer = nativeIsBuffer || stubFalse;

module.exports = isBuffer;

/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(73)(module)))

/***/ }),
/* 115 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(module) {var freeGlobal = __webpack_require__(52);

/** Detect free variable `exports`. */
var freeExports = typeof exports == 'object' && exports && !exports.nodeType && exports;

/** Detect free variable `module`. */
var freeModule = freeExports && typeof module == 'object' && module && !module.nodeType && module;

/** Detect the popular CommonJS extension `module.exports`. */
var moduleExports = freeModule && freeModule.exports === freeExports;

/** Detect free variable `process` from Node.js. */
var freeProcess = moduleExports && freeGlobal.process;

/** Used to access faster Node.js helpers. */
var nodeUtil = (function() {
  try {
    return freeProcess && freeProcess.binding && freeProcess.binding('util');
  } catch (e) {}
}());

module.exports = nodeUtil;

/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(73)(module)))

/***/ }),
/* 116 */
/***/ (function(module, exports) {

/** Used for built-in method references. */
var objectProto = Object.prototype;

/**
 * Checks if `value` is likely a prototype object.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a prototype, else `false`.
 */
function isPrototype(value) {
  var Ctor = value && value.constructor,
      proto = (typeof Ctor == 'function' && Ctor.prototype) || objectProto;

  return value === proto;
}

module.exports = isPrototype;


/***/ }),
/* 117 */
/***/ (function(module, exports, __webpack_require__) {

var isFunction = __webpack_require__(90),
    isLength = __webpack_require__(182);

/**
 * Checks if `value` is array-like. A value is considered array-like if it's
 * not a function and has a `value.length` that's an integer greater than or
 * equal to `0` and less than or equal to `Number.MAX_SAFE_INTEGER`.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is array-like, else `false`.
 * @example
 *
 * _.isArrayLike([1, 2, 3]);
 * // => true
 *
 * _.isArrayLike(document.body.children);
 * // => true
 *
 * _.isArrayLike('abc');
 * // => true
 *
 * _.isArrayLike(_.noop);
 * // => false
 */
function isArrayLike(value) {
  return value != null && isLength(value.length) && !isFunction(value);
}

module.exports = isArrayLike;


/***/ }),
/* 118 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = getBindingIdentifiers;

var _generated = __webpack_require__(17);

function getBindingIdentifiers(node, duplicates, outerOnly) {
  var search = [].concat(node);
  var ids = Object.create(null);

  while (search.length) {
    var id = search.shift();
    if (!id) continue;
    var keys = getBindingIdentifiers.keys[id.type];

    if ((0, _generated.isIdentifier)(id)) {
      if (duplicates) {
        var _ids = ids[id.name] = ids[id.name] || [];

        _ids.push(id);
      } else {
        ids[id.name] = id;
      }

      continue;
    }

    if ((0, _generated.isExportDeclaration)(id)) {
      if ((0, _generated.isDeclaration)(id.declaration)) {
        search.push(id.declaration);
      }

      continue;
    }

    if (outerOnly) {
      if ((0, _generated.isFunctionDeclaration)(id)) {
        search.push(id.id);
        continue;
      }

      if ((0, _generated.isFunctionExpression)(id)) {
        continue;
      }
    }

    if (keys) {
      for (var i = 0; i < keys.length; i++) {
        var key = keys[i];

        if (id[key]) {
          search = search.concat(id[key]);
        }
      }
    }
  }

  return ids;
}

getBindingIdentifiers.keys = {
  DeclareClass: ["id"],
  DeclareFunction: ["id"],
  DeclareModule: ["id"],
  DeclareVariable: ["id"],
  InterfaceDeclaration: ["id"],
  TypeAlias: ["id"],
  OpaqueType: ["id"],
  CatchClause: ["param"],
  LabeledStatement: ["label"],
  UnaryExpression: ["argument"],
  AssignmentExpression: ["left"],
  ImportSpecifier: ["local"],
  ImportNamespaceSpecifier: ["local"],
  ImportDefaultSpecifier: ["local"],
  ImportDeclaration: ["specifiers"],
  ExportSpecifier: ["exported"],
  ExportNamespaceSpecifier: ["exported"],
  ExportDefaultSpecifier: ["exported"],
  FunctionDeclaration: ["id", "params"],
  FunctionExpression: ["id", "params"],
  ForInStatement: ["left"],
  ForOfStatement: ["left"],
  ClassDeclaration: ["id"],
  ClassExpression: ["id"],
  RestElement: ["argument"],
  UpdateExpression: ["argument"],
  ObjectProperty: ["value"],
  AssignmentPattern: ["left"],
  ArrayPattern: ["elements"],
  ObjectPattern: ["properties"],
  VariableDeclaration: ["declarations"],
  VariableDeclarator: ["id"]
};

/***/ }),
/* 119 */,
/* 120 */,
/* 121 */
/***/ (function(module, exports, __webpack_require__) {

var memoizeCapped = __webpack_require__(122);

/** Used to match property names within property paths. */
var rePropName = /[^.[\]]+|\[(?:(-?\d+(?:\.\d+)?)|(["'])((?:(?!\2)[^\\]|\\.)*?)\2)\]|(?=(?:\.|\[\])(?:\.|\[\]|$))/g;

/** Used to match backslashes in property paths. */
var reEscapeChar = /\\(\\)?/g;

/**
 * Converts `string` to a property path array.
 *
 * @private
 * @param {string} string The string to convert.
 * @returns {Array} Returns the property path array.
 */
var stringToPath = memoizeCapped(function(string) {
  var result = [];
  if (string.charCodeAt(0) === 46 /* . */) {
    result.push('');
  }
  string.replace(rePropName, function(match, number, quote, subString) {
    result.push(quote ? subString.replace(reEscapeChar, '$1') : (number || match));
  });
  return result;
});

module.exports = stringToPath;


/***/ }),
/* 122 */
/***/ (function(module, exports, __webpack_require__) {

var memoize = __webpack_require__(123);

/** Used as the maximum memoize cache size. */
var MAX_MEMOIZE_SIZE = 500;

/**
 * A specialized version of `_.memoize` which clears the memoized function's
 * cache when it exceeds `MAX_MEMOIZE_SIZE`.
 *
 * @private
 * @param {Function} func The function to have its output memoized.
 * @returns {Function} Returns the new memoized function.
 */
function memoizeCapped(func) {
  var result = memoize(func, function(key) {
    if (cache.size === MAX_MEMOIZE_SIZE) {
      cache.clear();
    }
    return key;
  });

  var cache = result.cache;
  return result;
}

module.exports = memoizeCapped;


/***/ }),
/* 123 */
/***/ (function(module, exports, __webpack_require__) {

var MapCache = __webpack_require__(65);

/** Error message constants. */
var FUNC_ERROR_TEXT = 'Expected a function';

/**
 * Creates a function that memoizes the result of `func`. If `resolver` is
 * provided, it determines the cache key for storing the result based on the
 * arguments provided to the memoized function. By default, the first argument
 * provided to the memoized function is used as the map cache key. The `func`
 * is invoked with the `this` binding of the memoized function.
 *
 * **Note:** The cache is exposed as the `cache` property on the memoized
 * function. Its creation may be customized by replacing the `_.memoize.Cache`
 * constructor with one whose instances implement the
 * [`Map`](http://ecma-international.org/ecma-262/7.0/#sec-properties-of-the-map-prototype-object)
 * method interface of `clear`, `delete`, `get`, `has`, and `set`.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Function
 * @param {Function} func The function to have its output memoized.
 * @param {Function} [resolver] The function to resolve the cache key.
 * @returns {Function} Returns the new memoized function.
 * @example
 *
 * var object = { 'a': 1, 'b': 2 };
 * var other = { 'c': 3, 'd': 4 };
 *
 * var values = _.memoize(_.values);
 * values(object);
 * // => [1, 2]
 *
 * values(other);
 * // => [3, 4]
 *
 * object.a = 2;
 * values(object);
 * // => [1, 2]
 *
 * // Modify the result cache.
 * values.cache.set(object, ['a', 'b']);
 * values(object);
 * // => ['a', 'b']
 *
 * // Replace `_.memoize.Cache`.
 * _.memoize.Cache = WeakMap;
 */
function memoize(func, resolver) {
  if (typeof func != 'function' || (resolver != null && typeof resolver != 'function')) {
    throw new TypeError(FUNC_ERROR_TEXT);
  }
  var memoized = function() {
    var args = arguments,
        key = resolver ? resolver.apply(this, args) : args[0],
        cache = memoized.cache;

    if (cache.has(key)) {
      return cache.get(key);
    }
    var result = func.apply(this, args);
    memoized.cache = cache.set(key, result) || cache;
    return result;
  };
  memoized.cache = new (memoize.Cache || MapCache);
  return memoized;
}

// Expose `MapCache`.
memoize.Cache = MapCache;

module.exports = memoize;


/***/ }),
/* 124 */
/***/ (function(module, exports, __webpack_require__) {

var Hash = __webpack_require__(125),
    ListCache = __webpack_require__(53),
    Map = __webpack_require__(66);

/**
 * Removes all key-value entries from the map.
 *
 * @private
 * @name clear
 * @memberOf MapCache
 */
function mapCacheClear() {
  this.size = 0;
  this.__data__ = {
    'hash': new Hash,
    'map': new (Map || ListCache),
    'string': new Hash
  };
}

module.exports = mapCacheClear;


/***/ }),
/* 125 */
/***/ (function(module, exports, __webpack_require__) {

var hashClear = __webpack_require__(126),
    hashDelete = __webpack_require__(131),
    hashGet = __webpack_require__(132),
    hashHas = __webpack_require__(133),
    hashSet = __webpack_require__(134);

/**
 * Creates a hash object.
 *
 * @private
 * @constructor
 * @param {Array} [entries] The key-value pairs to cache.
 */
function Hash(entries) {
  var index = -1,
      length = entries == null ? 0 : entries.length;

  this.clear();
  while (++index < length) {
    var entry = entries[index];
    this.set(entry[0], entry[1]);
  }
}

// Add methods to `Hash`.
Hash.prototype.clear = hashClear;
Hash.prototype['delete'] = hashDelete;
Hash.prototype.get = hashGet;
Hash.prototype.has = hashHas;
Hash.prototype.set = hashSet;

module.exports = Hash;


/***/ }),
/* 126 */
/***/ (function(module, exports, __webpack_require__) {

var nativeCreate = __webpack_require__(36);

/**
 * Removes all key-value entries from the hash.
 *
 * @private
 * @name clear
 * @memberOf Hash
 */
function hashClear() {
  this.__data__ = nativeCreate ? nativeCreate(null) : {};
  this.size = 0;
}

module.exports = hashClear;


/***/ }),
/* 127 */
/***/ (function(module, exports, __webpack_require__) {

var isFunction = __webpack_require__(90),
    isMasked = __webpack_require__(128),
    isObject = __webpack_require__(26),
    toSource = __webpack_require__(91);

/**
 * Used to match `RegExp`
 * [syntax characters](http://ecma-international.org/ecma-262/7.0/#sec-patterns).
 */
var reRegExpChar = /[\\^$.*+?()[\]{}|]/g;

/** Used to detect host constructors (Safari). */
var reIsHostCtor = /^\[object .+?Constructor\]$/;

/** Used for built-in method references. */
var funcProto = Function.prototype,
    objectProto = Object.prototype;

/** Used to resolve the decompiled source of functions. */
var funcToString = funcProto.toString;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/** Used to detect if a method is native. */
var reIsNative = RegExp('^' +
  funcToString.call(hasOwnProperty).replace(reRegExpChar, '\\$&')
  .replace(/hasOwnProperty|(function).*?(?=\\\()| for .+?(?=\\\])/g, '$1.*?') + '$'
);

/**
 * The base implementation of `_.isNative` without bad shim checks.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a native function,
 *  else `false`.
 */
function baseIsNative(value) {
  if (!isObject(value) || isMasked(value)) {
    return false;
  }
  var pattern = isFunction(value) ? reIsNative : reIsHostCtor;
  return pattern.test(toSource(value));
}

module.exports = baseIsNative;


/***/ }),
/* 128 */
/***/ (function(module, exports, __webpack_require__) {

var coreJsData = __webpack_require__(129);

/** Used to detect methods masquerading as native. */
var maskSrcKey = (function() {
  var uid = /[^.]+$/.exec(coreJsData && coreJsData.keys && coreJsData.keys.IE_PROTO || '');
  return uid ? ('Symbol(src)_1.' + uid) : '';
}());

/**
 * Checks if `func` has its source masked.
 *
 * @private
 * @param {Function} func The function to check.
 * @returns {boolean} Returns `true` if `func` is masked, else `false`.
 */
function isMasked(func) {
  return !!maskSrcKey && (maskSrcKey in func);
}

module.exports = isMasked;


/***/ }),
/* 129 */
/***/ (function(module, exports, __webpack_require__) {

var root = __webpack_require__(18);

/** Used to detect overreaching core-js shims. */
var coreJsData = root['__core-js_shared__'];

module.exports = coreJsData;


/***/ }),
/* 130 */
/***/ (function(module, exports) {

/**
 * Gets the value at `key` of `object`.
 *
 * @private
 * @param {Object} [object] The object to query.
 * @param {string} key The key of the property to get.
 * @returns {*} Returns the property value.
 */
function getValue(object, key) {
  return object == null ? undefined : object[key];
}

module.exports = getValue;


/***/ }),
/* 131 */
/***/ (function(module, exports) {

/**
 * Removes `key` and its value from the hash.
 *
 * @private
 * @name delete
 * @memberOf Hash
 * @param {Object} hash The hash to modify.
 * @param {string} key The key of the value to remove.
 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
 */
function hashDelete(key) {
  var result = this.has(key) && delete this.__data__[key];
  this.size -= result ? 1 : 0;
  return result;
}

module.exports = hashDelete;


/***/ }),
/* 132 */
/***/ (function(module, exports, __webpack_require__) {

var nativeCreate = __webpack_require__(36);

/** Used to stand-in for `undefined` hash values. */
var HASH_UNDEFINED = '__lodash_hash_undefined__';

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * Gets the hash value for `key`.
 *
 * @private
 * @name get
 * @memberOf Hash
 * @param {string} key The key of the value to get.
 * @returns {*} Returns the entry value.
 */
function hashGet(key) {
  var data = this.__data__;
  if (nativeCreate) {
    var result = data[key];
    return result === HASH_UNDEFINED ? undefined : result;
  }
  return hasOwnProperty.call(data, key) ? data[key] : undefined;
}

module.exports = hashGet;


/***/ }),
/* 133 */
/***/ (function(module, exports, __webpack_require__) {

var nativeCreate = __webpack_require__(36);

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * Checks if a hash value for `key` exists.
 *
 * @private
 * @name has
 * @memberOf Hash
 * @param {string} key The key of the entry to check.
 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
 */
function hashHas(key) {
  var data = this.__data__;
  return nativeCreate ? (data[key] !== undefined) : hasOwnProperty.call(data, key);
}

module.exports = hashHas;


/***/ }),
/* 134 */
/***/ (function(module, exports, __webpack_require__) {

var nativeCreate = __webpack_require__(36);

/** Used to stand-in for `undefined` hash values. */
var HASH_UNDEFINED = '__lodash_hash_undefined__';

/**
 * Sets the hash `key` to `value`.
 *
 * @private
 * @name set
 * @memberOf Hash
 * @param {string} key The key of the value to set.
 * @param {*} value The value to set.
 * @returns {Object} Returns the hash instance.
 */
function hashSet(key, value) {
  var data = this.__data__;
  this.size += this.has(key) ? 0 : 1;
  data[key] = (nativeCreate && value === undefined) ? HASH_UNDEFINED : value;
  return this;
}

module.exports = hashSet;


/***/ }),
/* 135 */
/***/ (function(module, exports) {

/**
 * Removes all key-value entries from the list cache.
 *
 * @private
 * @name clear
 * @memberOf ListCache
 */
function listCacheClear() {
  this.__data__ = [];
  this.size = 0;
}

module.exports = listCacheClear;


/***/ }),
/* 136 */
/***/ (function(module, exports, __webpack_require__) {

var assocIndexOf = __webpack_require__(37);

/** Used for built-in method references. */
var arrayProto = Array.prototype;

/** Built-in value references. */
var splice = arrayProto.splice;

/**
 * Removes `key` and its value from the list cache.
 *
 * @private
 * @name delete
 * @memberOf ListCache
 * @param {string} key The key of the value to remove.
 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
 */
function listCacheDelete(key) {
  var data = this.__data__,
      index = assocIndexOf(data, key);

  if (index < 0) {
    return false;
  }
  var lastIndex = data.length - 1;
  if (index == lastIndex) {
    data.pop();
  } else {
    splice.call(data, index, 1);
  }
  --this.size;
  return true;
}

module.exports = listCacheDelete;


/***/ }),
/* 137 */
/***/ (function(module, exports, __webpack_require__) {

var assocIndexOf = __webpack_require__(37);

/**
 * Gets the list cache value for `key`.
 *
 * @private
 * @name get
 * @memberOf ListCache
 * @param {string} key The key of the value to get.
 * @returns {*} Returns the entry value.
 */
function listCacheGet(key) {
  var data = this.__data__,
      index = assocIndexOf(data, key);

  return index < 0 ? undefined : data[index][1];
}

module.exports = listCacheGet;


/***/ }),
/* 138 */
/***/ (function(module, exports, __webpack_require__) {

var assocIndexOf = __webpack_require__(37);

/**
 * Checks if a list cache value for `key` exists.
 *
 * @private
 * @name has
 * @memberOf ListCache
 * @param {string} key The key of the entry to check.
 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
 */
function listCacheHas(key) {
  return assocIndexOf(this.__data__, key) > -1;
}

module.exports = listCacheHas;


/***/ }),
/* 139 */
/***/ (function(module, exports, __webpack_require__) {

var assocIndexOf = __webpack_require__(37);

/**
 * Sets the list cache `key` to `value`.
 *
 * @private
 * @name set
 * @memberOf ListCache
 * @param {string} key The key of the value to set.
 * @param {*} value The value to set.
 * @returns {Object} Returns the list cache instance.
 */
function listCacheSet(key, value) {
  var data = this.__data__,
      index = assocIndexOf(data, key);

  if (index < 0) {
    ++this.size;
    data.push([key, value]);
  } else {
    data[index][1] = value;
  }
  return this;
}

module.exports = listCacheSet;


/***/ }),
/* 140 */
/***/ (function(module, exports, __webpack_require__) {

var getMapData = __webpack_require__(38);

/**
 * Removes `key` and its value from the map.
 *
 * @private
 * @name delete
 * @memberOf MapCache
 * @param {string} key The key of the value to remove.
 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
 */
function mapCacheDelete(key) {
  var result = getMapData(this, key)['delete'](key);
  this.size -= result ? 1 : 0;
  return result;
}

module.exports = mapCacheDelete;


/***/ }),
/* 141 */
/***/ (function(module, exports) {

/**
 * Checks if `value` is suitable for use as unique object key.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is suitable, else `false`.
 */
function isKeyable(value) {
  var type = typeof value;
  return (type == 'string' || type == 'number' || type == 'symbol' || type == 'boolean')
    ? (value !== '__proto__')
    : (value === null);
}

module.exports = isKeyable;


/***/ }),
/* 142 */
/***/ (function(module, exports, __webpack_require__) {

var getMapData = __webpack_require__(38);

/**
 * Gets the map value for `key`.
 *
 * @private
 * @name get
 * @memberOf MapCache
 * @param {string} key The key of the value to get.
 * @returns {*} Returns the entry value.
 */
function mapCacheGet(key) {
  return getMapData(this, key).get(key);
}

module.exports = mapCacheGet;


/***/ }),
/* 143 */
/***/ (function(module, exports, __webpack_require__) {

var getMapData = __webpack_require__(38);

/**
 * Checks if a map value for `key` exists.
 *
 * @private
 * @name has
 * @memberOf MapCache
 * @param {string} key The key of the entry to check.
 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
 */
function mapCacheHas(key) {
  return getMapData(this, key).has(key);
}

module.exports = mapCacheHas;


/***/ }),
/* 144 */
/***/ (function(module, exports, __webpack_require__) {

var getMapData = __webpack_require__(38);

/**
 * Sets the map `key` to `value`.
 *
 * @private
 * @name set
 * @memberOf MapCache
 * @param {string} key The key of the value to set.
 * @param {*} value The value to set.
 * @returns {Object} Returns the map cache instance.
 */
function mapCacheSet(key, value) {
  var data = getMapData(this, key),
      size = data.size;

  data.set(key, value);
  this.size += data.size == size ? 0 : 1;
  return this;
}

module.exports = mapCacheSet;


/***/ }),
/* 145 */,
/* 146 */,
/* 147 */,
/* 148 */,
/* 149 */
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const md5 = __webpack_require__(150);

function originalToGeneratedId(originalId) {
  const match = originalId.match(/(.*)\/originalSource/);
  return match ? match[1] : "";
}

function generatedToOriginalId(generatedId, url) {
  return `${generatedId}/originalSource-${md5(url)}`;
}

function isOriginalId(id) {
  return !!id.match(/\/originalSource/);
}

function isGeneratedId(id) {
  return !isOriginalId(id);
}

/**
 * Trims the query part or reference identifier of a URL string, if necessary.
 */
function trimUrlQuery(url) {
  let length = url.length;
  let q1 = url.indexOf("?");
  let q2 = url.indexOf("&");
  let q3 = url.indexOf("#");
  let q = Math.min(q1 != -1 ? q1 : length, q2 != -1 ? q2 : length, q3 != -1 ? q3 : length);

  return url.slice(0, q);
}

// Map suffix to content type.
const contentMap = {
  "js": "text/javascript",
  "jsm": "text/javascript",
  "ts": "text/typescript",
  "tsx": "text/typescript-jsx",
  "jsx": "text/jsx",
  "coffee": "text/coffeescript",
  "elm": "text/elm",
  "cljs": "text/x-clojure"
};

/**
 * Returns the content type for the specified URL.  If no specific
 * content type can be determined, "text/plain" is returned.
 *
 * @return String
 *         The content type.
 */
function getContentType(url) {
  url = trimUrlQuery(url);
  let dot = url.lastIndexOf(".");
  if (dot >= 0) {
    let name = url.substring(dot + 1);
    if (name in contentMap) {
      return contentMap[name];
    }
  }
  return "text/plain";
}

module.exports = {
  originalToGeneratedId,
  generatedToOriginalId,
  isOriginalId,
  isGeneratedId,
  getContentType,
  contentMapForTesting: contentMap
};

/***/ }),
/* 150 */
/***/ (function(module, exports, __webpack_require__) {

(function(){
  var crypt = __webpack_require__(151),
      utf8 = __webpack_require__(71).utf8,
      isBuffer = __webpack_require__(152),
      bin = __webpack_require__(71).bin,

  // The core
  md5 = function (message, options) {
    // Convert to byte array
    if (message.constructor == String)
      if (options && options.encoding === 'binary')
        message = bin.stringToBytes(message);
      else
        message = utf8.stringToBytes(message);
    else if (isBuffer(message))
      message = Array.prototype.slice.call(message, 0);
    else if (!Array.isArray(message))
      message = message.toString();
    // else, assume byte array already

    var m = crypt.bytesToWords(message),
        l = message.length * 8,
        a =  1732584193,
        b = -271733879,
        c = -1732584194,
        d =  271733878;

    // Swap endian
    for (var i = 0; i < m.length; i++) {
      m[i] = ((m[i] <<  8) | (m[i] >>> 24)) & 0x00FF00FF |
             ((m[i] << 24) | (m[i] >>>  8)) & 0xFF00FF00;
    }

    // Padding
    m[l >>> 5] |= 0x80 << (l % 32);
    m[(((l + 64) >>> 9) << 4) + 14] = l;

    // Method shortcuts
    var FF = md5._ff,
        GG = md5._gg,
        HH = md5._hh,
        II = md5._ii;

    for (var i = 0; i < m.length; i += 16) {

      var aa = a,
          bb = b,
          cc = c,
          dd = d;

      a = FF(a, b, c, d, m[i+ 0],  7, -680876936);
      d = FF(d, a, b, c, m[i+ 1], 12, -389564586);
      c = FF(c, d, a, b, m[i+ 2], 17,  606105819);
      b = FF(b, c, d, a, m[i+ 3], 22, -1044525330);
      a = FF(a, b, c, d, m[i+ 4],  7, -176418897);
      d = FF(d, a, b, c, m[i+ 5], 12,  1200080426);
      c = FF(c, d, a, b, m[i+ 6], 17, -1473231341);
      b = FF(b, c, d, a, m[i+ 7], 22, -45705983);
      a = FF(a, b, c, d, m[i+ 8],  7,  1770035416);
      d = FF(d, a, b, c, m[i+ 9], 12, -1958414417);
      c = FF(c, d, a, b, m[i+10], 17, -42063);
      b = FF(b, c, d, a, m[i+11], 22, -1990404162);
      a = FF(a, b, c, d, m[i+12],  7,  1804603682);
      d = FF(d, a, b, c, m[i+13], 12, -40341101);
      c = FF(c, d, a, b, m[i+14], 17, -1502002290);
      b = FF(b, c, d, a, m[i+15], 22,  1236535329);

      a = GG(a, b, c, d, m[i+ 1],  5, -165796510);
      d = GG(d, a, b, c, m[i+ 6],  9, -1069501632);
      c = GG(c, d, a, b, m[i+11], 14,  643717713);
      b = GG(b, c, d, a, m[i+ 0], 20, -373897302);
      a = GG(a, b, c, d, m[i+ 5],  5, -701558691);
      d = GG(d, a, b, c, m[i+10],  9,  38016083);
      c = GG(c, d, a, b, m[i+15], 14, -660478335);
      b = GG(b, c, d, a, m[i+ 4], 20, -405537848);
      a = GG(a, b, c, d, m[i+ 9],  5,  568446438);
      d = GG(d, a, b, c, m[i+14],  9, -1019803690);
      c = GG(c, d, a, b, m[i+ 3], 14, -187363961);
      b = GG(b, c, d, a, m[i+ 8], 20,  1163531501);
      a = GG(a, b, c, d, m[i+13],  5, -1444681467);
      d = GG(d, a, b, c, m[i+ 2],  9, -51403784);
      c = GG(c, d, a, b, m[i+ 7], 14,  1735328473);
      b = GG(b, c, d, a, m[i+12], 20, -1926607734);

      a = HH(a, b, c, d, m[i+ 5],  4, -378558);
      d = HH(d, a, b, c, m[i+ 8], 11, -2022574463);
      c = HH(c, d, a, b, m[i+11], 16,  1839030562);
      b = HH(b, c, d, a, m[i+14], 23, -35309556);
      a = HH(a, b, c, d, m[i+ 1],  4, -1530992060);
      d = HH(d, a, b, c, m[i+ 4], 11,  1272893353);
      c = HH(c, d, a, b, m[i+ 7], 16, -155497632);
      b = HH(b, c, d, a, m[i+10], 23, -1094730640);
      a = HH(a, b, c, d, m[i+13],  4,  681279174);
      d = HH(d, a, b, c, m[i+ 0], 11, -358537222);
      c = HH(c, d, a, b, m[i+ 3], 16, -722521979);
      b = HH(b, c, d, a, m[i+ 6], 23,  76029189);
      a = HH(a, b, c, d, m[i+ 9],  4, -640364487);
      d = HH(d, a, b, c, m[i+12], 11, -421815835);
      c = HH(c, d, a, b, m[i+15], 16,  530742520);
      b = HH(b, c, d, a, m[i+ 2], 23, -995338651);

      a = II(a, b, c, d, m[i+ 0],  6, -198630844);
      d = II(d, a, b, c, m[i+ 7], 10,  1126891415);
      c = II(c, d, a, b, m[i+14], 15, -1416354905);
      b = II(b, c, d, a, m[i+ 5], 21, -57434055);
      a = II(a, b, c, d, m[i+12],  6,  1700485571);
      d = II(d, a, b, c, m[i+ 3], 10, -1894986606);
      c = II(c, d, a, b, m[i+10], 15, -1051523);
      b = II(b, c, d, a, m[i+ 1], 21, -2054922799);
      a = II(a, b, c, d, m[i+ 8],  6,  1873313359);
      d = II(d, a, b, c, m[i+15], 10, -30611744);
      c = II(c, d, a, b, m[i+ 6], 15, -1560198380);
      b = II(b, c, d, a, m[i+13], 21,  1309151649);
      a = II(a, b, c, d, m[i+ 4],  6, -145523070);
      d = II(d, a, b, c, m[i+11], 10, -1120210379);
      c = II(c, d, a, b, m[i+ 2], 15,  718787259);
      b = II(b, c, d, a, m[i+ 9], 21, -343485551);

      a = (a + aa) >>> 0;
      b = (b + bb) >>> 0;
      c = (c + cc) >>> 0;
      d = (d + dd) >>> 0;
    }

    return crypt.endian([a, b, c, d]);
  };

  // Auxiliary functions
  md5._ff  = function (a, b, c, d, x, s, t) {
    var n = a + (b & c | ~b & d) + (x >>> 0) + t;
    return ((n << s) | (n >>> (32 - s))) + b;
  };
  md5._gg  = function (a, b, c, d, x, s, t) {
    var n = a + (b & d | c & ~d) + (x >>> 0) + t;
    return ((n << s) | (n >>> (32 - s))) + b;
  };
  md5._hh  = function (a, b, c, d, x, s, t) {
    var n = a + (b ^ c ^ d) + (x >>> 0) + t;
    return ((n << s) | (n >>> (32 - s))) + b;
  };
  md5._ii  = function (a, b, c, d, x, s, t) {
    var n = a + (c ^ (b | ~d)) + (x >>> 0) + t;
    return ((n << s) | (n >>> (32 - s))) + b;
  };

  // Package private blocksize
  md5._blocksize = 16;
  md5._digestsize = 16;

  module.exports = function (message, options) {
    if (message === undefined || message === null)
      throw new Error('Illegal argument ' + message);

    var digestbytes = crypt.wordsToBytes(md5(message, options));
    return options && options.asBytes ? digestbytes :
        options && options.asString ? bin.bytesToString(digestbytes) :
        crypt.bytesToHex(digestbytes);
  };

})();


/***/ }),
/* 151 */
/***/ (function(module, exports) {

(function() {
  var base64map
      = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/',

  crypt = {
    // Bit-wise rotation left
    rotl: function(n, b) {
      return (n << b) | (n >>> (32 - b));
    },

    // Bit-wise rotation right
    rotr: function(n, b) {
      return (n << (32 - b)) | (n >>> b);
    },

    // Swap big-endian to little-endian and vice versa
    endian: function(n) {
      // If number given, swap endian
      if (n.constructor == Number) {
        return crypt.rotl(n, 8) & 0x00FF00FF | crypt.rotl(n, 24) & 0xFF00FF00;
      }

      // Else, assume array and swap all items
      for (var i = 0; i < n.length; i++)
        n[i] = crypt.endian(n[i]);
      return n;
    },

    // Generate an array of any length of random bytes
    randomBytes: function(n) {
      for (var bytes = []; n > 0; n--)
        bytes.push(Math.floor(Math.random() * 256));
      return bytes;
    },

    // Convert a byte array to big-endian 32-bit words
    bytesToWords: function(bytes) {
      for (var words = [], i = 0, b = 0; i < bytes.length; i++, b += 8)
        words[b >>> 5] |= bytes[i] << (24 - b % 32);
      return words;
    },

    // Convert big-endian 32-bit words to a byte array
    wordsToBytes: function(words) {
      for (var bytes = [], b = 0; b < words.length * 32; b += 8)
        bytes.push((words[b >>> 5] >>> (24 - b % 32)) & 0xFF);
      return bytes;
    },

    // Convert a byte array to a hex string
    bytesToHex: function(bytes) {
      for (var hex = [], i = 0; i < bytes.length; i++) {
        hex.push((bytes[i] >>> 4).toString(16));
        hex.push((bytes[i] & 0xF).toString(16));
      }
      return hex.join('');
    },

    // Convert a hex string to a byte array
    hexToBytes: function(hex) {
      for (var bytes = [], c = 0; c < hex.length; c += 2)
        bytes.push(parseInt(hex.substr(c, 2), 16));
      return bytes;
    },

    // Convert a byte array to a base-64 string
    bytesToBase64: function(bytes) {
      for (var base64 = [], i = 0; i < bytes.length; i += 3) {
        var triplet = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
        for (var j = 0; j < 4; j++)
          if (i * 8 + j * 6 <= bytes.length * 8)
            base64.push(base64map.charAt((triplet >>> 6 * (3 - j)) & 0x3F));
          else
            base64.push('=');
      }
      return base64.join('');
    },

    // Convert a base-64 string to a byte array
    base64ToBytes: function(base64) {
      // Remove non-base-64 characters
      base64 = base64.replace(/[^A-Z0-9+\/]/ig, '');

      for (var bytes = [], i = 0, imod4 = 0; i < base64.length;
          imod4 = ++i % 4) {
        if (imod4 == 0) continue;
        bytes.push(((base64map.indexOf(base64.charAt(i - 1))
            & (Math.pow(2, -2 * imod4 + 8) - 1)) << (imod4 * 2))
            | (base64map.indexOf(base64.charAt(i)) >>> (6 - imod4 * 2)));
      }
      return bytes;
    }
  };

  module.exports = crypt;
})();


/***/ }),
/* 152 */
/***/ (function(module, exports) {

/*!
 * Determine if an object is a Buffer
 *
 * @author   Feross Aboukhadijeh <https://feross.org>
 * @license  MIT
 */

// The _isBuffer check is for Safari 5-7 support, because it's missing
// Object.prototype.constructor. Remove this eventually
module.exports = function (obj) {
  return obj != null && (isBuffer(obj) || isSlowBuffer(obj) || !!obj._isBuffer)
}

function isBuffer (obj) {
  return !!obj.constructor && typeof obj.constructor.isBuffer === 'function' && obj.constructor.isBuffer(obj)
}

// For Node v0.10 support. Remove this eventually.
function isSlowBuffer (obj) {
  return typeof obj.readFloatLE === 'function' && typeof obj.slice === 'function' && isBuffer(obj.slice(0, 0))
}


/***/ }),
/* 153 */,
/* 154 */,
/* 155 */,
/* 156 */,
/* 157 */,
/* 158 */,
/* 159 */,
/* 160 */,
/* 161 */,
/* 162 */,
/* 163 */,
/* 164 */,
/* 165 */,
/* 166 */,
/* 167 */,
/* 168 */,
/* 169 */,
/* 170 */,
/* 171 */,
/* 172 */,
/* 173 */,
/* 174 */,
/* 175 */,
/* 176 */,
/* 177 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isType;

var _definitions = __webpack_require__(35);

function isType(nodeType, targetType) {
  if (nodeType === targetType) return true;
  if (_definitions.ALIAS_KEYS[targetType]) return false;
  var aliases = _definitions.FLIPPED_ALIAS_KEYS[targetType];

  if (aliases) {
    if (aliases[0] === nodeType) return true;

    for (var _iterator = aliases, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
      var _ref;

      if (_isArray) {
        if (_i >= _iterator.length) break;
        _ref = _iterator[_i++];
      } else {
        _i = _iterator.next();
        if (_i.done) break;
        _ref = _i.value;
      }

      var _alias = _ref;
      if (nodeType === _alias) return true;
    }
  }

  return false;
}

/***/ }),
/* 178 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.patternLikeCommon = exports.functionDeclarationCommon = exports.functionTypeAnnotationCommon = exports.functionCommon = void 0;

var _isValidIdentifier = _interopRequireDefault(__webpack_require__(83));

var _constants = __webpack_require__(50);

var _utils = _interopRequireWildcard(__webpack_require__(43));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

(0, _utils.default)("ArrayExpression", {
  fields: {
    elements: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeOrValueType)("null", "Expression", "SpreadElement"))),
      default: []
    }
  },
  visitor: ["elements"],
  aliases: ["Expression"]
});
(0, _utils.default)("AssignmentExpression", {
  fields: {
    operator: {
      validate: (0, _utils.assertValueType)("string")
    },
    left: {
      validate: (0, _utils.assertNodeType)("LVal")
    },
    right: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  },
  builder: ["operator", "left", "right"],
  visitor: ["left", "right"],
  aliases: ["Expression"]
});
(0, _utils.default)("BinaryExpression", {
  builder: ["operator", "left", "right"],
  fields: {
    operator: {
      validate: _utils.assertOneOf.apply(void 0, _constants.BINARY_OPERATORS)
    },
    left: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    right: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  },
  visitor: ["left", "right"],
  aliases: ["Binary", "Expression"]
});
(0, _utils.default)("Directive", {
  visitor: ["value"],
  fields: {
    value: {
      validate: (0, _utils.assertNodeType)("DirectiveLiteral")
    }
  }
});
(0, _utils.default)("DirectiveLiteral", {
  builder: ["value"],
  fields: {
    value: {
      validate: (0, _utils.assertValueType)("string")
    }
  }
});
(0, _utils.default)("BlockStatement", {
  builder: ["body", "directives"],
  visitor: ["directives", "body"],
  fields: {
    directives: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Directive"))),
      default: []
    },
    body: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Statement")))
    }
  },
  aliases: ["Scopable", "BlockParent", "Block", "Statement"]
});
(0, _utils.default)("BreakStatement", {
  visitor: ["label"],
  fields: {
    label: {
      validate: (0, _utils.assertNodeType)("Identifier"),
      optional: true
    }
  },
  aliases: ["Statement", "Terminatorless", "CompletionStatement"]
});
(0, _utils.default)("CallExpression", {
  visitor: ["callee", "arguments", "typeParameters"],
  builder: ["callee", "arguments"],
  aliases: ["Expression"],
  fields: {
    callee: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    arguments: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Expression", "SpreadElement", "JSXNamespacedName")))
    },
    optional: {
      validate: (0, _utils.assertOneOf)(true, false),
      optional: true
    },
    typeParameters: {
      validate: (0, _utils.assertNodeType)("TypeParameterInstantiation", "TSTypeParameterInstantiation"),
      optional: true
    }
  }
});
(0, _utils.default)("CatchClause", {
  visitor: ["param", "body"],
  fields: {
    param: {
      validate: (0, _utils.assertNodeType)("Identifier"),
      optional: true
    },
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement")
    }
  },
  aliases: ["Scopable", "BlockParent"]
});
(0, _utils.default)("ConditionalExpression", {
  visitor: ["test", "consequent", "alternate"],
  fields: {
    test: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    consequent: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    alternate: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  },
  aliases: ["Expression", "Conditional"]
});
(0, _utils.default)("ContinueStatement", {
  visitor: ["label"],
  fields: {
    label: {
      validate: (0, _utils.assertNodeType)("Identifier"),
      optional: true
    }
  },
  aliases: ["Statement", "Terminatorless", "CompletionStatement"]
});
(0, _utils.default)("DebuggerStatement", {
  aliases: ["Statement"]
});
(0, _utils.default)("DoWhileStatement", {
  visitor: ["test", "body"],
  fields: {
    test: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    body: {
      validate: (0, _utils.assertNodeType)("Statement")
    }
  },
  aliases: ["Statement", "BlockParent", "Loop", "While", "Scopable"]
});
(0, _utils.default)("EmptyStatement", {
  aliases: ["Statement"]
});
(0, _utils.default)("ExpressionStatement", {
  visitor: ["expression"],
  fields: {
    expression: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  },
  aliases: ["Statement", "ExpressionWrapper"]
});
(0, _utils.default)("File", {
  builder: ["program", "comments", "tokens"],
  visitor: ["program"],
  fields: {
    program: {
      validate: (0, _utils.assertNodeType)("Program")
    }
  }
});
(0, _utils.default)("ForInStatement", {
  visitor: ["left", "right", "body"],
  aliases: ["Scopable", "Statement", "For", "BlockParent", "Loop", "ForXStatement"],
  fields: {
    left: {
      validate: (0, _utils.assertNodeType)("VariableDeclaration", "LVal")
    },
    right: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    body: {
      validate: (0, _utils.assertNodeType)("Statement")
    }
  }
});
(0, _utils.default)("ForStatement", {
  visitor: ["init", "test", "update", "body"],
  aliases: ["Scopable", "Statement", "For", "BlockParent", "Loop"],
  fields: {
    init: {
      validate: (0, _utils.assertNodeType)("VariableDeclaration", "Expression"),
      optional: true
    },
    test: {
      validate: (0, _utils.assertNodeType)("Expression"),
      optional: true
    },
    update: {
      validate: (0, _utils.assertNodeType)("Expression"),
      optional: true
    },
    body: {
      validate: (0, _utils.assertNodeType)("Statement")
    }
  }
});
var functionCommon = {
  params: {
    validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("LVal")))
  },
  generator: {
    default: false,
    validate: (0, _utils.assertValueType)("boolean")
  },
  async: {
    validate: (0, _utils.assertValueType)("boolean"),
    default: false
  }
};
exports.functionCommon = functionCommon;
var functionTypeAnnotationCommon = {
  returnType: {
    validate: (0, _utils.assertNodeType)("TypeAnnotation", "TSTypeAnnotation", "Noop"),
    optional: true
  },
  typeParameters: {
    validate: (0, _utils.assertNodeType)("TypeParameterDeclaration", "TSTypeParameterDeclaration", "Noop"),
    optional: true
  }
};
exports.functionTypeAnnotationCommon = functionTypeAnnotationCommon;
var functionDeclarationCommon = Object.assign({}, functionCommon, {
  declare: {
    validate: (0, _utils.assertValueType)("boolean"),
    optional: true
  },
  id: {
    validate: (0, _utils.assertNodeType)("Identifier"),
    optional: true
  }
});
exports.functionDeclarationCommon = functionDeclarationCommon;
(0, _utils.default)("FunctionDeclaration", {
  builder: ["id", "params", "body", "generator", "async"],
  visitor: ["id", "params", "body", "returnType", "typeParameters"],
  fields: Object.assign({}, functionDeclarationCommon, functionTypeAnnotationCommon, {
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement")
    }
  }),
  aliases: ["Scopable", "Function", "BlockParent", "FunctionParent", "Statement", "Pureish", "Declaration"]
});
(0, _utils.default)("FunctionExpression", {
  inherits: "FunctionDeclaration",
  aliases: ["Scopable", "Function", "BlockParent", "FunctionParent", "Expression", "Pureish"],
  fields: Object.assign({}, functionCommon, functionTypeAnnotationCommon, {
    id: {
      validate: (0, _utils.assertNodeType)("Identifier"),
      optional: true
    },
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement")
    }
  })
});
var patternLikeCommon = {
  typeAnnotation: {
    validate: (0, _utils.assertNodeType)("TypeAnnotation", "TSTypeAnnotation", "Noop"),
    optional: true
  },
  decorators: {
    validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator")))
  }
};
exports.patternLikeCommon = patternLikeCommon;
(0, _utils.default)("Identifier", {
  builder: ["name"],
  visitor: ["typeAnnotation"],
  aliases: ["Expression", "PatternLike", "LVal", "TSEntityName"],
  fields: Object.assign({}, patternLikeCommon, {
    name: {
      validate: (0, _utils.chain)(function (node, key, val) {
        if (!(0, _isValidIdentifier.default)(val)) {}
      }, (0, _utils.assertValueType)("string"))
    },
    optional: {
      validate: (0, _utils.assertValueType)("boolean"),
      optional: true
    }
  })
});
(0, _utils.default)("IfStatement", {
  visitor: ["test", "consequent", "alternate"],
  aliases: ["Statement", "Conditional"],
  fields: {
    test: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    consequent: {
      validate: (0, _utils.assertNodeType)("Statement")
    },
    alternate: {
      optional: true,
      validate: (0, _utils.assertNodeType)("Statement")
    }
  }
});
(0, _utils.default)("LabeledStatement", {
  visitor: ["label", "body"],
  aliases: ["Statement"],
  fields: {
    label: {
      validate: (0, _utils.assertNodeType)("Identifier")
    },
    body: {
      validate: (0, _utils.assertNodeType)("Statement")
    }
  }
});
(0, _utils.default)("StringLiteral", {
  builder: ["value"],
  fields: {
    value: {
      validate: (0, _utils.assertValueType)("string")
    }
  },
  aliases: ["Expression", "Pureish", "Literal", "Immutable"]
});
(0, _utils.default)("NumericLiteral", {
  builder: ["value"],
  deprecatedAlias: "NumberLiteral",
  fields: {
    value: {
      validate: (0, _utils.assertValueType)("number")
    }
  },
  aliases: ["Expression", "Pureish", "Literal", "Immutable"]
});
(0, _utils.default)("NullLiteral", {
  aliases: ["Expression", "Pureish", "Literal", "Immutable"]
});
(0, _utils.default)("BooleanLiteral", {
  builder: ["value"],
  fields: {
    value: {
      validate: (0, _utils.assertValueType)("boolean")
    }
  },
  aliases: ["Expression", "Pureish", "Literal", "Immutable"]
});
(0, _utils.default)("RegExpLiteral", {
  builder: ["pattern", "flags"],
  deprecatedAlias: "RegexLiteral",
  aliases: ["Expression", "Literal"],
  fields: {
    pattern: {
      validate: (0, _utils.assertValueType)("string")
    },
    flags: {
      validate: (0, _utils.assertValueType)("string"),
      default: ""
    }
  }
});
(0, _utils.default)("LogicalExpression", {
  builder: ["operator", "left", "right"],
  visitor: ["left", "right"],
  aliases: ["Binary", "Expression"],
  fields: {
    operator: {
      validate: _utils.assertOneOf.apply(void 0, _constants.LOGICAL_OPERATORS)
    },
    left: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    right: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("MemberExpression", {
  builder: ["object", "property", "computed", "optional"],
  visitor: ["object", "property"],
  aliases: ["Expression", "LVal"],
  fields: {
    object: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    property: {
      validate: function () {
        var normal = (0, _utils.assertNodeType)("Identifier");
        var computed = (0, _utils.assertNodeType)("Expression");
        return function (node, key, val) {
          var validator = node.computed ? computed : normal;
          validator(node, key, val);
        };
      }()
    },
    computed: {
      default: false
    },
    optional: {
      validate: (0, _utils.assertOneOf)(true, false),
      optional: true
    }
  }
});
(0, _utils.default)("NewExpression", {
  inherits: "CallExpression"
});
(0, _utils.default)("Program", {
  visitor: ["directives", "body"],
  builder: ["body", "directives", "sourceType"],
  fields: {
    sourceFile: {
      validate: (0, _utils.assertValueType)("string")
    },
    sourceType: {
      validate: (0, _utils.assertOneOf)("script", "module"),
      default: "script"
    },
    directives: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Directive"))),
      default: []
    },
    body: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Statement")))
    }
  },
  aliases: ["Scopable", "BlockParent", "Block"]
});
(0, _utils.default)("ObjectExpression", {
  visitor: ["properties"],
  aliases: ["Expression"],
  fields: {
    properties: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("ObjectMethod", "ObjectProperty", "SpreadElement")))
    }
  }
});
(0, _utils.default)("ObjectMethod", {
  builder: ["kind", "key", "params", "body", "computed"],
  fields: Object.assign({}, functionCommon, functionTypeAnnotationCommon, {
    kind: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("string"), (0, _utils.assertOneOf)("method", "get", "set")),
      default: "method"
    },
    computed: {
      validate: (0, _utils.assertValueType)("boolean"),
      default: false
    },
    key: {
      validate: function () {
        var normal = (0, _utils.assertNodeType)("Identifier", "StringLiteral", "NumericLiteral");
        var computed = (0, _utils.assertNodeType)("Expression");
        return function (node, key, val) {
          var validator = node.computed ? computed : normal;
          validator(node, key, val);
        };
      }()
    },
    decorators: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator")))
    },
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement")
    }
  }),
  visitor: ["key", "params", "body", "decorators", "returnType", "typeParameters"],
  aliases: ["UserWhitespacable", "Function", "Scopable", "BlockParent", "FunctionParent", "Method", "ObjectMember"]
});
(0, _utils.default)("ObjectProperty", {
  builder: ["key", "value", "computed", "shorthand", "decorators"],
  fields: {
    computed: {
      validate: (0, _utils.assertValueType)("boolean"),
      default: false
    },
    key: {
      validate: function () {
        var normal = (0, _utils.assertNodeType)("Identifier", "StringLiteral", "NumericLiteral");
        var computed = (0, _utils.assertNodeType)("Expression");
        return function (node, key, val) {
          var validator = node.computed ? computed : normal;
          validator(node, key, val);
        };
      }()
    },
    value: {
      validate: (0, _utils.assertNodeType)("Expression", "PatternLike")
    },
    shorthand: {
      validate: (0, _utils.assertValueType)("boolean"),
      default: false
    },
    decorators: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator"))),
      optional: true
    }
  },
  visitor: ["key", "value", "decorators"],
  aliases: ["UserWhitespacable", "Property", "ObjectMember"]
});
(0, _utils.default)("RestElement", {
  visitor: ["argument", "typeAnnotation"],
  builder: ["argument"],
  aliases: ["LVal", "PatternLike"],
  deprecatedAlias: "RestProperty",
  fields: Object.assign({}, patternLikeCommon, {
    argument: {
      validate: (0, _utils.assertNodeType)("LVal")
    }
  })
});
(0, _utils.default)("ReturnStatement", {
  visitor: ["argument"],
  aliases: ["Statement", "Terminatorless", "CompletionStatement"],
  fields: {
    argument: {
      validate: (0, _utils.assertNodeType)("Expression"),
      optional: true
    }
  }
});
(0, _utils.default)("SequenceExpression", {
  visitor: ["expressions"],
  fields: {
    expressions: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Expression")))
    }
  },
  aliases: ["Expression"]
});
(0, _utils.default)("SwitchCase", {
  visitor: ["test", "consequent"],
  fields: {
    test: {
      validate: (0, _utils.assertNodeType)("Expression"),
      optional: true
    },
    consequent: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Statement")))
    }
  }
});
(0, _utils.default)("SwitchStatement", {
  visitor: ["discriminant", "cases"],
  aliases: ["Statement", "BlockParent", "Scopable"],
  fields: {
    discriminant: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    cases: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("SwitchCase")))
    }
  }
});
(0, _utils.default)("ThisExpression", {
  aliases: ["Expression"]
});
(0, _utils.default)("ThrowStatement", {
  visitor: ["argument"],
  aliases: ["Statement", "Terminatorless", "CompletionStatement"],
  fields: {
    argument: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("TryStatement", {
  visitor: ["block", "handler", "finalizer"],
  aliases: ["Statement"],
  fields: {
    block: {
      validate: (0, _utils.assertNodeType)("BlockStatement")
    },
    handler: {
      optional: true,
      validate: (0, _utils.assertNodeType)("CatchClause")
    },
    finalizer: {
      optional: true,
      validate: (0, _utils.assertNodeType)("BlockStatement")
    }
  }
});
(0, _utils.default)("UnaryExpression", {
  builder: ["operator", "argument", "prefix"],
  fields: {
    prefix: {
      default: true
    },
    argument: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    operator: {
      validate: _utils.assertOneOf.apply(void 0, _constants.UNARY_OPERATORS)
    }
  },
  visitor: ["argument"],
  aliases: ["UnaryLike", "Expression"]
});
(0, _utils.default)("UpdateExpression", {
  builder: ["operator", "argument", "prefix"],
  fields: {
    prefix: {
      default: false
    },
    argument: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    operator: {
      validate: _utils.assertOneOf.apply(void 0, _constants.UPDATE_OPERATORS)
    }
  },
  visitor: ["argument"],
  aliases: ["Expression"]
});
(0, _utils.default)("VariableDeclaration", {
  builder: ["kind", "declarations"],
  visitor: ["declarations"],
  aliases: ["Statement", "Declaration"],
  fields: {
    declare: {
      validate: (0, _utils.assertValueType)("boolean"),
      optional: true
    },
    kind: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("string"), (0, _utils.assertOneOf)("var", "let", "const"))
    },
    declarations: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("VariableDeclarator")))
    }
  }
});
(0, _utils.default)("VariableDeclarator", {
  visitor: ["id", "init"],
  fields: {
    id: {
      validate: (0, _utils.assertNodeType)("LVal")
    },
    init: {
      optional: true,
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("WhileStatement", {
  visitor: ["test", "body"],
  aliases: ["Statement", "BlockParent", "Loop", "While", "Scopable"],
  fields: {
    test: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement", "Statement")
    }
  }
});
(0, _utils.default)("WithStatement", {
  visitor: ["object", "body"],
  aliases: ["Statement"],
  fields: {
    object: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement", "Statement")
    }
  }
});

/***/ }),
/* 179 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.classMethodOrDeclareMethodCommon = exports.classMethodOrPropertyCommon = void 0;

var _utils = _interopRequireWildcard(__webpack_require__(43));

var _core = __webpack_require__(178);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

(0, _utils.default)("AssignmentPattern", {
  visitor: ["left", "right"],
  builder: ["left", "right"],
  aliases: ["Pattern", "PatternLike", "LVal"],
  fields: Object.assign({}, _core.patternLikeCommon, {
    left: {
      validate: (0, _utils.assertNodeType)("Identifier", "ObjectPattern", "ArrayPattern")
    },
    right: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    decorators: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator")))
    }
  })
});
(0, _utils.default)("ArrayPattern", {
  visitor: ["elements", "typeAnnotation"],
  builder: ["elements"],
  aliases: ["Pattern", "PatternLike", "LVal"],
  fields: Object.assign({}, _core.patternLikeCommon, {
    elements: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("PatternLike")))
    },
    decorators: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator")))
    }
  })
});
(0, _utils.default)("ArrowFunctionExpression", {
  builder: ["params", "body", "async"],
  visitor: ["params", "body", "returnType", "typeParameters"],
  aliases: ["Scopable", "Function", "BlockParent", "FunctionParent", "Expression", "Pureish"],
  fields: Object.assign({}, _core.functionCommon, _core.functionTypeAnnotationCommon, {
    expression: {
      validate: (0, _utils.assertValueType)("boolean")
    },
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement", "Expression")
    }
  })
});
(0, _utils.default)("ClassBody", {
  visitor: ["body"],
  fields: {
    body: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("ClassMethod", "ClassProperty", "TSDeclareMethod", "TSIndexSignature")))
    }
  }
});
var classCommon = {
  typeParameters: {
    validate: (0, _utils.assertNodeType)("TypeParameterDeclaration", "TSTypeParameterDeclaration", "Noop"),
    optional: true
  },
  body: {
    validate: (0, _utils.assertNodeType)("ClassBody")
  },
  superClass: {
    optional: true,
    validate: (0, _utils.assertNodeType)("Expression")
  },
  superTypeParameters: {
    validate: (0, _utils.assertNodeType)("TypeParameterInstantiation", "TSTypeParameterInstantiation"),
    optional: true
  },
  implements: {
    validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("TSExpressionWithTypeArguments", "ClassImplements"))),
    optional: true
  }
};
(0, _utils.default)("ClassDeclaration", {
  builder: ["id", "superClass", "body", "decorators"],
  visitor: ["id", "body", "superClass", "mixins", "typeParameters", "superTypeParameters", "implements", "decorators"],
  aliases: ["Scopable", "Class", "Statement", "Declaration", "Pureish"],
  fields: Object.assign({}, classCommon, {
    declare: {
      validate: (0, _utils.assertValueType)("boolean"),
      optional: true
    },
    abstract: {
      validate: (0, _utils.assertValueType)("boolean"),
      optional: true
    },
    id: {
      validate: (0, _utils.assertNodeType)("Identifier"),
      optional: true
    },
    decorators: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator"))),
      optional: true
    }
  })
});
(0, _utils.default)("ClassExpression", {
  inherits: "ClassDeclaration",
  aliases: ["Scopable", "Class", "Expression", "Pureish"],
  fields: Object.assign({}, classCommon, {
    id: {
      optional: true,
      validate: (0, _utils.assertNodeType)("Identifier")
    },
    body: {
      validate: (0, _utils.assertNodeType)("ClassBody")
    },
    superClass: {
      optional: true,
      validate: (0, _utils.assertNodeType)("Expression")
    },
    decorators: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator"))),
      optional: true
    }
  })
});
(0, _utils.default)("ExportAllDeclaration", {
  visitor: ["source"],
  aliases: ["Statement", "Declaration", "ModuleDeclaration", "ExportDeclaration"],
  fields: {
    source: {
      validate: (0, _utils.assertNodeType)("StringLiteral")
    }
  }
});
(0, _utils.default)("ExportDefaultDeclaration", {
  visitor: ["declaration"],
  aliases: ["Statement", "Declaration", "ModuleDeclaration", "ExportDeclaration"],
  fields: {
    declaration: {
      validate: (0, _utils.assertNodeType)("FunctionDeclaration", "TSDeclareFunction", "ClassDeclaration", "Expression")
    }
  }
});
(0, _utils.default)("ExportNamedDeclaration", {
  visitor: ["declaration", "specifiers", "source"],
  aliases: ["Statement", "Declaration", "ModuleDeclaration", "ExportDeclaration"],
  fields: {
    declaration: {
      validate: (0, _utils.assertNodeType)("Declaration"),
      optional: true
    },
    specifiers: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("ExportSpecifier", "ExportDefaultSpecifier", "ExportNamespaceSpecifier")))
    },
    source: {
      validate: (0, _utils.assertNodeType)("StringLiteral"),
      optional: true
    }
  }
});
(0, _utils.default)("ExportSpecifier", {
  visitor: ["local", "exported"],
  aliases: ["ModuleSpecifier"],
  fields: {
    local: {
      validate: (0, _utils.assertNodeType)("Identifier")
    },
    exported: {
      validate: (0, _utils.assertNodeType)("Identifier")
    }
  }
});
(0, _utils.default)("ForOfStatement", {
  visitor: ["left", "right", "body"],
  aliases: ["Scopable", "Statement", "For", "BlockParent", "Loop", "ForXStatement"],
  fields: {
    left: {
      validate: (0, _utils.assertNodeType)("VariableDeclaration", "LVal")
    },
    right: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    body: {
      validate: (0, _utils.assertNodeType)("Statement")
    },
    await: {
      default: false,
      validate: (0, _utils.assertValueType)("boolean")
    }
  }
});
(0, _utils.default)("ImportDeclaration", {
  visitor: ["specifiers", "source"],
  aliases: ["Statement", "Declaration", "ModuleDeclaration"],
  fields: {
    specifiers: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("ImportSpecifier", "ImportDefaultSpecifier", "ImportNamespaceSpecifier")))
    },
    source: {
      validate: (0, _utils.assertNodeType)("StringLiteral")
    }
  }
});
(0, _utils.default)("ImportDefaultSpecifier", {
  visitor: ["local"],
  aliases: ["ModuleSpecifier"],
  fields: {
    local: {
      validate: (0, _utils.assertNodeType)("Identifier")
    }
  }
});
(0, _utils.default)("ImportNamespaceSpecifier", {
  visitor: ["local"],
  aliases: ["ModuleSpecifier"],
  fields: {
    local: {
      validate: (0, _utils.assertNodeType)("Identifier")
    }
  }
});
(0, _utils.default)("ImportSpecifier", {
  visitor: ["local", "imported"],
  aliases: ["ModuleSpecifier"],
  fields: {
    local: {
      validate: (0, _utils.assertNodeType)("Identifier")
    },
    imported: {
      validate: (0, _utils.assertNodeType)("Identifier")
    },
    importKind: {
      validate: (0, _utils.assertOneOf)(null, "type", "typeof")
    }
  }
});
(0, _utils.default)("MetaProperty", {
  visitor: ["meta", "property"],
  aliases: ["Expression"],
  fields: {
    meta: {
      validate: (0, _utils.assertNodeType)("Identifier")
    },
    property: {
      validate: (0, _utils.assertNodeType)("Identifier")
    }
  }
});
var classMethodOrPropertyCommon = {
  abstract: {
    validate: (0, _utils.assertValueType)("boolean"),
    optional: true
  },
  accessibility: {
    validate: (0, _utils.chain)((0, _utils.assertValueType)("string"), (0, _utils.assertOneOf)("public", "private", "protected")),
    optional: true
  },
  static: {
    validate: (0, _utils.assertValueType)("boolean"),
    optional: true
  },
  computed: {
    default: false,
    validate: (0, _utils.assertValueType)("boolean")
  },
  optional: {
    validate: (0, _utils.assertValueType)("boolean"),
    optional: true
  },
  key: {
    validate: (0, _utils.chain)(function () {
      var normal = (0, _utils.assertNodeType)("Identifier", "StringLiteral", "NumericLiteral");
      var computed = (0, _utils.assertNodeType)("Expression");
      return function (node, key, val) {
        var validator = node.computed ? computed : normal;
        validator(node, key, val);
      };
    }(), (0, _utils.assertNodeType)("Identifier", "StringLiteral", "NumericLiteral", "Expression"))
  }
};
exports.classMethodOrPropertyCommon = classMethodOrPropertyCommon;
var classMethodOrDeclareMethodCommon = Object.assign({}, _core.functionCommon, classMethodOrPropertyCommon, {
  kind: {
    validate: (0, _utils.chain)((0, _utils.assertValueType)("string"), (0, _utils.assertOneOf)("get", "set", "method", "constructor")),
    default: "method"
  },
  access: {
    validate: (0, _utils.chain)((0, _utils.assertValueType)("string"), (0, _utils.assertOneOf)("public", "private", "protected")),
    optional: true
  },
  decorators: {
    validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator"))),
    optional: true
  }
});
exports.classMethodOrDeclareMethodCommon = classMethodOrDeclareMethodCommon;
(0, _utils.default)("ClassMethod", {
  aliases: ["Function", "Scopable", "BlockParent", "FunctionParent", "Method"],
  builder: ["kind", "key", "params", "body", "computed", "static"],
  visitor: ["key", "params", "body", "decorators", "returnType", "typeParameters"],
  fields: Object.assign({}, classMethodOrDeclareMethodCommon, _core.functionTypeAnnotationCommon, {
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement")
    }
  })
});
(0, _utils.default)("ObjectPattern", {
  visitor: ["properties", "typeAnnotation"],
  builder: ["properties"],
  aliases: ["Pattern", "PatternLike", "LVal"],
  fields: Object.assign({}, _core.patternLikeCommon, {
    properties: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("RestElement", "ObjectProperty")))
    }
  })
});
(0, _utils.default)("SpreadElement", {
  visitor: ["argument"],
  aliases: ["UnaryLike"],
  deprecatedAlias: "SpreadProperty",
  fields: {
    argument: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("Super", {
  aliases: ["Expression"]
});
(0, _utils.default)("TaggedTemplateExpression", {
  visitor: ["tag", "quasi"],
  aliases: ["Expression"],
  fields: {
    tag: {
      validate: (0, _utils.assertNodeType)("Expression")
    },
    quasi: {
      validate: (0, _utils.assertNodeType)("TemplateLiteral")
    }
  }
});
(0, _utils.default)("TemplateElement", {
  builder: ["value", "tail"],
  fields: {
    value: {},
    tail: {
      validate: (0, _utils.assertValueType)("boolean"),
      default: false
    }
  }
});
(0, _utils.default)("TemplateLiteral", {
  visitor: ["quasis", "expressions"],
  aliases: ["Expression", "Literal"],
  fields: {
    quasis: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("TemplateElement")))
    },
    expressions: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Expression")))
    }
  }
});
(0, _utils.default)("YieldExpression", {
  builder: ["argument", "delegate"],
  visitor: ["argument"],
  aliases: ["Expression", "Terminatorless"],
  fields: {
    delegate: {
      validate: (0, _utils.assertValueType)("boolean"),
      default: false
    },
    argument: {
      optional: true,
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});

/***/ }),
/* 180 */
/***/ (function(module, exports, __webpack_require__) {

var ListCache = __webpack_require__(53),
    stackClear = __webpack_require__(680),
    stackDelete = __webpack_require__(681),
    stackGet = __webpack_require__(682),
    stackHas = __webpack_require__(683),
    stackSet = __webpack_require__(684);

/**
 * Creates a stack cache object to store key-value pairs.
 *
 * @private
 * @constructor
 * @param {Array} [entries] The key-value pairs to cache.
 */
function Stack(entries) {
  var data = this.__data__ = new ListCache(entries);
  this.size = data.size;
}

// Add methods to `Stack`.
Stack.prototype.clear = stackClear;
Stack.prototype['delete'] = stackDelete;
Stack.prototype.get = stackGet;
Stack.prototype.has = stackHas;
Stack.prototype.set = stackSet;

module.exports = Stack;


/***/ }),
/* 181 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsTypedArray = __webpack_require__(690),
    baseUnary = __webpack_require__(84),
    nodeUtil = __webpack_require__(115);

/* Node.js helper references. */
var nodeIsTypedArray = nodeUtil && nodeUtil.isTypedArray;

/**
 * Checks if `value` is classified as a typed array.
 *
 * @static
 * @memberOf _
 * @since 3.0.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a typed array, else `false`.
 * @example
 *
 * _.isTypedArray(new Uint8Array);
 * // => true
 *
 * _.isTypedArray([]);
 * // => false
 */
var isTypedArray = nodeIsTypedArray ? baseUnary(nodeIsTypedArray) : baseIsTypedArray;

module.exports = isTypedArray;


/***/ }),
/* 182 */
/***/ (function(module, exports) {

/** Used as references for various `Number` constants. */
var MAX_SAFE_INTEGER = 9007199254740991;

/**
 * Checks if `value` is a valid array-like length.
 *
 * **Note:** This method is loosely based on
 * [`ToLength`](http://ecma-international.org/ecma-262/7.0/#sec-tolength).
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a valid length, else `false`.
 * @example
 *
 * _.isLength(3);
 * // => true
 *
 * _.isLength(Number.MIN_VALUE);
 * // => false
 *
 * _.isLength(Infinity);
 * // => false
 *
 * _.isLength('3');
 * // => false
 */
function isLength(value) {
  return typeof value == 'number' &&
    value > -1 && value % 1 == 0 && value <= MAX_SAFE_INTEGER;
}

module.exports = isLength;


/***/ }),
/* 183 */
/***/ (function(module, exports, __webpack_require__) {

var arrayFilter = __webpack_require__(698),
    stubArray = __webpack_require__(264);

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Built-in value references. */
var propertyIsEnumerable = objectProto.propertyIsEnumerable;

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeGetSymbols = Object.getOwnPropertySymbols;

/**
 * Creates an array of the own enumerable symbols of `object`.
 *
 * @private
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of symbols.
 */
var getSymbols = !nativeGetSymbols ? stubArray : function(object) {
  if (object == null) {
    return [];
  }
  object = Object(object);
  return arrayFilter(nativeGetSymbols(object), function(symbol) {
    return propertyIsEnumerable.call(object, symbol);
  });
};

module.exports = getSymbols;


/***/ }),
/* 184 */
/***/ (function(module, exports) {

/**
 * Appends the elements of `values` to `array`.
 *
 * @private
 * @param {Array} array The array to modify.
 * @param {Array} values The values to append.
 * @returns {Array} Returns `array`.
 */
function arrayPush(array, values) {
  var index = -1,
      length = values.length,
      offset = array.length;

  while (++index < length) {
    array[offset + index] = values[index];
  }
  return array;
}

module.exports = arrayPush;


/***/ }),
/* 185 */
/***/ (function(module, exports, __webpack_require__) {

var overArg = __webpack_require__(262);

/** Built-in value references. */
var getPrototype = overArg(Object.getPrototypeOf, Object);

module.exports = getPrototype;


/***/ }),
/* 186 */
/***/ (function(module, exports, __webpack_require__) {

var Uint8Array = __webpack_require__(269);

/**
 * Creates a clone of `arrayBuffer`.
 *
 * @private
 * @param {ArrayBuffer} arrayBuffer The array buffer to clone.
 * @returns {ArrayBuffer} Returns the cloned array buffer.
 */
function cloneArrayBuffer(arrayBuffer) {
  var result = new arrayBuffer.constructor(arrayBuffer.byteLength);
  new Uint8Array(result).set(new Uint8Array(arrayBuffer));
  return result;
}

module.exports = cloneArrayBuffer;


/***/ }),
/* 187 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = inherit;

var _uniq = _interopRequireDefault(__webpack_require__(276));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inherit(key, child, parent) {
  if (child && parent) {
    child[key] = (0, _uniq.default)([].concat(child[key], parent[key]).filter(Boolean));
  }
}

/***/ }),
/* 188 */
/***/ (function(module, exports, __webpack_require__) {

var MapCache = __webpack_require__(65),
    setCacheAdd = __webpack_require__(724),
    setCacheHas = __webpack_require__(725);

/**
 *
 * Creates an array cache object to store unique values.
 *
 * @private
 * @constructor
 * @param {Array} [values] The values to cache.
 */
function SetCache(values) {
  var index = -1,
      length = values == null ? 0 : values.length;

  this.__data__ = new MapCache;
  while (++index < length) {
    this.add(values[index]);
  }
}

// Add methods to `SetCache`.
SetCache.prototype.add = SetCache.prototype.push = setCacheAdd;
SetCache.prototype.has = setCacheHas;

module.exports = SetCache;


/***/ }),
/* 189 */
/***/ (function(module, exports) {

/**
 * The base implementation of `_.findIndex` and `_.findLastIndex` without
 * support for iteratee shorthands.
 *
 * @private
 * @param {Array} array The array to inspect.
 * @param {Function} predicate The function invoked per iteration.
 * @param {number} fromIndex The index to search from.
 * @param {boolean} [fromRight] Specify iterating from right to left.
 * @returns {number} Returns the index of the matched value, else `-1`.
 */
function baseFindIndex(array, predicate, fromIndex, fromRight) {
  var length = array.length,
      index = fromIndex + (fromRight ? 1 : -1);

  while ((fromRight ? index-- : ++index < length)) {
    if (predicate(array[index], index, array)) {
      return index;
    }
  }
  return -1;
}

module.exports = baseFindIndex;


/***/ }),
/* 190 */
/***/ (function(module, exports) {

/**
 * Checks if a `cache` value for `key` exists.
 *
 * @private
 * @param {Object} cache The cache to query.
 * @param {string} key The key of the entry to check.
 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
 */
function cacheHas(cache, key) {
  return cache.has(key);
}

module.exports = cacheHas;


/***/ }),
/* 191 */
/***/ (function(module, exports) {

/**
 * Converts `set` to an array of its values.
 *
 * @private
 * @param {Object} set The set to convert.
 * @returns {Array} Returns the values.
 */
function setToArray(set) {
  var index = -1,
      result = Array(set.size);

  set.forEach(function(value) {
    result[++index] = value;
  });
  return result;
}

module.exports = setToArray;


/***/ }),
/* 192 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isFunction = isFunction;
exports.isAwaitExpression = isAwaitExpression;
exports.isYieldExpression = isYieldExpression;
exports.isVariable = isVariable;
exports.getMemberExpression = getMemberExpression;
exports.getVariables = getVariables;

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function isFunction(node) {
  return t.isFunction(node) || t.isArrowFunctionExpression(node) || t.isObjectMethod(node) || t.isClassMethod(node);
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function isAwaitExpression(path) {
  const { node, parent } = path;
  return t.isAwaitExpression(node) || t.isAwaitExpression(parent.init) || t.isAwaitExpression(parent);
}

function isYieldExpression(path) {
  const { node, parent } = path;
  return t.isYieldExpression(node) || t.isYieldExpression(parent.init) || t.isYieldExpression(parent);
}

function isVariable(path) {
  const node = path.node;
  return t.isVariableDeclaration(node) || isFunction(path) && path.node.params != null && path.node.params.length || t.isObjectProperty(node) && !isFunction(path.node.value);
}

function getMemberExpression(root) {
  function _getMemberExpression(node, expr) {
    if (t.isMemberExpression(node)) {
      expr = [node.property.name].concat(expr);
      return _getMemberExpression(node.object, expr);
    }

    if (t.isCallExpression(node)) {
      return [];
    }

    if (t.isThisExpression(node)) {
      return ["this"].concat(expr);
    }

    return [node.name].concat(expr);
  }

  const expr = _getMemberExpression(root, []);
  return expr.join(".");
}

function getVariables(dec) {
  if (!dec.id) {
    return [];
  }

  if (t.isArrayPattern(dec.id)) {
    if (!dec.id.elements) {
      return [];
    }

    // NOTE: it's possible that an element is empty
    // e.g. const [, a] = arr
    return dec.id.elements.filter(element => element).map(element => {
      return {
        name: t.isAssignmentPattern(element) ? element.left.name : element.name || element.argument.name,
        location: element.loc
      };
    });
  }

  return [{
    name: dec.id.name,
    location: dec.loc
  }];
}

/***/ }),
/* 193 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; /* This Source Code Form is subject to the terms of the Mozilla Public
                                                                                                                                                                                                                                                                   * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                                                                                                                                                                                                                                   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

exports.clearSymbols = clearSymbols;
exports.default = getSymbols;

var _flatten = __webpack_require__(762);

var _flatten2 = _interopRequireDefault(_flatten);

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

var _simplePath = __webpack_require__(288);

var _simplePath2 = _interopRequireDefault(_simplePath);

var _ast = __webpack_require__(51);

var _helpers = __webpack_require__(192);

var _inferClassName = __webpack_require__(764);

var _getFunctionName = __webpack_require__(294);

var _getFunctionName2 = _interopRequireDefault(_getFunctionName);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

let symbolDeclarations = new Map();

function getFunctionParameterNames(path) {
  if (path.node.params != null) {
    return path.node.params.map(param => {
      if (param.type !== "AssignmentPattern") {
        return param.name;
      }

      // Parameter with default value
      if (param.left.type === "Identifier" && param.right.type === "Identifier") {
        return `${param.left.name} = ${param.right.name}`;
      } else if (param.left.type === "Identifier" && param.right.type === "StringLiteral") {
        return `${param.left.name} = ${param.right.value}`;
      } else if (param.left.type === "Identifier" && param.right.type === "ObjectExpression") {
        return `${param.left.name} = {}`;
      } else if (param.left.type === "Identifier" && param.right.type === "ArrayExpression") {
        return `${param.left.name} = []`;
      } else if (param.left.type === "Identifier" && param.right.type === "NullLiteral") {
        return `${param.left.name} = null`;
      }
    });
  }
  return [];
}

function getVariableNames(path) {
  if (t.isObjectProperty(path.node) && !(0, _helpers.isFunction)(path.node.value)) {
    if (path.node.key.type === "StringLiteral") {
      return [{
        name: path.node.key.value,
        location: path.node.loc
      }];
    } else if (path.node.value.type === "Identifier") {
      return [{ name: path.node.value.name, location: path.node.loc }];
    } else if (path.node.value.type === "AssignmentPattern") {
      return [{ name: path.node.value.left.name, location: path.node.loc }];
    }

    return [{
      name: path.node.key.name,
      location: path.node.loc
    }];
  }

  if (!path.node.declarations) {
    return path.node.params.map(dec => ({
      name: dec.name,
      location: dec.loc
    }));
  }

  const declarations = path.node.declarations.filter(dec => dec.id.type !== "ObjectPattern").map(_helpers.getVariables);

  return (0, _flatten2.default)(declarations);
}

function getComments(ast) {
  if (!ast || !ast.comments) {
    return [];
  }
  return ast.comments.map(comment => ({
    name: comment.location,
    location: comment.loc
  }));
}

function getSpecifiers(specifiers) {
  if (!specifiers) {
    return null;
  }

  return specifiers.map(specifier => specifier.local && specifier.local.name);
}

function extractSymbol(path, symbols) {
  if ((0, _helpers.isVariable)(path)) {
    symbols.variables.push(...getVariableNames(path));
  }

  if ((0, _helpers.isFunction)(path)) {
    symbols.functions.push({
      name: (0, _getFunctionName2.default)(path.node, path.parent),
      klass: (0, _inferClassName.inferClassName)(path),
      location: path.node.loc,
      parameterNames: getFunctionParameterNames(path),
      identifier: path.node.id
    });
  }

  if (t.isJSXElement(path)) {
    symbols.hasJsx = true;
  }

  if (t.isClassDeclaration(path)) {
    symbols.classes.push({
      name: path.node.id.name,
      parent: path.node.superClass,
      location: path.node.loc
    });
  }

  if (t.isImportDeclaration(path)) {
    symbols.imports.push({
      source: path.node.source.value,
      location: path.node.loc,
      specifiers: getSpecifiers(path.node.specifiers)
    });
  }

  if (t.isObjectProperty(path)) {
    const { start, end, identifierName } = path.node.key.loc;
    symbols.objectProperties.push({
      name: identifierName,
      location: { start, end },
      expression: getSnippet(path)
    });
  }

  if (t.isMemberExpression(path)) {
    const { start, end } = path.node.property.loc;
    symbols.memberExpressions.push({
      name: path.node.property.name,
      location: { start, end },
      expressionLocation: path.node.loc,
      expression: getSnippet(path),
      computed: path.node.computed
    });
  }

  if (t.isCallExpression(path)) {
    const callee = path.node.callee;
    const args = path.node.arguments;
    if (!t.isMemberExpression(callee)) {
      const { start, end, identifierName } = callee.loc;
      symbols.callExpressions.push({
        name: identifierName,
        values: args.filter(arg => arg.value).map(arg => arg.value),
        location: { start, end }
      });
    }
  }

  if (t.isIdentifier(path) && !t.isGenericTypeAnnotation(path.parent)) {
    let { start, end } = path.node.loc;

    // We want to include function params, but exclude the function name
    if (t.isClassMethod(path.parent) && !path.inList) {
      return;
    }

    if (t.isProperty(path.parent)) {
      return;
    }

    if (path.node.typeAnnotation) {
      const column = path.node.typeAnnotation.loc.start.column;
      end = _extends({}, end, { column });
    }

    symbols.identifiers.push({
      name: path.node.name,
      expression: path.node.name,
      location: { start, end }
    });
  }

  if (t.isThisExpression(path.node)) {
    const { start, end } = path.node.loc;
    symbols.identifiers.push({
      name: "this",
      location: { start, end },
      expressionLocation: path.node.loc,
      expression: "this"
    });
  }

  if (t.isVariableDeclarator(path)) {
    const node = path.node.id;
    const { start, end } = path.node.loc;
    if (t.isArrayPattern(node)) {
      return;
    }
    symbols.identifiers.push({
      name: node.name,
      expression: node.name,
      location: { start, end }
    });
  }
}

function extractSymbols(sourceId) {
  const symbols = {
    functions: [],
    variables: [],
    callExpressions: [],
    memberExpressions: [],
    objectProperties: [],
    comments: [],
    identifiers: [],
    classes: [],
    imports: [],
    hasJsx: false
  };

  const ast = (0, _ast.traverseAst)(sourceId, {
    enter(node, ancestors) {
      try {
        const path = (0, _simplePath2.default)(ancestors);
        if (path) {
          extractSymbol(path, symbols);
        }
      } catch (e) {
        console.error(e);
      }
    }
  });

  // comments are extracted separately from the AST
  symbols.comments = getComments(ast);

  return symbols;
}

function extendSnippet(name, expression, path = null, prevPath = null) {
  const computed = path && path.node.computed;
  const prevComputed = prevPath && prevPath.node.computed;
  const prevArray = t.isArrayExpression(prevPath);
  const array = t.isArrayExpression(path);

  if (expression === "") {
    if (computed) {
      return `[${name}]`;
    }
    return name;
  }

  if (computed || array) {
    if (prevComputed || prevArray) {
      return `[${name}]${expression}`;
    }
    return `[${name}].${expression}`;
  }

  if (prevComputed || prevArray) {
    return `${name}${expression}`;
  }

  return `${name}.${expression}`;
}

function getMemberSnippet(node, expression = "") {
  if (t.isMemberExpression(node)) {
    const name = node.property.name;

    return getMemberSnippet(node.object, extendSnippet(name, expression));
  }

  if (t.isCallExpression(node)) {
    return "";
  }

  if (t.isThisExpression(node)) {
    return `this.${expression}`;
  }

  if (t.isIdentifier(node)) {
    return `${node.name}.${expression}`;
  }

  return expression;
}

function getObjectSnippet(path, prevPath, expression = "") {
  if (!path) {
    return expression;
  }

  const name = path.node.key.name;

  const extendedExpression = extendSnippet(name, expression, path, prevPath);

  const nextPrevPath = path;
  const nextPath = path.parentPath && path.parentPath.parentPath;

  return getSnippet(nextPath, nextPrevPath, extendedExpression);
}

function getArraySnippet(path, prevPath, expression) {
  if (!prevPath.parentPath) {
    throw new Error("Assertion failure - path should exist");
  }

  const index = `${prevPath.parentPath.containerIndex}`;
  const extendedExpression = extendSnippet(index, expression, path, prevPath);

  const nextPrevPath = path;
  const nextPath = path.parentPath && path.parentPath.parentPath;

  return getSnippet(nextPath, nextPrevPath, extendedExpression);
}

function getSnippet(path, prevPath = null, expression = "") {
  if (!path) {
    return expression;
  }

  if (t.isVariableDeclaration(path)) {
    const node = path.node.declarations[0];
    const name = node.id.name;
    return extendSnippet(name, expression, path, prevPath);
  }

  if (t.isVariableDeclarator(path)) {
    const node = path.node.id;
    if (t.isObjectPattern(node)) {
      return expression;
    }

    const name = node.name;
    const prop = extendSnippet(name, expression, path, prevPath);
    return prop;
  }

  if (t.isAssignmentExpression(path)) {
    const node = path.node.left;
    const name = t.isMemberExpression(node) ? getMemberSnippet(node) : node.name;

    const prop = extendSnippet(name, expression, path, prevPath);
    return prop;
  }

  if ((0, _helpers.isFunction)(path)) {
    return expression;
  }

  if (t.isIdentifier(path)) {
    const node = path.node;
    return `${node.name}.${expression}`;
  }

  if (t.isObjectProperty(path)) {
    return getObjectSnippet(path, prevPath, expression);
  }

  if (t.isObjectExpression(path)) {
    const parentPath = prevPath && prevPath.parentPath;
    return getObjectSnippet(parentPath, prevPath, expression);
  }

  if (t.isMemberExpression(path)) {
    return getMemberSnippet(path.node, expression);
  }

  if (t.isArrayExpression(path)) {
    if (!prevPath) {
      throw new Error("Assertion failure - path should exist");
    }

    return getArraySnippet(path, prevPath, expression);
  }
}

function clearSymbols() {
  symbolDeclarations = new Map();
}

function getSymbols(sourceId) {
  if (symbolDeclarations.has(sourceId)) {
    const symbols = symbolDeclarations.get(sourceId);
    if (symbols) {
      return symbols;
    }
  }

  const symbols = extractSymbols(sourceId);

  symbolDeclarations.set(sourceId, symbols);
  return symbols;
}

/***/ }),
/* 194 */
/***/ (function(module, exports) {

/**
 * This method returns the first argument it receives.
 *
 * @static
 * @since 0.1.0
 * @memberOf _
 * @category Util
 * @param {*} value Any value.
 * @returns {*} Returns `value`.
 * @example
 *
 * var object = { 'a': 1 };
 *
 * console.log(_.identity(object) === object);
 * // => true
 */
function identity(value) {
  return value;
}

module.exports = identity;


/***/ }),
/* 195 */,
/* 196 */,
/* 197 */,
/* 198 */,
/* 199 */,
/* 200 */,
/* 201 */,
/* 202 */,
/* 203 */,
/* 204 */,
/* 205 */,
/* 206 */,
/* 207 */,
/* 208 */,
/* 209 */,
/* 210 */,
/* 211 */,
/* 212 */,
/* 213 */,
/* 214 */,
/* 215 */,
/* 216 */,
/* 217 */,
/* 218 */,
/* 219 */,
/* 220 */,
/* 221 */,
/* 222 */,
/* 223 */,
/* 224 */,
/* 225 */,
/* 226 */,
/* 227 */,
/* 228 */,
/* 229 */,
/* 230 */,
/* 231 */,
/* 232 */,
/* 233 */,
/* 234 */,
/* 235 */,
/* 236 */,
/* 237 */,
/* 238 */,
/* 239 */,
/* 240 */,
/* 241 */,
/* 242 */,
/* 243 */,
/* 244 */,
/* 245 */,
/* 246 */,
/* 247 */,
/* 248 */,
/* 249 */,
/* 250 */,
/* 251 */,
/* 252 */,
/* 253 */,
/* 254 */,
/* 255 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getClosestExpression = getClosestExpression;
exports.getClosestPath = getClosestPath;

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

var _simplePath = __webpack_require__(288);

var _simplePath2 = _interopRequireDefault(_simplePath);

var _ast = __webpack_require__(51);

var _helpers = __webpack_require__(192);

var _contains = __webpack_require__(292);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function getNodeValue(node) {
  if (t.isThisExpression(node)) {
    return "this";
  }

  return node.name;
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function getClosestMemberExpression(sourceId, token, location) {
  const closest = getClosestPath(sourceId, location).find(path => t.isMemberExpression(path.node) && path.node.property.name === token);

  if (closest) {
    const memberExpression = (0, _helpers.getMemberExpression)(closest.node);
    return {
      expression: memberExpression,
      location: closest.node.loc
    };
  }
  return null;
}

function getClosestExpression(sourceId, token, location) {
  const memberExpression = getClosestMemberExpression(sourceId, token, location);
  if (memberExpression) {
    return memberExpression;
  }

  const path = getClosestPath(sourceId, location);
  if (!path || !path.node) {
    return;
  }

  const { node } = path;
  return { expression: getNodeValue(node), location: node.loc };
}

function getClosestPath(sourceId, location) {
  let closestPath = null;

  (0, _ast.traverseAst)(sourceId, {
    enter(node, ancestors) {
      if ((0, _contains.nodeContainsPosition)(node, location)) {
        const path = (0, _simplePath2.default)(ancestors);

        if (path && (!closestPath || path.depth > closestPath.depth)) {
          closestPath = path;
        }
      }
    }
  });

  if (!closestPath) {
    throw new Error("Assertion failure - This should always fine a path");
  }

  return closestPath;
}

/***/ }),
/* 256 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = buildMatchMemberExpression;

var _matchesPattern = _interopRequireDefault(__webpack_require__(257));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function buildMatchMemberExpression(match, allowPartial) {
  var parts = match.split(".");
  return function (member) {
    return (0, _matchesPattern.default)(member, parts, allowPartial);
  };
}

/***/ }),
/* 257 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = matchesPattern;

var _generated = __webpack_require__(17);

function matchesPattern(member, match, allowPartial) {
  if (!(0, _generated.isMemberExpression)(member)) return false;
  var parts = Array.isArray(match) ? match : match.split(".");
  var nodes = [];
  var node;

  for (node = member; (0, _generated.isMemberExpression)(node); node = node.object) {
    nodes.push(node.property);
  }

  nodes.push(node);
  if (nodes.length < parts.length) return false;
  if (!allowPartial && nodes.length > parts.length) return false;

  for (var i = 0, j = nodes.length - 1; i < parts.length; i++, j--) {
    var _node = nodes[j];
    var value = void 0;

    if ((0, _generated.isIdentifier)(_node)) {
      value = _node.name;
    } else if ((0, _generated.isStringLiteral)(_node)) {
      value = _node.value;
    } else {
      return false;
    }

    if (parts[i] !== value) return false;
  }

  return true;
}

/***/ }),
/* 258 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = shallowEqual;

function shallowEqual(actual, expected) {
  var keys = Object.keys(expected);
  var _arr = keys;

  for (var _i = 0; _i < _arr.length; _i++) {
    var key = _arr[_i];

    if (actual[key] !== expected[key]) {
      return false;
    }
  }

  return true;
}

/***/ }),
/* 259 */
/***/ (function(module, exports) {

/*
  Copyright (C) 2013-2014 Yusuke Suzuki <utatane.tea@gmail.com>
  Copyright (C) 2014 Ivan Nikulin <ifaaan@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

(function () {
    'use strict';

    var ES6Regex, ES5Regex, NON_ASCII_WHITESPACES, IDENTIFIER_START, IDENTIFIER_PART, ch;

    // See `tools/generate-identifier-regex.js`.
    ES5Regex = {
        // ECMAScript 5.1/Unicode v7.0.0 NonAsciiIdentifierStart:
        NonAsciiIdentifierStart: /[\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0559\u0561-\u0587\u05D0-\u05EA\u05F0-\u05F2\u0620-\u064A\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE\u06EF\u06FA-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07CA-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u08A0-\u08B2\u0904-\u0939\u093D\u0950\u0958-\u0961\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09F0\u09F1\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B71\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58\u0C59\u0C60\u0C61\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDE\u0CE0\u0CE1\u0CF1\u0CF2\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D60\u0D61\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0EDC-\u0EDF\u0F00\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1380-\u138F\u13A0-\u13F4\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u1820-\u1877\u1880-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1950-\u196D\u1970-\u1974\u1980-\u19AB\u19C1-\u19C7\u1A00-\u1A16\u1A20-\u1A54\u1AA7\u1B05-\u1B33\u1B45-\u1B4B\u1B83-\u1BA0\u1BAE\u1BAF\u1BBA-\u1BE5\u1C00-\u1C23\u1C4D-\u1C4F\u1C5A-\u1C7D\u1CE9-\u1CEC\u1CEE-\u1CF1\u1CF5\u1CF6\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2071\u207F\u2090-\u209C\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FCC\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA61F\uA62A\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA78E\uA790-\uA7AD\uA7B0\uA7B1\uA7F7-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA840-\uA873\uA882-\uA8B3\uA8F2-\uA8F7\uA8FB\uA90A-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF\uA9E0-\uA9E4\uA9E6-\uA9EF\uA9FA-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB5F\uAB64\uAB65\uABC0-\uABE2\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]/,
        // ECMAScript 5.1/Unicode v7.0.0 NonAsciiIdentifierPart:
        NonAsciiIdentifierPart: /[\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0300-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u0483-\u0487\u048A-\u052F\u0531-\u0556\u0559\u0561-\u0587\u0591-\u05BD\u05BF\u05C1\u05C2\u05C4\u05C5\u05C7\u05D0-\u05EA\u05F0-\u05F2\u0610-\u061A\u0620-\u0669\u066E-\u06D3\u06D5-\u06DC\u06DF-\u06E8\u06EA-\u06FC\u06FF\u0710-\u074A\u074D-\u07B1\u07C0-\u07F5\u07FA\u0800-\u082D\u0840-\u085B\u08A0-\u08B2\u08E4-\u0963\u0966-\u096F\u0971-\u0983\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BC-\u09C4\u09C7\u09C8\u09CB-\u09CE\u09D7\u09DC\u09DD\u09DF-\u09E3\u09E6-\u09F1\u0A01-\u0A03\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A3C\u0A3E-\u0A42\u0A47\u0A48\u0A4B-\u0A4D\u0A51\u0A59-\u0A5C\u0A5E\u0A66-\u0A75\u0A81-\u0A83\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABC-\u0AC5\u0AC7-\u0AC9\u0ACB-\u0ACD\u0AD0\u0AE0-\u0AE3\u0AE6-\u0AEF\u0B01-\u0B03\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3C-\u0B44\u0B47\u0B48\u0B4B-\u0B4D\u0B56\u0B57\u0B5C\u0B5D\u0B5F-\u0B63\u0B66-\u0B6F\u0B71\u0B82\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BBE-\u0BC2\u0BC6-\u0BC8\u0BCA-\u0BCD\u0BD0\u0BD7\u0BE6-\u0BEF\u0C00-\u0C03\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D-\u0C44\u0C46-\u0C48\u0C4A-\u0C4D\u0C55\u0C56\u0C58\u0C59\u0C60-\u0C63\u0C66-\u0C6F\u0C81-\u0C83\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBC-\u0CC4\u0CC6-\u0CC8\u0CCA-\u0CCD\u0CD5\u0CD6\u0CDE\u0CE0-\u0CE3\u0CE6-\u0CEF\u0CF1\u0CF2\u0D01-\u0D03\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D-\u0D44\u0D46-\u0D48\u0D4A-\u0D4E\u0D57\u0D60-\u0D63\u0D66-\u0D6F\u0D7A-\u0D7F\u0D82\u0D83\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DCA\u0DCF-\u0DD4\u0DD6\u0DD8-\u0DDF\u0DE6-\u0DEF\u0DF2\u0DF3\u0E01-\u0E3A\u0E40-\u0E4E\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB9\u0EBB-\u0EBD\u0EC0-\u0EC4\u0EC6\u0EC8-\u0ECD\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F18\u0F19\u0F20-\u0F29\u0F35\u0F37\u0F39\u0F3E-\u0F47\u0F49-\u0F6C\u0F71-\u0F84\u0F86-\u0F97\u0F99-\u0FBC\u0FC6\u1000-\u1049\u1050-\u109D\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u135D-\u135F\u1380-\u138F\u13A0-\u13F4\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1714\u1720-\u1734\u1740-\u1753\u1760-\u176C\u176E-\u1770\u1772\u1773\u1780-\u17D3\u17D7\u17DC\u17DD\u17E0-\u17E9\u180B-\u180D\u1810-\u1819\u1820-\u1877\u1880-\u18AA\u18B0-\u18F5\u1900-\u191E\u1920-\u192B\u1930-\u193B\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19D9\u1A00-\u1A1B\u1A20-\u1A5E\u1A60-\u1A7C\u1A7F-\u1A89\u1A90-\u1A99\u1AA7\u1AB0-\u1ABD\u1B00-\u1B4B\u1B50-\u1B59\u1B6B-\u1B73\u1B80-\u1BF3\u1C00-\u1C37\u1C40-\u1C49\u1C4D-\u1C7D\u1CD0-\u1CD2\u1CD4-\u1CF6\u1CF8\u1CF9\u1D00-\u1DF5\u1DFC-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u200C\u200D\u203F\u2040\u2054\u2071\u207F\u2090-\u209C\u20D0-\u20DC\u20E1\u20E5-\u20F0\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D7F-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2DE0-\u2DFF\u2E2F\u3005-\u3007\u3021-\u302F\u3031-\u3035\u3038-\u303C\u3041-\u3096\u3099\u309A\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FCC\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66F\uA674-\uA67D\uA67F-\uA69D\uA69F-\uA6F1\uA717-\uA71F\uA722-\uA788\uA78B-\uA78E\uA790-\uA7AD\uA7B0\uA7B1\uA7F7-\uA827\uA840-\uA873\uA880-\uA8C4\uA8D0-\uA8D9\uA8E0-\uA8F7\uA8FB\uA900-\uA92D\uA930-\uA953\uA960-\uA97C\uA980-\uA9C0\uA9CF-\uA9D9\uA9E0-\uA9FE\uAA00-\uAA36\uAA40-\uAA4D\uAA50-\uAA59\uAA60-\uAA76\uAA7A-\uAAC2\uAADB-\uAADD\uAAE0-\uAAEF\uAAF2-\uAAF6\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB5F\uAB64\uAB65\uABC0-\uABEA\uABEC\uABED\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE00-\uFE0F\uFE20-\uFE2D\uFE33\uFE34\uFE4D-\uFE4F\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF3F\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]/
    };

    ES6Regex = {
        // ECMAScript 6/Unicode v7.0.0 NonAsciiIdentifierStart:
        NonAsciiIdentifierStart: /[\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0559\u0561-\u0587\u05D0-\u05EA\u05F0-\u05F2\u0620-\u064A\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE\u06EF\u06FA-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07CA-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u08A0-\u08B2\u0904-\u0939\u093D\u0950\u0958-\u0961\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09F0\u09F1\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B71\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58\u0C59\u0C60\u0C61\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDE\u0CE0\u0CE1\u0CF1\u0CF2\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D60\u0D61\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0EDC-\u0EDF\u0F00\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1380-\u138F\u13A0-\u13F4\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u1820-\u1877\u1880-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1950-\u196D\u1970-\u1974\u1980-\u19AB\u19C1-\u19C7\u1A00-\u1A16\u1A20-\u1A54\u1AA7\u1B05-\u1B33\u1B45-\u1B4B\u1B83-\u1BA0\u1BAE\u1BAF\u1BBA-\u1BE5\u1C00-\u1C23\u1C4D-\u1C4F\u1C5A-\u1C7D\u1CE9-\u1CEC\u1CEE-\u1CF1\u1CF5\u1CF6\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2071\u207F\u2090-\u209C\u2102\u2107\u210A-\u2113\u2115\u2118-\u211D\u2124\u2126\u2128\u212A-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309B-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FCC\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA61F\uA62A\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA78E\uA790-\uA7AD\uA7B0\uA7B1\uA7F7-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA840-\uA873\uA882-\uA8B3\uA8F2-\uA8F7\uA8FB\uA90A-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF\uA9E0-\uA9E4\uA9E6-\uA9EF\uA9FA-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB5F\uAB64\uAB65\uABC0-\uABE2\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD40-\uDD74\uDE80-\uDE9C\uDEA0-\uDED0\uDF00-\uDF1F\uDF30-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDD00-\uDD27\uDD30-\uDD63\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC60-\uDC76\uDC80-\uDC9E\uDD00-\uDD15\uDD20-\uDD39\uDD80-\uDDB7\uDDBE\uDDBF\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE33\uDE60-\uDE7C\uDE80-\uDE9C\uDEC0-\uDEC7\uDEC9-\uDEE4\uDF00-\uDF35\uDF40-\uDF55\uDF60-\uDF72\uDF80-\uDF91]|\uD803[\uDC00-\uDC48]|\uD804[\uDC03-\uDC37\uDC83-\uDCAF\uDCD0-\uDCE8\uDD03-\uDD26\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDDA\uDE00-\uDE11\uDE13-\uDE2B\uDEB0-\uDEDE\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF5D-\uDF61]|\uD805[\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDD80-\uDDAE\uDE00-\uDE2F\uDE44\uDE80-\uDEAA]|\uD806[\uDCA0-\uDCDF\uDCFF\uDEC0-\uDEF8]|\uD808[\uDC00-\uDF98]|\uD809[\uDC00-\uDC6E]|[\uD80C\uD840-\uD868\uD86A-\uD86C][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2E]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDF00-\uDF44\uDF50\uDF93-\uDF9F]|\uD82C[\uDC00\uDC01]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA5\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB]|\uD83A[\uDC00-\uDCC4]|\uD83B[\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD869[\uDC00-\uDED6\uDF00-\uDFFF]|\uD86D[\uDC00-\uDF34\uDF40-\uDFFF]|\uD86E[\uDC00-\uDC1D]|\uD87E[\uDC00-\uDE1D]/,
        // ECMAScript 6/Unicode v7.0.0 NonAsciiIdentifierPart:
        NonAsciiIdentifierPart: /[\xAA\xB5\xB7\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0300-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u0483-\u0487\u048A-\u052F\u0531-\u0556\u0559\u0561-\u0587\u0591-\u05BD\u05BF\u05C1\u05C2\u05C4\u05C5\u05C7\u05D0-\u05EA\u05F0-\u05F2\u0610-\u061A\u0620-\u0669\u066E-\u06D3\u06D5-\u06DC\u06DF-\u06E8\u06EA-\u06FC\u06FF\u0710-\u074A\u074D-\u07B1\u07C0-\u07F5\u07FA\u0800-\u082D\u0840-\u085B\u08A0-\u08B2\u08E4-\u0963\u0966-\u096F\u0971-\u0983\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BC-\u09C4\u09C7\u09C8\u09CB-\u09CE\u09D7\u09DC\u09DD\u09DF-\u09E3\u09E6-\u09F1\u0A01-\u0A03\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A3C\u0A3E-\u0A42\u0A47\u0A48\u0A4B-\u0A4D\u0A51\u0A59-\u0A5C\u0A5E\u0A66-\u0A75\u0A81-\u0A83\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABC-\u0AC5\u0AC7-\u0AC9\u0ACB-\u0ACD\u0AD0\u0AE0-\u0AE3\u0AE6-\u0AEF\u0B01-\u0B03\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3C-\u0B44\u0B47\u0B48\u0B4B-\u0B4D\u0B56\u0B57\u0B5C\u0B5D\u0B5F-\u0B63\u0B66-\u0B6F\u0B71\u0B82\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BBE-\u0BC2\u0BC6-\u0BC8\u0BCA-\u0BCD\u0BD0\u0BD7\u0BE6-\u0BEF\u0C00-\u0C03\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D-\u0C44\u0C46-\u0C48\u0C4A-\u0C4D\u0C55\u0C56\u0C58\u0C59\u0C60-\u0C63\u0C66-\u0C6F\u0C81-\u0C83\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBC-\u0CC4\u0CC6-\u0CC8\u0CCA-\u0CCD\u0CD5\u0CD6\u0CDE\u0CE0-\u0CE3\u0CE6-\u0CEF\u0CF1\u0CF2\u0D01-\u0D03\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D-\u0D44\u0D46-\u0D48\u0D4A-\u0D4E\u0D57\u0D60-\u0D63\u0D66-\u0D6F\u0D7A-\u0D7F\u0D82\u0D83\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DCA\u0DCF-\u0DD4\u0DD6\u0DD8-\u0DDF\u0DE6-\u0DEF\u0DF2\u0DF3\u0E01-\u0E3A\u0E40-\u0E4E\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB9\u0EBB-\u0EBD\u0EC0-\u0EC4\u0EC6\u0EC8-\u0ECD\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F18\u0F19\u0F20-\u0F29\u0F35\u0F37\u0F39\u0F3E-\u0F47\u0F49-\u0F6C\u0F71-\u0F84\u0F86-\u0F97\u0F99-\u0FBC\u0FC6\u1000-\u1049\u1050-\u109D\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u135D-\u135F\u1369-\u1371\u1380-\u138F\u13A0-\u13F4\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1714\u1720-\u1734\u1740-\u1753\u1760-\u176C\u176E-\u1770\u1772\u1773\u1780-\u17D3\u17D7\u17DC\u17DD\u17E0-\u17E9\u180B-\u180D\u1810-\u1819\u1820-\u1877\u1880-\u18AA\u18B0-\u18F5\u1900-\u191E\u1920-\u192B\u1930-\u193B\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19DA\u1A00-\u1A1B\u1A20-\u1A5E\u1A60-\u1A7C\u1A7F-\u1A89\u1A90-\u1A99\u1AA7\u1AB0-\u1ABD\u1B00-\u1B4B\u1B50-\u1B59\u1B6B-\u1B73\u1B80-\u1BF3\u1C00-\u1C37\u1C40-\u1C49\u1C4D-\u1C7D\u1CD0-\u1CD2\u1CD4-\u1CF6\u1CF8\u1CF9\u1D00-\u1DF5\u1DFC-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u200C\u200D\u203F\u2040\u2054\u2071\u207F\u2090-\u209C\u20D0-\u20DC\u20E1\u20E5-\u20F0\u2102\u2107\u210A-\u2113\u2115\u2118-\u211D\u2124\u2126\u2128\u212A-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D7F-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2DE0-\u2DFF\u3005-\u3007\u3021-\u302F\u3031-\u3035\u3038-\u303C\u3041-\u3096\u3099-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FCC\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66F\uA674-\uA67D\uA67F-\uA69D\uA69F-\uA6F1\uA717-\uA71F\uA722-\uA788\uA78B-\uA78E\uA790-\uA7AD\uA7B0\uA7B1\uA7F7-\uA827\uA840-\uA873\uA880-\uA8C4\uA8D0-\uA8D9\uA8E0-\uA8F7\uA8FB\uA900-\uA92D\uA930-\uA953\uA960-\uA97C\uA980-\uA9C0\uA9CF-\uA9D9\uA9E0-\uA9FE\uAA00-\uAA36\uAA40-\uAA4D\uAA50-\uAA59\uAA60-\uAA76\uAA7A-\uAAC2\uAADB-\uAADD\uAAE0-\uAAEF\uAAF2-\uAAF6\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB5F\uAB64\uAB65\uABC0-\uABEA\uABEC\uABED\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE00-\uFE0F\uFE20-\uFE2D\uFE33\uFE34\uFE4D-\uFE4F\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF3F\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD40-\uDD74\uDDFD\uDE80-\uDE9C\uDEA0-\uDED0\uDEE0\uDF00-\uDF1F\uDF30-\uDF4A\uDF50-\uDF7A\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDD00-\uDD27\uDD30-\uDD63\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC60-\uDC76\uDC80-\uDC9E\uDD00-\uDD15\uDD20-\uDD39\uDD80-\uDDB7\uDDBE\uDDBF\uDE00-\uDE03\uDE05\uDE06\uDE0C-\uDE13\uDE15-\uDE17\uDE19-\uDE33\uDE38-\uDE3A\uDE3F\uDE60-\uDE7C\uDE80-\uDE9C\uDEC0-\uDEC7\uDEC9-\uDEE6\uDF00-\uDF35\uDF40-\uDF55\uDF60-\uDF72\uDF80-\uDF91]|\uD803[\uDC00-\uDC48]|\uD804[\uDC00-\uDC46\uDC66-\uDC6F\uDC7F-\uDCBA\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD00-\uDD34\uDD36-\uDD3F\uDD50-\uDD73\uDD76\uDD80-\uDDC4\uDDD0-\uDDDA\uDE00-\uDE11\uDE13-\uDE37\uDEB0-\uDEEA\uDEF0-\uDEF9\uDF01-\uDF03\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3C-\uDF44\uDF47\uDF48\uDF4B-\uDF4D\uDF57\uDF5D-\uDF63\uDF66-\uDF6C\uDF70-\uDF74]|\uD805[\uDC80-\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDB5\uDDB8-\uDDC0\uDE00-\uDE40\uDE44\uDE50-\uDE59\uDE80-\uDEB7\uDEC0-\uDEC9]|\uD806[\uDCA0-\uDCE9\uDCFF\uDEC0-\uDEF8]|\uD808[\uDC00-\uDF98]|\uD809[\uDC00-\uDC6E]|[\uD80C\uD840-\uD868\uD86A-\uD86C][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2E]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDED0-\uDEED\uDEF0-\uDEF4\uDF00-\uDF36\uDF40-\uDF43\uDF50-\uDF59\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDF00-\uDF44\uDF50-\uDF7E\uDF8F-\uDF9F]|\uD82C[\uDC00\uDC01]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99\uDC9D\uDC9E]|\uD834[\uDD65-\uDD69\uDD6D-\uDD72\uDD7B-\uDD82\uDD85-\uDD8B\uDDAA-\uDDAD\uDE42-\uDE44]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA5\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD83A[\uDC00-\uDCC4\uDCD0-\uDCD6]|\uD83B[\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD869[\uDC00-\uDED6\uDF00-\uDFFF]|\uD86D[\uDC00-\uDF34\uDF40-\uDFFF]|\uD86E[\uDC00-\uDC1D]|\uD87E[\uDC00-\uDE1D]|\uDB40[\uDD00-\uDDEF]/
    };

    function isDecimalDigit(ch) {
        return 0x30 <= ch && ch <= 0x39;  // 0..9
    }

    function isHexDigit(ch) {
        return 0x30 <= ch && ch <= 0x39 ||  // 0..9
            0x61 <= ch && ch <= 0x66 ||     // a..f
            0x41 <= ch && ch <= 0x46;       // A..F
    }

    function isOctalDigit(ch) {
        return ch >= 0x30 && ch <= 0x37;  // 0..7
    }

    // 7.2 White Space

    NON_ASCII_WHITESPACES = [
        0x1680, 0x180E,
        0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A,
        0x202F, 0x205F,
        0x3000,
        0xFEFF
    ];

    function isWhiteSpace(ch) {
        return ch === 0x20 || ch === 0x09 || ch === 0x0B || ch === 0x0C || ch === 0xA0 ||
            ch >= 0x1680 && NON_ASCII_WHITESPACES.indexOf(ch) >= 0;
    }

    // 7.3 Line Terminators

    function isLineTerminator(ch) {
        return ch === 0x0A || ch === 0x0D || ch === 0x2028 || ch === 0x2029;
    }

    // 7.6 Identifier Names and Identifiers

    function fromCodePoint(cp) {
        if (cp <= 0xFFFF) { return String.fromCharCode(cp); }
        var cu1 = String.fromCharCode(Math.floor((cp - 0x10000) / 0x400) + 0xD800);
        var cu2 = String.fromCharCode(((cp - 0x10000) % 0x400) + 0xDC00);
        return cu1 + cu2;
    }

    IDENTIFIER_START = new Array(0x80);
    for(ch = 0; ch < 0x80; ++ch) {
        IDENTIFIER_START[ch] =
            ch >= 0x61 && ch <= 0x7A ||  // a..z
            ch >= 0x41 && ch <= 0x5A ||  // A..Z
            ch === 0x24 || ch === 0x5F;  // $ (dollar) and _ (underscore)
    }

    IDENTIFIER_PART = new Array(0x80);
    for(ch = 0; ch < 0x80; ++ch) {
        IDENTIFIER_PART[ch] =
            ch >= 0x61 && ch <= 0x7A ||  // a..z
            ch >= 0x41 && ch <= 0x5A ||  // A..Z
            ch >= 0x30 && ch <= 0x39 ||  // 0..9
            ch === 0x24 || ch === 0x5F;  // $ (dollar) and _ (underscore)
    }

    function isIdentifierStartES5(ch) {
        return ch < 0x80 ? IDENTIFIER_START[ch] : ES5Regex.NonAsciiIdentifierStart.test(fromCodePoint(ch));
    }

    function isIdentifierPartES5(ch) {
        return ch < 0x80 ? IDENTIFIER_PART[ch] : ES5Regex.NonAsciiIdentifierPart.test(fromCodePoint(ch));
    }

    function isIdentifierStartES6(ch) {
        return ch < 0x80 ? IDENTIFIER_START[ch] : ES6Regex.NonAsciiIdentifierStart.test(fromCodePoint(ch));
    }

    function isIdentifierPartES6(ch) {
        return ch < 0x80 ? IDENTIFIER_PART[ch] : ES6Regex.NonAsciiIdentifierPart.test(fromCodePoint(ch));
    }

    module.exports = {
        isDecimalDigit: isDecimalDigit,
        isHexDigit: isHexDigit,
        isOctalDigit: isOctalDigit,
        isWhiteSpace: isWhiteSpace,
        isLineTerminator: isLineTerminator,
        isIdentifierStartES5: isIdentifierStartES5,
        isIdentifierPartES5: isIdentifierPartES5,
        isIdentifierStartES6: isIdentifierStartES6,
        isIdentifierPartES6: isIdentifierPartES6
    };
}());
/* vim: set sw=4 ts=4 et tw=80 : */


/***/ }),
/* 260 */
/***/ (function(module, exports, __webpack_require__) {

var baseTimes = __webpack_require__(687),
    isArguments = __webpack_require__(113),
    isArray = __webpack_require__(16),
    isBuffer = __webpack_require__(114),
    isIndex = __webpack_require__(95),
    isTypedArray = __webpack_require__(181);

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * Creates an array of the enumerable property names of the array-like `value`.
 *
 * @private
 * @param {*} value The value to query.
 * @param {boolean} inherited Specify returning inherited property names.
 * @returns {Array} Returns the array of property names.
 */
function arrayLikeKeys(value, inherited) {
  var isArr = isArray(value),
      isArg = !isArr && isArguments(value),
      isBuff = !isArr && !isArg && isBuffer(value),
      isType = !isArr && !isArg && !isBuff && isTypedArray(value),
      skipIndexes = isArr || isArg || isBuff || isType,
      result = skipIndexes ? baseTimes(value.length, String) : [],
      length = result.length;

  for (var key in value) {
    if ((inherited || hasOwnProperty.call(value, key)) &&
        !(skipIndexes && (
           // Safari 9 has enumerable `arguments.length` in strict mode.
           key == 'length' ||
           // Node.js 0.10 has enumerable non-index properties on buffers.
           (isBuff && (key == 'offset' || key == 'parent')) ||
           // PhantomJS 2 has enumerable non-index properties on typed arrays.
           (isType && (key == 'buffer' || key == 'byteLength' || key == 'byteOffset')) ||
           // Skip index properties.
           isIndex(key, length)
        ))) {
      result.push(key);
    }
  }
  return result;
}

module.exports = arrayLikeKeys;


/***/ }),
/* 261 */
/***/ (function(module, exports, __webpack_require__) {

var isPrototype = __webpack_require__(116),
    nativeKeys = __webpack_require__(691);

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * The base implementation of `_.keys` which doesn't treat sparse arrays as dense.
 *
 * @private
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of property names.
 */
function baseKeys(object) {
  if (!isPrototype(object)) {
    return nativeKeys(object);
  }
  var result = [];
  for (var key in Object(object)) {
    if (hasOwnProperty.call(object, key) && key != 'constructor') {
      result.push(key);
    }
  }
  return result;
}

module.exports = baseKeys;


/***/ }),
/* 262 */
/***/ (function(module, exports) {

/**
 * Creates a unary function that invokes `func` with its argument transformed.
 *
 * @private
 * @param {Function} func The function to wrap.
 * @param {Function} transform The argument transform.
 * @returns {Function} Returns the new function.
 */
function overArg(func, transform) {
  return function(arg) {
    return func(transform(arg));
  };
}

module.exports = overArg;


/***/ }),
/* 263 */
/***/ (function(module, exports, __webpack_require__) {

var arrayLikeKeys = __webpack_require__(260),
    baseKeysIn = __webpack_require__(693),
    isArrayLike = __webpack_require__(117);

/**
 * Creates an array of the own and inherited enumerable property names of `object`.
 *
 * **Note:** Non-object values are coerced to objects.
 *
 * @static
 * @memberOf _
 * @since 3.0.0
 * @category Object
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of property names.
 * @example
 *
 * function Foo() {
 *   this.a = 1;
 *   this.b = 2;
 * }
 *
 * Foo.prototype.c = 3;
 *
 * _.keysIn(new Foo);
 * // => ['a', 'b', 'c'] (iteration order is not guaranteed)
 */
function keysIn(object) {
  return isArrayLike(object) ? arrayLikeKeys(object, true) : baseKeysIn(object);
}

module.exports = keysIn;


/***/ }),
/* 264 */
/***/ (function(module, exports) {

/**
 * This method returns a new empty array.
 *
 * @static
 * @memberOf _
 * @since 4.13.0
 * @category Util
 * @returns {Array} Returns the new empty array.
 * @example
 *
 * var arrays = _.times(2, _.stubArray);
 *
 * console.log(arrays);
 * // => [[], []]
 *
 * console.log(arrays[0] === arrays[1]);
 * // => false
 */
function stubArray() {
  return [];
}

module.exports = stubArray;


/***/ }),
/* 265 */
/***/ (function(module, exports, __webpack_require__) {

var arrayPush = __webpack_require__(184),
    getPrototype = __webpack_require__(185),
    getSymbols = __webpack_require__(183),
    stubArray = __webpack_require__(264);

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeGetSymbols = Object.getOwnPropertySymbols;

/**
 * Creates an array of the own and inherited enumerable symbols of `object`.
 *
 * @private
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of symbols.
 */
var getSymbolsIn = !nativeGetSymbols ? stubArray : function(object) {
  var result = [];
  while (object) {
    arrayPush(result, getSymbols(object));
    object = getPrototype(object);
  }
  return result;
};

module.exports = getSymbolsIn;


/***/ }),
/* 266 */
/***/ (function(module, exports, __webpack_require__) {

var baseGetAllKeys = __webpack_require__(267),
    getSymbols = __webpack_require__(183),
    keys = __webpack_require__(112);

/**
 * Creates an array of own enumerable property names and symbols of `object`.
 *
 * @private
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of property names and symbols.
 */
function getAllKeys(object) {
  return baseGetAllKeys(object, keys, getSymbols);
}

module.exports = getAllKeys;


/***/ }),
/* 267 */
/***/ (function(module, exports, __webpack_require__) {

var arrayPush = __webpack_require__(184),
    isArray = __webpack_require__(16);

/**
 * The base implementation of `getAllKeys` and `getAllKeysIn` which uses
 * `keysFunc` and `symbolsFunc` to get the enumerable property names and
 * symbols of `object`.
 *
 * @private
 * @param {Object} object The object to query.
 * @param {Function} keysFunc The function to get the keys of `object`.
 * @param {Function} symbolsFunc The function to get the symbols of `object`.
 * @returns {Array} Returns the array of property names and symbols.
 */
function baseGetAllKeys(object, keysFunc, symbolsFunc) {
  var result = keysFunc(object);
  return isArray(object) ? result : arrayPush(result, symbolsFunc(object));
}

module.exports = baseGetAllKeys;


/***/ }),
/* 268 */
/***/ (function(module, exports, __webpack_require__) {

var getNative = __webpack_require__(25),
    root = __webpack_require__(18);

/* Built-in method references that are verified to be native. */
var Set = getNative(root, 'Set');

module.exports = Set;


/***/ }),
/* 269 */
/***/ (function(module, exports, __webpack_require__) {

var root = __webpack_require__(18);

/** Built-in value references. */
var Uint8Array = root.Uint8Array;

module.exports = Uint8Array;


/***/ }),
/* 270 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = validate;

var _definitions = __webpack_require__(35);

function validate(node, key, val) {
  if (!node) return;
  var fields = _definitions.NODE_FIELDS[node.type];
  if (!fields) return;
  var field = fields[key];
  if (!field || !field.validate) return;
  if (field.optional && val == null) return;
  field.validate(node, key, val);
}

/***/ }),
/* 271 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isNode;

var _definitions = __webpack_require__(35);

function isNode(node) {
  return !!(node && _definitions.VISITOR_KEYS[node.type]);
}

/***/ }),
/* 272 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = removeTypeDuplicates;

var _generated = __webpack_require__(17);

function removeTypeDuplicates(nodes) {
  var generics = {};
  var bases = {};
  var typeGroups = [];
  var types = [];

  for (var i = 0; i < nodes.length; i++) {
    var node = nodes[i];
    if (!node) continue;

    if (types.indexOf(node) >= 0) {
      continue;
    }

    if ((0, _generated.isAnyTypeAnnotation)(node)) {
      return [node];
    }

    if ((0, _generated.isFlowBaseAnnotation)(node)) {
      bases[node.type] = node;
      continue;
    }

    if ((0, _generated.isUnionTypeAnnotation)(node)) {
      if (typeGroups.indexOf(node.types) < 0) {
        nodes = nodes.concat(node.types);
        typeGroups.push(node.types);
      }

      continue;
    }

    if ((0, _generated.isGenericTypeAnnotation)(node)) {
      var name = node.id.name;

      if (generics[name]) {
        var existing = generics[name];

        if (existing.typeParameters) {
          if (node.typeParameters) {
            existing.typeParameters.params = removeTypeDuplicates(existing.typeParameters.params.concat(node.typeParameters.params));
          }
        } else {
          existing = node.typeParameters;
        }
      } else {
        generics[name] = node;
      }

      continue;
    }

    types.push(node);
  }

  for (var type in bases) {
    types.push(bases[type]);
  }

  for (var _name in generics) {
    types.push(generics[_name]);
  }

  return types;
}

/***/ }),
/* 273 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = clone;

var _cloneNode = _interopRequireDefault(__webpack_require__(86));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function clone(node) {
  return (0, _cloneNode.default)(node, false);
}

/***/ }),
/* 274 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = addComments;

function addComments(node, type, comments) {
  if (!comments || !node) return node;
  var key = type + "Comments";

  if (node[key]) {
    if (type === "leading") {
      node[key] = comments.concat(node[key]);
    } else {
      node[key] = node[key].concat(comments);
    }
  } else {
    node[key] = comments;
  }

  return node;
}

/***/ }),
/* 275 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = inheritInnerComments;

var _inherit = _interopRequireDefault(__webpack_require__(187));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inheritInnerComments(child, parent) {
  (0, _inherit.default)("innerComments", child, parent);
}

/***/ }),
/* 276 */
/***/ (function(module, exports, __webpack_require__) {

var baseUniq = __webpack_require__(723);

/**
 * Creates a duplicate-free version of an array, using
 * [`SameValueZero`](http://ecma-international.org/ecma-262/7.0/#sec-samevaluezero)
 * for equality comparisons, in which only the first occurrence of each element
 * is kept. The order of result values is determined by the order they occur
 * in the array.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Array
 * @param {Array} array The array to inspect.
 * @returns {Array} Returns the new duplicate free array.
 * @example
 *
 * _.uniq([2, 1, 2]);
 * // => [2, 1]
 */
function uniq(array) {
  return (array && array.length) ? baseUniq(array) : [];
}

module.exports = uniq;


/***/ }),
/* 277 */
/***/ (function(module, exports, __webpack_require__) {

var baseIndexOf = __webpack_require__(726);

/**
 * A specialized version of `_.includes` for arrays without support for
 * specifying an index to search from.
 *
 * @private
 * @param {Array} [array] The array to inspect.
 * @param {*} target The value to search for.
 * @returns {boolean} Returns `true` if `target` is found, else `false`.
 */
function arrayIncludes(array, value) {
  var length = array == null ? 0 : array.length;
  return !!length && baseIndexOf(array, value, 0) > -1;
}

module.exports = arrayIncludes;


/***/ }),
/* 278 */
/***/ (function(module, exports) {

/**
 * This function is like `arrayIncludes` except that it accepts a comparator.
 *
 * @private
 * @param {Array} [array] The array to inspect.
 * @param {*} target The value to search for.
 * @param {Function} comparator The comparator invoked per element.
 * @returns {boolean} Returns `true` if `target` is found, else `false`.
 */
function arrayIncludesWith(array, value, comparator) {
  var index = -1,
      length = array == null ? 0 : array.length;

  while (++index < length) {
    if (comparator(value, array[index])) {
      return true;
    }
  }
  return false;
}

module.exports = arrayIncludesWith;


/***/ }),
/* 279 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = inheritLeadingComments;

var _inherit = _interopRequireDefault(__webpack_require__(187));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inheritLeadingComments(child, parent) {
  (0, _inherit.default)("leadingComments", child, parent);
}

/***/ }),
/* 280 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = inheritsComments;

var _inheritTrailingComments = _interopRequireDefault(__webpack_require__(281));

var _inheritLeadingComments = _interopRequireDefault(__webpack_require__(279));

var _inheritInnerComments = _interopRequireDefault(__webpack_require__(275));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inheritsComments(child, parent) {
  (0, _inheritTrailingComments.default)(child, parent);
  (0, _inheritLeadingComments.default)(child, parent);
  (0, _inheritInnerComments.default)(child, parent);
  return child;
}

/***/ }),
/* 281 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = inheritTrailingComments;

var _inherit = _interopRequireDefault(__webpack_require__(187));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inheritTrailingComments(child, parent) {
  (0, _inherit.default)("trailingComments", child, parent);
}

/***/ }),
/* 282 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = toBlock;

var _generated = __webpack_require__(17);

var _generated2 = __webpack_require__(29);

function toBlock(node, parent) {
  if ((0, _generated.isBlockStatement)(node)) {
    return node;
  }

  var blockNodes = [];

  if ((0, _generated.isEmptyStatement)(node)) {
    blockNodes = [];
  } else {
    if (!(0, _generated.isStatement)(node)) {
      if ((0, _generated.isFunction)(parent)) {
        node = (0, _generated2.returnStatement)(node);
      } else {
        node = (0, _generated2.expressionStatement)(node);
      }
    }

    blockNodes = [node];
  }

  return (0, _generated2.blockStatement)(blockNodes);
}

/***/ }),
/* 283 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = toIdentifier;

var _isValidIdentifier = _interopRequireDefault(__webpack_require__(83));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function toIdentifier(name) {
  name = name + "";
  name = name.replace(/[^a-zA-Z0-9$_]/g, "-");
  name = name.replace(/^[-0-9]+/, "");
  name = name.replace(/[-\s]+(.)?/g, function (match, c) {
    return c ? c.toUpperCase() : "";
  });

  if (!(0, _isValidIdentifier.default)(name)) {
    name = "_" + name;
  }

  return name || "_";
}

/***/ }),
/* 284 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = removePropertiesDeep;

var _traverseFast = _interopRequireDefault(__webpack_require__(285));

var _removeProperties = _interopRequireDefault(__webpack_require__(286));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function removePropertiesDeep(tree, opts) {
  (0, _traverseFast.default)(tree, _removeProperties.default, opts);
  return tree;
}

/***/ }),
/* 285 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = traverseFast;

var _definitions = __webpack_require__(35);

function traverseFast(node, enter, opts) {
  if (!node) return;
  var keys = _definitions.VISITOR_KEYS[node.type];
  if (!keys) return;
  opts = opts || {};
  enter(node, opts);

  for (var _iterator = keys, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
    var _ref;

    if (_isArray) {
      if (_i >= _iterator.length) break;
      _ref = _iterator[_i++];
    } else {
      _i = _iterator.next();
      if (_i.done) break;
      _ref = _i.value;
    }

    var _key = _ref;
    var subNode = node[_key];

    if (Array.isArray(subNode)) {
      for (var _iterator2 = subNode, _isArray2 = Array.isArray(_iterator2), _i2 = 0, _iterator2 = _isArray2 ? _iterator2 : _iterator2[Symbol.iterator]();;) {
        var _ref2;

        if (_isArray2) {
          if (_i2 >= _iterator2.length) break;
          _ref2 = _iterator2[_i2++];
        } else {
          _i2 = _iterator2.next();
          if (_i2.done) break;
          _ref2 = _i2.value;
        }

        var _node2 = _ref2;
        traverseFast(_node2, enter, opts);
      }
    } else {
      traverseFast(subNode, enter, opts);
    }
  }
}

/***/ }),
/* 286 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = removeProperties;

var _constants = __webpack_require__(50);

var CLEAR_KEYS = ["tokens", "start", "end", "loc", "raw", "rawValue"];

var CLEAR_KEYS_PLUS_COMMENTS = _constants.COMMENT_KEYS.concat(["comments"]).concat(CLEAR_KEYS);

function removeProperties(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  var map = opts.preserveComments ? CLEAR_KEYS : CLEAR_KEYS_PLUS_COMMENTS;

  for (var _iterator = map, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
    var _ref;

    if (_isArray) {
      if (_i >= _iterator.length) break;
      _ref = _iterator[_i++];
    } else {
      _i = _iterator.next();
      if (_i.done) break;
      _ref = _i.value;
    }

    var _key2 = _ref;
    if (node[_key2] != null) node[_key2] = undefined;
  }

  for (var _key in node) {
    if (_key[0] === "_" && node[_key] != null) node[_key] = undefined;
  }

  var symbols = Object.getOwnPropertySymbols(node);

  for (var _iterator2 = symbols, _isArray2 = Array.isArray(_iterator2), _i2 = 0, _iterator2 = _isArray2 ? _iterator2 : _iterator2[Symbol.iterator]();;) {
    var _ref2;

    if (_isArray2) {
      if (_i2 >= _iterator2.length) break;
      _ref2 = _iterator2[_i2++];
    } else {
      _i2 = _iterator2.next();
      if (_i2.done) break;
      _ref2 = _i2.value;
    }

    var _sym = _ref2;
    node[_sym] = null;
  }
}

/***/ }),
/* 287 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isLet;

var _generated = __webpack_require__(17);

var _constants = __webpack_require__(50);

function isLet(node) {
  return (0, _generated.isVariableDeclaration)(node) && (node.kind !== "var" || node[_constants.BLOCK_SCOPED_SYMBOL]);
}

/***/ }),
/* 288 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = createSimplePath;
function createSimplePath(ancestors) {
  if (ancestors.length === 0) {
    return null;
  }

  // Slice the array because babel-types traverse may continue mutating
  // the ancestors array in later traversal logic.
  return new SimplePath(ancestors.slice());
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Mimics @babel/traverse's NodePath API in a simpler fashion that isn't as
 * heavy, but still allows the ease of passing paths around to process nested
 * AST structures.
 */
class SimplePath {

  constructor(ancestors, index = ancestors.length - 1) {
    if (index < 0 || index >= ancestors.length) {
      console.error(ancestors);
      throw new Error("Created invalid path");
    }

    this._ancestors = ancestors;
    this._ancestor = ancestors[index];
    this._index = index;
  }

  get parentPath() {
    let path = this._parentPath;
    if (path === undefined) {
      if (this._index === 0) {
        path = null;
      } else {
        path = new SimplePath(this._ancestors, this._index - 1);
      }
      this._parentPath = path;
    }

    return path;
  }

  get parent() {
    return this._ancestor.node;
  }

  get node() {
    const { node, key, index } = this._ancestor;

    if (typeof index === "number") {
      return node[key][index];
    }

    return node[key];
  }

  set node(replacement) {
    if (this.type !== "Identifier") {
      throw new Error("Replacing anything other than leaf nodes is undefined behavior " + "in t.traverse()");
    }

    const { node, key, index } = this._ancestor;
    if (typeof index === "number") {
      node[key][index] = replacement;
    } else {
      node[key] = replacement;
    }
  }

  get type() {
    return this.node.type;
  }

  get inList() {
    return typeof this._ancestor.index === "number";
  }

  get containerIndex() {
    const { index } = this._ancestor;

    if (typeof index !== "number") {
      throw new Error("Cannot get index of non-array node");
    }

    return index;
  }

  get depth() {
    return this._index;
  }

  replace(node) {
    this.node = node;
  }

  find(predicate) {
    for (let path = this; path; path = path.parentPath) {
      if (predicate(path)) {
        return path;
      }
    }
    return null;
  }
  findParent(predicate) {
    if (!this.parentPath) {
      throw new Error("Cannot use findParent on root path");
    }

    return this.parentPath.find(predicate);
  }

  getSibling(offset) {
    const { node, key, index } = this._ancestor;

    if (typeof index !== "number") {
      throw new Error("Non-array nodes do not have siblings");
    }

    const container = node[key];

    const siblingIndex = index + offset;
    if (siblingIndex < 0 || siblingIndex >= container.length) {
      return null;
    }

    return new SimplePath(this._ancestors.slice(0, -1).concat([{ node, key, index: siblingIndex }]));
  }
}

/***/ }),
/* 289 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, '__esModule', { value: true });

function _inheritsLoose(subClass, superClass) {
  subClass.prototype = Object.create(superClass.prototype);
  subClass.prototype.constructor = subClass;
  subClass.__proto__ = superClass;
}

// A second optional argument can be given to further configure
// the parser process. These options are recognized:
var defaultOptions = {
  // Source type ("script" or "module") for different semantics
  sourceType: "script",
  // Source filename.
  sourceFilename: undefined,
  // Line from which to start counting source. Useful for
  // integration with other tools.
  startLine: 1,
  // When enabled, a return at the top level is not considered an
  // error.
  allowReturnOutsideFunction: false,
  // When enabled, import/export statements are not constrained to
  // appearing at the top of the program.
  allowImportExportEverywhere: false,
  // TODO
  allowSuperOutsideMethod: false,
  // An array of plugins to enable
  plugins: [],
  // TODO
  strictMode: null,
  // Nodes have their start and end characters offsets recorded in
  // `start` and `end` properties (directly on the node, rather than
  // the `loc` object, which holds line/column data. To also add a
  // [semi-standardized][range] `range` property holding a `[start,
  // end]` array with the same numbers, set the `ranges` option to
  // `true`.
  //
  // [range]: https://bugzilla.mozilla.org/show_bug.cgi?id=745678
  ranges: false,
  // Adds all parsed tokens to a `tokens` property on the `File` node
  tokens: false
}; // Interpret and default an options object

function getOptions(opts) {
  var options = {};

  for (var key in defaultOptions) {
    options[key] = opts && opts[key] != null ? opts[key] : defaultOptions[key];
  }

  return options;
}

// ## Token types
// The assignment of fine-grained, information-carrying type objects
// allows the tokenizer to store the information it has about a
// token in a way that is very cheap for the parser to look up.
// All token type variables start with an underscore, to make them
// easy to recognize.
// The `beforeExpr` property is used to disambiguate between regular
// expressions and divisions. It is set on all token types that can
// be followed by an expression (thus, a slash after them would be a
// regular expression).
//
// `isLoop` marks a keyword as starting a loop, which is important
// to know when parsing a label, in order to allow or disallow
// continue jumps to that label.
var beforeExpr = true;
var startsExpr = true;
var isLoop = true;
var isAssign = true;
var prefix = true;
var postfix = true;
var TokenType = function TokenType(label, conf) {
  if (conf === void 0) {
    conf = {};
  }

  this.label = label;
  this.keyword = conf.keyword;
  this.beforeExpr = !!conf.beforeExpr;
  this.startsExpr = !!conf.startsExpr;
  this.rightAssociative = !!conf.rightAssociative;
  this.isLoop = !!conf.isLoop;
  this.isAssign = !!conf.isAssign;
  this.prefix = !!conf.prefix;
  this.postfix = !!conf.postfix;
  this.binop = conf.binop === 0 ? 0 : conf.binop || null;
  this.updateContext = null;
};

var KeywordTokenType =
/*#__PURE__*/
function (_TokenType) {
  _inheritsLoose(KeywordTokenType, _TokenType);

  function KeywordTokenType(name, options) {
    if (options === void 0) {
      options = {};
    }

    options.keyword = name;
    return _TokenType.call(this, name, options) || this;
  }

  return KeywordTokenType;
}(TokenType);

var BinopTokenType =
/*#__PURE__*/
function (_TokenType2) {
  _inheritsLoose(BinopTokenType, _TokenType2);

  function BinopTokenType(name, prec) {
    return _TokenType2.call(this, name, {
      beforeExpr,
      binop: prec
    }) || this;
  }

  return BinopTokenType;
}(TokenType);
var types = {
  num: new TokenType("num", {
    startsExpr
  }),
  bigint: new TokenType("bigint", {
    startsExpr
  }),
  regexp: new TokenType("regexp", {
    startsExpr
  }),
  string: new TokenType("string", {
    startsExpr
  }),
  name: new TokenType("name", {
    startsExpr
  }),
  eof: new TokenType("eof"),
  // Punctuation token types.
  bracketL: new TokenType("[", {
    beforeExpr,
    startsExpr
  }),
  bracketR: new TokenType("]"),
  braceL: new TokenType("{", {
    beforeExpr,
    startsExpr
  }),
  braceBarL: new TokenType("{|", {
    beforeExpr,
    startsExpr
  }),
  braceR: new TokenType("}"),
  braceBarR: new TokenType("|}"),
  parenL: new TokenType("(", {
    beforeExpr,
    startsExpr
  }),
  parenR: new TokenType(")"),
  comma: new TokenType(",", {
    beforeExpr
  }),
  semi: new TokenType(";", {
    beforeExpr
  }),
  colon: new TokenType(":", {
    beforeExpr
  }),
  doubleColon: new TokenType("::", {
    beforeExpr
  }),
  dot: new TokenType("."),
  question: new TokenType("?", {
    beforeExpr
  }),
  questionDot: new TokenType("?."),
  arrow: new TokenType("=>", {
    beforeExpr
  }),
  template: new TokenType("template"),
  ellipsis: new TokenType("...", {
    beforeExpr
  }),
  backQuote: new TokenType("`", {
    startsExpr
  }),
  dollarBraceL: new TokenType("${", {
    beforeExpr,
    startsExpr
  }),
  at: new TokenType("@"),
  hash: new TokenType("#"),
  // Operators. These carry several kinds of properties to help the
  // parser use them properly (the presence of these properties is
  // what categorizes them as operators).
  //
  // `binop`, when present, specifies that this operator is a binary
  // operator, and will refer to its precedence.
  //
  // `prefix` and `postfix` mark the operator as a prefix or postfix
  // unary operator.
  //
  // `isAssign` marks all of `=`, `+=`, `-=` etcetera, which act as
  // binary operators with a very low precedence, that should result
  // in AssignmentExpression nodes.
  eq: new TokenType("=", {
    beforeExpr,
    isAssign
  }),
  assign: new TokenType("_=", {
    beforeExpr,
    isAssign
  }),
  incDec: new TokenType("++/--", {
    prefix,
    postfix,
    startsExpr
  }),
  bang: new TokenType("!", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  tilde: new TokenType("~", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  pipeline: new BinopTokenType("|>", 0),
  nullishCoalescing: new BinopTokenType("??", 1),
  logicalOR: new BinopTokenType("||", 1),
  logicalAND: new BinopTokenType("&&", 2),
  bitwiseOR: new BinopTokenType("|", 3),
  bitwiseXOR: new BinopTokenType("^", 4),
  bitwiseAND: new BinopTokenType("&", 5),
  equality: new BinopTokenType("==/!=", 6),
  relational: new BinopTokenType("</>", 7),
  bitShift: new BinopTokenType("<</>>", 8),
  plusMin: new TokenType("+/-", {
    beforeExpr,
    binop: 9,
    prefix,
    startsExpr
  }),
  modulo: new BinopTokenType("%", 10),
  star: new BinopTokenType("*", 10),
  slash: new BinopTokenType("/", 10),
  exponent: new TokenType("**", {
    beforeExpr,
    binop: 11,
    rightAssociative: true
  })
};
var keywords = {
  break: new KeywordTokenType("break"),
  case: new KeywordTokenType("case", {
    beforeExpr
  }),
  catch: new KeywordTokenType("catch"),
  continue: new KeywordTokenType("continue"),
  debugger: new KeywordTokenType("debugger"),
  default: new KeywordTokenType("default", {
    beforeExpr
  }),
  do: new KeywordTokenType("do", {
    isLoop,
    beforeExpr
  }),
  else: new KeywordTokenType("else", {
    beforeExpr
  }),
  finally: new KeywordTokenType("finally"),
  for: new KeywordTokenType("for", {
    isLoop
  }),
  function: new KeywordTokenType("function", {
    startsExpr
  }),
  if: new KeywordTokenType("if"),
  return: new KeywordTokenType("return", {
    beforeExpr
  }),
  switch: new KeywordTokenType("switch"),
  throw: new KeywordTokenType("throw", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  try: new KeywordTokenType("try"),
  var: new KeywordTokenType("var"),
  let: new KeywordTokenType("let"),
  const: new KeywordTokenType("const"),
  while: new KeywordTokenType("while", {
    isLoop
  }),
  with: new KeywordTokenType("with"),
  new: new KeywordTokenType("new", {
    beforeExpr,
    startsExpr
  }),
  this: new KeywordTokenType("this", {
    startsExpr
  }),
  super: new KeywordTokenType("super", {
    startsExpr
  }),
  class: new KeywordTokenType("class"),
  extends: new KeywordTokenType("extends", {
    beforeExpr
  }),
  export: new KeywordTokenType("export"),
  import: new KeywordTokenType("import", {
    startsExpr
  }),
  yield: new KeywordTokenType("yield", {
    beforeExpr,
    startsExpr
  }),
  null: new KeywordTokenType("null", {
    startsExpr
  }),
  true: new KeywordTokenType("true", {
    startsExpr
  }),
  false: new KeywordTokenType("false", {
    startsExpr
  }),
  in: new KeywordTokenType("in", {
    beforeExpr,
    binop: 7
  }),
  instanceof: new KeywordTokenType("instanceof", {
    beforeExpr,
    binop: 7
  }),
  typeof: new KeywordTokenType("typeof", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  void: new KeywordTokenType("void", {
    beforeExpr,
    prefix,
    startsExpr
  }),
  delete: new KeywordTokenType("delete", {
    beforeExpr,
    prefix,
    startsExpr
  })
}; // Map keyword names to token types.

Object.keys(keywords).forEach(function (name) {
  types["_" + name] = keywords[name];
});

/* eslint max-len: 0 */
function makePredicate(words) {
  var wordsArr = words.split(" ");
  return function (str) {
    return wordsArr.indexOf(str) >= 0;
  };
} // Reserved word lists for various dialects of the language


var reservedWords = {
  "6": makePredicate("enum await"),
  strict: makePredicate("implements interface let package private protected public static yield"),
  strictBind: makePredicate("eval arguments")
}; // And the keywords

var isKeyword = makePredicate("break case catch continue debugger default do else finally for function if return switch throw try var while with null true false instanceof typeof void delete new in this let const class extends export import yield super"); // ## Character categories
// Big ugly regular expressions that match characters in the
// whitespace, identifier, and identifier-start categories. These
// are only applied when a character is found to actually have a
// code point above 128.
// Generated by `bin/generate-identifier-regex.js`.

/* prettier-ignore */

var nonASCIIidentifierStartChars = "\xaa\xb5\xba\xc0-\xd6\xd8-\xf6\xf8-\u02c1\u02c6-\u02d1\u02e0-\u02e4\u02ec\u02ee\u0370-\u0374\u0376\u0377\u037a-\u037d\u037f\u0386\u0388-\u038a\u038c\u038e-\u03a1\u03a3-\u03f5\u03f7-\u0481\u048a-\u052f\u0531-\u0556\u0559\u0561-\u0587\u05d0-\u05ea\u05f0-\u05f2\u0620-\u064a\u066e\u066f\u0671-\u06d3\u06d5\u06e5\u06e6\u06ee\u06ef\u06fa-\u06fc\u06ff\u0710\u0712-\u072f\u074d-\u07a5\u07b1\u07ca-\u07ea\u07f4\u07f5\u07fa\u0800-\u0815\u081a\u0824\u0828\u0840-\u0858\u0860-\u086a\u08a0-\u08b4\u08b6-\u08bd\u0904-\u0939\u093d\u0950\u0958-\u0961\u0971-\u0980\u0985-\u098c\u098f\u0990\u0993-\u09a8\u09aa-\u09b0\u09b2\u09b6-\u09b9\u09bd\u09ce\u09dc\u09dd\u09df-\u09e1\u09f0\u09f1\u09fc\u0a05-\u0a0a\u0a0f\u0a10\u0a13-\u0a28\u0a2a-\u0a30\u0a32\u0a33\u0a35\u0a36\u0a38\u0a39\u0a59-\u0a5c\u0a5e\u0a72-\u0a74\u0a85-\u0a8d\u0a8f-\u0a91\u0a93-\u0aa8\u0aaa-\u0ab0\u0ab2\u0ab3\u0ab5-\u0ab9\u0abd\u0ad0\u0ae0\u0ae1\u0af9\u0b05-\u0b0c\u0b0f\u0b10\u0b13-\u0b28\u0b2a-\u0b30\u0b32\u0b33\u0b35-\u0b39\u0b3d\u0b5c\u0b5d\u0b5f-\u0b61\u0b71\u0b83\u0b85-\u0b8a\u0b8e-\u0b90\u0b92-\u0b95\u0b99\u0b9a\u0b9c\u0b9e\u0b9f\u0ba3\u0ba4\u0ba8-\u0baa\u0bae-\u0bb9\u0bd0\u0c05-\u0c0c\u0c0e-\u0c10\u0c12-\u0c28\u0c2a-\u0c39\u0c3d\u0c58-\u0c5a\u0c60\u0c61\u0c80\u0c85-\u0c8c\u0c8e-\u0c90\u0c92-\u0ca8\u0caa-\u0cb3\u0cb5-\u0cb9\u0cbd\u0cde\u0ce0\u0ce1\u0cf1\u0cf2\u0d05-\u0d0c\u0d0e-\u0d10\u0d12-\u0d3a\u0d3d\u0d4e\u0d54-\u0d56\u0d5f-\u0d61\u0d7a-\u0d7f\u0d85-\u0d96\u0d9a-\u0db1\u0db3-\u0dbb\u0dbd\u0dc0-\u0dc6\u0e01-\u0e30\u0e32\u0e33\u0e40-\u0e46\u0e81\u0e82\u0e84\u0e87\u0e88\u0e8a\u0e8d\u0e94-\u0e97\u0e99-\u0e9f\u0ea1-\u0ea3\u0ea5\u0ea7\u0eaa\u0eab\u0ead-\u0eb0\u0eb2\u0eb3\u0ebd\u0ec0-\u0ec4\u0ec6\u0edc-\u0edf\u0f00\u0f40-\u0f47\u0f49-\u0f6c\u0f88-\u0f8c\u1000-\u102a\u103f\u1050-\u1055\u105a-\u105d\u1061\u1065\u1066\u106e-\u1070\u1075-\u1081\u108e\u10a0-\u10c5\u10c7\u10cd\u10d0-\u10fa\u10fc-\u1248\u124a-\u124d\u1250-\u1256\u1258\u125a-\u125d\u1260-\u1288\u128a-\u128d\u1290-\u12b0\u12b2-\u12b5\u12b8-\u12be\u12c0\u12c2-\u12c5\u12c8-\u12d6\u12d8-\u1310\u1312-\u1315\u1318-\u135a\u1380-\u138f\u13a0-\u13f5\u13f8-\u13fd\u1401-\u166c\u166f-\u167f\u1681-\u169a\u16a0-\u16ea\u16ee-\u16f8\u1700-\u170c\u170e-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176c\u176e-\u1770\u1780-\u17b3\u17d7\u17dc\u1820-\u1877\u1880-\u18a8\u18aa\u18b0-\u18f5\u1900-\u191e\u1950-\u196d\u1970-\u1974\u1980-\u19ab\u19b0-\u19c9\u1a00-\u1a16\u1a20-\u1a54\u1aa7\u1b05-\u1b33\u1b45-\u1b4b\u1b83-\u1ba0\u1bae\u1baf\u1bba-\u1be5\u1c00-\u1c23\u1c4d-\u1c4f\u1c5a-\u1c7d\u1c80-\u1c88\u1ce9-\u1cec\u1cee-\u1cf1\u1cf5\u1cf6\u1d00-\u1dbf\u1e00-\u1f15\u1f18-\u1f1d\u1f20-\u1f45\u1f48-\u1f4d\u1f50-\u1f57\u1f59\u1f5b\u1f5d\u1f5f-\u1f7d\u1f80-\u1fb4\u1fb6-\u1fbc\u1fbe\u1fc2-\u1fc4\u1fc6-\u1fcc\u1fd0-\u1fd3\u1fd6-\u1fdb\u1fe0-\u1fec\u1ff2-\u1ff4\u1ff6-\u1ffc\u2071\u207f\u2090-\u209c\u2102\u2107\u210a-\u2113\u2115\u2118-\u211d\u2124\u2126\u2128\u212a-\u2139\u213c-\u213f\u2145-\u2149\u214e\u2160-\u2188\u2c00-\u2c2e\u2c30-\u2c5e\u2c60-\u2ce4\u2ceb-\u2cee\u2cf2\u2cf3\u2d00-\u2d25\u2d27\u2d2d\u2d30-\u2d67\u2d6f\u2d80-\u2d96\u2da0-\u2da6\u2da8-\u2dae\u2db0-\u2db6\u2db8-\u2dbe\u2dc0-\u2dc6\u2dc8-\u2dce\u2dd0-\u2dd6\u2dd8-\u2dde\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303c\u3041-\u3096\u309b-\u309f\u30a1-\u30fa\u30fc-\u30ff\u3105-\u312e\u3131-\u318e\u31a0-\u31ba\u31f0-\u31ff\u3400-\u4db5\u4e00-\u9fea\ua000-\ua48c\ua4d0-\ua4fd\ua500-\ua60c\ua610-\ua61f\ua62a\ua62b\ua640-\ua66e\ua67f-\ua69d\ua6a0-\ua6ef\ua717-\ua71f\ua722-\ua788\ua78b-\ua7ae\ua7b0-\ua7b7\ua7f7-\ua801\ua803-\ua805\ua807-\ua80a\ua80c-\ua822\ua840-\ua873\ua882-\ua8b3\ua8f2-\ua8f7\ua8fb\ua8fd\ua90a-\ua925\ua930-\ua946\ua960-\ua97c\ua984-\ua9b2\ua9cf\ua9e0-\ua9e4\ua9e6-\ua9ef\ua9fa-\ua9fe\uaa00-\uaa28\uaa40-\uaa42\uaa44-\uaa4b\uaa60-\uaa76\uaa7a\uaa7e-\uaaaf\uaab1\uaab5\uaab6\uaab9-\uaabd\uaac0\uaac2\uaadb-\uaadd\uaae0-\uaaea\uaaf2-\uaaf4\uab01-\uab06\uab09-\uab0e\uab11-\uab16\uab20-\uab26\uab28-\uab2e\uab30-\uab5a\uab5c-\uab65\uab70-\uabe2\uac00-\ud7a3\ud7b0-\ud7c6\ud7cb-\ud7fb\uf900-\ufa6d\ufa70-\ufad9\ufb00-\ufb06\ufb13-\ufb17\ufb1d\ufb1f-\ufb28\ufb2a-\ufb36\ufb38-\ufb3c\ufb3e\ufb40\ufb41\ufb43\ufb44\ufb46-\ufbb1\ufbd3-\ufd3d\ufd50-\ufd8f\ufd92-\ufdc7\ufdf0-\ufdfb\ufe70-\ufe74\ufe76-\ufefc\uff21-\uff3a\uff41-\uff5a\uff66-\uffbe\uffc2-\uffc7\uffca-\uffcf\uffd2-\uffd7\uffda-\uffdc";
/* prettier-ignore */

var nonASCIIidentifierChars = "\u200c\u200d\xb7\u0300-\u036f\u0387\u0483-\u0487\u0591-\u05bd\u05bf\u05c1\u05c2\u05c4\u05c5\u05c7\u0610-\u061a\u064b-\u0669\u0670\u06d6-\u06dc\u06df-\u06e4\u06e7\u06e8\u06ea-\u06ed\u06f0-\u06f9\u0711\u0730-\u074a\u07a6-\u07b0\u07c0-\u07c9\u07eb-\u07f3\u0816-\u0819\u081b-\u0823\u0825-\u0827\u0829-\u082d\u0859-\u085b\u08d4-\u08e1\u08e3-\u0903\u093a-\u093c\u093e-\u094f\u0951-\u0957\u0962\u0963\u0966-\u096f\u0981-\u0983\u09bc\u09be-\u09c4\u09c7\u09c8\u09cb-\u09cd\u09d7\u09e2\u09e3\u09e6-\u09ef\u0a01-\u0a03\u0a3c\u0a3e-\u0a42\u0a47\u0a48\u0a4b-\u0a4d\u0a51\u0a66-\u0a71\u0a75\u0a81-\u0a83\u0abc\u0abe-\u0ac5\u0ac7-\u0ac9\u0acb-\u0acd\u0ae2\u0ae3\u0ae6-\u0aef\u0afa-\u0aff\u0b01-\u0b03\u0b3c\u0b3e-\u0b44\u0b47\u0b48\u0b4b-\u0b4d\u0b56\u0b57\u0b62\u0b63\u0b66-\u0b6f\u0b82\u0bbe-\u0bc2\u0bc6-\u0bc8\u0bca-\u0bcd\u0bd7\u0be6-\u0bef\u0c00-\u0c03\u0c3e-\u0c44\u0c46-\u0c48\u0c4a-\u0c4d\u0c55\u0c56\u0c62\u0c63\u0c66-\u0c6f\u0c81-\u0c83\u0cbc\u0cbe-\u0cc4\u0cc6-\u0cc8\u0cca-\u0ccd\u0cd5\u0cd6\u0ce2\u0ce3\u0ce6-\u0cef\u0d00-\u0d03\u0d3b\u0d3c\u0d3e-\u0d44\u0d46-\u0d48\u0d4a-\u0d4d\u0d57\u0d62\u0d63\u0d66-\u0d6f\u0d82\u0d83\u0dca\u0dcf-\u0dd4\u0dd6\u0dd8-\u0ddf\u0de6-\u0def\u0df2\u0df3\u0e31\u0e34-\u0e3a\u0e47-\u0e4e\u0e50-\u0e59\u0eb1\u0eb4-\u0eb9\u0ebb\u0ebc\u0ec8-\u0ecd\u0ed0-\u0ed9\u0f18\u0f19\u0f20-\u0f29\u0f35\u0f37\u0f39\u0f3e\u0f3f\u0f71-\u0f84\u0f86\u0f87\u0f8d-\u0f97\u0f99-\u0fbc\u0fc6\u102b-\u103e\u1040-\u1049\u1056-\u1059\u105e-\u1060\u1062-\u1064\u1067-\u106d\u1071-\u1074\u1082-\u108d\u108f-\u109d\u135d-\u135f\u1369-\u1371\u1712-\u1714\u1732-\u1734\u1752\u1753\u1772\u1773\u17b4-\u17d3\u17dd\u17e0-\u17e9\u180b-\u180d\u1810-\u1819\u18a9\u1920-\u192b\u1930-\u193b\u1946-\u194f\u19d0-\u19da\u1a17-\u1a1b\u1a55-\u1a5e\u1a60-\u1a7c\u1a7f-\u1a89\u1a90-\u1a99\u1ab0-\u1abd\u1b00-\u1b04\u1b34-\u1b44\u1b50-\u1b59\u1b6b-\u1b73\u1b80-\u1b82\u1ba1-\u1bad\u1bb0-\u1bb9\u1be6-\u1bf3\u1c24-\u1c37\u1c40-\u1c49\u1c50-\u1c59\u1cd0-\u1cd2\u1cd4-\u1ce8\u1ced\u1cf2-\u1cf4\u1cf7-\u1cf9\u1dc0-\u1df9\u1dfb-\u1dff\u203f\u2040\u2054\u20d0-\u20dc\u20e1\u20e5-\u20f0\u2cef-\u2cf1\u2d7f\u2de0-\u2dff\u302a-\u302f\u3099\u309a\ua620-\ua629\ua66f\ua674-\ua67d\ua69e\ua69f\ua6f0\ua6f1\ua802\ua806\ua80b\ua823-\ua827\ua880\ua881\ua8b4-\ua8c5\ua8d0-\ua8d9\ua8e0-\ua8f1\ua900-\ua909\ua926-\ua92d\ua947-\ua953\ua980-\ua983\ua9b3-\ua9c0\ua9d0-\ua9d9\ua9e5\ua9f0-\ua9f9\uaa29-\uaa36\uaa43\uaa4c\uaa4d\uaa50-\uaa59\uaa7b-\uaa7d\uaab0\uaab2-\uaab4\uaab7\uaab8\uaabe\uaabf\uaac1\uaaeb-\uaaef\uaaf5\uaaf6\uabe3-\uabea\uabec\uabed\uabf0-\uabf9\ufb1e\ufe00-\ufe0f\ufe20-\ufe2f\ufe33\ufe34\ufe4d-\ufe4f\uff10-\uff19\uff3f";
var nonASCIIidentifierStart = new RegExp("[" + nonASCIIidentifierStartChars + "]");
var nonASCIIidentifier = new RegExp("[" + nonASCIIidentifierStartChars + nonASCIIidentifierChars + "]");
nonASCIIidentifierStartChars = nonASCIIidentifierChars = null; // These are a run-length and offset encoded representation of the
// >0xffff code points that are a valid part of identifiers. The
// offset starts at 0x10000, and each pair of numbers represents an
// offset to the next range, and then a size of the range. They were
// generated by `bin/generate-identifier-regex.js`.

/* prettier-ignore */

var astralIdentifierStartCodes = [0, 11, 2, 25, 2, 18, 2, 1, 2, 14, 3, 13, 35, 122, 70, 52, 268, 28, 4, 48, 48, 31, 14, 29, 6, 37, 11, 29, 3, 35, 5, 7, 2, 4, 43, 157, 19, 35, 5, 35, 5, 39, 9, 51, 157, 310, 10, 21, 11, 7, 153, 5, 3, 0, 2, 43, 2, 1, 4, 0, 3, 22, 11, 22, 10, 30, 66, 18, 2, 1, 11, 21, 11, 25, 71, 55, 7, 1, 65, 0, 16, 3, 2, 2, 2, 26, 45, 28, 4, 28, 36, 7, 2, 27, 28, 53, 11, 21, 11, 18, 14, 17, 111, 72, 56, 50, 14, 50, 785, 52, 76, 44, 33, 24, 27, 35, 42, 34, 4, 0, 13, 47, 15, 3, 22, 0, 2, 0, 36, 17, 2, 24, 85, 6, 2, 0, 2, 3, 2, 14, 2, 9, 8, 46, 39, 7, 3, 1, 3, 21, 2, 6, 2, 1, 2, 4, 4, 0, 19, 0, 13, 4, 159, 52, 19, 3, 54, 47, 21, 1, 2, 0, 185, 46, 42, 3, 37, 47, 21, 0, 60, 42, 86, 25, 391, 63, 32, 0, 257, 0, 11, 39, 8, 0, 22, 0, 12, 39, 3, 3, 55, 56, 264, 8, 2, 36, 18, 0, 50, 29, 113, 6, 2, 1, 2, 37, 22, 0, 698, 921, 103, 110, 18, 195, 2749, 1070, 4050, 582, 8634, 568, 8, 30, 114, 29, 19, 47, 17, 3, 32, 20, 6, 18, 881, 68, 12, 0, 67, 12, 65, 1, 31, 6124, 20, 754, 9486, 286, 82, 395, 2309, 106, 6, 12, 4, 8, 8, 9, 5991, 84, 2, 70, 2, 1, 3, 0, 3, 1, 3, 3, 2, 11, 2, 0, 2, 6, 2, 64, 2, 3, 3, 7, 2, 6, 2, 27, 2, 3, 2, 4, 2, 0, 4, 6, 2, 339, 3, 24, 2, 24, 2, 30, 2, 24, 2, 30, 2, 24, 2, 30, 2, 24, 2, 30, 2, 24, 2, 7, 4149, 196, 60, 67, 1213, 3, 2, 26, 2, 1, 2, 0, 3, 0, 2, 9, 2, 3, 2, 0, 2, 0, 7, 0, 5, 0, 2, 0, 2, 0, 2, 2, 2, 1, 2, 0, 3, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 1, 2, 0, 3, 3, 2, 6, 2, 3, 2, 3, 2, 0, 2, 9, 2, 16, 6, 2, 2, 4, 2, 16, 4421, 42710, 42, 4148, 12, 221, 3, 5761, 15, 7472, 3104, 541];
/* prettier-ignore */

var astralIdentifierCodes = [509, 0, 227, 0, 150, 4, 294, 9, 1368, 2, 2, 1, 6, 3, 41, 2, 5, 0, 166, 1, 1306, 2, 54, 14, 32, 9, 16, 3, 46, 10, 54, 9, 7, 2, 37, 13, 2, 9, 52, 0, 13, 2, 49, 13, 10, 2, 4, 9, 83, 11, 7, 0, 161, 11, 6, 9, 7, 3, 57, 0, 2, 6, 3, 1, 3, 2, 10, 0, 11, 1, 3, 6, 4, 4, 193, 17, 10, 9, 87, 19, 13, 9, 214, 6, 3, 8, 28, 1, 83, 16, 16, 9, 82, 12, 9, 9, 84, 14, 5, 9, 423, 9, 280, 9, 41, 6, 2, 3, 9, 0, 10, 10, 47, 15, 406, 7, 2, 7, 17, 9, 57, 21, 2, 13, 123, 5, 4, 0, 2, 1, 2, 6, 2, 0, 9, 9, 19719, 9, 135, 4, 60, 6, 26, 9, 1016, 45, 17, 3, 19723, 1, 5319, 4, 4, 5, 9, 7, 3, 6, 31, 3, 149, 2, 1418, 49, 513, 54, 5, 49, 9, 0, 15, 0, 23, 4, 2, 14, 1361, 6, 2, 16, 3, 6, 2, 1, 2, 4, 2214, 6, 110, 6, 6, 9, 792487, 239]; // This has a complexity linear to the value of the code. The
// assumption is that looking up astral identifier characters is
// rare.

function isInAstralSet(code, set) {
  var pos = 0x10000;

  for (var i = 0; i < set.length; i += 2) {
    pos += set[i];
    if (pos > code) return false;
    pos += set[i + 1];
    if (pos >= code) return true;
  }

  return false;
} // Test whether a given character code starts an identifier.


function isIdentifierStart(code) {
  if (code < 65) return code === 36;
  if (code < 91) return true;
  if (code < 97) return code === 95;
  if (code < 123) return true;

  if (code <= 0xffff) {
    return code >= 0xaa && nonASCIIidentifierStart.test(String.fromCharCode(code));
  }

  return isInAstralSet(code, astralIdentifierStartCodes);
} // Test whether a current state character code and next character code  is @

function isIteratorStart(current, next) {
  return current === 64 && next === 64;
} // Test whether a given character is part of an identifier.

function isIdentifierChar(code) {
  if (code < 48) return code === 36;
  if (code < 58) return true;
  if (code < 65) return false;
  if (code < 91) return true;
  if (code < 97) return code === 95;
  if (code < 123) return true;

  if (code <= 0xffff) {
    return code >= 0xaa && nonASCIIidentifier.test(String.fromCharCode(code));
  }

  return isInAstralSet(code, astralIdentifierStartCodes) || isInAstralSet(code, astralIdentifierCodes);
}

// Matches a whole line break (where CRLF is considered a single
// line break). Used to count lines.
var lineBreak = /\r\n?|\n|\u2028|\u2029/;
var lineBreakG = new RegExp(lineBreak.source, "g");
function isNewLine(code) {
  return code === 10 || code === 13 || code === 0x2028 || code === 0x2029;
}
var nonASCIIwhitespace = /[\u1680\u180e\u2000-\u200a\u202f\u205f\u3000\ufeff]/;

// The algorithm used to determine whether a regexp can appear at a
// given point in the program is loosely based on sweet.js' approach.
// See https://github.com/mozilla/sweet.js/wiki/design
var TokContext = function TokContext(token, isExpr, preserveSpace, override) // Takes a Tokenizer as a this-parameter, and returns void.
{
  this.token = token;
  this.isExpr = !!isExpr;
  this.preserveSpace = !!preserveSpace;
  this.override = override;
};
var types$1 = {
  braceStatement: new TokContext("{", false),
  braceExpression: new TokContext("{", true),
  templateQuasi: new TokContext("${", true),
  parenStatement: new TokContext("(", false),
  parenExpression: new TokContext("(", true),
  template: new TokContext("`", true, true, function (p) {
    return p.readTmplToken();
  }),
  functionExpression: new TokContext("function", true)
}; // Token-specific context update code

types.parenR.updateContext = types.braceR.updateContext = function () {
  if (this.state.context.length === 1) {
    this.state.exprAllowed = true;
    return;
  }

  var out = this.state.context.pop();

  if (out === types$1.braceStatement && this.curContext() === types$1.functionExpression) {
    this.state.context.pop();
    this.state.exprAllowed = false;
  } else if (out === types$1.templateQuasi) {
    this.state.exprAllowed = true;
  } else {
    this.state.exprAllowed = !out.isExpr;
  }
};

types.name.updateContext = function (prevType) {
  if (this.state.value === "of" && this.curContext() === types$1.parenStatement) {
    this.state.exprAllowed = !prevType.beforeExpr;
    return;
  }

  this.state.exprAllowed = false;

  if (prevType === types._let || prevType === types._const || prevType === types._var) {
    if (lineBreak.test(this.input.slice(this.state.end))) {
      this.state.exprAllowed = true;
    }
  }

  if (this.state.isIterator) {
    this.state.isIterator = false;
  }
};

types.braceL.updateContext = function (prevType) {
  this.state.context.push(this.braceIsBlock(prevType) ? types$1.braceStatement : types$1.braceExpression);
  this.state.exprAllowed = true;
};

types.dollarBraceL.updateContext = function () {
  this.state.context.push(types$1.templateQuasi);
  this.state.exprAllowed = true;
};

types.parenL.updateContext = function (prevType) {
  var statementParens = prevType === types._if || prevType === types._for || prevType === types._with || prevType === types._while;
  this.state.context.push(statementParens ? types$1.parenStatement : types$1.parenExpression);
  this.state.exprAllowed = true;
};

types.incDec.updateContext = function () {// tokExprAllowed stays unchanged
};

types._function.updateContext = function (prevType) {
  if (this.state.exprAllowed && !this.braceIsBlock(prevType)) {
    this.state.context.push(types$1.functionExpression);
  }

  this.state.exprAllowed = false;
};

types.backQuote.updateContext = function () {
  if (this.curContext() === types$1.template) {
    this.state.context.pop();
  } else {
    this.state.context.push(types$1.template);
  }

  this.state.exprAllowed = false;
};

// These are used when `options.locations` is on, for the
// `startLoc` and `endLoc` properties.
var Position = function Position(line, col) {
  this.line = line;
  this.column = col;
};
var SourceLocation = function SourceLocation(start, end) {
  this.start = start; // $FlowIgnore (may start as null, but initialized later)

  this.end = end;
}; // The `getLineInfo` function is mostly useful when the
// `locations` option is off (for performance reasons) and you
// want to find the line/column position for a given character
// offset. `input` should be the code string that the offset refers
// into.

function getLineInfo(input, offset) {
  for (var line = 1, cur = 0;;) {
    lineBreakG.lastIndex = cur;
    var match = lineBreakG.exec(input);

    if (match && match.index < offset) {
      ++line;
      cur = match.index + match[0].length;
    } else {
      return new Position(line, offset - cur);
    }
  } // istanbul ignore next


  throw new Error("Unreachable");
}

var BaseParser =
/*#__PURE__*/
function () {
  function BaseParser() {}

  var _proto = BaseParser.prototype;

  // Properties set by constructor in index.js
  // Initialized by Tokenizer
  _proto.isReservedWord = function isReservedWord(word) {
    if (word === "await") {
      return this.inModule;
    } else {
      return reservedWords[6](word);
    }
  };

  _proto.hasPlugin = function hasPlugin(name) {
    return !!this.plugins[name];
  };

  return BaseParser;
}();

/* eslint max-len: 0 */

/**
 * Based on the comment attachment algorithm used in espree and estraverse.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
function last(stack) {
  return stack[stack.length - 1];
}

var CommentsParser =
/*#__PURE__*/
function (_BaseParser) {
  _inheritsLoose(CommentsParser, _BaseParser);

  function CommentsParser() {
    return _BaseParser.apply(this, arguments) || this;
  }

  var _proto = CommentsParser.prototype;

  _proto.addComment = function addComment(comment) {
    if (this.filename) comment.loc.filename = this.filename;
    this.state.trailingComments.push(comment);
    this.state.leadingComments.push(comment);
  };

  _proto.processComment = function processComment(node) {
    if (node.type === "Program" && node.body.length > 0) return;
    var stack = this.state.commentStack;
    var firstChild, lastChild, trailingComments, i, j;

    if (this.state.trailingComments.length > 0) {
      // If the first comment in trailingComments comes after the
      // current node, then we're good - all comments in the array will
      // come after the node and so it's safe to add them as official
      // trailingComments.
      if (this.state.trailingComments[0].start >= node.end) {
        trailingComments = this.state.trailingComments;
        this.state.trailingComments = [];
      } else {
        // Otherwise, if the first comment doesn't come after the
        // current node, that means we have a mix of leading and trailing
        // comments in the array and that leadingComments contains the
        // same items as trailingComments. Reset trailingComments to
        // zero items and we'll handle this by evaluating leadingComments
        // later.
        this.state.trailingComments.length = 0;
      }
    } else if (stack.length > 0) {
      var lastInStack = last(stack);

      if (lastInStack.trailingComments && lastInStack.trailingComments[0].start >= node.end) {
        trailingComments = lastInStack.trailingComments;
        delete lastInStack.trailingComments;
      }
    } // Eating the stack.


    if (stack.length > 0 && last(stack).start >= node.start) {
      firstChild = stack.pop();
    }

    while (stack.length > 0 && last(stack).start >= node.start) {
      lastChild = stack.pop();
    }

    if (!lastChild && firstChild) lastChild = firstChild; // Attach comments that follow a trailing comma on the last
    // property in an object literal or a trailing comma in function arguments
    // as trailing comments

    if (firstChild && this.state.leadingComments.length > 0) {
      var lastComment = last(this.state.leadingComments);

      if (firstChild.type === "ObjectProperty") {
        if (lastComment.start >= node.start) {
          if (this.state.commentPreviousNode) {
            for (j = 0; j < this.state.leadingComments.length; j++) {
              if (this.state.leadingComments[j].end < this.state.commentPreviousNode.end) {
                this.state.leadingComments.splice(j, 1);
                j--;
              }
            }

            if (this.state.leadingComments.length > 0) {
              firstChild.trailingComments = this.state.leadingComments;
              this.state.leadingComments = [];
            }
          }
        }
      } else if (node.type === "CallExpression" && node.arguments && node.arguments.length) {
        var lastArg = last(node.arguments);

        if (lastArg && lastComment.start >= lastArg.start && lastComment.end <= node.end) {
          if (this.state.commentPreviousNode) {
            if (this.state.leadingComments.length > 0) {
              lastArg.trailingComments = this.state.leadingComments;
              this.state.leadingComments = [];
            }
          }
        }
      }
    }

    if (lastChild) {
      if (lastChild.leadingComments) {
        if (lastChild !== node && lastChild.leadingComments.length > 0 && last(lastChild.leadingComments).end <= node.start) {
          node.leadingComments = lastChild.leadingComments;
          delete lastChild.leadingComments;
        } else {
          // A leading comment for an anonymous class had been stolen by its first ClassMethod,
          // so this takes back the leading comment.
          // See also: https://github.com/eslint/espree/issues/158
          for (i = lastChild.leadingComments.length - 2; i >= 0; --i) {
            if (lastChild.leadingComments[i].end <= node.start) {
              node.leadingComments = lastChild.leadingComments.splice(0, i + 1);
              break;
            }
          }
        }
      }
    } else if (this.state.leadingComments.length > 0) {
      if (last(this.state.leadingComments).end <= node.start) {
        if (this.state.commentPreviousNode) {
          for (j = 0; j < this.state.leadingComments.length; j++) {
            if (this.state.leadingComments[j].end < this.state.commentPreviousNode.end) {
              this.state.leadingComments.splice(j, 1);
              j--;
            }
          }
        }

        if (this.state.leadingComments.length > 0) {
          node.leadingComments = this.state.leadingComments;
          this.state.leadingComments = [];
        }
      } else {
        // https://github.com/eslint/espree/issues/2
        //
        // In special cases, such as return (without a value) and
        // debugger, all comments will end up as leadingComments and
        // will otherwise be eliminated. This step runs when the
        // commentStack is empty and there are comments left
        // in leadingComments.
        //
        // This loop figures out the stopping point between the actual
        // leading and trailing comments by finding the location of the
        // first comment that comes after the given node.
        for (i = 0; i < this.state.leadingComments.length; i++) {
          if (this.state.leadingComments[i].end > node.start) {
            break;
          }
        } // Split the array based on the location of the first comment
        // that comes after the node. Keep in mind that this could
        // result in an empty array, and if so, the array must be
        // deleted.


        var leadingComments = this.state.leadingComments.slice(0, i);

        if (leadingComments.length) {
          node.leadingComments = leadingComments;
        } // Similarly, trailing comments are attached later. The variable
        // must be reset to null if there are no trailing comments.


        trailingComments = this.state.leadingComments.slice(i);

        if (trailingComments.length === 0) {
          trailingComments = null;
        }
      }
    }

    this.state.commentPreviousNode = node;

    if (trailingComments) {
      if (trailingComments.length && trailingComments[0].start >= node.start && last(trailingComments).end <= node.end) {
        node.innerComments = trailingComments;
      } else {
        node.trailingComments = trailingComments;
      }
    }

    stack.push(node);
  };

  return CommentsParser;
}(BaseParser);

// takes an offset integer (into the current `input`) to indicate
// the location of the error, attaches the position to the end
// of the error message, and then raises a `SyntaxError` with that
// message.

var LocationParser =
/*#__PURE__*/
function (_CommentsParser) {
  _inheritsLoose(LocationParser, _CommentsParser);

  function LocationParser() {
    return _CommentsParser.apply(this, arguments) || this;
  }

  var _proto = LocationParser.prototype;

  _proto.raise = function raise(pos, message, missingPluginNames) {
    var loc = getLineInfo(this.input, pos);
    message += ` (${loc.line}:${loc.column})`; // $FlowIgnore

    var err = new SyntaxError(message);
    err.pos = pos;
    err.loc = loc;

    if (missingPluginNames) {
      err.missingPlugin = missingPluginNames;
    }

    throw err;
  };

  return LocationParser;
}(CommentsParser);

var State =
/*#__PURE__*/
function () {
  function State() {}

  var _proto = State.prototype;

  _proto.init = function init(options, input) {
    this.strict = options.strictMode === false ? false : options.sourceType === "module";
    this.input = input;
    this.potentialArrowAt = -1;
    this.noArrowAt = [];
    this.noArrowParamsConversionAt = []; // eslint-disable-next-line max-len

    this.inMethod = this.inFunction = this.inParameters = this.maybeInArrowParameters = this.inGenerator = this.inAsync = this.inPropertyName = this.inType = this.inClassProperty = this.noAnonFunctionType = false;
    this.classLevel = 0;
    this.labels = [];
    this.decoratorStack = [[]];
    this.yieldInPossibleArrowParameters = null;
    this.tokens = [];
    this.comments = [];
    this.trailingComments = [];
    this.leadingComments = [];
    this.commentStack = []; // $FlowIgnore

    this.commentPreviousNode = null;
    this.pos = this.lineStart = 0;
    this.curLine = options.startLine;
    this.type = types.eof;
    this.value = null;
    this.start = this.end = this.pos;
    this.startLoc = this.endLoc = this.curPosition(); // $FlowIgnore

    this.lastTokEndLoc = this.lastTokStartLoc = null;
    this.lastTokStart = this.lastTokEnd = this.pos;
    this.context = [types$1.braceStatement];
    this.exprAllowed = true;
    this.containsEsc = this.containsOctal = false;
    this.octalPosition = null;
    this.invalidTemplateEscapePosition = null;
    this.exportedIdentifiers = [];
  }; // TODO


  _proto.curPosition = function curPosition() {
    return new Position(this.curLine, this.pos - this.lineStart);
  };

  _proto.clone = function clone(skipArrays) {
    var _this = this;

    var state = new State();
    Object.keys(this).forEach(function (key) {
      // $FlowIgnore
      var val = _this[key];

      if ((!skipArrays || key === "context") && Array.isArray(val)) {
        val = val.slice();
      } // $FlowIgnore


      state[key] = val;
    });
    return state;
  };

  return State;
}();

var _isDigit = function isDigit(code) {
  return code >= 48 && code <= 57;
};

/* eslint max-len: 0 */
// an immediate sibling of NumericLiteralSeparator _

var forbiddenNumericSeparatorSiblings = {
  decBinOct: [46, 66, 69, 79, 95, // multiple separators are not allowed
  98, 101, 111],
  hex: [46, 88, 95, // multiple separators are not allowed
  120]
};
var allowedNumericSeparatorSiblings = {};
allowedNumericSeparatorSiblings.bin = [// 0 - 1
48, 49];
allowedNumericSeparatorSiblings.oct = allowedNumericSeparatorSiblings.bin.concat([50, 51, 52, 53, 54, 55]);
allowedNumericSeparatorSiblings.dec = allowedNumericSeparatorSiblings.oct.concat([56, 57]);
allowedNumericSeparatorSiblings.hex = allowedNumericSeparatorSiblings.dec.concat([65, 66, 67, 68, 69, 70, 97, 98, 99, 100, 101, 102]); // Object type used to represent tokens. Note that normally, tokens
// simply exist as properties on the parser object. This is only
// used for the onToken callback and the external tokenizer.

var Token = function Token(state) {
  this.type = state.type;
  this.value = state.value;
  this.start = state.start;
  this.end = state.end;
  this.loc = new SourceLocation(state.startLoc, state.endLoc);
}; // ## Tokenizer

function codePointToString(code) {
  // UTF-16 Decoding
  if (code <= 0xffff) {
    return String.fromCharCode(code);
  } else {
    return String.fromCharCode((code - 0x10000 >> 10) + 0xd800, (code - 0x10000 & 1023) + 0xdc00);
  }
}

var Tokenizer =
/*#__PURE__*/
function (_LocationParser) {
  _inheritsLoose(Tokenizer, _LocationParser);

  // Forward-declarations
  // parser/util.js
  function Tokenizer(options, input) {
    var _this;

    _this = _LocationParser.call(this) || this;
    _this.state = new State();

    _this.state.init(options, input);

    _this.isLookahead = false;
    return _this;
  } // Move to the next token


  var _proto = Tokenizer.prototype;

  _proto.next = function next() {
    if (this.options.tokens && !this.isLookahead) {
      this.state.tokens.push(new Token(this.state));
    }

    this.state.lastTokEnd = this.state.end;
    this.state.lastTokStart = this.state.start;
    this.state.lastTokEndLoc = this.state.endLoc;
    this.state.lastTokStartLoc = this.state.startLoc;
    this.nextToken();
  }; // TODO


  _proto.eat = function eat(type) {
    if (this.match(type)) {
      this.next();
      return true;
    } else {
      return false;
    }
  }; // TODO


  _proto.match = function match(type) {
    return this.state.type === type;
  }; // TODO


  _proto.isKeyword = function isKeyword$$1(word) {
    return isKeyword(word);
  }; // TODO


  _proto.lookahead = function lookahead() {
    var old = this.state;
    this.state = old.clone(true);
    this.isLookahead = true;
    this.next();
    this.isLookahead = false;
    var curr = this.state;
    this.state = old;
    return curr;
  }; // Toggle strict mode. Re-reads the next number or string to please
  // pedantic tests (`"use strict"; 010;` should fail).


  _proto.setStrict = function setStrict(strict) {
    this.state.strict = strict;
    if (!this.match(types.num) && !this.match(types.string)) return;
    this.state.pos = this.state.start;

    while (this.state.pos < this.state.lineStart) {
      this.state.lineStart = this.input.lastIndexOf("\n", this.state.lineStart - 2) + 1;
      --this.state.curLine;
    }

    this.nextToken();
  };

  _proto.curContext = function curContext() {
    return this.state.context[this.state.context.length - 1];
  }; // Read a single token, updating the parser object's token-related
  // properties.


  _proto.nextToken = function nextToken() {
    var curContext = this.curContext();
    if (!curContext || !curContext.preserveSpace) this.skipSpace();
    this.state.containsOctal = false;
    this.state.octalPosition = null;
    this.state.start = this.state.pos;
    this.state.startLoc = this.state.curPosition();

    if (this.state.pos >= this.input.length) {
      this.finishToken(types.eof);
      return;
    }

    if (curContext.override) {
      curContext.override(this);
    } else {
      this.readToken(this.fullCharCodeAtPos());
    }
  };

  _proto.readToken = function readToken(code) {
    // Identifier or keyword. '\uXXXX' sequences are allowed in
    // identifiers, so '\' also dispatches to that.
    if (isIdentifierStart(code) || code === 92) {
      this.readWord();
    } else {
      this.getTokenFromCode(code);
    }
  };

  _proto.fullCharCodeAtPos = function fullCharCodeAtPos() {
    var code = this.input.charCodeAt(this.state.pos);
    if (code <= 0xd7ff || code >= 0xe000) return code;
    var next = this.input.charCodeAt(this.state.pos + 1);
    return (code << 10) + next - 0x35fdc00;
  };

  _proto.pushComment = function pushComment(block, text, start, end, startLoc, endLoc) {
    var comment = {
      type: block ? "CommentBlock" : "CommentLine",
      value: text,
      start: start,
      end: end,
      loc: new SourceLocation(startLoc, endLoc)
    };

    if (!this.isLookahead) {
      if (this.options.tokens) this.state.tokens.push(comment);
      this.state.comments.push(comment);
      this.addComment(comment);
    }
  };

  _proto.skipBlockComment = function skipBlockComment() {
    var startLoc = this.state.curPosition();
    var start = this.state.pos;
    var end = this.input.indexOf("*/", this.state.pos += 2);
    if (end === -1) this.raise(this.state.pos - 2, "Unterminated comment");
    this.state.pos = end + 2;
    lineBreakG.lastIndex = start;
    var match;

    while ((match = lineBreakG.exec(this.input)) && match.index < this.state.pos) {
      ++this.state.curLine;
      this.state.lineStart = match.index + match[0].length;
    }

    this.pushComment(true, this.input.slice(start + 2, end), start, this.state.pos, startLoc, this.state.curPosition());
  };

  _proto.skipLineComment = function skipLineComment(startSkip) {
    var start = this.state.pos;
    var startLoc = this.state.curPosition();
    var ch = this.input.charCodeAt(this.state.pos += startSkip);

    if (this.state.pos < this.input.length) {
      while (ch !== 10 && ch !== 13 && ch !== 8232 && ch !== 8233 && ++this.state.pos < this.input.length) {
        ch = this.input.charCodeAt(this.state.pos);
      }
    }

    this.pushComment(false, this.input.slice(start + startSkip, this.state.pos), start, this.state.pos, startLoc, this.state.curPosition());
  }; // Called at the start of the parse and after every token. Skips
  // whitespace and comments, and.


  _proto.skipSpace = function skipSpace() {
    loop: while (this.state.pos < this.input.length) {
      var ch = this.input.charCodeAt(this.state.pos);

      switch (ch) {
        case 32:
        case 160:
          ++this.state.pos;
          break;

        case 13:
          if (this.input.charCodeAt(this.state.pos + 1) === 10) {
            ++this.state.pos;
          }

        case 10:
        case 8232:
        case 8233:
          ++this.state.pos;
          ++this.state.curLine;
          this.state.lineStart = this.state.pos;
          break;

        case 47:
          switch (this.input.charCodeAt(this.state.pos + 1)) {
            case 42:
              this.skipBlockComment();
              break;

            case 47:
              this.skipLineComment(2);
              break;

            default:
              break loop;
          }

          break;

        default:
          if (ch > 8 && ch < 14 || ch >= 5760 && nonASCIIwhitespace.test(String.fromCharCode(ch))) {
            ++this.state.pos;
          } else {
            break loop;
          }

      }
    }
  }; // Called at the end of every token. Sets `end`, `val`, and
  // maintains `context` and `exprAllowed`, and skips the space after
  // the token, so that the next one's `start` will point at the
  // right position.


  _proto.finishToken = function finishToken(type, val) {
    this.state.end = this.state.pos;
    this.state.endLoc = this.state.curPosition();
    var prevType = this.state.type;
    this.state.type = type;
    this.state.value = val;
    this.updateContext(prevType);
  }; // ### Token reading
  // This is the function that is called to fetch the next token. It
  // is somewhat obscure, because it works in character codes rather
  // than characters, and because operator parsing has been inlined
  // into it.
  //
  // All in the name of speed.
  //


  _proto.readToken_dot = function readToken_dot() {
    var next = this.input.charCodeAt(this.state.pos + 1);

    if (next >= 48 && next <= 57) {
      this.readNumber(true);
      return;
    }

    var next2 = this.input.charCodeAt(this.state.pos + 2);

    if (next === 46 && next2 === 46) {
      this.state.pos += 3;
      this.finishToken(types.ellipsis);
    } else {
      ++this.state.pos;
      this.finishToken(types.dot);
    }
  };

  _proto.readToken_slash = function readToken_slash() {
    // '/'
    if (this.state.exprAllowed) {
      ++this.state.pos;
      this.readRegexp();
      return;
    }

    var next = this.input.charCodeAt(this.state.pos + 1);

    if (next === 61) {
      this.finishOp(types.assign, 2);
    } else {
      this.finishOp(types.slash, 1);
    }
  };

  _proto.readToken_mult_modulo = function readToken_mult_modulo(code) {
    // '%*'
    var type = code === 42 ? types.star : types.modulo;
    var width = 1;
    var next = this.input.charCodeAt(this.state.pos + 1);
    var exprAllowed = this.state.exprAllowed; // Exponentiation operator **

    if (code === 42 && next === 42) {
      width++;
      next = this.input.charCodeAt(this.state.pos + 2);
      type = types.exponent;
    }

    if (next === 61 && !exprAllowed) {
      width++;
      type = types.assign;
    }

    this.finishOp(type, width);
  };

  _proto.readToken_pipe_amp = function readToken_pipe_amp(code) {
    // '|&'
    var next = this.input.charCodeAt(this.state.pos + 1);

    if (next === code) {
      this.finishOp(code === 124 ? types.logicalOR : types.logicalAND, 2);
      return;
    }

    if (code === 124) {
      // '|>'
      if (next === 62) {
        this.finishOp(types.pipeline, 2);
        return;
      } else if (next === 125 && this.hasPlugin("flow")) {
        // '|}'
        this.finishOp(types.braceBarR, 2);
        return;
      }
    }

    if (next === 61) {
      this.finishOp(types.assign, 2);
      return;
    }

    this.finishOp(code === 124 ? types.bitwiseOR : types.bitwiseAND, 1);
  };

  _proto.readToken_caret = function readToken_caret() {
    // '^'
    var next = this.input.charCodeAt(this.state.pos + 1);

    if (next === 61) {
      this.finishOp(types.assign, 2);
    } else {
      this.finishOp(types.bitwiseXOR, 1);
    }
  };

  _proto.readToken_plus_min = function readToken_plus_min(code) {
    // '+-'
    var next = this.input.charCodeAt(this.state.pos + 1);

    if (next === code) {
      if (next === 45 && !this.inModule && this.input.charCodeAt(this.state.pos + 2) === 62 && lineBreak.test(this.input.slice(this.state.lastTokEnd, this.state.pos))) {
        // A `-->` line comment
        this.skipLineComment(3);
        this.skipSpace();
        this.nextToken();
        return;
      }

      this.finishOp(types.incDec, 2);
      return;
    }

    if (next === 61) {
      this.finishOp(types.assign, 2);
    } else {
      this.finishOp(types.plusMin, 1);
    }
  };

  _proto.readToken_lt_gt = function readToken_lt_gt(code) {
    // '<>'
    var next = this.input.charCodeAt(this.state.pos + 1);
    var size = 1;

    if (next === code) {
      size = code === 62 && this.input.charCodeAt(this.state.pos + 2) === 62 ? 3 : 2;

      if (this.input.charCodeAt(this.state.pos + size) === 61) {
        this.finishOp(types.assign, size + 1);
        return;
      }

      this.finishOp(types.bitShift, size);
      return;
    }

    if (next === 33 && code === 60 && !this.inModule && this.input.charCodeAt(this.state.pos + 2) === 45 && this.input.charCodeAt(this.state.pos + 3) === 45) {
      // `<!--`, an XML-style comment that should be interpreted as a line comment
      this.skipLineComment(4);
      this.skipSpace();
      this.nextToken();
      return;
    }

    if (next === 61) {
      // <= | >=
      size = 2;
    }

    this.finishOp(types.relational, size);
  };

  _proto.readToken_eq_excl = function readToken_eq_excl(code) {
    // '=!'
    var next = this.input.charCodeAt(this.state.pos + 1);

    if (next === 61) {
      this.finishOp(types.equality, this.input.charCodeAt(this.state.pos + 2) === 61 ? 3 : 2);
      return;
    }

    if (code === 61 && next === 62) {
      // '=>'
      this.state.pos += 2;
      this.finishToken(types.arrow);
      return;
    }

    this.finishOp(code === 61 ? types.eq : types.bang, 1);
  };

  _proto.readToken_question = function readToken_question() {
    // '?'
    var next = this.input.charCodeAt(this.state.pos + 1);
    var next2 = this.input.charCodeAt(this.state.pos + 2);

    if (next === 63) {
      // '??'
      this.finishOp(types.nullishCoalescing, 2);
    } else if (next === 46 && !(next2 >= 48 && next2 <= 57)) {
      // '.' not followed by a number
      this.state.pos += 2;
      this.finishToken(types.questionDot);
    } else {
      ++this.state.pos;
      this.finishToken(types.question);
    }
  };

  _proto.getTokenFromCode = function getTokenFromCode(code) {
    switch (code) {
      case 35:
        if ((this.hasPlugin("classPrivateProperties") || this.hasPlugin("classPrivateMethods")) && this.state.classLevel > 0) {
          ++this.state.pos;
          this.finishToken(types.hash);
          return;
        } else {
          this.raise(this.state.pos, `Unexpected character '${codePointToString(code)}'`);
        }

      // The interpretation of a dot depends on whether it is followed
      // by a digit or another two dots.

      case 46:
        this.readToken_dot();
        return;
      // Punctuation tokens.

      case 40:
        ++this.state.pos;
        this.finishToken(types.parenL);
        return;

      case 41:
        ++this.state.pos;
        this.finishToken(types.parenR);
        return;

      case 59:
        ++this.state.pos;
        this.finishToken(types.semi);
        return;

      case 44:
        ++this.state.pos;
        this.finishToken(types.comma);
        return;

      case 91:
        ++this.state.pos;
        this.finishToken(types.bracketL);
        return;

      case 93:
        ++this.state.pos;
        this.finishToken(types.bracketR);
        return;

      case 123:
        if (this.hasPlugin("flow") && this.input.charCodeAt(this.state.pos + 1) === 124) {
          this.finishOp(types.braceBarL, 2);
        } else {
          ++this.state.pos;
          this.finishToken(types.braceL);
        }

        return;

      case 125:
        ++this.state.pos;
        this.finishToken(types.braceR);
        return;

      case 58:
        if (this.hasPlugin("functionBind") && this.input.charCodeAt(this.state.pos + 1) === 58) {
          this.finishOp(types.doubleColon, 2);
        } else {
          ++this.state.pos;
          this.finishToken(types.colon);
        }

        return;

      case 63:
        this.readToken_question();
        return;

      case 64:
        ++this.state.pos;
        this.finishToken(types.at);
        return;

      case 96:
        ++this.state.pos;
        this.finishToken(types.backQuote);
        return;

      case 48:
        {
          var next = this.input.charCodeAt(this.state.pos + 1); // '0x', '0X' - hex number

          if (next === 120 || next === 88) {
            this.readRadixNumber(16);
            return;
          } // '0o', '0O' - octal number


          if (next === 111 || next === 79) {
            this.readRadixNumber(8);
            return;
          } // '0b', '0B' - binary number


          if (next === 98 || next === 66) {
            this.readRadixNumber(2);
            return;
          }
        }
      // Anything else beginning with a digit is an integer, octal
      // number, or float.

      case 49:
      case 50:
      case 51:
      case 52:
      case 53:
      case 54:
      case 55:
      case 56:
      case 57:
        this.readNumber(false);
        return;
      // Quotes produce strings.

      case 34:
      case 39:
        this.readString(code);
        return;
      // Operators are parsed inline in tiny state machines. '=' (charCodes.equalsTo) is
      // often referred to. `finishOp` simply skips the amount of
      // characters it is given as second argument, and returns a token
      // of the type given by its first argument.

      case 47:
        this.readToken_slash();
        return;

      case 37:
      case 42:
        this.readToken_mult_modulo(code);
        return;

      case 124:
      case 38:
        this.readToken_pipe_amp(code);
        return;

      case 94:
        this.readToken_caret();
        return;

      case 43:
      case 45:
        this.readToken_plus_min(code);
        return;

      case 60:
      case 62:
        this.readToken_lt_gt(code);
        return;

      case 61:
      case 33:
        this.readToken_eq_excl(code);
        return;

      case 126:
        this.finishOp(types.tilde, 1);
        return;
    }

    this.raise(this.state.pos, `Unexpected character '${codePointToString(code)}'`);
  };

  _proto.finishOp = function finishOp(type, size) {
    var str = this.input.slice(this.state.pos, this.state.pos + size);
    this.state.pos += size;
    this.finishToken(type, str);
  };

  _proto.readRegexp = function readRegexp() {
    var start = this.state.pos;
    var escaped, inClass;

    for (;;) {
      if (this.state.pos >= this.input.length) {
        this.raise(start, "Unterminated regular expression");
      }

      var ch = this.input.charAt(this.state.pos);

      if (lineBreak.test(ch)) {
        this.raise(start, "Unterminated regular expression");
      }

      if (escaped) {
        escaped = false;
      } else {
        if (ch === "[") {
          inClass = true;
        } else if (ch === "]" && inClass) {
          inClass = false;
        } else if (ch === "/" && !inClass) {
          break;
        }

        escaped = ch === "\\";
      }

      ++this.state.pos;
    }

    var content = this.input.slice(start, this.state.pos);
    ++this.state.pos;
    var validFlags = /^[gmsiyu]$/;
    var mods = "";

    while (this.state.pos < this.input.length) {
      var char = this.input[this.state.pos];
      var charCode = this.fullCharCodeAtPos();

      if (validFlags.test(char)) {
        ++this.state.pos;
        mods += char;
      } else if (isIdentifierChar(charCode) || charCode === 92) {
        this.raise(this.state.pos, "Invalid regular expression flag");
      } else {
        break;
      }
    }

    this.finishToken(types.regexp, {
      pattern: content,
      flags: mods
    });
  }; // Read an integer in the given radix. Return null if zero digits
  // were read, the integer value otherwise. When `len` is given, this
  // will return `null` unless the integer has exactly `len` digits.


  _proto.readInt = function readInt(radix, len) {
    var start = this.state.pos;
    var forbiddenSiblings = radix === 16 ? forbiddenNumericSeparatorSiblings.hex : forbiddenNumericSeparatorSiblings.decBinOct;
    var allowedSiblings = radix === 16 ? allowedNumericSeparatorSiblings.hex : radix === 10 ? allowedNumericSeparatorSiblings.dec : radix === 8 ? allowedNumericSeparatorSiblings.oct : allowedNumericSeparatorSiblings.bin;
    var total = 0;

    for (var i = 0, e = len == null ? Infinity : len; i < e; ++i) {
      var code = this.input.charCodeAt(this.state.pos);
      var val = void 0;

      if (this.hasPlugin("numericSeparator")) {
        var prev = this.input.charCodeAt(this.state.pos - 1);
        var next = this.input.charCodeAt(this.state.pos + 1);

        if (code === 95) {
          if (allowedSiblings.indexOf(next) === -1) {
            this.raise(this.state.pos, "Invalid or unexpected token");
          }

          if (forbiddenSiblings.indexOf(prev) > -1 || forbiddenSiblings.indexOf(next) > -1 || Number.isNaN(next)) {
            this.raise(this.state.pos, "Invalid or unexpected token");
          } // Ignore this _ character


          ++this.state.pos;
          continue;
        }
      }

      if (code >= 97) {
        val = code - 97 + 10;
      } else if (code >= 65) {
        val = code - 65 + 10;
      } else if (_isDigit(code)) {
        val = code - 48; // 0-9
      } else {
        val = Infinity;
      }

      if (val >= radix) break;
      ++this.state.pos;
      total = total * radix + val;
    }

    if (this.state.pos === start || len != null && this.state.pos - start !== len) {
      return null;
    }

    return total;
  };

  _proto.readRadixNumber = function readRadixNumber(radix) {
    var start = this.state.pos;
    var isBigInt = false;
    this.state.pos += 2; // 0x

    var val = this.readInt(radix);

    if (val == null) {
      this.raise(this.state.start + 2, "Expected number in radix " + radix);
    }

    if (this.hasPlugin("bigInt")) {
      if (this.input.charCodeAt(this.state.pos) === 110) {
        ++this.state.pos;
        isBigInt = true;
      }
    }

    if (isIdentifierStart(this.fullCharCodeAtPos())) {
      this.raise(this.state.pos, "Identifier directly after number");
    }

    if (isBigInt) {
      var str = this.input.slice(start, this.state.pos).replace(/[_n]/g, "");
      this.finishToken(types.bigint, str);
      return;
    }

    this.finishToken(types.num, val);
  }; // Read an integer, octal integer, or floating-point number.


  _proto.readNumber = function readNumber(startsWithDot) {
    var start = this.state.pos;
    var octal = this.input.charCodeAt(start) === 48;
    var isFloat = false;
    var isBigInt = false;

    if (!startsWithDot && this.readInt(10) === null) {
      this.raise(start, "Invalid number");
    }

    if (octal && this.state.pos == start + 1) octal = false; // number === 0

    var next = this.input.charCodeAt(this.state.pos);

    if (next === 46 && !octal) {
      ++this.state.pos;
      this.readInt(10);
      isFloat = true;
      next = this.input.charCodeAt(this.state.pos);
    }

    if ((next === 69 || next === 101) && !octal) {
      next = this.input.charCodeAt(++this.state.pos);

      if (next === 43 || next === 45) {
        ++this.state.pos;
      }

      if (this.readInt(10) === null) this.raise(start, "Invalid number");
      isFloat = true;
      next = this.input.charCodeAt(this.state.pos);
    }

    if (this.hasPlugin("bigInt")) {
      if (next === 110) {
        // disallow floats and legacy octal syntax, new style octal ("0o") is handled in this.readRadixNumber
        if (isFloat || octal) this.raise(start, "Invalid BigIntLiteral");
        ++this.state.pos;
        isBigInt = true;
      }
    }

    if (isIdentifierStart(this.fullCharCodeAtPos())) {
      this.raise(this.state.pos, "Identifier directly after number");
    } // remove "_" for numeric literal separator, and "n" for BigInts


    var str = this.input.slice(start, this.state.pos).replace(/[_n]/g, "");

    if (isBigInt) {
      this.finishToken(types.bigint, str);
      return;
    }

    var val;

    if (isFloat) {
      val = parseFloat(str);
    } else if (!octal || str.length === 1) {
      val = parseInt(str, 10);
    } else if (this.state.strict) {
      this.raise(start, "Invalid number");
    } else if (/[89]/.test(str)) {
      val = parseInt(str, 10);
    } else {
      val = parseInt(str, 8);
    }

    this.finishToken(types.num, val);
  }; // Read a string value, interpreting backslash-escapes.


  _proto.readCodePoint = function readCodePoint(throwOnInvalid) {
    var ch = this.input.charCodeAt(this.state.pos);
    var code;

    if (ch === 123) {
      var codePos = ++this.state.pos;
      code = this.readHexChar(this.input.indexOf("}", this.state.pos) - this.state.pos, throwOnInvalid);
      ++this.state.pos;

      if (code === null) {
        // $FlowFixMe (is this always non-null?)
        --this.state.invalidTemplateEscapePosition; // to point to the '\'' instead of the 'u'
      } else if (code > 0x10ffff) {
        if (throwOnInvalid) {
          this.raise(codePos, "Code point out of bounds");
        } else {
          this.state.invalidTemplateEscapePosition = codePos - 2;
          return null;
        }
      }
    } else {
      code = this.readHexChar(4, throwOnInvalid);
    }

    return code;
  };

  _proto.readString = function readString(quote) {
    var out = "",
        chunkStart = ++this.state.pos;

    for (;;) {
      if (this.state.pos >= this.input.length) {
        this.raise(this.state.start, "Unterminated string constant");
      }

      var ch = this.input.charCodeAt(this.state.pos);
      if (ch === quote) break;

      if (ch === 92) {
        out += this.input.slice(chunkStart, this.state.pos); // $FlowFixMe

        out += this.readEscapedChar(false);
        chunkStart = this.state.pos;
      } else {
        if (isNewLine(ch)) {
          this.raise(this.state.start, "Unterminated string constant");
        }

        ++this.state.pos;
      }
    }

    out += this.input.slice(chunkStart, this.state.pos++);
    this.finishToken(types.string, out);
  }; // Reads template string tokens.


  _proto.readTmplToken = function readTmplToken() {
    var out = "",
        chunkStart = this.state.pos,
        containsInvalid = false;

    for (;;) {
      if (this.state.pos >= this.input.length) {
        this.raise(this.state.start, "Unterminated template");
      }

      var ch = this.input.charCodeAt(this.state.pos);

      if (ch === 96 || ch === 36 && this.input.charCodeAt(this.state.pos + 1) === 123) {
        if (this.state.pos === this.state.start && this.match(types.template)) {
          if (ch === 36) {
            this.state.pos += 2;
            this.finishToken(types.dollarBraceL);
            return;
          } else {
            ++this.state.pos;
            this.finishToken(types.backQuote);
            return;
          }
        }

        out += this.input.slice(chunkStart, this.state.pos);
        this.finishToken(types.template, containsInvalid ? null : out);
        return;
      }

      if (ch === 92) {
        out += this.input.slice(chunkStart, this.state.pos);
        var escaped = this.readEscapedChar(true);

        if (escaped === null) {
          containsInvalid = true;
        } else {
          out += escaped;
        }

        chunkStart = this.state.pos;
      } else if (isNewLine(ch)) {
        out += this.input.slice(chunkStart, this.state.pos);
        ++this.state.pos;

        switch (ch) {
          case 13:
            if (this.input.charCodeAt(this.state.pos) === 10) {
              ++this.state.pos;
            }

          case 10:
            out += "\n";
            break;

          default:
            out += String.fromCharCode(ch);
            break;
        }

        ++this.state.curLine;
        this.state.lineStart = this.state.pos;
        chunkStart = this.state.pos;
      } else {
        ++this.state.pos;
      }
    }
  }; // Used to read escaped characters


  _proto.readEscapedChar = function readEscapedChar(inTemplate) {
    var throwOnInvalid = !inTemplate;
    var ch = this.input.charCodeAt(++this.state.pos);
    ++this.state.pos;

    switch (ch) {
      case 110:
        return "\n";

      case 114:
        return "\r";

      case 120:
        {
          var code = this.readHexChar(2, throwOnInvalid);
          return code === null ? null : String.fromCharCode(code);
        }

      case 117:
        {
          var _code = this.readCodePoint(throwOnInvalid);

          return _code === null ? null : codePointToString(_code);
        }

      case 116:
        return "\t";

      case 98:
        return "\b";

      case 118:
        return "\u000b";

      case 102:
        return "\f";

      case 13:
        if (this.input.charCodeAt(this.state.pos) === 10) {
          ++this.state.pos;
        }

      case 10:
        this.state.lineStart = this.state.pos;
        ++this.state.curLine;
        return "";

      default:
        if (ch >= 48 && ch <= 55) {
          var codePos = this.state.pos - 1; // $FlowFixMe

          var octalStr = this.input.substr(this.state.pos - 1, 3).match(/^[0-7]+/)[0];
          var octal = parseInt(octalStr, 8);

          if (octal > 255) {
            octalStr = octalStr.slice(0, -1);
            octal = parseInt(octalStr, 8);
          }

          if (octal > 0) {
            if (inTemplate) {
              this.state.invalidTemplateEscapePosition = codePos;
              return null;
            } else if (this.state.strict) {
              this.raise(codePos, "Octal literal in strict mode");
            } else if (!this.state.containsOctal) {
              // These properties are only used to throw an error for an octal which occurs
              // in a directive which occurs prior to a "use strict" directive.
              this.state.containsOctal = true;
              this.state.octalPosition = codePos;
            }
          }

          this.state.pos += octalStr.length - 1;
          return String.fromCharCode(octal);
        }

        return String.fromCharCode(ch);
    }
  }; // Used to read character escape sequences ('\x', '\u').


  _proto.readHexChar = function readHexChar(len, throwOnInvalid) {
    var codePos = this.state.pos;
    var n = this.readInt(16, len);

    if (n === null) {
      if (throwOnInvalid) {
        this.raise(codePos, "Bad character escape sequence");
      } else {
        this.state.pos = codePos - 1;
        this.state.invalidTemplateEscapePosition = codePos - 1;
      }
    }

    return n;
  }; // Read an identifier, and return it as a string. Sets `this.state.containsEsc`
  // to whether the word contained a '\u' escape.
  //
  // Incrementally adds only escaped chars, adding other chunks as-is
  // as a micro-optimization.


  _proto.readWord1 = function readWord1() {
    this.state.containsEsc = false;
    var word = "",
        first = true,
        chunkStart = this.state.pos;

    while (this.state.pos < this.input.length) {
      var ch = this.fullCharCodeAtPos();

      if (isIdentifierChar(ch)) {
        this.state.pos += ch <= 0xffff ? 1 : 2;
      } else if (this.state.isIterator && ch === 64) {
        this.state.pos += 1;
      } else if (ch === 92) {
        this.state.containsEsc = true;
        word += this.input.slice(chunkStart, this.state.pos);
        var escStart = this.state.pos;

        if (this.input.charCodeAt(++this.state.pos) !== 117) {
          this.raise(this.state.pos, "Expecting Unicode escape sequence \\uXXXX");
        }

        ++this.state.pos;
        var esc = this.readCodePoint(true); // $FlowFixMe (thinks esc may be null, but throwOnInvalid is true)

        if (!(first ? isIdentifierStart : isIdentifierChar)(esc, true)) {
          this.raise(escStart, "Invalid Unicode escape");
        } // $FlowFixMe


        word += codePointToString(esc);
        chunkStart = this.state.pos;
      } else {
        break;
      }

      first = false;
    }

    return word + this.input.slice(chunkStart, this.state.pos);
  };

  _proto.isIterator = function isIterator(word) {
    return word === "@@iterator" || word === "@@asyncIterator";
  }; // Read an identifier or keyword token. Will check for reserved
  // words when necessary.


  _proto.readWord = function readWord() {
    var word = this.readWord1();
    var type = types.name;

    if (this.isKeyword(word)) {
      if (this.state.containsEsc) {
        this.raise(this.state.pos, `Escape sequence in keyword ${word}`);
      }

      type = keywords[word];
    } // Allow @@iterator and @@asyncIterator as a identifier only inside type


    if (this.state.isIterator && (!this.isIterator(word) || !this.state.inType)) {
      this.raise(this.state.pos, `Invalid identifier ${word}`);
    }

    this.finishToken(type, word);
  };

  _proto.braceIsBlock = function braceIsBlock(prevType) {
    if (prevType === types.colon) {
      var parent = this.curContext();

      if (parent === types$1.braceStatement || parent === types$1.braceExpression) {
        return !parent.isExpr;
      }
    }

    if (prevType === types._return) {
      return lineBreak.test(this.input.slice(this.state.lastTokEnd, this.state.start));
    }

    if (prevType === types._else || prevType === types.semi || prevType === types.eof || prevType === types.parenR) {
      return true;
    }

    if (prevType === types.braceL) {
      return this.curContext() === types$1.braceStatement;
    }

    if (prevType === types.relational) {
      // `class C<T> { ... }`
      return true;
    }

    return !this.state.exprAllowed;
  };

  _proto.updateContext = function updateContext(prevType) {
    var type = this.state.type;
    var update;

    if (type.keyword && (prevType === types.dot || prevType === types.questionDot)) {
      this.state.exprAllowed = false;
    } else if (update = type.updateContext) {
      update.call(this, prevType);
    } else {
      this.state.exprAllowed = type.beforeExpr;
    }
  };

  return Tokenizer;
}(LocationParser);

var UtilParser =
/*#__PURE__*/
function (_Tokenizer) {
  _inheritsLoose(UtilParser, _Tokenizer);

  function UtilParser() {
    return _Tokenizer.apply(this, arguments) || this;
  }

  var _proto = UtilParser.prototype;

  // TODO
  _proto.addExtra = function addExtra(node, key, val) {
    if (!node) return;
    var extra = node.extra = node.extra || {};
    extra[key] = val;
  }; // TODO


  _proto.isRelational = function isRelational(op) {
    return this.match(types.relational) && this.state.value === op;
  }; // TODO


  _proto.expectRelational = function expectRelational(op) {
    if (this.isRelational(op)) {
      this.next();
    } else {
      this.unexpected(null, types.relational);
    }
  }; // eat() for relational operators.


  _proto.eatRelational = function eatRelational(op) {
    if (this.isRelational(op)) {
      this.next();
      return true;
    }

    return false;
  }; // Tests whether parsed token is a contextual keyword.


  _proto.isContextual = function isContextual(name) {
    return this.match(types.name) && this.state.value === name;
  };

  _proto.isLookaheadContextual = function isLookaheadContextual(name) {
    var l = this.lookahead();
    return l.type === types.name && l.value === name;
  }; // Consumes contextual keyword if possible.


  _proto.eatContextual = function eatContextual(name) {
    return this.state.value === name && this.eat(types.name);
  }; // Asserts that following token is given contextual keyword.


  _proto.expectContextual = function expectContextual(name, message) {
    if (!this.eatContextual(name)) this.unexpected(null, message);
  }; // Test whether a semicolon can be inserted at the current position.


  _proto.canInsertSemicolon = function canInsertSemicolon() {
    return this.match(types.eof) || this.match(types.braceR) || this.hasPrecedingLineBreak();
  };

  _proto.hasPrecedingLineBreak = function hasPrecedingLineBreak() {
    return lineBreak.test(this.input.slice(this.state.lastTokEnd, this.state.start));
  }; // TODO


  _proto.isLineTerminator = function isLineTerminator() {
    return this.eat(types.semi) || this.canInsertSemicolon();
  }; // Consume a semicolon, or, failing that, see if we are allowed to
  // pretend that there is a semicolon at this position.


  _proto.semicolon = function semicolon() {
    if (!this.isLineTerminator()) this.unexpected(null, types.semi);
  }; // Expect a token of a given type. If found, consume it, otherwise,
  // raise an unexpected token error at given pos.


  _proto.expect = function expect(type, pos) {
    this.eat(type) || this.unexpected(pos, type);
  }; // Raise an unexpected token error. Can take the expected token type
  // instead of a message string.


  _proto.unexpected = function unexpected(pos, messageOrType) {
    if (messageOrType === void 0) {
      messageOrType = "Unexpected token";
    }

    if (typeof messageOrType !== "string") {
      messageOrType = `Unexpected token, expected "${messageOrType.label}"`;
    }

    throw this.raise(pos != null ? pos : this.state.start, messageOrType);
  };

  _proto.expectPlugin = function expectPlugin(name, pos) {
    if (!this.hasPlugin(name)) {
      throw this.raise(pos != null ? pos : this.state.start, `This experimental syntax requires enabling the parser plugin: '${name}'`, [name]);
    }

    return true;
  };

  _proto.expectOnePlugin = function expectOnePlugin(names, pos) {
    var _this = this;

    if (!names.some(function (n) {
      return _this.hasPlugin(n);
    })) {
      throw this.raise(pos != null ? pos : this.state.start, `This experimental syntax requires enabling one of the following parser plugin(s): '${names.join(", ")}'`, names);
    }
  };

  return UtilParser;
}(Tokenizer);

// Start an AST node, attaching a start offset.
var commentKeys = ["leadingComments", "trailingComments", "innerComments"];

var Node =
/*#__PURE__*/
function () {
  function Node(parser, pos, loc) {
    this.type = "";
    this.start = pos;
    this.end = 0;
    this.loc = new SourceLocation(loc);
    if (parser && parser.options.ranges) this.range = [pos, 0];
    if (parser && parser.filename) this.loc.filename = parser.filename;
  }

  var _proto = Node.prototype;

  _proto.__clone = function __clone() {
    var _this = this;

    // $FlowIgnore
    var node2 = new Node();
    Object.keys(this).forEach(function (key) {
      // Do not clone comments that are already attached to the node
      if (commentKeys.indexOf(key) < 0) {
        // $FlowIgnore
        node2[key] = _this[key];
      }
    });
    return node2;
  };

  return Node;
}();

var NodeUtils =
/*#__PURE__*/
function (_UtilParser) {
  _inheritsLoose(NodeUtils, _UtilParser);

  function NodeUtils() {
    return _UtilParser.apply(this, arguments) || this;
  }

  var _proto2 = NodeUtils.prototype;

  _proto2.startNode = function startNode() {
    // $FlowIgnore
    return new Node(this, this.state.start, this.state.startLoc);
  };

  _proto2.startNodeAt = function startNodeAt(pos, loc) {
    // $FlowIgnore
    return new Node(this, pos, loc);
  };
  /** Start a new node with a previous node's location. */


  _proto2.startNodeAtNode = function startNodeAtNode(type) {
    return this.startNodeAt(type.start, type.loc.start);
  }; // Finish an AST node, adding `type` and `end` properties.


  _proto2.finishNode = function finishNode(node, type) {
    return this.finishNodeAt(node, type, this.state.lastTokEnd, this.state.lastTokEndLoc);
  }; // Finish node at given position


  _proto2.finishNodeAt = function finishNodeAt(node, type, pos, loc) {
    node.type = type;
    node.end = pos;
    node.loc.end = loc;
    if (this.options.ranges) node.range[1] = pos;
    this.processComment(node);
    return node;
  };
  /**
   * Reset the start location of node to the start location of locationNode
   */


  _proto2.resetStartLocationFromNode = function resetStartLocationFromNode(node, locationNode) {
    node.start = locationNode.start;
    node.loc.start = locationNode.loc.start;
    if (this.options.ranges) node.range[0] = locationNode.range[0];
  };

  return NodeUtils;
}(UtilParser);

var LValParser =
/*#__PURE__*/
function (_NodeUtils) {
  _inheritsLoose(LValParser, _NodeUtils);

  function LValParser() {
    return _NodeUtils.apply(this, arguments) || this;
  }

  var _proto = LValParser.prototype;

  // Forward-declaration: defined in expression.js
  // Forward-declaration: defined in statement.js
  // Convert existing expression atom to assignable pattern
  // if possible.
  _proto.toAssignable = function toAssignable(node, isBinding, contextDescription) {
    if (node) {
      switch (node.type) {
        case "Identifier":
        case "ObjectPattern":
        case "ArrayPattern":
        case "AssignmentPattern":
          break;

        case "ObjectExpression":
          node.type = "ObjectPattern";

          for (var index = 0; index < node.properties.length; index++) {
            var prop = node.properties[index];
            var isLast = index === node.properties.length - 1;
            this.toAssignableObjectExpressionProp(prop, isBinding, isLast);
          }

          break;

        case "ObjectProperty":
          this.toAssignable(node.value, isBinding, contextDescription);
          break;

        case "SpreadElement":
          {
            this.checkToRestConversion(node);
            node.type = "RestElement";
            var arg = node.argument;
            this.toAssignable(arg, isBinding, contextDescription);
            break;
          }

        case "ArrayExpression":
          node.type = "ArrayPattern";
          this.toAssignableList(node.elements, isBinding, contextDescription);
          break;

        case "AssignmentExpression":
          if (node.operator === "=") {
            node.type = "AssignmentPattern";
            delete node.operator;
          } else {
            this.raise(node.left.end, "Only '=' operator can be used for specifying default value.");
          }

          break;

        case "MemberExpression":
          if (!isBinding) break;

        default:
          {
            var message = "Invalid left-hand side" + (contextDescription ? " in " + contextDescription :
            /* istanbul ignore next */
            "expression");
            this.raise(node.start, message);
          }
      }
    }

    return node;
  };

  _proto.toAssignableObjectExpressionProp = function toAssignableObjectExpressionProp(prop, isBinding, isLast) {
    if (prop.type === "ObjectMethod") {
      var error = prop.kind === "get" || prop.kind === "set" ? "Object pattern can't contain getter or setter" : "Object pattern can't contain methods";
      this.raise(prop.key.start, error);
    } else if (prop.type === "SpreadElement" && !isLast) {
      this.raise(prop.start, "The rest element has to be the last element when destructuring");
    } else {
      this.toAssignable(prop, isBinding, "object destructuring pattern");
    }
  }; // Convert list of expression atoms to binding list.


  _proto.toAssignableList = function toAssignableList(exprList, isBinding, contextDescription) {
    var end = exprList.length;

    if (end) {
      var last = exprList[end - 1];

      if (last && last.type === "RestElement") {
        --end;
      } else if (last && last.type === "SpreadElement") {
        last.type = "RestElement";
        var arg = last.argument;
        this.toAssignable(arg, isBinding, contextDescription);

        if (["Identifier", "MemberExpression", "ArrayPattern", "ObjectPattern"].indexOf(arg.type) === -1) {
          this.unexpected(arg.start);
        }

        --end;
      }
    }

    for (var i = 0; i < end; i++) {
      var elt = exprList[i];

      if (elt && elt.type === "SpreadElement") {
        this.raise(elt.start, "The rest element has to be the last element when destructuring");
      }

      if (elt) this.toAssignable(elt, isBinding, contextDescription);
    }

    return exprList;
  }; // Convert list of expression atoms to a list of


  _proto.toReferencedList = function toReferencedList(exprList) {
    return exprList;
  }; // Parses spread element.


  _proto.parseSpread = function parseSpread(refShorthandDefaultPos) {
    var node = this.startNode();
    this.next();
    node.argument = this.parseMaybeAssign(false, refShorthandDefaultPos);
    return this.finishNode(node, "SpreadElement");
  };

  _proto.parseRest = function parseRest() {
    var node = this.startNode();
    this.next();
    node.argument = this.parseBindingAtom();
    return this.finishNode(node, "RestElement");
  };

  _proto.shouldAllowYieldIdentifier = function shouldAllowYieldIdentifier() {
    return this.match(types._yield) && !this.state.strict && !this.state.inGenerator;
  };

  _proto.parseBindingIdentifier = function parseBindingIdentifier() {
    return this.parseIdentifier(this.shouldAllowYieldIdentifier());
  }; // Parses lvalue (assignable) atom.


  _proto.parseBindingAtom = function parseBindingAtom() {
    switch (this.state.type) {
      case types._yield:
      case types.name:
        return this.parseBindingIdentifier();

      case types.bracketL:
        {
          var node = this.startNode();
          this.next();
          node.elements = this.parseBindingList(types.bracketR, true);
          return this.finishNode(node, "ArrayPattern");
        }

      case types.braceL:
        return this.parseObj(true);

      default:
        throw this.unexpected();
    }
  };

  _proto.parseBindingList = function parseBindingList(close, allowEmpty, allowModifiers) {
    var elts = [];
    var first = true;

    while (!this.eat(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(types.comma);
      }

      if (allowEmpty && this.match(types.comma)) {
        // $FlowFixMe This method returns `$ReadOnlyArray<?Pattern>` if `allowEmpty` is set.
        elts.push(null);
      } else if (this.eat(close)) {
        break;
      } else if (this.match(types.ellipsis)) {
        elts.push(this.parseAssignableListItemTypes(this.parseRest()));
        this.expect(close);
        break;
      } else {
        var decorators = [];

        if (this.match(types.at) && this.hasPlugin("decorators2")) {
          this.raise(this.state.start, "Stage 2 decorators cannot be used to decorate parameters");
        }

        while (this.match(types.at)) {
          decorators.push(this.parseDecorator());
        }

        elts.push(this.parseAssignableListItem(allowModifiers, decorators));
      }
    }

    return elts;
  };

  _proto.parseAssignableListItem = function parseAssignableListItem(allowModifiers, decorators) {
    var left = this.parseMaybeDefault();
    this.parseAssignableListItemTypes(left);
    var elt = this.parseMaybeDefault(left.start, left.loc.start, left);

    if (decorators.length) {
      left.decorators = decorators;
    }

    return elt;
  };

  _proto.parseAssignableListItemTypes = function parseAssignableListItemTypes(param) {
    return param;
  }; // Parses assignment pattern around given atom if possible.


  _proto.parseMaybeDefault = function parseMaybeDefault(startPos, startLoc, left) {
    startLoc = startLoc || this.state.startLoc;
    startPos = startPos || this.state.start;
    left = left || this.parseBindingAtom();
    if (!this.eat(types.eq)) return left;
    var node = this.startNodeAt(startPos, startLoc);
    node.left = left;
    node.right = this.parseMaybeAssign();
    return this.finishNode(node, "AssignmentPattern");
  }; // Verify that a node is an lval — something that can be assigned
  // to.


  _proto.checkLVal = function checkLVal(expr, isBinding, checkClashes, contextDescription) {
    switch (expr.type) {
      case "Identifier":
        this.checkReservedWord(expr.name, expr.start, false, true);

        if (checkClashes) {
          // we need to prefix this with an underscore for the cases where we have a key of
          // `__proto__`. there's a bug in old V8 where the following wouldn't work:
          //
          //   > var obj = Object.create(null);
          //   undefined
          //   > obj.__proto__
          //   null
          //   > obj.__proto__ = true;
          //   true
          //   > obj.__proto__
          //   null
          var _key = `_${expr.name}`;

          if (checkClashes[_key]) {
            this.raise(expr.start, "Argument name clash in strict mode");
          } else {
            checkClashes[_key] = true;
          }
        }

        break;

      case "MemberExpression":
        if (isBinding) this.raise(expr.start, "Binding member expression");
        break;

      case "ObjectPattern":
        for (var _i2 = 0, _expr$properties2 = expr.properties; _i2 < _expr$properties2.length; _i2++) {
          var prop = _expr$properties2[_i2];
          if (prop.type === "ObjectProperty") prop = prop.value;
          this.checkLVal(prop, isBinding, checkClashes, "object destructuring pattern");
        }

        break;

      case "ArrayPattern":
        for (var _i4 = 0, _expr$elements2 = expr.elements; _i4 < _expr$elements2.length; _i4++) {
          var elem = _expr$elements2[_i4];

          if (elem) {
            this.checkLVal(elem, isBinding, checkClashes, "array destructuring pattern");
          }
        }

        break;

      case "AssignmentPattern":
        this.checkLVal(expr.left, isBinding, checkClashes, "assignment pattern");
        break;

      case "RestElement":
        this.checkLVal(expr.argument, isBinding, checkClashes, "rest element");
        break;

      default:
        {
          var message = (isBinding ?
          /* istanbul ignore next */
          "Binding invalid" : "Invalid") + " left-hand side" + (contextDescription ? " in " + contextDescription :
          /* istanbul ignore next */
          "expression");
          this.raise(expr.start, message);
        }
    }
  };

  _proto.checkToRestConversion = function checkToRestConversion(node) {
    var validArgumentTypes = ["Identifier", "MemberExpression"];

    if (validArgumentTypes.indexOf(node.argument.type) !== -1) {
      return;
    }

    this.raise(node.argument.start, "Invalid rest operator's argument");
  };

  return LValParser;
}(NodeUtils);

/* eslint max-len: 0 */
// A recursive descent parser operates by defining functions for all
// syntactic elements, and recursively calling those, each function
// advancing the input stream and returning an AST node. Precedence
// of constructs (for example, the fact that `!x[1]` means `!(x[1])`
// instead of `(!x)[1]` is handled by the fact that the parser
// function that parses unary prefix operators is called first, and
// in turn calls the function that parses `[]` subscripts — that
// way, it'll receive the node for `x[1]` already parsed, and wraps
// *that* in the unary operator node.
//
// Acorn uses an [operator precedence parser][opp] to handle binary
// operator precedence, because it is much more compact than using
// the technique outlined above, which uses different, nesting
// functions to specify precedence, for all of the ten binary
// precedence levels that JavaScript defines.
//
// [opp]: http://en.wikipedia.org/wiki/Operator-precedence_parser
var ExpressionParser =
/*#__PURE__*/
function (_LValParser) {
  _inheritsLoose(ExpressionParser, _LValParser);

  function ExpressionParser() {
    return _LValParser.apply(this, arguments) || this;
  }

  var _proto = ExpressionParser.prototype;

  // Forward-declaration: defined in statement.js
  // Check if property name clashes with already added.
  // Object/class getters and setters are not allowed to clash —
  // either with each other or with an init property — and in
  // strict mode, init properties are also not allowed to be repeated.
  _proto.checkPropClash = function checkPropClash(prop, propHash) {
    if (prop.computed || prop.kind) return;
    var key = prop.key; // It is either an Identifier or a String/NumericLiteral

    var name = key.type === "Identifier" ? key.name : String(key.value);

    if (name === "__proto__") {
      if (propHash.proto) {
        this.raise(key.start, "Redefinition of __proto__ property");
      }

      propHash.proto = true;
    }
  }; // Convenience method to parse an Expression only


  _proto.getExpression = function getExpression() {
    this.nextToken();
    var expr = this.parseExpression();

    if (!this.match(types.eof)) {
      this.unexpected();
    }

    expr.comments = this.state.comments;
    return expr;
  }; // ### Expression parsing
  // These nest, from the most general expression type at the top to
  // 'atomic', nondivisible expression types at the bottom. Most of
  // the functions will simply let the function (s) below them parse,
  // and, *if* the syntactic construct they handle is present, wrap
  // the AST node that the inner parser gave them in another node.
  // Parse a full expression. The optional arguments are used to
  // forbid the `in` operator (in for loops initialization expressions)
  // and provide reference for storing '=' operator inside shorthand
  // property assignment in contexts where both object expression
  // and object pattern might appear (so it's possible to raise
  // delayed syntax error at correct position).


  _proto.parseExpression = function parseExpression(noIn, refShorthandDefaultPos) {
    var startPos = this.state.start;
    var startLoc = this.state.startLoc;
    var expr = this.parseMaybeAssign(noIn, refShorthandDefaultPos);

    if (this.match(types.comma)) {
      var _node = this.startNodeAt(startPos, startLoc);

      _node.expressions = [expr];

      while (this.eat(types.comma)) {
        _node.expressions.push(this.parseMaybeAssign(noIn, refShorthandDefaultPos));
      }

      this.toReferencedList(_node.expressions);
      return this.finishNode(_node, "SequenceExpression");
    }

    return expr;
  }; // Parse an assignment expression. This includes applications of
  // operators like `+=`.


  _proto.parseMaybeAssign = function parseMaybeAssign(noIn, refShorthandDefaultPos, afterLeftParse, refNeedsArrowPos) {
    var startPos = this.state.start;
    var startLoc = this.state.startLoc;

    if (this.match(types._yield) && this.state.inGenerator) {
      var _left = this.parseYield();

      if (afterLeftParse) {
        _left = afterLeftParse.call(this, _left, startPos, startLoc);
      }

      return _left;
    }

    var failOnShorthandAssign;

    if (refShorthandDefaultPos) {
      failOnShorthandAssign = false;
    } else {
      refShorthandDefaultPos = {
        start: 0
      };
      failOnShorthandAssign = true;
    }

    if (this.match(types.parenL) || this.match(types.name) || this.match(types._yield)) {
      this.state.potentialArrowAt = this.state.start;
    }

    var left = this.parseMaybeConditional(noIn, refShorthandDefaultPos, refNeedsArrowPos);

    if (afterLeftParse) {
      left = afterLeftParse.call(this, left, startPos, startLoc);
    }

    if (this.state.type.isAssign) {
      var _node2 = this.startNodeAt(startPos, startLoc);

      _node2.operator = this.state.value;
      _node2.left = this.match(types.eq) ? this.toAssignable(left, undefined, "assignment expression") : left;
      refShorthandDefaultPos.start = 0; // reset because shorthand default was used correctly

      this.checkLVal(left, undefined, undefined, "assignment expression");

      if (left.extra && left.extra.parenthesized) {
        var errorMsg;

        if (left.type === "ObjectPattern") {
          errorMsg = "`({a}) = 0` use `({a} = 0)`";
        } else if (left.type === "ArrayPattern") {
          errorMsg = "`([a]) = 0` use `([a] = 0)`";
        }

        if (errorMsg) {
          this.raise(left.start, `You're trying to assign to a parenthesized expression, eg. instead of ${errorMsg}`);
        }
      }

      this.next();
      _node2.right = this.parseMaybeAssign(noIn);
      return this.finishNode(_node2, "AssignmentExpression");
    } else if (failOnShorthandAssign && refShorthandDefaultPos.start) {
      this.unexpected(refShorthandDefaultPos.start);
    }

    return left;
  }; // Parse a ternary conditional (`?:`) operator.


  _proto.parseMaybeConditional = function parseMaybeConditional(noIn, refShorthandDefaultPos, refNeedsArrowPos) {
    var startPos = this.state.start;
    var startLoc = this.state.startLoc;
    var potentialArrowAt = this.state.potentialArrowAt;
    var expr = this.parseExprOps(noIn, refShorthandDefaultPos);

    if (expr.type === "ArrowFunctionExpression" && expr.start === potentialArrowAt) {
      return expr;
    }

    if (refShorthandDefaultPos && refShorthandDefaultPos.start) return expr;
    return this.parseConditional(expr, noIn, startPos, startLoc, refNeedsArrowPos);
  };

  _proto.parseConditional = function parseConditional(expr, noIn, startPos, startLoc, // FIXME: Disabling this for now since can't seem to get it to play nicely
  refNeedsArrowPos) {
    if (this.eat(types.question)) {
      var _node3 = this.startNodeAt(startPos, startLoc);

      _node3.test = expr;
      _node3.consequent = this.parseMaybeAssign();
      this.expect(types.colon);
      _node3.alternate = this.parseMaybeAssign(noIn);
      return this.finishNode(_node3, "ConditionalExpression");
    }

    return expr;
  }; // Start the precedence parser.


  _proto.parseExprOps = function parseExprOps(noIn, refShorthandDefaultPos) {
    var startPos = this.state.start;
    var startLoc = this.state.startLoc;
    var potentialArrowAt = this.state.potentialArrowAt;
    var expr = this.parseMaybeUnary(refShorthandDefaultPos);

    if (expr.type === "ArrowFunctionExpression" && expr.start === potentialArrowAt) {
      return expr;
    }

    if (refShorthandDefaultPos && refShorthandDefaultPos.start) {
      return expr;
    }

    return this.parseExprOp(expr, startPos, startLoc, -1, noIn);
  }; // Parse binary operators with the operator precedence parsing
  // algorithm. `left` is the left-hand side of the operator.
  // `minPrec` provides context that allows the function to stop and
  // defer further parser to one of its callers when it encounters an
  // operator that has a lower precedence than the set it is parsing.


  _proto.parseExprOp = function parseExprOp(left, leftStartPos, leftStartLoc, minPrec, noIn) {
    var prec = this.state.type.binop;

    if (prec != null && (!noIn || !this.match(types._in))) {
      if (prec > minPrec) {
        var _node4 = this.startNodeAt(leftStartPos, leftStartLoc);

        _node4.left = left;
        _node4.operator = this.state.value;

        if (_node4.operator === "**" && left.type === "UnaryExpression" && left.extra && !left.extra.parenthesizedArgument && !left.extra.parenthesized) {
          this.raise(left.argument.start, "Illegal expression. Wrap left hand side or entire exponentiation in parentheses.");
        }

        var op = this.state.type;
        this.next();
        var startPos = this.state.start;
        var startLoc = this.state.startLoc;

        if (_node4.operator === "|>") {
          this.expectPlugin("pipelineOperator"); // Support syntax such as 10 |> x => x + 1

          this.state.potentialArrowAt = startPos;
        }

        if (_node4.operator === "??") {
          this.expectPlugin("nullishCoalescingOperator");
        }

        _node4.right = this.parseExprOp(this.parseMaybeUnary(), startPos, startLoc, op.rightAssociative ? prec - 1 : prec, noIn);
        this.finishNode(_node4, op === types.logicalOR || op === types.logicalAND || op === types.nullishCoalescing ? "LogicalExpression" : "BinaryExpression");
        return this.parseExprOp(_node4, leftStartPos, leftStartLoc, minPrec, noIn);
      }
    }

    return left;
  }; // Parse unary operators, both prefix and postfix.


  _proto.parseMaybeUnary = function parseMaybeUnary(refShorthandDefaultPos) {
    if (this.state.type.prefix) {
      var _node5 = this.startNode();

      var update = this.match(types.incDec);
      _node5.operator = this.state.value;
      _node5.prefix = true;

      if (_node5.operator === "throw") {
        this.expectPlugin("throwExpressions");
      }

      this.next();
      var argType = this.state.type;
      _node5.argument = this.parseMaybeUnary();
      this.addExtra(_node5, "parenthesizedArgument", argType === types.parenL && (!_node5.argument.extra || !_node5.argument.extra.parenthesized));

      if (refShorthandDefaultPos && refShorthandDefaultPos.start) {
        this.unexpected(refShorthandDefaultPos.start);
      }

      if (update) {
        this.checkLVal(_node5.argument, undefined, undefined, "prefix operation");
      } else if (this.state.strict && _node5.operator === "delete") {
        var arg = _node5.argument;

        if (arg.type === "Identifier") {
          this.raise(_node5.start, "Deleting local variable in strict mode");
        } else if (arg.type === "MemberExpression" && arg.property.type === "PrivateName") {
          this.raise(_node5.start, "Deleting a private field is not allowed");
        }
      }

      return this.finishNode(_node5, update ? "UpdateExpression" : "UnaryExpression");
    }

    var startPos = this.state.start;
    var startLoc = this.state.startLoc;
    var expr = this.parseExprSubscripts(refShorthandDefaultPos);
    if (refShorthandDefaultPos && refShorthandDefaultPos.start) return expr;

    while (this.state.type.postfix && !this.canInsertSemicolon()) {
      var _node6 = this.startNodeAt(startPos, startLoc);

      _node6.operator = this.state.value;
      _node6.prefix = false;
      _node6.argument = expr;
      this.checkLVal(expr, undefined, undefined, "postfix operation");
      this.next();
      expr = this.finishNode(_node6, "UpdateExpression");
    }

    return expr;
  }; // Parse call, dot, and `[]`-subscript expressions.


  _proto.parseExprSubscripts = function parseExprSubscripts(refShorthandDefaultPos) {
    var startPos = this.state.start;
    var startLoc = this.state.startLoc;
    var potentialArrowAt = this.state.potentialArrowAt;
    var expr = this.parseExprAtom(refShorthandDefaultPos);

    if (expr.type === "ArrowFunctionExpression" && expr.start === potentialArrowAt) {
      return expr;
    }

    if (refShorthandDefaultPos && refShorthandDefaultPos.start) {
      return expr;
    }

    return this.parseSubscripts(expr, startPos, startLoc);
  };

  _proto.parseSubscripts = function parseSubscripts(base, startPos, startLoc, noCalls) {
    var state = {
      stop: false
    };

    do {
      base = this.parseSubscript(base, startPos, startLoc, noCalls, state);
    } while (!state.stop);

    return base;
  };
  /** @param state Set 'state.stop = true' to indicate that we should stop parsing subscripts. */


  _proto.parseSubscript = function parseSubscript(base, startPos, startLoc, noCalls, state) {
    if (!noCalls && this.eat(types.doubleColon)) {
      var _node7 = this.startNodeAt(startPos, startLoc);

      _node7.object = base;
      _node7.callee = this.parseNoCallExpr();
      state.stop = true;
      return this.parseSubscripts(this.finishNode(_node7, "BindExpression"), startPos, startLoc, noCalls);
    } else if (this.match(types.questionDot)) {
      this.expectPlugin("optionalChaining");

      if (noCalls && this.lookahead().type == types.parenL) {
        state.stop = true;
        return base;
      }

      this.next();

      var _node8 = this.startNodeAt(startPos, startLoc);

      if (this.eat(types.bracketL)) {
        _node8.object = base;
        _node8.property = this.parseExpression();
        _node8.computed = true;
        _node8.optional = true;
        this.expect(types.bracketR);
        return this.finishNode(_node8, "MemberExpression");
      } else if (this.eat(types.parenL)) {
        var possibleAsync = this.atPossibleAsync(base);
        _node8.callee = base;
        _node8.arguments = this.parseCallExpressionArguments(types.parenR, possibleAsync);
        _node8.optional = true;
        return this.finishNode(_node8, "CallExpression");
      } else {
        _node8.object = base;
        _node8.property = this.parseIdentifier(true);
        _node8.computed = false;
        _node8.optional = true;
        return this.finishNode(_node8, "MemberExpression");
      }
    } else if (this.eat(types.dot)) {
      var _node9 = this.startNodeAt(startPos, startLoc);

      _node9.object = base;
      _node9.property = this.parseMaybePrivateName();
      _node9.computed = false;
      return this.finishNode(_node9, "MemberExpression");
    } else if (this.eat(types.bracketL)) {
      var _node10 = this.startNodeAt(startPos, startLoc);

      _node10.object = base;
      _node10.property = this.parseExpression();
      _node10.computed = true;
      this.expect(types.bracketR);
      return this.finishNode(_node10, "MemberExpression");
    } else if (!noCalls && this.match(types.parenL)) {
      var _possibleAsync = this.atPossibleAsync(base);

      this.next();

      var _node11 = this.startNodeAt(startPos, startLoc);

      _node11.callee = base; // TODO: Clean up/merge this into `this.state` or a class like acorn's
      // `DestructuringErrors` alongside refShorthandDefaultPos and
      // refNeedsArrowPos.

      var refTrailingCommaPos = {
        start: -1
      };
      _node11.arguments = this.parseCallExpressionArguments(types.parenR, _possibleAsync, refTrailingCommaPos);
      this.finishCallExpression(_node11);

      if (_possibleAsync && this.shouldParseAsyncArrow()) {
        state.stop = true;

        if (refTrailingCommaPos.start > -1) {
          this.raise(refTrailingCommaPos.start, "A trailing comma is not permitted after the rest element");
        }

        return this.parseAsyncArrowFromCallExpression(this.startNodeAt(startPos, startLoc), _node11);
      } else {
        this.toReferencedList(_node11.arguments);
      }

      return _node11;
    } else if (this.match(types.backQuote)) {
      var _node12 = this.startNodeAt(startPos, startLoc);

      _node12.tag = base;
      _node12.quasi = this.parseTemplate(true);
      return this.finishNode(_node12, "TaggedTemplateExpression");
    } else {
      state.stop = true;
      return base;
    }
  };

  _proto.atPossibleAsync = function atPossibleAsync(base) {
    return this.state.potentialArrowAt === base.start && base.type === "Identifier" && base.name === "async" && !this.canInsertSemicolon();
  };

  _proto.finishCallExpression = function finishCallExpression(node) {
    if (node.callee.type === "Import") {
      if (node.arguments.length !== 1) {
        this.raise(node.start, "import() requires exactly one argument");
      }

      var importArg = node.arguments[0];

      if (importArg && importArg.type === "SpreadElement") {
        this.raise(importArg.start, "... is not allowed in import()");
      }
    }

    return this.finishNode(node, "CallExpression");
  };

  _proto.parseCallExpressionArguments = function parseCallExpressionArguments(close, possibleAsyncArrow, refTrailingCommaPos) {
    var elts = [];
    var innerParenStart;
    var first = true;

    while (!this.eat(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(types.comma);
        if (this.eat(close)) break;
      } // we need to make sure that if this is an async arrow functions, that we don't allow inner parens inside the params


      if (this.match(types.parenL) && !innerParenStart) {
        innerParenStart = this.state.start;
      }

      elts.push(this.parseExprListItem(false, possibleAsyncArrow ? {
        start: 0
      } : undefined, possibleAsyncArrow ? {
        start: 0
      } : undefined, possibleAsyncArrow ? refTrailingCommaPos : undefined));
    } // we found an async arrow function so let's not allow any inner parens


    if (possibleAsyncArrow && innerParenStart && this.shouldParseAsyncArrow()) {
      this.unexpected();
    }

    return elts;
  };

  _proto.shouldParseAsyncArrow = function shouldParseAsyncArrow() {
    return this.match(types.arrow);
  };

  _proto.parseAsyncArrowFromCallExpression = function parseAsyncArrowFromCallExpression(node, call) {
    var oldYield = this.state.yieldInPossibleArrowParameters;
    this.state.yieldInPossibleArrowParameters = null;
    this.expect(types.arrow);
    this.parseArrowExpression(node, call.arguments, true);
    this.state.yieldInPossibleArrowParameters = oldYield;
    return node;
  }; // Parse a no-call expression (like argument of `new` or `::` operators).


  _proto.parseNoCallExpr = function parseNoCallExpr() {
    var startPos = this.state.start;
    var startLoc = this.state.startLoc;
    return this.parseSubscripts(this.parseExprAtom(), startPos, startLoc, true);
  }; // Parse an atomic expression — either a single token that is an
  // expression, an expression started by a keyword like `function` or
  // `new`, or an expression wrapped in punctuation like `()`, `[]`,
  // or `{}`.


  _proto.parseExprAtom = function parseExprAtom(refShorthandDefaultPos) {
    var canBeArrow = this.state.potentialArrowAt === this.state.start;
    var node;

    switch (this.state.type) {
      case types._super:
        if (!this.state.inMethod && !this.state.inClassProperty && !this.options.allowSuperOutsideMethod) {
          this.raise(this.state.start, "super is only allowed in object methods and classes");
        }

        node = this.startNode();
        this.next();

        if (!this.match(types.parenL) && !this.match(types.bracketL) && !this.match(types.dot)) {
          this.unexpected();
        }

        if (this.match(types.parenL) && this.state.inMethod !== "constructor" && !this.options.allowSuperOutsideMethod) {
          this.raise(node.start, "super() is only valid inside a class constructor. Make sure the method name is spelled exactly as 'constructor'.");
        }

        return this.finishNode(node, "Super");

      case types._import:
        if (this.lookahead().type === types.dot) {
          return this.parseImportMetaProperty();
        }

        this.expectPlugin("dynamicImport");
        node = this.startNode();
        this.next();

        if (!this.match(types.parenL)) {
          this.unexpected(null, types.parenL);
        }

        return this.finishNode(node, "Import");

      case types._this:
        node = this.startNode();
        this.next();
        return this.finishNode(node, "ThisExpression");

      case types._yield:
        if (this.state.inGenerator) this.unexpected();

      case types.name:
        {
          node = this.startNode();
          var allowAwait = this.state.value === "await" && this.state.inAsync;
          var allowYield = this.shouldAllowYieldIdentifier();
          var id = this.parseIdentifier(allowAwait || allowYield);

          if (id.name === "await") {
            if (this.state.inAsync || this.inModule) {
              return this.parseAwait(node);
            }
          } else if (id.name === "async" && this.match(types._function) && !this.canInsertSemicolon()) {
            this.next();
            return this.parseFunction(node, false, false, true);
          } else if (canBeArrow && id.name === "async" && this.match(types.name)) {
            var oldYield = this.state.yieldInPossibleArrowParameters;
            this.state.yieldInPossibleArrowParameters = null;
            var params = [this.parseIdentifier()];
            this.expect(types.arrow); // let foo = bar => {};

            this.parseArrowExpression(node, params, true);
            this.state.yieldInPossibleArrowParameters = oldYield;
            return node;
          }

          if (canBeArrow && !this.canInsertSemicolon() && this.eat(types.arrow)) {
            var _oldYield = this.state.yieldInPossibleArrowParameters;
            this.state.yieldInPossibleArrowParameters = null;
            this.parseArrowExpression(node, [id]);
            this.state.yieldInPossibleArrowParameters = _oldYield;
            return node;
          }

          return id;
        }

      case types._do:
        {
          this.expectPlugin("doExpressions");

          var _node13 = this.startNode();

          this.next();
          var oldInFunction = this.state.inFunction;
          var oldLabels = this.state.labels;
          this.state.labels = [];
          this.state.inFunction = false;
          _node13.body = this.parseBlock(false);
          this.state.inFunction = oldInFunction;
          this.state.labels = oldLabels;
          return this.finishNode(_node13, "DoExpression");
        }

      case types.regexp:
        {
          var value = this.state.value;
          node = this.parseLiteral(value.value, "RegExpLiteral");
          node.pattern = value.pattern;
          node.flags = value.flags;
          return node;
        }

      case types.num:
        return this.parseLiteral(this.state.value, "NumericLiteral");

      case types.bigint:
        return this.parseLiteral(this.state.value, "BigIntLiteral");

      case types.string:
        return this.parseLiteral(this.state.value, "StringLiteral");

      case types._null:
        node = this.startNode();
        this.next();
        return this.finishNode(node, "NullLiteral");

      case types._true:
      case types._false:
        return this.parseBooleanLiteral();

      case types.parenL:
        return this.parseParenAndDistinguishExpression(canBeArrow);

      case types.bracketL:
        node = this.startNode();
        this.next();
        node.elements = this.parseExprList(types.bracketR, true, refShorthandDefaultPos);
        this.toReferencedList(node.elements);
        return this.finishNode(node, "ArrayExpression");

      case types.braceL:
        return this.parseObj(false, refShorthandDefaultPos);

      case types._function:
        return this.parseFunctionExpression();

      case types.at:
        this.parseDecorators();

      case types._class:
        node = this.startNode();
        this.takeDecorators(node);
        return this.parseClass(node, false);

      case types._new:
        return this.parseNew();

      case types.backQuote:
        return this.parseTemplate(false);

      case types.doubleColon:
        {
          node = this.startNode();
          this.next();
          node.object = null;
          var callee = node.callee = this.parseNoCallExpr();

          if (callee.type === "MemberExpression") {
            return this.finishNode(node, "BindExpression");
          } else {
            throw this.raise(callee.start, "Binding should be performed on object property.");
          }
        }

      default:
        throw this.unexpected();
    }
  };

  _proto.parseBooleanLiteral = function parseBooleanLiteral() {
    var node = this.startNode();
    node.value = this.match(types._true);
    this.next();
    return this.finishNode(node, "BooleanLiteral");
  };

  _proto.parseMaybePrivateName = function parseMaybePrivateName() {
    var isPrivate = this.match(types.hash);

    if (isPrivate) {
      this.expectOnePlugin(["classPrivateProperties", "classPrivateMethods"]);

      var _node14 = this.startNode();

      this.next();
      _node14.id = this.parseIdentifier(true);
      return this.finishNode(_node14, "PrivateName");
    } else {
      return this.parseIdentifier(true);
    }
  };

  _proto.parseFunctionExpression = function parseFunctionExpression() {
    var node = this.startNode();
    var meta = this.parseIdentifier(true);

    if (this.state.inGenerator && this.eat(types.dot)) {
      return this.parseMetaProperty(node, meta, "sent");
    }

    return this.parseFunction(node, false);
  };

  _proto.parseMetaProperty = function parseMetaProperty(node, meta, propertyName) {
    node.meta = meta;

    if (meta.name === "function" && propertyName === "sent") {
      if (this.isContextual(propertyName)) {
        this.expectPlugin("functionSent");
      } else if (!this.hasPlugin("functionSent")) {
        // They didn't actually say `function.sent`, just `function.`, so a simple error would be less confusing.
        this.unexpected();
      }
    }

    node.property = this.parseIdentifier(true);

    if (node.property.name !== propertyName) {
      this.raise(node.property.start, `The only valid meta property for ${meta.name} is ${meta.name}.${propertyName}`);
    }

    return this.finishNode(node, "MetaProperty");
  };

  _proto.parseImportMetaProperty = function parseImportMetaProperty() {
    var node = this.startNode();
    var id = this.parseIdentifier(true);
    this.expect(types.dot);

    if (id.name === "import") {
      if (this.isContextual("meta")) {
        this.expectPlugin("importMeta");
      } else if (!this.hasPlugin("importMeta")) {
        this.raise(id.start, `Dynamic imports require a parameter: import('a.js').then`);
      }
    }

    if (!this.inModule) {
      this.raise(id.start, `import.meta may appear only with 'sourceType: "module"'`);
    }

    return this.parseMetaProperty(node, id, "meta");
  };

  _proto.parseLiteral = function parseLiteral(value, type, startPos, startLoc) {
    startPos = startPos || this.state.start;
    startLoc = startLoc || this.state.startLoc;
    var node = this.startNodeAt(startPos, startLoc);
    this.addExtra(node, "rawValue", value);
    this.addExtra(node, "raw", this.input.slice(startPos, this.state.end));
    node.value = value;
    this.next();
    return this.finishNode(node, type);
  };

  _proto.parseParenExpression = function parseParenExpression() {
    this.expect(types.parenL);
    var val = this.parseExpression();
    this.expect(types.parenR);
    return val;
  };

  _proto.parseParenAndDistinguishExpression = function parseParenAndDistinguishExpression(canBeArrow) {
    var startPos = this.state.start;
    var startLoc = this.state.startLoc;
    var val;
    this.expect(types.parenL);
    var oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    var oldYield = this.state.yieldInPossibleArrowParameters;
    this.state.maybeInArrowParameters = true;
    this.state.yieldInPossibleArrowParameters = null;
    var innerStartPos = this.state.start;
    var innerStartLoc = this.state.startLoc;
    var exprList = [];
    var refShorthandDefaultPos = {
      start: 0
    };
    var refNeedsArrowPos = {
      start: 0
    };
    var first = true;
    var spreadStart;
    var optionalCommaStart;

    while (!this.match(types.parenR)) {
      if (first) {
        first = false;
      } else {
        this.expect(types.comma, refNeedsArrowPos.start || null);

        if (this.match(types.parenR)) {
          optionalCommaStart = this.state.start;
          break;
        }
      }

      if (this.match(types.ellipsis)) {
        var spreadNodeStartPos = this.state.start;
        var spreadNodeStartLoc = this.state.startLoc;
        spreadStart = this.state.start;
        exprList.push(this.parseParenItem(this.parseRest(), spreadNodeStartPos, spreadNodeStartLoc));

        if (this.match(types.comma) && this.lookahead().type === types.parenR) {
          this.raise(this.state.start, "A trailing comma is not permitted after the rest element");
        }

        break;
      } else {
        exprList.push(this.parseMaybeAssign(false, refShorthandDefaultPos, this.parseParenItem, refNeedsArrowPos));
      }
    }

    var innerEndPos = this.state.start;
    var innerEndLoc = this.state.startLoc;
    this.expect(types.parenR);
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    var arrowNode = this.startNodeAt(startPos, startLoc);

    if (canBeArrow && this.shouldParseArrow() && (arrowNode = this.parseArrow(arrowNode))) {
      for (var _i2 = 0; _i2 < exprList.length; _i2++) {
        var param = exprList[_i2];

        if (param.extra && param.extra.parenthesized) {
          this.unexpected(param.extra.parenStart);
        }
      }

      this.parseArrowExpression(arrowNode, exprList);
      this.state.yieldInPossibleArrowParameters = oldYield;
      return arrowNode;
    }

    this.state.yieldInPossibleArrowParameters = oldYield;

    if (!exprList.length) {
      this.unexpected(this.state.lastTokStart);
    }

    if (optionalCommaStart) this.unexpected(optionalCommaStart);
    if (spreadStart) this.unexpected(spreadStart);

    if (refShorthandDefaultPos.start) {
      this.unexpected(refShorthandDefaultPos.start);
    }

    if (refNeedsArrowPos.start) this.unexpected(refNeedsArrowPos.start);

    if (exprList.length > 1) {
      val = this.startNodeAt(innerStartPos, innerStartLoc);
      val.expressions = exprList;
      this.toReferencedList(val.expressions);
      this.finishNodeAt(val, "SequenceExpression", innerEndPos, innerEndLoc);
    } else {
      val = exprList[0];
    }

    this.addExtra(val, "parenthesized", true);
    this.addExtra(val, "parenStart", startPos);
    return val;
  };

  _proto.shouldParseArrow = function shouldParseArrow() {
    return !this.canInsertSemicolon();
  };

  _proto.parseArrow = function parseArrow(node) {
    if (this.eat(types.arrow)) {
      return node;
    }
  };

  _proto.parseParenItem = function parseParenItem(node, startPos, // eslint-disable-next-line no-unused-vars
  startLoc) {
    return node;
  }; // New's precedence is slightly tricky. It must allow its argument to
  // be a `[]` or dot subscript expression, but not a call — at least,
  // not without wrapping it in parentheses. Thus, it uses the noCalls
  // argument to parseSubscripts to prevent it from consuming the
  // argument list.


  _proto.parseNew = function parseNew() {
    var node = this.startNode();
    var meta = this.parseIdentifier(true);

    if (this.eat(types.dot)) {
      var metaProp = this.parseMetaProperty(node, meta, "target");

      if (!this.state.inFunction && !this.state.inClassProperty) {
        var error = "new.target can only be used in functions";

        if (this.hasPlugin("classProperties")) {
          error += " or class properties";
        }

        this.raise(metaProp.start, error);
      }

      return metaProp;
    }

    node.callee = this.parseNoCallExpr();
    if (this.eat(types.questionDot)) node.optional = true;
    this.parseNewArguments(node);
    return this.finishNode(node, "NewExpression");
  };

  _proto.parseNewArguments = function parseNewArguments(node) {
    if (this.eat(types.parenL)) {
      var args = this.parseExprList(types.parenR);
      this.toReferencedList(args); // $FlowFixMe (parseExprList should be all non-null in this case)

      node.arguments = args;
    } else {
      node.arguments = [];
    }
  }; // Parse template expression.


  _proto.parseTemplateElement = function parseTemplateElement(isTagged) {
    var elem = this.startNode();

    if (this.state.value === null) {
      if (!isTagged) {
        // TODO: fix this
        this.raise(this.state.invalidTemplateEscapePosition || 0, "Invalid escape sequence in template");
      } else {
        this.state.invalidTemplateEscapePosition = null;
      }
    }

    elem.value = {
      raw: this.input.slice(this.state.start, this.state.end).replace(/\r\n?/g, "\n"),
      cooked: this.state.value
    };
    this.next();
    elem.tail = this.match(types.backQuote);
    return this.finishNode(elem, "TemplateElement");
  };

  _proto.parseTemplate = function parseTemplate(isTagged) {
    var node = this.startNode();
    this.next();
    node.expressions = [];
    var curElt = this.parseTemplateElement(isTagged);
    node.quasis = [curElt];

    while (!curElt.tail) {
      this.expect(types.dollarBraceL);
      node.expressions.push(this.parseExpression());
      this.expect(types.braceR);
      node.quasis.push(curElt = this.parseTemplateElement(isTagged));
    }

    this.next();
    return this.finishNode(node, "TemplateLiteral");
  }; // Parse an object literal or binding pattern.


  _proto.parseObj = function parseObj(isPattern, refShorthandDefaultPos) {
    var decorators = [];
    var propHash = Object.create(null);
    var first = true;
    var node = this.startNode();
    node.properties = [];
    this.next();
    var firstRestLocation = null;

    while (!this.eat(types.braceR)) {
      if (first) {
        first = false;
      } else {
        this.expect(types.comma);
        if (this.eat(types.braceR)) break;
      }

      if (this.match(types.at)) {
        if (this.hasPlugin("decorators2")) {
          this.raise(this.state.start, "Stage 2 decorators disallow object literal property decorators");
        } else {
          // we needn't check if decorators (stage 0) plugin is enabled since it's checked by
          // the call to this.parseDecorator
          while (this.match(types.at)) {
            decorators.push(this.parseDecorator());
          }
        }
      }

      var prop = this.startNode(),
          isGenerator = false,
          _isAsync = false,
          startPos = void 0,
          startLoc = void 0;

      if (decorators.length) {
        prop.decorators = decorators;
        decorators = [];
      }

      if (this.match(types.ellipsis)) {
        this.expectPlugin("objectRestSpread");
        prop = this.parseSpread(isPattern ? {
          start: 0
        } : undefined);

        if (isPattern) {
          this.toAssignable(prop, true, "object pattern");
        }

        node.properties.push(prop);

        if (isPattern) {
          var position = this.state.start;

          if (firstRestLocation !== null) {
            this.unexpected(firstRestLocation, "Cannot have multiple rest elements when destructuring");
          } else if (this.eat(types.braceR)) {
            break;
          } else if (this.match(types.comma) && this.lookahead().type === types.braceR) {
            this.unexpected(position, "A trailing comma is not permitted after the rest element");
          } else {
            firstRestLocation = position;
            continue;
          }
        } else {
          continue;
        }
      }

      prop.method = false;

      if (isPattern || refShorthandDefaultPos) {
        startPos = this.state.start;
        startLoc = this.state.startLoc;
      }

      if (!isPattern) {
        isGenerator = this.eat(types.star);
      }

      if (!isPattern && this.isContextual("async")) {
        if (isGenerator) this.unexpected();
        var asyncId = this.parseIdentifier();

        if (this.match(types.colon) || this.match(types.parenL) || this.match(types.braceR) || this.match(types.eq) || this.match(types.comma)) {
          prop.key = asyncId;
          prop.computed = false;
        } else {
          _isAsync = true;

          if (this.match(types.star)) {
            this.expectPlugin("asyncGenerators");
            this.next();
            isGenerator = true;
          }

          this.parsePropertyName(prop);
        }
      } else {
        this.parsePropertyName(prop);
      }

      this.parseObjPropValue(prop, startPos, startLoc, isGenerator, _isAsync, isPattern, refShorthandDefaultPos);
      this.checkPropClash(prop, propHash);

      if (prop.shorthand) {
        this.addExtra(prop, "shorthand", true);
      }

      node.properties.push(prop);
    }

    if (firstRestLocation !== null) {
      this.unexpected(firstRestLocation, "The rest element has to be the last element when destructuring");
    }

    if (decorators.length) {
      this.raise(this.state.start, "You have trailing decorators with no property");
    }

    return this.finishNode(node, isPattern ? "ObjectPattern" : "ObjectExpression");
  };

  _proto.isGetterOrSetterMethod = function isGetterOrSetterMethod(prop, isPattern) {
    return !isPattern && !prop.computed && prop.key.type === "Identifier" && (prop.key.name === "get" || prop.key.name === "set") && (this.match(types.string) || // get "string"() {}
    this.match(types.num) || // get 1() {}
    this.match(types.bracketL) || // get ["string"]() {}
    this.match(types.name) || // get foo() {}
    !!this.state.type.keyword) // get debugger() {}
    ;
  }; // get methods aren't allowed to have any parameters
  // set methods must have exactly 1 parameter


  _proto.checkGetterSetterParamCount = function checkGetterSetterParamCount(method) {
    var paramCount = method.kind === "get" ? 0 : 1;

    if (method.params.length !== paramCount) {
      var start = method.start;

      if (method.kind === "get") {
        this.raise(start, "getter should have no params");
      } else {
        this.raise(start, "setter should have exactly one param");
      }
    }
  };

  _proto.parseObjectMethod = function parseObjectMethod(prop, isGenerator, isAsync, isPattern) {
    if (isAsync || isGenerator || this.match(types.parenL)) {
      if (isPattern) this.unexpected();
      prop.kind = "method";
      prop.method = true;
      return this.parseMethod(prop, isGenerator, isAsync,
      /* isConstructor */
      false, "ObjectMethod");
    }

    if (this.isGetterOrSetterMethod(prop, isPattern)) {
      if (isGenerator || isAsync) this.unexpected();
      prop.kind = prop.key.name;
      this.parsePropertyName(prop);
      this.parseMethod(prop,
      /* isGenerator */
      false,
      /* isAsync */
      false,
      /* isConstructor */
      false, "ObjectMethod");
      this.checkGetterSetterParamCount(prop);
      return prop;
    }
  };

  _proto.parseObjectProperty = function parseObjectProperty(prop, startPos, startLoc, isPattern, refShorthandDefaultPos) {
    prop.shorthand = false;

    if (this.eat(types.colon)) {
      prop.value = isPattern ? this.parseMaybeDefault(this.state.start, this.state.startLoc) : this.parseMaybeAssign(false, refShorthandDefaultPos);
      return this.finishNode(prop, "ObjectProperty");
    }

    if (!prop.computed && prop.key.type === "Identifier") {
      this.checkReservedWord(prop.key.name, prop.key.start, true, true);

      if (isPattern) {
        prop.value = this.parseMaybeDefault(startPos, startLoc, prop.key.__clone());
      } else if (this.match(types.eq) && refShorthandDefaultPos) {
        if (!refShorthandDefaultPos.start) {
          refShorthandDefaultPos.start = this.state.start;
        }

        prop.value = this.parseMaybeDefault(startPos, startLoc, prop.key.__clone());
      } else {
        prop.value = prop.key.__clone();
      }

      prop.shorthand = true;
      return this.finishNode(prop, "ObjectProperty");
    }
  };

  _proto.parseObjPropValue = function parseObjPropValue(prop, startPos, startLoc, isGenerator, isAsync, isPattern, refShorthandDefaultPos) {
    var node = this.parseObjectMethod(prop, isGenerator, isAsync, isPattern) || this.parseObjectProperty(prop, startPos, startLoc, isPattern, refShorthandDefaultPos);
    if (!node) this.unexpected(); // $FlowFixMe

    return node;
  };

  _proto.parsePropertyName = function parsePropertyName(prop) {
    if (this.eat(types.bracketL)) {
      prop.computed = true;
      prop.key = this.parseMaybeAssign();
      this.expect(types.bracketR);
    } else {
      var oldInPropertyName = this.state.inPropertyName;
      this.state.inPropertyName = true; // We check if it's valid for it to be a private name when we push it.

      prop.key = this.match(types.num) || this.match(types.string) ? this.parseExprAtom() : this.parseMaybePrivateName();

      if (prop.key.type !== "PrivateName") {
        // ClassPrivateProperty is never computed, so we don't assign in that case.
        prop.computed = false;
      }

      this.state.inPropertyName = oldInPropertyName;
    }

    return prop.key;
  }; // Initialize empty function node.


  _proto.initFunction = function initFunction(node, isAsync) {
    node.id = null;
    node.generator = false;
    node.async = !!isAsync;
  }; // Parse object or class method.


  _proto.parseMethod = function parseMethod(node, isGenerator, isAsync, isConstructor, type) {
    var oldInFunc = this.state.inFunction;
    var oldInMethod = this.state.inMethod;
    var oldInGenerator = this.state.inGenerator;
    this.state.inFunction = true;
    this.state.inMethod = node.kind || true;
    this.state.inGenerator = isGenerator;
    this.initFunction(node, isAsync);
    node.generator = !!isGenerator;
    var allowModifiers = isConstructor; // For TypeScript parameter properties

    this.parseFunctionParams(node, allowModifiers);
    this.parseFunctionBodyAndFinish(node, type);
    this.state.inFunction = oldInFunc;
    this.state.inMethod = oldInMethod;
    this.state.inGenerator = oldInGenerator;
    return node;
  }; // Parse arrow function expression.
  // If the parameters are provided, they will be converted to an
  // assignable list.


  _proto.parseArrowExpression = function parseArrowExpression(node, params, isAsync) {
    // if we got there, it's no more "yield in possible arrow parameters";
    // it's just "yield in arrow parameters"
    if (this.state.yieldInPossibleArrowParameters) {
      this.raise(this.state.yieldInPossibleArrowParameters.start, "yield is not allowed in the parameters of an arrow function" + " inside a generator");
    }

    var oldInFunc = this.state.inFunction;
    this.state.inFunction = true;
    this.initFunction(node, isAsync);
    if (params) this.setArrowFunctionParameters(node, params);
    var oldInGenerator = this.state.inGenerator;
    var oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    this.state.inGenerator = false;
    this.state.maybeInArrowParameters = false;
    this.parseFunctionBody(node, true);
    this.state.inGenerator = oldInGenerator;
    this.state.inFunction = oldInFunc;
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    return this.finishNode(node, "ArrowFunctionExpression");
  };

  _proto.setArrowFunctionParameters = function setArrowFunctionParameters(node, params) {
    node.params = this.toAssignableList(params, true, "arrow function parameters");
  };

  _proto.isStrictBody = function isStrictBody(node) {
    var isBlockStatement = node.body.type === "BlockStatement";

    if (isBlockStatement && node.body.directives.length) {
      for (var _i4 = 0, _node$body$directives2 = node.body.directives; _i4 < _node$body$directives2.length; _i4++) {
        var directive = _node$body$directives2[_i4];

        if (directive.value.value === "use strict") {
          return true;
        }
      }
    }

    return false;
  };

  _proto.parseFunctionBodyAndFinish = function parseFunctionBodyAndFinish(node, type, allowExpressionBody) {
    // $FlowIgnore (node is not bodiless if we get here)
    this.parseFunctionBody(node, allowExpressionBody);
    this.finishNode(node, type);
  }; // Parse function body and check parameters.


  _proto.parseFunctionBody = function parseFunctionBody(node, allowExpression) {
    var isExpression = allowExpression && !this.match(types.braceL);
    var oldInParameters = this.state.inParameters;
    var oldInAsync = this.state.inAsync;
    this.state.inParameters = false;
    this.state.inAsync = node.async;

    if (isExpression) {
      node.body = this.parseMaybeAssign();
    } else {
      // Start a new scope with regard to labels and the `inGenerator`
      // flag (restore them to their old value afterwards).
      var oldInGen = this.state.inGenerator;
      var oldInFunc = this.state.inFunction;
      var oldLabels = this.state.labels;
      this.state.inGenerator = node.generator;
      this.state.inFunction = true;
      this.state.labels = [];
      node.body = this.parseBlock(true);
      this.state.inFunction = oldInFunc;
      this.state.inGenerator = oldInGen;
      this.state.labels = oldLabels;
    }

    this.state.inAsync = oldInAsync;
    this.checkFunctionNameAndParams(node, allowExpression);
    this.state.inParameters = oldInParameters;
  };

  _proto.checkFunctionNameAndParams = function checkFunctionNameAndParams(node, isArrowFunction) {
    // If this is a strict mode function, verify that argument names
    // are not repeated, and it does not try to bind the words `eval`
    // or `arguments`.
    var isStrict = this.isStrictBody(node); // Also check for arrow functions

    var checkLVal = this.state.strict || isStrict || isArrowFunction;
    var oldStrict = this.state.strict;
    if (isStrict) this.state.strict = isStrict;

    if (node.id) {
      this.checkReservedWord(node.id, node.start, true, true);
    }

    if (checkLVal) {
      var nameHash = Object.create(null);

      if (node.id) {
        this.checkLVal(node.id, true, undefined, "function name");
      }

      for (var _i6 = 0, _node$params2 = node.params; _i6 < _node$params2.length; _i6++) {
        var param = _node$params2[_i6];

        if (isStrict && param.type !== "Identifier") {
          this.raise(param.start, "Non-simple parameter in strict mode");
        }

        this.checkLVal(param, true, nameHash, "function parameter list");
      }
    }

    this.state.strict = oldStrict;
  }; // Parses a comma-separated list of expressions, and returns them as
  // an array. `close` is the token type that ends the list, and
  // `allowEmpty` can be turned on to allow subsequent commas with
  // nothing in between them to be parsed as `null` (which is needed
  // for array literals).


  _proto.parseExprList = function parseExprList(close, allowEmpty, refShorthandDefaultPos) {
    var elts = [];
    var first = true;

    while (!this.eat(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(types.comma);
        if (this.eat(close)) break;
      }

      elts.push(this.parseExprListItem(allowEmpty, refShorthandDefaultPos));
    }

    return elts;
  };

  _proto.parseExprListItem = function parseExprListItem(allowEmpty, refShorthandDefaultPos, refNeedsArrowPos, refTrailingCommaPos) {
    var elt;

    if (allowEmpty && this.match(types.comma)) {
      elt = null;
    } else if (this.match(types.ellipsis)) {
      elt = this.parseSpread(refShorthandDefaultPos);

      if (refTrailingCommaPos && this.match(types.comma)) {
        refTrailingCommaPos.start = this.state.start;
      }
    } else {
      elt = this.parseMaybeAssign(false, refShorthandDefaultPos, this.parseParenItem, refNeedsArrowPos);
    }

    return elt;
  }; // Parse the next token as an identifier. If `liberal` is true (used
  // when parsing properties), it will also convert keywords into
  // identifiers.


  _proto.parseIdentifier = function parseIdentifier(liberal) {
    var node = this.startNode();
    var name = this.parseIdentifierName(node.start, liberal);
    node.name = name;
    node.loc.identifierName = name;
    return this.finishNode(node, "Identifier");
  };

  _proto.parseIdentifierName = function parseIdentifierName(pos, liberal) {
    if (!liberal) {
      this.checkReservedWord(this.state.value, this.state.start, !!this.state.type.keyword, false);
    }

    var name;

    if (this.match(types.name)) {
      name = this.state.value;
    } else if (this.state.type.keyword) {
      name = this.state.type.keyword;
    } else {
      throw this.unexpected();
    }

    if (!liberal && name === "await" && this.state.inAsync) {
      this.raise(pos, "invalid use of await inside of an async function");
    }

    this.next();
    return name;
  };

  _proto.checkReservedWord = function checkReservedWord(word, startLoc, checkKeywords, isBinding) {
    if (this.state.strict && (reservedWords.strict(word) || isBinding && reservedWords.strictBind(word))) {
      this.raise(startLoc, word + " is a reserved word in strict mode");
    }

    if (this.state.inGenerator && word === "yield") {
      this.raise(startLoc, "yield is a reserved word inside generator functions");
    }

    if (this.isReservedWord(word) || checkKeywords && this.isKeyword(word)) {
      this.raise(startLoc, word + " is a reserved word");
    }
  }; // Parses await expression inside async function.


  _proto.parseAwait = function parseAwait(node) {
    // istanbul ignore next: this condition is checked at the call site so won't be hit here
    if (!this.state.inAsync) {
      this.unexpected();
    }

    if (this.match(types.star)) {
      this.raise(node.start, "await* has been removed from the async functions proposal. Use Promise.all() instead.");
    }

    node.argument = this.parseMaybeUnary();
    return this.finishNode(node, "AwaitExpression");
  }; // Parses yield expression inside generator.


  _proto.parseYield = function parseYield() {
    var node = this.startNode();

    if (this.state.inParameters) {
      this.raise(node.start, "yield is not allowed in generator parameters");
    }

    if (this.state.maybeInArrowParameters && // We only set yieldInPossibleArrowParameters if we haven't already
    // found a possible invalid YieldExpression.
    !this.state.yieldInPossibleArrowParameters) {
      this.state.yieldInPossibleArrowParameters = node;
    }

    this.next();

    if (this.match(types.semi) || this.canInsertSemicolon() || !this.match(types.star) && !this.state.type.startsExpr) {
      node.delegate = false;
      node.argument = null;
    } else {
      node.delegate = this.eat(types.star);
      node.argument = this.parseMaybeAssign();
    }

    return this.finishNode(node, "YieldExpression");
  };

  return ExpressionParser;
}(LValParser);

/* eslint max-len: 0 */
var empty = [];
var loopLabel = {
  kind: "loop"
};
var switchLabel = {
  kind: "switch"
};

var StatementParser =
/*#__PURE__*/
function (_ExpressionParser) {
  _inheritsLoose(StatementParser, _ExpressionParser);

  function StatementParser() {
    return _ExpressionParser.apply(this, arguments) || this;
  }

  var _proto = StatementParser.prototype;

  // ### Statement parsing
  // Parse a program. Initializes the parser, reads any number of
  // statements, and wraps them in a Program node.  Optionally takes a
  // `program` argument.  If present, the statements will be appended
  // to its body instead of creating a new node.
  _proto.parseTopLevel = function parseTopLevel(file, program) {
    program.sourceType = this.options.sourceType;
    this.parseBlockBody(program, true, true, types.eof);
    file.program = this.finishNode(program, "Program");
    file.comments = this.state.comments;
    if (this.options.tokens) file.tokens = this.state.tokens;
    return this.finishNode(file, "File");
  }; // TODO


  _proto.stmtToDirective = function stmtToDirective(stmt) {
    var expr = stmt.expression;
    var directiveLiteral = this.startNodeAt(expr.start, expr.loc.start);
    var directive = this.startNodeAt(stmt.start, stmt.loc.start);
    var raw = this.input.slice(expr.start, expr.end);
    var val = directiveLiteral.value = raw.slice(1, -1); // remove quotes

    this.addExtra(directiveLiteral, "raw", raw);
    this.addExtra(directiveLiteral, "rawValue", val);
    directive.value = this.finishNodeAt(directiveLiteral, "DirectiveLiteral", expr.end, expr.loc.end);
    return this.finishNodeAt(directive, "Directive", stmt.end, stmt.loc.end);
  }; // Parse a single statement.
  //
  // If expecting a statement and finding a slash operator, parse a
  // regular expression literal. This is to handle cases like
  // `if (foo) /blah/.exec(foo)`, where looking at the previous token
  // does not help.


  _proto.parseStatement = function parseStatement(declaration, topLevel) {
    if (this.match(types.at)) {
      this.parseDecorators(true);
    }

    return this.parseStatementContent(declaration, topLevel);
  };

  _proto.parseStatementContent = function parseStatementContent(declaration, topLevel) {
    var starttype = this.state.type;
    var node = this.startNode(); // Most types of statements are recognized by the keyword they
    // start with. Many are trivial to parse, some require a bit of
    // complexity.

    switch (starttype) {
      case types._break:
      case types._continue:
        // $FlowFixMe
        return this.parseBreakContinueStatement(node, starttype.keyword);

      case types._debugger:
        return this.parseDebuggerStatement(node);

      case types._do:
        return this.parseDoStatement(node);

      case types._for:
        return this.parseForStatement(node);

      case types._function:
        if (this.lookahead().type === types.dot) break;
        if (!declaration) this.unexpected();
        return this.parseFunctionStatement(node);

      case types._class:
        if (!declaration) this.unexpected();
        return this.parseClass(node, true);

      case types._if:
        return this.parseIfStatement(node);

      case types._return:
        return this.parseReturnStatement(node);

      case types._switch:
        return this.parseSwitchStatement(node);

      case types._throw:
        return this.parseThrowStatement(node);

      case types._try:
        return this.parseTryStatement(node);

      case types._let:
      case types._const:
        if (!declaration) this.unexpected();
      // NOTE: falls through to _var

      case types._var:
        return this.parseVarStatement(node, starttype);

      case types._while:
        return this.parseWhileStatement(node);

      case types._with:
        return this.parseWithStatement(node);

      case types.braceL:
        return this.parseBlock();

      case types.semi:
        return this.parseEmptyStatement(node);

      case types._export:
      case types._import:
        {
          var nextToken = this.lookahead();

          if (nextToken.type === types.parenL || nextToken.type === types.dot) {
            break;
          }

          if (!this.options.allowImportExportEverywhere && !topLevel) {
            this.raise(this.state.start, "'import' and 'export' may only appear at the top level");
          }

          this.next();
          var result;

          if (starttype == types._import) {
            result = this.parseImport(node);
          } else {
            result = this.parseExport(node);
          }

          this.assertModuleNodeAllowed(node);
          return result;
        }

      case types.name:
        if (this.state.value === "async") {
          // peek ahead and see if next token is a function
          var state = this.state.clone();
          this.next();

          if (this.match(types._function) && !this.canInsertSemicolon()) {
            this.expect(types._function);
            return this.parseFunction(node, true, false, true);
          } else {
            this.state = state;
          }
        }

    } // If the statement does not start with a statement keyword or a
    // brace, it's an ExpressionStatement or LabeledStatement. We
    // simply start parsing an expression, and afterwards, if the
    // next token is a colon and the expression was a simple
    // Identifier node, we switch to interpreting it as a label.


    var maybeName = this.state.value;
    var expr = this.parseExpression();

    if (starttype === types.name && expr.type === "Identifier" && this.eat(types.colon)) {
      return this.parseLabeledStatement(node, maybeName, expr);
    } else {
      return this.parseExpressionStatement(node, expr);
    }
  };

  _proto.assertModuleNodeAllowed = function assertModuleNodeAllowed(node) {
    if (!this.options.allowImportExportEverywhere && !this.inModule) {
      this.raise(node.start, `'import' and 'export' may appear only with 'sourceType: "module"'`);
    }
  };

  _proto.takeDecorators = function takeDecorators(node) {
    var decorators = this.state.decoratorStack[this.state.decoratorStack.length - 1];

    if (decorators.length) {
      node.decorators = decorators;
      this.resetStartLocationFromNode(node, decorators[0]);
      this.state.decoratorStack[this.state.decoratorStack.length - 1] = [];
    }
  };

  _proto.parseDecorators = function parseDecorators(allowExport) {
    if (this.hasPlugin("decorators2")) {
      allowExport = false;
    }

    var currentContextDecorators = this.state.decoratorStack[this.state.decoratorStack.length - 1];

    while (this.match(types.at)) {
      var decorator = this.parseDecorator();
      currentContextDecorators.push(decorator);
    }

    if (this.match(types._export)) {
      if (allowExport) {
        return;
      } else {
        this.raise(this.state.start, "Using the export keyword between a decorator and a class is not allowed. Please use `export @dec class` instead");
      }
    }

    if (!this.match(types._class)) {
      this.raise(this.state.start, "Leading decorators must be attached to a class declaration");
    }
  };

  _proto.parseDecorator = function parseDecorator() {
    this.expectOnePlugin(["decorators", "decorators2"]);
    var node = this.startNode();
    this.next();

    if (this.hasPlugin("decorators2")) {
      var startPos = this.state.start;
      var startLoc = this.state.startLoc;
      var expr = this.parseIdentifier(false);

      while (this.eat(types.dot)) {
        var _node = this.startNodeAt(startPos, startLoc);

        _node.object = expr;
        _node.property = this.parseIdentifier(true);
        _node.computed = false;
        expr = this.finishNode(_node, "MemberExpression");
      }

      if (this.eat(types.parenL)) {
        var _node2 = this.startNodeAt(startPos, startLoc);

        _node2.callee = expr; // Every time a decorator class expression is evaluated, a new empty array is pushed onto the stack
        // So that the decorators of any nested class expressions will be dealt with separately

        this.state.decoratorStack.push([]);
        _node2.arguments = this.parseCallExpressionArguments(types.parenR, false);
        this.state.decoratorStack.pop();
        expr = this.finishNode(_node2, "CallExpression");
        this.toReferencedList(expr.arguments);
      }

      node.expression = expr;
    } else {
      node.expression = this.parseMaybeAssign();
    }

    return this.finishNode(node, "Decorator");
  };

  _proto.parseBreakContinueStatement = function parseBreakContinueStatement(node, keyword) {
    var isBreak = keyword === "break";
    this.next();

    if (this.isLineTerminator()) {
      node.label = null;
    } else if (!this.match(types.name)) {
      this.unexpected();
    } else {
      node.label = this.parseIdentifier();
      this.semicolon();
    } // Verify that there is an actual destination to break or
    // continue to.


    var i;

    for (i = 0; i < this.state.labels.length; ++i) {
      var lab = this.state.labels[i];

      if (node.label == null || lab.name === node.label.name) {
        if (lab.kind != null && (isBreak || lab.kind === "loop")) break;
        if (node.label && isBreak) break;
      }
    }

    if (i === this.state.labels.length) {
      this.raise(node.start, "Unsyntactic " + keyword);
    }

    return this.finishNode(node, isBreak ? "BreakStatement" : "ContinueStatement");
  };

  _proto.parseDebuggerStatement = function parseDebuggerStatement(node) {
    this.next();
    this.semicolon();
    return this.finishNode(node, "DebuggerStatement");
  };

  _proto.parseDoStatement = function parseDoStatement(node) {
    this.next();
    this.state.labels.push(loopLabel);
    node.body = this.parseStatement(false);
    this.state.labels.pop();
    this.expect(types._while);
    node.test = this.parseParenExpression();
    this.eat(types.semi);
    return this.finishNode(node, "DoWhileStatement");
  }; // Disambiguating between a `for` and a `for`/`in` or `for`/`of`
  // loop is non-trivial. Basically, we have to parse the init `var`
  // statement or expression, disallowing the `in` operator (see
  // the second parameter to `parseExpression`), and then check
  // whether the next token is `in` or `of`. When there is no init
  // part (semicolon immediately after the opening parenthesis), it
  // is a regular `for` loop.


  _proto.parseForStatement = function parseForStatement(node) {
    this.next();
    this.state.labels.push(loopLabel);
    var forAwait = false;

    if (this.state.inAsync && this.isContextual("await")) {
      this.expectPlugin("asyncGenerators");
      forAwait = true;
      this.next();
    }

    this.expect(types.parenL);

    if (this.match(types.semi)) {
      if (forAwait) {
        this.unexpected();
      }

      return this.parseFor(node, null);
    }

    if (this.match(types._var) || this.match(types._let) || this.match(types._const)) {
      var _init = this.startNode();

      var varKind = this.state.type;
      this.next();
      this.parseVar(_init, true, varKind);
      this.finishNode(_init, "VariableDeclaration");

      if (this.match(types._in) || this.isContextual("of")) {
        if (_init.declarations.length === 1 && !_init.declarations[0].init) {
          return this.parseForIn(node, _init, forAwait);
        }
      }

      if (forAwait) {
        this.unexpected();
      }

      return this.parseFor(node, _init);
    }

    var refShorthandDefaultPos = {
      start: 0
    };
    var init = this.parseExpression(true, refShorthandDefaultPos);

    if (this.match(types._in) || this.isContextual("of")) {
      var description = this.isContextual("of") ? "for-of statement" : "for-in statement";
      this.toAssignable(init, undefined, description);
      this.checkLVal(init, undefined, undefined, description);
      return this.parseForIn(node, init, forAwait);
    } else if (refShorthandDefaultPos.start) {
      this.unexpected(refShorthandDefaultPos.start);
    }

    if (forAwait) {
      this.unexpected();
    }

    return this.parseFor(node, init);
  };

  _proto.parseFunctionStatement = function parseFunctionStatement(node) {
    this.next();
    return this.parseFunction(node, true);
  };

  _proto.parseIfStatement = function parseIfStatement(node) {
    this.next();
    node.test = this.parseParenExpression();
    node.consequent = this.parseStatement(false);
    node.alternate = this.eat(types._else) ? this.parseStatement(false) : null;
    return this.finishNode(node, "IfStatement");
  };

  _proto.parseReturnStatement = function parseReturnStatement(node) {
    if (!this.state.inFunction && !this.options.allowReturnOutsideFunction) {
      this.raise(this.state.start, "'return' outside of function");
    }

    this.next(); // In `return` (and `break`/`continue`), the keywords with
    // optional arguments, we eagerly look for a semicolon or the
    // possibility to insert one.

    if (this.isLineTerminator()) {
      node.argument = null;
    } else {
      node.argument = this.parseExpression();
      this.semicolon();
    }

    return this.finishNode(node, "ReturnStatement");
  };

  _proto.parseSwitchStatement = function parseSwitchStatement(node) {
    this.next();
    node.discriminant = this.parseParenExpression();
    var cases = node.cases = [];
    this.expect(types.braceL);
    this.state.labels.push(switchLabel); // Statements under must be grouped (by label) in SwitchCase
    // nodes. `cur` is used to keep the node that we are currently
    // adding statements to.

    var cur;

    for (var sawDefault; !this.match(types.braceR);) {
      if (this.match(types._case) || this.match(types._default)) {
        var isCase = this.match(types._case);
        if (cur) this.finishNode(cur, "SwitchCase");
        cases.push(cur = this.startNode());
        cur.consequent = [];
        this.next();

        if (isCase) {
          cur.test = this.parseExpression();
        } else {
          if (sawDefault) {
            this.raise(this.state.lastTokStart, "Multiple default clauses");
          }

          sawDefault = true;
          cur.test = null;
        }

        this.expect(types.colon);
      } else {
        if (cur) {
          cur.consequent.push(this.parseStatement(true));
        } else {
          this.unexpected();
        }
      }
    }

    if (cur) this.finishNode(cur, "SwitchCase");
    this.next(); // Closing brace

    this.state.labels.pop();
    return this.finishNode(node, "SwitchStatement");
  };

  _proto.parseThrowStatement = function parseThrowStatement(node) {
    this.next();

    if (lineBreak.test(this.input.slice(this.state.lastTokEnd, this.state.start))) {
      this.raise(this.state.lastTokEnd, "Illegal newline after throw");
    }

    node.argument = this.parseExpression();
    this.semicolon();
    return this.finishNode(node, "ThrowStatement");
  };

  _proto.parseTryStatement = function parseTryStatement(node) {
    this.next();
    node.block = this.parseBlock();
    node.handler = null;

    if (this.match(types._catch)) {
      var clause = this.startNode();
      this.next();

      if (this.match(types.parenL)) {
        this.expect(types.parenL);
        clause.param = this.parseBindingAtom();
        var clashes = Object.create(null);
        this.checkLVal(clause.param, true, clashes, "catch clause");
        this.expect(types.parenR);
      } else {
        this.expectPlugin("optionalCatchBinding");
        clause.param = null;
      }

      clause.body = this.parseBlock();
      node.handler = this.finishNode(clause, "CatchClause");
    }

    node.guardedHandlers = empty;
    node.finalizer = this.eat(types._finally) ? this.parseBlock() : null;

    if (!node.handler && !node.finalizer) {
      this.raise(node.start, "Missing catch or finally clause");
    }

    return this.finishNode(node, "TryStatement");
  };

  _proto.parseVarStatement = function parseVarStatement(node, kind) {
    this.next();
    this.parseVar(node, false, kind);
    this.semicolon();
    return this.finishNode(node, "VariableDeclaration");
  };

  _proto.parseWhileStatement = function parseWhileStatement(node) {
    this.next();
    node.test = this.parseParenExpression();
    this.state.labels.push(loopLabel);
    node.body = this.parseStatement(false);
    this.state.labels.pop();
    return this.finishNode(node, "WhileStatement");
  };

  _proto.parseWithStatement = function parseWithStatement(node) {
    if (this.state.strict) {
      this.raise(this.state.start, "'with' in strict mode");
    }

    this.next();
    node.object = this.parseParenExpression();
    node.body = this.parseStatement(false);
    return this.finishNode(node, "WithStatement");
  };

  _proto.parseEmptyStatement = function parseEmptyStatement(node) {
    this.next();
    return this.finishNode(node, "EmptyStatement");
  };

  _proto.parseLabeledStatement = function parseLabeledStatement(node, maybeName, expr) {
    for (var _i2 = 0, _state$labels2 = this.state.labels; _i2 < _state$labels2.length; _i2++) {
      var label = _state$labels2[_i2];

      if (label.name === maybeName) {
        this.raise(expr.start, `Label '${maybeName}' is already declared`);
      }
    }

    var kind = this.state.type.isLoop ? "loop" : this.match(types._switch) ? "switch" : null;

    for (var i = this.state.labels.length - 1; i >= 0; i--) {
      var _label = this.state.labels[i];

      if (_label.statementStart === node.start) {
        _label.statementStart = this.state.start;
        _label.kind = kind;
      } else {
        break;
      }
    }

    this.state.labels.push({
      name: maybeName,
      kind: kind,
      statementStart: this.state.start
    });
    node.body = this.parseStatement(true);

    if (node.body.type == "ClassDeclaration" || node.body.type == "VariableDeclaration" && node.body.kind !== "var" || node.body.type == "FunctionDeclaration" && (this.state.strict || node.body.generator || node.body.async)) {
      this.raise(node.body.start, "Invalid labeled declaration");
    }

    this.state.labels.pop();
    node.label = expr;
    return this.finishNode(node, "LabeledStatement");
  };

  _proto.parseExpressionStatement = function parseExpressionStatement(node, expr) {
    node.expression = expr;
    this.semicolon();
    return this.finishNode(node, "ExpressionStatement");
  }; // Parse a semicolon-enclosed block of statements, handling `"use
  // strict"` declarations when `allowStrict` is true (used for
  // function bodies).


  _proto.parseBlock = function parseBlock(allowDirectives) {
    var node = this.startNode();
    this.expect(types.braceL);
    this.parseBlockBody(node, allowDirectives, false, types.braceR);
    return this.finishNode(node, "BlockStatement");
  };

  _proto.isValidDirective = function isValidDirective(stmt) {
    return stmt.type === "ExpressionStatement" && stmt.expression.type === "StringLiteral" && !stmt.expression.extra.parenthesized;
  };

  _proto.parseBlockBody = function parseBlockBody(node, allowDirectives, topLevel, end) {
    var body = node.body = [];
    var directives = node.directives = [];
    this.parseBlockOrModuleBlockBody(body, allowDirectives ? directives : undefined, topLevel, end);
  }; // Undefined directives means that directives are not allowed.


  _proto.parseBlockOrModuleBlockBody = function parseBlockOrModuleBlockBody(body, directives, topLevel, end) {
    var parsedNonDirective = false;
    var oldStrict;
    var octalPosition;

    while (!this.eat(end)) {
      if (!parsedNonDirective && this.state.containsOctal && !octalPosition) {
        octalPosition = this.state.octalPosition;
      }

      var stmt = this.parseStatement(true, topLevel);

      if (directives && !parsedNonDirective && this.isValidDirective(stmt)) {
        var directive = this.stmtToDirective(stmt);
        directives.push(directive);

        if (oldStrict === undefined && directive.value.value === "use strict") {
          oldStrict = this.state.strict;
          this.setStrict(true);

          if (octalPosition) {
            this.raise(octalPosition, "Octal literal in strict mode");
          }
        }

        continue;
      }

      parsedNonDirective = true;
      body.push(stmt);
    }

    if (oldStrict === false) {
      this.setStrict(false);
    }
  }; // Parse a regular `for` loop. The disambiguation code in
  // `parseStatement` will already have parsed the init statement or
  // expression.


  _proto.parseFor = function parseFor(node, init) {
    node.init = init;
    this.expect(types.semi);
    node.test = this.match(types.semi) ? null : this.parseExpression();
    this.expect(types.semi);
    node.update = this.match(types.parenR) ? null : this.parseExpression();
    this.expect(types.parenR);
    node.body = this.parseStatement(false);
    this.state.labels.pop();
    return this.finishNode(node, "ForStatement");
  }; // Parse a `for`/`in` and `for`/`of` loop, which are almost
  // same from parser's perspective.


  _proto.parseForIn = function parseForIn(node, init, forAwait) {
    var type = this.match(types._in) ? "ForInStatement" : "ForOfStatement";

    if (forAwait) {
      this.eatContextual("of");
    } else {
      this.next();
    }

    if (type === "ForOfStatement") {
      node.await = !!forAwait;
    }

    node.left = init;
    node.right = this.parseExpression();
    this.expect(types.parenR);
    node.body = this.parseStatement(false);
    this.state.labels.pop();
    return this.finishNode(node, type);
  }; // Parse a list of variable declarations.


  _proto.parseVar = function parseVar(node, isFor, kind) {
    var declarations = node.declarations = []; // $FlowFixMe

    node.kind = kind.keyword;

    for (;;) {
      var decl = this.startNode();
      this.parseVarHead(decl);

      if (this.eat(types.eq)) {
        decl.init = this.parseMaybeAssign(isFor);
      } else {
        if (kind === types._const && !(this.match(types._in) || this.isContextual("of"))) {
          // `const` with no initializer is allowed in TypeScript. It could be a declaration `const x: number;`.
          if (!this.hasPlugin("typescript")) {
            this.unexpected();
          }
        } else if (decl.id.type !== "Identifier" && !(isFor && (this.match(types._in) || this.isContextual("of")))) {
          this.raise(this.state.lastTokEnd, "Complex binding patterns require an initialization value");
        }

        decl.init = null;
      }

      declarations.push(this.finishNode(decl, "VariableDeclarator"));
      if (!this.eat(types.comma)) break;
    }

    return node;
  };

  _proto.parseVarHead = function parseVarHead(decl) {
    decl.id = this.parseBindingAtom();
    this.checkLVal(decl.id, true, undefined, "variable declaration");
  }; // Parse a function declaration or literal (depending on the
  // `isStatement` parameter).


  _proto.parseFunction = function parseFunction(node, isStatement, allowExpressionBody, isAsync, optionalId) {
    var oldInFunc = this.state.inFunction;
    var oldInMethod = this.state.inMethod;
    var oldInGenerator = this.state.inGenerator;
    this.state.inFunction = true;
    this.state.inMethod = false;
    this.initFunction(node, isAsync);

    if (this.match(types.star)) {
      if (node.async) {
        this.expectPlugin("asyncGenerators");
      }

      node.generator = true;
      this.next();
    }

    if (isStatement && !optionalId && !this.match(types.name) && !this.match(types._yield)) {
      this.unexpected();
    } // When parsing function expression, the binding identifier is parsed
    // according to the rules inside the function.
    // e.g. (function* yield() {}) is invalid because "yield" is disallowed in
    // generators.
    // This isn't the case with function declarations: function* yield() {} is
    // valid because yield is parsed as if it was outside the generator.
    // Therefore, this.state.inGenerator is set before or after parsing the
    // function id according to the "isStatement" parameter.


    if (!isStatement) this.state.inGenerator = node.generator;

    if (this.match(types.name) || this.match(types._yield)) {
      node.id = this.parseBindingIdentifier();
    }

    if (isStatement) this.state.inGenerator = node.generator;
    this.parseFunctionParams(node);
    this.parseFunctionBodyAndFinish(node, isStatement ? "FunctionDeclaration" : "FunctionExpression", allowExpressionBody);
    this.state.inFunction = oldInFunc;
    this.state.inMethod = oldInMethod;
    this.state.inGenerator = oldInGenerator;
    return node;
  };

  _proto.parseFunctionParams = function parseFunctionParams(node, allowModifiers) {
    var oldInParameters = this.state.inParameters;
    this.state.inParameters = true;
    this.expect(types.parenL);
    node.params = this.parseBindingList(types.parenR,
    /* allowEmpty */
    false, allowModifiers);
    this.state.inParameters = oldInParameters;
  }; // Parse a class declaration or literal (depending on the
  // `isStatement` parameter).


  _proto.parseClass = function parseClass(node, isStatement, optionalId) {
    this.next();
    this.takeDecorators(node);
    this.parseClassId(node, isStatement, optionalId);
    this.parseClassSuper(node);
    this.parseClassBody(node);
    return this.finishNode(node, isStatement ? "ClassDeclaration" : "ClassExpression");
  };

  _proto.isClassProperty = function isClassProperty() {
    return this.match(types.eq) || this.match(types.semi) || this.match(types.braceR);
  };

  _proto.isClassMethod = function isClassMethod() {
    return this.match(types.parenL);
  };

  _proto.isNonstaticConstructor = function isNonstaticConstructor(method) {
    return !method.computed && !method.static && (method.key.name === "constructor" || // Identifier
    method.key.value === "constructor") // String literal
    ;
  };

  _proto.parseClassBody = function parseClassBody(node) {
    // class bodies are implicitly strict
    var oldStrict = this.state.strict;
    this.state.strict = true;
    this.state.classLevel++;
    var state = {
      hadConstructor: false
    };
    var decorators = [];
    var classBody = this.startNode();
    classBody.body = [];
    this.expect(types.braceL);

    while (!this.eat(types.braceR)) {
      if (this.eat(types.semi)) {
        if (decorators.length > 0) {
          this.raise(this.state.lastTokEnd, "Decorators must not be followed by a semicolon");
        }

        continue;
      }

      if (this.match(types.at)) {
        decorators.push(this.parseDecorator());
        continue;
      }

      var member = this.startNode(); // steal the decorators if there are any

      if (decorators.length) {
        member.decorators = decorators;
        this.resetStartLocationFromNode(member, decorators[0]);
        decorators = [];
      }

      this.parseClassMember(classBody, member, state);

      if (this.hasPlugin("decorators2") && ["method", "get", "set"].indexOf(member.kind) === -1 && member.decorators && member.decorators.length > 0) {
        this.raise(member.start, "Stage 2 decorators may only be used with a class or a class method");
      }
    }

    if (decorators.length) {
      this.raise(this.state.start, "You have trailing decorators with no method");
    }

    node.body = this.finishNode(classBody, "ClassBody");
    this.state.classLevel--;
    this.state.strict = oldStrict;
  };

  _proto.parseClassMember = function parseClassMember(classBody, member, state) {
    var isStatic = false;

    if (this.match(types.name) && this.state.value === "static") {
      var key = this.parseIdentifier(true); // eats 'static'

      if (this.isClassMethod()) {
        var method = member; // a method named 'static'

        method.kind = "method";
        method.computed = false;
        method.key = key;
        method.static = false;
        this.pushClassMethod(classBody, method, false, false,
        /* isConstructor */
        false);
        return;
      } else if (this.isClassProperty()) {
        var prop = member; // a property named 'static'

        prop.computed = false;
        prop.key = key;
        prop.static = false;
        classBody.body.push(this.parseClassProperty(prop));
        return;
      } // otherwise something static


      isStatic = true;
    }

    this.parseClassMemberWithIsStatic(classBody, member, state, isStatic);
  };

  _proto.parseClassMemberWithIsStatic = function parseClassMemberWithIsStatic(classBody, member, state, isStatic) {
    var publicMethod = member;
    var privateMethod = member;
    var publicProp = member;
    var privateProp = member;
    var method = publicMethod;
    var publicMember = publicMethod;
    member.static = isStatic;

    if (this.eat(types.star)) {
      // a generator
      method.kind = "method";
      this.parseClassPropertyName(method);

      if (method.key.type === "PrivateName") {
        // Private generator method
        this.pushClassPrivateMethod(classBody, privateMethod, true, false);
        return;
      }

      if (this.isNonstaticConstructor(publicMethod)) {
        this.raise(publicMethod.key.start, "Constructor can't be a generator");
      }

      this.pushClassMethod(classBody, publicMethod, true, false,
      /* isConstructor */
      false);
      return;
    }

    var key = this.parseClassPropertyName(member);
    var isPrivate = key.type === "PrivateName"; // Check the key is not a computed expression or string literal.

    var isSimple = key.type === "Identifier";
    this.parsePostMemberNameModifiers(publicMember);

    if (this.isClassMethod()) {
      method.kind = "method";

      if (isPrivate) {
        this.pushClassPrivateMethod(classBody, privateMethod, false, false);
        return;
      } // a normal method


      var isConstructor = this.isNonstaticConstructor(publicMethod);

      if (isConstructor) {
        publicMethod.kind = "constructor";

        if (publicMethod.decorators) {
          this.raise(publicMethod.start, "You can't attach decorators to a class constructor");
        } // TypeScript allows multiple overloaded constructor declarations.


        if (state.hadConstructor && !this.hasPlugin("typescript")) {
          this.raise(key.start, "Duplicate constructor in the same class");
        }

        state.hadConstructor = true;
      }

      this.pushClassMethod(classBody, publicMethod, false, false, isConstructor);
    } else if (this.isClassProperty()) {
      if (isPrivate) {
        this.pushClassPrivateProperty(classBody, privateProp);
      } else {
        this.pushClassProperty(classBody, publicProp);
      }
    } else if (isSimple && key.name === "async" && !this.isLineTerminator()) {
      // an async method
      var isGenerator = this.match(types.star);

      if (isGenerator) {
        this.expectPlugin("asyncGenerators");
        this.next();
      }

      method.kind = "method"; // The so-called parsed name would have been "async": get the real name.

      this.parseClassPropertyName(method);

      if (method.key.type === "PrivateName") {
        // private async method
        this.pushClassPrivateMethod(classBody, privateMethod, isGenerator, true);
      } else {
        if (this.isNonstaticConstructor(publicMethod)) {
          this.raise(publicMethod.key.start, "Constructor can't be an async function");
        }

        this.pushClassMethod(classBody, publicMethod, isGenerator, true,
        /* isConstructor */
        false);
      }
    } else if (isSimple && (key.name === "get" || key.name === "set") && !(this.isLineTerminator() && this.match(types.star))) {
      // `get\n*` is an uninitialized property named 'get' followed by a generator.
      // a getter or setter
      method.kind = key.name; // The so-called parsed name would have been "get/set": get the real name.

      this.parseClassPropertyName(publicMethod);

      if (method.key.type === "PrivateName") {
        // private getter/setter
        this.pushClassPrivateMethod(classBody, privateMethod, false, false);
      } else {
        if (this.isNonstaticConstructor(publicMethod)) {
          this.raise(publicMethod.key.start, "Constructor can't have get/set modifier");
        }

        this.pushClassMethod(classBody, publicMethod, false, false,
        /* isConstructor */
        false);
      }

      this.checkGetterSetterParamCount(publicMethod);
    } else if (this.isLineTerminator()) {
      // an uninitialized class property (due to ASI, since we don't otherwise recognize the next token)
      if (isPrivate) {
        this.pushClassPrivateProperty(classBody, privateProp);
      } else {
        this.pushClassProperty(classBody, publicProp);
      }
    } else {
      this.unexpected();
    }
  };

  _proto.parseClassPropertyName = function parseClassPropertyName(member) {
    var key = this.parsePropertyName(member);

    if (!member.computed && member.static && (key.name === "prototype" || key.value === "prototype")) {
      this.raise(key.start, "Classes may not have static property named prototype");
    }

    if (key.type === "PrivateName" && key.id.name === "constructor") {
      this.raise(key.start, "Classes may not have a private field named '#constructor'");
    }

    return key;
  };

  _proto.pushClassProperty = function pushClassProperty(classBody, prop) {
    // This only affects properties, not methods.
    if (this.isNonstaticConstructor(prop)) {
      this.raise(prop.key.start, "Classes may not have a non-static field named 'constructor'");
    }

    classBody.body.push(this.parseClassProperty(prop));
  };

  _proto.pushClassPrivateProperty = function pushClassPrivateProperty(classBody, prop) {
    this.expectPlugin("classPrivateProperties", prop.key.start);
    classBody.body.push(this.parseClassPrivateProperty(prop));
  };

  _proto.pushClassMethod = function pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor) {
    classBody.body.push(this.parseMethod(method, isGenerator, isAsync, isConstructor, "ClassMethod"));
  };

  _proto.pushClassPrivateMethod = function pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
    this.expectPlugin("classPrivateMethods", method.key.start);
    classBody.body.push(this.parseMethod(method, isGenerator, isAsync,
    /* isConstructor */
    false, "ClassPrivateMethod"));
  }; // Overridden in typescript.js


  _proto.parsePostMemberNameModifiers = function parsePostMemberNameModifiers( // eslint-disable-next-line no-unused-vars
  methodOrProp) {}; // Overridden in typescript.js


  _proto.parseAccessModifier = function parseAccessModifier() {
    return undefined;
  };

  _proto.parseClassPrivateProperty = function parseClassPrivateProperty(node) {
    this.state.inClassProperty = true;
    node.value = this.eat(types.eq) ? this.parseMaybeAssign() : null;
    this.semicolon();
    this.state.inClassProperty = false;
    return this.finishNode(node, "ClassPrivateProperty");
  };

  _proto.parseClassProperty = function parseClassProperty(node) {
    if (!node.typeAnnotation) {
      this.expectPlugin("classProperties");
    }

    this.state.inClassProperty = true;

    if (this.match(types.eq)) {
      this.expectPlugin("classProperties");
      this.next();
      node.value = this.parseMaybeAssign();
    } else {
      node.value = null;
    }

    this.semicolon();
    this.state.inClassProperty = false;
    return this.finishNode(node, "ClassProperty");
  };

  _proto.parseClassId = function parseClassId(node, isStatement, optionalId) {
    if (this.match(types.name)) {
      node.id = this.parseIdentifier();
    } else {
      if (optionalId || !isStatement) {
        node.id = null;
      } else {
        this.unexpected(null, "A class name is required");
      }
    }
  };

  _proto.parseClassSuper = function parseClassSuper(node) {
    node.superClass = this.eat(types._extends) ? this.parseExprSubscripts() : null;
  }; // Parses module export declaration.
  // TODO: better type. Node is an N.AnyExport.


  _proto.parseExport = function parseExport(node) {
    // export * from '...'
    if (this.shouldParseExportStar()) {
      this.parseExportStar(node);
      if (node.type === "ExportAllDeclaration") return node;
    } else if (this.isExportDefaultSpecifier()) {
      this.expectPlugin("exportDefaultFrom");
      var specifier = this.startNode();
      specifier.exported = this.parseIdentifier(true);
      var specifiers = [this.finishNode(specifier, "ExportDefaultSpecifier")];
      node.specifiers = specifiers;

      if (this.match(types.comma) && this.lookahead().type === types.star) {
        this.expect(types.comma);

        var _specifier = this.startNode();

        this.expect(types.star);
        this.expectContextual("as");
        _specifier.exported = this.parseIdentifier();
        specifiers.push(this.finishNode(_specifier, "ExportNamespaceSpecifier"));
      } else {
        this.parseExportSpecifiersMaybe(node);
      }

      this.parseExportFrom(node, true);
    } else if (this.eat(types._default)) {
      // export default ...
      node.declaration = this.parseExportDefaultExpression();
      this.checkExport(node, true, true);
      return this.finishNode(node, "ExportDefaultDeclaration");
    } else if (this.shouldParseExportDeclaration()) {
      if (this.isContextual("async")) {
        var next = this.lookahead(); // export async;

        if (next.type !== types._function) {
          this.unexpected(next.start, `Unexpected token, expected "function"`);
        }
      }

      node.specifiers = [];
      node.source = null;
      node.declaration = this.parseExportDeclaration(node);
    } else {
      // export { x, y as z } [from '...']
      node.declaration = null;
      node.specifiers = this.parseExportSpecifiers();
      this.parseExportFrom(node);
    }

    this.checkExport(node, true);
    return this.finishNode(node, "ExportNamedDeclaration");
  };

  _proto.parseExportDefaultExpression = function parseExportDefaultExpression() {
    var expr = this.startNode();

    if (this.eat(types._function)) {
      return this.parseFunction(expr, true, false, false, true);
    } else if (this.isContextual("async") && this.lookahead().type === types._function) {
      // async function declaration
      this.eatContextual("async");
      this.eat(types._function);
      return this.parseFunction(expr, true, false, true, true);
    } else if (this.match(types._class)) {
      return this.parseClass(expr, true, true);
    } else if (this.match(types.at)) {
      this.parseDecorators(false);
      return this.parseClass(expr, true, true);
    } else {
      var res = this.parseMaybeAssign();
      this.semicolon();
      return res;
    }
  }; // eslint-disable-next-line no-unused-vars


  _proto.parseExportDeclaration = function parseExportDeclaration(node) {
    return this.parseStatement(true);
  };

  _proto.isExportDefaultSpecifier = function isExportDefaultSpecifier() {
    if (this.match(types.name)) {
      return this.state.value !== "async";
    }

    if (!this.match(types._default)) {
      return false;
    }

    var lookahead = this.lookahead();
    return lookahead.type === types.comma || lookahead.type === types.name && lookahead.value === "from";
  };

  _proto.parseExportSpecifiersMaybe = function parseExportSpecifiersMaybe(node) {
    if (this.eat(types.comma)) {
      node.specifiers = node.specifiers.concat(this.parseExportSpecifiers());
    }
  };

  _proto.parseExportFrom = function parseExportFrom(node, expect) {
    if (this.eatContextual("from")) {
      node.source = this.match(types.string) ? this.parseExprAtom() : this.unexpected();
      this.checkExport(node);
    } else {
      if (expect) {
        this.unexpected();
      } else {
        node.source = null;
      }
    }

    this.semicolon();
  };

  _proto.shouldParseExportStar = function shouldParseExportStar() {
    return this.match(types.star);
  };

  _proto.parseExportStar = function parseExportStar(node) {
    this.expect(types.star);

    if (this.isContextual("as")) {
      this.parseExportNamespace(node);
    } else {
      this.parseExportFrom(node, true);
      this.finishNode(node, "ExportAllDeclaration");
    }
  };

  _proto.parseExportNamespace = function parseExportNamespace(node) {
    this.expectPlugin("exportNamespaceFrom");
    var specifier = this.startNodeAt(this.state.lastTokStart, this.state.lastTokStartLoc);
    this.next();
    specifier.exported = this.parseIdentifier(true);
    node.specifiers = [this.finishNode(specifier, "ExportNamespaceSpecifier")];
    this.parseExportSpecifiersMaybe(node);
    this.parseExportFrom(node, true);
  };

  _proto.shouldParseExportDeclaration = function shouldParseExportDeclaration() {
    return this.state.type.keyword === "var" || this.state.type.keyword === "const" || this.state.type.keyword === "let" || this.state.type.keyword === "function" || this.state.type.keyword === "class" || this.isContextual("async") || this.match(types.at) && this.expectPlugin("decorators2");
  };

  _proto.checkExport = function checkExport(node, checkNames, isDefault) {
    if (checkNames) {
      // Check for duplicate exports
      if (isDefault) {
        // Default exports
        this.checkDuplicateExports(node, "default");
      } else if (node.specifiers && node.specifiers.length) {
        // Named exports
        for (var _i4 = 0, _node$specifiers2 = node.specifiers; _i4 < _node$specifiers2.length; _i4++) {
          var specifier = _node$specifiers2[_i4];
          this.checkDuplicateExports(specifier, specifier.exported.name);
        }
      } else if (node.declaration) {
        // Exported declarations
        if (node.declaration.type === "FunctionDeclaration" || node.declaration.type === "ClassDeclaration") {
          this.checkDuplicateExports(node, node.declaration.id.name);
        } else if (node.declaration.type === "VariableDeclaration") {
          for (var _i6 = 0, _node$declaration$dec2 = node.declaration.declarations; _i6 < _node$declaration$dec2.length; _i6++) {
            var declaration = _node$declaration$dec2[_i6];
            this.checkDeclaration(declaration.id);
          }
        }
      }
    }

    var currentContextDecorators = this.state.decoratorStack[this.state.decoratorStack.length - 1];

    if (currentContextDecorators.length) {
      var isClass = node.declaration && (node.declaration.type === "ClassDeclaration" || node.declaration.type === "ClassExpression");

      if (!node.declaration || !isClass) {
        throw this.raise(node.start, "You can only use decorators on an export when exporting a class");
      }

      this.takeDecorators(node.declaration);
    }
  };

  _proto.checkDeclaration = function checkDeclaration(node) {
    if (node.type === "ObjectPattern") {
      for (var _i8 = 0, _node$properties2 = node.properties; _i8 < _node$properties2.length; _i8++) {
        var prop = _node$properties2[_i8];
        this.checkDeclaration(prop);
      }
    } else if (node.type === "ArrayPattern") {
      for (var _i10 = 0, _node$elements2 = node.elements; _i10 < _node$elements2.length; _i10++) {
        var elem = _node$elements2[_i10];

        if (elem) {
          this.checkDeclaration(elem);
        }
      }
    } else if (node.type === "ObjectProperty") {
      this.checkDeclaration(node.value);
    } else if (node.type === "RestElement") {
      this.checkDeclaration(node.argument);
    } else if (node.type === "Identifier") {
      this.checkDuplicateExports(node, node.name);
    }
  };

  _proto.checkDuplicateExports = function checkDuplicateExports(node, name) {
    if (this.state.exportedIdentifiers.indexOf(name) > -1) {
      this.raiseDuplicateExportError(node, name);
    }

    this.state.exportedIdentifiers.push(name);
  };

  _proto.raiseDuplicateExportError = function raiseDuplicateExportError(node, name) {
    throw this.raise(node.start, name === "default" ? "Only one default export allowed per module." : `\`${name}\` has already been exported. Exported identifiers must be unique.`);
  }; // Parses a comma-separated list of module exports.


  _proto.parseExportSpecifiers = function parseExportSpecifiers() {
    var nodes = [];
    var first = true;
    var needsFrom; // export { x, y as z } [from '...']

    this.expect(types.braceL);

    while (!this.eat(types.braceR)) {
      if (first) {
        first = false;
      } else {
        this.expect(types.comma);
        if (this.eat(types.braceR)) break;
      }

      var isDefault = this.match(types._default);
      if (isDefault && !needsFrom) needsFrom = true;
      var node = this.startNode();
      node.local = this.parseIdentifier(isDefault);
      node.exported = this.eatContextual("as") ? this.parseIdentifier(true) : node.local.__clone();
      nodes.push(this.finishNode(node, "ExportSpecifier"));
    } // https://github.com/ember-cli/ember-cli/pull/3739


    if (needsFrom && !this.isContextual("from")) {
      this.unexpected();
    }

    return nodes;
  }; // Parses import declaration.


  _proto.parseImport = function parseImport(node) {
    // import '...'
    if (this.match(types.string)) {
      node.specifiers = [];
      node.source = this.parseExprAtom();
    } else {
      node.specifiers = [];
      this.parseImportSpecifiers(node);
      this.expectContextual("from");
      node.source = this.match(types.string) ? this.parseExprAtom() : this.unexpected();
    }

    this.semicolon();
    return this.finishNode(node, "ImportDeclaration");
  }; // eslint-disable-next-line no-unused-vars


  _proto.shouldParseDefaultImport = function shouldParseDefaultImport(node) {
    return this.match(types.name);
  };

  _proto.parseImportSpecifierLocal = function parseImportSpecifierLocal(node, specifier, type, contextDescription) {
    specifier.local = this.parseIdentifier();
    this.checkLVal(specifier.local, true, undefined, contextDescription);
    node.specifiers.push(this.finishNode(specifier, type));
  }; // Parses a comma-separated list of module imports.


  _proto.parseImportSpecifiers = function parseImportSpecifiers(node) {
    var first = true;

    if (this.shouldParseDefaultImport(node)) {
      // import defaultObj, { x, y as z } from '...'
      this.parseImportSpecifierLocal(node, this.startNode(), "ImportDefaultSpecifier", "default import specifier");
      if (!this.eat(types.comma)) return;
    }

    if (this.match(types.star)) {
      var specifier = this.startNode();
      this.next();
      this.expectContextual("as");
      this.parseImportSpecifierLocal(node, specifier, "ImportNamespaceSpecifier", "import namespace specifier");
      return;
    }

    this.expect(types.braceL);

    while (!this.eat(types.braceR)) {
      if (first) {
        first = false;
      } else {
        // Detect an attempt to deep destructure
        if (this.eat(types.colon)) {
          this.unexpected(null, "ES2015 named imports do not destructure. Use another statement for destructuring after the import.");
        }

        this.expect(types.comma);
        if (this.eat(types.braceR)) break;
      }

      this.parseImportSpecifier(node);
    }
  };

  _proto.parseImportSpecifier = function parseImportSpecifier(node) {
    var specifier = this.startNode();
    specifier.imported = this.parseIdentifier(true);

    if (this.eatContextual("as")) {
      specifier.local = this.parseIdentifier();
    } else {
      this.checkReservedWord(specifier.imported.name, specifier.start, true, true);
      specifier.local = specifier.imported.__clone();
    }

    this.checkLVal(specifier.local, true, undefined, "import specifier");
    node.specifiers.push(this.finishNode(specifier, "ImportSpecifier"));
  };

  return StatementParser;
}(ExpressionParser);

var plugins = {};

var Parser =
/*#__PURE__*/
function (_StatementParser) {
  _inheritsLoose(Parser, _StatementParser);

  function Parser(options, input) {
    var _this;

    options = getOptions(options);
    _this = _StatementParser.call(this, options, input) || this;
    _this.options = options;
    _this.inModule = _this.options.sourceType === "module";
    _this.input = input;
    _this.plugins = pluginsMap(_this.options.plugins);
    _this.filename = options.sourceFilename; // If enabled, skip leading hashbang line.

    if (_this.state.pos === 0 && _this.input[0] === "#" && _this.input[1] === "!") {
      _this.skipLineComment(2);
    }

    return _this;
  }

  var _proto = Parser.prototype;

  _proto.parse = function parse() {
    var file = this.startNode();
    var program = this.startNode();
    this.nextToken();
    return this.parseTopLevel(file, program);
  };

  return Parser;
}(StatementParser);

function pluginsMap(pluginList) {
  var pluginMap = {};

  for (var _i2 = 0; _i2 < pluginList.length; _i2++) {
    var _name = pluginList[_i2];
    pluginMap[_name] = true;
  }

  return pluginMap;
}

function isSimpleProperty(node) {
  return node != null && node.type === "Property" && node.kind === "init" && node.method === false;
}

var estreePlugin = (function (superClass) {
  return (
    /*#__PURE__*/
    function (_superClass) {
      _inheritsLoose(_class, _superClass);

      function _class() {
        return _superClass.apply(this, arguments) || this;
      }

      var _proto = _class.prototype;

      _proto.estreeParseRegExpLiteral = function estreeParseRegExpLiteral(_ref) {
        var pattern = _ref.pattern,
            flags = _ref.flags;
        var regex = null;

        try {
          regex = new RegExp(pattern, flags);
        } catch (e) {// In environments that don't support these flags value will
          // be null as the regex can't be represented natively.
        }

        var node = this.estreeParseLiteral(regex);
        node.regex = {
          pattern,
          flags
        };
        return node;
      };

      _proto.estreeParseLiteral = function estreeParseLiteral(value) {
        return this.parseLiteral(value, "Literal");
      };

      _proto.directiveToStmt = function directiveToStmt(directive) {
        var directiveLiteral = directive.value;
        var stmt = this.startNodeAt(directive.start, directive.loc.start);
        var expression = this.startNodeAt(directiveLiteral.start, directiveLiteral.loc.start);
        expression.value = directiveLiteral.value;
        expression.raw = directiveLiteral.extra.raw;
        stmt.expression = this.finishNodeAt(expression, "Literal", directiveLiteral.end, directiveLiteral.loc.end);
        stmt.directive = directiveLiteral.extra.raw.slice(1, -1);
        return this.finishNodeAt(stmt, "ExpressionStatement", directive.end, directive.loc.end);
      }; // ==================================
      // Overrides
      // ==================================


      _proto.initFunction = function initFunction(node, isAsync) {
        _superClass.prototype.initFunction.call(this, node, isAsync);

        node.expression = false;
      };

      _proto.checkDeclaration = function checkDeclaration(node) {
        if (isSimpleProperty(node)) {
          this.checkDeclaration(node.value);
        } else {
          _superClass.prototype.checkDeclaration.call(this, node);
        }
      };

      _proto.checkGetterSetterParamCount = function checkGetterSetterParamCount(prop) {
        var paramCount = prop.kind === "get" ? 0 : 1; // $FlowFixMe (prop.value present for ObjectMethod, but for ClassMethod should use prop.params?)

        if (prop.value.params.length !== paramCount) {
          var start = prop.start;

          if (prop.kind === "get") {
            this.raise(start, "getter should have no params");
          } else {
            this.raise(start, "setter should have exactly one param");
          }
        }
      };

      _proto.checkLVal = function checkLVal(expr, isBinding, checkClashes, contextDescription) {
        var _this = this;

        switch (expr.type) {
          case "ObjectPattern":
            expr.properties.forEach(function (prop) {
              _this.checkLVal(prop.type === "Property" ? prop.value : prop, isBinding, checkClashes, "object destructuring pattern");
            });
            break;

          default:
            _superClass.prototype.checkLVal.call(this, expr, isBinding, checkClashes, contextDescription);

        }
      };

      _proto.checkPropClash = function checkPropClash(prop, propHash) {
        if (prop.computed || !isSimpleProperty(prop)) return;
        var key = prop.key; // It is either an Identifier or a String/NumericLiteral

        var name = key.type === "Identifier" ? key.name : String(key.value);

        if (name === "__proto__") {
          if (propHash.proto) {
            this.raise(key.start, "Redefinition of __proto__ property");
          }

          propHash.proto = true;
        }
      };

      _proto.isStrictBody = function isStrictBody(node) {
        var isBlockStatement = node.body.type === "BlockStatement";

        if (isBlockStatement && node.body.body.length > 0) {
          for (var _i2 = 0, _node$body$body2 = node.body.body; _i2 < _node$body$body2.length; _i2++) {
            var directive = _node$body$body2[_i2];

            if (directive.type === "ExpressionStatement" && directive.expression.type === "Literal") {
              if (directive.expression.value === "use strict") return true;
            } else {
              // Break for the first non literal expression
              break;
            }
          }
        }

        return false;
      };

      _proto.isValidDirective = function isValidDirective(stmt) {
        return stmt.type === "ExpressionStatement" && stmt.expression.type === "Literal" && typeof stmt.expression.value === "string" && (!stmt.expression.extra || !stmt.expression.extra.parenthesized);
      };

      _proto.stmtToDirective = function stmtToDirective(stmt) {
        var directive = _superClass.prototype.stmtToDirective.call(this, stmt);

        var value = stmt.expression.value; // Reset value to the actual value as in estree mode we want
        // the stmt to have the real value and not the raw value

        directive.value.value = value;
        return directive;
      };

      _proto.parseBlockBody = function parseBlockBody(node, allowDirectives, topLevel, end) {
        var _this2 = this;

        _superClass.prototype.parseBlockBody.call(this, node, allowDirectives, topLevel, end);

        var directiveStatements = node.directives.map(function (d) {
          return _this2.directiveToStmt(d);
        });
        node.body = directiveStatements.concat(node.body);
        delete node.directives;
      };

      _proto.pushClassMethod = function pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor) {
        this.parseMethod(method, isGenerator, isAsync, isConstructor, "MethodDefinition");

        if (method.typeParameters) {
          // $FlowIgnore
          method.value.typeParameters = method.typeParameters;
          delete method.typeParameters;
        }

        classBody.body.push(method);
      };

      _proto.parseExprAtom = function parseExprAtom(refShorthandDefaultPos) {
        switch (this.state.type) {
          case types.regexp:
            return this.estreeParseRegExpLiteral(this.state.value);

          case types.num:
          case types.string:
            return this.estreeParseLiteral(this.state.value);

          case types._null:
            return this.estreeParseLiteral(null);

          case types._true:
            return this.estreeParseLiteral(true);

          case types._false:
            return this.estreeParseLiteral(false);

          default:
            return _superClass.prototype.parseExprAtom.call(this, refShorthandDefaultPos);
        }
      };

      _proto.parseLiteral = function parseLiteral(value, type, startPos, startLoc) {
        var node = _superClass.prototype.parseLiteral.call(this, value, type, startPos, startLoc);

        node.raw = node.extra.raw;
        delete node.extra;
        return node;
      };

      _proto.parseFunctionBody = function parseFunctionBody(node, allowExpression) {
        _superClass.prototype.parseFunctionBody.call(this, node, allowExpression);

        node.expression = node.body.type !== "BlockStatement";
      };

      _proto.parseMethod = function parseMethod(node, isGenerator, isAsync, isConstructor, type) {
        var funcNode = this.startNode();
        funcNode.kind = node.kind; // provide kind, so super method correctly sets state

        funcNode = _superClass.prototype.parseMethod.call(this, funcNode, isGenerator, isAsync, isConstructor, "FunctionExpression");
        delete funcNode.kind; // $FlowIgnore

        node.value = funcNode;
        return this.finishNode(node, type);
      };

      _proto.parseObjectMethod = function parseObjectMethod(prop, isGenerator, isAsync, isPattern) {
        var node = _superClass.prototype.parseObjectMethod.call(this, prop, isGenerator, isAsync, isPattern);

        if (node) {
          node.type = "Property";
          if (node.kind === "method") node.kind = "init";
          node.shorthand = false;
        }

        return node;
      };

      _proto.parseObjectProperty = function parseObjectProperty(prop, startPos, startLoc, isPattern, refShorthandDefaultPos) {
        var node = _superClass.prototype.parseObjectProperty.call(this, prop, startPos, startLoc, isPattern, refShorthandDefaultPos);

        if (node) {
          node.kind = "init";
          node.type = "Property";
        }

        return node;
      };

      _proto.toAssignable = function toAssignable(node, isBinding, contextDescription) {
        if (isSimpleProperty(node)) {
          this.toAssignable(node.value, isBinding, contextDescription);
          return node;
        }

        return _superClass.prototype.toAssignable.call(this, node, isBinding, contextDescription);
      };

      _proto.toAssignableObjectExpressionProp = function toAssignableObjectExpressionProp(prop, isBinding, isLast) {
        if (prop.kind === "get" || prop.kind === "set") {
          this.raise(prop.key.start, "Object pattern can't contain getter or setter");
        } else if (prop.method) {
          this.raise(prop.key.start, "Object pattern can't contain methods");
        } else {
          _superClass.prototype.toAssignableObjectExpressionProp.call(this, prop, isBinding, isLast);
        }
      };

      return _class;
    }(superClass)
  );
});

/* eslint max-len: 0 */
var primitiveTypes = ["any", "bool", "boolean", "empty", "false", "mixed", "null", "number", "static", "string", "true", "typeof", "void"];

function isEsModuleType(bodyElement) {
  return bodyElement.type === "DeclareExportAllDeclaration" || bodyElement.type === "DeclareExportDeclaration" && (!bodyElement.declaration || bodyElement.declaration.type !== "TypeAlias" && bodyElement.declaration.type !== "InterfaceDeclaration");
}

function hasTypeImportKind(node) {
  return node.importKind === "type" || node.importKind === "typeof";
}

function isMaybeDefaultImport(state) {
  return (state.type === types.name || !!state.type.keyword) && state.value !== "from";
}

var exportSuggestions = {
  const: "declare export var",
  let: "declare export var",
  type: "export type",
  interface: "export interface"
}; // Like Array#filter, but returns a tuple [ acceptedElements, discardedElements ]

function partition(list, test) {
  var list1 = [];
  var list2 = [];

  for (var i = 0; i < list.length; i++) {
    (test(list[i], i, list) ? list1 : list2).push(list[i]);
  }

  return [list1, list2];
}

var flowPlugin = (function (superClass) {
  return (
    /*#__PURE__*/
    function (_superClass) {
      _inheritsLoose(_class, _superClass);

      function _class() {
        return _superClass.apply(this, arguments) || this;
      }

      var _proto = _class.prototype;

      _proto.flowParseTypeInitialiser = function flowParseTypeInitialiser(tok) {
        var oldInType = this.state.inType;
        this.state.inType = true;
        this.expect(tok || types.colon);
        var type = this.flowParseType();
        this.state.inType = oldInType;
        return type;
      };

      _proto.flowParsePredicate = function flowParsePredicate() {
        var node = this.startNode();
        var moduloLoc = this.state.startLoc;
        var moduloPos = this.state.start;
        this.expect(types.modulo);
        var checksLoc = this.state.startLoc;
        this.expectContextual("checks"); // Force '%' and 'checks' to be adjacent

        if (moduloLoc.line !== checksLoc.line || moduloLoc.column !== checksLoc.column - 1) {
          this.raise(moduloPos, "Spaces between ´%´ and ´checks´ are not allowed here.");
        }

        if (this.eat(types.parenL)) {
          node.value = this.parseExpression();
          this.expect(types.parenR);
          return this.finishNode(node, "DeclaredPredicate");
        } else {
          return this.finishNode(node, "InferredPredicate");
        }
      };

      _proto.flowParseTypeAndPredicateInitialiser = function flowParseTypeAndPredicateInitialiser() {
        var oldInType = this.state.inType;
        this.state.inType = true;
        this.expect(types.colon);
        var type = null;
        var predicate = null;

        if (this.match(types.modulo)) {
          this.state.inType = oldInType;
          predicate = this.flowParsePredicate();
        } else {
          type = this.flowParseType();
          this.state.inType = oldInType;

          if (this.match(types.modulo)) {
            predicate = this.flowParsePredicate();
          }
        }

        return [type, predicate];
      };

      _proto.flowParseDeclareClass = function flowParseDeclareClass(node) {
        this.next();
        this.flowParseInterfaceish(node,
        /*isClass*/
        true);
        return this.finishNode(node, "DeclareClass");
      };

      _proto.flowParseDeclareFunction = function flowParseDeclareFunction(node) {
        this.next();
        var id = node.id = this.parseIdentifier();
        var typeNode = this.startNode();
        var typeContainer = this.startNode();

        if (this.isRelational("<")) {
          typeNode.typeParameters = this.flowParseTypeParameterDeclaration();
        } else {
          typeNode.typeParameters = null;
        }

        this.expect(types.parenL);
        var tmp = this.flowParseFunctionTypeParams();
        typeNode.params = tmp.params;
        typeNode.rest = tmp.rest;
        this.expect(types.parenR);

        var _flowParseTypeAndPred = this.flowParseTypeAndPredicateInitialiser();

        // $FlowFixMe (destructuring not supported yet)
        typeNode.returnType = _flowParseTypeAndPred[0];
        // $FlowFixMe (destructuring not supported yet)
        node.predicate = _flowParseTypeAndPred[1];
        typeContainer.typeAnnotation = this.finishNode(typeNode, "FunctionTypeAnnotation");
        id.typeAnnotation = this.finishNode(typeContainer, "TypeAnnotation");
        this.finishNode(id, id.type);
        this.semicolon();
        return this.finishNode(node, "DeclareFunction");
      };

      _proto.flowParseDeclare = function flowParseDeclare(node, insideModule) {
        if (this.match(types._class)) {
          return this.flowParseDeclareClass(node);
        } else if (this.match(types._function)) {
          return this.flowParseDeclareFunction(node);
        } else if (this.match(types._var)) {
          return this.flowParseDeclareVariable(node);
        } else if (this.isContextual("module")) {
          if (this.lookahead().type === types.dot) {
            return this.flowParseDeclareModuleExports(node);
          } else {
            if (insideModule) {
              this.unexpected(null, "`declare module` cannot be used inside another `declare module`");
            }

            return this.flowParseDeclareModule(node);
          }
        } else if (this.isContextual("type")) {
          return this.flowParseDeclareTypeAlias(node);
        } else if (this.isContextual("opaque")) {
          return this.flowParseDeclareOpaqueType(node);
        } else if (this.isContextual("interface")) {
          return this.flowParseDeclareInterface(node);
        } else if (this.match(types._export)) {
          return this.flowParseDeclareExportDeclaration(node, insideModule);
        } else {
          throw this.unexpected();
        }
      };

      _proto.flowParseDeclareVariable = function flowParseDeclareVariable(node) {
        this.next();
        node.id = this.flowParseTypeAnnotatableIdentifier(
        /*allowPrimitiveOverride*/
        true);
        this.semicolon();
        return this.finishNode(node, "DeclareVariable");
      };

      _proto.flowParseDeclareModule = function flowParseDeclareModule(node) {
        var _this = this;

        this.next();

        if (this.match(types.string)) {
          node.id = this.parseExprAtom();
        } else {
          node.id = this.parseIdentifier();
        }

        var bodyNode = node.body = this.startNode();
        var body = bodyNode.body = [];
        this.expect(types.braceL);

        while (!this.match(types.braceR)) {
          var _bodyNode = this.startNode();

          if (this.match(types._import)) {
            var lookahead = this.lookahead();

            if (lookahead.value !== "type" && lookahead.value !== "typeof") {
              this.unexpected(null, "Imports within a `declare module` body must always be `import type` or `import typeof`");
            }

            this.next();
            this.parseImport(_bodyNode);
          } else {
            this.expectContextual("declare", "Only declares and type imports are allowed inside declare module");
            _bodyNode = this.flowParseDeclare(_bodyNode, true);
          }

          body.push(_bodyNode);
        }

        this.expect(types.braceR);
        this.finishNode(bodyNode, "BlockStatement");
        var kind = null;
        var hasModuleExport = false;
        var errorMessage = "Found both `declare module.exports` and `declare export` in the same module. Modules can only have 1 since they are either an ES module or they are a CommonJS module";
        body.forEach(function (bodyElement) {
          if (isEsModuleType(bodyElement)) {
            if (kind === "CommonJS") {
              _this.unexpected(bodyElement.start, errorMessage);
            }

            kind = "ES";
          } else if (bodyElement.type === "DeclareModuleExports") {
            if (hasModuleExport) {
              _this.unexpected(bodyElement.start, "Duplicate `declare module.exports` statement");
            }

            if (kind === "ES") _this.unexpected(bodyElement.start, errorMessage);
            kind = "CommonJS";
            hasModuleExport = true;
          }
        });
        node.kind = kind || "CommonJS";
        return this.finishNode(node, "DeclareModule");
      };

      _proto.flowParseDeclareExportDeclaration = function flowParseDeclareExportDeclaration(node, insideModule) {
        this.expect(types._export);

        if (this.eat(types._default)) {
          if (this.match(types._function) || this.match(types._class)) {
            // declare export default class ...
            // declare export default function ...
            node.declaration = this.flowParseDeclare(this.startNode());
          } else {
            // declare export default [type];
            node.declaration = this.flowParseType();
            this.semicolon();
          }

          node.default = true;
          return this.finishNode(node, "DeclareExportDeclaration");
        } else {
          if (this.match(types._const) || this.match(types._let) || (this.isContextual("type") || this.isContextual("interface")) && !insideModule) {
            var label = this.state.value;
            var suggestion = exportSuggestions[label];
            this.unexpected(this.state.start, `\`declare export ${label}\` is not supported. Use \`${suggestion}\` instead`);
          }

          if (this.match(types._var) || // declare export var ...
          this.match(types._function) || // declare export function ...
          this.match(types._class) || // declare export class ...
          this.isContextual("opaque") // declare export opaque ..
          ) {
              node.declaration = this.flowParseDeclare(this.startNode());
              node.default = false;
              return this.finishNode(node, "DeclareExportDeclaration");
            } else if (this.match(types.star) || // declare export * from ''
          this.match(types.braceL) || // declare export {} ...
          this.isContextual("interface") || // declare export interface ...
          this.isContextual("type") || // declare export type ...
          this.isContextual("opaque") // declare export opaque type ...
          ) {
              node = this.parseExport(node);

              if (node.type === "ExportNamedDeclaration") {
                // flow does not support the ExportNamedDeclaration
                // $FlowIgnore
                node.type = "ExportDeclaration"; // $FlowFixMe

                node.default = false;
                delete node.exportKind;
              } // $FlowIgnore


              node.type = "Declare" + node.type;
              return node;
            }
        }

        throw this.unexpected();
      };

      _proto.flowParseDeclareModuleExports = function flowParseDeclareModuleExports(node) {
        this.expectContextual("module");
        this.expect(types.dot);
        this.expectContextual("exports");
        node.typeAnnotation = this.flowParseTypeAnnotation();
        this.semicolon();
        return this.finishNode(node, "DeclareModuleExports");
      };

      _proto.flowParseDeclareTypeAlias = function flowParseDeclareTypeAlias(node) {
        this.next();
        this.flowParseTypeAlias(node);
        return this.finishNode(node, "DeclareTypeAlias");
      };

      _proto.flowParseDeclareOpaqueType = function flowParseDeclareOpaqueType(node) {
        this.next();
        this.flowParseOpaqueType(node, true);
        return this.finishNode(node, "DeclareOpaqueType");
      };

      _proto.flowParseDeclareInterface = function flowParseDeclareInterface(node) {
        this.next();
        this.flowParseInterfaceish(node);
        return this.finishNode(node, "DeclareInterface");
      }; // Interfaces


      _proto.flowParseInterfaceish = function flowParseInterfaceish(node, isClass) {
        node.id = this.flowParseRestrictedIdentifier(
        /*liberal*/
        !isClass);

        if (this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterDeclaration();
        } else {
          node.typeParameters = null;
        }

        node.extends = [];
        node.mixins = [];

        if (this.eat(types._extends)) {
          do {
            node.extends.push(this.flowParseInterfaceExtends());
          } while (!isClass && this.eat(types.comma));
        }

        if (this.isContextual("mixins")) {
          this.next();

          do {
            node.mixins.push(this.flowParseInterfaceExtends());
          } while (this.eat(types.comma));
        }

        node.body = this.flowParseObjectType(true, false, false);
      };

      _proto.flowParseInterfaceExtends = function flowParseInterfaceExtends() {
        var node = this.startNode();
        node.id = this.flowParseQualifiedTypeIdentifier();

        if (this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterInstantiation();
        } else {
          node.typeParameters = null;
        }

        return this.finishNode(node, "InterfaceExtends");
      };

      _proto.flowParseInterface = function flowParseInterface(node) {
        this.flowParseInterfaceish(node);
        return this.finishNode(node, "InterfaceDeclaration");
      };

      _proto.checkReservedType = function checkReservedType(word, startLoc) {
        if (primitiveTypes.indexOf(word) > -1) {
          this.raise(startLoc, `Cannot overwrite primitive type ${word}`);
        }
      };

      _proto.flowParseRestrictedIdentifier = function flowParseRestrictedIdentifier(liberal) {
        this.checkReservedType(this.state.value, this.state.start);
        return this.parseIdentifier(liberal);
      }; // Type aliases


      _proto.flowParseTypeAlias = function flowParseTypeAlias(node) {
        node.id = this.flowParseRestrictedIdentifier();

        if (this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterDeclaration();
        } else {
          node.typeParameters = null;
        }

        node.right = this.flowParseTypeInitialiser(types.eq);
        this.semicolon();
        return this.finishNode(node, "TypeAlias");
      };

      _proto.flowParseOpaqueType = function flowParseOpaqueType(node, declare) {
        this.expectContextual("type");
        node.id = this.flowParseRestrictedIdentifier(
        /*liberal*/
        true);

        if (this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterDeclaration();
        } else {
          node.typeParameters = null;
        } // Parse the supertype


        node.supertype = null;

        if (this.match(types.colon)) {
          node.supertype = this.flowParseTypeInitialiser(types.colon);
        }

        node.impltype = null;

        if (!declare) {
          node.impltype = this.flowParseTypeInitialiser(types.eq);
        }

        this.semicolon();
        return this.finishNode(node, "OpaqueType");
      }; // Type annotations


      _proto.flowParseTypeParameter = function flowParseTypeParameter() {
        var node = this.startNode();
        var variance = this.flowParseVariance();
        var ident = this.flowParseTypeAnnotatableIdentifier();
        node.name = ident.name;
        node.variance = variance;
        node.bound = ident.typeAnnotation;

        if (this.match(types.eq)) {
          this.eat(types.eq);
          node.default = this.flowParseType();
        }

        return this.finishNode(node, "TypeParameter");
      };

      _proto.flowParseTypeParameterDeclaration = function flowParseTypeParameterDeclaration() {
        var oldInType = this.state.inType;
        var node = this.startNode();
        node.params = [];
        this.state.inType = true; // istanbul ignore else: this condition is already checked at all call sites

        if (this.isRelational("<") || this.match(types.jsxTagStart)) {
          this.next();
        } else {
          this.unexpected();
        }

        do {
          node.params.push(this.flowParseTypeParameter());

          if (!this.isRelational(">")) {
            this.expect(types.comma);
          }
        } while (!this.isRelational(">"));

        this.expectRelational(">");
        this.state.inType = oldInType;
        return this.finishNode(node, "TypeParameterDeclaration");
      };

      _proto.flowParseTypeParameterInstantiation = function flowParseTypeParameterInstantiation() {
        var node = this.startNode();
        var oldInType = this.state.inType;
        node.params = [];
        this.state.inType = true;
        this.expectRelational("<");

        while (!this.isRelational(">")) {
          node.params.push(this.flowParseType());

          if (!this.isRelational(">")) {
            this.expect(types.comma);
          }
        }

        this.expectRelational(">");
        this.state.inType = oldInType;
        return this.finishNode(node, "TypeParameterInstantiation");
      };

      _proto.flowParseObjectPropertyKey = function flowParseObjectPropertyKey() {
        return this.match(types.num) || this.match(types.string) ? this.parseExprAtom() : this.parseIdentifier(true);
      };

      _proto.flowParseObjectTypeIndexer = function flowParseObjectTypeIndexer(node, isStatic, variance) {
        node.static = isStatic;
        this.expect(types.bracketL);

        if (this.lookahead().type === types.colon) {
          node.id = this.flowParseObjectPropertyKey();
          node.key = this.flowParseTypeInitialiser();
        } else {
          node.id = null;
          node.key = this.flowParseType();
        }

        this.expect(types.bracketR);
        node.value = this.flowParseTypeInitialiser();
        node.variance = variance;
        return this.finishNode(node, "ObjectTypeIndexer");
      };

      _proto.flowParseObjectTypeMethodish = function flowParseObjectTypeMethodish(node) {
        node.params = [];
        node.rest = null;
        node.typeParameters = null;

        if (this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterDeclaration();
        }

        this.expect(types.parenL);

        while (!this.match(types.parenR) && !this.match(types.ellipsis)) {
          node.params.push(this.flowParseFunctionTypeParam());

          if (!this.match(types.parenR)) {
            this.expect(types.comma);
          }
        }

        if (this.eat(types.ellipsis)) {
          node.rest = this.flowParseFunctionTypeParam();
        }

        this.expect(types.parenR);
        node.returnType = this.flowParseTypeInitialiser();
        return this.finishNode(node, "FunctionTypeAnnotation");
      };

      _proto.flowParseObjectTypeCallProperty = function flowParseObjectTypeCallProperty(node, isStatic) {
        var valueNode = this.startNode();
        node.static = isStatic;
        node.value = this.flowParseObjectTypeMethodish(valueNode);
        return this.finishNode(node, "ObjectTypeCallProperty");
      };

      _proto.flowParseObjectType = function flowParseObjectType(allowStatic, allowExact, allowSpread) {
        var oldInType = this.state.inType;
        this.state.inType = true;
        var nodeStart = this.startNode();
        nodeStart.callProperties = [];
        nodeStart.properties = [];
        nodeStart.indexers = [];
        var endDelim;
        var exact;

        if (allowExact && this.match(types.braceBarL)) {
          this.expect(types.braceBarL);
          endDelim = types.braceBarR;
          exact = true;
        } else {
          this.expect(types.braceL);
          endDelim = types.braceR;
          exact = false;
        }

        nodeStart.exact = exact;

        while (!this.match(endDelim)) {
          var isStatic = false;
          var node = this.startNode();

          if (allowStatic && this.isContextual("static") && this.lookahead().type !== types.colon) {
            this.next();
            isStatic = true;
          }

          var variance = this.flowParseVariance();

          if (this.match(types.bracketL)) {
            nodeStart.indexers.push(this.flowParseObjectTypeIndexer(node, isStatic, variance));
          } else if (this.match(types.parenL) || this.isRelational("<")) {
            if (variance) {
              this.unexpected(variance.start);
            }

            nodeStart.callProperties.push(this.flowParseObjectTypeCallProperty(node, isStatic));
          } else {
            var kind = "init";

            if (this.isContextual("get") || this.isContextual("set")) {
              var lookahead = this.lookahead();

              if (lookahead.type === types.name || lookahead.type === types.string || lookahead.type === types.num) {
                kind = this.state.value;
                this.next();
              }
            }

            nodeStart.properties.push(this.flowParseObjectTypeProperty(node, isStatic, variance, kind, allowSpread));
          }

          this.flowObjectTypeSemicolon();
        }

        this.expect(endDelim);
        var out = this.finishNode(nodeStart, "ObjectTypeAnnotation");
        this.state.inType = oldInType;
        return out;
      };

      _proto.flowParseObjectTypeProperty = function flowParseObjectTypeProperty(node, isStatic, variance, kind, allowSpread) {
        if (this.match(types.ellipsis)) {
          if (!allowSpread) {
            this.unexpected(null, "Spread operator cannot appear in class or interface definitions");
          }

          if (variance) {
            this.unexpected(variance.start, "Spread properties cannot have variance");
          }

          this.expect(types.ellipsis);
          node.argument = this.flowParseType();
          return this.finishNode(node, "ObjectTypeSpreadProperty");
        } else {
          node.key = this.flowParseObjectPropertyKey();
          node.static = isStatic;
          node.kind = kind;
          var optional = false;

          if (this.isRelational("<") || this.match(types.parenL)) {
            // This is a method property
            node.method = true;

            if (variance) {
              this.unexpected(variance.start);
            }

            node.value = this.flowParseObjectTypeMethodish(this.startNodeAt(node.start, node.loc.start));

            if (kind === "get" || kind === "set") {
              this.flowCheckGetterSetterParamCount(node);
            }
          } else {
            if (kind !== "init") this.unexpected();
            node.method = false;

            if (this.eat(types.question)) {
              optional = true;
            }

            node.value = this.flowParseTypeInitialiser();
            node.variance = variance;
          }

          node.optional = optional;
          return this.finishNode(node, "ObjectTypeProperty");
        }
      }; // This is similar to checkGetterSetterParamCount, but as
      // babylon uses non estree properties we cannot reuse it here


      _proto.flowCheckGetterSetterParamCount = function flowCheckGetterSetterParamCount(property) {
        var paramCount = property.kind === "get" ? 0 : 1;

        if (property.value.params.length !== paramCount) {
          var start = property.start;

          if (property.kind === "get") {
            this.raise(start, "getter should have no params");
          } else {
            this.raise(start, "setter should have exactly one param");
          }
        }
      };

      _proto.flowObjectTypeSemicolon = function flowObjectTypeSemicolon() {
        if (!this.eat(types.semi) && !this.eat(types.comma) && !this.match(types.braceR) && !this.match(types.braceBarR)) {
          this.unexpected();
        }
      };

      _proto.flowParseQualifiedTypeIdentifier = function flowParseQualifiedTypeIdentifier(startPos, startLoc, id) {
        startPos = startPos || this.state.start;
        startLoc = startLoc || this.state.startLoc;
        var node = id || this.parseIdentifier();

        while (this.eat(types.dot)) {
          var node2 = this.startNodeAt(startPos, startLoc);
          node2.qualification = node;
          node2.id = this.parseIdentifier();
          node = this.finishNode(node2, "QualifiedTypeIdentifier");
        }

        return node;
      };

      _proto.flowParseGenericType = function flowParseGenericType(startPos, startLoc, id) {
        var node = this.startNodeAt(startPos, startLoc);
        node.typeParameters = null;
        node.id = this.flowParseQualifiedTypeIdentifier(startPos, startLoc, id);

        if (this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterInstantiation();
        }

        return this.finishNode(node, "GenericTypeAnnotation");
      };

      _proto.flowParseTypeofType = function flowParseTypeofType() {
        var node = this.startNode();
        this.expect(types._typeof);
        node.argument = this.flowParsePrimaryType();
        return this.finishNode(node, "TypeofTypeAnnotation");
      };

      _proto.flowParseTupleType = function flowParseTupleType() {
        var node = this.startNode();
        node.types = [];
        this.expect(types.bracketL); // We allow trailing commas

        while (this.state.pos < this.input.length && !this.match(types.bracketR)) {
          node.types.push(this.flowParseType());
          if (this.match(types.bracketR)) break;
          this.expect(types.comma);
        }

        this.expect(types.bracketR);
        return this.finishNode(node, "TupleTypeAnnotation");
      };

      _proto.flowParseFunctionTypeParam = function flowParseFunctionTypeParam() {
        var name = null;
        var optional = false;
        var typeAnnotation = null;
        var node = this.startNode();
        var lh = this.lookahead();

        if (lh.type === types.colon || lh.type === types.question) {
          name = this.parseIdentifier();

          if (this.eat(types.question)) {
            optional = true;
          }

          typeAnnotation = this.flowParseTypeInitialiser();
        } else {
          typeAnnotation = this.flowParseType();
        }

        node.name = name;
        node.optional = optional;
        node.typeAnnotation = typeAnnotation;
        return this.finishNode(node, "FunctionTypeParam");
      };

      _proto.reinterpretTypeAsFunctionTypeParam = function reinterpretTypeAsFunctionTypeParam(type) {
        var node = this.startNodeAt(type.start, type.loc.start);
        node.name = null;
        node.optional = false;
        node.typeAnnotation = type;
        return this.finishNode(node, "FunctionTypeParam");
      };

      _proto.flowParseFunctionTypeParams = function flowParseFunctionTypeParams(params) {
        if (params === void 0) {
          params = [];
        }

        var rest = null;

        while (!this.match(types.parenR) && !this.match(types.ellipsis)) {
          params.push(this.flowParseFunctionTypeParam());

          if (!this.match(types.parenR)) {
            this.expect(types.comma);
          }
        }

        if (this.eat(types.ellipsis)) {
          rest = this.flowParseFunctionTypeParam();
        }

        return {
          params,
          rest
        };
      };

      _proto.flowIdentToTypeAnnotation = function flowIdentToTypeAnnotation(startPos, startLoc, node, id) {
        switch (id.name) {
          case "any":
            return this.finishNode(node, "AnyTypeAnnotation");

          case "void":
            return this.finishNode(node, "VoidTypeAnnotation");

          case "bool":
          case "boolean":
            return this.finishNode(node, "BooleanTypeAnnotation");

          case "mixed":
            return this.finishNode(node, "MixedTypeAnnotation");

          case "empty":
            return this.finishNode(node, "EmptyTypeAnnotation");

          case "number":
            return this.finishNode(node, "NumberTypeAnnotation");

          case "string":
            return this.finishNode(node, "StringTypeAnnotation");

          default:
            return this.flowParseGenericType(startPos, startLoc, id);
        }
      }; // The parsing of types roughly parallels the parsing of expressions, and
      // primary types are kind of like primary expressions...they're the
      // primitives with which other types are constructed.


      _proto.flowParsePrimaryType = function flowParsePrimaryType() {
        var startPos = this.state.start;
        var startLoc = this.state.startLoc;
        var node = this.startNode();
        var tmp;
        var type;
        var isGroupedType = false;
        var oldNoAnonFunctionType = this.state.noAnonFunctionType;

        switch (this.state.type) {
          case types.name:
            return this.flowIdentToTypeAnnotation(startPos, startLoc, node, this.parseIdentifier());

          case types.braceL:
            return this.flowParseObjectType(false, false, true);

          case types.braceBarL:
            return this.flowParseObjectType(false, true, true);

          case types.bracketL:
            return this.flowParseTupleType();

          case types.relational:
            if (this.state.value === "<") {
              node.typeParameters = this.flowParseTypeParameterDeclaration();
              this.expect(types.parenL);
              tmp = this.flowParseFunctionTypeParams();
              node.params = tmp.params;
              node.rest = tmp.rest;
              this.expect(types.parenR);
              this.expect(types.arrow);
              node.returnType = this.flowParseType();
              return this.finishNode(node, "FunctionTypeAnnotation");
            }

            break;

          case types.parenL:
            this.next(); // Check to see if this is actually a grouped type

            if (!this.match(types.parenR) && !this.match(types.ellipsis)) {
              if (this.match(types.name)) {
                var token = this.lookahead().type;
                isGroupedType = token !== types.question && token !== types.colon;
              } else {
                isGroupedType = true;
              }
            }

            if (isGroupedType) {
              this.state.noAnonFunctionType = false;
              type = this.flowParseType();
              this.state.noAnonFunctionType = oldNoAnonFunctionType; // A `,` or a `) =>` means this is an anonymous function type

              if (this.state.noAnonFunctionType || !(this.match(types.comma) || this.match(types.parenR) && this.lookahead().type === types.arrow)) {
                this.expect(types.parenR);
                return type;
              } else {
                // Eat a comma if there is one
                this.eat(types.comma);
              }
            }

            if (type) {
              tmp = this.flowParseFunctionTypeParams([this.reinterpretTypeAsFunctionTypeParam(type)]);
            } else {
              tmp = this.flowParseFunctionTypeParams();
            }

            node.params = tmp.params;
            node.rest = tmp.rest;
            this.expect(types.parenR);
            this.expect(types.arrow);
            node.returnType = this.flowParseType();
            node.typeParameters = null;
            return this.finishNode(node, "FunctionTypeAnnotation");

          case types.string:
            return this.parseLiteral(this.state.value, "StringLiteralTypeAnnotation");

          case types._true:
          case types._false:
            node.value = this.match(types._true);
            this.next();
            return this.finishNode(node, "BooleanLiteralTypeAnnotation");

          case types.plusMin:
            if (this.state.value === "-") {
              this.next();

              if (!this.match(types.num)) {
                this.unexpected(null, `Unexpected token, expected "number"`);
              }

              return this.parseLiteral(-this.state.value, "NumberLiteralTypeAnnotation", node.start, node.loc.start);
            }

            this.unexpected();

          case types.num:
            return this.parseLiteral(this.state.value, "NumberLiteralTypeAnnotation");

          case types._null:
            this.next();
            return this.finishNode(node, "NullLiteralTypeAnnotation");

          case types._this:
            this.next();
            return this.finishNode(node, "ThisTypeAnnotation");

          case types.star:
            this.next();
            return this.finishNode(node, "ExistsTypeAnnotation");

          default:
            if (this.state.type.keyword === "typeof") {
              return this.flowParseTypeofType();
            }

        }

        throw this.unexpected();
      };

      _proto.flowParsePostfixType = function flowParsePostfixType() {
        var startPos = this.state.start,
            startLoc = this.state.startLoc;
        var type = this.flowParsePrimaryType();

        while (!this.canInsertSemicolon() && this.match(types.bracketL)) {
          var node = this.startNodeAt(startPos, startLoc);
          node.elementType = type;
          this.expect(types.bracketL);
          this.expect(types.bracketR);
          type = this.finishNode(node, "ArrayTypeAnnotation");
        }

        return type;
      };

      _proto.flowParsePrefixType = function flowParsePrefixType() {
        var node = this.startNode();

        if (this.eat(types.question)) {
          node.typeAnnotation = this.flowParsePrefixType();
          return this.finishNode(node, "NullableTypeAnnotation");
        } else {
          return this.flowParsePostfixType();
        }
      };

      _proto.flowParseAnonFunctionWithoutParens = function flowParseAnonFunctionWithoutParens() {
        var param = this.flowParsePrefixType();

        if (!this.state.noAnonFunctionType && this.eat(types.arrow)) {
          // TODO: This should be a type error. Passing in a SourceLocation, and it expects a Position.
          var node = this.startNodeAt(param.start, param.loc.start);
          node.params = [this.reinterpretTypeAsFunctionTypeParam(param)];
          node.rest = null;
          node.returnType = this.flowParseType();
          node.typeParameters = null;
          return this.finishNode(node, "FunctionTypeAnnotation");
        }

        return param;
      };

      _proto.flowParseIntersectionType = function flowParseIntersectionType() {
        var node = this.startNode();
        this.eat(types.bitwiseAND);
        var type = this.flowParseAnonFunctionWithoutParens();
        node.types = [type];

        while (this.eat(types.bitwiseAND)) {
          node.types.push(this.flowParseAnonFunctionWithoutParens());
        }

        return node.types.length === 1 ? type : this.finishNode(node, "IntersectionTypeAnnotation");
      };

      _proto.flowParseUnionType = function flowParseUnionType() {
        var node = this.startNode();
        this.eat(types.bitwiseOR);
        var type = this.flowParseIntersectionType();
        node.types = [type];

        while (this.eat(types.bitwiseOR)) {
          node.types.push(this.flowParseIntersectionType());
        }

        return node.types.length === 1 ? type : this.finishNode(node, "UnionTypeAnnotation");
      };

      _proto.flowParseType = function flowParseType() {
        var oldInType = this.state.inType;
        this.state.inType = true;
        var type = this.flowParseUnionType();
        this.state.inType = oldInType; // Ensure that a brace after a function generic type annotation is a
        // statement, except in arrow functions (noAnonFunctionType)

        this.state.exprAllowed = this.state.exprAllowed || this.state.noAnonFunctionType;
        return type;
      };

      _proto.flowParseTypeAnnotation = function flowParseTypeAnnotation() {
        var node = this.startNode();
        node.typeAnnotation = this.flowParseTypeInitialiser();
        return this.finishNode(node, "TypeAnnotation");
      };

      _proto.flowParseTypeAnnotatableIdentifier = function flowParseTypeAnnotatableIdentifier(allowPrimitiveOverride) {
        var ident = allowPrimitiveOverride ? this.parseIdentifier() : this.flowParseRestrictedIdentifier();

        if (this.match(types.colon)) {
          ident.typeAnnotation = this.flowParseTypeAnnotation();
          this.finishNode(ident, ident.type);
        }

        return ident;
      };

      _proto.typeCastToParameter = function typeCastToParameter(node) {
        node.expression.typeAnnotation = node.typeAnnotation;
        return this.finishNodeAt(node.expression, node.expression.type, node.typeAnnotation.end, node.typeAnnotation.loc.end);
      };

      _proto.flowParseVariance = function flowParseVariance() {
        var variance = null;

        if (this.match(types.plusMin)) {
          variance = this.startNode();

          if (this.state.value === "+") {
            variance.kind = "plus";
          } else {
            variance.kind = "minus";
          }

          this.next();
          this.finishNode(variance, "Variance");
        }

        return variance;
      }; // ==================================
      // Overrides
      // ==================================


      _proto.parseFunctionBody = function parseFunctionBody(node, allowExpressionBody) {
        var _this2 = this;

        if (allowExpressionBody) {
          return this.forwardNoArrowParamsConversionAt(node, function () {
            return _superClass.prototype.parseFunctionBody.call(_this2, node, true);
          });
        }

        return _superClass.prototype.parseFunctionBody.call(this, node, false);
      };

      _proto.parseFunctionBodyAndFinish = function parseFunctionBodyAndFinish(node, type, allowExpressionBody) {
        // For arrow functions, `parseArrow` handles the return type itself.
        if (!allowExpressionBody && this.match(types.colon)) {
          var typeNode = this.startNode();

          var _flowParseTypeAndPred2 = this.flowParseTypeAndPredicateInitialiser();

          // $FlowFixMe (destructuring not supported yet)
          typeNode.typeAnnotation = _flowParseTypeAndPred2[0];
          // $FlowFixMe (destructuring not supported yet)
          node.predicate = _flowParseTypeAndPred2[1];
          node.returnType = typeNode.typeAnnotation ? this.finishNode(typeNode, "TypeAnnotation") : null;
        }

        _superClass.prototype.parseFunctionBodyAndFinish.call(this, node, type, allowExpressionBody);
      }; // interfaces


      _proto.parseStatement = function parseStatement(declaration, topLevel) {
        // strict mode handling of `interface` since it's a reserved word
        if (this.state.strict && this.match(types.name) && this.state.value === "interface") {
          var node = this.startNode();
          this.next();
          return this.flowParseInterface(node);
        } else {
          return _superClass.prototype.parseStatement.call(this, declaration, topLevel);
        }
      }; // declares, interfaces and type aliases


      _proto.parseExpressionStatement = function parseExpressionStatement(node, expr) {
        if (expr.type === "Identifier") {
          if (expr.name === "declare") {
            if (this.match(types._class) || this.match(types.name) || this.match(types._function) || this.match(types._var) || this.match(types._export)) {
              return this.flowParseDeclare(node);
            }
          } else if (this.match(types.name)) {
            if (expr.name === "interface") {
              return this.flowParseInterface(node);
            } else if (expr.name === "type") {
              return this.flowParseTypeAlias(node);
            } else if (expr.name === "opaque") {
              return this.flowParseOpaqueType(node, false);
            }
          }
        }

        return _superClass.prototype.parseExpressionStatement.call(this, node, expr);
      }; // export type


      _proto.shouldParseExportDeclaration = function shouldParseExportDeclaration() {
        return this.isContextual("type") || this.isContextual("interface") || this.isContextual("opaque") || _superClass.prototype.shouldParseExportDeclaration.call(this);
      };

      _proto.isExportDefaultSpecifier = function isExportDefaultSpecifier() {
        if (this.match(types.name) && (this.state.value === "type" || this.state.value === "interface" || this.state.value == "opaque")) {
          return false;
        }

        return _superClass.prototype.isExportDefaultSpecifier.call(this);
      };

      _proto.parseConditional = function parseConditional(expr, noIn, startPos, startLoc, refNeedsArrowPos) {
        var _this3 = this;

        if (!this.match(types.question)) return expr; // only do the expensive clone if there is a question mark
        // and if we come from inside parens

        if (refNeedsArrowPos) {
          var _state = this.state.clone();

          try {
            return _superClass.prototype.parseConditional.call(this, expr, noIn, startPos, startLoc);
          } catch (err) {
            if (err instanceof SyntaxError) {
              this.state = _state;
              refNeedsArrowPos.start = err.pos || this.state.start;
              return expr;
            } else {
              // istanbul ignore next: no such error is expected
              throw err;
            }
          }
        }

        this.expect(types.question);
        var state = this.state.clone();
        var originalNoArrowAt = this.state.noArrowAt;
        var node = this.startNodeAt(startPos, startLoc);

        var _tryParseConditionalC = this.tryParseConditionalConsequent(),
            consequent = _tryParseConditionalC.consequent,
            failed = _tryParseConditionalC.failed;

        var _getArrowLikeExpressi = this.getArrowLikeExpressions(consequent),
            valid = _getArrowLikeExpressi[0],
            invalid = _getArrowLikeExpressi[1];

        if (failed || invalid.length > 0) {
          var noArrowAt = originalNoArrowAt.concat();

          if (invalid.length > 0) {
            this.state = state;
            this.state.noArrowAt = noArrowAt;

            for (var i = 0; i < invalid.length; i++) {
              noArrowAt.push(invalid[i].start);
            }

            var _tryParseConditionalC2 = this.tryParseConditionalConsequent();

            consequent = _tryParseConditionalC2.consequent;
            failed = _tryParseConditionalC2.failed;

            var _getArrowLikeExpressi2 = this.getArrowLikeExpressions(consequent);

            valid = _getArrowLikeExpressi2[0];
            invalid = _getArrowLikeExpressi2[1];
          }

          if (failed && valid.length > 1) {
            // if there are two or more possible correct ways of parsing, throw an
            // error.
            // e.g.   Source: a ? (b): c => (d): e => f
            //      Result 1: a ? b : (c => ((d): e => f))
            //      Result 2: a ? ((b): c => d) : (e => f)
            this.raise(state.start, "Ambiguous expression: wrap the arrow functions in parentheses to disambiguate.");
          }

          if (failed && valid.length === 1) {
            this.state = state;
            this.state.noArrowAt = noArrowAt.concat(valid[0].start);

            var _tryParseConditionalC3 = this.tryParseConditionalConsequent();

            consequent = _tryParseConditionalC3.consequent;
            failed = _tryParseConditionalC3.failed;
          }

          this.getArrowLikeExpressions(consequent, true);
        }

        this.state.noArrowAt = originalNoArrowAt;
        this.expect(types.colon);
        node.test = expr;
        node.consequent = consequent;
        node.alternate = this.forwardNoArrowParamsConversionAt(node, function () {
          return _this3.parseMaybeAssign(noIn, undefined, undefined, undefined);
        });
        return this.finishNode(node, "ConditionalExpression");
      };

      _proto.tryParseConditionalConsequent = function tryParseConditionalConsequent() {
        this.state.noArrowParamsConversionAt.push(this.state.start);
        var consequent = this.parseMaybeAssign();
        var failed = !this.match(types.colon);
        this.state.noArrowParamsConversionAt.pop();
        return {
          consequent,
          failed
        };
      }; // Given an expression, walks throught its arrow functions whose body is
      // an expression and throught conditional expressions. It returns every
      // function which has been parsed with a return type but could have been
      // parenthesized expressions.
      // These functions are separated into two arrays: one containing the ones
      // whose parameters can be converted to assignable lists, one containing the
      // others.


      _proto.getArrowLikeExpressions = function getArrowLikeExpressions(node, disallowInvalid) {
        var _this4 = this;

        var stack = [node];
        var arrows = [];

        while (stack.length !== 0) {
          var _node = stack.pop();

          if (_node.type === "ArrowFunctionExpression") {
            if (_node.typeParameters || !_node.returnType) {
              // This is an arrow expression without ambiguity, so check its parameters
              this.toAssignableList( // node.params is Expression[] instead of $ReadOnlyArray<Pattern> because it
              // has not been converted yet.
              _node.params, true, "arrow function parameters"); // Use super's method to force the parameters to be checked

              _superClass.prototype.checkFunctionNameAndParams.call(this, _node, true);
            } else {
              arrows.push(_node);
            }

            stack.push(_node.body);
          } else if (_node.type === "ConditionalExpression") {
            stack.push(_node.consequent);
            stack.push(_node.alternate);
          }
        }

        if (disallowInvalid) {
          for (var i = 0; i < arrows.length; i++) {
            this.toAssignableList(node.params, true, "arrow function parameters");
          }

          return [arrows, []];
        }

        return partition(arrows, function (node) {
          try {
            _this4.toAssignableList(node.params, true, "arrow function parameters");

            return true;
          } catch (err) {
            return false;
          }
        });
      };

      _proto.forwardNoArrowParamsConversionAt = function forwardNoArrowParamsConversionAt(node, parse) {
        var result;

        if (this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
          this.state.noArrowParamsConversionAt.push(this.state.start);
          result = parse();
          this.state.noArrowParamsConversionAt.pop();
        } else {
          result = parse();
        }

        return result;
      };

      _proto.parseParenItem = function parseParenItem(node, startPos, startLoc) {
        node = _superClass.prototype.parseParenItem.call(this, node, startPos, startLoc);

        if (this.eat(types.question)) {
          node.optional = true;
        }

        if (this.match(types.colon)) {
          var typeCastNode = this.startNodeAt(startPos, startLoc);
          typeCastNode.expression = node;
          typeCastNode.typeAnnotation = this.flowParseTypeAnnotation();
          return this.finishNode(typeCastNode, "TypeCastExpression");
        }

        return node;
      };

      _proto.assertModuleNodeAllowed = function assertModuleNodeAllowed(node) {
        if (node.type === "ImportDeclaration" && (node.importKind === "type" || node.importKind === "typeof") || node.type === "ExportNamedDeclaration" && node.exportKind === "type" || node.type === "ExportAllDeclaration" && node.exportKind === "type") {
          // Allow Flowtype imports and exports in all conditions because
          // Flow itself does not care about 'sourceType'.
          return;
        }

        _superClass.prototype.assertModuleNodeAllowed.call(this, node);
      };

      _proto.parseExport = function parseExport(node) {
        node = _superClass.prototype.parseExport.call(this, node);

        if (node.type === "ExportNamedDeclaration" || node.type === "ExportAllDeclaration") {
          node.exportKind = node.exportKind || "value";
        }

        return node;
      };

      _proto.parseExportDeclaration = function parseExportDeclaration(node) {
        if (this.isContextual("type")) {
          node.exportKind = "type";
          var declarationNode = this.startNode();
          this.next();

          if (this.match(types.braceL)) {
            // export type { foo, bar };
            node.specifiers = this.parseExportSpecifiers();
            this.parseExportFrom(node);
            return null;
          } else {
            // export type Foo = Bar;
            return this.flowParseTypeAlias(declarationNode);
          }
        } else if (this.isContextual("opaque")) {
          node.exportKind = "type";

          var _declarationNode = this.startNode();

          this.next(); // export opaque type Foo = Bar;

          return this.flowParseOpaqueType(_declarationNode, false);
        } else if (this.isContextual("interface")) {
          node.exportKind = "type";

          var _declarationNode2 = this.startNode();

          this.next();
          return this.flowParseInterface(_declarationNode2);
        } else {
          return _superClass.prototype.parseExportDeclaration.call(this, node);
        }
      };

      _proto.shouldParseExportStar = function shouldParseExportStar() {
        return _superClass.prototype.shouldParseExportStar.call(this) || this.isContextual("type") && this.lookahead().type === types.star;
      };

      _proto.parseExportStar = function parseExportStar(node) {
        if (this.eatContextual("type")) {
          node.exportKind = "type";
        }

        return _superClass.prototype.parseExportStar.call(this, node);
      };

      _proto.parseExportNamespace = function parseExportNamespace(node) {
        if (node.exportKind === "type") {
          this.unexpected();
        }

        return _superClass.prototype.parseExportNamespace.call(this, node);
      };

      _proto.parseClassId = function parseClassId(node, isStatement, optionalId) {
        _superClass.prototype.parseClassId.call(this, node, isStatement, optionalId);

        if (this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterDeclaration();
        }
      }; // don't consider `void` to be a keyword as then it'll use the void token type
      // and set startExpr


      _proto.isKeyword = function isKeyword$$1(name) {
        if (this.state.inType && name === "void") {
          return false;
        } else {
          return _superClass.prototype.isKeyword.call(this, name);
        }
      }; // ensure that inside flow types, we bypass the jsx parser plugin


      _proto.readToken = function readToken(code) {
        var next = this.input.charCodeAt(this.state.pos + 1);

        if (this.state.inType && (code === 62 || code === 60)) {
          return this.finishOp(types.relational, 1);
        } else if (isIteratorStart(code, next)) {
          this.state.isIterator = true;
          return _superClass.prototype.readWord.call(this, code);
        } else {
          return _superClass.prototype.readToken.call(this, code);
        }
      };

      _proto.toAssignable = function toAssignable(node, isBinding, contextDescription) {
        if (node.type === "TypeCastExpression") {
          return _superClass.prototype.toAssignable.call(this, this.typeCastToParameter(node), isBinding, contextDescription);
        } else {
          return _superClass.prototype.toAssignable.call(this, node, isBinding, contextDescription);
        }
      }; // turn type casts that we found in function parameter head into type annotated params


      _proto.toAssignableList = function toAssignableList(exprList, isBinding, contextDescription) {
        for (var i = 0; i < exprList.length; i++) {
          var expr = exprList[i];

          if (expr && expr.type === "TypeCastExpression") {
            exprList[i] = this.typeCastToParameter(expr);
          }
        }

        return _superClass.prototype.toAssignableList.call(this, exprList, isBinding, contextDescription);
      }; // this is a list of nodes, from something like a call expression, we need to filter the
      // type casts that we've found that are illegal in this context


      _proto.toReferencedList = function toReferencedList(exprList) {
        for (var i = 0; i < exprList.length; i++) {
          var expr = exprList[i];

          if (expr && expr._exprListItem && expr.type === "TypeCastExpression") {
            this.raise(expr.start, "Unexpected type cast");
          }
        }

        return exprList;
      }; // parse an item inside a expression list eg. `(NODE, NODE)` where NODE represents
      // the position where this function is called


      _proto.parseExprListItem = function parseExprListItem(allowEmpty, refShorthandDefaultPos, refNeedsArrowPos) {
        var container = this.startNode();

        var node = _superClass.prototype.parseExprListItem.call(this, allowEmpty, refShorthandDefaultPos, refNeedsArrowPos);

        if (this.match(types.colon)) {
          container._exprListItem = true;
          container.expression = node;
          container.typeAnnotation = this.flowParseTypeAnnotation();
          return this.finishNode(container, "TypeCastExpression");
        } else {
          return node;
        }
      };

      _proto.checkLVal = function checkLVal(expr, isBinding, checkClashes, contextDescription) {
        if (expr.type !== "TypeCastExpression") {
          return _superClass.prototype.checkLVal.call(this, expr, isBinding, checkClashes, contextDescription);
        }
      }; // parse class property type annotations


      _proto.parseClassProperty = function parseClassProperty(node) {
        if (this.match(types.colon)) {
          node.typeAnnotation = this.flowParseTypeAnnotation();
        }

        return _superClass.prototype.parseClassProperty.call(this, node);
      }; // determine whether or not we're currently in the position where a class method would appear


      _proto.isClassMethod = function isClassMethod() {
        return this.isRelational("<") || _superClass.prototype.isClassMethod.call(this);
      }; // determine whether or not we're currently in the position where a class property would appear


      _proto.isClassProperty = function isClassProperty() {
        return this.match(types.colon) || _superClass.prototype.isClassProperty.call(this);
      };

      _proto.isNonstaticConstructor = function isNonstaticConstructor(method) {
        return !this.match(types.colon) && _superClass.prototype.isNonstaticConstructor.call(this, method);
      }; // parse type parameters for class methods


      _proto.pushClassMethod = function pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor) {
        if (method.variance) {
          this.unexpected(method.variance.start);
        }

        delete method.variance;

        if (this.isRelational("<")) {
          method.typeParameters = this.flowParseTypeParameterDeclaration();
        }

        _superClass.prototype.pushClassMethod.call(this, classBody, method, isGenerator, isAsync, isConstructor);
      };

      _proto.pushClassPrivateMethod = function pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
        if (method.variance) {
          this.unexpected(method.variance.start);
        }

        delete method.variance;

        if (this.isRelational("<")) {
          method.typeParameters = this.flowParseTypeParameterDeclaration();
        }

        _superClass.prototype.pushClassPrivateMethod.call(this, classBody, method, isGenerator, isAsync);
      }; // parse a the super class type parameters and implements


      _proto.parseClassSuper = function parseClassSuper(node) {
        _superClass.prototype.parseClassSuper.call(this, node);

        if (node.superClass && this.isRelational("<")) {
          node.superTypeParameters = this.flowParseTypeParameterInstantiation();
        }

        if (this.isContextual("implements")) {
          this.next();
          var implemented = node.implements = [];

          do {
            var _node2 = this.startNode();

            _node2.id = this.flowParseRestrictedIdentifier(
            /*liberal*/
            true);

            if (this.isRelational("<")) {
              _node2.typeParameters = this.flowParseTypeParameterInstantiation();
            } else {
              _node2.typeParameters = null;
            }

            implemented.push(this.finishNode(_node2, "ClassImplements"));
          } while (this.eat(types.comma));
        }
      };

      _proto.parsePropertyName = function parsePropertyName(node) {
        var variance = this.flowParseVariance();

        var key = _superClass.prototype.parsePropertyName.call(this, node); // $FlowIgnore ("variance" not defined on TsNamedTypeElementBase)


        node.variance = variance;
        return key;
      }; // parse type parameters for object method shorthand


      _proto.parseObjPropValue = function parseObjPropValue(prop, startPos, startLoc, isGenerator, isAsync, isPattern, refShorthandDefaultPos) {
        if (prop.variance) {
          this.unexpected(prop.variance.start);
        }

        delete prop.variance;
        var typeParameters; // method shorthand

        if (this.isRelational("<")) {
          typeParameters = this.flowParseTypeParameterDeclaration();
          if (!this.match(types.parenL)) this.unexpected();
        }

        _superClass.prototype.parseObjPropValue.call(this, prop, startPos, startLoc, isGenerator, isAsync, isPattern, refShorthandDefaultPos); // add typeParameters if we found them


        if (typeParameters) {
          // $FlowFixMe (trying to set '.typeParameters' on an expression)
          (prop.value || prop).typeParameters = typeParameters;
        }
      };

      _proto.parseAssignableListItemTypes = function parseAssignableListItemTypes(param) {
        if (this.eat(types.question)) {
          if (param.type !== "Identifier") {
            throw this.raise(param.start, "A binding pattern parameter cannot be optional in an implementation signature.");
          }

          param.optional = true;
        }

        if (this.match(types.colon)) {
          param.typeAnnotation = this.flowParseTypeAnnotation();
        }

        this.finishNode(param, param.type);
        return param;
      };

      _proto.parseMaybeDefault = function parseMaybeDefault(startPos, startLoc, left) {
        var node = _superClass.prototype.parseMaybeDefault.call(this, startPos, startLoc, left);

        if (node.type === "AssignmentPattern" && node.typeAnnotation && node.right.start < node.typeAnnotation.start) {
          this.raise(node.typeAnnotation.start, "Type annotations must come before default assignments, e.g. instead of `age = 25: number` use `age: number = 25`");
        }

        return node;
      };

      _proto.shouldParseDefaultImport = function shouldParseDefaultImport(node) {
        if (!hasTypeImportKind(node)) {
          return _superClass.prototype.shouldParseDefaultImport.call(this, node);
        }

        return isMaybeDefaultImport(this.state);
      };

      _proto.parseImportSpecifierLocal = function parseImportSpecifierLocal(node, specifier, type, contextDescription) {
        specifier.local = hasTypeImportKind(node) ? this.flowParseRestrictedIdentifier(true) : this.parseIdentifier();
        this.checkLVal(specifier.local, true, undefined, contextDescription);
        node.specifiers.push(this.finishNode(specifier, type));
      }; // parse typeof and type imports


      _proto.parseImportSpecifiers = function parseImportSpecifiers(node) {
        node.importKind = "value";
        var kind = null;

        if (this.match(types._typeof)) {
          kind = "typeof";
        } else if (this.isContextual("type")) {
          kind = "type";
        }

        if (kind) {
          var lh = this.lookahead(); // import type * is not allowed

          if (kind === "type" && lh.type === types.star) {
            this.unexpected(lh.start);
          }

          if (isMaybeDefaultImport(lh) || lh.type === types.braceL || lh.type === types.star) {
            this.next();
            node.importKind = kind;
          }
        }

        _superClass.prototype.parseImportSpecifiers.call(this, node);
      }; // parse import-type/typeof shorthand


      _proto.parseImportSpecifier = function parseImportSpecifier(node) {
        var specifier = this.startNode();
        var firstIdentLoc = this.state.start;
        var firstIdent = this.parseIdentifier(true);
        var specifierTypeKind = null;

        if (firstIdent.name === "type") {
          specifierTypeKind = "type";
        } else if (firstIdent.name === "typeof") {
          specifierTypeKind = "typeof";
        }

        var isBinding = false;

        if (this.isContextual("as") && !this.isLookaheadContextual("as")) {
          var as_ident = this.parseIdentifier(true);

          if (specifierTypeKind !== null && !this.match(types.name) && !this.state.type.keyword) {
            // `import {type as ,` or `import {type as }`
            specifier.imported = as_ident;
            specifier.importKind = specifierTypeKind;
            specifier.local = as_ident.__clone();
          } else {
            // `import {type as foo`
            specifier.imported = firstIdent;
            specifier.importKind = null;
            specifier.local = this.parseIdentifier();
          }
        } else if (specifierTypeKind !== null && (this.match(types.name) || this.state.type.keyword)) {
          // `import {type foo`
          specifier.imported = this.parseIdentifier(true);
          specifier.importKind = specifierTypeKind;

          if (this.eatContextual("as")) {
            specifier.local = this.parseIdentifier();
          } else {
            isBinding = true;
            specifier.local = specifier.imported.__clone();
          }
        } else {
          isBinding = true;
          specifier.imported = firstIdent;
          specifier.importKind = null;
          specifier.local = specifier.imported.__clone();
        }

        var nodeIsTypeImport = hasTypeImportKind(node);
        var specifierIsTypeImport = hasTypeImportKind(specifier);

        if (nodeIsTypeImport && specifierIsTypeImport) {
          this.raise(firstIdentLoc, "The `type` and `typeof` keywords on named imports can only be used on regular `import` statements. It cannot be used with `import type` or `import typeof` statements");
        }

        if (nodeIsTypeImport || specifierIsTypeImport) {
          this.checkReservedType(specifier.local.name, specifier.local.start);
        }

        if (isBinding && !nodeIsTypeImport && !specifierIsTypeImport) {
          this.checkReservedWord(specifier.local.name, specifier.start, true, true);
        }

        this.checkLVal(specifier.local, true, undefined, "import specifier");
        node.specifiers.push(this.finishNode(specifier, "ImportSpecifier"));
      }; // parse function type parameters - function foo<T>() {}


      _proto.parseFunctionParams = function parseFunctionParams(node) {
        // $FlowFixMe
        var kind = node.kind;

        if (kind !== "get" && kind !== "set" && this.isRelational("<")) {
          node.typeParameters = this.flowParseTypeParameterDeclaration();
        }

        _superClass.prototype.parseFunctionParams.call(this, node);
      }; // parse flow type annotations on variable declarator heads - let foo: string = bar


      _proto.parseVarHead = function parseVarHead(decl) {
        _superClass.prototype.parseVarHead.call(this, decl);

        if (this.match(types.colon)) {
          decl.id.typeAnnotation = this.flowParseTypeAnnotation();
          this.finishNode(decl.id, decl.id.type);
        }
      }; // parse the return type of an async arrow function - let foo = (async (): number => {});


      _proto.parseAsyncArrowFromCallExpression = function parseAsyncArrowFromCallExpression(node, call) {
        if (this.match(types.colon)) {
          var oldNoAnonFunctionType = this.state.noAnonFunctionType;
          this.state.noAnonFunctionType = true;
          node.returnType = this.flowParseTypeAnnotation();
          this.state.noAnonFunctionType = oldNoAnonFunctionType;
        }

        return _superClass.prototype.parseAsyncArrowFromCallExpression.call(this, node, call);
      }; // todo description


      _proto.shouldParseAsyncArrow = function shouldParseAsyncArrow() {
        return this.match(types.colon) || _superClass.prototype.shouldParseAsyncArrow.call(this);
      }; // We need to support type parameter declarations for arrow functions. This
      // is tricky. There are three situations we need to handle
      //
      // 1. This is either JSX or an arrow function. We'll try JSX first. If that
      //    fails, we'll try an arrow function. If that fails, we'll throw the JSX
      //    error.
      // 2. This is an arrow function. We'll parse the type parameter declaration,
      //    parse the rest, make sure the rest is an arrow function, and go from
      //    there
      // 3. This is neither. Just call the super method


      _proto.parseMaybeAssign = function parseMaybeAssign(noIn, refShorthandDefaultPos, afterLeftParse, refNeedsArrowPos) {
        var _this5 = this;

        var jsxError = null;

        if (types.jsxTagStart && this.match(types.jsxTagStart)) {
          var state = this.state.clone();

          try {
            return _superClass.prototype.parseMaybeAssign.call(this, noIn, refShorthandDefaultPos, afterLeftParse, refNeedsArrowPos);
          } catch (err) {
            if (err instanceof SyntaxError) {
              this.state = state; // Remove `tc.j_expr` and `tc.j_oTag` from context added
              // by parsing `jsxTagStart` to stop the JSX plugin from
              // messing with the tokens

              this.state.context.length -= 2;
              jsxError = err;
            } else {
              // istanbul ignore next: no such error is expected
              throw err;
            }
          }
        }

        if (jsxError != null || this.isRelational("<")) {
          var arrowExpression;
          var typeParameters;

          try {
            typeParameters = this.flowParseTypeParameterDeclaration();
            arrowExpression = this.forwardNoArrowParamsConversionAt(typeParameters, function () {
              return _superClass.prototype.parseMaybeAssign.call(_this5, noIn, refShorthandDefaultPos, afterLeftParse, refNeedsArrowPos);
            });
            arrowExpression.typeParameters = typeParameters;
            this.resetStartLocationFromNode(arrowExpression, typeParameters);
          } catch (err) {
            throw jsxError || err;
          }

          if (arrowExpression.type === "ArrowFunctionExpression") {
            return arrowExpression;
          } else if (jsxError != null) {
            throw jsxError;
          } else {
            this.raise(typeParameters.start, "Expected an arrow function after this type parameter declaration");
          }
        }

        return _superClass.prototype.parseMaybeAssign.call(this, noIn, refShorthandDefaultPos, afterLeftParse, refNeedsArrowPos);
      }; // handle return types for arrow functions


      _proto.parseArrow = function parseArrow(node) {
        if (this.match(types.colon)) {
          var state = this.state.clone();

          try {
            var oldNoAnonFunctionType = this.state.noAnonFunctionType;
            this.state.noAnonFunctionType = true;
            var typeNode = this.startNode();

            var _flowParseTypeAndPred3 = this.flowParseTypeAndPredicateInitialiser();

            // $FlowFixMe (destructuring not supported yet)
            typeNode.typeAnnotation = _flowParseTypeAndPred3[0];
            // $FlowFixMe (destructuring not supported yet)
            node.predicate = _flowParseTypeAndPred3[1];
            this.state.noAnonFunctionType = oldNoAnonFunctionType;
            if (this.canInsertSemicolon()) this.unexpected();
            if (!this.match(types.arrow)) this.unexpected(); // assign after it is clear it is an arrow

            node.returnType = typeNode.typeAnnotation ? this.finishNode(typeNode, "TypeAnnotation") : null;
          } catch (err) {
            if (err instanceof SyntaxError) {
              this.state = state;
            } else {
              // istanbul ignore next: no such error is expected
              throw err;
            }
          }
        }

        return _superClass.prototype.parseArrow.call(this, node);
      };

      _proto.shouldParseArrow = function shouldParseArrow() {
        return this.match(types.colon) || _superClass.prototype.shouldParseArrow.call(this);
      };

      _proto.setArrowFunctionParameters = function setArrowFunctionParameters(node, params) {
        if (this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
          node.params = params;
        } else {
          _superClass.prototype.setArrowFunctionParameters.call(this, node, params);
        }
      };

      _proto.checkFunctionNameAndParams = function checkFunctionNameAndParams(node, isArrowFunction) {
        if (isArrowFunction && this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
          return;
        }

        return _superClass.prototype.checkFunctionNameAndParams.call(this, node, isArrowFunction);
      };

      _proto.parseParenAndDistinguishExpression = function parseParenAndDistinguishExpression(canBeArrow) {
        return _superClass.prototype.parseParenAndDistinguishExpression.call(this, canBeArrow && this.state.noArrowAt.indexOf(this.state.start) === -1);
      };

      _proto.parseSubscripts = function parseSubscripts(base, startPos, startLoc, noCalls) {
        if (base.type === "Identifier" && base.name === "async" && this.state.noArrowAt.indexOf(startPos) !== -1) {
          this.next();
          var node = this.startNodeAt(startPos, startLoc);
          node.callee = base;
          node.arguments = this.parseCallExpressionArguments(types.parenR, false);
          base = this.finishNode(node, "CallExpression");
        } else if (base.type === "Identifier" && base.name === "async" && this.isRelational("<")) {
          var state = this.state.clone();
          var error;

          try {
            var _node3 = this.parseAsyncArrowWithTypeParameters(startPos, startLoc);

            if (_node3) return _node3;
          } catch (e) {
            error = e;
          }

          this.state = state;

          try {
            return _superClass.prototype.parseSubscripts.call(this, base, startPos, startLoc, noCalls);
          } catch (e) {
            throw error || e;
          }
        }

        return _superClass.prototype.parseSubscripts.call(this, base, startPos, startLoc, noCalls);
      };

      _proto.parseAsyncArrowWithTypeParameters = function parseAsyncArrowWithTypeParameters(startPos, startLoc) {
        var node = this.startNodeAt(startPos, startLoc);
        this.parseFunctionParams(node);
        if (!this.parseArrow(node)) return;
        return this.parseArrowExpression(node,
        /* params */
        undefined,
        /* isAsync */
        true);
      };

      _proto.readToken_mult_modulo = function readToken_mult_modulo(code) {
        var next = this.input.charCodeAt(this.state.pos + 1);

        if (code === 42 && next === 47 && this.state.hasFlowComment) {
          this.state.hasFlowComment = false;
          this.state.pos += 2;
          this.nextToken();
          return;
        }

        _superClass.prototype.readToken_mult_modulo.call(this, code);
      };

      _proto.skipBlockComment = function skipBlockComment() {
        if (this.hasPlugin("flow") && this.hasPlugin("flowComments") && this.skipFlowComment()) {
          this.hasFlowCommentCompletion();
          this.state.pos += this.skipFlowComment();
          this.state.hasFlowComment = true;
          return;
        }

        var end;

        if (this.hasPlugin("flow") && this.state.hasFlowComment) {
          end = this.input.indexOf("*-/", this.state.pos += 2);
          if (end === -1) this.raise(this.state.pos - 2, "Unterminated comment");
          this.state.pos = end + 3;
          return;
        }

        _superClass.prototype.skipBlockComment.call(this);
      };

      _proto.skipFlowComment = function skipFlowComment() {
        var ch2 = this.input.charCodeAt(this.state.pos + 2);
        var ch3 = this.input.charCodeAt(this.state.pos + 3);

        if (ch2 === 58 && ch3 === 58) {
          return 4; // check for /*::
        }

        if (this.input.slice(this.state.pos + 2, 14) === "flow-include") {
          return 14; // check for /*flow-include
        }

        if (ch2 === 58 && ch3 !== 58 && 2) {
          return 2; // check for /*:, advance only 2 steps
        }

        return false;
      };

      _proto.hasFlowCommentCompletion = function hasFlowCommentCompletion() {
        var end = this.input.indexOf("*/", this.state.pos);

        if (end === -1) {
          this.raise(this.state.pos, "Unterminated comment");
        }
      };

      return _class;
    }(superClass)
  );
});

var entities = {
  quot: "\u0022",
  amp: "&",
  apos: "\u0027",
  lt: "<",
  gt: ">",
  nbsp: "\u00A0",
  iexcl: "\u00A1",
  cent: "\u00A2",
  pound: "\u00A3",
  curren: "\u00A4",
  yen: "\u00A5",
  brvbar: "\u00A6",
  sect: "\u00A7",
  uml: "\u00A8",
  copy: "\u00A9",
  ordf: "\u00AA",
  laquo: "\u00AB",
  not: "\u00AC",
  shy: "\u00AD",
  reg: "\u00AE",
  macr: "\u00AF",
  deg: "\u00B0",
  plusmn: "\u00B1",
  sup2: "\u00B2",
  sup3: "\u00B3",
  acute: "\u00B4",
  micro: "\u00B5",
  para: "\u00B6",
  middot: "\u00B7",
  cedil: "\u00B8",
  sup1: "\u00B9",
  ordm: "\u00BA",
  raquo: "\u00BB",
  frac14: "\u00BC",
  frac12: "\u00BD",
  frac34: "\u00BE",
  iquest: "\u00BF",
  Agrave: "\u00C0",
  Aacute: "\u00C1",
  Acirc: "\u00C2",
  Atilde: "\u00C3",
  Auml: "\u00C4",
  Aring: "\u00C5",
  AElig: "\u00C6",
  Ccedil: "\u00C7",
  Egrave: "\u00C8",
  Eacute: "\u00C9",
  Ecirc: "\u00CA",
  Euml: "\u00CB",
  Igrave: "\u00CC",
  Iacute: "\u00CD",
  Icirc: "\u00CE",
  Iuml: "\u00CF",
  ETH: "\u00D0",
  Ntilde: "\u00D1",
  Ograve: "\u00D2",
  Oacute: "\u00D3",
  Ocirc: "\u00D4",
  Otilde: "\u00D5",
  Ouml: "\u00D6",
  times: "\u00D7",
  Oslash: "\u00D8",
  Ugrave: "\u00D9",
  Uacute: "\u00DA",
  Ucirc: "\u00DB",
  Uuml: "\u00DC",
  Yacute: "\u00DD",
  THORN: "\u00DE",
  szlig: "\u00DF",
  agrave: "\u00E0",
  aacute: "\u00E1",
  acirc: "\u00E2",
  atilde: "\u00E3",
  auml: "\u00E4",
  aring: "\u00E5",
  aelig: "\u00E6",
  ccedil: "\u00E7",
  egrave: "\u00E8",
  eacute: "\u00E9",
  ecirc: "\u00EA",
  euml: "\u00EB",
  igrave: "\u00EC",
  iacute: "\u00ED",
  icirc: "\u00EE",
  iuml: "\u00EF",
  eth: "\u00F0",
  ntilde: "\u00F1",
  ograve: "\u00F2",
  oacute: "\u00F3",
  ocirc: "\u00F4",
  otilde: "\u00F5",
  ouml: "\u00F6",
  divide: "\u00F7",
  oslash: "\u00F8",
  ugrave: "\u00F9",
  uacute: "\u00FA",
  ucirc: "\u00FB",
  uuml: "\u00FC",
  yacute: "\u00FD",
  thorn: "\u00FE",
  yuml: "\u00FF",
  OElig: "\u0152",
  oelig: "\u0153",
  Scaron: "\u0160",
  scaron: "\u0161",
  Yuml: "\u0178",
  fnof: "\u0192",
  circ: "\u02C6",
  tilde: "\u02DC",
  Alpha: "\u0391",
  Beta: "\u0392",
  Gamma: "\u0393",
  Delta: "\u0394",
  Epsilon: "\u0395",
  Zeta: "\u0396",
  Eta: "\u0397",
  Theta: "\u0398",
  Iota: "\u0399",
  Kappa: "\u039A",
  Lambda: "\u039B",
  Mu: "\u039C",
  Nu: "\u039D",
  Xi: "\u039E",
  Omicron: "\u039F",
  Pi: "\u03A0",
  Rho: "\u03A1",
  Sigma: "\u03A3",
  Tau: "\u03A4",
  Upsilon: "\u03A5",
  Phi: "\u03A6",
  Chi: "\u03A7",
  Psi: "\u03A8",
  Omega: "\u03A9",
  alpha: "\u03B1",
  beta: "\u03B2",
  gamma: "\u03B3",
  delta: "\u03B4",
  epsilon: "\u03B5",
  zeta: "\u03B6",
  eta: "\u03B7",
  theta: "\u03B8",
  iota: "\u03B9",
  kappa: "\u03BA",
  lambda: "\u03BB",
  mu: "\u03BC",
  nu: "\u03BD",
  xi: "\u03BE",
  omicron: "\u03BF",
  pi: "\u03C0",
  rho: "\u03C1",
  sigmaf: "\u03C2",
  sigma: "\u03C3",
  tau: "\u03C4",
  upsilon: "\u03C5",
  phi: "\u03C6",
  chi: "\u03C7",
  psi: "\u03C8",
  omega: "\u03C9",
  thetasym: "\u03D1",
  upsih: "\u03D2",
  piv: "\u03D6",
  ensp: "\u2002",
  emsp: "\u2003",
  thinsp: "\u2009",
  zwnj: "\u200C",
  zwj: "\u200D",
  lrm: "\u200E",
  rlm: "\u200F",
  ndash: "\u2013",
  mdash: "\u2014",
  lsquo: "\u2018",
  rsquo: "\u2019",
  sbquo: "\u201A",
  ldquo: "\u201C",
  rdquo: "\u201D",
  bdquo: "\u201E",
  dagger: "\u2020",
  Dagger: "\u2021",
  bull: "\u2022",
  hellip: "\u2026",
  permil: "\u2030",
  prime: "\u2032",
  Prime: "\u2033",
  lsaquo: "\u2039",
  rsaquo: "\u203A",
  oline: "\u203E",
  frasl: "\u2044",
  euro: "\u20AC",
  image: "\u2111",
  weierp: "\u2118",
  real: "\u211C",
  trade: "\u2122",
  alefsym: "\u2135",
  larr: "\u2190",
  uarr: "\u2191",
  rarr: "\u2192",
  darr: "\u2193",
  harr: "\u2194",
  crarr: "\u21B5",
  lArr: "\u21D0",
  uArr: "\u21D1",
  rArr: "\u21D2",
  dArr: "\u21D3",
  hArr: "\u21D4",
  forall: "\u2200",
  part: "\u2202",
  exist: "\u2203",
  empty: "\u2205",
  nabla: "\u2207",
  isin: "\u2208",
  notin: "\u2209",
  ni: "\u220B",
  prod: "\u220F",
  sum: "\u2211",
  minus: "\u2212",
  lowast: "\u2217",
  radic: "\u221A",
  prop: "\u221D",
  infin: "\u221E",
  ang: "\u2220",
  and: "\u2227",
  or: "\u2228",
  cap: "\u2229",
  cup: "\u222A",
  int: "\u222B",
  there4: "\u2234",
  sim: "\u223C",
  cong: "\u2245",
  asymp: "\u2248",
  ne: "\u2260",
  equiv: "\u2261",
  le: "\u2264",
  ge: "\u2265",
  sub: "\u2282",
  sup: "\u2283",
  nsub: "\u2284",
  sube: "\u2286",
  supe: "\u2287",
  oplus: "\u2295",
  otimes: "\u2297",
  perp: "\u22A5",
  sdot: "\u22C5",
  lceil: "\u2308",
  rceil: "\u2309",
  lfloor: "\u230A",
  rfloor: "\u230B",
  lang: "\u2329",
  rang: "\u232A",
  loz: "\u25CA",
  spades: "\u2660",
  clubs: "\u2663",
  hearts: "\u2665",
  diams: "\u2666"
};

var HEX_NUMBER = /^[\da-fA-F]+$/;
var DECIMAL_NUMBER = /^\d+$/;
types$1.j_oTag = new TokContext("<tag", false);
types$1.j_cTag = new TokContext("</tag", false);
types$1.j_expr = new TokContext("<tag>...</tag>", true, true);
types.jsxName = new TokenType("jsxName");
types.jsxText = new TokenType("jsxText", {
  beforeExpr: true
});
types.jsxTagStart = new TokenType("jsxTagStart", {
  startsExpr: true
});
types.jsxTagEnd = new TokenType("jsxTagEnd");

types.jsxTagStart.updateContext = function () {
  this.state.context.push(types$1.j_expr); // treat as beginning of JSX expression

  this.state.context.push(types$1.j_oTag); // start opening tag context

  this.state.exprAllowed = false;
};

types.jsxTagEnd.updateContext = function (prevType) {
  var out = this.state.context.pop();

  if (out === types$1.j_oTag && prevType === types.slash || out === types$1.j_cTag) {
    this.state.context.pop();
    this.state.exprAllowed = this.curContext() === types$1.j_expr;
  } else {
    this.state.exprAllowed = true;
  }
};

function isFragment(object) {
  return object ? object.type === "JSXOpeningFragment" || object.type === "JSXClosingFragment" : false;
} // Transforms JSX element name to string.


function getQualifiedJSXName(object) {
  if (object.type === "JSXIdentifier") {
    return object.name;
  }

  if (object.type === "JSXNamespacedName") {
    return object.namespace.name + ":" + object.name.name;
  }

  if (object.type === "JSXMemberExpression") {
    return getQualifiedJSXName(object.object) + "." + getQualifiedJSXName(object.property);
  } // istanbul ignore next


  throw new Error("Node had unexpected type: " + object.type);
}

var jsxPlugin = (function (superClass) {
  return (
    /*#__PURE__*/
    function (_superClass) {
      _inheritsLoose(_class, _superClass);

      function _class() {
        return _superClass.apply(this, arguments) || this;
      }

      var _proto = _class.prototype;

      // Reads inline JSX contents token.
      _proto.jsxReadToken = function jsxReadToken() {
        var out = "";
        var chunkStart = this.state.pos;

        for (;;) {
          if (this.state.pos >= this.input.length) {
            this.raise(this.state.start, "Unterminated JSX contents");
          }

          var ch = this.input.charCodeAt(this.state.pos);

          switch (ch) {
            case 60:
            case 123:
              if (this.state.pos === this.state.start) {
                if (ch === 60 && this.state.exprAllowed) {
                  ++this.state.pos;
                  return this.finishToken(types.jsxTagStart);
                }

                return this.getTokenFromCode(ch);
              }

              out += this.input.slice(chunkStart, this.state.pos);
              return this.finishToken(types.jsxText, out);

            case 38:
              out += this.input.slice(chunkStart, this.state.pos);
              out += this.jsxReadEntity();
              chunkStart = this.state.pos;
              break;

            default:
              if (isNewLine(ch)) {
                out += this.input.slice(chunkStart, this.state.pos);
                out += this.jsxReadNewLine(true);
                chunkStart = this.state.pos;
              } else {
                ++this.state.pos;
              }

          }
        }
      };

      _proto.jsxReadNewLine = function jsxReadNewLine(normalizeCRLF) {
        var ch = this.input.charCodeAt(this.state.pos);
        var out;
        ++this.state.pos;

        if (ch === 13 && this.input.charCodeAt(this.state.pos) === 10) {
          ++this.state.pos;
          out = normalizeCRLF ? "\n" : "\r\n";
        } else {
          out = String.fromCharCode(ch);
        }

        ++this.state.curLine;
        this.state.lineStart = this.state.pos;
        return out;
      };

      _proto.jsxReadString = function jsxReadString(quote) {
        var out = "";
        var chunkStart = ++this.state.pos;

        for (;;) {
          if (this.state.pos >= this.input.length) {
            this.raise(this.state.start, "Unterminated string constant");
          }

          var ch = this.input.charCodeAt(this.state.pos);
          if (ch === quote) break;

          if (ch === 38) {
            out += this.input.slice(chunkStart, this.state.pos);
            out += this.jsxReadEntity();
            chunkStart = this.state.pos;
          } else if (isNewLine(ch)) {
            out += this.input.slice(chunkStart, this.state.pos);
            out += this.jsxReadNewLine(false);
            chunkStart = this.state.pos;
          } else {
            ++this.state.pos;
          }
        }

        out += this.input.slice(chunkStart, this.state.pos++);
        return this.finishToken(types.string, out);
      };

      _proto.jsxReadEntity = function jsxReadEntity() {
        var str = "";
        var count = 0;
        var entity;
        var ch = this.input[this.state.pos];
        var startPos = ++this.state.pos;

        while (this.state.pos < this.input.length && count++ < 10) {
          ch = this.input[this.state.pos++];

          if (ch === ";") {
            if (str[0] === "#") {
              if (str[1] === "x") {
                str = str.substr(2);

                if (HEX_NUMBER.test(str)) {
                  entity = String.fromCodePoint(parseInt(str, 16));
                }
              } else {
                str = str.substr(1);

                if (DECIMAL_NUMBER.test(str)) {
                  entity = String.fromCodePoint(parseInt(str, 10));
                }
              }
            } else {
              entity = entities[str];
            }

            break;
          }

          str += ch;
        }

        if (!entity) {
          this.state.pos = startPos;
          return "&";
        }

        return entity;
      }; // Read a JSX identifier (valid tag or attribute name).
      //
      // Optimized version since JSX identifiers can"t contain
      // escape characters and so can be read as single slice.
      // Also assumes that first character was already checked
      // by isIdentifierStart in readToken.


      _proto.jsxReadWord = function jsxReadWord() {
        var ch;
        var start = this.state.pos;

        do {
          ch = this.input.charCodeAt(++this.state.pos);
        } while (isIdentifierChar(ch) || ch === 45);

        return this.finishToken(types.jsxName, this.input.slice(start, this.state.pos));
      }; // Parse next token as JSX identifier


      _proto.jsxParseIdentifier = function jsxParseIdentifier() {
        var node = this.startNode();

        if (this.match(types.jsxName)) {
          node.name = this.state.value;
        } else if (this.state.type.keyword) {
          node.name = this.state.type.keyword;
        } else {
          this.unexpected();
        }

        this.next();
        return this.finishNode(node, "JSXIdentifier");
      }; // Parse namespaced identifier.


      _proto.jsxParseNamespacedName = function jsxParseNamespacedName() {
        var startPos = this.state.start;
        var startLoc = this.state.startLoc;
        var name = this.jsxParseIdentifier();
        if (!this.eat(types.colon)) return name;
        var node = this.startNodeAt(startPos, startLoc);
        node.namespace = name;
        node.name = this.jsxParseIdentifier();
        return this.finishNode(node, "JSXNamespacedName");
      }; // Parses element name in any form - namespaced, member
      // or single identifier.


      _proto.jsxParseElementName = function jsxParseElementName() {
        var startPos = this.state.start;
        var startLoc = this.state.startLoc;
        var node = this.jsxParseNamespacedName();

        while (this.eat(types.dot)) {
          var newNode = this.startNodeAt(startPos, startLoc);
          newNode.object = node;
          newNode.property = this.jsxParseIdentifier();
          node = this.finishNode(newNode, "JSXMemberExpression");
        }

        return node;
      }; // Parses any type of JSX attribute value.


      _proto.jsxParseAttributeValue = function jsxParseAttributeValue() {
        var node;

        switch (this.state.type) {
          case types.braceL:
            node = this.jsxParseExpressionContainer();

            if (node.expression.type === "JSXEmptyExpression") {
              throw this.raise(node.start, "JSX attributes must only be assigned a non-empty expression");
            } else {
              return node;
            }

          case types.jsxTagStart:
          case types.string:
            return this.parseExprAtom();

          default:
            throw this.raise(this.state.start, "JSX value should be either an expression or a quoted JSX text");
        }
      }; // JSXEmptyExpression is unique type since it doesn't actually parse anything,
      // and so it should start at the end of last read token (left brace) and finish
      // at the beginning of the next one (right brace).


      _proto.jsxParseEmptyExpression = function jsxParseEmptyExpression() {
        var node = this.startNodeAt(this.state.lastTokEnd, this.state.lastTokEndLoc);
        return this.finishNodeAt(node, "JSXEmptyExpression", this.state.start, this.state.startLoc);
      }; // Parse JSX spread child


      _proto.jsxParseSpreadChild = function jsxParseSpreadChild() {
        var node = this.startNode();
        this.expect(types.braceL);
        this.expect(types.ellipsis);
        node.expression = this.parseExpression();
        this.expect(types.braceR);
        return this.finishNode(node, "JSXSpreadChild");
      }; // Parses JSX expression enclosed into curly brackets.


      _proto.jsxParseExpressionContainer = function jsxParseExpressionContainer() {
        var node = this.startNode();
        this.next();

        if (this.match(types.braceR)) {
          node.expression = this.jsxParseEmptyExpression();
        } else {
          node.expression = this.parseExpression();
        }

        this.expect(types.braceR);
        return this.finishNode(node, "JSXExpressionContainer");
      }; // Parses following JSX attribute name-value pair.


      _proto.jsxParseAttribute = function jsxParseAttribute() {
        var node = this.startNode();

        if (this.eat(types.braceL)) {
          this.expect(types.ellipsis);
          node.argument = this.parseMaybeAssign();
          this.expect(types.braceR);
          return this.finishNode(node, "JSXSpreadAttribute");
        }

        node.name = this.jsxParseNamespacedName();
        node.value = this.eat(types.eq) ? this.jsxParseAttributeValue() : null;
        return this.finishNode(node, "JSXAttribute");
      }; // Parses JSX opening tag starting after "<".


      _proto.jsxParseOpeningElementAt = function jsxParseOpeningElementAt(startPos, startLoc) {
        var node = this.startNodeAt(startPos, startLoc);

        if (this.match(types.jsxTagEnd)) {
          this.expect(types.jsxTagEnd);
          return this.finishNode(node, "JSXOpeningFragment");
        }

        node.attributes = [];
        node.name = this.jsxParseElementName();

        while (!this.match(types.slash) && !this.match(types.jsxTagEnd)) {
          node.attributes.push(this.jsxParseAttribute());
        }

        node.selfClosing = this.eat(types.slash);
        this.expect(types.jsxTagEnd);
        return this.finishNode(node, "JSXOpeningElement");
      }; // Parses JSX closing tag starting after "</".


      _proto.jsxParseClosingElementAt = function jsxParseClosingElementAt(startPos, startLoc) {
        var node = this.startNodeAt(startPos, startLoc);

        if (this.match(types.jsxTagEnd)) {
          this.expect(types.jsxTagEnd);
          return this.finishNode(node, "JSXClosingFragment");
        }

        node.name = this.jsxParseElementName();
        this.expect(types.jsxTagEnd);
        return this.finishNode(node, "JSXClosingElement");
      }; // Parses entire JSX element, including it"s opening tag
      // (starting after "<"), attributes, contents and closing tag.


      _proto.jsxParseElementAt = function jsxParseElementAt(startPos, startLoc) {
        var node = this.startNodeAt(startPos, startLoc);
        var children = [];
        var openingElement = this.jsxParseOpeningElementAt(startPos, startLoc);
        var closingElement = null;

        if (!openingElement.selfClosing) {
          contents: for (;;) {
            switch (this.state.type) {
              case types.jsxTagStart:
                startPos = this.state.start;
                startLoc = this.state.startLoc;
                this.next();

                if (this.eat(types.slash)) {
                  closingElement = this.jsxParseClosingElementAt(startPos, startLoc);
                  break contents;
                }

                children.push(this.jsxParseElementAt(startPos, startLoc));
                break;

              case types.jsxText:
                children.push(this.parseExprAtom());
                break;

              case types.braceL:
                if (this.lookahead().type === types.ellipsis) {
                  children.push(this.jsxParseSpreadChild());
                } else {
                  children.push(this.jsxParseExpressionContainer());
                }

                break;
              // istanbul ignore next - should never happen

              default:
                throw this.unexpected();
            }
          }

          if (isFragment(openingElement) && !isFragment(closingElement)) {
            this.raise( // $FlowIgnore
            closingElement.start, "Expected corresponding JSX closing tag for <>");
          } else if (!isFragment(openingElement) && isFragment(closingElement)) {
            this.raise( // $FlowIgnore
            closingElement.start, "Expected corresponding JSX closing tag for <" + getQualifiedJSXName(openingElement.name) + ">");
          } else if (!isFragment(openingElement) && !isFragment(closingElement)) {
            if ( // $FlowIgnore
            getQualifiedJSXName(closingElement.name) !== getQualifiedJSXName(openingElement.name)) {
              this.raise( // $FlowIgnore
              closingElement.start, "Expected corresponding JSX closing tag for <" + getQualifiedJSXName(openingElement.name) + ">");
            }
          }
        }

        if (isFragment(openingElement)) {
          node.openingFragment = openingElement;
          node.closingFragment = closingElement;
        } else {
          node.openingElement = openingElement;
          node.closingElement = closingElement;
        }

        node.children = children;

        if (this.match(types.relational) && this.state.value === "<") {
          this.raise(this.state.start, "Adjacent JSX elements must be wrapped in an enclosing tag. " + "Did you want a JSX fragment <>...</>?");
        }

        return isFragment(openingElement) ? this.finishNode(node, "JSXFragment") : this.finishNode(node, "JSXElement");
      }; // Parses entire JSX element from current position.


      _proto.jsxParseElement = function jsxParseElement() {
        var startPos = this.state.start;
        var startLoc = this.state.startLoc;
        this.next();
        return this.jsxParseElementAt(startPos, startLoc);
      }; // ==================================
      // Overrides
      // ==================================


      _proto.parseExprAtom = function parseExprAtom(refShortHandDefaultPos) {
        if (this.match(types.jsxText)) {
          return this.parseLiteral(this.state.value, "JSXText");
        } else if (this.match(types.jsxTagStart)) {
          return this.jsxParseElement();
        } else {
          return _superClass.prototype.parseExprAtom.call(this, refShortHandDefaultPos);
        }
      };

      _proto.readToken = function readToken(code) {
        if (this.state.inPropertyName) return _superClass.prototype.readToken.call(this, code);
        var context = this.curContext();

        if (context === types$1.j_expr) {
          return this.jsxReadToken();
        }

        if (context === types$1.j_oTag || context === types$1.j_cTag) {
          if (isIdentifierStart(code)) {
            return this.jsxReadWord();
          }

          if (code === 62) {
            ++this.state.pos;
            return this.finishToken(types.jsxTagEnd);
          }

          if ((code === 34 || code === 39) && context === types$1.j_oTag) {
            return this.jsxReadString(code);
          }
        }

        if (code === 60 && this.state.exprAllowed) {
          ++this.state.pos;
          return this.finishToken(types.jsxTagStart);
        }

        return _superClass.prototype.readToken.call(this, code);
      };

      _proto.updateContext = function updateContext(prevType) {
        if (this.match(types.braceL)) {
          var curContext = this.curContext();

          if (curContext === types$1.j_oTag) {
            this.state.context.push(types$1.braceExpression);
          } else if (curContext === types$1.j_expr) {
            this.state.context.push(types$1.templateQuasi);
          } else {
            _superClass.prototype.updateContext.call(this, prevType);
          }

          this.state.exprAllowed = true;
        } else if (this.match(types.slash) && prevType === types.jsxTagStart) {
          this.state.context.length -= 2; // do not consider JSX expr -> JSX open tag -> ... anymore

          this.state.context.push(types$1.j_cTag); // reconsider as closing tag context

          this.state.exprAllowed = false;
        } else {
          return _superClass.prototype.updateContext.call(this, prevType);
        }
      };

      return _class;
    }(superClass)
  );
});

function nonNull(x) {
  if (x == null) {
    // $FlowIgnore
    throw new Error(`Unexpected ${x} value.`);
  }

  return x;
}

function assert(x) {
  if (!x) {
    throw new Error("Assert fail");
  }
}

// Doesn't handle "void" or "null" because those are keywords, not identifiers.
function keywordTypeFromName(value) {
  switch (value) {
    case "any":
      return "TSAnyKeyword";

    case "boolean":
      return "TSBooleanKeyword";

    case "never":
      return "TSNeverKeyword";

    case "number":
      return "TSNumberKeyword";

    case "object":
      return "TSObjectKeyword";

    case "string":
      return "TSStringKeyword";

    case "symbol":
      return "TSSymbolKeyword";

    case "undefined":
      return "TSUndefinedKeyword";

    default:
      return undefined;
  }
}

var typescriptPlugin = (function (superClass) {
  return (
    /*#__PURE__*/
    function (_superClass) {
      _inheritsLoose(_class, _superClass);

      function _class() {
        return _superClass.apply(this, arguments) || this;
      }

      var _proto = _class.prototype;

      _proto.tsIsIdentifier = function tsIsIdentifier() {
        // TODO: actually a bit more complex in TypeScript, but shouldn't matter.
        // See https://github.com/Microsoft/TypeScript/issues/15008
        return this.match(types.name);
      };

      _proto.tsNextTokenCanFollowModifier = function tsNextTokenCanFollowModifier() {
        // Note: TypeScript's implementation is much more complicated because
        // more things are considered modifiers there.
        // This implementation only handles modifiers not handled by babylon itself. And "static".
        // TODO: Would be nice to avoid lookahead. Want a hasLineBreakUpNext() method...
        this.next();
        return !this.hasPrecedingLineBreak() && !this.match(types.parenL) && !this.match(types.colon) && !this.match(types.eq) && !this.match(types.question);
      };
      /** Parses a modifier matching one the given modifier names. */


      _proto.tsParseModifier = function tsParseModifier(allowedModifiers) {
        if (!this.match(types.name)) {
          return undefined;
        }

        var modifier = this.state.value;

        if (allowedModifiers.indexOf(modifier) !== -1 && this.tsTryParse(this.tsNextTokenCanFollowModifier.bind(this))) {
          return modifier;
        }

        return undefined;
      };

      _proto.tsIsListTerminator = function tsIsListTerminator(kind) {
        switch (kind) {
          case "EnumMembers":
          case "TypeMembers":
            return this.match(types.braceR);

          case "HeritageClauseElement":
            return this.match(types.braceL);

          case "TupleElementTypes":
            return this.match(types.bracketR);

          case "TypeParametersOrArguments":
            return this.isRelational(">");
        }

        throw new Error("Unreachable");
      };

      _proto.tsParseList = function tsParseList(kind, parseElement) {
        var result = [];

        while (!this.tsIsListTerminator(kind)) {
          // Skipping "parseListElement" from the TS source since that's just for error handling.
          result.push(parseElement());
        }

        return result;
      };

      _proto.tsParseDelimitedList = function tsParseDelimitedList(kind, parseElement) {
        return nonNull(this.tsParseDelimitedListWorker(kind, parseElement,
        /* expectSuccess */
        true));
      };

      _proto.tsTryParseDelimitedList = function tsTryParseDelimitedList(kind, parseElement) {
        return this.tsParseDelimitedListWorker(kind, parseElement,
        /* expectSuccess */
        false);
      };
      /**
       * If !expectSuccess, returns undefined instead of failing to parse.
       * If expectSuccess, parseElement should always return a defined value.
       */


      _proto.tsParseDelimitedListWorker = function tsParseDelimitedListWorker(kind, parseElement, expectSuccess) {
        var result = [];

        while (true) {
          if (this.tsIsListTerminator(kind)) {
            break;
          }

          var element = parseElement();

          if (element == null) {
            return undefined;
          }

          result.push(element);

          if (this.eat(types.comma)) {
            continue;
          }

          if (this.tsIsListTerminator(kind)) {
            break;
          }

          if (expectSuccess) {
            // This will fail with an error about a missing comma
            this.expect(types.comma);
          }

          return undefined;
        }

        return result;
      };

      _proto.tsParseBracketedList = function tsParseBracketedList(kind, parseElement, bracket, skipFirstToken) {
        if (!skipFirstToken) {
          if (bracket) {
            this.expect(types.bracketL);
          } else {
            this.expectRelational("<");
          }
        }

        var result = this.tsParseDelimitedList(kind, parseElement);

        if (bracket) {
          this.expect(types.bracketR);
        } else {
          this.expectRelational(">");
        }

        return result;
      };

      _proto.tsParseEntityName = function tsParseEntityName(allowReservedWords) {
        var entity = this.parseIdentifier();

        while (this.eat(types.dot)) {
          var node = this.startNodeAtNode(entity);
          node.left = entity;
          node.right = this.parseIdentifier(allowReservedWords);
          entity = this.finishNode(node, "TSQualifiedName");
        }

        return entity;
      };

      _proto.tsParseTypeReference = function tsParseTypeReference() {
        var node = this.startNode();
        node.typeName = this.tsParseEntityName(
        /* allowReservedWords */
        false);

        if (!this.hasPrecedingLineBreak() && this.isRelational("<")) {
          node.typeParameters = this.tsParseTypeArguments();
        }

        return this.finishNode(node, "TSTypeReference");
      };

      _proto.tsParseThisTypePredicate = function tsParseThisTypePredicate(lhs) {
        this.next();
        var node = this.startNode();
        node.parameterName = lhs;
        node.typeAnnotation = this.tsParseTypeAnnotation(
        /* eatColon */
        false);
        return this.finishNode(node, "TSTypePredicate");
      };

      _proto.tsParseThisTypeNode = function tsParseThisTypeNode() {
        var node = this.startNode();
        this.next();
        return this.finishNode(node, "TSThisType");
      };

      _proto.tsParseTypeQuery = function tsParseTypeQuery() {
        var node = this.startNode();
        this.expect(types._typeof);
        node.exprName = this.tsParseEntityName(
        /* allowReservedWords */
        true);
        return this.finishNode(node, "TSTypeQuery");
      };

      _proto.tsParseTypeParameter = function tsParseTypeParameter() {
        var node = this.startNode();
        node.name = this.parseIdentifierName(node.start);
        node.constraint = this.tsEatThenParseType(types._extends);
        node.default = this.tsEatThenParseType(types.eq);
        return this.finishNode(node, "TSTypeParameter");
      };

      _proto.tsTryParseTypeParameters = function tsTryParseTypeParameters() {
        if (this.isRelational("<")) {
          return this.tsParseTypeParameters();
        }
      };

      _proto.tsParseTypeParameters = function tsParseTypeParameters() {
        var node = this.startNode();

        if (this.isRelational("<") || this.match(types.jsxTagStart)) {
          this.next();
        } else {
          this.unexpected();
        }

        node.params = this.tsParseBracketedList("TypeParametersOrArguments", this.tsParseTypeParameter.bind(this),
        /* bracket */
        false,
        /* skipFirstToken */
        true);
        return this.finishNode(node, "TSTypeParameterDeclaration");
      }; // Note: In TypeScript implementation we must provide `yieldContext` and `awaitContext`,
      // but here it's always false, because this is only used for types.


      _proto.tsFillSignature = function tsFillSignature(returnToken, signature) {
        // Arrow fns *must* have return token (`=>`). Normal functions can omit it.
        var returnTokenRequired = returnToken === types.arrow;
        signature.typeParameters = this.tsTryParseTypeParameters();
        this.expect(types.parenL);
        signature.parameters = this.tsParseBindingListForSignature();

        if (returnTokenRequired) {
          signature.typeAnnotation = this.tsParseTypeOrTypePredicateAnnotation(returnToken);
        } else if (this.match(returnToken)) {
          signature.typeAnnotation = this.tsParseTypeOrTypePredicateAnnotation(returnToken);
        }
      };

      _proto.tsParseBindingListForSignature = function tsParseBindingListForSignature() {
        var _this = this;

        return this.parseBindingList(types.parenR).map(function (pattern) {
          if (pattern.type !== "Identifier" && pattern.type !== "RestElement") {
            throw _this.unexpected(pattern.start, "Name in a signature must be an Identifier.");
          }

          return pattern;
        });
      };

      _proto.tsParseTypeMemberSemicolon = function tsParseTypeMemberSemicolon() {
        if (!this.eat(types.comma)) {
          this.semicolon();
        }
      };

      _proto.tsParseSignatureMember = function tsParseSignatureMember(kind) {
        var node = this.startNode();

        if (kind === "TSConstructSignatureDeclaration") {
          this.expect(types._new);
        }

        this.tsFillSignature(types.colon, node);
        this.tsParseTypeMemberSemicolon();
        return this.finishNode(node, kind);
      };

      _proto.tsIsUnambiguouslyIndexSignature = function tsIsUnambiguouslyIndexSignature() {
        this.next(); // Skip '{'

        return this.eat(types.name) && this.match(types.colon);
      };

      _proto.tsTryParseIndexSignature = function tsTryParseIndexSignature(node) {
        if (!(this.match(types.bracketL) && this.tsLookAhead(this.tsIsUnambiguouslyIndexSignature.bind(this)))) {
          return undefined;
        }

        this.expect(types.bracketL);
        var id = this.parseIdentifier();
        this.expect(types.colon);
        id.typeAnnotation = this.tsParseTypeAnnotation(
        /* eatColon */
        false);
        this.expect(types.bracketR);
        node.parameters = [id];
        var type = this.tsTryParseTypeAnnotation();
        if (type) node.typeAnnotation = type;
        this.tsParseTypeMemberSemicolon();
        return this.finishNode(node, "TSIndexSignature");
      };

      _proto.tsParsePropertyOrMethodSignature = function tsParsePropertyOrMethodSignature(node, readonly) {
        this.parsePropertyName(node);
        if (this.eat(types.question)) node.optional = true;
        var nodeAny = node;

        if (!readonly && (this.match(types.parenL) || this.isRelational("<"))) {
          var method = nodeAny;
          this.tsFillSignature(types.colon, method);
          this.tsParseTypeMemberSemicolon();
          return this.finishNode(method, "TSMethodSignature");
        } else {
          var property = nodeAny;
          if (readonly) property.readonly = true;
          var type = this.tsTryParseTypeAnnotation();
          if (type) property.typeAnnotation = type;
          this.tsParseTypeMemberSemicolon();
          return this.finishNode(property, "TSPropertySignature");
        }
      };

      _proto.tsParseTypeMember = function tsParseTypeMember() {
        if (this.match(types.parenL) || this.isRelational("<")) {
          return this.tsParseSignatureMember("TSCallSignatureDeclaration");
        }

        if (this.match(types._new) && this.tsLookAhead(this.tsIsStartOfConstructSignature.bind(this))) {
          return this.tsParseSignatureMember("TSConstructSignatureDeclaration");
        } // Instead of fullStart, we create a node here.


        var node = this.startNode();
        var readonly = !!this.tsParseModifier(["readonly"]);
        var idx = this.tsTryParseIndexSignature(node);

        if (idx) {
          if (readonly) node.readonly = true;
          return idx;
        }

        return this.tsParsePropertyOrMethodSignature(node, readonly);
      };

      _proto.tsIsStartOfConstructSignature = function tsIsStartOfConstructSignature() {
        this.next();
        return this.match(types.parenL) || this.isRelational("<");
      };

      _proto.tsParseTypeLiteral = function tsParseTypeLiteral() {
        var node = this.startNode();
        node.members = this.tsParseObjectTypeMembers();
        return this.finishNode(node, "TSTypeLiteral");
      };

      _proto.tsParseObjectTypeMembers = function tsParseObjectTypeMembers() {
        this.expect(types.braceL);
        var members = this.tsParseList("TypeMembers", this.tsParseTypeMember.bind(this));
        this.expect(types.braceR);
        return members;
      };

      _proto.tsIsStartOfMappedType = function tsIsStartOfMappedType() {
        this.next();

        if (this.isContextual("readonly")) {
          this.next();
        }

        if (!this.match(types.bracketL)) {
          return false;
        }

        this.next();

        if (!this.tsIsIdentifier()) {
          return false;
        }

        this.next();
        return this.match(types._in);
      };

      _proto.tsParseMappedTypeParameter = function tsParseMappedTypeParameter() {
        var node = this.startNode();
        node.name = this.parseIdentifierName(node.start);
        node.constraint = this.tsExpectThenParseType(types._in);
        return this.finishNode(node, "TSTypeParameter");
      };

      _proto.tsParseMappedType = function tsParseMappedType() {
        var node = this.startNode();
        this.expect(types.braceL);

        if (this.eatContextual("readonly")) {
          node.readonly = true;
        }

        this.expect(types.bracketL);
        node.typeParameter = this.tsParseMappedTypeParameter();
        this.expect(types.bracketR);

        if (this.eat(types.question)) {
          node.optional = true;
        }

        node.typeAnnotation = this.tsTryParseType();
        this.semicolon();
        this.expect(types.braceR);
        return this.finishNode(node, "TSMappedType");
      };

      _proto.tsParseTupleType = function tsParseTupleType() {
        var node = this.startNode();
        node.elementTypes = this.tsParseBracketedList("TupleElementTypes", this.tsParseType.bind(this),
        /* bracket */
        true,
        /* skipFirstToken */
        false);
        return this.finishNode(node, "TSTupleType");
      };

      _proto.tsParseParenthesizedType = function tsParseParenthesizedType() {
        var node = this.startNode();
        this.expect(types.parenL);
        node.typeAnnotation = this.tsParseType();
        this.expect(types.parenR);
        return this.finishNode(node, "TSParenthesizedType");
      };

      _proto.tsParseFunctionOrConstructorType = function tsParseFunctionOrConstructorType(type) {
        var node = this.startNode();

        if (type === "TSConstructorType") {
          this.expect(types._new);
        }

        this.tsFillSignature(types.arrow, node);
        return this.finishNode(node, type);
      };

      _proto.tsParseLiteralTypeNode = function tsParseLiteralTypeNode() {
        var _this2 = this;

        var node = this.startNode();

        node.literal = function () {
          switch (_this2.state.type) {
            case types.num:
              return _this2.parseLiteral(_this2.state.value, "NumericLiteral");

            case types.string:
              return _this2.parseLiteral(_this2.state.value, "StringLiteral");

            case types._true:
            case types._false:
              return _this2.parseBooleanLiteral();

            default:
              throw _this2.unexpected();
          }
        }();

        return this.finishNode(node, "TSLiteralType");
      };

      _proto.tsParseNonArrayType = function tsParseNonArrayType() {
        switch (this.state.type) {
          case types.name:
          case types._void:
          case types._null:
            {
              var type = this.match(types._void) ? "TSVoidKeyword" : this.match(types._null) ? "TSNullKeyword" : keywordTypeFromName(this.state.value);

              if (type !== undefined && this.lookahead().type !== types.dot) {
                var node = this.startNode();
                this.next();
                return this.finishNode(node, type);
              }

              return this.tsParseTypeReference();
            }

          case types.string:
          case types.num:
          case types._true:
          case types._false:
            return this.tsParseLiteralTypeNode();

          case types.plusMin:
            if (this.state.value === "-") {
              var _node = this.startNode();

              this.next();

              if (!this.match(types.num)) {
                throw this.unexpected();
              }

              _node.literal = this.parseLiteral(-this.state.value, "NumericLiteral", _node.start, _node.loc.start);
              return this.finishNode(_node, "TSLiteralType");
            }

            break;

          case types._this:
            {
              var thisKeyword = this.tsParseThisTypeNode();

              if (this.isContextual("is") && !this.hasPrecedingLineBreak()) {
                return this.tsParseThisTypePredicate(thisKeyword);
              } else {
                return thisKeyword;
              }
            }

          case types._typeof:
            return this.tsParseTypeQuery();

          case types.braceL:
            return this.tsLookAhead(this.tsIsStartOfMappedType.bind(this)) ? this.tsParseMappedType() : this.tsParseTypeLiteral();

          case types.bracketL:
            return this.tsParseTupleType();

          case types.parenL:
            return this.tsParseParenthesizedType();
        }

        throw this.unexpected();
      };

      _proto.tsParseArrayTypeOrHigher = function tsParseArrayTypeOrHigher() {
        var type = this.tsParseNonArrayType();

        while (!this.hasPrecedingLineBreak() && this.eat(types.bracketL)) {
          if (this.match(types.bracketR)) {
            var node = this.startNodeAtNode(type);
            node.elementType = type;
            this.expect(types.bracketR);
            type = this.finishNode(node, "TSArrayType");
          } else {
            var _node2 = this.startNodeAtNode(type);

            _node2.objectType = type;
            _node2.indexType = this.tsParseType();
            this.expect(types.bracketR);
            type = this.finishNode(_node2, "TSIndexedAccessType");
          }
        }

        return type;
      };

      _proto.tsParseTypeOperator = function tsParseTypeOperator(operator) {
        var node = this.startNode();
        this.expectContextual(operator);
        node.operator = operator;
        node.typeAnnotation = this.tsParseTypeOperatorOrHigher();
        return this.finishNode(node, "TSTypeOperator");
      };

      _proto.tsParseTypeOperatorOrHigher = function tsParseTypeOperatorOrHigher() {
        var _this3 = this;

        var operator = ["keyof", "unique"].find(function (kw) {
          return _this3.isContextual(kw);
        });
        return operator ? this.tsParseTypeOperator(operator) : this.tsParseArrayTypeOrHigher();
      };

      _proto.tsParseUnionOrIntersectionType = function tsParseUnionOrIntersectionType(kind, parseConstituentType, operator) {
        this.eat(operator);
        var type = parseConstituentType();

        if (this.match(operator)) {
          var types$$1 = [type];

          while (this.eat(operator)) {
            types$$1.push(parseConstituentType());
          }

          var node = this.startNodeAtNode(type);
          node.types = types$$1;
          type = this.finishNode(node, kind);
        }

        return type;
      };

      _proto.tsParseIntersectionTypeOrHigher = function tsParseIntersectionTypeOrHigher() {
        return this.tsParseUnionOrIntersectionType("TSIntersectionType", this.tsParseTypeOperatorOrHigher.bind(this), types.bitwiseAND);
      };

      _proto.tsParseUnionTypeOrHigher = function tsParseUnionTypeOrHigher() {
        return this.tsParseUnionOrIntersectionType("TSUnionType", this.tsParseIntersectionTypeOrHigher.bind(this), types.bitwiseOR);
      };

      _proto.tsIsStartOfFunctionType = function tsIsStartOfFunctionType() {
        if (this.isRelational("<")) {
          return true;
        }

        return this.match(types.parenL) && this.tsLookAhead(this.tsIsUnambiguouslyStartOfFunctionType.bind(this));
      };

      _proto.tsSkipParameterStart = function tsSkipParameterStart() {
        if (this.match(types.name) || this.match(types._this)) {
          this.next();
          return true;
        }

        return false;
      };

      _proto.tsIsUnambiguouslyStartOfFunctionType = function tsIsUnambiguouslyStartOfFunctionType() {
        this.next();

        if (this.match(types.parenR) || this.match(types.ellipsis)) {
          // ( )
          // ( ...
          return true;
        }

        if (this.tsSkipParameterStart()) {
          if (this.match(types.colon) || this.match(types.comma) || this.match(types.question) || this.match(types.eq)) {
            // ( xxx :
            // ( xxx ,
            // ( xxx ?
            // ( xxx =
            return true;
          }

          if (this.match(types.parenR)) {
            this.next();

            if (this.match(types.arrow)) {
              // ( xxx ) =>
              return true;
            }
          }
        }

        return false;
      };

      _proto.tsParseTypeOrTypePredicateAnnotation = function tsParseTypeOrTypePredicateAnnotation(returnToken) {
        var _this4 = this;

        return this.tsInType(function () {
          var t = _this4.startNode();

          _this4.expect(returnToken);

          var typePredicateVariable = _this4.tsIsIdentifier() && _this4.tsTryParse(_this4.tsParseTypePredicatePrefix.bind(_this4));

          if (!typePredicateVariable) {
            return _this4.tsParseTypeAnnotation(
            /* eatColon */
            false, t);
          }

          var type = _this4.tsParseTypeAnnotation(
          /* eatColon */
          false);

          var node = _this4.startNodeAtNode(typePredicateVariable);

          node.parameterName = typePredicateVariable;
          node.typeAnnotation = type;
          t.typeAnnotation = _this4.finishNode(node, "TSTypePredicate");
          return _this4.finishNode(t, "TSTypeAnnotation");
        });
      };

      _proto.tsTryParseTypeOrTypePredicateAnnotation = function tsTryParseTypeOrTypePredicateAnnotation() {
        return this.match(types.colon) ? this.tsParseTypeOrTypePredicateAnnotation(types.colon) : undefined;
      };

      _proto.tsTryParseTypeAnnotation = function tsTryParseTypeAnnotation() {
        return this.match(types.colon) ? this.tsParseTypeAnnotation() : undefined;
      };

      _proto.tsTryParseType = function tsTryParseType() {
        return this.tsEatThenParseType(types.colon);
      };

      _proto.tsParseTypePredicatePrefix = function tsParseTypePredicatePrefix() {
        var id = this.parseIdentifier();

        if (this.isContextual("is") && !this.hasPrecedingLineBreak()) {
          this.next();
          return id;
        }
      };

      _proto.tsParseTypeAnnotation = function tsParseTypeAnnotation(eatColon, t) {
        var _this5 = this;

        if (eatColon === void 0) {
          eatColon = true;
        }

        if (t === void 0) {
          t = this.startNode();
        }

        this.tsInType(function () {
          if (eatColon) _this5.expect(types.colon);
          t.typeAnnotation = _this5.tsParseType();
        });
        return this.finishNode(t, "TSTypeAnnotation");
      };
      /** Be sure to be in a type context before calling this, using `tsInType`. */


      _proto.tsParseType = function tsParseType() {
        // Need to set `state.inType` so that we don't parse JSX in a type context.
        assert(this.state.inType);

        if (this.tsIsStartOfFunctionType()) {
          return this.tsParseFunctionOrConstructorType("TSFunctionType");
        }

        if (this.match(types._new)) {
          // As in `new () => Date`
          return this.tsParseFunctionOrConstructorType("TSConstructorType");
        }

        return this.tsParseUnionTypeOrHigher();
      };

      _proto.tsParseTypeAssertion = function tsParseTypeAssertion() {
        var _this6 = this;

        var node = this.startNode(); // Not actually necessary to set state.inType because we never reach here if JSX plugin is enabled,
        // but need `tsInType` to satisfy the assertion in `tsParseType`.

        node.typeAnnotation = this.tsInType(function () {
          return _this6.tsParseType();
        });
        this.expectRelational(">");
        node.expression = this.parseMaybeUnary();
        return this.finishNode(node, "TSTypeAssertion");
      };

      _proto.tsTryParseTypeArgumentsInExpression = function tsTryParseTypeArgumentsInExpression() {
        var _this7 = this;

        return this.tsTryParseAndCatch(function () {
          var res = _this7.tsParseTypeArguments();

          _this7.expect(types.parenL);

          return res;
        });
      };

      _proto.tsParseHeritageClause = function tsParseHeritageClause() {
        return this.tsParseDelimitedList("HeritageClauseElement", this.tsParseExpressionWithTypeArguments.bind(this));
      };

      _proto.tsParseExpressionWithTypeArguments = function tsParseExpressionWithTypeArguments() {
        var node = this.startNode(); // Note: TS uses parseLeftHandSideExpressionOrHigher,
        // then has grammar errors later if it's not an EntityName.

        node.expression = this.tsParseEntityName(
        /* allowReservedWords */
        false);

        if (this.isRelational("<")) {
          node.typeParameters = this.tsParseTypeArguments();
        }

        return this.finishNode(node, "TSExpressionWithTypeArguments");
      };

      _proto.tsParseInterfaceDeclaration = function tsParseInterfaceDeclaration(node) {
        node.id = this.parseIdentifier();
        node.typeParameters = this.tsTryParseTypeParameters();

        if (this.eat(types._extends)) {
          node.extends = this.tsParseHeritageClause();
        }

        var body = this.startNode();
        body.body = this.tsParseObjectTypeMembers();
        node.body = this.finishNode(body, "TSInterfaceBody");
        return this.finishNode(node, "TSInterfaceDeclaration");
      };

      _proto.tsParseTypeAliasDeclaration = function tsParseTypeAliasDeclaration(node) {
        node.id = this.parseIdentifier();
        node.typeParameters = this.tsTryParseTypeParameters();
        node.typeAnnotation = this.tsExpectThenParseType(types.eq);
        this.semicolon();
        return this.finishNode(node, "TSTypeAliasDeclaration");
      };
      /**
       * Runs `cb` in a type context.
       * This should be called one token *before* the first type token,
       * so that the call to `next()` is run in type context.
       */


      _proto.tsInType = function tsInType(cb) {
        var oldInType = this.state.inType;
        this.state.inType = true;

        try {
          return cb();
        } finally {
          this.state.inType = oldInType;
        }
      };

      _proto.tsEatThenParseType = function tsEatThenParseType(token) {
        return !this.match(token) ? undefined : this.tsNextThenParseType();
      };

      _proto.tsExpectThenParseType = function tsExpectThenParseType(token) {
        var _this8 = this;

        return this.tsDoThenParseType(function () {
          return _this8.expect(token);
        });
      };

      _proto.tsNextThenParseType = function tsNextThenParseType() {
        var _this9 = this;

        return this.tsDoThenParseType(function () {
          return _this9.next();
        });
      };

      _proto.tsDoThenParseType = function tsDoThenParseType(cb) {
        var _this10 = this;

        return this.tsInType(function () {
          cb();
          return _this10.tsParseType();
        });
      };

      _proto.tsParseEnumMember = function tsParseEnumMember() {
        var node = this.startNode(); // Computed property names are grammar errors in an enum, so accept just string literal or identifier.

        node.id = this.match(types.string) ? this.parseLiteral(this.state.value, "StringLiteral") : this.parseIdentifier(
        /* liberal */
        true);

        if (this.eat(types.eq)) {
          node.initializer = this.parseMaybeAssign();
        }

        return this.finishNode(node, "TSEnumMember");
      };

      _proto.tsParseEnumDeclaration = function tsParseEnumDeclaration(node, isConst) {
        if (isConst) node.const = true;
        node.id = this.parseIdentifier();
        this.expect(types.braceL);
        node.members = this.tsParseDelimitedList("EnumMembers", this.tsParseEnumMember.bind(this));
        this.expect(types.braceR);
        return this.finishNode(node, "TSEnumDeclaration");
      };

      _proto.tsParseModuleBlock = function tsParseModuleBlock() {
        var node = this.startNode();
        this.expect(types.braceL); // Inside of a module block is considered "top-level", meaning it can have imports and exports.

        this.parseBlockOrModuleBlockBody(node.body = [],
        /* directives */
        undefined,
        /* topLevel */
        true,
        /* end */
        types.braceR);
        return this.finishNode(node, "TSModuleBlock");
      };

      _proto.tsParseModuleOrNamespaceDeclaration = function tsParseModuleOrNamespaceDeclaration(node) {
        node.id = this.parseIdentifier();

        if (this.eat(types.dot)) {
          var inner = this.startNode();
          this.tsParseModuleOrNamespaceDeclaration(inner);
          node.body = inner;
        } else {
          node.body = this.tsParseModuleBlock();
        }

        return this.finishNode(node, "TSModuleDeclaration");
      };

      _proto.tsParseAmbientExternalModuleDeclaration = function tsParseAmbientExternalModuleDeclaration(node) {
        if (this.isContextual("global")) {
          node.global = true;
          node.id = this.parseIdentifier();
        } else if (this.match(types.string)) {
          node.id = this.parseExprAtom();
        } else {
          this.unexpected();
        }

        if (this.match(types.braceL)) {
          node.body = this.tsParseModuleBlock();
        } else {
          this.semicolon();
        }

        return this.finishNode(node, "TSModuleDeclaration");
      };

      _proto.tsParseImportEqualsDeclaration = function tsParseImportEqualsDeclaration(node, isExport) {
        node.isExport = isExport || false;
        node.id = this.parseIdentifier();
        this.expect(types.eq);
        node.moduleReference = this.tsParseModuleReference();
        this.semicolon();
        return this.finishNode(node, "TSImportEqualsDeclaration");
      };

      _proto.tsIsExternalModuleReference = function tsIsExternalModuleReference() {
        return this.isContextual("require") && this.lookahead().type === types.parenL;
      };

      _proto.tsParseModuleReference = function tsParseModuleReference() {
        return this.tsIsExternalModuleReference() ? this.tsParseExternalModuleReference() : this.tsParseEntityName(
        /* allowReservedWords */
        false);
      };

      _proto.tsParseExternalModuleReference = function tsParseExternalModuleReference() {
        var node = this.startNode();
        this.expectContextual("require");
        this.expect(types.parenL);

        if (!this.match(types.string)) {
          throw this.unexpected();
        }

        node.expression = this.parseLiteral(this.state.value, "StringLiteral");
        this.expect(types.parenR);
        return this.finishNode(node, "TSExternalModuleReference");
      }; // Utilities


      _proto.tsLookAhead = function tsLookAhead(f) {
        var state = this.state.clone();
        var res = f();
        this.state = state;
        return res;
      };

      _proto.tsTryParseAndCatch = function tsTryParseAndCatch(f) {
        var state = this.state.clone();

        try {
          return f();
        } catch (e) {
          if (e instanceof SyntaxError) {
            this.state = state;
            return undefined;
          }

          throw e;
        }
      };

      _proto.tsTryParse = function tsTryParse(f) {
        var state = this.state.clone();
        var result = f();

        if (result !== undefined && result !== false) {
          return result;
        } else {
          this.state = state;
          return undefined;
        }
      };

      _proto.nodeWithSamePosition = function nodeWithSamePosition(original, type) {
        var node = this.startNodeAtNode(original);
        node.type = type;
        node.end = original.end;
        node.loc.end = original.loc.end;

        if (original.leadingComments) {
          node.leadingComments = original.leadingComments;
        }

        if (original.trailingComments) {
          node.trailingComments = original.trailingComments;
        }

        if (original.innerComments) node.innerComments = original.innerComments;
        return node;
      };

      _proto.tsTryParseDeclare = function tsTryParseDeclare(nany) {
        switch (this.state.type) {
          case types._function:
            this.next();
            return this.parseFunction(nany,
            /* isStatement */
            true);

          case types._class:
            return this.parseClass(nany,
            /* isStatement */
            true,
            /* optionalId */
            false);

          case types._const:
            if (this.match(types._const) && this.isLookaheadContextual("enum")) {
              // `const enum = 0;` not allowed because "enum" is a strict mode reserved word.
              this.expect(types._const);
              this.expectContextual("enum");
              return this.tsParseEnumDeclaration(nany,
              /* isConst */
              true);
            }

          // falls through

          case types._var:
          case types._let:
            return this.parseVarStatement(nany, this.state.type);

          case types.name:
            {
              var value = this.state.value;

              if (value === "global") {
                return this.tsParseAmbientExternalModuleDeclaration(nany);
              } else {
                return this.tsParseDeclaration(nany, value,
                /* next */
                true);
              }
            }
        }
      }; // Note: this won't be called unless the keyword is allowed in `shouldParseExportDeclaration`.


      _proto.tsTryParseExportDeclaration = function tsTryParseExportDeclaration() {
        return this.tsParseDeclaration(this.startNode(), this.state.value,
        /* next */
        true);
      };

      _proto.tsParseExpressionStatement = function tsParseExpressionStatement(node, expr) {
        switch (expr.name) {
          case "declare":
            {
              var declaration = this.tsTryParseDeclare(node);

              if (declaration) {
                declaration.declare = true;
                return declaration;
              }

              break;
            }

          case "global":
            // `global { }` (with no `declare`) may appear inside an ambient module declaration.
            // Would like to use tsParseAmbientExternalModuleDeclaration here, but already ran past "global".
            if (this.match(types.braceL)) {
              var mod = node;
              mod.global = true;
              mod.id = expr;
              mod.body = this.tsParseModuleBlock();
              return this.finishNode(mod, "TSModuleDeclaration");
            }

            break;

          default:
            return this.tsParseDeclaration(node, expr.name,
            /* next */
            false);
        }
      }; // Common to tsTryParseDeclare, tsTryParseExportDeclaration, and tsParseExpressionStatement.


      _proto.tsParseDeclaration = function tsParseDeclaration(node, value, next) {
        switch (value) {
          case "abstract":
            if (next || this.match(types._class)) {
              var cls = node;
              cls.abstract = true;
              if (next) this.next();
              return this.parseClass(cls,
              /* isStatement */
              true,
              /* optionalId */
              false);
            }

            break;

          case "enum":
            if (next || this.match(types.name)) {
              if (next) this.next();
              return this.tsParseEnumDeclaration(node,
              /* isConst */
              false);
            }

            break;

          case "interface":
            if (next || this.match(types.name)) {
              if (next) this.next();
              return this.tsParseInterfaceDeclaration(node);
            }

            break;

          case "module":
            if (next) this.next();

            if (this.match(types.string)) {
              return this.tsParseAmbientExternalModuleDeclaration(node);
            } else if (next || this.match(types.name)) {
              return this.tsParseModuleOrNamespaceDeclaration(node);
            }

            break;

          case "namespace":
            if (next || this.match(types.name)) {
              if (next) this.next();
              return this.tsParseModuleOrNamespaceDeclaration(node);
            }

            break;

          case "type":
            if (next || this.match(types.name)) {
              if (next) this.next();
              return this.tsParseTypeAliasDeclaration(node);
            }

            break;
        }
      };

      _proto.tsTryParseGenericAsyncArrowFunction = function tsTryParseGenericAsyncArrowFunction(startPos, startLoc) {
        var _this11 = this;

        var res = this.tsTryParseAndCatch(function () {
          var node = _this11.startNodeAt(startPos, startLoc);

          node.typeParameters = _this11.tsParseTypeParameters(); // Don't use overloaded parseFunctionParams which would look for "<" again.

          _superClass.prototype.parseFunctionParams.call(_this11, node);

          node.returnType = _this11.tsTryParseTypeOrTypePredicateAnnotation();

          _this11.expect(types.arrow);

          return node;
        });

        if (!res) {
          return undefined;
        }

        res.id = null;
        res.generator = false;
        res.expression = true; // May be set again by parseFunctionBody.

        res.async = true;
        this.parseFunctionBody(res, true);
        return this.finishNode(res, "ArrowFunctionExpression");
      };

      _proto.tsParseTypeArguments = function tsParseTypeArguments() {
        var _this12 = this;

        var node = this.startNode();
        node.params = this.tsInType(function () {
          _this12.expectRelational("<");

          return _this12.tsParseDelimitedList("TypeParametersOrArguments", _this12.tsParseType.bind(_this12));
        });
        this.expectRelational(">");
        return this.finishNode(node, "TSTypeParameterInstantiation");
      };

      _proto.tsIsDeclarationStart = function tsIsDeclarationStart() {
        if (this.match(types.name)) {
          switch (this.state.value) {
            case "abstract":
            case "declare":
            case "enum":
            case "interface":
            case "module":
            case "namespace":
            case "type":
              return true;
          }
        }

        return false;
      }; // ======================================================
      // OVERRIDES
      // ======================================================


      _proto.isExportDefaultSpecifier = function isExportDefaultSpecifier() {
        if (this.tsIsDeclarationStart()) return false;
        return _superClass.prototype.isExportDefaultSpecifier.call(this);
      };

      _proto.parseAssignableListItem = function parseAssignableListItem(allowModifiers, decorators) {
        var accessibility;
        var readonly = false;

        if (allowModifiers) {
          accessibility = this.parseAccessModifier();
          readonly = !!this.tsParseModifier(["readonly"]);
        }

        var left = this.parseMaybeDefault();
        this.parseAssignableListItemTypes(left);
        var elt = this.parseMaybeDefault(left.start, left.loc.start, left);

        if (accessibility || readonly) {
          var pp = this.startNodeAtNode(elt);

          if (decorators.length) {
            pp.decorators = decorators;
          }

          if (accessibility) pp.accessibility = accessibility;
          if (readonly) pp.readonly = readonly;

          if (elt.type !== "Identifier" && elt.type !== "AssignmentPattern") {
            throw this.raise(pp.start, "A parameter property may not be declared using a binding pattern.");
          }

          pp.parameter = elt;
          return this.finishNode(pp, "TSParameterProperty");
        } else {
          if (decorators.length) {
            left.decorators = decorators;
          }

          return elt;
        }
      };

      _proto.parseFunctionBodyAndFinish = function parseFunctionBodyAndFinish(node, type, allowExpressionBody) {
        // For arrow functions, `parseArrow` handles the return type itself.
        if (!allowExpressionBody && this.match(types.colon)) {
          node.returnType = this.tsParseTypeOrTypePredicateAnnotation(types.colon);
        }

        var bodilessType = type === "FunctionDeclaration" ? "TSDeclareFunction" : type === "ClassMethod" ? "TSDeclareMethod" : undefined;

        if (bodilessType && !this.match(types.braceL) && this.isLineTerminator()) {
          this.finishNode(node, bodilessType);
          return;
        }

        _superClass.prototype.parseFunctionBodyAndFinish.call(this, node, type, allowExpressionBody);
      };

      _proto.parseSubscript = function parseSubscript(base, startPos, startLoc, noCalls, state) {
        if (!this.hasPrecedingLineBreak() && this.eat(types.bang)) {
          var nonNullExpression = this.startNodeAt(startPos, startLoc);
          nonNullExpression.expression = base;
          return this.finishNode(nonNullExpression, "TSNonNullExpression");
        }

        if (!noCalls && this.isRelational("<")) {
          if (this.atPossibleAsync(base)) {
            // Almost certainly this is a generic async function `async <T>() => ...
            // But it might be a call with a type argument `async<T>();`
            var asyncArrowFn = this.tsTryParseGenericAsyncArrowFunction(startPos, startLoc);

            if (asyncArrowFn) {
              return asyncArrowFn;
            }
          }

          var node = this.startNodeAt(startPos, startLoc);
          node.callee = base; // May be passing type arguments. But may just be the `<` operator.

          var typeArguments = this.tsTryParseTypeArgumentsInExpression(); // Also eats the "("

          if (typeArguments) {
            // possibleAsync always false here, because we would have handled it above.
            // $FlowIgnore (won't be any undefined arguments)
            node.arguments = this.parseCallExpressionArguments(types.parenR,
            /* possibleAsync */
            false);
            node.typeParameters = typeArguments;
            return this.finishCallExpression(node);
          }
        }

        return _superClass.prototype.parseSubscript.call(this, base, startPos, startLoc, noCalls, state);
      };

      _proto.parseNewArguments = function parseNewArguments(node) {
        var _this13 = this;

        if (this.isRelational("<")) {
          // tsTryParseAndCatch is expensive, so avoid if not necessary.
          // 99% certain this is `new C<T>();`. But may be `new C < T;`, which is also legal.
          var typeParameters = this.tsTryParseAndCatch(function () {
            var args = _this13.tsParseTypeArguments();

            if (!_this13.match(types.parenL)) _this13.unexpected();
            return args;
          });

          if (typeParameters) {
            node.typeParameters = typeParameters;
          }
        }

        _superClass.prototype.parseNewArguments.call(this, node);
      };

      _proto.parseExprOp = function parseExprOp(left, leftStartPos, leftStartLoc, minPrec, noIn) {
        if (nonNull(types._in.binop) > minPrec && !this.hasPrecedingLineBreak() && this.isContextual("as")) {
          var node = this.startNodeAt(leftStartPos, leftStartLoc);
          node.expression = left;
          node.typeAnnotation = this.tsNextThenParseType();
          this.finishNode(node, "TSAsExpression");
          return this.parseExprOp(node, leftStartPos, leftStartLoc, minPrec, noIn);
        }

        return _superClass.prototype.parseExprOp.call(this, left, leftStartPos, leftStartLoc, minPrec, noIn);
      };

      _proto.checkReservedWord = function checkReservedWord(word, startLoc, checkKeywords, // eslint-disable-next-line no-unused-vars
      isBinding) {} // Don't bother checking for TypeScript code.
      // Strict mode words may be allowed as in `declare namespace N { const static: number; }`.
      // And we have a type checker anyway, so don't bother having the parser do it.

      /*
      Don't bother doing this check in TypeScript code because:
      1. We may have a nested export statement with the same name:
        export const x = 0;
        export namespace N {
          export const x = 1;
        }
      2. We have a type checker to warn us about this sort of thing.
      */
      ;

      _proto.checkDuplicateExports = function checkDuplicateExports() {};

      _proto.parseImport = function parseImport(node) {
        if (this.match(types.name) && this.lookahead().type === types.eq) {
          return this.tsParseImportEqualsDeclaration(node);
        }

        return _superClass.prototype.parseImport.call(this, node);
      };

      _proto.parseExport = function parseExport(node) {
        if (this.match(types._import)) {
          // `export import A = B;`
          this.expect(types._import);
          return this.tsParseImportEqualsDeclaration(node,
          /* isExport */
          true);
        } else if (this.eat(types.eq)) {
          // `export = x;`
          var assign = node;
          assign.expression = this.parseExpression();
          this.semicolon();
          return this.finishNode(assign, "TSExportAssignment");
        } else if (this.eatContextual("as")) {
          // `export as namespace A;`
          var decl = node; // See `parseNamespaceExportDeclaration` in TypeScript's own parser

          this.expectContextual("namespace");
          decl.id = this.parseIdentifier();
          this.semicolon();
          return this.finishNode(decl, "TSNamespaceExportDeclaration");
        } else {
          return _superClass.prototype.parseExport.call(this, node);
        }
      };

      _proto.parseExportDefaultExpression = function parseExportDefaultExpression() {
        if (this.isContextual("abstract") && this.lookahead().type === types._class) {
          var cls = this.startNode();
          this.next(); // Skip "abstract"

          this.parseClass(cls, true, true);
          cls.abstract = true;
          return cls;
        }

        return _superClass.prototype.parseExportDefaultExpression.call(this);
      };

      _proto.parseStatementContent = function parseStatementContent(declaration, topLevel) {
        if (this.state.type === types._const) {
          var ahead = this.lookahead();

          if (ahead.type === types.name && ahead.value === "enum") {
            var node = this.startNode();
            this.expect(types._const);
            this.expectContextual("enum");
            return this.tsParseEnumDeclaration(node,
            /* isConst */
            true);
          }
        }

        return _superClass.prototype.parseStatementContent.call(this, declaration, topLevel);
      };

      _proto.parseAccessModifier = function parseAccessModifier() {
        return this.tsParseModifier(["public", "protected", "private"]);
      };

      _proto.parseClassMember = function parseClassMember(classBody, member, state) {
        var accessibility = this.parseAccessModifier();
        if (accessibility) member.accessibility = accessibility;

        _superClass.prototype.parseClassMember.call(this, classBody, member, state);
      };

      _proto.parseClassMemberWithIsStatic = function parseClassMemberWithIsStatic(classBody, member, state, isStatic) {
        var methodOrProp = member;
        var prop = member;
        var propOrIdx = member;
        var abstract = false,
            readonly = false;
        var mod = this.tsParseModifier(["abstract", "readonly"]);

        switch (mod) {
          case "readonly":
            readonly = true;
            abstract = !!this.tsParseModifier(["abstract"]);
            break;

          case "abstract":
            abstract = true;
            readonly = !!this.tsParseModifier(["readonly"]);
            break;
        }

        if (abstract) methodOrProp.abstract = true;
        if (readonly) propOrIdx.readonly = true;

        if (!abstract && !isStatic && !methodOrProp.accessibility) {
          var idx = this.tsTryParseIndexSignature(member);

          if (idx) {
            classBody.body.push(idx);
            return;
          }
        }

        if (readonly) {
          // Must be a property (if not an index signature).
          methodOrProp.static = isStatic;
          this.parseClassPropertyName(prop);
          this.parsePostMemberNameModifiers(methodOrProp);
          this.pushClassProperty(classBody, prop);
          return;
        }

        _superClass.prototype.parseClassMemberWithIsStatic.call(this, classBody, member, state, isStatic);
      };

      _proto.parsePostMemberNameModifiers = function parsePostMemberNameModifiers(methodOrProp) {
        var optional = this.eat(types.question);
        if (optional) methodOrProp.optional = true;
      }; // Note: The reason we do this in `parseExpressionStatement` and not `parseStatement`
      // is that e.g. `type()` is valid JS, so we must try parsing that first.
      // If it's really a type, we will parse `type` as the statement, and can correct it here
      // by parsing the rest.


      _proto.parseExpressionStatement = function parseExpressionStatement(node, expr) {
        var decl = expr.type === "Identifier" ? this.tsParseExpressionStatement(node, expr) : undefined;
        return decl || _superClass.prototype.parseExpressionStatement.call(this, node, expr);
      }; // export type
      // Should be true for anything parsed by `tsTryParseExportDeclaration`.


      _proto.shouldParseExportDeclaration = function shouldParseExportDeclaration() {
        if (this.tsIsDeclarationStart()) return true;
        return _superClass.prototype.shouldParseExportDeclaration.call(this);
      }; // An apparent conditional expression could actually be an optional parameter in an arrow function.


      _proto.parseConditional = function parseConditional(expr, noIn, startPos, startLoc, refNeedsArrowPos) {
        // only do the expensive clone if there is a question mark
        // and if we come from inside parens
        if (!refNeedsArrowPos || !this.match(types.question)) {
          return _superClass.prototype.parseConditional.call(this, expr, noIn, startPos, startLoc, refNeedsArrowPos);
        }

        var state = this.state.clone();

        try {
          return _superClass.prototype.parseConditional.call(this, expr, noIn, startPos, startLoc);
        } catch (err) {
          if (!(err instanceof SyntaxError)) {
            // istanbul ignore next: no such error is expected
            throw err;
          }

          this.state = state;
          refNeedsArrowPos.start = err.pos || this.state.start;
          return expr;
        }
      }; // Note: These "type casts" are *not* valid TS expressions.
      // But we parse them here and change them when completing the arrow function.


      _proto.parseParenItem = function parseParenItem(node, startPos, startLoc) {
        node = _superClass.prototype.parseParenItem.call(this, node, startPos, startLoc);

        if (this.eat(types.question)) {
          node.optional = true;
        }

        if (this.match(types.colon)) {
          var typeCastNode = this.startNodeAt(startPos, startLoc);
          typeCastNode.expression = node;
          typeCastNode.typeAnnotation = this.tsParseTypeAnnotation();
          return this.finishNode(typeCastNode, "TSTypeCastExpression");
        }

        return node;
      };

      _proto.parseExportDeclaration = function parseExportDeclaration(node) {
        // "export declare" is equivalent to just "export".
        var isDeclare = this.eatContextual("declare");
        var declaration;

        if (this.match(types.name)) {
          declaration = this.tsTryParseExportDeclaration();
        }

        if (!declaration) {
          declaration = _superClass.prototype.parseExportDeclaration.call(this, node);
        }

        if (declaration && isDeclare) {
          declaration.declare = true;
        }

        return declaration;
      };

      _proto.parseClassId = function parseClassId(node, isStatement, optionalId) {
        var _superClass$prototype;

        if ((!isStatement || optionalId) && this.isContextual("implements")) {
          return;
        }

        (_superClass$prototype = _superClass.prototype.parseClassId).call.apply(_superClass$prototype, [this].concat(Array.prototype.slice.call(arguments)));

        var typeParameters = this.tsTryParseTypeParameters();
        if (typeParameters) node.typeParameters = typeParameters;
      };

      _proto.parseClassProperty = function parseClassProperty(node) {
        var type = this.tsTryParseTypeAnnotation();
        if (type) node.typeAnnotation = type;
        return _superClass.prototype.parseClassProperty.call(this, node);
      };

      _proto.pushClassMethod = function pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor) {
        var typeParameters = this.tsTryParseTypeParameters();
        if (typeParameters) method.typeParameters = typeParameters;

        _superClass.prototype.pushClassMethod.call(this, classBody, method, isGenerator, isAsync, isConstructor);
      };

      _proto.pushClassPrivateMethod = function pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
        var typeParameters = this.tsTryParseTypeParameters();
        if (typeParameters) method.typeParameters = typeParameters;

        _superClass.prototype.pushClassPrivateMethod.call(this, classBody, method, isGenerator, isAsync);
      };

      _proto.parseClassSuper = function parseClassSuper(node) {
        _superClass.prototype.parseClassSuper.call(this, node);

        if (node.superClass && this.isRelational("<")) {
          node.superTypeParameters = this.tsParseTypeArguments();
        }

        if (this.eatContextual("implements")) {
          node.implements = this.tsParseHeritageClause();
        }
      };

      _proto.parseObjPropValue = function parseObjPropValue(prop) {
        var _superClass$prototype2;

        if (this.isRelational("<")) {
          throw new Error("TODO");
        }

        for (var _len = arguments.length, args = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
          args[_key - 1] = arguments[_key];
        }

        (_superClass$prototype2 = _superClass.prototype.parseObjPropValue).call.apply(_superClass$prototype2, [this, prop].concat(args));
      };

      _proto.parseFunctionParams = function parseFunctionParams(node, allowModifiers) {
        var typeParameters = this.tsTryParseTypeParameters();
        if (typeParameters) node.typeParameters = typeParameters;

        _superClass.prototype.parseFunctionParams.call(this, node, allowModifiers);
      }; // `let x: number;`


      _proto.parseVarHead = function parseVarHead(decl) {
        _superClass.prototype.parseVarHead.call(this, decl);

        var type = this.tsTryParseTypeAnnotation();

        if (type) {
          decl.id.typeAnnotation = type;
          this.finishNode(decl.id, decl.id.type); // set end position to end of type
        }
      }; // parse the return type of an async arrow function - let foo = (async (): number => {});


      _proto.parseAsyncArrowFromCallExpression = function parseAsyncArrowFromCallExpression(node, call) {
        if (this.match(types.colon)) {
          node.returnType = this.tsParseTypeAnnotation();
        }

        return _superClass.prototype.parseAsyncArrowFromCallExpression.call(this, node, call);
      };

      _proto.parseMaybeAssign = function parseMaybeAssign() {
        // Note: When the JSX plugin is on, type assertions (`<T> x`) aren't valid syntax.
        var jsxError;

        for (var _len2 = arguments.length, args = new Array(_len2), _key2 = 0; _key2 < _len2; _key2++) {
          args[_key2] = arguments[_key2];
        }

        if (this.match(types.jsxTagStart)) {
          var context = this.curContext();
          assert(context === types$1.j_oTag); // Only time j_oTag is pushed is right after j_expr.

          assert(this.state.context[this.state.context.length - 2] === types$1.j_expr); // Prefer to parse JSX if possible. But may be an arrow fn.

          var _state = this.state.clone();

          try {
            var _superClass$prototype3;

            return (_superClass$prototype3 = _superClass.prototype.parseMaybeAssign).call.apply(_superClass$prototype3, [this].concat(args));
          } catch (err) {
            if (!(err instanceof SyntaxError)) {
              // istanbul ignore next: no such error is expected
              throw err;
            }

            this.state = _state; // Pop the context added by the jsxTagStart.

            assert(this.curContext() === types$1.j_oTag);
            this.state.context.pop();
            assert(this.curContext() === types$1.j_expr);
            this.state.context.pop();
            jsxError = err;
          }
        }

        if (jsxError === undefined && !this.isRelational("<")) {
          var _superClass$prototype4;

          return (_superClass$prototype4 = _superClass.prototype.parseMaybeAssign).call.apply(_superClass$prototype4, [this].concat(args));
        } // Either way, we're looking at a '<': tt.jsxTagStart or relational.


        var arrowExpression;
        var typeParameters;
        var state = this.state.clone();

        try {
          var _superClass$prototype5;

          // This is similar to TypeScript's `tryParseParenthesizedArrowFunctionExpression`.
          typeParameters = this.tsParseTypeParameters();
          arrowExpression = (_superClass$prototype5 = _superClass.prototype.parseMaybeAssign).call.apply(_superClass$prototype5, [this].concat(args));

          if (arrowExpression.type !== "ArrowFunctionExpression") {
            this.unexpected(); // Go to the catch block (needs a SyntaxError).
          }
        } catch (err) {
          var _superClass$prototype6;

          if (!(err instanceof SyntaxError)) {
            // istanbul ignore next: no such error is expected
            throw err;
          }

          if (jsxError) {
            throw jsxError;
          } // Try parsing a type cast instead of an arrow function.
          // This will never happen outside of JSX.
          // (Because in JSX the '<' should be a jsxTagStart and not a relational.


          assert(!this.hasPlugin("jsx")); // Parsing an arrow function failed, so try a type cast.

          this.state = state; // This will start with a type assertion (via parseMaybeUnary).
          // But don't directly call `this.tsParseTypeAssertion` because we want to handle any binary after it.

          return (_superClass$prototype6 = _superClass.prototype.parseMaybeAssign).call.apply(_superClass$prototype6, [this].concat(args));
        } // Correct TypeScript code should have at least 1 type parameter, but don't crash on bad code.


        if (typeParameters && typeParameters.params.length !== 0) {
          this.resetStartLocationFromNode(arrowExpression, typeParameters.params[0]);
        }

        arrowExpression.typeParameters = typeParameters;
        return arrowExpression;
      }; // Handle type assertions


      _proto.parseMaybeUnary = function parseMaybeUnary(refShorthandDefaultPos) {
        if (!this.hasPlugin("jsx") && this.eatRelational("<")) {
          return this.tsParseTypeAssertion();
        } else {
          return _superClass.prototype.parseMaybeUnary.call(this, refShorthandDefaultPos);
        }
      };

      _proto.parseArrow = function parseArrow(node) {
        if (this.match(types.colon)) {
          // This is different from how the TS parser does it.
          // TS uses lookahead. Babylon parses it as a parenthesized expression and converts.
          var state = this.state.clone();

          try {
            var returnType = this.tsParseTypeOrTypePredicateAnnotation(types.colon);
            if (this.canInsertSemicolon()) this.unexpected();
            if (!this.match(types.arrow)) this.unexpected();
            node.returnType = returnType;
          } catch (err) {
            if (err instanceof SyntaxError) {
              this.state = state;
            } else {
              // istanbul ignore next: no such error is expected
              throw err;
            }
          }
        }

        return _superClass.prototype.parseArrow.call(this, node);
      }; // Allow type annotations inside of a parameter list.


      _proto.parseAssignableListItemTypes = function parseAssignableListItemTypes(param) {
        if (this.eat(types.question)) {
          if (param.type !== "Identifier") {
            throw this.raise(param.start, "A binding pattern parameter cannot be optional in an implementation signature.");
          }

          param.optional = true;
        }

        var type = this.tsTryParseTypeAnnotation();
        if (type) param.typeAnnotation = type;
        return this.finishNode(param, param.type);
      };

      _proto.toAssignable = function toAssignable(node, isBinding, contextDescription) {
        switch (node.type) {
          case "TSTypeCastExpression":
            return _superClass.prototype.toAssignable.call(this, this.typeCastToParameter(node), isBinding, contextDescription);

          case "TSParameterProperty":
            return _superClass.prototype.toAssignable.call(this, node, isBinding, contextDescription);

          case "TSAsExpression":
            node.expression = this.toAssignable(node.expression, isBinding, contextDescription);
            return node;

          default:
            return _superClass.prototype.toAssignable.call(this, node, isBinding, contextDescription);
        }
      };

      _proto.checkLVal = function checkLVal(expr, isBinding, checkClashes, contextDescription) {
        switch (expr.type) {
          case "TSTypeCastExpression":
            // Allow "typecasts" to appear on the left of assignment expressions,
            // because it may be in an arrow function.
            // e.g. `const f = (foo: number = 0) => foo;`
            return;

          case "TSParameterProperty":
            this.checkLVal(expr.parameter, isBinding, checkClashes, "parameter property");
            return;

          case "TSAsExpression":
            this.checkLVal(expr.expression, isBinding, checkClashes, contextDescription);
            return;

          default:
            _superClass.prototype.checkLVal.call(this, expr, isBinding, checkClashes, contextDescription);

            return;
        }
      };

      _proto.parseBindingAtom = function parseBindingAtom() {
        switch (this.state.type) {
          case types._this:
            // "this" may be the name of a parameter, so allow it.
            return this.parseIdentifier(
            /* liberal */
            true);

          default:
            return _superClass.prototype.parseBindingAtom.call(this);
        }
      }; // === === === === === === === === === === === === === === === ===
      // Note: All below methods are duplicates of something in flow.js.
      // Not sure what the best way to combine these is.
      // === === === === === === === === === === === === === === === ===


      _proto.isClassMethod = function isClassMethod() {
        return this.isRelational("<") || _superClass.prototype.isClassMethod.call(this);
      };

      _proto.isClassProperty = function isClassProperty() {
        return this.match(types.colon) || _superClass.prototype.isClassProperty.call(this);
      };

      _proto.parseMaybeDefault = function parseMaybeDefault() {
        var _superClass$prototype7;

        for (var _len3 = arguments.length, args = new Array(_len3), _key3 = 0; _key3 < _len3; _key3++) {
          args[_key3] = arguments[_key3];
        }

        var node = (_superClass$prototype7 = _superClass.prototype.parseMaybeDefault).call.apply(_superClass$prototype7, [this].concat(args));

        if (node.type === "AssignmentPattern" && node.typeAnnotation && node.right.start < node.typeAnnotation.start) {
          this.raise(node.typeAnnotation.start, "Type annotations must come before default assignments, " + "e.g. instead of `age = 25: number` use `age: number = 25`");
        }

        return node;
      }; // ensure that inside types, we bypass the jsx parser plugin


      _proto.readToken = function readToken(code) {
        if (this.state.inType && (code === 62 || code === 60)) {
          return this.finishOp(types.relational, 1);
        } else {
          return _superClass.prototype.readToken.call(this, code);
        }
      };

      _proto.toAssignableList = function toAssignableList(exprList, isBinding, contextDescription) {
        for (var i = 0; i < exprList.length; i++) {
          var expr = exprList[i];

          if (expr && expr.type === "TSTypeCastExpression") {
            exprList[i] = this.typeCastToParameter(expr);
          }
        }

        return _superClass.prototype.toAssignableList.call(this, exprList, isBinding, contextDescription);
      };

      _proto.typeCastToParameter = function typeCastToParameter(node) {
        node.expression.typeAnnotation = node.typeAnnotation;
        return this.finishNodeAt(node.expression, node.expression.type, node.typeAnnotation.end, node.typeAnnotation.loc.end);
      };

      _proto.toReferencedList = function toReferencedList(exprList) {
        for (var i = 0; i < exprList.length; i++) {
          var expr = exprList[i];

          if (expr && expr._exprListItem && expr.type === "TsTypeCastExpression") {
            this.raise(expr.start, "Did not expect a type annotation here.");
          }
        }

        return exprList;
      };

      _proto.shouldParseArrow = function shouldParseArrow() {
        return this.match(types.colon) || _superClass.prototype.shouldParseArrow.call(this);
      };

      _proto.shouldParseAsyncArrow = function shouldParseAsyncArrow() {
        return this.match(types.colon) || _superClass.prototype.shouldParseAsyncArrow.call(this);
      };

      return _class;
    }(superClass)
  );
});

plugins.estree = estreePlugin;
plugins.flow = flowPlugin;
plugins.jsx = jsxPlugin;
plugins.typescript = typescriptPlugin;
function parse(input, options) {
  if (options && options.sourceType === "unambiguous") {
    options = Object.assign({}, options);

    try {
      options.sourceType = "module";
      var ast = getParser(options, input).parse(); // Rather than try to parse as a script first, we opt to parse as a module and convert back
      // to a script where possible to avoid having to do a full re-parse of the input content.

      if (!hasModuleSyntax(ast)) ast.program.sourceType = "script";
      return ast;
    } catch (moduleError) {
      try {
        options.sourceType = "script";
        return getParser(options, input).parse();
      } catch (scriptError) {}

      throw moduleError;
    }
  } else {
    return getParser(options, input).parse();
  }
}
function parseExpression(input, options) {
  var parser = getParser(options, input);

  if (parser.options.strictMode) {
    parser.state.strict = true;
  }

  return parser.getExpression();
}
function getParser(options, input) {
  var cls = options && options.plugins ? getParserClass(options.plugins) : Parser;
  return new cls(options, input);
}

var parserClassCache = {};
/** Get a Parser class with plugins applied. */

function getParserClass(pluginsFromOptions) {
  if (pluginsFromOptions.indexOf("decorators") >= 0 && pluginsFromOptions.indexOf("decorators2") >= 0) {
    throw new Error("Cannot use decorators and decorators2 plugin together");
  } // Filter out just the plugins that have an actual mixin associated with them.


  var pluginList = pluginsFromOptions.filter(function (p) {
    return p === "estree" || p === "flow" || p === "jsx" || p === "typescript";
  });

  if (pluginList.indexOf("flow") >= 0) {
    // ensure flow plugin loads last
    pluginList = pluginList.filter(function (plugin) {
      return plugin !== "flow";
    });
    pluginList.push("flow");
  }

  if (pluginList.indexOf("flow") >= 0 && pluginList.indexOf("typescript") >= 0) {
    throw new Error("Cannot combine flow and typescript plugins.");
  }

  if (pluginList.indexOf("typescript") >= 0) {
    // ensure typescript plugin loads last
    pluginList = pluginList.filter(function (plugin) {
      return plugin !== "typescript";
    });
    pluginList.push("typescript");
  }

  if (pluginList.indexOf("estree") >= 0) {
    // ensure estree plugin loads first
    pluginList = pluginList.filter(function (plugin) {
      return plugin !== "estree";
    });
    pluginList.unshift("estree");
  }

  var key = pluginList.join("/");
  var cls = parserClassCache[key];

  if (!cls) {
    cls = Parser;

    for (var _i2 = 0, _pluginList2 = pluginList; _i2 < _pluginList2.length; _i2++) {
      var plugin = _pluginList2[_i2];
      cls = plugins[plugin](cls);
    }

    parserClassCache[key] = cls;
  }

  return cls;
}

function hasModuleSyntax(ast) {
  return ast.program.body.some(function (child) {
    return child.type === "ImportDeclaration" && (!child.importKind || child.importKind === "value") || child.type === "ExportNamedDeclaration" && (!child.exportKind || child.exportKind === "value") || child.type === "ExportAllDeclaration" && (!child.exportKind || child.exportKind === "value") || child.type === "ExportDefaultDeclaration";
  });
}

exports.parse = parse;
exports.parseExpression = parseExpression;
exports.tokTypes = types;


/***/ }),
/* 290 */
/***/ (function(module, exports, __webpack_require__) {

var baseKeys = __webpack_require__(261),
    getTag = __webpack_require__(85),
    isArguments = __webpack_require__(113),
    isArray = __webpack_require__(16),
    isArrayLike = __webpack_require__(117),
    isBuffer = __webpack_require__(114),
    isPrototype = __webpack_require__(116),
    isTypedArray = __webpack_require__(181);

/** `Object#toString` result references. */
var mapTag = '[object Map]',
    setTag = '[object Set]';

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * Checks if `value` is an empty object, collection, map, or set.
 *
 * Objects are considered empty if they have no own enumerable string keyed
 * properties.
 *
 * Array-like values such as `arguments` objects, arrays, buffers, strings, or
 * jQuery-like collections are considered empty if they have a `length` of `0`.
 * Similarly, maps and sets are considered empty if they have a `size` of `0`.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is empty, else `false`.
 * @example
 *
 * _.isEmpty(null);
 * // => true
 *
 * _.isEmpty(true);
 * // => true
 *
 * _.isEmpty(1);
 * // => true
 *
 * _.isEmpty([1, 2, 3]);
 * // => false
 *
 * _.isEmpty({ 'a': 1 });
 * // => false
 */
function isEmpty(value) {
  if (value == null) {
    return true;
  }
  if (isArrayLike(value) &&
      (isArray(value) || typeof value == 'string' || typeof value.splice == 'function' ||
        isBuffer(value) || isTypedArray(value) || isArguments(value))) {
    return !value.length;
  }
  var tag = getTag(value);
  if (tag == mapTag || tag == setTag) {
    return !value.size;
  }
  if (isPrototype(value)) {
    return !baseKeys(value).length;
  }
  for (var key in value) {
    if (hasOwnProperty.call(value, key)) {
      return false;
    }
  }
  return true;
}

module.exports = isEmpty;


/***/ }),
/* 291 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.hasSource = hasSource;
exports.setSource = setSource;
exports.getSource = getSource;
exports.clearSources = clearSources;


let cachedSources = new Map(); /* This Source Code Form is subject to the terms of the Mozilla Public
                                * License, v. 2.0. If a copy of the MPL was not distributed with this
                                * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function hasSource(sourceId) {
  return cachedSources.has(sourceId);
}

function setSource(source) {
  cachedSources.set(source.id, source);
}

function getSource(sourceId) {
  if (!cachedSources.has(sourceId)) {
    throw new Error(`Parser: source ${sourceId} was not provided.`);
  }

  return cachedSources.get(sourceId);
}

function clearSources() {
  cachedSources = new Map();
}

/***/ }),
/* 292 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.containsPosition = containsPosition;
exports.containsLocation = containsLocation;
exports.nodeContainsPosition = nodeContainsPosition;
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function startsBefore(a, b) {
  let before = a.start.line < b.line;
  if (a.start.line === b.line) {
    before = a.start.column >= 0 && b.column >= 0 ? a.start.column <= b.column : true;
  }
  return before;
}

function endsAfter(a, b) {
  let after = a.end.line > b.line;
  if (a.end.line === b.line) {
    after = a.end.column >= 0 && b.column >= 0 ? a.end.column >= b.column : true;
  }
  return after;
}

function containsPosition(a, b) {
  return startsBefore(a, b) && endsAfter(a, b);
}

function containsLocation(a, b) {
  return containsPosition(a, b.start) && containsPosition(a, b.end);
}

function nodeContainsPosition(node, position) {
  return containsPosition(node.loc, position);
}

/***/ }),
/* 293 */
/***/ (function(module, exports, __webpack_require__) {

var arrayPush = __webpack_require__(184),
    isFlattenable = __webpack_require__(763);

/**
 * The base implementation of `_.flatten` with support for restricting flattening.
 *
 * @private
 * @param {Array} array The array to flatten.
 * @param {number} depth The maximum recursion depth.
 * @param {boolean} [predicate=isFlattenable] The function invoked per iteration.
 * @param {boolean} [isStrict] Restrict to values that pass `predicate` checks.
 * @param {Array} [result=[]] The initial result value.
 * @returns {Array} Returns the new flattened array.
 */
function baseFlatten(array, depth, predicate, isStrict, result) {
  var index = -1,
      length = array.length;

  predicate || (predicate = isFlattenable);
  result || (result = []);

  while (++index < length) {
    var value = array[index];
    if (depth > 0 && predicate(value)) {
      if (depth > 1) {
        // Recursively flatten arrays (susceptible to call stack limits).
        baseFlatten(value, depth - 1, predicate, isStrict, result);
      } else {
        arrayPush(result, value);
      }
    } else if (!isStrict) {
      result[result.length] = value;
    }
  }
  return result;
}

module.exports = baseFlatten;


/***/ }),
/* 294 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = getFunctionName;

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

// Perform ES6's anonymous function name inference for all
// locations where static analysis is possible.
// eslint-disable-next-line complexity
function getFunctionName(node, parent) {
  if (t.isIdentifier(node.id)) {
    return node.id.name;
  }

  if (t.isObjectMethod(node, { computed: false }) || t.isClassMethod(node, { computed: false })) {
    const key = node.key;

    if (t.isIdentifier(key)) {
      return key.name;
    }
    if (t.isStringLiteral(key)) {
      return key.value;
    }
    if (t.isNumericLiteral(key)) {
      return `${key.value}`;
    }
  }

  if (t.isObjectProperty(parent, { computed: false, value: node }) ||
  // TODO: Babylon 6 doesn't support computed class props. It is included
  // here so that it is most flexible. Once Babylon 7 is used, this
  // can change to use computed: false like ObjectProperty.
  t.isClassProperty(parent, { value: node }) && !parent.computed) {
    const key = parent.key;

    if (t.isIdentifier(key)) {
      return key.name;
    }
    if (t.isStringLiteral(key)) {
      return key.value;
    }
    if (t.isNumericLiteral(key)) {
      return `${key.value}`;
    }
  }

  if (t.isAssignmentExpression(parent, { operator: "=", right: node })) {
    if (t.isIdentifier(parent.left)) {
      return parent.left.name;
    }

    // This case is not supported in standard ES6 name inference, but it
    // is included here since it is still a helpful case during debugging.
    if (t.isMemberExpression(parent.left, { computed: false })) {
      return parent.left.property.name;
    }
  }

  if (t.isAssignmentPattern(parent, { right: node }) && t.isIdentifier(parent.left)) {
    return parent.left.name;
  }

  if (t.isVariableDeclarator(parent, { init: node }) && t.isIdentifier(parent.id)) {
    return parent.id.name;
  }

  if (t.isExportDefaultDeclaration(parent, { declaration: node }) && t.isFunctionDeclaration(node)) {
    return "default";
  }

  return "anonymous";
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/***/ }),
/* 295 */
/***/ (function(module, exports, __webpack_require__) {

var baseMatches = __webpack_require__(769),
    baseMatchesProperty = __webpack_require__(777),
    identity = __webpack_require__(194),
    isArray = __webpack_require__(16),
    property = __webpack_require__(781);

/**
 * The base implementation of `_.iteratee`.
 *
 * @private
 * @param {*} [value=_.identity] The value to convert to an iteratee.
 * @returns {Function} Returns the iteratee.
 */
function baseIteratee(value) {
  // Don't store the `typeof` result in a variable to avoid a JIT bug in Safari 9.
  // See https://bugs.webkit.org/show_bug.cgi?id=156034 for more details.
  if (typeof value == 'function') {
    return value;
  }
  if (value == null) {
    return identity;
  }
  if (typeof value == 'object') {
    return isArray(value)
      ? baseMatchesProperty(value[0], value[1])
      : baseMatches(value);
  }
  return property(value);
}

module.exports = baseIteratee;


/***/ }),
/* 296 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsEqualDeep = __webpack_require__(771),
    isObjectLike = __webpack_require__(21);

/**
 * The base implementation of `_.isEqual` which supports partial comparisons
 * and tracks traversed objects.
 *
 * @private
 * @param {*} value The value to compare.
 * @param {*} other The other value to compare.
 * @param {boolean} bitmask The bitmask flags.
 *  1 - Unordered comparison
 *  2 - Partial comparison
 * @param {Function} [customizer] The function to customize comparisons.
 * @param {Object} [stack] Tracks traversed `value` and `other` objects.
 * @returns {boolean} Returns `true` if the values are equivalent, else `false`.
 */
function baseIsEqual(value, other, bitmask, customizer, stack) {
  if (value === other) {
    return true;
  }
  if (value == null || other == null || (!isObjectLike(value) && !isObjectLike(other))) {
    return value !== value && other !== other;
  }
  return baseIsEqualDeep(value, other, bitmask, customizer, baseIsEqual, stack);
}

module.exports = baseIsEqual;


/***/ }),
/* 297 */
/***/ (function(module, exports, __webpack_require__) {

var SetCache = __webpack_require__(188),
    arraySome = __webpack_require__(772),
    cacheHas = __webpack_require__(190);

/** Used to compose bitmasks for value comparisons. */
var COMPARE_PARTIAL_FLAG = 1,
    COMPARE_UNORDERED_FLAG = 2;

/**
 * A specialized version of `baseIsEqualDeep` for arrays with support for
 * partial deep comparisons.
 *
 * @private
 * @param {Array} array The array to compare.
 * @param {Array} other The other array to compare.
 * @param {number} bitmask The bitmask flags. See `baseIsEqual` for more details.
 * @param {Function} customizer The function to customize comparisons.
 * @param {Function} equalFunc The function to determine equivalents of values.
 * @param {Object} stack Tracks traversed `array` and `other` objects.
 * @returns {boolean} Returns `true` if the arrays are equivalent, else `false`.
 */
function equalArrays(array, other, bitmask, customizer, equalFunc, stack) {
  var isPartial = bitmask & COMPARE_PARTIAL_FLAG,
      arrLength = array.length,
      othLength = other.length;

  if (arrLength != othLength && !(isPartial && othLength > arrLength)) {
    return false;
  }
  // Assume cyclic values are equal.
  var stacked = stack.get(array);
  if (stacked && stack.get(other)) {
    return stacked == other;
  }
  var index = -1,
      result = true,
      seen = (bitmask & COMPARE_UNORDERED_FLAG) ? new SetCache : undefined;

  stack.set(array, other);
  stack.set(other, array);

  // Ignore non-index properties.
  while (++index < arrLength) {
    var arrValue = array[index],
        othValue = other[index];

    if (customizer) {
      var compared = isPartial
        ? customizer(othValue, arrValue, index, other, array, stack)
        : customizer(arrValue, othValue, index, array, other, stack);
    }
    if (compared !== undefined) {
      if (compared) {
        continue;
      }
      result = false;
      break;
    }
    // Recursively compare arrays (susceptible to call stack limits).
    if (seen) {
      if (!arraySome(other, function(othValue, othIndex) {
            if (!cacheHas(seen, othIndex) &&
                (arrValue === othValue || equalFunc(arrValue, othValue, bitmask, customizer, stack))) {
              return seen.push(othIndex);
            }
          })) {
        result = false;
        break;
      }
    } else if (!(
          arrValue === othValue ||
            equalFunc(arrValue, othValue, bitmask, customizer, stack)
        )) {
      result = false;
      break;
    }
  }
  stack['delete'](array);
  stack['delete'](other);
  return result;
}

module.exports = equalArrays;


/***/ }),
/* 298 */
/***/ (function(module, exports, __webpack_require__) {

var isObject = __webpack_require__(26);

/**
 * Checks if `value` is suitable for strict equality comparisons, i.e. `===`.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` if suitable for strict
 *  equality comparisons, else `false`.
 */
function isStrictComparable(value) {
  return value === value && !isObject(value);
}

module.exports = isStrictComparable;


/***/ }),
/* 299 */
/***/ (function(module, exports) {

/**
 * A specialized version of `matchesProperty` for source values suitable
 * for strict equality comparisons, i.e. `===`.
 *
 * @private
 * @param {string} key The key of the property to get.
 * @param {*} srcValue The value to match.
 * @returns {Function} Returns the new spec function.
 */
function matchesStrictComparable(key, srcValue) {
  return function(object) {
    if (object == null) {
      return false;
    }
    return object[key] === srcValue &&
      (srcValue !== undefined || (key in Object(object)));
  };
}

module.exports = matchesStrictComparable;


/***/ }),
/* 300 */
/***/ (function(module, exports, __webpack_require__) {

var toFinite = __webpack_require__(784);

/**
 * Converts `value` to an integer.
 *
 * **Note:** This method is loosely based on
 * [`ToInteger`](http://www.ecma-international.org/ecma-262/7.0/#sec-tointeger).
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to convert.
 * @returns {number} Returns the converted integer.
 * @example
 *
 * _.toInteger(3.2);
 * // => 3
 *
 * _.toInteger(Number.MIN_VALUE);
 * // => 0
 *
 * _.toInteger(Infinity);
 * // => 1.7976931348623157e+308
 *
 * _.toInteger('3.2');
 * // => 3
 */
function toInteger(value) {
  var result = toFinite(value),
      remainder = result % 1;

  return result === result ? (remainder ? result - remainder : result) : 0;
}

module.exports = toInteger;


/***/ }),
/* 301 */,
/* 302 */,
/* 303 */,
/* 304 */,
/* 305 */,
/* 306 */,
/* 307 */,
/* 308 */,
/* 309 */,
/* 310 */,
/* 311 */,
/* 312 */,
/* 313 */,
/* 314 */,
/* 315 */,
/* 316 */,
/* 317 */,
/* 318 */,
/* 319 */,
/* 320 */,
/* 321 */,
/* 322 */,
/* 323 */,
/* 324 */,
/* 325 */,
/* 326 */,
/* 327 */,
/* 328 */,
/* 329 */,
/* 330 */,
/* 331 */,
/* 332 */,
/* 333 */,
/* 334 */,
/* 335 */,
/* 336 */,
/* 337 */,
/* 338 */,
/* 339 */,
/* 340 */,
/* 341 */,
/* 342 */,
/* 343 */,
/* 344 */,
/* 345 */,
/* 346 */,
/* 347 */,
/* 348 */,
/* 349 */,
/* 350 */,
/* 351 */,
/* 352 */,
/* 353 */,
/* 354 */,
/* 355 */,
/* 356 */,
/* 357 */,
/* 358 */,
/* 359 */,
/* 360 */,
/* 361 */,
/* 362 */,
/* 363 */,
/* 364 */,
/* 365 */,
/* 366 */,
/* 367 */,
/* 368 */,
/* 369 */,
/* 370 */,
/* 371 */,
/* 372 */,
/* 373 */,
/* 374 */,
/* 375 */,
/* 376 */,
/* 377 */,
/* 378 */,
/* 379 */,
/* 380 */,
/* 381 */,
/* 382 */,
/* 383 */,
/* 384 */,
/* 385 */,
/* 386 */,
/* 387 */,
/* 388 */,
/* 389 */,
/* 390 */,
/* 391 */,
/* 392 */,
/* 393 */,
/* 394 */,
/* 395 */,
/* 396 */,
/* 397 */,
/* 398 */,
/* 399 */,
/* 400 */,
/* 401 */,
/* 402 */,
/* 403 */,
/* 404 */,
/* 405 */,
/* 406 */,
/* 407 */,
/* 408 */,
/* 409 */,
/* 410 */,
/* 411 */,
/* 412 */,
/* 413 */,
/* 414 */,
/* 415 */,
/* 416 */,
/* 417 */,
/* 418 */,
/* 419 */,
/* 420 */,
/* 421 */,
/* 422 */,
/* 423 */,
/* 424 */,
/* 425 */,
/* 426 */,
/* 427 */,
/* 428 */,
/* 429 */,
/* 430 */,
/* 431 */,
/* 432 */,
/* 433 */,
/* 434 */,
/* 435 */,
/* 436 */,
/* 437 */,
/* 438 */,
/* 439 */,
/* 440 */,
/* 441 */,
/* 442 */,
/* 443 */,
/* 444 */,
/* 445 */,
/* 446 */,
/* 447 */,
/* 448 */,
/* 449 */,
/* 450 */,
/* 451 */,
/* 452 */,
/* 453 */,
/* 454 */,
/* 455 */,
/* 456 */,
/* 457 */,
/* 458 */,
/* 459 */,
/* 460 */,
/* 461 */,
/* 462 */,
/* 463 */,
/* 464 */,
/* 465 */,
/* 466 */,
/* 467 */,
/* 468 */,
/* 469 */,
/* 470 */,
/* 471 */,
/* 472 */,
/* 473 */,
/* 474 */,
/* 475 */,
/* 476 */,
/* 477 */,
/* 478 */,
/* 479 */,
/* 480 */,
/* 481 */,
/* 482 */,
/* 483 */,
/* 484 */,
/* 485 */,
/* 486 */,
/* 487 */,
/* 488 */,
/* 489 */,
/* 490 */,
/* 491 */,
/* 492 */,
/* 493 */,
/* 494 */,
/* 495 */,
/* 496 */,
/* 497 */,
/* 498 */,
/* 499 */,
/* 500 */,
/* 501 */,
/* 502 */,
/* 503 */,
/* 504 */,
/* 505 */,
/* 506 */,
/* 507 */,
/* 508 */,
/* 509 */,
/* 510 */,
/* 511 */,
/* 512 */,
/* 513 */,
/* 514 */,
/* 515 */,
/* 516 */,
/* 517 */,
/* 518 */,
/* 519 */,
/* 520 */,
/* 521 */,
/* 522 */,
/* 523 */,
/* 524 */,
/* 525 */,
/* 526 */,
/* 527 */,
/* 528 */,
/* 529 */,
/* 530 */,
/* 531 */,
/* 532 */,
/* 533 */,
/* 534 */,
/* 535 */,
/* 536 */,
/* 537 */,
/* 538 */,
/* 539 */,
/* 540 */,
/* 541 */,
/* 542 */,
/* 543 */,
/* 544 */,
/* 545 */,
/* 546 */,
/* 547 */,
/* 548 */,
/* 549 */,
/* 550 */,
/* 551 */,
/* 552 */,
/* 553 */,
/* 554 */,
/* 555 */,
/* 556 */,
/* 557 */,
/* 558 */,
/* 559 */,
/* 560 */,
/* 561 */,
/* 562 */,
/* 563 */,
/* 564 */,
/* 565 */,
/* 566 */,
/* 567 */,
/* 568 */,
/* 569 */,
/* 570 */,
/* 571 */,
/* 572 */,
/* 573 */,
/* 574 */,
/* 575 */,
/* 576 */,
/* 577 */,
/* 578 */,
/* 579 */,
/* 580 */,
/* 581 */,
/* 582 */,
/* 583 */,
/* 584 */,
/* 585 */,
/* 586 */,
/* 587 */,
/* 588 */,
/* 589 */,
/* 590 */,
/* 591 */,
/* 592 */,
/* 593 */,
/* 594 */,
/* 595 */,
/* 596 */,
/* 597 */,
/* 598 */,
/* 599 */,
/* 600 */,
/* 601 */,
/* 602 */,
/* 603 */,
/* 604 */,
/* 605 */,
/* 606 */,
/* 607 */,
/* 608 */,
/* 609 */,
/* 610 */,
/* 611 */,
/* 612 */,
/* 613 */,
/* 614 */,
/* 615 */,
/* 616 */,
/* 617 */,
/* 618 */,
/* 619 */,
/* 620 */,
/* 621 */,
/* 622 */,
/* 623 */,
/* 624 */,
/* 625 */,
/* 626 */,
/* 627 */,
/* 628 */,
/* 629 */,
/* 630 */,
/* 631 */,
/* 632 */,
/* 633 */,
/* 634 */,
/* 635 */,
/* 636 */,
/* 637 */,
/* 638 */,
/* 639 */,
/* 640 */,
/* 641 */,
/* 642 */,
/* 643 */,
/* 644 */,
/* 645 */,
/* 646 */,
/* 647 */,
/* 648 */,
/* 649 */,
/* 650 */,
/* 651 */,
/* 652 */,
/* 653 */,
/* 654 */,
/* 655 */,
/* 656 */,
/* 657 */,
/* 658 */,
/* 659 */,
/* 660 */,
/* 661 */,
/* 662 */
/***/ (function(module, exports, __webpack_require__) {

module.exports = __webpack_require__(663);


/***/ }),
/* 663 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _closest = __webpack_require__(255);

var _getSymbols = __webpack_require__(193);

var _getSymbols2 = _interopRequireDefault(_getSymbols);

var _ast = __webpack_require__(51);

var _getScopes = __webpack_require__(765);

var _getScopes2 = _interopRequireDefault(_getScopes);

var _sources = __webpack_require__(291);

var _findOutOfScopeLocations = __webpack_require__(767);

var _findOutOfScopeLocations2 = _interopRequireDefault(_findOutOfScopeLocations);

var _steps = __webpack_require__(787);

var _getEmptyLines = __webpack_require__(789);

var _getEmptyLines2 = _interopRequireDefault(_getEmptyLines);

var _validate = __webpack_require__(800);

var _frameworks = __webpack_require__(801);

var _pauseLocation = __webpack_require__(802);

var _devtoolsUtils = __webpack_require__(27);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { workerHandler } = _devtoolsUtils.workerUtils;

self.onmessage = workerHandler({
  getClosestExpression: _closest.getClosestExpression,
  findOutOfScopeLocations: _findOutOfScopeLocations2.default,
  getSymbols: _getSymbols2.default,
  getScopes: _getScopes2.default,
  clearSymbols: _getSymbols.clearSymbols,
  clearScopes: _getScopes.clearScopes,
  clearASTs: _ast.clearASTs,
  hasSource: _sources.hasSource,
  setSource: _sources.setSource,
  clearSources: _sources.clearSources,
  isInvalidPauseLocation: _pauseLocation.isInvalidPauseLocation,
  getNextStep: _steps.getNextStep,
  getEmptyLines: _getEmptyLines2.default,
  hasSyntaxError: _validate.hasSyntaxError,
  getFramework: _frameworks.getFramework
});

/***/ }),
/* 664 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = void 0;

var _buildMatchMemberExpression = _interopRequireDefault(__webpack_require__(256));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var isReactComponent = (0, _buildMatchMemberExpression.default)("React.Component");
var _default = isReactComponent;
exports.default = _default;

/***/ }),
/* 665 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


let fastProto = null;

// Creates an object with permanently fast properties in V8. See Toon Verwaest's
// post https://medium.com/@tverwaes/setting-up-prototypes-in-v8-ec9c9491dfe2#5f62
// for more details. Use %HasFastProperties(object) and the Node.js flag
// --allow-natives-syntax to check whether an object has fast properties.
function FastObject(o) {
	// A prototype object will have "fast properties" enabled once it is checked
	// against the inline property cache of a function, e.g. fastProto.property:
	// https://github.com/v8/v8/blob/6.0.122/test/mjsunit/fast-prototype.js#L48-L63
	if (fastProto !== null && typeof fastProto.property) {
		const result = fastProto;
		fastProto = FastObject.prototype = null;
		return result;
	}
	fastProto = FastObject.prototype = o == null ? Object.create(null) : o;
	return new FastObject;
}

// Initialize the inline property cache of FastObject
FastObject();

module.exports = function toFastproperties(o) {
	return FastObject(o);
};


/***/ }),
/* 666 */
/***/ (function(module, exports, __webpack_require__) {

/*
  Copyright (C) 2013 Yusuke Suzuki <utatane.tea@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


(function () {
    'use strict';

    exports.ast = __webpack_require__(667);
    exports.code = __webpack_require__(259);
    exports.keyword = __webpack_require__(668);
}());
/* vim: set sw=4 ts=4 et tw=80 : */


/***/ }),
/* 667 */
/***/ (function(module, exports) {

/*
  Copyright (C) 2013 Yusuke Suzuki <utatane.tea@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS'
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

(function () {
    'use strict';

    function isExpression(node) {
        if (node == null) { return false; }
        switch (node.type) {
            case 'ArrayExpression':
            case 'AssignmentExpression':
            case 'BinaryExpression':
            case 'CallExpression':
            case 'ConditionalExpression':
            case 'FunctionExpression':
            case 'Identifier':
            case 'Literal':
            case 'LogicalExpression':
            case 'MemberExpression':
            case 'NewExpression':
            case 'ObjectExpression':
            case 'SequenceExpression':
            case 'ThisExpression':
            case 'UnaryExpression':
            case 'UpdateExpression':
                return true;
        }
        return false;
    }

    function isIterationStatement(node) {
        if (node == null) { return false; }
        switch (node.type) {
            case 'DoWhileStatement':
            case 'ForInStatement':
            case 'ForStatement':
            case 'WhileStatement':
                return true;
        }
        return false;
    }

    function isStatement(node) {
        if (node == null) { return false; }
        switch (node.type) {
            case 'BlockStatement':
            case 'BreakStatement':
            case 'ContinueStatement':
            case 'DebuggerStatement':
            case 'DoWhileStatement':
            case 'EmptyStatement':
            case 'ExpressionStatement':
            case 'ForInStatement':
            case 'ForStatement':
            case 'IfStatement':
            case 'LabeledStatement':
            case 'ReturnStatement':
            case 'SwitchStatement':
            case 'ThrowStatement':
            case 'TryStatement':
            case 'VariableDeclaration':
            case 'WhileStatement':
            case 'WithStatement':
                return true;
        }
        return false;
    }

    function isSourceElement(node) {
      return isStatement(node) || node != null && node.type === 'FunctionDeclaration';
    }

    function trailingStatement(node) {
        switch (node.type) {
        case 'IfStatement':
            if (node.alternate != null) {
                return node.alternate;
            }
            return node.consequent;

        case 'LabeledStatement':
        case 'ForStatement':
        case 'ForInStatement':
        case 'WhileStatement':
        case 'WithStatement':
            return node.body;
        }
        return null;
    }

    function isProblematicIfStatement(node) {
        var current;

        if (node.type !== 'IfStatement') {
            return false;
        }
        if (node.alternate == null) {
            return false;
        }
        current = node.consequent;
        do {
            if (current.type === 'IfStatement') {
                if (current.alternate == null)  {
                    return true;
                }
            }
            current = trailingStatement(current);
        } while (current);

        return false;
    }

    module.exports = {
        isExpression: isExpression,
        isStatement: isStatement,
        isIterationStatement: isIterationStatement,
        isSourceElement: isSourceElement,
        isProblematicIfStatement: isProblematicIfStatement,

        trailingStatement: trailingStatement
    };
}());
/* vim: set sw=4 ts=4 et tw=80 : */


/***/ }),
/* 668 */
/***/ (function(module, exports, __webpack_require__) {

/*
  Copyright (C) 2013 Yusuke Suzuki <utatane.tea@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

(function () {
    'use strict';

    var code = __webpack_require__(259);

    function isStrictModeReservedWordES6(id) {
        switch (id) {
        case 'implements':
        case 'interface':
        case 'package':
        case 'private':
        case 'protected':
        case 'public':
        case 'static':
        case 'let':
            return true;
        default:
            return false;
        }
    }

    function isKeywordES5(id, strict) {
        // yield should not be treated as keyword under non-strict mode.
        if (!strict && id === 'yield') {
            return false;
        }
        return isKeywordES6(id, strict);
    }

    function isKeywordES6(id, strict) {
        if (strict && isStrictModeReservedWordES6(id)) {
            return true;
        }

        switch (id.length) {
        case 2:
            return (id === 'if') || (id === 'in') || (id === 'do');
        case 3:
            return (id === 'var') || (id === 'for') || (id === 'new') || (id === 'try');
        case 4:
            return (id === 'this') || (id === 'else') || (id === 'case') ||
                (id === 'void') || (id === 'with') || (id === 'enum');
        case 5:
            return (id === 'while') || (id === 'break') || (id === 'catch') ||
                (id === 'throw') || (id === 'const') || (id === 'yield') ||
                (id === 'class') || (id === 'super');
        case 6:
            return (id === 'return') || (id === 'typeof') || (id === 'delete') ||
                (id === 'switch') || (id === 'export') || (id === 'import');
        case 7:
            return (id === 'default') || (id === 'finally') || (id === 'extends');
        case 8:
            return (id === 'function') || (id === 'continue') || (id === 'debugger');
        case 10:
            return (id === 'instanceof');
        default:
            return false;
        }
    }

    function isReservedWordES5(id, strict) {
        return id === 'null' || id === 'true' || id === 'false' || isKeywordES5(id, strict);
    }

    function isReservedWordES6(id, strict) {
        return id === 'null' || id === 'true' || id === 'false' || isKeywordES6(id, strict);
    }

    function isRestrictedWord(id) {
        return id === 'eval' || id === 'arguments';
    }

    function isIdentifierNameES5(id) {
        var i, iz, ch;

        if (id.length === 0) { return false; }

        ch = id.charCodeAt(0);
        if (!code.isIdentifierStartES5(ch)) {
            return false;
        }

        for (i = 1, iz = id.length; i < iz; ++i) {
            ch = id.charCodeAt(i);
            if (!code.isIdentifierPartES5(ch)) {
                return false;
            }
        }
        return true;
    }

    function decodeUtf16(lead, trail) {
        return (lead - 0xD800) * 0x400 + (trail - 0xDC00) + 0x10000;
    }

    function isIdentifierNameES6(id) {
        var i, iz, ch, lowCh, check;

        if (id.length === 0) { return false; }

        check = code.isIdentifierStartES6;
        for (i = 0, iz = id.length; i < iz; ++i) {
            ch = id.charCodeAt(i);
            if (0xD800 <= ch && ch <= 0xDBFF) {
                ++i;
                if (i >= iz) { return false; }
                lowCh = id.charCodeAt(i);
                if (!(0xDC00 <= lowCh && lowCh <= 0xDFFF)) {
                    return false;
                }
                ch = decodeUtf16(ch, lowCh);
            }
            if (!check(ch)) {
                return false;
            }
            check = code.isIdentifierPartES6;
        }
        return true;
    }

    function isIdentifierES5(id, strict) {
        return isIdentifierNameES5(id) && !isReservedWordES5(id, strict);
    }

    function isIdentifierES6(id, strict) {
        return isIdentifierNameES6(id) && !isReservedWordES6(id, strict);
    }

    module.exports = {
        isKeywordES5: isKeywordES5,
        isKeywordES6: isKeywordES6,
        isReservedWordES5: isReservedWordES5,
        isReservedWordES6: isReservedWordES6,
        isRestrictedWord: isRestrictedWord,
        isIdentifierNameES5: isIdentifierNameES5,
        isIdentifierNameES6: isIdentifierNameES6,
        isIdentifierES5: isIdentifierES5,
        isIdentifierES6: isIdentifierES6
    };
}());
/* vim: set sw=4 ts=4 et tw=80 : */


/***/ }),
/* 669 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _utils = _interopRequireWildcard(__webpack_require__(43));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

(0, _utils.default)("AnyTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});
(0, _utils.default)("ArrayTypeAnnotation", {
  visitor: ["elementType"],
  aliases: ["Flow", "FlowType"],
  fields: {
    elementType: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("BooleanTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});
(0, _utils.default)("BooleanLiteralTypeAnnotation", {
  aliases: ["Flow", "FlowType"],
  fields: {
    value: (0, _utils.validate)((0, _utils.assertValueType)("boolean"))
  }
});
(0, _utils.default)("NullLiteralTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});
(0, _utils.default)("ClassImplements", {
  visitor: ["id", "typeParameters"],
  aliases: ["Flow"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterInstantiation")
  }
});
(0, _utils.default)("DeclareClass", {
  visitor: ["id", "typeParameters", "extends", "body"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterInstantiation"),
    extends: (0, _utils.validateOptional)((0, _utils.arrayOfType)("InterfaceExtends")),
    mixins: (0, _utils.validateOptional)((0, _utils.arrayOfType)("InterfaceExtends")),
    body: (0, _utils.validateType)("ObjectTypeAnnotation")
  }
});
(0, _utils.default)("DeclareFunction", {
  visitor: ["id"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    predicate: (0, _utils.validateOptionalType)("DeclaredPredicate")
  }
});
(0, _utils.default)("DeclareInterface", {
  visitor: ["id", "typeParameters", "extends", "body"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterDeclaration"),
    extends: (0, _utils.validateOptionalType)("InterfaceExtends"),
    mixins: (0, _utils.validateOptional)((0, _utils.arrayOfType)("Flow")),
    body: (0, _utils.validateType)("ObjectTypeAnnotation")
  }
});
(0, _utils.default)("DeclareModule", {
  builder: ["id", "body", "kind"],
  visitor: ["id", "body"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)(["Identifier", "StringLiteral"]),
    body: (0, _utils.validateType)("BlockStatement"),
    kind: (0, _utils.validateOptional)((0, _utils.assertOneOf)("CommonJS", "ES"))
  }
});
(0, _utils.default)("DeclareModuleExports", {
  visitor: ["typeAnnotation"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    typeAnnotation: (0, _utils.validateType)("TypeAnnotation")
  }
});
(0, _utils.default)("DeclareTypeAlias", {
  visitor: ["id", "typeParameters", "right"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterDeclaration"),
    right: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("DeclareOpaqueType", {
  visitor: ["id", "typeParameters", "supertype"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterDeclaration"),
    supertype: (0, _utils.validateOptionalType)("FlowType")
  }
});
(0, _utils.default)("DeclareVariable", {
  visitor: ["id"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier")
  }
});
(0, _utils.default)("DeclareExportDeclaration", {
  visitor: ["declaration", "specifiers", "source"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    declaration: (0, _utils.validateOptionalType)("Flow"),
    specifiers: (0, _utils.validateOptional)((0, _utils.arrayOfType)(["ExportSpecifier", "ExportNamespaceSpecifier"])),
    source: (0, _utils.validateOptionalType)("StringLiteral"),
    default: (0, _utils.validateOptional)((0, _utils.assertValueType)("boolean"))
  }
});
(0, _utils.default)("DeclareExportAllDeclaration", {
  visitor: ["source"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    source: (0, _utils.validateType)("StringLiteral"),
    exportKind: (0, _utils.validateOptional)((0, _utils.assertOneOf)(["type", "value"]))
  }
});
(0, _utils.default)("DeclaredPredicate", {
  visitor: ["value"],
  aliases: ["Flow", "FlowPredicate"],
  fields: {
    value: (0, _utils.validateType)("Flow")
  }
});
(0, _utils.default)("ExistsTypeAnnotation", {
  aliases: ["Flow", "FlowType"]
});
(0, _utils.default)("FunctionTypeAnnotation", {
  visitor: ["typeParameters", "params", "rest", "returnType"],
  aliases: ["Flow", "FlowType"],
  fields: {
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterDeclaration"),
    params: (0, _utils.validate)((0, _utils.arrayOfType)("FunctionTypeParam")),
    rest: (0, _utils.validateOptionalType)("FunctionTypeParam"),
    returnType: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("FunctionTypeParam", {
  visitor: ["name", "typeAnnotation"],
  aliases: ["Flow"],
  fields: {
    name: (0, _utils.validateOptionalType)("Identifier"),
    typeAnnotation: (0, _utils.validateType)("FlowType"),
    optional: (0, _utils.validateOptional)((0, _utils.assertValueType)("boolean"))
  }
});
(0, _utils.default)("GenericTypeAnnotation", {
  visitor: ["id", "typeParameters"],
  aliases: ["Flow", "FlowType"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterInstantiation")
  }
});
(0, _utils.default)("InferredPredicate", {
  aliases: ["Flow", "FlowPredicate"]
});
(0, _utils.default)("InterfaceExtends", {
  visitor: ["id", "typeParameters"],
  aliases: ["Flow"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterInstantiation")
  }
});
(0, _utils.default)("InterfaceDeclaration", {
  visitor: ["id", "typeParameters", "extends", "body"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterDeclaration"),
    extends: (0, _utils.validate)((0, _utils.arrayOfType)("InterfaceExtends")),
    mixins: (0, _utils.validate)((0, _utils.arrayOfType)("InterfaceExtends")),
    body: (0, _utils.validateType)("ObjectTypeAnnotation")
  }
});
(0, _utils.default)("IntersectionTypeAnnotation", {
  visitor: ["types"],
  aliases: ["Flow", "FlowType"],
  fields: {
    types: (0, _utils.validate)((0, _utils.arrayOfType)("FlowType"))
  }
});
(0, _utils.default)("MixedTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});
(0, _utils.default)("EmptyTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});
(0, _utils.default)("NullableTypeAnnotation", {
  visitor: ["typeAnnotation"],
  aliases: ["Flow", "FlowType"],
  fields: {
    typeAnnotation: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("NumberLiteralTypeAnnotation", {
  aliases: ["Flow", "FlowType"],
  fields: {
    value: (0, _utils.validate)((0, _utils.assertValueType)("number"))
  }
});
(0, _utils.default)("NumberTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});
(0, _utils.default)("ObjectTypeAnnotation", {
  visitor: ["properties", "indexers", "callProperties"],
  aliases: ["Flow", "FlowType"],
  fields: {
    properties: (0, _utils.validate)((0, _utils.arrayOfType)(["ObjectTypeProperty", "ObjectTypeSpreadProperty"])),
    indexers: (0, _utils.validateOptional)((0, _utils.arrayOfType)("ObjectTypeIndexer")),
    callProperties: (0, _utils.validateOptional)((0, _utils.arrayOfType)("ObjectTypeCallProperty")),
    exact: (0, _utils.validate)((0, _utils.assertValueType)("boolean"))
  }
});
(0, _utils.default)("ObjectTypeCallProperty", {
  visitor: ["value"],
  aliases: ["Flow", "UserWhitespacable"],
  fields: {
    value: (0, _utils.validateType)("FlowType"),
    static: (0, _utils.validate)((0, _utils.assertValueType)("boolean"))
  }
});
(0, _utils.default)("ObjectTypeIndexer", {
  visitor: ["id", "key", "value", "variance"],
  aliases: ["Flow", "UserWhitespacable"],
  fields: {
    id: (0, _utils.validateOptionalType)("Identifier"),
    key: (0, _utils.validateType)("FlowType"),
    value: (0, _utils.validateType)("FlowType"),
    static: (0, _utils.validate)((0, _utils.assertValueType)("boolean")),
    variance: (0, _utils.validateOptionalType)("Variance")
  }
});
(0, _utils.default)("ObjectTypeProperty", {
  visitor: ["key", "value", "variance"],
  aliases: ["Flow", "UserWhitespacable"],
  fields: {
    key: (0, _utils.validateType)("Identifier"),
    value: (0, _utils.validateType)("FlowType"),
    kind: (0, _utils.validate)((0, _utils.assertOneOf)("init", "get", "set")),
    static: (0, _utils.validate)((0, _utils.assertValueType)("boolean")),
    optional: (0, _utils.validate)((0, _utils.assertValueType)("boolean")),
    variance: (0, _utils.validateOptionalType)("Variance")
  }
});
(0, _utils.default)("ObjectTypeSpreadProperty", {
  visitor: ["argument"],
  aliases: ["Flow", "UserWhitespacable"],
  fields: {
    argument: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("OpaqueType", {
  visitor: ["id", "typeParameters", "supertype", "impltype"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterDeclaration"),
    supertype: (0, _utils.validateOptionalType)("FlowType"),
    impltype: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("QualifiedTypeIdentifier", {
  visitor: ["id", "qualification"],
  aliases: ["Flow"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    qualification: (0, _utils.validateType)(["Identifier", "QualifiedTypeIdentifier"])
  }
});
(0, _utils.default)("StringLiteralTypeAnnotation", {
  aliases: ["Flow", "FlowType"],
  fields: {
    value: (0, _utils.validate)((0, _utils.assertValueType)("string"))
  }
});
(0, _utils.default)("StringTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});
(0, _utils.default)("ThisTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});
(0, _utils.default)("TupleTypeAnnotation", {
  visitor: ["types"],
  aliases: ["Flow", "FlowType"],
  fields: {
    types: (0, _utils.validate)((0, _utils.arrayOfType)("FlowType"))
  }
});
(0, _utils.default)("TypeofTypeAnnotation", {
  visitor: ["argument"],
  aliases: ["Flow", "FlowType"],
  fields: {
    argument: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("TypeAlias", {
  visitor: ["id", "typeParameters", "right"],
  aliases: ["Flow", "FlowDeclaration", "Statement", "Declaration"],
  fields: {
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TypeParameterDeclaration"),
    right: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("TypeAnnotation", {
  aliases: ["Flow"],
  visitor: ["typeAnnotation"],
  fields: {
    typeAnnotation: (0, _utils.validateType)("FlowType")
  }
});
(0, _utils.default)("TypeCastExpression", {
  visitor: ["expression", "typeAnnotation"],
  aliases: ["Flow", "ExpressionWrapper", "Expression"],
  fields: {
    expression: (0, _utils.validateType)("Expression"),
    typeAnnotation: (0, _utils.validateType)("TypeAnnotation")
  }
});
(0, _utils.default)("TypeParameter", {
  aliases: ["Flow"],
  visitor: ["bound", "default", "variance"],
  fields: {
    name: (0, _utils.validate)((0, _utils.assertValueType)("string")),
    bound: (0, _utils.validateOptionalType)("TypeAnnotation"),
    default: (0, _utils.validateOptionalType)("FlowType"),
    variance: (0, _utils.validateOptionalType)("Variance")
  }
});
(0, _utils.default)("TypeParameterDeclaration", {
  aliases: ["Flow"],
  visitor: ["params"],
  fields: {
    params: (0, _utils.validate)((0, _utils.arrayOfType)("TypeParameter"))
  }
});
(0, _utils.default)("TypeParameterInstantiation", {
  aliases: ["Flow"],
  visitor: ["params"],
  fields: {
    params: (0, _utils.validate)((0, _utils.arrayOfType)("FlowType"))
  }
});
(0, _utils.default)("UnionTypeAnnotation", {
  visitor: ["types"],
  aliases: ["Flow", "FlowType"],
  fields: {
    types: (0, _utils.validate)((0, _utils.arrayOfType)("FlowType"))
  }
});
(0, _utils.default)("Variance", {
  aliases: ["Flow"],
  builder: ["kind"],
  fields: {
    kind: (0, _utils.validate)((0, _utils.assertOneOf)("minus", "plus"))
  }
});
(0, _utils.default)("VoidTypeAnnotation", {
  aliases: ["Flow", "FlowType", "FlowBaseAnnotation"]
});

/***/ }),
/* 670 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _utils = _interopRequireWildcard(__webpack_require__(43));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

(0, _utils.default)("JSXAttribute", {
  visitor: ["name", "value"],
  aliases: ["JSX", "Immutable"],
  fields: {
    name: {
      validate: (0, _utils.assertNodeType)("JSXIdentifier", "JSXNamespacedName")
    },
    value: {
      optional: true,
      validate: (0, _utils.assertNodeType)("JSXElement", "JSXFragment", "StringLiteral", "JSXExpressionContainer")
    }
  }
});
(0, _utils.default)("JSXClosingElement", {
  visitor: ["name"],
  aliases: ["JSX", "Immutable"],
  fields: {
    name: {
      validate: (0, _utils.assertNodeType)("JSXIdentifier", "JSXMemberExpression")
    }
  }
});
(0, _utils.default)("JSXElement", {
  builder: ["openingElement", "closingElement", "children", "selfClosing"],
  visitor: ["openingElement", "children", "closingElement"],
  aliases: ["JSX", "Immutable", "Expression"],
  fields: {
    openingElement: {
      validate: (0, _utils.assertNodeType)("JSXOpeningElement")
    },
    closingElement: {
      optional: true,
      validate: (0, _utils.assertNodeType)("JSXClosingElement")
    },
    children: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("JSXText", "JSXExpressionContainer", "JSXSpreadChild", "JSXElement", "JSXFragment")))
    }
  }
});
(0, _utils.default)("JSXEmptyExpression", {
  aliases: ["JSX"]
});
(0, _utils.default)("JSXExpressionContainer", {
  visitor: ["expression"],
  aliases: ["JSX", "Immutable"],
  fields: {
    expression: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("JSXSpreadChild", {
  visitor: ["expression"],
  aliases: ["JSX", "Immutable"],
  fields: {
    expression: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("JSXIdentifier", {
  builder: ["name"],
  aliases: ["JSX"],
  fields: {
    name: {
      validate: (0, _utils.assertValueType)("string")
    }
  }
});
(0, _utils.default)("JSXMemberExpression", {
  visitor: ["object", "property"],
  aliases: ["JSX"],
  fields: {
    object: {
      validate: (0, _utils.assertNodeType)("JSXMemberExpression", "JSXIdentifier")
    },
    property: {
      validate: (0, _utils.assertNodeType)("JSXIdentifier")
    }
  }
});
(0, _utils.default)("JSXNamespacedName", {
  visitor: ["namespace", "name"],
  aliases: ["JSX"],
  fields: {
    namespace: {
      validate: (0, _utils.assertNodeType)("JSXIdentifier")
    },
    name: {
      validate: (0, _utils.assertNodeType)("JSXIdentifier")
    }
  }
});
(0, _utils.default)("JSXOpeningElement", {
  builder: ["name", "attributes", "selfClosing"],
  visitor: ["name", "attributes"],
  aliases: ["JSX", "Immutable"],
  fields: {
    name: {
      validate: (0, _utils.assertNodeType)("JSXIdentifier", "JSXMemberExpression")
    },
    selfClosing: {
      default: false,
      validate: (0, _utils.assertValueType)("boolean")
    },
    attributes: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("JSXAttribute", "JSXSpreadAttribute")))
    }
  }
});
(0, _utils.default)("JSXSpreadAttribute", {
  visitor: ["argument"],
  aliases: ["JSX"],
  fields: {
    argument: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("JSXText", {
  aliases: ["JSX", "Immutable"],
  builder: ["value"],
  fields: {
    value: {
      validate: (0, _utils.assertValueType)("string")
    }
  }
});
(0, _utils.default)("JSXFragment", {
  builder: ["openingFragment", "closingFragment", "children"],
  visitor: ["openingFragment", "children", "closingFragment"],
  aliases: ["JSX", "Immutable", "Expression"],
  fields: {
    openingFragment: {
      validate: (0, _utils.assertNodeType)("JSXOpeningFragment")
    },
    closingFragment: {
      validate: (0, _utils.assertNodeType)("JSXClosingFragment")
    },
    children: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("JSXText", "JSXExpressionContainer", "JSXSpreadChild", "JSXElement", "JSXFragment")))
    }
  }
});
(0, _utils.default)("JSXOpeningFragment", {
  aliases: ["JSX", "Immutable"]
});
(0, _utils.default)("JSXClosingFragment", {
  aliases: ["JSX", "Immutable"]
});

/***/ }),
/* 671 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _utils = _interopRequireWildcard(__webpack_require__(43));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

(0, _utils.default)("Noop", {
  visitor: []
});
(0, _utils.default)("ParenthesizedExpression", {
  visitor: ["expression"],
  aliases: ["Expression", "ExpressionWrapper"],
  fields: {
    expression: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});

/***/ }),
/* 672 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _utils = _interopRequireWildcard(__webpack_require__(43));

var _es = __webpack_require__(179);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

(0, _utils.default)("AwaitExpression", {
  builder: ["argument"],
  visitor: ["argument"],
  aliases: ["Expression", "Terminatorless"],
  fields: {
    argument: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("BindExpression", {
  visitor: ["object", "callee"],
  aliases: ["Expression"],
  fields: {}
});
(0, _utils.default)("ClassProperty", {
  visitor: ["key", "value", "typeAnnotation", "decorators"],
  builder: ["key", "value", "typeAnnotation", "decorators", "computed"],
  aliases: ["Property"],
  fields: Object.assign({}, _es.classMethodOrPropertyCommon, {
    value: {
      validate: (0, _utils.assertNodeType)("Expression"),
      optional: true
    },
    typeAnnotation: {
      validate: (0, _utils.assertNodeType)("TypeAnnotation", "TSTypeAnnotation", "Noop"),
      optional: true
    },
    decorators: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("Decorator"))),
      optional: true
    },
    readonly: {
      validate: (0, _utils.assertValueType)("boolean"),
      optional: true
    }
  })
});
(0, _utils.default)("Import", {
  aliases: ["Expression"]
});
(0, _utils.default)("Decorator", {
  visitor: ["expression"],
  fields: {
    expression: {
      validate: (0, _utils.assertNodeType)("Expression")
    }
  }
});
(0, _utils.default)("DoExpression", {
  visitor: ["body"],
  aliases: ["Expression"],
  fields: {
    body: {
      validate: (0, _utils.assertNodeType)("BlockStatement")
    }
  }
});
(0, _utils.default)("ExportDefaultSpecifier", {
  visitor: ["exported"],
  aliases: ["ModuleSpecifier"],
  fields: {
    exported: {
      validate: (0, _utils.assertNodeType)("Identifier")
    }
  }
});
(0, _utils.default)("ExportNamespaceSpecifier", {
  visitor: ["exported"],
  aliases: ["ModuleSpecifier"],
  fields: {
    exported: {
      validate: (0, _utils.assertNodeType)("Identifier")
    }
  }
});

/***/ }),
/* 673 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _utils = _interopRequireWildcard(__webpack_require__(43));

var _core = __webpack_require__(178);

var _es = __webpack_require__(179);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

var bool = (0, _utils.assertValueType)("boolean");
var tSFunctionTypeAnnotationCommon = {
  returnType: {
    validate: (0, _utils.assertNodeType)("TSTypeAnnotation", "Noop"),
    optional: true
  },
  typeParameters: {
    validate: (0, _utils.assertNodeType)("TSTypeParameterDeclaration", "Noop"),
    optional: true
  }
};
(0, _utils.default)("TSParameterProperty", {
  aliases: ["LVal"],
  visitor: ["parameter"],
  fields: {
    accessibility: {
      validate: (0, _utils.assertOneOf)("public", "private", "protected"),
      optional: true
    },
    readonly: {
      validate: (0, _utils.assertValueType)("boolean"),
      optional: true
    },
    parameter: {
      validate: (0, _utils.assertNodeType)("Identifier", "AssignmentPattern")
    }
  }
});
(0, _utils.default)("TSDeclareFunction", {
  aliases: ["Statement", "Declaration"],
  visitor: ["id", "typeParameters", "params", "returnType"],
  fields: Object.assign({}, _core.functionDeclarationCommon, tSFunctionTypeAnnotationCommon)
});
(0, _utils.default)("TSDeclareMethod", {
  visitor: ["decorators", "key", "typeParameters", "params", "returnType"],
  fields: Object.assign({}, _es.classMethodOrDeclareMethodCommon, tSFunctionTypeAnnotationCommon)
});
(0, _utils.default)("TSQualifiedName", {
  aliases: ["TSEntityName"],
  visitor: ["left", "right"],
  fields: {
    left: (0, _utils.validateType)("TSEntityName"),
    right: (0, _utils.validateType)("Identifier")
  }
});
var signatureDeclarationCommon = {
  typeParameters: (0, _utils.validateOptionalType)("TSTypeParameterDeclaration"),
  parameters: (0, _utils.validateArrayOfType)(["Identifier", "RestElement"]),
  typeAnnotation: (0, _utils.validateOptionalType)("TSTypeAnnotation")
};
var callConstructSignatureDeclaration = {
  aliases: ["TSTypeElement"],
  visitor: ["typeParameters", "parameters", "typeAnnotation"],
  fields: signatureDeclarationCommon
};
(0, _utils.default)("TSCallSignatureDeclaration", callConstructSignatureDeclaration);
(0, _utils.default)("TSConstructSignatureDeclaration", callConstructSignatureDeclaration);
var namedTypeElementCommon = {
  key: (0, _utils.validateType)("Expression"),
  computed: (0, _utils.validate)(bool),
  optional: (0, _utils.validateOptional)(bool)
};
(0, _utils.default)("TSPropertySignature", {
  aliases: ["TSTypeElement"],
  visitor: ["key", "typeAnnotation", "initializer"],
  fields: Object.assign({}, namedTypeElementCommon, {
    readonly: (0, _utils.validateOptional)(bool),
    typeAnnotation: (0, _utils.validateOptionalType)("TSTypeAnnotation"),
    initializer: (0, _utils.validateOptionalType)("Expression")
  })
});
(0, _utils.default)("TSMethodSignature", {
  aliases: ["TSTypeElement"],
  visitor: ["key", "typeParameters", "parameters", "typeAnnotation"],
  fields: Object.assign({}, signatureDeclarationCommon, namedTypeElementCommon)
});
(0, _utils.default)("TSIndexSignature", {
  aliases: ["TSTypeElement"],
  visitor: ["parameters", "typeAnnotation"],
  fields: {
    readonly: (0, _utils.validateOptional)(bool),
    parameters: (0, _utils.validateArrayOfType)("Identifier"),
    typeAnnotation: (0, _utils.validateOptionalType)("TSTypeAnnotation")
  }
});
var tsKeywordTypes = ["TSAnyKeyword", "TSNumberKeyword", "TSObjectKeyword", "TSBooleanKeyword", "TSStringKeyword", "TSSymbolKeyword", "TSVoidKeyword", "TSUndefinedKeyword", "TSNullKeyword", "TSNeverKeyword"];

for (var _i = 0; _i < tsKeywordTypes.length; _i++) {
  var type = tsKeywordTypes[_i];
  (0, _utils.default)(type, {
    aliases: ["TSType"],
    visitor: [],
    fields: {}
  });
}

(0, _utils.default)("TSThisType", {
  aliases: ["TSType"],
  visitor: [],
  fields: {}
});
var fnOrCtr = {
  aliases: ["TSType"],
  visitor: ["typeParameters", "typeAnnotation"],
  fields: signatureDeclarationCommon
};
(0, _utils.default)("TSFunctionType", fnOrCtr);
(0, _utils.default)("TSConstructorType", fnOrCtr);
(0, _utils.default)("TSTypeReference", {
  aliases: ["TSType"],
  visitor: ["typeName", "typeParameters"],
  fields: {
    typeName: (0, _utils.validateType)("TSEntityName"),
    typeParameters: (0, _utils.validateOptionalType)("TSTypeParameterInstantiation")
  }
});
(0, _utils.default)("TSTypePredicate", {
  aliases: ["TSType"],
  visitor: ["parameterName", "typeAnnotation"],
  fields: {
    parameterName: (0, _utils.validateType)(["Identifier", "TSThisType"]),
    typeAnnotation: (0, _utils.validateType)("TSTypeAnnotation")
  }
});
(0, _utils.default)("TSTypeQuery", {
  aliases: ["TSType"],
  visitor: ["exprName"],
  fields: {
    exprName: (0, _utils.validateType)("TSEntityName")
  }
});
(0, _utils.default)("TSTypeLiteral", {
  aliases: ["TSType"],
  visitor: ["members"],
  fields: {
    members: (0, _utils.validateArrayOfType)("TSTypeElement")
  }
});
(0, _utils.default)("TSArrayType", {
  aliases: ["TSType"],
  visitor: ["elementType"],
  fields: {
    elementType: (0, _utils.validateType)("TSType")
  }
});
(0, _utils.default)("TSTupleType", {
  aliases: ["TSType"],
  visitor: ["elementTypes"],
  fields: {
    elementTypes: (0, _utils.validateArrayOfType)("TSType")
  }
});
var unionOrIntersection = {
  aliases: ["TSType"],
  visitor: ["types"],
  fields: {
    types: (0, _utils.validateArrayOfType)("TSType")
  }
};
(0, _utils.default)("TSUnionType", unionOrIntersection);
(0, _utils.default)("TSIntersectionType", unionOrIntersection);
(0, _utils.default)("TSParenthesizedType", {
  aliases: ["TSType"],
  visitor: ["typeAnnotation"],
  fields: {
    typeAnnotation: (0, _utils.validateType)("TSType")
  }
});
(0, _utils.default)("TSTypeOperator", {
  aliases: ["TSType"],
  visitor: ["typeAnnotation"],
  fields: {
    operator: (0, _utils.validate)((0, _utils.assertValueType)("string")),
    typeAnnotation: (0, _utils.validateType)("TSType")
  }
});
(0, _utils.default)("TSIndexedAccessType", {
  aliases: ["TSType"],
  visitor: ["objectType", "indexType"],
  fields: {
    objectType: (0, _utils.validateType)("TSType"),
    indexType: (0, _utils.validateType)("TSType")
  }
});
(0, _utils.default)("TSMappedType", {
  aliases: ["TSType"],
  visitor: ["typeParameter", "typeAnnotation"],
  fields: {
    readonly: (0, _utils.validateOptional)(bool),
    typeParameter: (0, _utils.validateType)("TSTypeParameter"),
    optional: (0, _utils.validateOptional)(bool),
    typeAnnotation: (0, _utils.validateOptionalType)("TSType")
  }
});
(0, _utils.default)("TSLiteralType", {
  aliases: ["TSType"],
  visitor: ["literal"],
  fields: {
    literal: (0, _utils.validateType)(["NumericLiteral", "StringLiteral", "BooleanLiteral"])
  }
});
(0, _utils.default)("TSExpressionWithTypeArguments", {
  aliases: ["TSType"],
  visitor: ["expression", "typeParameters"],
  fields: {
    expression: (0, _utils.validateType)("TSEntityName"),
    typeParameters: (0, _utils.validateOptionalType)("TSTypeParameterInstantiation")
  }
});
(0, _utils.default)("TSInterfaceDeclaration", {
  aliases: ["Statement", "Declaration"],
  visitor: ["id", "typeParameters", "extends", "body"],
  fields: {
    declare: (0, _utils.validateOptional)(bool),
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TSTypeParameterDeclaration"),
    extends: (0, _utils.validateOptional)((0, _utils.arrayOfType)("TSExpressionWithTypeArguments")),
    body: (0, _utils.validateType)("TSInterfaceBody")
  }
});
(0, _utils.default)("TSInterfaceBody", {
  visitor: ["body"],
  fields: {
    body: (0, _utils.validateArrayOfType)("TSTypeElement")
  }
});
(0, _utils.default)("TSTypeAliasDeclaration", {
  aliases: ["Statement", "Declaration"],
  visitor: ["id", "typeParameters", "typeAnnotation"],
  fields: {
    declare: (0, _utils.validateOptional)(bool),
    id: (0, _utils.validateType)("Identifier"),
    typeParameters: (0, _utils.validateOptionalType)("TSTypeParameterDeclaration"),
    typeAnnotation: (0, _utils.validateType)("TSType")
  }
});
(0, _utils.default)("TSAsExpression", {
  aliases: ["Expression"],
  visitor: ["expression", "typeAnnotation"],
  fields: {
    expression: (0, _utils.validateType)("Expression"),
    typeAnnotation: (0, _utils.validateType)("TSType")
  }
});
(0, _utils.default)("TSTypeAssertion", {
  aliases: ["Expression"],
  visitor: ["typeAnnotation", "expression"],
  fields: {
    typeAnnotation: (0, _utils.validateType)("TSType"),
    expression: (0, _utils.validateType)("Expression")
  }
});
(0, _utils.default)("TSEnumDeclaration", {
  aliases: ["Statement", "Declaration"],
  visitor: ["id", "members"],
  fields: {
    declare: (0, _utils.validateOptional)(bool),
    const: (0, _utils.validateOptional)(bool),
    id: (0, _utils.validateType)("Identifier"),
    members: (0, _utils.validateArrayOfType)("TSEnumMember"),
    initializer: (0, _utils.validateOptionalType)("Expression")
  }
});
(0, _utils.default)("TSEnumMember", {
  visitor: ["id", "initializer"],
  fields: {
    id: (0, _utils.validateType)(["Identifier", "StringLiteral"]),
    initializer: (0, _utils.validateOptionalType)("Expression")
  }
});
(0, _utils.default)("TSModuleDeclaration", {
  aliases: ["Statement", "Declaration"],
  visitor: ["id", "body"],
  fields: {
    declare: (0, _utils.validateOptional)(bool),
    global: (0, _utils.validateOptional)(bool),
    id: (0, _utils.validateType)(["Identifier", "StringLiteral"]),
    body: (0, _utils.validateType)(["TSModuleBlock", "TSModuleDeclaration"])
  }
});
(0, _utils.default)("TSModuleBlock", {
  visitor: ["body"],
  fields: {
    body: (0, _utils.validateArrayOfType)("Statement")
  }
});
(0, _utils.default)("TSImportEqualsDeclaration", {
  aliases: ["Statement"],
  visitor: ["id", "moduleReference"],
  fields: {
    isExport: (0, _utils.validate)(bool),
    id: (0, _utils.validateType)("Identifier"),
    moduleReference: (0, _utils.validateType)(["TSEntityName", "TSExternalModuleReference"])
  }
});
(0, _utils.default)("TSExternalModuleReference", {
  visitor: ["expression"],
  fields: {
    expression: (0, _utils.validateType)("StringLiteral")
  }
});
(0, _utils.default)("TSNonNullExpression", {
  aliases: ["Expression"],
  visitor: ["expression"],
  fields: {
    expression: (0, _utils.validateType)("Expression")
  }
});
(0, _utils.default)("TSExportAssignment", {
  aliases: ["Statement"],
  visitor: ["expression"],
  fields: {
    expression: (0, _utils.validateType)("Expression")
  }
});
(0, _utils.default)("TSNamespaceExportDeclaration", {
  aliases: ["Statement"],
  visitor: ["id"],
  fields: {
    id: (0, _utils.validateType)("Identifier")
  }
});
(0, _utils.default)("TSTypeAnnotation", {
  visitor: ["typeAnnotation"],
  fields: {
    typeAnnotation: {
      validate: (0, _utils.assertNodeType)("TSType")
    }
  }
});
(0, _utils.default)("TSTypeParameterInstantiation", {
  visitor: ["params"],
  fields: {
    params: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("TSType")))
    }
  }
});
(0, _utils.default)("TSTypeParameterDeclaration", {
  visitor: ["params"],
  fields: {
    params: {
      validate: (0, _utils.chain)((0, _utils.assertValueType)("array"), (0, _utils.assertEach)((0, _utils.assertNodeType)("TSTypeParameter")))
    }
  }
});
(0, _utils.default)("TSTypeParameter", {
  visitor: ["constraint", "default"],
  fields: {
    name: {
      validate: (0, _utils.assertValueType)("string")
    },
    constraint: {
      validate: (0, _utils.assertNodeType)("TSType"),
      optional: true
    },
    default: {
      validate: (0, _utils.assertNodeType)("TSType"),
      optional: true
    }
  }
});

/***/ }),
/* 674 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isCompatTag;

function isCompatTag(tagName) {
  return !!tagName && /^[a-z]/.test(tagName);
}

/***/ }),
/* 675 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = buildChildren;

var _generated = __webpack_require__(17);

var _cleanJSXElementLiteralChild = _interopRequireDefault(__webpack_require__(676));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function buildChildren(node) {
  var elements = [];

  for (var i = 0; i < node.children.length; i++) {
    var child = node.children[i];

    if ((0, _generated.isJSXText)(child)) {
      (0, _cleanJSXElementLiteralChild.default)(child, elements);
      continue;
    }

    if ((0, _generated.isJSXExpressionContainer)(child)) child = child.expression;
    if ((0, _generated.isJSXEmptyExpression)(child)) continue;
    elements.push(child);
  }

  return elements;
}

/***/ }),
/* 676 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = cleanJSXElementLiteralChild;

var _generated = __webpack_require__(29);

function cleanJSXElementLiteralChild(child, args) {
  var lines = child.value.split(/\r\n|\n|\r/);
  var lastNonEmptyLine = 0;

  for (var i = 0; i < lines.length; i++) {
    if (lines[i].match(/[^ \t]/)) {
      lastNonEmptyLine = i;
    }
  }

  var str = "";

  for (var _i = 0; _i < lines.length; _i++) {
    var line = lines[_i];
    var isFirstLine = _i === 0;
    var isLastLine = _i === lines.length - 1;
    var isLastNonEmptyLine = _i === lastNonEmptyLine;
    var trimmedLine = line.replace(/\t/g, " ");

    if (!isFirstLine) {
      trimmedLine = trimmedLine.replace(/^[ ]+/, "");
    }

    if (!isLastLine) {
      trimmedLine = trimmedLine.replace(/[ ]+$/, "");
    }

    if (trimmedLine) {
      if (!isLastNonEmptyLine) {
        trimmedLine += " ";
      }

      str += trimmedLine;
    }
  }

  if (str) args.push((0, _generated.stringLiteral)(str));
}

/***/ }),
/* 677 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = builder;

var _clone = _interopRequireDefault(__webpack_require__(678));

var _definitions = __webpack_require__(35);

var _validate = _interopRequireDefault(__webpack_require__(270));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function builder(type) {
  for (var _len = arguments.length, args = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
    args[_key - 1] = arguments[_key];
  }

  var keys = _definitions.BUILDER_KEYS[type];
  var countArgs = args.length;

  if (countArgs > keys.length) {
    throw new Error(type + ": Too many arguments passed. Received " + countArgs + " but can receive no more than " + keys.length);
  }

  var node = {
    type: type
  };
  var i = 0;
  keys.forEach(function (key) {
    var field = _definitions.NODE_FIELDS[type][key];
    var arg;
    if (i < countArgs) arg = args[i];
    if (arg === undefined) arg = (0, _clone.default)(field.default);
    node[key] = arg;
    i++;
  });

  for (var key in node) {
    (0, _validate.default)(node, key, node[key]);
  }

  return node;
}

/***/ }),
/* 678 */
/***/ (function(module, exports, __webpack_require__) {

var baseClone = __webpack_require__(679);

/** Used to compose bitmasks for cloning. */
var CLONE_SYMBOLS_FLAG = 4;

/**
 * Creates a shallow clone of `value`.
 *
 * **Note:** This method is loosely based on the
 * [structured clone algorithm](https://mdn.io/Structured_clone_algorithm)
 * and supports cloning arrays, array buffers, booleans, date objects, maps,
 * numbers, `Object` objects, regexes, sets, strings, symbols, and typed
 * arrays. The own enumerable properties of `arguments` objects are cloned
 * as plain objects. An empty object is returned for uncloneable values such
 * as error objects, functions, DOM nodes, and WeakMaps.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Lang
 * @param {*} value The value to clone.
 * @returns {*} Returns the cloned value.
 * @see _.cloneDeep
 * @example
 *
 * var objects = [{ 'a': 1 }, { 'b': 2 }];
 *
 * var shallow = _.clone(objects);
 * console.log(shallow[0] === objects[0]);
 * // => true
 */
function clone(value) {
  return baseClone(value, CLONE_SYMBOLS_FLAG);
}

module.exports = clone;


/***/ }),
/* 679 */
/***/ (function(module, exports, __webpack_require__) {

var Stack = __webpack_require__(180),
    arrayEach = __webpack_require__(685),
    assignValue = __webpack_require__(92),
    baseAssign = __webpack_require__(686),
    baseAssignIn = __webpack_require__(692),
    cloneBuffer = __webpack_require__(695),
    copyArray = __webpack_require__(696),
    copySymbols = __webpack_require__(697),
    copySymbolsIn = __webpack_require__(699),
    getAllKeys = __webpack_require__(266),
    getAllKeysIn = __webpack_require__(700),
    getTag = __webpack_require__(85),
    initCloneArray = __webpack_require__(704),
    initCloneByTag = __webpack_require__(705),
    initCloneObject = __webpack_require__(710),
    isArray = __webpack_require__(16),
    isBuffer = __webpack_require__(114),
    isMap = __webpack_require__(712),
    isObject = __webpack_require__(26),
    isSet = __webpack_require__(714),
    keys = __webpack_require__(112);

/** Used to compose bitmasks for cloning. */
var CLONE_DEEP_FLAG = 1,
    CLONE_FLAT_FLAG = 2,
    CLONE_SYMBOLS_FLAG = 4;

/** `Object#toString` result references. */
var argsTag = '[object Arguments]',
    arrayTag = '[object Array]',
    boolTag = '[object Boolean]',
    dateTag = '[object Date]',
    errorTag = '[object Error]',
    funcTag = '[object Function]',
    genTag = '[object GeneratorFunction]',
    mapTag = '[object Map]',
    numberTag = '[object Number]',
    objectTag = '[object Object]',
    regexpTag = '[object RegExp]',
    setTag = '[object Set]',
    stringTag = '[object String]',
    symbolTag = '[object Symbol]',
    weakMapTag = '[object WeakMap]';

var arrayBufferTag = '[object ArrayBuffer]',
    dataViewTag = '[object DataView]',
    float32Tag = '[object Float32Array]',
    float64Tag = '[object Float64Array]',
    int8Tag = '[object Int8Array]',
    int16Tag = '[object Int16Array]',
    int32Tag = '[object Int32Array]',
    uint8Tag = '[object Uint8Array]',
    uint8ClampedTag = '[object Uint8ClampedArray]',
    uint16Tag = '[object Uint16Array]',
    uint32Tag = '[object Uint32Array]';

/** Used to identify `toStringTag` values supported by `_.clone`. */
var cloneableTags = {};
cloneableTags[argsTag] = cloneableTags[arrayTag] =
cloneableTags[arrayBufferTag] = cloneableTags[dataViewTag] =
cloneableTags[boolTag] = cloneableTags[dateTag] =
cloneableTags[float32Tag] = cloneableTags[float64Tag] =
cloneableTags[int8Tag] = cloneableTags[int16Tag] =
cloneableTags[int32Tag] = cloneableTags[mapTag] =
cloneableTags[numberTag] = cloneableTags[objectTag] =
cloneableTags[regexpTag] = cloneableTags[setTag] =
cloneableTags[stringTag] = cloneableTags[symbolTag] =
cloneableTags[uint8Tag] = cloneableTags[uint8ClampedTag] =
cloneableTags[uint16Tag] = cloneableTags[uint32Tag] = true;
cloneableTags[errorTag] = cloneableTags[funcTag] =
cloneableTags[weakMapTag] = false;

/**
 * The base implementation of `_.clone` and `_.cloneDeep` which tracks
 * traversed objects.
 *
 * @private
 * @param {*} value The value to clone.
 * @param {boolean} bitmask The bitmask flags.
 *  1 - Deep clone
 *  2 - Flatten inherited properties
 *  4 - Clone symbols
 * @param {Function} [customizer] The function to customize cloning.
 * @param {string} [key] The key of `value`.
 * @param {Object} [object] The parent object of `value`.
 * @param {Object} [stack] Tracks traversed objects and their clone counterparts.
 * @returns {*} Returns the cloned value.
 */
function baseClone(value, bitmask, customizer, key, object, stack) {
  var result,
      isDeep = bitmask & CLONE_DEEP_FLAG,
      isFlat = bitmask & CLONE_FLAT_FLAG,
      isFull = bitmask & CLONE_SYMBOLS_FLAG;

  if (customizer) {
    result = object ? customizer(value, key, object, stack) : customizer(value);
  }
  if (result !== undefined) {
    return result;
  }
  if (!isObject(value)) {
    return value;
  }
  var isArr = isArray(value);
  if (isArr) {
    result = initCloneArray(value);
    if (!isDeep) {
      return copyArray(value, result);
    }
  } else {
    var tag = getTag(value),
        isFunc = tag == funcTag || tag == genTag;

    if (isBuffer(value)) {
      return cloneBuffer(value, isDeep);
    }
    if (tag == objectTag || tag == argsTag || (isFunc && !object)) {
      result = (isFlat || isFunc) ? {} : initCloneObject(value);
      if (!isDeep) {
        return isFlat
          ? copySymbolsIn(value, baseAssignIn(result, value))
          : copySymbols(value, baseAssign(result, value));
      }
    } else {
      if (!cloneableTags[tag]) {
        return object ? value : {};
      }
      result = initCloneByTag(value, tag, isDeep);
    }
  }
  // Check for circular references and return its corresponding clone.
  stack || (stack = new Stack);
  var stacked = stack.get(value);
  if (stacked) {
    return stacked;
  }
  stack.set(value, result);

  if (isSet(value)) {
    value.forEach(function(subValue) {
      result.add(baseClone(subValue, bitmask, customizer, subValue, value, stack));
    });

    return result;
  }

  if (isMap(value)) {
    value.forEach(function(subValue, key) {
      result.set(key, baseClone(subValue, bitmask, customizer, key, value, stack));
    });

    return result;
  }

  var keysFunc = isFull
    ? (isFlat ? getAllKeysIn : getAllKeys)
    : (isFlat ? keysIn : keys);

  var props = isArr ? undefined : keysFunc(value);
  arrayEach(props || value, function(subValue, key) {
    if (props) {
      key = subValue;
      subValue = value[key];
    }
    // Recursively populate clone (susceptible to call stack limits).
    assignValue(result, key, baseClone(subValue, bitmask, customizer, key, value, stack));
  });
  return result;
}

module.exports = baseClone;


/***/ }),
/* 680 */
/***/ (function(module, exports, __webpack_require__) {

var ListCache = __webpack_require__(53);

/**
 * Removes all key-value entries from the stack.
 *
 * @private
 * @name clear
 * @memberOf Stack
 */
function stackClear() {
  this.__data__ = new ListCache;
  this.size = 0;
}

module.exports = stackClear;


/***/ }),
/* 681 */
/***/ (function(module, exports) {

/**
 * Removes `key` and its value from the stack.
 *
 * @private
 * @name delete
 * @memberOf Stack
 * @param {string} key The key of the value to remove.
 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
 */
function stackDelete(key) {
  var data = this.__data__,
      result = data['delete'](key);

  this.size = data.size;
  return result;
}

module.exports = stackDelete;


/***/ }),
/* 682 */
/***/ (function(module, exports) {

/**
 * Gets the stack value for `key`.
 *
 * @private
 * @name get
 * @memberOf Stack
 * @param {string} key The key of the value to get.
 * @returns {*} Returns the entry value.
 */
function stackGet(key) {
  return this.__data__.get(key);
}

module.exports = stackGet;


/***/ }),
/* 683 */
/***/ (function(module, exports) {

/**
 * Checks if a stack value for `key` exists.
 *
 * @private
 * @name has
 * @memberOf Stack
 * @param {string} key The key of the entry to check.
 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
 */
function stackHas(key) {
  return this.__data__.has(key);
}

module.exports = stackHas;


/***/ }),
/* 684 */
/***/ (function(module, exports, __webpack_require__) {

var ListCache = __webpack_require__(53),
    Map = __webpack_require__(66),
    MapCache = __webpack_require__(65);

/** Used as the size to enable large array optimizations. */
var LARGE_ARRAY_SIZE = 200;

/**
 * Sets the stack `key` to `value`.
 *
 * @private
 * @name set
 * @memberOf Stack
 * @param {string} key The key of the value to set.
 * @param {*} value The value to set.
 * @returns {Object} Returns the stack cache instance.
 */
function stackSet(key, value) {
  var data = this.__data__;
  if (data instanceof ListCache) {
    var pairs = data.__data__;
    if (!Map || (pairs.length < LARGE_ARRAY_SIZE - 1)) {
      pairs.push([key, value]);
      this.size = ++data.size;
      return this;
    }
    data = this.__data__ = new MapCache(pairs);
  }
  data.set(key, value);
  this.size = data.size;
  return this;
}

module.exports = stackSet;


/***/ }),
/* 685 */
/***/ (function(module, exports) {

/**
 * A specialized version of `_.forEach` for arrays without support for
 * iteratee shorthands.
 *
 * @private
 * @param {Array} [array] The array to iterate over.
 * @param {Function} iteratee The function invoked per iteration.
 * @returns {Array} Returns `array`.
 */
function arrayEach(array, iteratee) {
  var index = -1,
      length = array == null ? 0 : array.length;

  while (++index < length) {
    if (iteratee(array[index], index, array) === false) {
      break;
    }
  }
  return array;
}

module.exports = arrayEach;


/***/ }),
/* 686 */
/***/ (function(module, exports, __webpack_require__) {

var copyObject = __webpack_require__(111),
    keys = __webpack_require__(112);

/**
 * The base implementation of `_.assign` without support for multiple sources
 * or `customizer` functions.
 *
 * @private
 * @param {Object} object The destination object.
 * @param {Object} source The source object.
 * @returns {Object} Returns `object`.
 */
function baseAssign(object, source) {
  return object && copyObject(source, keys(source), object);
}

module.exports = baseAssign;


/***/ }),
/* 687 */
/***/ (function(module, exports) {

/**
 * The base implementation of `_.times` without support for iteratee shorthands
 * or max array length checks.
 *
 * @private
 * @param {number} n The number of times to invoke `iteratee`.
 * @param {Function} iteratee The function invoked per iteration.
 * @returns {Array} Returns the array of results.
 */
function baseTimes(n, iteratee) {
  var index = -1,
      result = Array(n);

  while (++index < n) {
    result[index] = iteratee(index);
  }
  return result;
}

module.exports = baseTimes;


/***/ }),
/* 688 */
/***/ (function(module, exports, __webpack_require__) {

var baseGetTag = __webpack_require__(23),
    isObjectLike = __webpack_require__(21);

/** `Object#toString` result references. */
var argsTag = '[object Arguments]';

/**
 * The base implementation of `_.isArguments`.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is an `arguments` object,
 */
function baseIsArguments(value) {
  return isObjectLike(value) && baseGetTag(value) == argsTag;
}

module.exports = baseIsArguments;


/***/ }),
/* 689 */
/***/ (function(module, exports) {

/**
 * This method returns `false`.
 *
 * @static
 * @memberOf _
 * @since 4.13.0
 * @category Util
 * @returns {boolean} Returns `false`.
 * @example
 *
 * _.times(2, _.stubFalse);
 * // => [false, false]
 */
function stubFalse() {
  return false;
}

module.exports = stubFalse;


/***/ }),
/* 690 */
/***/ (function(module, exports, __webpack_require__) {

var baseGetTag = __webpack_require__(23),
    isLength = __webpack_require__(182),
    isObjectLike = __webpack_require__(21);

/** `Object#toString` result references. */
var argsTag = '[object Arguments]',
    arrayTag = '[object Array]',
    boolTag = '[object Boolean]',
    dateTag = '[object Date]',
    errorTag = '[object Error]',
    funcTag = '[object Function]',
    mapTag = '[object Map]',
    numberTag = '[object Number]',
    objectTag = '[object Object]',
    regexpTag = '[object RegExp]',
    setTag = '[object Set]',
    stringTag = '[object String]',
    weakMapTag = '[object WeakMap]';

var arrayBufferTag = '[object ArrayBuffer]',
    dataViewTag = '[object DataView]',
    float32Tag = '[object Float32Array]',
    float64Tag = '[object Float64Array]',
    int8Tag = '[object Int8Array]',
    int16Tag = '[object Int16Array]',
    int32Tag = '[object Int32Array]',
    uint8Tag = '[object Uint8Array]',
    uint8ClampedTag = '[object Uint8ClampedArray]',
    uint16Tag = '[object Uint16Array]',
    uint32Tag = '[object Uint32Array]';

/** Used to identify `toStringTag` values of typed arrays. */
var typedArrayTags = {};
typedArrayTags[float32Tag] = typedArrayTags[float64Tag] =
typedArrayTags[int8Tag] = typedArrayTags[int16Tag] =
typedArrayTags[int32Tag] = typedArrayTags[uint8Tag] =
typedArrayTags[uint8ClampedTag] = typedArrayTags[uint16Tag] =
typedArrayTags[uint32Tag] = true;
typedArrayTags[argsTag] = typedArrayTags[arrayTag] =
typedArrayTags[arrayBufferTag] = typedArrayTags[boolTag] =
typedArrayTags[dataViewTag] = typedArrayTags[dateTag] =
typedArrayTags[errorTag] = typedArrayTags[funcTag] =
typedArrayTags[mapTag] = typedArrayTags[numberTag] =
typedArrayTags[objectTag] = typedArrayTags[regexpTag] =
typedArrayTags[setTag] = typedArrayTags[stringTag] =
typedArrayTags[weakMapTag] = false;

/**
 * The base implementation of `_.isTypedArray` without Node.js optimizations.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a typed array, else `false`.
 */
function baseIsTypedArray(value) {
  return isObjectLike(value) &&
    isLength(value.length) && !!typedArrayTags[baseGetTag(value)];
}

module.exports = baseIsTypedArray;


/***/ }),
/* 691 */
/***/ (function(module, exports, __webpack_require__) {

var overArg = __webpack_require__(262);

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeKeys = overArg(Object.keys, Object);

module.exports = nativeKeys;


/***/ }),
/* 692 */
/***/ (function(module, exports, __webpack_require__) {

var copyObject = __webpack_require__(111),
    keysIn = __webpack_require__(263);

/**
 * The base implementation of `_.assignIn` without support for multiple sources
 * or `customizer` functions.
 *
 * @private
 * @param {Object} object The destination object.
 * @param {Object} source The source object.
 * @returns {Object} Returns `object`.
 */
function baseAssignIn(object, source) {
  return object && copyObject(source, keysIn(source), object);
}

module.exports = baseAssignIn;


/***/ }),
/* 693 */
/***/ (function(module, exports, __webpack_require__) {

var isObject = __webpack_require__(26),
    isPrototype = __webpack_require__(116),
    nativeKeysIn = __webpack_require__(694);

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * The base implementation of `_.keysIn` which doesn't treat sparse arrays as dense.
 *
 * @private
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of property names.
 */
function baseKeysIn(object) {
  if (!isObject(object)) {
    return nativeKeysIn(object);
  }
  var isProto = isPrototype(object),
      result = [];

  for (var key in object) {
    if (!(key == 'constructor' && (isProto || !hasOwnProperty.call(object, key)))) {
      result.push(key);
    }
  }
  return result;
}

module.exports = baseKeysIn;


/***/ }),
/* 694 */
/***/ (function(module, exports) {

/**
 * This function is like
 * [`Object.keys`](http://ecma-international.org/ecma-262/7.0/#sec-object.keys)
 * except that it includes inherited enumerable properties.
 *
 * @private
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of property names.
 */
function nativeKeysIn(object) {
  var result = [];
  if (object != null) {
    for (var key in Object(object)) {
      result.push(key);
    }
  }
  return result;
}

module.exports = nativeKeysIn;


/***/ }),
/* 695 */
/***/ (function(module, exports, __webpack_require__) {

/* WEBPACK VAR INJECTION */(function(module) {var root = __webpack_require__(18);

/** Detect free variable `exports`. */
var freeExports = typeof exports == 'object' && exports && !exports.nodeType && exports;

/** Detect free variable `module`. */
var freeModule = freeExports && typeof module == 'object' && module && !module.nodeType && module;

/** Detect the popular CommonJS extension `module.exports`. */
var moduleExports = freeModule && freeModule.exports === freeExports;

/** Built-in value references. */
var Buffer = moduleExports ? root.Buffer : undefined,
    allocUnsafe = Buffer ? Buffer.allocUnsafe : undefined;

/**
 * Creates a clone of  `buffer`.
 *
 * @private
 * @param {Buffer} buffer The buffer to clone.
 * @param {boolean} [isDeep] Specify a deep clone.
 * @returns {Buffer} Returns the cloned buffer.
 */
function cloneBuffer(buffer, isDeep) {
  if (isDeep) {
    return buffer.slice();
  }
  var length = buffer.length,
      result = allocUnsafe ? allocUnsafe(length) : new buffer.constructor(length);

  buffer.copy(result);
  return result;
}

module.exports = cloneBuffer;

/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(73)(module)))

/***/ }),
/* 696 */
/***/ (function(module, exports) {

/**
 * Copies the values of `source` to `array`.
 *
 * @private
 * @param {Array} source The array to copy values from.
 * @param {Array} [array=[]] The array to copy values to.
 * @returns {Array} Returns `array`.
 */
function copyArray(source, array) {
  var index = -1,
      length = source.length;

  array || (array = Array(length));
  while (++index < length) {
    array[index] = source[index];
  }
  return array;
}

module.exports = copyArray;


/***/ }),
/* 697 */
/***/ (function(module, exports, __webpack_require__) {

var copyObject = __webpack_require__(111),
    getSymbols = __webpack_require__(183);

/**
 * Copies own symbols of `source` to `object`.
 *
 * @private
 * @param {Object} source The object to copy symbols from.
 * @param {Object} [object={}] The object to copy symbols to.
 * @returns {Object} Returns `object`.
 */
function copySymbols(source, object) {
  return copyObject(source, getSymbols(source), object);
}

module.exports = copySymbols;


/***/ }),
/* 698 */
/***/ (function(module, exports) {

/**
 * A specialized version of `_.filter` for arrays without support for
 * iteratee shorthands.
 *
 * @private
 * @param {Array} [array] The array to iterate over.
 * @param {Function} predicate The function invoked per iteration.
 * @returns {Array} Returns the new filtered array.
 */
function arrayFilter(array, predicate) {
  var index = -1,
      length = array == null ? 0 : array.length,
      resIndex = 0,
      result = [];

  while (++index < length) {
    var value = array[index];
    if (predicate(value, index, array)) {
      result[resIndex++] = value;
    }
  }
  return result;
}

module.exports = arrayFilter;


/***/ }),
/* 699 */
/***/ (function(module, exports, __webpack_require__) {

var copyObject = __webpack_require__(111),
    getSymbolsIn = __webpack_require__(265);

/**
 * Copies own and inherited symbols of `source` to `object`.
 *
 * @private
 * @param {Object} source The object to copy symbols from.
 * @param {Object} [object={}] The object to copy symbols to.
 * @returns {Object} Returns `object`.
 */
function copySymbolsIn(source, object) {
  return copyObject(source, getSymbolsIn(source), object);
}

module.exports = copySymbolsIn;


/***/ }),
/* 700 */
/***/ (function(module, exports, __webpack_require__) {

var baseGetAllKeys = __webpack_require__(267),
    getSymbolsIn = __webpack_require__(265),
    keysIn = __webpack_require__(263);

/**
 * Creates an array of own and inherited enumerable property names and
 * symbols of `object`.
 *
 * @private
 * @param {Object} object The object to query.
 * @returns {Array} Returns the array of property names and symbols.
 */
function getAllKeysIn(object) {
  return baseGetAllKeys(object, keysIn, getSymbolsIn);
}

module.exports = getAllKeysIn;


/***/ }),
/* 701 */
/***/ (function(module, exports, __webpack_require__) {

var getNative = __webpack_require__(25),
    root = __webpack_require__(18);

/* Built-in method references that are verified to be native. */
var DataView = getNative(root, 'DataView');

module.exports = DataView;


/***/ }),
/* 702 */
/***/ (function(module, exports, __webpack_require__) {

var getNative = __webpack_require__(25),
    root = __webpack_require__(18);

/* Built-in method references that are verified to be native. */
var Promise = getNative(root, 'Promise');

module.exports = Promise;


/***/ }),
/* 703 */
/***/ (function(module, exports, __webpack_require__) {

var getNative = __webpack_require__(25),
    root = __webpack_require__(18);

/* Built-in method references that are verified to be native. */
var WeakMap = getNative(root, 'WeakMap');

module.exports = WeakMap;


/***/ }),
/* 704 */
/***/ (function(module, exports) {

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * Initializes an array clone.
 *
 * @private
 * @param {Array} array The array to clone.
 * @returns {Array} Returns the initialized clone.
 */
function initCloneArray(array) {
  var length = array.length,
      result = new array.constructor(length);

  // Add properties assigned by `RegExp#exec`.
  if (length && typeof array[0] == 'string' && hasOwnProperty.call(array, 'index')) {
    result.index = array.index;
    result.input = array.input;
  }
  return result;
}

module.exports = initCloneArray;


/***/ }),
/* 705 */
/***/ (function(module, exports, __webpack_require__) {

var cloneArrayBuffer = __webpack_require__(186),
    cloneDataView = __webpack_require__(706),
    cloneRegExp = __webpack_require__(707),
    cloneSymbol = __webpack_require__(708),
    cloneTypedArray = __webpack_require__(709);

/** `Object#toString` result references. */
var boolTag = '[object Boolean]',
    dateTag = '[object Date]',
    mapTag = '[object Map]',
    numberTag = '[object Number]',
    regexpTag = '[object RegExp]',
    setTag = '[object Set]',
    stringTag = '[object String]',
    symbolTag = '[object Symbol]';

var arrayBufferTag = '[object ArrayBuffer]',
    dataViewTag = '[object DataView]',
    float32Tag = '[object Float32Array]',
    float64Tag = '[object Float64Array]',
    int8Tag = '[object Int8Array]',
    int16Tag = '[object Int16Array]',
    int32Tag = '[object Int32Array]',
    uint8Tag = '[object Uint8Array]',
    uint8ClampedTag = '[object Uint8ClampedArray]',
    uint16Tag = '[object Uint16Array]',
    uint32Tag = '[object Uint32Array]';

/**
 * Initializes an object clone based on its `toStringTag`.
 *
 * **Note:** This function only supports cloning values with tags of
 * `Boolean`, `Date`, `Error`, `Map`, `Number`, `RegExp`, `Set`, or `String`.
 *
 * @private
 * @param {Object} object The object to clone.
 * @param {string} tag The `toStringTag` of the object to clone.
 * @param {boolean} [isDeep] Specify a deep clone.
 * @returns {Object} Returns the initialized clone.
 */
function initCloneByTag(object, tag, isDeep) {
  var Ctor = object.constructor;
  switch (tag) {
    case arrayBufferTag:
      return cloneArrayBuffer(object);

    case boolTag:
    case dateTag:
      return new Ctor(+object);

    case dataViewTag:
      return cloneDataView(object, isDeep);

    case float32Tag: case float64Tag:
    case int8Tag: case int16Tag: case int32Tag:
    case uint8Tag: case uint8ClampedTag: case uint16Tag: case uint32Tag:
      return cloneTypedArray(object, isDeep);

    case mapTag:
      return new Ctor;

    case numberTag:
    case stringTag:
      return new Ctor(object);

    case regexpTag:
      return cloneRegExp(object);

    case setTag:
      return new Ctor;

    case symbolTag:
      return cloneSymbol(object);
  }
}

module.exports = initCloneByTag;


/***/ }),
/* 706 */
/***/ (function(module, exports, __webpack_require__) {

var cloneArrayBuffer = __webpack_require__(186);

/**
 * Creates a clone of `dataView`.
 *
 * @private
 * @param {Object} dataView The data view to clone.
 * @param {boolean} [isDeep] Specify a deep clone.
 * @returns {Object} Returns the cloned data view.
 */
function cloneDataView(dataView, isDeep) {
  var buffer = isDeep ? cloneArrayBuffer(dataView.buffer) : dataView.buffer;
  return new dataView.constructor(buffer, dataView.byteOffset, dataView.byteLength);
}

module.exports = cloneDataView;


/***/ }),
/* 707 */
/***/ (function(module, exports) {

/** Used to match `RegExp` flags from their coerced string values. */
var reFlags = /\w*$/;

/**
 * Creates a clone of `regexp`.
 *
 * @private
 * @param {Object} regexp The regexp to clone.
 * @returns {Object} Returns the cloned regexp.
 */
function cloneRegExp(regexp) {
  var result = new regexp.constructor(regexp.source, reFlags.exec(regexp));
  result.lastIndex = regexp.lastIndex;
  return result;
}

module.exports = cloneRegExp;


/***/ }),
/* 708 */
/***/ (function(module, exports, __webpack_require__) {

var Symbol = __webpack_require__(20);

/** Used to convert symbols to primitives and strings. */
var symbolProto = Symbol ? Symbol.prototype : undefined,
    symbolValueOf = symbolProto ? symbolProto.valueOf : undefined;

/**
 * Creates a clone of the `symbol` object.
 *
 * @private
 * @param {Object} symbol The symbol object to clone.
 * @returns {Object} Returns the cloned symbol object.
 */
function cloneSymbol(symbol) {
  return symbolValueOf ? Object(symbolValueOf.call(symbol)) : {};
}

module.exports = cloneSymbol;


/***/ }),
/* 709 */
/***/ (function(module, exports, __webpack_require__) {

var cloneArrayBuffer = __webpack_require__(186);

/**
 * Creates a clone of `typedArray`.
 *
 * @private
 * @param {Object} typedArray The typed array to clone.
 * @param {boolean} [isDeep] Specify a deep clone.
 * @returns {Object} Returns the cloned typed array.
 */
function cloneTypedArray(typedArray, isDeep) {
  var buffer = isDeep ? cloneArrayBuffer(typedArray.buffer) : typedArray.buffer;
  return new typedArray.constructor(buffer, typedArray.byteOffset, typedArray.length);
}

module.exports = cloneTypedArray;


/***/ }),
/* 710 */
/***/ (function(module, exports, __webpack_require__) {

var baseCreate = __webpack_require__(711),
    getPrototype = __webpack_require__(185),
    isPrototype = __webpack_require__(116);

/**
 * Initializes an object clone.
 *
 * @private
 * @param {Object} object The object to clone.
 * @returns {Object} Returns the initialized clone.
 */
function initCloneObject(object) {
  return (typeof object.constructor == 'function' && !isPrototype(object))
    ? baseCreate(getPrototype(object))
    : {};
}

module.exports = initCloneObject;


/***/ }),
/* 711 */
/***/ (function(module, exports, __webpack_require__) {

var isObject = __webpack_require__(26);

/** Built-in value references. */
var objectCreate = Object.create;

/**
 * The base implementation of `_.create` without support for assigning
 * properties to the created object.
 *
 * @private
 * @param {Object} proto The object to inherit from.
 * @returns {Object} Returns the new object.
 */
var baseCreate = (function() {
  function object() {}
  return function(proto) {
    if (!isObject(proto)) {
      return {};
    }
    if (objectCreate) {
      return objectCreate(proto);
    }
    object.prototype = proto;
    var result = new object;
    object.prototype = undefined;
    return result;
  };
}());

module.exports = baseCreate;


/***/ }),
/* 712 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsMap = __webpack_require__(713),
    baseUnary = __webpack_require__(84),
    nodeUtil = __webpack_require__(115);

/* Node.js helper references. */
var nodeIsMap = nodeUtil && nodeUtil.isMap;

/**
 * Checks if `value` is classified as a `Map` object.
 *
 * @static
 * @memberOf _
 * @since 4.3.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a map, else `false`.
 * @example
 *
 * _.isMap(new Map);
 * // => true
 *
 * _.isMap(new WeakMap);
 * // => false
 */
var isMap = nodeIsMap ? baseUnary(nodeIsMap) : baseIsMap;

module.exports = isMap;


/***/ }),
/* 713 */
/***/ (function(module, exports, __webpack_require__) {

var getTag = __webpack_require__(85),
    isObjectLike = __webpack_require__(21);

/** `Object#toString` result references. */
var mapTag = '[object Map]';

/**
 * The base implementation of `_.isMap` without Node.js optimizations.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a map, else `false`.
 */
function baseIsMap(value) {
  return isObjectLike(value) && getTag(value) == mapTag;
}

module.exports = baseIsMap;


/***/ }),
/* 714 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsSet = __webpack_require__(715),
    baseUnary = __webpack_require__(84),
    nodeUtil = __webpack_require__(115);

/* Node.js helper references. */
var nodeIsSet = nodeUtil && nodeUtil.isSet;

/**
 * Checks if `value` is classified as a `Set` object.
 *
 * @static
 * @memberOf _
 * @since 4.3.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a set, else `false`.
 * @example
 *
 * _.isSet(new Set);
 * // => true
 *
 * _.isSet(new WeakSet);
 * // => false
 */
var isSet = nodeIsSet ? baseUnary(nodeIsSet) : baseIsSet;

module.exports = isSet;


/***/ }),
/* 715 */
/***/ (function(module, exports, __webpack_require__) {

var getTag = __webpack_require__(85),
    isObjectLike = __webpack_require__(21);

/** `Object#toString` result references. */
var setTag = '[object Set]';

/**
 * The base implementation of `_.isSet` without Node.js optimizations.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a set, else `false`.
 */
function baseIsSet(value) {
  return isObjectLike(value) && getTag(value) == setTag;
}

module.exports = baseIsSet;


/***/ }),
/* 716 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = assertNode;

var _isNode = _interopRequireDefault(__webpack_require__(271));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function assertNode(node) {
  if (!(0, _isNode.default)(node)) {
    var type = node && node.type || JSON.stringify(node);
    throw new TypeError("Not a valid node of type \"" + type + "\"");
  }
}

/***/ }),
/* 717 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.assertArrayExpression = assertArrayExpression;
exports.assertAssignmentExpression = assertAssignmentExpression;
exports.assertBinaryExpression = assertBinaryExpression;
exports.assertDirective = assertDirective;
exports.assertDirectiveLiteral = assertDirectiveLiteral;
exports.assertBlockStatement = assertBlockStatement;
exports.assertBreakStatement = assertBreakStatement;
exports.assertCallExpression = assertCallExpression;
exports.assertCatchClause = assertCatchClause;
exports.assertConditionalExpression = assertConditionalExpression;
exports.assertContinueStatement = assertContinueStatement;
exports.assertDebuggerStatement = assertDebuggerStatement;
exports.assertDoWhileStatement = assertDoWhileStatement;
exports.assertEmptyStatement = assertEmptyStatement;
exports.assertExpressionStatement = assertExpressionStatement;
exports.assertFile = assertFile;
exports.assertForInStatement = assertForInStatement;
exports.assertForStatement = assertForStatement;
exports.assertFunctionDeclaration = assertFunctionDeclaration;
exports.assertFunctionExpression = assertFunctionExpression;
exports.assertIdentifier = assertIdentifier;
exports.assertIfStatement = assertIfStatement;
exports.assertLabeledStatement = assertLabeledStatement;
exports.assertStringLiteral = assertStringLiteral;
exports.assertNumericLiteral = assertNumericLiteral;
exports.assertNullLiteral = assertNullLiteral;
exports.assertBooleanLiteral = assertBooleanLiteral;
exports.assertRegExpLiteral = assertRegExpLiteral;
exports.assertLogicalExpression = assertLogicalExpression;
exports.assertMemberExpression = assertMemberExpression;
exports.assertNewExpression = assertNewExpression;
exports.assertProgram = assertProgram;
exports.assertObjectExpression = assertObjectExpression;
exports.assertObjectMethod = assertObjectMethod;
exports.assertObjectProperty = assertObjectProperty;
exports.assertRestElement = assertRestElement;
exports.assertReturnStatement = assertReturnStatement;
exports.assertSequenceExpression = assertSequenceExpression;
exports.assertSwitchCase = assertSwitchCase;
exports.assertSwitchStatement = assertSwitchStatement;
exports.assertThisExpression = assertThisExpression;
exports.assertThrowStatement = assertThrowStatement;
exports.assertTryStatement = assertTryStatement;
exports.assertUnaryExpression = assertUnaryExpression;
exports.assertUpdateExpression = assertUpdateExpression;
exports.assertVariableDeclaration = assertVariableDeclaration;
exports.assertVariableDeclarator = assertVariableDeclarator;
exports.assertWhileStatement = assertWhileStatement;
exports.assertWithStatement = assertWithStatement;
exports.assertAssignmentPattern = assertAssignmentPattern;
exports.assertArrayPattern = assertArrayPattern;
exports.assertArrowFunctionExpression = assertArrowFunctionExpression;
exports.assertClassBody = assertClassBody;
exports.assertClassDeclaration = assertClassDeclaration;
exports.assertClassExpression = assertClassExpression;
exports.assertExportAllDeclaration = assertExportAllDeclaration;
exports.assertExportDefaultDeclaration = assertExportDefaultDeclaration;
exports.assertExportNamedDeclaration = assertExportNamedDeclaration;
exports.assertExportSpecifier = assertExportSpecifier;
exports.assertForOfStatement = assertForOfStatement;
exports.assertImportDeclaration = assertImportDeclaration;
exports.assertImportDefaultSpecifier = assertImportDefaultSpecifier;
exports.assertImportNamespaceSpecifier = assertImportNamespaceSpecifier;
exports.assertImportSpecifier = assertImportSpecifier;
exports.assertMetaProperty = assertMetaProperty;
exports.assertClassMethod = assertClassMethod;
exports.assertObjectPattern = assertObjectPattern;
exports.assertSpreadElement = assertSpreadElement;
exports.assertSuper = assertSuper;
exports.assertTaggedTemplateExpression = assertTaggedTemplateExpression;
exports.assertTemplateElement = assertTemplateElement;
exports.assertTemplateLiteral = assertTemplateLiteral;
exports.assertYieldExpression = assertYieldExpression;
exports.assertAnyTypeAnnotation = assertAnyTypeAnnotation;
exports.assertArrayTypeAnnotation = assertArrayTypeAnnotation;
exports.assertBooleanTypeAnnotation = assertBooleanTypeAnnotation;
exports.assertBooleanLiteralTypeAnnotation = assertBooleanLiteralTypeAnnotation;
exports.assertNullLiteralTypeAnnotation = assertNullLiteralTypeAnnotation;
exports.assertClassImplements = assertClassImplements;
exports.assertDeclareClass = assertDeclareClass;
exports.assertDeclareFunction = assertDeclareFunction;
exports.assertDeclareInterface = assertDeclareInterface;
exports.assertDeclareModule = assertDeclareModule;
exports.assertDeclareModuleExports = assertDeclareModuleExports;
exports.assertDeclareTypeAlias = assertDeclareTypeAlias;
exports.assertDeclareOpaqueType = assertDeclareOpaqueType;
exports.assertDeclareVariable = assertDeclareVariable;
exports.assertDeclareExportDeclaration = assertDeclareExportDeclaration;
exports.assertDeclareExportAllDeclaration = assertDeclareExportAllDeclaration;
exports.assertDeclaredPredicate = assertDeclaredPredicate;
exports.assertExistsTypeAnnotation = assertExistsTypeAnnotation;
exports.assertFunctionTypeAnnotation = assertFunctionTypeAnnotation;
exports.assertFunctionTypeParam = assertFunctionTypeParam;
exports.assertGenericTypeAnnotation = assertGenericTypeAnnotation;
exports.assertInferredPredicate = assertInferredPredicate;
exports.assertInterfaceExtends = assertInterfaceExtends;
exports.assertInterfaceDeclaration = assertInterfaceDeclaration;
exports.assertIntersectionTypeAnnotation = assertIntersectionTypeAnnotation;
exports.assertMixedTypeAnnotation = assertMixedTypeAnnotation;
exports.assertEmptyTypeAnnotation = assertEmptyTypeAnnotation;
exports.assertNullableTypeAnnotation = assertNullableTypeAnnotation;
exports.assertNumberLiteralTypeAnnotation = assertNumberLiteralTypeAnnotation;
exports.assertNumberTypeAnnotation = assertNumberTypeAnnotation;
exports.assertObjectTypeAnnotation = assertObjectTypeAnnotation;
exports.assertObjectTypeCallProperty = assertObjectTypeCallProperty;
exports.assertObjectTypeIndexer = assertObjectTypeIndexer;
exports.assertObjectTypeProperty = assertObjectTypeProperty;
exports.assertObjectTypeSpreadProperty = assertObjectTypeSpreadProperty;
exports.assertOpaqueType = assertOpaqueType;
exports.assertQualifiedTypeIdentifier = assertQualifiedTypeIdentifier;
exports.assertStringLiteralTypeAnnotation = assertStringLiteralTypeAnnotation;
exports.assertStringTypeAnnotation = assertStringTypeAnnotation;
exports.assertThisTypeAnnotation = assertThisTypeAnnotation;
exports.assertTupleTypeAnnotation = assertTupleTypeAnnotation;
exports.assertTypeofTypeAnnotation = assertTypeofTypeAnnotation;
exports.assertTypeAlias = assertTypeAlias;
exports.assertTypeAnnotation = assertTypeAnnotation;
exports.assertTypeCastExpression = assertTypeCastExpression;
exports.assertTypeParameter = assertTypeParameter;
exports.assertTypeParameterDeclaration = assertTypeParameterDeclaration;
exports.assertTypeParameterInstantiation = assertTypeParameterInstantiation;
exports.assertUnionTypeAnnotation = assertUnionTypeAnnotation;
exports.assertVariance = assertVariance;
exports.assertVoidTypeAnnotation = assertVoidTypeAnnotation;
exports.assertJSXAttribute = assertJSXAttribute;
exports.assertJSXClosingElement = assertJSXClosingElement;
exports.assertJSXElement = assertJSXElement;
exports.assertJSXEmptyExpression = assertJSXEmptyExpression;
exports.assertJSXExpressionContainer = assertJSXExpressionContainer;
exports.assertJSXSpreadChild = assertJSXSpreadChild;
exports.assertJSXIdentifier = assertJSXIdentifier;
exports.assertJSXMemberExpression = assertJSXMemberExpression;
exports.assertJSXNamespacedName = assertJSXNamespacedName;
exports.assertJSXOpeningElement = assertJSXOpeningElement;
exports.assertJSXSpreadAttribute = assertJSXSpreadAttribute;
exports.assertJSXText = assertJSXText;
exports.assertJSXFragment = assertJSXFragment;
exports.assertJSXOpeningFragment = assertJSXOpeningFragment;
exports.assertJSXClosingFragment = assertJSXClosingFragment;
exports.assertNoop = assertNoop;
exports.assertParenthesizedExpression = assertParenthesizedExpression;
exports.assertAwaitExpression = assertAwaitExpression;
exports.assertBindExpression = assertBindExpression;
exports.assertClassProperty = assertClassProperty;
exports.assertImport = assertImport;
exports.assertDecorator = assertDecorator;
exports.assertDoExpression = assertDoExpression;
exports.assertExportDefaultSpecifier = assertExportDefaultSpecifier;
exports.assertExportNamespaceSpecifier = assertExportNamespaceSpecifier;
exports.assertTSParameterProperty = assertTSParameterProperty;
exports.assertTSDeclareFunction = assertTSDeclareFunction;
exports.assertTSDeclareMethod = assertTSDeclareMethod;
exports.assertTSQualifiedName = assertTSQualifiedName;
exports.assertTSCallSignatureDeclaration = assertTSCallSignatureDeclaration;
exports.assertTSConstructSignatureDeclaration = assertTSConstructSignatureDeclaration;
exports.assertTSPropertySignature = assertTSPropertySignature;
exports.assertTSMethodSignature = assertTSMethodSignature;
exports.assertTSIndexSignature = assertTSIndexSignature;
exports.assertTSAnyKeyword = assertTSAnyKeyword;
exports.assertTSNumberKeyword = assertTSNumberKeyword;
exports.assertTSObjectKeyword = assertTSObjectKeyword;
exports.assertTSBooleanKeyword = assertTSBooleanKeyword;
exports.assertTSStringKeyword = assertTSStringKeyword;
exports.assertTSSymbolKeyword = assertTSSymbolKeyword;
exports.assertTSVoidKeyword = assertTSVoidKeyword;
exports.assertTSUndefinedKeyword = assertTSUndefinedKeyword;
exports.assertTSNullKeyword = assertTSNullKeyword;
exports.assertTSNeverKeyword = assertTSNeverKeyword;
exports.assertTSThisType = assertTSThisType;
exports.assertTSFunctionType = assertTSFunctionType;
exports.assertTSConstructorType = assertTSConstructorType;
exports.assertTSTypeReference = assertTSTypeReference;
exports.assertTSTypePredicate = assertTSTypePredicate;
exports.assertTSTypeQuery = assertTSTypeQuery;
exports.assertTSTypeLiteral = assertTSTypeLiteral;
exports.assertTSArrayType = assertTSArrayType;
exports.assertTSTupleType = assertTSTupleType;
exports.assertTSUnionType = assertTSUnionType;
exports.assertTSIntersectionType = assertTSIntersectionType;
exports.assertTSParenthesizedType = assertTSParenthesizedType;
exports.assertTSTypeOperator = assertTSTypeOperator;
exports.assertTSIndexedAccessType = assertTSIndexedAccessType;
exports.assertTSMappedType = assertTSMappedType;
exports.assertTSLiteralType = assertTSLiteralType;
exports.assertTSExpressionWithTypeArguments = assertTSExpressionWithTypeArguments;
exports.assertTSInterfaceDeclaration = assertTSInterfaceDeclaration;
exports.assertTSInterfaceBody = assertTSInterfaceBody;
exports.assertTSTypeAliasDeclaration = assertTSTypeAliasDeclaration;
exports.assertTSAsExpression = assertTSAsExpression;
exports.assertTSTypeAssertion = assertTSTypeAssertion;
exports.assertTSEnumDeclaration = assertTSEnumDeclaration;
exports.assertTSEnumMember = assertTSEnumMember;
exports.assertTSModuleDeclaration = assertTSModuleDeclaration;
exports.assertTSModuleBlock = assertTSModuleBlock;
exports.assertTSImportEqualsDeclaration = assertTSImportEqualsDeclaration;
exports.assertTSExternalModuleReference = assertTSExternalModuleReference;
exports.assertTSNonNullExpression = assertTSNonNullExpression;
exports.assertTSExportAssignment = assertTSExportAssignment;
exports.assertTSNamespaceExportDeclaration = assertTSNamespaceExportDeclaration;
exports.assertTSTypeAnnotation = assertTSTypeAnnotation;
exports.assertTSTypeParameterInstantiation = assertTSTypeParameterInstantiation;
exports.assertTSTypeParameterDeclaration = assertTSTypeParameterDeclaration;
exports.assertTSTypeParameter = assertTSTypeParameter;
exports.assertExpression = assertExpression;
exports.assertBinary = assertBinary;
exports.assertScopable = assertScopable;
exports.assertBlockParent = assertBlockParent;
exports.assertBlock = assertBlock;
exports.assertStatement = assertStatement;
exports.assertTerminatorless = assertTerminatorless;
exports.assertCompletionStatement = assertCompletionStatement;
exports.assertConditional = assertConditional;
exports.assertLoop = assertLoop;
exports.assertWhile = assertWhile;
exports.assertExpressionWrapper = assertExpressionWrapper;
exports.assertFor = assertFor;
exports.assertForXStatement = assertForXStatement;
exports.assertFunction = assertFunction;
exports.assertFunctionParent = assertFunctionParent;
exports.assertPureish = assertPureish;
exports.assertDeclaration = assertDeclaration;
exports.assertPatternLike = assertPatternLike;
exports.assertLVal = assertLVal;
exports.assertTSEntityName = assertTSEntityName;
exports.assertLiteral = assertLiteral;
exports.assertImmutable = assertImmutable;
exports.assertUserWhitespacable = assertUserWhitespacable;
exports.assertMethod = assertMethod;
exports.assertObjectMember = assertObjectMember;
exports.assertProperty = assertProperty;
exports.assertUnaryLike = assertUnaryLike;
exports.assertPattern = assertPattern;
exports.assertClass = assertClass;
exports.assertModuleDeclaration = assertModuleDeclaration;
exports.assertExportDeclaration = assertExportDeclaration;
exports.assertModuleSpecifier = assertModuleSpecifier;
exports.assertFlow = assertFlow;
exports.assertFlowType = assertFlowType;
exports.assertFlowBaseAnnotation = assertFlowBaseAnnotation;
exports.assertFlowDeclaration = assertFlowDeclaration;
exports.assertFlowPredicate = assertFlowPredicate;
exports.assertJSX = assertJSX;
exports.assertTSTypeElement = assertTSTypeElement;
exports.assertTSType = assertTSType;
exports.assertNumberLiteral = assertNumberLiteral;
exports.assertRegexLiteral = assertRegexLiteral;
exports.assertRestProperty = assertRestProperty;
exports.assertSpreadProperty = assertSpreadProperty;

var _is = _interopRequireDefault(__webpack_require__(110));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function assert(type, node, opts) {
  if (!(0, _is.default)(type, node, opts)) {
    throw new Error("Expected type \"" + type + "\" with option " + JSON.stringify(opts) + ", but instead got \"" + node.type + "\".");
  }
}

function assertArrayExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ArrayExpression", node, opts);
}

function assertAssignmentExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("AssignmentExpression", node, opts);
}

function assertBinaryExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("BinaryExpression", node, opts);
}

function assertDirective(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Directive", node, opts);
}

function assertDirectiveLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DirectiveLiteral", node, opts);
}

function assertBlockStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("BlockStatement", node, opts);
}

function assertBreakStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("BreakStatement", node, opts);
}

function assertCallExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("CallExpression", node, opts);
}

function assertCatchClause(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("CatchClause", node, opts);
}

function assertConditionalExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ConditionalExpression", node, opts);
}

function assertContinueStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ContinueStatement", node, opts);
}

function assertDebuggerStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DebuggerStatement", node, opts);
}

function assertDoWhileStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DoWhileStatement", node, opts);
}

function assertEmptyStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("EmptyStatement", node, opts);
}

function assertExpressionStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExpressionStatement", node, opts);
}

function assertFile(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("File", node, opts);
}

function assertForInStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ForInStatement", node, opts);
}

function assertForStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ForStatement", node, opts);
}

function assertFunctionDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FunctionDeclaration", node, opts);
}

function assertFunctionExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FunctionExpression", node, opts);
}

function assertIdentifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Identifier", node, opts);
}

function assertIfStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("IfStatement", node, opts);
}

function assertLabeledStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("LabeledStatement", node, opts);
}

function assertStringLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("StringLiteral", node, opts);
}

function assertNumericLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("NumericLiteral", node, opts);
}

function assertNullLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("NullLiteral", node, opts);
}

function assertBooleanLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("BooleanLiteral", node, opts);
}

function assertRegExpLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("RegExpLiteral", node, opts);
}

function assertLogicalExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("LogicalExpression", node, opts);
}

function assertMemberExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("MemberExpression", node, opts);
}

function assertNewExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("NewExpression", node, opts);
}

function assertProgram(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Program", node, opts);
}

function assertObjectExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectExpression", node, opts);
}

function assertObjectMethod(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectMethod", node, opts);
}

function assertObjectProperty(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectProperty", node, opts);
}

function assertRestElement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("RestElement", node, opts);
}

function assertReturnStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ReturnStatement", node, opts);
}

function assertSequenceExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("SequenceExpression", node, opts);
}

function assertSwitchCase(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("SwitchCase", node, opts);
}

function assertSwitchStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("SwitchStatement", node, opts);
}

function assertThisExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ThisExpression", node, opts);
}

function assertThrowStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ThrowStatement", node, opts);
}

function assertTryStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TryStatement", node, opts);
}

function assertUnaryExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("UnaryExpression", node, opts);
}

function assertUpdateExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("UpdateExpression", node, opts);
}

function assertVariableDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("VariableDeclaration", node, opts);
}

function assertVariableDeclarator(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("VariableDeclarator", node, opts);
}

function assertWhileStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("WhileStatement", node, opts);
}

function assertWithStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("WithStatement", node, opts);
}

function assertAssignmentPattern(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("AssignmentPattern", node, opts);
}

function assertArrayPattern(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ArrayPattern", node, opts);
}

function assertArrowFunctionExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ArrowFunctionExpression", node, opts);
}

function assertClassBody(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ClassBody", node, opts);
}

function assertClassDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ClassDeclaration", node, opts);
}

function assertClassExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ClassExpression", node, opts);
}

function assertExportAllDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExportAllDeclaration", node, opts);
}

function assertExportDefaultDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExportDefaultDeclaration", node, opts);
}

function assertExportNamedDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExportNamedDeclaration", node, opts);
}

function assertExportSpecifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExportSpecifier", node, opts);
}

function assertForOfStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ForOfStatement", node, opts);
}

function assertImportDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ImportDeclaration", node, opts);
}

function assertImportDefaultSpecifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ImportDefaultSpecifier", node, opts);
}

function assertImportNamespaceSpecifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ImportNamespaceSpecifier", node, opts);
}

function assertImportSpecifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ImportSpecifier", node, opts);
}

function assertMetaProperty(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("MetaProperty", node, opts);
}

function assertClassMethod(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ClassMethod", node, opts);
}

function assertObjectPattern(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectPattern", node, opts);
}

function assertSpreadElement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("SpreadElement", node, opts);
}

function assertSuper(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Super", node, opts);
}

function assertTaggedTemplateExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TaggedTemplateExpression", node, opts);
}

function assertTemplateElement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TemplateElement", node, opts);
}

function assertTemplateLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TemplateLiteral", node, opts);
}

function assertYieldExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("YieldExpression", node, opts);
}

function assertAnyTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("AnyTypeAnnotation", node, opts);
}

function assertArrayTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ArrayTypeAnnotation", node, opts);
}

function assertBooleanTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("BooleanTypeAnnotation", node, opts);
}

function assertBooleanLiteralTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("BooleanLiteralTypeAnnotation", node, opts);
}

function assertNullLiteralTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("NullLiteralTypeAnnotation", node, opts);
}

function assertClassImplements(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ClassImplements", node, opts);
}

function assertDeclareClass(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareClass", node, opts);
}

function assertDeclareFunction(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareFunction", node, opts);
}

function assertDeclareInterface(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareInterface", node, opts);
}

function assertDeclareModule(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareModule", node, opts);
}

function assertDeclareModuleExports(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareModuleExports", node, opts);
}

function assertDeclareTypeAlias(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareTypeAlias", node, opts);
}

function assertDeclareOpaqueType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareOpaqueType", node, opts);
}

function assertDeclareVariable(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareVariable", node, opts);
}

function assertDeclareExportDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareExportDeclaration", node, opts);
}

function assertDeclareExportAllDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclareExportAllDeclaration", node, opts);
}

function assertDeclaredPredicate(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DeclaredPredicate", node, opts);
}

function assertExistsTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExistsTypeAnnotation", node, opts);
}

function assertFunctionTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FunctionTypeAnnotation", node, opts);
}

function assertFunctionTypeParam(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FunctionTypeParam", node, opts);
}

function assertGenericTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("GenericTypeAnnotation", node, opts);
}

function assertInferredPredicate(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("InferredPredicate", node, opts);
}

function assertInterfaceExtends(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("InterfaceExtends", node, opts);
}

function assertInterfaceDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("InterfaceDeclaration", node, opts);
}

function assertIntersectionTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("IntersectionTypeAnnotation", node, opts);
}

function assertMixedTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("MixedTypeAnnotation", node, opts);
}

function assertEmptyTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("EmptyTypeAnnotation", node, opts);
}

function assertNullableTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("NullableTypeAnnotation", node, opts);
}

function assertNumberLiteralTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("NumberLiteralTypeAnnotation", node, opts);
}

function assertNumberTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("NumberTypeAnnotation", node, opts);
}

function assertObjectTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectTypeAnnotation", node, opts);
}

function assertObjectTypeCallProperty(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectTypeCallProperty", node, opts);
}

function assertObjectTypeIndexer(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectTypeIndexer", node, opts);
}

function assertObjectTypeProperty(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectTypeProperty", node, opts);
}

function assertObjectTypeSpreadProperty(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectTypeSpreadProperty", node, opts);
}

function assertOpaqueType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("OpaqueType", node, opts);
}

function assertQualifiedTypeIdentifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("QualifiedTypeIdentifier", node, opts);
}

function assertStringLiteralTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("StringLiteralTypeAnnotation", node, opts);
}

function assertStringTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("StringTypeAnnotation", node, opts);
}

function assertThisTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ThisTypeAnnotation", node, opts);
}

function assertTupleTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TupleTypeAnnotation", node, opts);
}

function assertTypeofTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TypeofTypeAnnotation", node, opts);
}

function assertTypeAlias(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TypeAlias", node, opts);
}

function assertTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TypeAnnotation", node, opts);
}

function assertTypeCastExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TypeCastExpression", node, opts);
}

function assertTypeParameter(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TypeParameter", node, opts);
}

function assertTypeParameterDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TypeParameterDeclaration", node, opts);
}

function assertTypeParameterInstantiation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TypeParameterInstantiation", node, opts);
}

function assertUnionTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("UnionTypeAnnotation", node, opts);
}

function assertVariance(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Variance", node, opts);
}

function assertVoidTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("VoidTypeAnnotation", node, opts);
}

function assertJSXAttribute(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXAttribute", node, opts);
}

function assertJSXClosingElement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXClosingElement", node, opts);
}

function assertJSXElement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXElement", node, opts);
}

function assertJSXEmptyExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXEmptyExpression", node, opts);
}

function assertJSXExpressionContainer(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXExpressionContainer", node, opts);
}

function assertJSXSpreadChild(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXSpreadChild", node, opts);
}

function assertJSXIdentifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXIdentifier", node, opts);
}

function assertJSXMemberExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXMemberExpression", node, opts);
}

function assertJSXNamespacedName(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXNamespacedName", node, opts);
}

function assertJSXOpeningElement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXOpeningElement", node, opts);
}

function assertJSXSpreadAttribute(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXSpreadAttribute", node, opts);
}

function assertJSXText(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXText", node, opts);
}

function assertJSXFragment(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXFragment", node, opts);
}

function assertJSXOpeningFragment(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXOpeningFragment", node, opts);
}

function assertJSXClosingFragment(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSXClosingFragment", node, opts);
}

function assertNoop(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Noop", node, opts);
}

function assertParenthesizedExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ParenthesizedExpression", node, opts);
}

function assertAwaitExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("AwaitExpression", node, opts);
}

function assertBindExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("BindExpression", node, opts);
}

function assertClassProperty(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ClassProperty", node, opts);
}

function assertImport(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Import", node, opts);
}

function assertDecorator(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Decorator", node, opts);
}

function assertDoExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("DoExpression", node, opts);
}

function assertExportDefaultSpecifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExportDefaultSpecifier", node, opts);
}

function assertExportNamespaceSpecifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExportNamespaceSpecifier", node, opts);
}

function assertTSParameterProperty(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSParameterProperty", node, opts);
}

function assertTSDeclareFunction(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSDeclareFunction", node, opts);
}

function assertTSDeclareMethod(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSDeclareMethod", node, opts);
}

function assertTSQualifiedName(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSQualifiedName", node, opts);
}

function assertTSCallSignatureDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSCallSignatureDeclaration", node, opts);
}

function assertTSConstructSignatureDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSConstructSignatureDeclaration", node, opts);
}

function assertTSPropertySignature(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSPropertySignature", node, opts);
}

function assertTSMethodSignature(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSMethodSignature", node, opts);
}

function assertTSIndexSignature(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSIndexSignature", node, opts);
}

function assertTSAnyKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSAnyKeyword", node, opts);
}

function assertTSNumberKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSNumberKeyword", node, opts);
}

function assertTSObjectKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSObjectKeyword", node, opts);
}

function assertTSBooleanKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSBooleanKeyword", node, opts);
}

function assertTSStringKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSStringKeyword", node, opts);
}

function assertTSSymbolKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSSymbolKeyword", node, opts);
}

function assertTSVoidKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSVoidKeyword", node, opts);
}

function assertTSUndefinedKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSUndefinedKeyword", node, opts);
}

function assertTSNullKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSNullKeyword", node, opts);
}

function assertTSNeverKeyword(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSNeverKeyword", node, opts);
}

function assertTSThisType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSThisType", node, opts);
}

function assertTSFunctionType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSFunctionType", node, opts);
}

function assertTSConstructorType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSConstructorType", node, opts);
}

function assertTSTypeReference(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeReference", node, opts);
}

function assertTSTypePredicate(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypePredicate", node, opts);
}

function assertTSTypeQuery(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeQuery", node, opts);
}

function assertTSTypeLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeLiteral", node, opts);
}

function assertTSArrayType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSArrayType", node, opts);
}

function assertTSTupleType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTupleType", node, opts);
}

function assertTSUnionType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSUnionType", node, opts);
}

function assertTSIntersectionType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSIntersectionType", node, opts);
}

function assertTSParenthesizedType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSParenthesizedType", node, opts);
}

function assertTSTypeOperator(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeOperator", node, opts);
}

function assertTSIndexedAccessType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSIndexedAccessType", node, opts);
}

function assertTSMappedType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSMappedType", node, opts);
}

function assertTSLiteralType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSLiteralType", node, opts);
}

function assertTSExpressionWithTypeArguments(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSExpressionWithTypeArguments", node, opts);
}

function assertTSInterfaceDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSInterfaceDeclaration", node, opts);
}

function assertTSInterfaceBody(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSInterfaceBody", node, opts);
}

function assertTSTypeAliasDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeAliasDeclaration", node, opts);
}

function assertTSAsExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSAsExpression", node, opts);
}

function assertTSTypeAssertion(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeAssertion", node, opts);
}

function assertTSEnumDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSEnumDeclaration", node, opts);
}

function assertTSEnumMember(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSEnumMember", node, opts);
}

function assertTSModuleDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSModuleDeclaration", node, opts);
}

function assertTSModuleBlock(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSModuleBlock", node, opts);
}

function assertTSImportEqualsDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSImportEqualsDeclaration", node, opts);
}

function assertTSExternalModuleReference(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSExternalModuleReference", node, opts);
}

function assertTSNonNullExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSNonNullExpression", node, opts);
}

function assertTSExportAssignment(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSExportAssignment", node, opts);
}

function assertTSNamespaceExportDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSNamespaceExportDeclaration", node, opts);
}

function assertTSTypeAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeAnnotation", node, opts);
}

function assertTSTypeParameterInstantiation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeParameterInstantiation", node, opts);
}

function assertTSTypeParameterDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeParameterDeclaration", node, opts);
}

function assertTSTypeParameter(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeParameter", node, opts);
}

function assertExpression(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Expression", node, opts);
}

function assertBinary(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Binary", node, opts);
}

function assertScopable(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Scopable", node, opts);
}

function assertBlockParent(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("BlockParent", node, opts);
}

function assertBlock(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Block", node, opts);
}

function assertStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Statement", node, opts);
}

function assertTerminatorless(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Terminatorless", node, opts);
}

function assertCompletionStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("CompletionStatement", node, opts);
}

function assertConditional(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Conditional", node, opts);
}

function assertLoop(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Loop", node, opts);
}

function assertWhile(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("While", node, opts);
}

function assertExpressionWrapper(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExpressionWrapper", node, opts);
}

function assertFor(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("For", node, opts);
}

function assertForXStatement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ForXStatement", node, opts);
}

function assertFunction(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Function", node, opts);
}

function assertFunctionParent(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FunctionParent", node, opts);
}

function assertPureish(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Pureish", node, opts);
}

function assertDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Declaration", node, opts);
}

function assertPatternLike(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("PatternLike", node, opts);
}

function assertLVal(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("LVal", node, opts);
}

function assertTSEntityName(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSEntityName", node, opts);
}

function assertLiteral(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Literal", node, opts);
}

function assertImmutable(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Immutable", node, opts);
}

function assertUserWhitespacable(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("UserWhitespacable", node, opts);
}

function assertMethod(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Method", node, opts);
}

function assertObjectMember(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ObjectMember", node, opts);
}

function assertProperty(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Property", node, opts);
}

function assertUnaryLike(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("UnaryLike", node, opts);
}

function assertPattern(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Pattern", node, opts);
}

function assertClass(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Class", node, opts);
}

function assertModuleDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ModuleDeclaration", node, opts);
}

function assertExportDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ExportDeclaration", node, opts);
}

function assertModuleSpecifier(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("ModuleSpecifier", node, opts);
}

function assertFlow(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("Flow", node, opts);
}

function assertFlowType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FlowType", node, opts);
}

function assertFlowBaseAnnotation(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FlowBaseAnnotation", node, opts);
}

function assertFlowDeclaration(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FlowDeclaration", node, opts);
}

function assertFlowPredicate(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("FlowPredicate", node, opts);
}

function assertJSX(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("JSX", node, opts);
}

function assertTSTypeElement(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSTypeElement", node, opts);
}

function assertTSType(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  assert("TSType", node, opts);
}

function assertNumberLiteral(node, opts) {
  console.trace("The node type NumberLiteral has been renamed to NumericLiteral");
  assert("NumberLiteral", node, opts);
}

function assertRegexLiteral(node, opts) {
  console.trace("The node type RegexLiteral has been renamed to RegExpLiteral");
  assert("RegexLiteral", node, opts);
}

function assertRestProperty(node, opts) {
  console.trace("The node type RestProperty has been renamed to RestElement");
  assert("RestProperty", node, opts);
}

function assertSpreadProperty(node, opts) {
  console.trace("The node type SpreadProperty has been renamed to SpreadElement");
  assert("SpreadProperty", node, opts);
}

/***/ }),
/* 718 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = createTypeAnnotationBasedOnTypeof;

var _generated = __webpack_require__(29);

function createTypeAnnotationBasedOnTypeof(type) {
  if (type === "string") {
    return (0, _generated.stringTypeAnnotation)();
  } else if (type === "number") {
    return (0, _generated.numberTypeAnnotation)();
  } else if (type === "undefined") {
    return (0, _generated.voidTypeAnnotation)();
  } else if (type === "boolean") {
    return (0, _generated.booleanTypeAnnotation)();
  } else if (type === "function") {
    return (0, _generated.genericTypeAnnotation)((0, _generated.identifier)("Function"));
  } else if (type === "object") {
    return (0, _generated.genericTypeAnnotation)((0, _generated.identifier)("Object"));
  } else if (type === "symbol") {
    return (0, _generated.genericTypeAnnotation)((0, _generated.identifier)("Symbol"));
  } else {
    throw new Error("Invalid typeof value");
  }
}

/***/ }),
/* 719 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = createUnionTypeAnnotation;

var _generated = __webpack_require__(29);

var _removeTypeDuplicates = _interopRequireDefault(__webpack_require__(272));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function createUnionTypeAnnotation(types) {
  var flattened = (0, _removeTypeDuplicates.default)(types);

  if (flattened.length === 1) {
    return flattened[0];
  } else {
    return (0, _generated.unionTypeAnnotation)(flattened);
  }
}

/***/ }),
/* 720 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = cloneDeep;

var _cloneNode = _interopRequireDefault(__webpack_require__(86));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function cloneDeep(node) {
  return (0, _cloneNode.default)(node);
}

/***/ }),
/* 721 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = cloneWithoutLoc;

var _clone = _interopRequireDefault(__webpack_require__(273));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function cloneWithoutLoc(node) {
  var newNode = (0, _clone.default)(node);
  newNode.loc = null;
  return newNode;
}

/***/ }),
/* 722 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = addComment;

var _addComments = _interopRequireDefault(__webpack_require__(274));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function addComment(node, type, content, line) {
  return (0, _addComments.default)(node, type, [{
    type: line ? "CommentLine" : "CommentBlock",
    value: content
  }]);
}

/***/ }),
/* 723 */
/***/ (function(module, exports, __webpack_require__) {

var SetCache = __webpack_require__(188),
    arrayIncludes = __webpack_require__(277),
    arrayIncludesWith = __webpack_require__(278),
    cacheHas = __webpack_require__(190),
    createSet = __webpack_require__(729),
    setToArray = __webpack_require__(191);

/** Used as the size to enable large array optimizations. */
var LARGE_ARRAY_SIZE = 200;

/**
 * The base implementation of `_.uniqBy` without support for iteratee shorthands.
 *
 * @private
 * @param {Array} array The array to inspect.
 * @param {Function} [iteratee] The iteratee invoked per element.
 * @param {Function} [comparator] The comparator invoked per element.
 * @returns {Array} Returns the new duplicate free array.
 */
function baseUniq(array, iteratee, comparator) {
  var index = -1,
      includes = arrayIncludes,
      length = array.length,
      isCommon = true,
      result = [],
      seen = result;

  if (comparator) {
    isCommon = false;
    includes = arrayIncludesWith;
  }
  else if (length >= LARGE_ARRAY_SIZE) {
    var set = iteratee ? null : createSet(array);
    if (set) {
      return setToArray(set);
    }
    isCommon = false;
    includes = cacheHas;
    seen = new SetCache;
  }
  else {
    seen = iteratee ? [] : result;
  }
  outer:
  while (++index < length) {
    var value = array[index],
        computed = iteratee ? iteratee(value) : value;

    value = (comparator || value !== 0) ? value : 0;
    if (isCommon && computed === computed) {
      var seenIndex = seen.length;
      while (seenIndex--) {
        if (seen[seenIndex] === computed) {
          continue outer;
        }
      }
      if (iteratee) {
        seen.push(computed);
      }
      result.push(value);
    }
    else if (!includes(seen, computed, comparator)) {
      if (seen !== result) {
        seen.push(computed);
      }
      result.push(value);
    }
  }
  return result;
}

module.exports = baseUniq;


/***/ }),
/* 724 */
/***/ (function(module, exports) {

/** Used to stand-in for `undefined` hash values. */
var HASH_UNDEFINED = '__lodash_hash_undefined__';

/**
 * Adds `value` to the array cache.
 *
 * @private
 * @name add
 * @memberOf SetCache
 * @alias push
 * @param {*} value The value to cache.
 * @returns {Object} Returns the cache instance.
 */
function setCacheAdd(value) {
  this.__data__.set(value, HASH_UNDEFINED);
  return this;
}

module.exports = setCacheAdd;


/***/ }),
/* 725 */
/***/ (function(module, exports) {

/**
 * Checks if `value` is in the array cache.
 *
 * @private
 * @name has
 * @memberOf SetCache
 * @param {*} value The value to search for.
 * @returns {number} Returns `true` if `value` is found, else `false`.
 */
function setCacheHas(value) {
  return this.__data__.has(value);
}

module.exports = setCacheHas;


/***/ }),
/* 726 */
/***/ (function(module, exports, __webpack_require__) {

var baseFindIndex = __webpack_require__(189),
    baseIsNaN = __webpack_require__(727),
    strictIndexOf = __webpack_require__(728);

/**
 * The base implementation of `_.indexOf` without `fromIndex` bounds checks.
 *
 * @private
 * @param {Array} array The array to inspect.
 * @param {*} value The value to search for.
 * @param {number} fromIndex The index to search from.
 * @returns {number} Returns the index of the matched value, else `-1`.
 */
function baseIndexOf(array, value, fromIndex) {
  return value === value
    ? strictIndexOf(array, value, fromIndex)
    : baseFindIndex(array, baseIsNaN, fromIndex);
}

module.exports = baseIndexOf;


/***/ }),
/* 727 */
/***/ (function(module, exports) {

/**
 * The base implementation of `_.isNaN` without support for number objects.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is `NaN`, else `false`.
 */
function baseIsNaN(value) {
  return value !== value;
}

module.exports = baseIsNaN;


/***/ }),
/* 728 */
/***/ (function(module, exports) {

/**
 * A specialized version of `_.indexOf` which performs strict equality
 * comparisons of values, i.e. `===`.
 *
 * @private
 * @param {Array} array The array to inspect.
 * @param {*} value The value to search for.
 * @param {number} fromIndex The index to search from.
 * @returns {number} Returns the index of the matched value, else `-1`.
 */
function strictIndexOf(array, value, fromIndex) {
  var index = fromIndex - 1,
      length = array.length;

  while (++index < length) {
    if (array[index] === value) {
      return index;
    }
  }
  return -1;
}

module.exports = strictIndexOf;


/***/ }),
/* 729 */
/***/ (function(module, exports, __webpack_require__) {

var Set = __webpack_require__(268),
    noop = __webpack_require__(730),
    setToArray = __webpack_require__(191);

/** Used as references for various `Number` constants. */
var INFINITY = 1 / 0;

/**
 * Creates a set object of `values`.
 *
 * @private
 * @param {Array} values The values to add to the set.
 * @returns {Object} Returns the new set.
 */
var createSet = !(Set && (1 / setToArray(new Set([,-0]))[1]) == INFINITY) ? noop : function(values) {
  return new Set(values);
};

module.exports = createSet;


/***/ }),
/* 730 */
/***/ (function(module, exports) {

/**
 * This method returns `undefined`.
 *
 * @static
 * @memberOf _
 * @since 2.3.0
 * @category Util
 * @example
 *
 * _.times(2, _.noop);
 * // => [undefined, undefined]
 */
function noop() {
  // No operation performed.
}

module.exports = noop;


/***/ }),
/* 731 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = removeComments;

var _constants = __webpack_require__(50);

function removeComments(node) {
  _constants.COMMENT_KEYS.forEach(function (key) {
    node[key] = null;
  });

  return node;
}

/***/ }),
/* 732 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.TSTYPE_TYPES = exports.TSTYPEELEMENT_TYPES = exports.JSX_TYPES = exports.FLOWPREDICATE_TYPES = exports.FLOWDECLARATION_TYPES = exports.FLOWBASEANNOTATION_TYPES = exports.FLOWTYPE_TYPES = exports.FLOW_TYPES = exports.MODULESPECIFIER_TYPES = exports.EXPORTDECLARATION_TYPES = exports.MODULEDECLARATION_TYPES = exports.CLASS_TYPES = exports.PATTERN_TYPES = exports.UNARYLIKE_TYPES = exports.PROPERTY_TYPES = exports.OBJECTMEMBER_TYPES = exports.METHOD_TYPES = exports.USERWHITESPACABLE_TYPES = exports.IMMUTABLE_TYPES = exports.LITERAL_TYPES = exports.TSENTITYNAME_TYPES = exports.LVAL_TYPES = exports.PATTERNLIKE_TYPES = exports.DECLARATION_TYPES = exports.PUREISH_TYPES = exports.FUNCTIONPARENT_TYPES = exports.FUNCTION_TYPES = exports.FORXSTATEMENT_TYPES = exports.FOR_TYPES = exports.EXPRESSIONWRAPPER_TYPES = exports.WHILE_TYPES = exports.LOOP_TYPES = exports.CONDITIONAL_TYPES = exports.COMPLETIONSTATEMENT_TYPES = exports.TERMINATORLESS_TYPES = exports.STATEMENT_TYPES = exports.BLOCK_TYPES = exports.BLOCKPARENT_TYPES = exports.SCOPABLE_TYPES = exports.BINARY_TYPES = exports.EXPRESSION_TYPES = void 0;

var _definitions = __webpack_require__(35);

var EXPRESSION_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Expression"];
exports.EXPRESSION_TYPES = EXPRESSION_TYPES;
var BINARY_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Binary"];
exports.BINARY_TYPES = BINARY_TYPES;
var SCOPABLE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Scopable"];
exports.SCOPABLE_TYPES = SCOPABLE_TYPES;
var BLOCKPARENT_TYPES = _definitions.FLIPPED_ALIAS_KEYS["BlockParent"];
exports.BLOCKPARENT_TYPES = BLOCKPARENT_TYPES;
var BLOCK_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Block"];
exports.BLOCK_TYPES = BLOCK_TYPES;
var STATEMENT_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Statement"];
exports.STATEMENT_TYPES = STATEMENT_TYPES;
var TERMINATORLESS_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Terminatorless"];
exports.TERMINATORLESS_TYPES = TERMINATORLESS_TYPES;
var COMPLETIONSTATEMENT_TYPES = _definitions.FLIPPED_ALIAS_KEYS["CompletionStatement"];
exports.COMPLETIONSTATEMENT_TYPES = COMPLETIONSTATEMENT_TYPES;
var CONDITIONAL_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Conditional"];
exports.CONDITIONAL_TYPES = CONDITIONAL_TYPES;
var LOOP_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Loop"];
exports.LOOP_TYPES = LOOP_TYPES;
var WHILE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["While"];
exports.WHILE_TYPES = WHILE_TYPES;
var EXPRESSIONWRAPPER_TYPES = _definitions.FLIPPED_ALIAS_KEYS["ExpressionWrapper"];
exports.EXPRESSIONWRAPPER_TYPES = EXPRESSIONWRAPPER_TYPES;
var FOR_TYPES = _definitions.FLIPPED_ALIAS_KEYS["For"];
exports.FOR_TYPES = FOR_TYPES;
var FORXSTATEMENT_TYPES = _definitions.FLIPPED_ALIAS_KEYS["ForXStatement"];
exports.FORXSTATEMENT_TYPES = FORXSTATEMENT_TYPES;
var FUNCTION_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Function"];
exports.FUNCTION_TYPES = FUNCTION_TYPES;
var FUNCTIONPARENT_TYPES = _definitions.FLIPPED_ALIAS_KEYS["FunctionParent"];
exports.FUNCTIONPARENT_TYPES = FUNCTIONPARENT_TYPES;
var PUREISH_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Pureish"];
exports.PUREISH_TYPES = PUREISH_TYPES;
var DECLARATION_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Declaration"];
exports.DECLARATION_TYPES = DECLARATION_TYPES;
var PATTERNLIKE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["PatternLike"];
exports.PATTERNLIKE_TYPES = PATTERNLIKE_TYPES;
var LVAL_TYPES = _definitions.FLIPPED_ALIAS_KEYS["LVal"];
exports.LVAL_TYPES = LVAL_TYPES;
var TSENTITYNAME_TYPES = _definitions.FLIPPED_ALIAS_KEYS["TSEntityName"];
exports.TSENTITYNAME_TYPES = TSENTITYNAME_TYPES;
var LITERAL_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Literal"];
exports.LITERAL_TYPES = LITERAL_TYPES;
var IMMUTABLE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Immutable"];
exports.IMMUTABLE_TYPES = IMMUTABLE_TYPES;
var USERWHITESPACABLE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["UserWhitespacable"];
exports.USERWHITESPACABLE_TYPES = USERWHITESPACABLE_TYPES;
var METHOD_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Method"];
exports.METHOD_TYPES = METHOD_TYPES;
var OBJECTMEMBER_TYPES = _definitions.FLIPPED_ALIAS_KEYS["ObjectMember"];
exports.OBJECTMEMBER_TYPES = OBJECTMEMBER_TYPES;
var PROPERTY_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Property"];
exports.PROPERTY_TYPES = PROPERTY_TYPES;
var UNARYLIKE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["UnaryLike"];
exports.UNARYLIKE_TYPES = UNARYLIKE_TYPES;
var PATTERN_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Pattern"];
exports.PATTERN_TYPES = PATTERN_TYPES;
var CLASS_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Class"];
exports.CLASS_TYPES = CLASS_TYPES;
var MODULEDECLARATION_TYPES = _definitions.FLIPPED_ALIAS_KEYS["ModuleDeclaration"];
exports.MODULEDECLARATION_TYPES = MODULEDECLARATION_TYPES;
var EXPORTDECLARATION_TYPES = _definitions.FLIPPED_ALIAS_KEYS["ExportDeclaration"];
exports.EXPORTDECLARATION_TYPES = EXPORTDECLARATION_TYPES;
var MODULESPECIFIER_TYPES = _definitions.FLIPPED_ALIAS_KEYS["ModuleSpecifier"];
exports.MODULESPECIFIER_TYPES = MODULESPECIFIER_TYPES;
var FLOW_TYPES = _definitions.FLIPPED_ALIAS_KEYS["Flow"];
exports.FLOW_TYPES = FLOW_TYPES;
var FLOWTYPE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["FlowType"];
exports.FLOWTYPE_TYPES = FLOWTYPE_TYPES;
var FLOWBASEANNOTATION_TYPES = _definitions.FLIPPED_ALIAS_KEYS["FlowBaseAnnotation"];
exports.FLOWBASEANNOTATION_TYPES = FLOWBASEANNOTATION_TYPES;
var FLOWDECLARATION_TYPES = _definitions.FLIPPED_ALIAS_KEYS["FlowDeclaration"];
exports.FLOWDECLARATION_TYPES = FLOWDECLARATION_TYPES;
var FLOWPREDICATE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["FlowPredicate"];
exports.FLOWPREDICATE_TYPES = FLOWPREDICATE_TYPES;
var JSX_TYPES = _definitions.FLIPPED_ALIAS_KEYS["JSX"];
exports.JSX_TYPES = JSX_TYPES;
var TSTYPEELEMENT_TYPES = _definitions.FLIPPED_ALIAS_KEYS["TSTypeElement"];
exports.TSTYPEELEMENT_TYPES = TSTYPEELEMENT_TYPES;
var TSTYPE_TYPES = _definitions.FLIPPED_ALIAS_KEYS["TSType"];
exports.TSTYPE_TYPES = TSTYPE_TYPES;

/***/ }),
/* 733 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = ensureBlock;

var _toBlock = _interopRequireDefault(__webpack_require__(282));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function ensureBlock(node, key) {
  if (key === void 0) {
    key = "body";
  }

  return node[key] = (0, _toBlock.default)(node[key], node);
}

/***/ }),
/* 734 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = toBindingIdentifierName;

var _toIdentifier = _interopRequireDefault(__webpack_require__(283));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function toBindingIdentifierName(name) {
  name = (0, _toIdentifier.default)(name);
  if (name === "eval" || name === "arguments") name = "_" + name;
  return name;
}

/***/ }),
/* 735 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = toComputedKey;

var _generated = __webpack_require__(17);

var _generated2 = __webpack_require__(29);

function toComputedKey(node, key) {
  if (key === void 0) {
    key = node.key || node.property;
  }

  if (!node.computed && (0, _generated.isIdentifier)(key)) key = (0, _generated2.stringLiteral)(key.name);
  return key;
}

/***/ }),
/* 736 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = toExpression;

var _generated = __webpack_require__(17);

function toExpression(node) {
  if ((0, _generated.isExpressionStatement)(node)) {
    node = node.expression;
  }

  if ((0, _generated.isExpression)(node)) {
    return node;
  }

  if ((0, _generated.isClass)(node)) {
    node.type = "ClassExpression";
  } else if ((0, _generated.isFunction)(node)) {
    node.type = "FunctionExpression";
  }

  if (!(0, _generated.isExpression)(node)) {
    throw new Error("cannot turn " + node.type + " to an expression");
  }

  return node;
}

/***/ }),
/* 737 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = toKeyAlias;

var _generated = __webpack_require__(17);

var _cloneNode = _interopRequireDefault(__webpack_require__(86));

var _removePropertiesDeep = _interopRequireDefault(__webpack_require__(284));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function toKeyAlias(node, key) {
  if (key === void 0) {
    key = node.key;
  }

  var alias;

  if (node.kind === "method") {
    return toKeyAlias.increment() + "";
  } else if ((0, _generated.isIdentifier)(key)) {
    alias = key.name;
  } else if ((0, _generated.isStringLiteral)(key)) {
    alias = JSON.stringify(key.value);
  } else {
    alias = JSON.stringify((0, _removePropertiesDeep.default)((0, _cloneNode.default)(key)));
  }

  if (node.computed) {
    alias = "[" + alias + "]";
  }

  if (node.static) {
    alias = "static:" + alias;
  }

  return alias;
}

toKeyAlias.uid = 0;

toKeyAlias.increment = function () {
  if (toKeyAlias.uid >= Number.MAX_SAFE_INTEGER) {
    return toKeyAlias.uid = 0;
  } else {
    return toKeyAlias.uid++;
  }
};

/***/ }),
/* 738 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = toSequenceExpression;

var _gatherSequenceExpressions = _interopRequireDefault(__webpack_require__(739));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function toSequenceExpression(nodes, scope) {
  if (!nodes || !nodes.length) return;
  var declars = [];
  var result = (0, _gatherSequenceExpressions.default)(nodes, scope, declars);
  if (!result) return;

  for (var _i = 0; _i < declars.length; _i++) {
    var declar = declars[_i];
    scope.push(declar);
  }

  return result;
}

/***/ }),
/* 739 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = gatherSequenceExpressions;

var _getBindingIdentifiers = _interopRequireDefault(__webpack_require__(118));

var _generated = __webpack_require__(17);

var _generated2 = __webpack_require__(29);

var _cloneNode = _interopRequireDefault(__webpack_require__(86));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function gatherSequenceExpressions(nodes, scope, declars) {
  var exprs = [];
  var ensureLastUndefined = true;

  for (var _iterator = nodes, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
    var _ref;

    if (_isArray) {
      if (_i >= _iterator.length) break;
      _ref = _iterator[_i++];
    } else {
      _i = _iterator.next();
      if (_i.done) break;
      _ref = _i.value;
    }

    var _node = _ref;
    ensureLastUndefined = false;

    if ((0, _generated.isExpression)(_node)) {
      exprs.push(_node);
    } else if ((0, _generated.isExpressionStatement)(_node)) {
      exprs.push(_node.expression);
    } else if ((0, _generated.isVariableDeclaration)(_node)) {
      if (_node.kind !== "var") return;
      var _arr = _node.declarations;

      for (var _i2 = 0; _i2 < _arr.length; _i2++) {
        var declar = _arr[_i2];
        var bindings = (0, _getBindingIdentifiers.default)(declar);

        for (var key in bindings) {
          declars.push({
            kind: _node.kind,
            id: (0, _cloneNode.default)(bindings[key])
          });
        }

        if (declar.init) {
          exprs.push((0, _generated2.assignmentExpression)("=", declar.id, declar.init));
        }
      }

      ensureLastUndefined = true;
    } else if ((0, _generated.isIfStatement)(_node)) {
      var consequent = _node.consequent ? gatherSequenceExpressions([_node.consequent], scope, declars) : scope.buildUndefinedNode();
      var alternate = _node.alternate ? gatherSequenceExpressions([_node.alternate], scope, declars) : scope.buildUndefinedNode();
      if (!consequent || !alternate) return;
      exprs.push((0, _generated2.conditionalExpression)(_node.test, consequent, alternate));
    } else if ((0, _generated.isBlockStatement)(_node)) {
      var body = gatherSequenceExpressions(_node.body, scope, declars);
      if (!body) return;
      exprs.push(body);
    } else if ((0, _generated.isEmptyStatement)(_node)) {
      ensureLastUndefined = true;
    } else {
      return;
    }
  }

  if (ensureLastUndefined) {
    exprs.push(scope.buildUndefinedNode());
  }

  if (exprs.length === 1) {
    return exprs[0];
  } else {
    return (0, _generated2.sequenceExpression)(exprs);
  }
}

/***/ }),
/* 740 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = toStatement;

var _generated = __webpack_require__(17);

var _generated2 = __webpack_require__(29);

function toStatement(node, ignore) {
  if ((0, _generated.isStatement)(node)) {
    return node;
  }

  var mustHaveId = false;
  var newType;

  if ((0, _generated.isClass)(node)) {
    mustHaveId = true;
    newType = "ClassDeclaration";
  } else if ((0, _generated.isFunction)(node)) {
    mustHaveId = true;
    newType = "FunctionDeclaration";
  } else if ((0, _generated.isAssignmentExpression)(node)) {
    return (0, _generated2.expressionStatement)(node);
  }

  if (mustHaveId && !node.id) {
    newType = false;
  }

  if (!newType) {
    if (ignore) {
      return false;
    } else {
      throw new Error("cannot turn " + node.type + " to a statement");
    }
  }

  node.type = newType;
  return node;
}

/***/ }),
/* 741 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = valueToNode;

var _isPlainObject = _interopRequireDefault(__webpack_require__(742));

var _isRegExp = _interopRequireDefault(__webpack_require__(743));

var _isValidIdentifier = _interopRequireDefault(__webpack_require__(83));

var _generated = __webpack_require__(29);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function valueToNode(value) {
  if (value === undefined) {
    return (0, _generated.identifier)("undefined");
  }

  if (value === true || value === false) {
    return (0, _generated.booleanLiteral)(value);
  }

  if (value === null) {
    return (0, _generated.nullLiteral)();
  }

  if (typeof value === "string") {
    return (0, _generated.stringLiteral)(value);
  }

  if (typeof value === "number") {
    return (0, _generated.numericLiteral)(value);
  }

  if ((0, _isRegExp.default)(value)) {
    var pattern = value.source;
    var flags = value.toString().match(/\/([a-z]+|)$/)[1];
    return (0, _generated.regExpLiteral)(pattern, flags);
  }

  if (Array.isArray(value)) {
    return (0, _generated.arrayExpression)(value.map(valueToNode));
  }

  if ((0, _isPlainObject.default)(value)) {
    var props = [];

    for (var key in value) {
      var nodeKey = void 0;

      if ((0, _isValidIdentifier.default)(key)) {
        nodeKey = (0, _generated.identifier)(key);
      } else {
        nodeKey = (0, _generated.stringLiteral)(key);
      }

      props.push((0, _generated.objectProperty)(nodeKey, valueToNode(value[key])));
    }

    return (0, _generated.objectExpression)(props);
  }

  throw new Error("don't know how to turn this value into a node");
}

/***/ }),
/* 742 */
/***/ (function(module, exports, __webpack_require__) {

var baseGetTag = __webpack_require__(23),
    getPrototype = __webpack_require__(185),
    isObjectLike = __webpack_require__(21);

/** `Object#toString` result references. */
var objectTag = '[object Object]';

/** Used for built-in method references. */
var funcProto = Function.prototype,
    objectProto = Object.prototype;

/** Used to resolve the decompiled source of functions. */
var funcToString = funcProto.toString;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/** Used to infer the `Object` constructor. */
var objectCtorString = funcToString.call(Object);

/**
 * Checks if `value` is a plain object, that is, an object created by the
 * `Object` constructor or one with a `[[Prototype]]` of `null`.
 *
 * @static
 * @memberOf _
 * @since 0.8.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a plain object, else `false`.
 * @example
 *
 * function Foo() {
 *   this.a = 1;
 * }
 *
 * _.isPlainObject(new Foo);
 * // => false
 *
 * _.isPlainObject([1, 2, 3]);
 * // => false
 *
 * _.isPlainObject({ 'x': 0, 'y': 0 });
 * // => true
 *
 * _.isPlainObject(Object.create(null));
 * // => true
 */
function isPlainObject(value) {
  if (!isObjectLike(value) || baseGetTag(value) != objectTag) {
    return false;
  }
  var proto = getPrototype(value);
  if (proto === null) {
    return true;
  }
  var Ctor = hasOwnProperty.call(proto, 'constructor') && proto.constructor;
  return typeof Ctor == 'function' && Ctor instanceof Ctor &&
    funcToString.call(Ctor) == objectCtorString;
}

module.exports = isPlainObject;


/***/ }),
/* 743 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsRegExp = __webpack_require__(744),
    baseUnary = __webpack_require__(84),
    nodeUtil = __webpack_require__(115);

/* Node.js helper references. */
var nodeIsRegExp = nodeUtil && nodeUtil.isRegExp;

/**
 * Checks if `value` is classified as a `RegExp` object.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a regexp, else `false`.
 * @example
 *
 * _.isRegExp(/abc/);
 * // => true
 *
 * _.isRegExp('/abc/');
 * // => false
 */
var isRegExp = nodeIsRegExp ? baseUnary(nodeIsRegExp) : baseIsRegExp;

module.exports = isRegExp;


/***/ }),
/* 744 */
/***/ (function(module, exports, __webpack_require__) {

var baseGetTag = __webpack_require__(23),
    isObjectLike = __webpack_require__(21);

/** `Object#toString` result references. */
var regexpTag = '[object RegExp]';

/**
 * The base implementation of `_.isRegExp` without Node.js optimizations.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is a regexp, else `false`.
 */
function baseIsRegExp(value) {
  return isObjectLike(value) && baseGetTag(value) == regexpTag;
}

module.exports = baseIsRegExp;


/***/ }),
/* 745 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = appendToMemberExpression;

var _generated = __webpack_require__(29);

function appendToMemberExpression(member, append, computed) {
  if (computed === void 0) {
    computed = false;
  }

  member.object = (0, _generated.memberExpression)(member.object, member.property, member.computed);
  member.property = append;
  member.computed = !!computed;
  return member;
}

/***/ }),
/* 746 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = inherits;

var _constants = __webpack_require__(50);

var _inheritsComments = _interopRequireDefault(__webpack_require__(280));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inherits(child, parent) {
  if (!child || !parent) return child;
  var _arr = _constants.INHERIT_KEYS.optional;

  for (var _i = 0; _i < _arr.length; _i++) {
    var key = _arr[_i];

    if (child[key] == null) {
      child[key] = parent[key];
    }
  }

  for (var _key in parent) {
    if (_key[0] === "_" && _key !== "__clone") child[_key] = parent[_key];
  }

  var _arr2 = _constants.INHERIT_KEYS.force;

  for (var _i2 = 0; _i2 < _arr2.length; _i2++) {
    var _key2 = _arr2[_i2];
    child[_key2] = parent[_key2];
  }

  (0, _inheritsComments.default)(child, parent);
  return child;
}

/***/ }),
/* 747 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = prependToMemberExpression;

var _generated = __webpack_require__(29);

function prependToMemberExpression(member, prepend) {
  member.object = (0, _generated.memberExpression)(prepend, member.object);
  return member;
}

/***/ }),
/* 748 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = getOuterBindingIdentifiers;

var _getBindingIdentifiers = _interopRequireDefault(__webpack_require__(118));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function getOuterBindingIdentifiers(node, duplicates) {
  return (0, _getBindingIdentifiers.default)(node, duplicates, true);
}

/***/ }),
/* 749 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = traverse;

var _definitions = __webpack_require__(35);

function traverse(node, handlers, state) {
  if (typeof handlers === "function") {
    handlers = {
      enter: handlers
    };
  }

  var _ref = handlers,
      enter = _ref.enter,
      exit = _ref.exit;
  traverseSimpleImpl(node, enter, exit, state, []);
}

function traverseSimpleImpl(node, enter, exit, state, ancestors) {
  var keys = _definitions.VISITOR_KEYS[node.type];
  if (!keys) return;
  if (enter) enter(node, ancestors, state);

  for (var _iterator = keys, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
    var _ref2;

    if (_isArray) {
      if (_i >= _iterator.length) break;
      _ref2 = _iterator[_i++];
    } else {
      _i = _iterator.next();
      if (_i.done) break;
      _ref2 = _i.value;
    }

    var _key2 = _ref2;
    var subNode = node[_key2];

    if (Array.isArray(subNode)) {
      for (var i = 0; i < subNode.length; i++) {
        var child = subNode[i];
        if (!child) continue;
        ancestors.push({
          node: node,
          key: _key2,
          index: i
        });
        traverseSimpleImpl(child, enter, exit, state, ancestors);
        ancestors.pop();
      }
    } else if (subNode) {
      ancestors.push({
        node: node,
        key: _key2
      });
      traverseSimpleImpl(subNode, enter, exit, state, ancestors);
      ancestors.pop();
    }
  }

  if (exit) exit(node, ancestors, state);
}

/***/ }),
/* 750 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isBinding;

var _getBindingIdentifiers = _interopRequireDefault(__webpack_require__(118));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function isBinding(node, parent) {
  var keys = _getBindingIdentifiers.default.keys[parent.type];

  if (keys) {
    for (var i = 0; i < keys.length; i++) {
      var key = keys[i];
      var val = parent[key];

      if (Array.isArray(val)) {
        if (val.indexOf(node) >= 0) return true;
      } else {
        if (val === node) return true;
      }
    }
  }

  return false;
}

/***/ }),
/* 751 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isBlockScoped;

var _generated = __webpack_require__(17);

var _isLet = _interopRequireDefault(__webpack_require__(287));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function isBlockScoped(node) {
  return (0, _generated.isFunctionDeclaration)(node) || (0, _generated.isClassDeclaration)(node) || (0, _isLet.default)(node);
}

/***/ }),
/* 752 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isImmutable;

var _isType = _interopRequireDefault(__webpack_require__(177));

var _generated = __webpack_require__(17);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function isImmutable(node) {
  if ((0, _isType.default)(node.type, "Immutable")) return true;

  if ((0, _generated.isIdentifier)(node)) {
    if (node.name === "undefined") {
      return true;
    } else {
      return false;
    }
  }

  return false;
}

/***/ }),
/* 753 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isNodesEquivalent;

var _definitions = __webpack_require__(35);

function isNodesEquivalent(a, b) {
  if (typeof a !== "object" || typeof b !== "object" || a == null || b == null) {
    return a === b;
  }

  if (a.type !== b.type) {
    return false;
  }

  var fields = Object.keys(_definitions.NODE_FIELDS[a.type] || a.type);

  for (var _i = 0; _i < fields.length; _i++) {
    var field = fields[_i];

    if (typeof a[field] !== typeof b[field]) {
      return false;
    }

    if (Array.isArray(a[field])) {
      if (!Array.isArray(b[field])) {
        return false;
      }

      if (a[field].length !== b[field].length) {
        return false;
      }

      for (var i = 0; i < a[field].length; i++) {
        if (!isNodesEquivalent(a[field][i], b[field][i])) {
          return false;
        }
      }

      continue;
    }

    if (!isNodesEquivalent(a[field], b[field])) {
      return false;
    }
  }

  return true;
}

/***/ }),
/* 754 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isReferenced;

function isReferenced(node, parent) {
  switch (parent.type) {
    case "BindExpression":
      return parent.object === node || parent.callee === node;

    case "MemberExpression":
    case "JSXMemberExpression":
      if (parent.property === node && parent.computed) {
        return true;
      } else if (parent.object === node) {
        return true;
      } else {
        return false;
      }

    case "MetaProperty":
      return false;

    case "ObjectProperty":
      if (parent.key === node) {
        return parent.computed;
      }

    case "VariableDeclarator":
      return parent.id !== node;

    case "ArrowFunctionExpression":
    case "FunctionDeclaration":
    case "FunctionExpression":
      var _arr = parent.params;

      for (var _i = 0; _i < _arr.length; _i++) {
        var param = _arr[_i];
        if (param === node) return false;
      }

      return parent.id !== node;

    case "ExportSpecifier":
      if (parent.source) {
        return false;
      } else {
        return parent.local === node;
      }

    case "ExportNamespaceSpecifier":
    case "ExportDefaultSpecifier":
      return false;

    case "JSXAttribute":
      return parent.name !== node;

    case "ClassProperty":
      if (parent.key === node) {
        return parent.computed;
      } else {
        return parent.value === node;
      }

    case "ImportDefaultSpecifier":
    case "ImportNamespaceSpecifier":
    case "ImportSpecifier":
      return false;

    case "ClassDeclaration":
    case "ClassExpression":
      return parent.id !== node;

    case "ClassMethod":
    case "ObjectMethod":
      return parent.key === node && parent.computed;

    case "LabeledStatement":
      return false;

    case "CatchClause":
      return parent.param !== node;

    case "RestElement":
      return false;

    case "AssignmentExpression":
      return parent.right === node;

    case "AssignmentPattern":
      return parent.right === node;

    case "ObjectPattern":
    case "ArrayPattern":
      return false;
  }

  return true;
}

/***/ }),
/* 755 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isScope;

var _generated = __webpack_require__(17);

function isScope(node, parent) {
  if ((0, _generated.isBlockStatement)(node) && (0, _generated.isFunction)(parent, {
    body: node
  })) {
    return false;
  }

  if ((0, _generated.isBlockStatement)(node) && (0, _generated.isCatchClause)(parent, {
    body: node
  })) {
    return false;
  }

  return (0, _generated.isScopable)(node);
}

/***/ }),
/* 756 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isSpecifierDefault;

var _generated = __webpack_require__(17);

function isSpecifierDefault(specifier) {
  return (0, _generated.isImportDefaultSpecifier)(specifier) || (0, _generated.isIdentifier)(specifier.imported || specifier.exported, {
    name: "default"
  });
}

/***/ }),
/* 757 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isValidES3Identifier;

var _isValidIdentifier = _interopRequireDefault(__webpack_require__(83));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var RESERVED_WORDS_ES3_ONLY = new Set(["abstract", "boolean", "byte", "char", "double", "enum", "final", "float", "goto", "implements", "int", "interface", "long", "native", "package", "private", "protected", "public", "short", "static", "synchronized", "throws", "transient", "volatile"]);

function isValidES3Identifier(name) {
  return (0, _isValidIdentifier.default)(name) && !RESERVED_WORDS_ES3_ONLY.has(name);
}

/***/ }),
/* 758 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


exports.__esModule = true;
exports.default = isVar;

var _generated = __webpack_require__(17);

var _constants = __webpack_require__(50);

function isVar(node) {
  return (0, _generated.isVariableDeclaration)(node, {
    kind: "var"
  }) && !node[_constants.BLOCK_SCOPED_SYMBOL];
}

/***/ }),
/* 759 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.parseScriptTags = exports.parseScripts = exports.parseScript = exports.getCandidateScriptLocations = exports.generateWhitespace = exports.extractScriptTags = undefined;

var _types = __webpack_require__(28);

var types = _interopRequireWildcard(_types);

var _babylon = __webpack_require__(289);

var babylon = _interopRequireWildcard(_babylon);

var _customParse = __webpack_require__(760);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function parseScript(_ref) {
  var source = _ref.source,
      line = _ref.line;

  // remove empty or only whitespace scripts
  if (source.length === 0 || /^\s+$/.test(source)) {
    return null;
  }

  try {
    return babylon.parse(source, {
      sourceType: "script",
      startLine: line
    });
  } catch (e) {
    return null;
  }
}

function parseScripts(locations) {
  var parser = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : parseScript;

  return (0, _customParse.parseScripts)(locations, parser);
}

function extractScriptTags(source) {
  return parseScripts((0, _customParse.getCandidateScriptLocations)(source), function (loc) {
    var ast = parseScript(loc);

    if (ast) {
      return loc;
    }

    return null;
  }).filter(types.isFile);
}

function parseScriptTags(source) {
  var parser = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : parseScript;

  return (0, _customParse.parseScriptTags)(source, parser);
}

exports.default = parseScriptTags;
exports.extractScriptTags = extractScriptTags;
exports.generateWhitespace = _customParse.generateWhitespace;
exports.getCandidateScriptLocations = _customParse.getCandidateScriptLocations;
exports.parseScript = parseScript;
exports.parseScripts = parseScripts;
exports.parseScriptTags = parseScriptTags;

/***/ }),
/* 760 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.parseScriptTags = exports.parseScripts = exports.getCandidateScriptLocations = exports.generateWhitespace = undefined;

var _types = __webpack_require__(28);

var types = _interopRequireWildcard(_types);

var _parseScriptFragment = __webpack_require__(761);

var _parseScriptFragment2 = _interopRequireDefault(_parseScriptFragment);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _toConsumableArray(arr) { if (Array.isArray(arr)) { for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) { arr2[i] = arr[i]; } return arr2; } else { return Array.from(arr); } }

var startScript = /<script[^>]*>/im;
var endScript = /<\/script\s*>/im;
// https://stackoverflow.com/questions/5034781/js-regex-to-split-by-line#comment5633979_5035005
var newLines = /\r\n|[\n\v\f\r\x85\u2028\u2029]/;

function getType(tag) {
  var fragment = (0, _parseScriptFragment2.default)(tag);

  if (fragment) {
    var type = fragment.attributes.type;

    return type ? type.toLowerCase() : null;
  }

  return null;
}

function getCandidateScriptLocations(source, index) {
  var i = index || 0;
  var str = source.substring(i);

  var startMatch = startScript.exec(str);
  if (startMatch) {
    var startsAt = startMatch.index + startMatch[0].length;
    var afterStart = str.substring(startsAt);
    var endMatch = endScript.exec(afterStart);
    if (endMatch) {
      var locLength = endMatch.index;
      var locIndex = i + startsAt;
      var endIndex = locIndex + locLength + endMatch[0].length;

      // extract the complete tag (incl start and end tags and content). if the
      // type is invalid (= not JS), skip this tag and continue
      var tag = source.substring(i + startMatch.index, endIndex);
      var type = getType(tag);
      if (type && type !== "javascript" && type !== "text/javascript") {
        return getCandidateScriptLocations(source, endIndex);
      }

      return [adjustForLineAndColumn(source, {
        index: locIndex,
        length: locLength,
        source: source.substring(locIndex, locIndex + locLength)
      })].concat(_toConsumableArray(getCandidateScriptLocations(source, endIndex)));
    }
  }

  return [];
}

function parseScripts(locations, parser) {
  return locations.map(parser);
}

function generateWhitespace(length) {
  return Array.from(new Array(length + 1)).join(" ");
}

function calcLineAndColumn(source, index) {
  var lines = source.substring(0, index).split(newLines);
  var line = lines.length;
  var column = lines.pop().length + 1;

  return {
    column: column,
    line: line
  };
}

function adjustForLineAndColumn(fullSource, location) {
  var _calcLineAndColumn = calcLineAndColumn(fullSource, location.index),
      column = _calcLineAndColumn.column,
      line = _calcLineAndColumn.line;

  return Object.assign({}, location, {
    line: line,
    column: column,
    // prepend whitespace for scripts that do not start on the first column
    source: generateWhitespace(column) + location.source
  });
}

function parseScriptTags(source, parser) {
  var scripts = parseScripts(getCandidateScriptLocations(source), parser).filter(types.isFile).reduce(function (main, script) {
    return {
      statements: main.statements.concat(script.program.body),
      comments: main.comments.concat(script.comments),
      tokens: main.tokens.concat(script.tokens)
    };
  }, {
    statements: [],
    comments: [],
    tokens: []
  });

  var program = types.program(scripts.statements);
  var file = types.file(program, scripts.comments, scripts.tokens);

  var end = calcLineAndColumn(source, source.length);
  file.start = program.start = 0;
  file.end = program.end = source.length;
  file.loc = program.loc = {
    start: {
      line: 1,
      column: 0
    },
    end: end
  };

  return file;
}

exports.default = parseScriptTags;
exports.generateWhitespace = generateWhitespace;
exports.getCandidateScriptLocations = getCandidateScriptLocations;
exports.parseScripts = parseScripts;
exports.parseScriptTags = parseScriptTags;

/***/ }),
/* 761 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});

var alphanum = /[a-z0-9\-]/i;

function parseToken(str, start) {
  var i = start;
  while (i < str.length && alphanum.test(str.charAt(i++))) {
    continue;
  }

  if (i !== start) {
    return {
      token: str.substring(start, i - 1),
      index: i
    };
  }

  return null;
}

function parseAttributes(str, start) {
  var i = start;
  var attributes = {};
  var attribute = null;

  while (i < str.length) {
    var c = str.charAt(i);

    if (attribute === null && c == ">") {
      break;
    } else if (attribute === null && alphanum.test(c)) {
      attribute = {
        name: null,
        value: true,
        bool: true,
        terminator: null
      };

      var attributeNameNode = parseToken(str, i);
      if (attributeNameNode) {
        attribute.name = attributeNameNode.token;
        i = attributeNameNode.index - 2;
      }
    } else if (attribute !== null) {
      if (c === "=") {
        // once we've started an attribute, look for = to indicate
        // it's a non-boolean attribute
        attribute.bool = false;
        if (attribute.value === true) {
          attribute.value = "";
        }
      } else if (!attribute.bool && attribute.terminator === null && (c === '"' || c === "'")) {
        // once we've determined it's non-boolean, look for a
        // value terminator (", ')
        attribute.terminator = c;
      } else if (attribute.terminator) {
        if (c === attribute.terminator) {
          // if we had a terminator and found another, we've
          // reach the end of the attribute
          attributes[attribute.name] = attribute.value;
          attribute = null;
        } else {
          // otherwise, append the character to the attribute value
          attribute.value += c;

          // check for an escaped terminator and push it as well
          // to avoid terminating prematurely
          if (c === "\\") {
            var next = str.charAt(i + 1);
            if (next === attribute.terminator) {
              attribute.value += next;
              i += 1;
            }
          }
        }
      } else if (!/\s/.test(c)) {
        // if we've hit a non-space character and aren't processing a value,
        // we're starting a new attribute so push the attribute and clear the
        // local variable
        attributes[attribute.name] = attribute.value;
        attribute = null;

        // move the cursor back to re-find the start of the attribute
        i -= 1;
      }
    }

    i++;
  }

  if (i !== start) {
    return {
      attributes: attributes,
      index: i
    };
  }

  return null;
}

function parseFragment(str) {
  var start = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;

  var tag = null;
  var open = false;
  var attributes = {};

  var i = start;
  while (i < str.length) {
    var c = str.charAt(i++);

    if (!open && !tag && c === "<") {
      // Open Start Tag
      open = true;

      var tagNode = parseToken(str, i);
      if (!tagNode) {
        return null;
      }

      i = tagNode.index - 1;
      tag = tagNode.token;
    } else if (open && c === ">") {
      // Close Start Tag
      break;
    } else if (open) {
      // Attributes
      var attributeNode = parseAttributes(str, i - 1);

      if (attributeNode) {
        i = attributeNode.index;
        attributes = attributeNode.attributes || attributes;
      }
    }
  }

  if (tag) {
    return {
      tag: tag,
      attributes: attributes
    };
  }

  return null;
}

exports.default = parseFragment;
exports.parseFragment = parseFragment;

/***/ }),
/* 762 */
/***/ (function(module, exports, __webpack_require__) {

var baseFlatten = __webpack_require__(293);

/**
 * Flattens `array` a single level deep.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Array
 * @param {Array} array The array to flatten.
 * @returns {Array} Returns the new flattened array.
 * @example
 *
 * _.flatten([1, [2, [3, [4]], 5]]);
 * // => [1, 2, [3, [4]], 5]
 */
function flatten(array) {
  var length = array == null ? 0 : array.length;
  return length ? baseFlatten(array, 1) : [];
}

module.exports = flatten;


/***/ }),
/* 763 */
/***/ (function(module, exports, __webpack_require__) {

var Symbol = __webpack_require__(20),
    isArguments = __webpack_require__(113),
    isArray = __webpack_require__(16);

/** Built-in value references. */
var spreadableSymbol = Symbol ? Symbol.isConcatSpreadable : undefined;

/**
 * Checks if `value` is a flattenable `arguments` object or array.
 *
 * @private
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is flattenable, else `false`.
 */
function isFlattenable(value) {
  return isArray(value) || isArguments(value) ||
    !!(spreadableSymbol && value && value[spreadableSymbol]);
}

module.exports = isFlattenable;


/***/ }),
/* 764 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.inferClassName = inferClassName;

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

// the function class is inferred from a call like
// createClass or extend
function fromCallExpression(callExpression) {
  const whitelist = ["extend", "createClass"];
  const callee = callExpression.node.callee;
  if (!callee) {
    return null;
  }

  const name = t.isMemberExpression(callee) ? callee.property.name : callee.name;

  if (!whitelist.includes(name)) {
    return null;
  }

  const variable = callExpression.findParent(p => t.isVariableDeclarator(p.node));
  if (variable) {
    return variable.node.id.name;
  }

  const assignment = callExpression.findParent(p => t.isAssignmentExpression(p.node));

  if (!assignment) {
    return null;
  }

  const left = assignment.node.left;

  if (left.name) {
    return name;
  }

  if (t.isMemberExpression(left)) {
    return left.property.name;
  }

  return null;
}

// the function class is inferred from a prototype assignment
// e.g. TodoClass.prototype.render = function() {}
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function fromPrototype(assignment) {
  const left = assignment.node.left;
  if (!left) {
    return null;
  }

  if (t.isMemberExpression(left) && left.object && t.isMemberExpression(left.object) && left.object.property.identifier === "prototype") {
    return left.object.object.name;
  }

  return null;
}

// infer class finds an appropriate class for functions
// that are defined inside of a class like thing.
// e.g. `class Foo`, `TodoClass.prototype.foo`,
//      `Todo = createClass({ foo: () => {}})`
function inferClassName(path) {
  const classDeclaration = path.findParent(p => t.isClassDeclaration(p.node));
  if (classDeclaration) {
    return classDeclaration.node.id.name;
  }

  const callExpression = path.findParent(p => t.isCallExpression(p.node));
  if (callExpression) {
    return fromCallExpression(callExpression);
  }

  const assignment = path.findParent(p => t.isAssignmentExpression(p.node));
  if (assignment) {
    return fromPrototype(assignment);
  }

  return null;
}

/***/ }),
/* 765 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = getScopes;
exports.clearScopes = clearScopes;

var _visitor = __webpack_require__(766);

let parsedScopesCache = new Map(); /* This Source Code Form is subject to the terms of the Mozilla Public
                                    * License, v. 2.0. If a copy of the MPL was not distributed with this
                                    * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function getScopes(location) {
  const { sourceId } = location;
  let parsedScopes = parsedScopesCache.get(sourceId);
  if (!parsedScopes) {
    parsedScopes = (0, _visitor.parseSourceScopes)(sourceId);
    parsedScopesCache.set(sourceId, parsedScopes);
  }
  return parsedScopes ? findScopes(parsedScopes, location) : [];
}

function clearScopes() {
  parsedScopesCache = new Map();
}

/**
 * Searches all scopes and their bindings at the specific location.
 */
function findScopes(scopes, location) {
  // Find inner most in the tree structure.
  let searchInScopes = scopes;
  const found = [];
  while (searchInScopes) {
    const foundOne = searchInScopes.some(s => {
      if (compareLocations(s.start, location) <= 0 && compareLocations(location, s.end) < 0) {
        // Found the next scope, trying to search recusevly in its children.
        found.unshift(s);
        searchInScopes = s.children;
        return true;
      }
      return false;
    });
    if (!foundOne) {
      break;
    }
  }
  return found.map(i => {
    return {
      type: i.type,
      displayName: i.displayName,
      start: i.start,
      end: i.end,
      bindings: i.bindings
    };
  });
}

function compareLocations(a, b) {
  // According to type of Location.column can be undefined, if will not be the
  // case here, ignoring flow error.
  // $FlowIgnore
  return a.line == b.line ? a.column - b.column : a.line - b.line;
}

/***/ }),
/* 766 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.parseSourceScopes = parseSourceScopes;

var _isEmpty = __webpack_require__(290);

var _isEmpty2 = _interopRequireDefault(_isEmpty);

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

var _devtoolsSourceMap = __webpack_require__(12);

var _getFunctionName = __webpack_require__(294);

var _getFunctionName2 = _interopRequireDefault(_getFunctionName);

var _ast = __webpack_require__(51);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * "implicit"
 * Variables added automaticly like "this" and "arguments"
 *
 * "var"
 * Variables declared with "var" or non-block function declarations
 *
 * "let"
 * Variables declared with "let".
 *
 * "const"
 * Variables declared with "const", or added as const
 * bindings like inner function expressions and inner class names.
 *
 * "import"
 * Imported binding names exposed from other modules.
 */


// Location information about the expression immediartely surrounding a
// given binding reference.
function parseSourceScopes(sourceId) {
  const ast = (0, _ast.getAst)(sourceId);
  if ((0, _isEmpty2.default)(ast)) {
    return null;
  }

  const { global, lexical } = createGlobalScope(ast, sourceId);

  const state = {
    sourceId,
    scope: lexical,
    scopeStack: []
  };
  t.traverse(ast, scopeCollectionVisitor, state);

  // TODO: This should probably check for ".mjs" extension on the
  // original file, and should also be skipped if the the generated
  // code is an ES6 module rather than a script.
  if ((0, _devtoolsSourceMap.isGeneratedId)(sourceId) || ast.program.sourceType === "script" && !looksLikeCommonJS(global)) {
    stripModuleScope(global);
  }

  return toParsedScopes([global], sourceId) || [];
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function toParsedScopes(children, sourceId) {
  if (!children || children.length === 0) {
    return undefined;
  }
  return children.map(scope => {
    // Removing unneed information from TempScope such as parent reference.
    // We also need to convert BabelLocation to the Location type.
    return {
      start: scope.loc.start,
      end: scope.loc.end,
      type: scope.type === "module" ? "block" : scope.type,
      displayName: scope.displayName,
      bindings: scope.bindings,
      children: toParsedScopes(scope.children, sourceId)
    };
  });
}

function createTempScope(type, displayName, parent, loc) {
  const result = {
    type,
    displayName,
    parent,
    children: [],
    loc,
    bindings: Object.create(null)
  };
  if (parent) {
    parent.children.push(result);
  }
  return result;
}
function pushTempScope(state, type, displayName, loc) {
  const scope = createTempScope(type, displayName, state.scope, loc);

  state.scope = scope;
  return scope;
}

function isNode(node, type) {
  return node ? node.type === type : false;
}

function getVarScope(scope) {
  let s = scope;
  while (s.type !== "function" && s.type !== "module") {
    if (!s.parent) {
      return s;
    }
    s = s.parent;
  }
  return s;
}

function fromBabelLocation(location, sourceId) {
  return {
    sourceId,
    line: location.line,
    column: location.column
  };
}

function parseDeclarator(declaratorId, targetScope, type, declaration, sourceId) {
  if (isNode(declaratorId, "Identifier")) {
    let existing = targetScope.bindings[declaratorId.name];
    if (!existing) {
      existing = {
        type,
        refs: []
      };
      targetScope.bindings[declaratorId.name] = existing;
    }
    existing.refs.push({
      type: "decl",
      start: fromBabelLocation(declaratorId.loc.start, sourceId),
      end: fromBabelLocation(declaratorId.loc.end, sourceId),
      declaration: {
        start: fromBabelLocation(declaration.loc.start, sourceId),
        end: fromBabelLocation(declaration.loc.end, sourceId)
      }
    });
  } else if (isNode(declaratorId, "ObjectPattern")) {
    declaratorId.properties.forEach(prop => {
      parseDeclarator(prop.value, targetScope, type, declaration, sourceId);
    });
  } else if (isNode(declaratorId, "ArrayPattern")) {
    declaratorId.elements.forEach(item => {
      parseDeclarator(item, targetScope, type, declaration, sourceId);
    });
  } else if (isNode(declaratorId, "AssignmentPattern")) {
    parseDeclarator(declaratorId.left, targetScope, type, declaration, sourceId);
  } else if (isNode(declaratorId, "RestElement")) {
    parseDeclarator(declaratorId.argument, targetScope, type, declaration, sourceId);
  }
}

function isLetOrConst(node) {
  return node.kind === "let" || node.kind === "const";
}

function hasLexicalDeclaration(node, parent) {
  const isFunctionBody = t.isFunction(parent, { body: node });

  return node.body.some(child => isLexicalVariable(child) || !isFunctionBody && child.type === "FunctionDeclaration" || child.type === "ClassDeclaration");
}
function isLexicalVariable(node) {
  return isNode(node, "VariableDeclaration") && isLetOrConst(node);
}

function findIdentifierInScopes(scope, name) {
  // Find nearest outer scope with the specifed name and add reference.
  for (let s = scope; s; s = s.parent) {
    if (name in s.bindings) {
      return s;
    }
  }
  return null;
}

function createGlobalScope(ast, sourceId) {
  const global = createTempScope("object", "Global", null, {
    start: fromBabelLocation(ast.loc.start, sourceId),
    end: fromBabelLocation(ast.loc.end, sourceId)
  });

  // Include fake bindings to collect references to CommonJS
  Object.assign(global.bindings, {
    module: {
      type: "var",
      refs: []
    },
    exports: {
      type: "var",
      refs: []
    },
    __dirname: {
      type: "var",
      refs: []
    },
    __filename: {
      type: "var",
      refs: []
    },
    require: {
      type: "var",
      refs: []
    }
  });

  const lexical = createTempScope("block", "Lexical Global", global, {
    start: fromBabelLocation(ast.loc.start, sourceId),
    end: fromBabelLocation(ast.loc.end, sourceId)
  });

  return {
    global,
    lexical
  };
}

const scopeCollectionVisitor = {
  // eslint-disable-next-line complexity
  enter(node, ancestors, state) {
    state.scopeStack.push(state.scope);

    const parentNode = ancestors.length === 0 ? null : ancestors[ancestors.length - 1].node;

    if (t.isProgram(node)) {
      const scope = pushTempScope(state, "module", "Module", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
      scope.bindings.this = {
        type: "implicit",
        refs: []
      };
    } else if (t.isFunction(node)) {
      let scope = state.scope;
      if (t.isFunctionExpression(node) && isNode(node.id, "Identifier")) {
        scope = pushTempScope(state, "block", "Function Expression", {
          start: fromBabelLocation(node.loc.start, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId)
        });
        scope.bindings[node.id.name] = {
          type: "const",
          refs: [{
            type: "decl",
            start: fromBabelLocation(node.id.loc.start, state.sourceId),
            end: fromBabelLocation(node.id.loc.end, state.sourceId),
            declaration: {
              start: fromBabelLocation(node.loc.start, state.sourceId),
              end: fromBabelLocation(node.loc.end, state.sourceId)
            }
          }]
        };
      }

      if (t.isFunctionDeclaration(node) && isNode(node.id, "Identifier")) {
        // This ignores Annex B function declaration hoisting, which
        // is probably a fine assumption.
        const fnScope = getVarScope(scope);
        scope.bindings[node.id.name] = {
          type: fnScope === scope ? "var" : "let",
          refs: [{
            type: "decl",
            start: fromBabelLocation(node.id.loc.start, state.sourceId),
            end: fromBabelLocation(node.id.loc.end, state.sourceId),
            declaration: {
              start: fromBabelLocation(node.loc.start, state.sourceId),
              end: fromBabelLocation(node.loc.end, state.sourceId)
            }
          }]
        };
      }

      scope = pushTempScope(state, "function", (0, _getFunctionName2.default)(node, parentNode), {
        // Being at the start of a function doesn't count as
        // being inside of it.
        start: fromBabelLocation(node.params[0] ? node.params[0].loc.start : node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });

      node.params.forEach(param => parseDeclarator(param, scope, "var", node, state.sourceId));

      if (!t.isArrowFunctionExpression(node)) {
        scope.bindings.this = {
          type: "implicit",
          refs: []
        };
        scope.bindings.arguments = {
          type: "implicit",
          refs: []
        };
      }
    } else if (t.isClass(node)) {
      if (t.isClassDeclaration(node) && t.isIdentifier(node.id)) {
        state.scope.bindings[node.id.name] = {
          type: "let",
          refs: [{
            type: "decl",
            start: fromBabelLocation(node.id.loc.start, state.sourceId),
            end: fromBabelLocation(node.id.loc.end, state.sourceId),
            declaration: {
              start: fromBabelLocation(node.loc.start, state.sourceId),
              end: fromBabelLocation(node.loc.end, state.sourceId)
            }
          }]
        };
      }

      if (t.isIdentifier(node.id)) {
        const scope = pushTempScope(state, "block", "Class", {
          start: fromBabelLocation(node.loc.start, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId)
        });

        scope.bindings[node.id.name] = {
          type: "const",
          refs: [{
            type: "decl",
            start: fromBabelLocation(node.id.loc.start, state.sourceId),
            end: fromBabelLocation(node.id.loc.end, state.sourceId),
            declaration: {
              start: fromBabelLocation(node.loc.start, state.sourceId),
              end: fromBabelLocation(node.loc.end, state.sourceId)
            }
          }]
        };
      }
    } else if (t.isForXStatement(node) || t.isForStatement(node)) {
      const init = node.init || node.left;
      if (isNode(init, "VariableDeclaration") && isLetOrConst(init)) {
        // Debugger will create new lexical environment for the for.
        pushTempScope(state, "block", "For", {
          // Being at the start of a for loop doesn't count as
          // being inside it.
          start: fromBabelLocation(init.loc.start, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId)
        });
      }
    } else if (t.isCatchClause(node)) {
      const scope = pushTempScope(state, "block", "Catch", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
      parseDeclarator(node.param, scope, "var", node, state.sourceId);
    } else if (t.isBlockStatement(node) && hasLexicalDeclaration(node, parentNode)) {
      // Debugger will create new lexical environment for the block.
      pushTempScope(state, "block", "Block", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
    } else if (t.isVariableDeclaration(node) && (node.kind === "var" ||
    // Lexical declarations in for statements are handled above.
    !t.isForStatement(parentNode, { init: node }) || !t.isForXStatement(parentNode, { left: node }))) {
      // Finds right lexical environment
      const hoistAt = !isLetOrConst(node) ? getVarScope(state.scope) : state.scope;
      node.declarations.forEach(declarator => {
        parseDeclarator(declarator.id, hoistAt, node.kind, node, state.sourceId);
      });
    } else if (t.isImportDeclaration(node)) {
      node.specifiers.forEach(spec => {
        if (t.isImportNamespaceSpecifier(spec)) {
          state.scope.bindings[spec.local.name] = {
            // Imported namespaces aren't live import bindings, they are
            // just normal const bindings.
            type: "const",
            refs: [{
              type: "decl",
              start: fromBabelLocation(spec.local.loc.start, state.sourceId),
              end: fromBabelLocation(spec.local.loc.end, state.sourceId)
            }]
          };
        } else {
          state.scope.bindings[spec.local.name] = {
            type: "import",
            refs: [{
              type: "decl",
              start: fromBabelLocation(spec.local.loc.start, state.sourceId),
              end: fromBabelLocation(spec.local.loc.end, state.sourceId),
              importName: t.isImportDefaultSpecifier(spec) ? "default" : spec.imported.name,
              declaration: {
                start: fromBabelLocation(node.loc.start, state.sourceId),
                end: fromBabelLocation(node.loc.end, state.sourceId)
              }
            }]
          };
        }
      });
    } else if (t.isIdentifier(node) && t.isReferenced(node, parentNode)) {
      const identScope = findIdentifierInScopes(state.scope, node.name);
      if (identScope) {
        identScope.bindings[node.name].refs.push({
          type: "ref",
          start: fromBabelLocation(node.loc.start, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId),
          meta: buildMetaBindings(state.sourceId, node, ancestors)
        });
      }
    } else if (t.isThisExpression(node)) {
      const identScope = findIdentifierInScopes(state.scope, "this");
      if (identScope) {
        identScope.bindings.this.refs.push({
          type: "ref",
          start: fromBabelLocation(node.loc.start, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId),
          meta: buildMetaBindings(state.sourceId, node, ancestors)
        });
      }
    } else if (t.isClassProperty(parentNode, { value: node })) {
      const scope = pushTempScope(state, "function", "Class Field", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
      scope.bindings.this = {
        type: "implicit",
        refs: []
      };
      scope.bindings.arguments = {
        type: "implicit",
        refs: []
      };
    } else if (t.isSwitchStatement(node) && node.cases.some(caseNode => caseNode.consequent.some(child => isLexicalVariable(child)))) {
      pushTempScope(state, "block", "Switch", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
    }
  },
  exit(node, ancestors, state) {
    const scope = state.scopeStack.pop();
    if (!scope) {
      throw new Error("Assertion failure - unsynchronized pop");
    }

    state.scope = scope;
  }
};

function buildMetaBindings(sourceId, node, ancestors, parentIndex = ancestors.length - 1) {
  if (parentIndex <= 1) {
    return null;
  }
  const parent = ancestors[parentIndex].node;
  const grandparent = ancestors[parentIndex - 1].node;

  // Consider "0, foo" to be equivalent to "foo".
  if (t.isSequenceExpression(parent) && parent.expressions.length === 2 && t.isNumericLiteral(parent.expressions[0]) && parent.expressions[1] === node) {
    let start = parent.loc.start;
    let end = parent.loc.end;

    if (t.isCallExpression(grandparent, { callee: parent })) {
      // Attempt to expand the range around parentheses, e.g.
      // (0, foo.bar)()
      start = grandparent.loc.start;
      end = Object.assign({}, end);
      end.column += 1;
    }

    return {
      type: "inherit",
      start: fromBabelLocation(start, sourceId),
      end: fromBabelLocation(end, sourceId),
      parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
    };
  }

  // Consider "Object(foo)" to be equivalent to "foo"
  if (t.isCallExpression(parent) && t.isIdentifier(parent.callee, { name: "Object" }) && parent.arguments.length === 1 && parent.arguments[0] === node) {
    return {
      type: "inherit",
      start: fromBabelLocation(parent.loc.start, sourceId),
      end: fromBabelLocation(parent.loc.end, sourceId),
      parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
    };
  }

  if (t.isMemberExpression(parent, { object: node })) {
    if (parent.computed) {
      if (t.isStringLiteral(parent.property)) {
        return {
          type: "member",
          start: fromBabelLocation(parent.loc.start, sourceId),
          end: fromBabelLocation(parent.loc.end, sourceId),
          property: parent.property.value,
          parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
        };
      }
    } else {
      return {
        type: "member",
        start: fromBabelLocation(parent.loc.start, sourceId),
        end: fromBabelLocation(parent.loc.end, sourceId),
        property: parent.property.name,
        parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
      };
    }
  }
  if (t.isCallExpression(parent, { callee: node }) && parent.arguments.length == 0) {
    return {
      type: "call",
      start: fromBabelLocation(parent.loc.start, sourceId),
      end: fromBabelLocation(parent.loc.end, sourceId),
      parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
    };
  }

  return null;
}

function looksLikeCommonJS(rootScope) {
  return rootScope.bindings.__dirname.refs.length > 0 || rootScope.bindings.__filename.refs.length > 0 || rootScope.bindings.require.refs.length > 0 || rootScope.bindings.exports.refs.length > 0 || rootScope.bindings.module.refs.length > 0;
}

function stripModuleScope(rootScope) {
  const rootLexicalScope = rootScope.children[0];
  const moduleScope = rootLexicalScope.children[0];
  if (moduleScope.type !== "module") {
    throw new Error("Assertion failure - should be module");
  }

  Object.keys(moduleScope.bindings).forEach(name => {
    const binding = moduleScope.bindings[name];
    if (binding.type === "let" || binding.type === "const") {
      rootLexicalScope.bindings[name] = binding;
    } else {
      rootScope.bindings[name] = binding;
    }
  });
  rootLexicalScope.children = moduleScope.children;
  rootLexicalScope.children.forEach(child => {
    child.parent = rootLexicalScope;
  });
}

/***/ }),
/* 767 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; /* This Source Code Form is subject to the terms of the Mozilla Public
                                                                                                                                                                                                                                                                   * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                                                                                                                                                                                                                                   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

var _get = __webpack_require__(88);

var _get2 = _interopRequireDefault(_get);

var _findIndex = __webpack_require__(768);

var _findIndex2 = _interopRequireDefault(_findIndex);

var _findLastIndex = __webpack_require__(786);

var _findLastIndex2 = _interopRequireDefault(_findLastIndex);

var _contains = __webpack_require__(292);

var _getSymbols = __webpack_require__(193);

var _getSymbols2 = _interopRequireDefault(_getSymbols);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function findSymbols(source) {
  const { functions, comments } = (0, _getSymbols2.default)(source);
  return { functions, comments };
}

/**
 * Returns the location for a given function path. If the path represents a
 * function declaration, the location will begin after the function identifier
 * but before the function parameters.
 */

function getLocation(func) {
  const location = _extends({}, func.location);

  // if the function has an identifier, start the block after it so the
  // identifier is included in the "scope" of its parent
  const identifierEnd = (0, _get2.default)(func, "identifier.loc.end");
  if (identifierEnd) {
    location.start = identifierEnd;
  }

  return location;
}

/**
 * Find the nearest location containing the input position and
 * return new locations without inner locations under that nearest location
 *
 * @param locations Notice! The locations MUST be sorted by `sortByStart`
 *                  so that we can do linear time complexity operation.
 */
function removeInnerLocations(locations, position) {
  // First, let's find the nearest position-enclosing function location,
  // which is to find the last location enclosing the position.
  const newLocs = locations.slice();
  const parentIndex = (0, _findLastIndex2.default)(newLocs, loc => (0, _contains.containsPosition)(loc, position));
  if (parentIndex < 0) {
    return newLocs;
  }

  // Second, from the nearest location, loop locations again, stop looping
  // once seeing the 1st location not enclosed by the nearest location
  // to find the last inner locations inside the nearest location.
  const innerStartIndex = parentIndex + 1;
  const parentLoc = newLocs[parentIndex];
  const outerBoundaryIndex = (0, _findIndex2.default)(newLocs, loc => !(0, _contains.containsLocation)(parentLoc, loc), innerStartIndex);
  const innerBoundaryIndex = outerBoundaryIndex < 0 ? newLocs.length - 1 : outerBoundaryIndex - 1;

  // Third, remove those inner functions
  newLocs.splice(innerStartIndex, innerBoundaryIndex - parentIndex);
  return newLocs;
}

/**
 * Return an new locations array which excludes
 * items that are completely enclosed by another location in the input locations
 *
 * @param locations Notice! The locations MUST be sorted by `sortByStart`
 *                  so that we can do linear time complexity operation.
 */
function removeOverlaps(locations) {
  if (locations.length == 0) {
    return [];
  }
  const firstParent = locations[0];
  return locations.reduce(deduplicateNode, [firstParent]);
}

function deduplicateNode(nodes, location) {
  const parent = nodes[nodes.length - 1];
  if (!(0, _contains.containsLocation)(parent, location)) {
    nodes.push(location);
  }
  return nodes;
}

/**
 * Sorts an array of locations by start position
 */
function sortByStart(a, b) {
  if (a.start.line < b.start.line) {
    return -1;
  } else if (a.start.line === b.start.line) {
    return a.start.column - b.start.column;
  }

  return 1;
}

/**
 * Returns an array of locations that are considered out of scope for the given
 * location.
 */
function findOutOfScopeLocations(sourceId, position) {
  const { functions, comments } = findSymbols(sourceId);
  const commentLocations = comments.map(c => c.location);
  let locations = functions.map(getLocation).concat(commentLocations).sort(sortByStart);
  // Must remove inner locations then filter, otherwise,
  // we will mis-judge in-scope inner locations as out of scope.
  locations = removeInnerLocations(locations, position).filter(loc => !(0, _contains.containsPosition)(loc, position));
  return removeOverlaps(locations);
}

exports.default = findOutOfScopeLocations;

/***/ }),
/* 768 */
/***/ (function(module, exports, __webpack_require__) {

var baseFindIndex = __webpack_require__(189),
    baseIteratee = __webpack_require__(295),
    toInteger = __webpack_require__(300);

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeMax = Math.max;

/**
 * This method is like `_.find` except that it returns the index of the first
 * element `predicate` returns truthy for instead of the element itself.
 *
 * @static
 * @memberOf _
 * @since 1.1.0
 * @category Array
 * @param {Array} array The array to inspect.
 * @param {Function} [predicate=_.identity] The function invoked per iteration.
 * @param {number} [fromIndex=0] The index to search from.
 * @returns {number} Returns the index of the found element, else `-1`.
 * @example
 *
 * var users = [
 *   { 'user': 'barney',  'active': false },
 *   { 'user': 'fred',    'active': false },
 *   { 'user': 'pebbles', 'active': true }
 * ];
 *
 * _.findIndex(users, function(o) { return o.user == 'barney'; });
 * // => 0
 *
 * // The `_.matches` iteratee shorthand.
 * _.findIndex(users, { 'user': 'fred', 'active': false });
 * // => 1
 *
 * // The `_.matchesProperty` iteratee shorthand.
 * _.findIndex(users, ['active', false]);
 * // => 0
 *
 * // The `_.property` iteratee shorthand.
 * _.findIndex(users, 'active');
 * // => 2
 */
function findIndex(array, predicate, fromIndex) {
  var length = array == null ? 0 : array.length;
  if (!length) {
    return -1;
  }
  var index = fromIndex == null ? 0 : toInteger(fromIndex);
  if (index < 0) {
    index = nativeMax(length + index, 0);
  }
  return baseFindIndex(array, baseIteratee(predicate, 3), index);
}

module.exports = findIndex;


/***/ }),
/* 769 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsMatch = __webpack_require__(770),
    getMatchData = __webpack_require__(776),
    matchesStrictComparable = __webpack_require__(299);

/**
 * The base implementation of `_.matches` which doesn't clone `source`.
 *
 * @private
 * @param {Object} source The object of property values to match.
 * @returns {Function} Returns the new spec function.
 */
function baseMatches(source) {
  var matchData = getMatchData(source);
  if (matchData.length == 1 && matchData[0][2]) {
    return matchesStrictComparable(matchData[0][0], matchData[0][1]);
  }
  return function(object) {
    return object === source || baseIsMatch(object, source, matchData);
  };
}

module.exports = baseMatches;


/***/ }),
/* 770 */
/***/ (function(module, exports, __webpack_require__) {

var Stack = __webpack_require__(180),
    baseIsEqual = __webpack_require__(296);

/** Used to compose bitmasks for value comparisons. */
var COMPARE_PARTIAL_FLAG = 1,
    COMPARE_UNORDERED_FLAG = 2;

/**
 * The base implementation of `_.isMatch` without support for iteratee shorthands.
 *
 * @private
 * @param {Object} object The object to inspect.
 * @param {Object} source The object of property values to match.
 * @param {Array} matchData The property names, values, and compare flags to match.
 * @param {Function} [customizer] The function to customize comparisons.
 * @returns {boolean} Returns `true` if `object` is a match, else `false`.
 */
function baseIsMatch(object, source, matchData, customizer) {
  var index = matchData.length,
      length = index,
      noCustomizer = !customizer;

  if (object == null) {
    return !length;
  }
  object = Object(object);
  while (index--) {
    var data = matchData[index];
    if ((noCustomizer && data[2])
          ? data[1] !== object[data[0]]
          : !(data[0] in object)
        ) {
      return false;
    }
  }
  while (++index < length) {
    data = matchData[index];
    var key = data[0],
        objValue = object[key],
        srcValue = data[1];

    if (noCustomizer && data[2]) {
      if (objValue === undefined && !(key in object)) {
        return false;
      }
    } else {
      var stack = new Stack;
      if (customizer) {
        var result = customizer(objValue, srcValue, key, object, source, stack);
      }
      if (!(result === undefined
            ? baseIsEqual(srcValue, objValue, COMPARE_PARTIAL_FLAG | COMPARE_UNORDERED_FLAG, customizer, stack)
            : result
          )) {
        return false;
      }
    }
  }
  return true;
}

module.exports = baseIsMatch;


/***/ }),
/* 771 */
/***/ (function(module, exports, __webpack_require__) {

var Stack = __webpack_require__(180),
    equalArrays = __webpack_require__(297),
    equalByTag = __webpack_require__(773),
    equalObjects = __webpack_require__(775),
    getTag = __webpack_require__(85),
    isArray = __webpack_require__(16),
    isBuffer = __webpack_require__(114),
    isTypedArray = __webpack_require__(181);

/** Used to compose bitmasks for value comparisons. */
var COMPARE_PARTIAL_FLAG = 1;

/** `Object#toString` result references. */
var argsTag = '[object Arguments]',
    arrayTag = '[object Array]',
    objectTag = '[object Object]';

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * A specialized version of `baseIsEqual` for arrays and objects which performs
 * deep comparisons and tracks traversed objects enabling objects with circular
 * references to be compared.
 *
 * @private
 * @param {Object} object The object to compare.
 * @param {Object} other The other object to compare.
 * @param {number} bitmask The bitmask flags. See `baseIsEqual` for more details.
 * @param {Function} customizer The function to customize comparisons.
 * @param {Function} equalFunc The function to determine equivalents of values.
 * @param {Object} [stack] Tracks traversed `object` and `other` objects.
 * @returns {boolean} Returns `true` if the objects are equivalent, else `false`.
 */
function baseIsEqualDeep(object, other, bitmask, customizer, equalFunc, stack) {
  var objIsArr = isArray(object),
      othIsArr = isArray(other),
      objTag = objIsArr ? arrayTag : getTag(object),
      othTag = othIsArr ? arrayTag : getTag(other);

  objTag = objTag == argsTag ? objectTag : objTag;
  othTag = othTag == argsTag ? objectTag : othTag;

  var objIsObj = objTag == objectTag,
      othIsObj = othTag == objectTag,
      isSameTag = objTag == othTag;

  if (isSameTag && isBuffer(object)) {
    if (!isBuffer(other)) {
      return false;
    }
    objIsArr = true;
    objIsObj = false;
  }
  if (isSameTag && !objIsObj) {
    stack || (stack = new Stack);
    return (objIsArr || isTypedArray(object))
      ? equalArrays(object, other, bitmask, customizer, equalFunc, stack)
      : equalByTag(object, other, objTag, bitmask, customizer, equalFunc, stack);
  }
  if (!(bitmask & COMPARE_PARTIAL_FLAG)) {
    var objIsWrapped = objIsObj && hasOwnProperty.call(object, '__wrapped__'),
        othIsWrapped = othIsObj && hasOwnProperty.call(other, '__wrapped__');

    if (objIsWrapped || othIsWrapped) {
      var objUnwrapped = objIsWrapped ? object.value() : object,
          othUnwrapped = othIsWrapped ? other.value() : other;

      stack || (stack = new Stack);
      return equalFunc(objUnwrapped, othUnwrapped, bitmask, customizer, stack);
    }
  }
  if (!isSameTag) {
    return false;
  }
  stack || (stack = new Stack);
  return equalObjects(object, other, bitmask, customizer, equalFunc, stack);
}

module.exports = baseIsEqualDeep;


/***/ }),
/* 772 */
/***/ (function(module, exports) {

/**
 * A specialized version of `_.some` for arrays without support for iteratee
 * shorthands.
 *
 * @private
 * @param {Array} [array] The array to iterate over.
 * @param {Function} predicate The function invoked per iteration.
 * @returns {boolean} Returns `true` if any element passes the predicate check,
 *  else `false`.
 */
function arraySome(array, predicate) {
  var index = -1,
      length = array == null ? 0 : array.length;

  while (++index < length) {
    if (predicate(array[index], index, array)) {
      return true;
    }
  }
  return false;
}

module.exports = arraySome;


/***/ }),
/* 773 */
/***/ (function(module, exports, __webpack_require__) {

var Symbol = __webpack_require__(20),
    Uint8Array = __webpack_require__(269),
    eq = __webpack_require__(54),
    equalArrays = __webpack_require__(297),
    mapToArray = __webpack_require__(774),
    setToArray = __webpack_require__(191);

/** Used to compose bitmasks for value comparisons. */
var COMPARE_PARTIAL_FLAG = 1,
    COMPARE_UNORDERED_FLAG = 2;

/** `Object#toString` result references. */
var boolTag = '[object Boolean]',
    dateTag = '[object Date]',
    errorTag = '[object Error]',
    mapTag = '[object Map]',
    numberTag = '[object Number]',
    regexpTag = '[object RegExp]',
    setTag = '[object Set]',
    stringTag = '[object String]',
    symbolTag = '[object Symbol]';

var arrayBufferTag = '[object ArrayBuffer]',
    dataViewTag = '[object DataView]';

/** Used to convert symbols to primitives and strings. */
var symbolProto = Symbol ? Symbol.prototype : undefined,
    symbolValueOf = symbolProto ? symbolProto.valueOf : undefined;

/**
 * A specialized version of `baseIsEqualDeep` for comparing objects of
 * the same `toStringTag`.
 *
 * **Note:** This function only supports comparing values with tags of
 * `Boolean`, `Date`, `Error`, `Number`, `RegExp`, or `String`.
 *
 * @private
 * @param {Object} object The object to compare.
 * @param {Object} other The other object to compare.
 * @param {string} tag The `toStringTag` of the objects to compare.
 * @param {number} bitmask The bitmask flags. See `baseIsEqual` for more details.
 * @param {Function} customizer The function to customize comparisons.
 * @param {Function} equalFunc The function to determine equivalents of values.
 * @param {Object} stack Tracks traversed `object` and `other` objects.
 * @returns {boolean} Returns `true` if the objects are equivalent, else `false`.
 */
function equalByTag(object, other, tag, bitmask, customizer, equalFunc, stack) {
  switch (tag) {
    case dataViewTag:
      if ((object.byteLength != other.byteLength) ||
          (object.byteOffset != other.byteOffset)) {
        return false;
      }
      object = object.buffer;
      other = other.buffer;

    case arrayBufferTag:
      if ((object.byteLength != other.byteLength) ||
          !equalFunc(new Uint8Array(object), new Uint8Array(other))) {
        return false;
      }
      return true;

    case boolTag:
    case dateTag:
    case numberTag:
      // Coerce booleans to `1` or `0` and dates to milliseconds.
      // Invalid dates are coerced to `NaN`.
      return eq(+object, +other);

    case errorTag:
      return object.name == other.name && object.message == other.message;

    case regexpTag:
    case stringTag:
      // Coerce regexes to strings and treat strings, primitives and objects,
      // as equal. See http://www.ecma-international.org/ecma-262/7.0/#sec-regexp.prototype.tostring
      // for more details.
      return object == (other + '');

    case mapTag:
      var convert = mapToArray;

    case setTag:
      var isPartial = bitmask & COMPARE_PARTIAL_FLAG;
      convert || (convert = setToArray);

      if (object.size != other.size && !isPartial) {
        return false;
      }
      // Assume cyclic values are equal.
      var stacked = stack.get(object);
      if (stacked) {
        return stacked == other;
      }
      bitmask |= COMPARE_UNORDERED_FLAG;

      // Recursively compare objects (susceptible to call stack limits).
      stack.set(object, other);
      var result = equalArrays(convert(object), convert(other), bitmask, customizer, equalFunc, stack);
      stack['delete'](object);
      return result;

    case symbolTag:
      if (symbolValueOf) {
        return symbolValueOf.call(object) == symbolValueOf.call(other);
      }
  }
  return false;
}

module.exports = equalByTag;


/***/ }),
/* 774 */
/***/ (function(module, exports) {

/**
 * Converts `map` to its key-value pairs.
 *
 * @private
 * @param {Object} map The map to convert.
 * @returns {Array} Returns the key-value pairs.
 */
function mapToArray(map) {
  var index = -1,
      result = Array(map.size);

  map.forEach(function(value, key) {
    result[++index] = [key, value];
  });
  return result;
}

module.exports = mapToArray;


/***/ }),
/* 775 */
/***/ (function(module, exports, __webpack_require__) {

var getAllKeys = __webpack_require__(266);

/** Used to compose bitmasks for value comparisons. */
var COMPARE_PARTIAL_FLAG = 1;

/** Used for built-in method references. */
var objectProto = Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * A specialized version of `baseIsEqualDeep` for objects with support for
 * partial deep comparisons.
 *
 * @private
 * @param {Object} object The object to compare.
 * @param {Object} other The other object to compare.
 * @param {number} bitmask The bitmask flags. See `baseIsEqual` for more details.
 * @param {Function} customizer The function to customize comparisons.
 * @param {Function} equalFunc The function to determine equivalents of values.
 * @param {Object} stack Tracks traversed `object` and `other` objects.
 * @returns {boolean} Returns `true` if the objects are equivalent, else `false`.
 */
function equalObjects(object, other, bitmask, customizer, equalFunc, stack) {
  var isPartial = bitmask & COMPARE_PARTIAL_FLAG,
      objProps = getAllKeys(object),
      objLength = objProps.length,
      othProps = getAllKeys(other),
      othLength = othProps.length;

  if (objLength != othLength && !isPartial) {
    return false;
  }
  var index = objLength;
  while (index--) {
    var key = objProps[index];
    if (!(isPartial ? key in other : hasOwnProperty.call(other, key))) {
      return false;
    }
  }
  // Assume cyclic values are equal.
  var stacked = stack.get(object);
  if (stacked && stack.get(other)) {
    return stacked == other;
  }
  var result = true;
  stack.set(object, other);
  stack.set(other, object);

  var skipCtor = isPartial;
  while (++index < objLength) {
    key = objProps[index];
    var objValue = object[key],
        othValue = other[key];

    if (customizer) {
      var compared = isPartial
        ? customizer(othValue, objValue, key, other, object, stack)
        : customizer(objValue, othValue, key, object, other, stack);
    }
    // Recursively compare objects (susceptible to call stack limits).
    if (!(compared === undefined
          ? (objValue === othValue || equalFunc(objValue, othValue, bitmask, customizer, stack))
          : compared
        )) {
      result = false;
      break;
    }
    skipCtor || (skipCtor = key == 'constructor');
  }
  if (result && !skipCtor) {
    var objCtor = object.constructor,
        othCtor = other.constructor;

    // Non `Object` object instances with different constructors are not equal.
    if (objCtor != othCtor &&
        ('constructor' in object && 'constructor' in other) &&
        !(typeof objCtor == 'function' && objCtor instanceof objCtor &&
          typeof othCtor == 'function' && othCtor instanceof othCtor)) {
      result = false;
    }
  }
  stack['delete'](object);
  stack['delete'](other);
  return result;
}

module.exports = equalObjects;


/***/ }),
/* 776 */
/***/ (function(module, exports, __webpack_require__) {

var isStrictComparable = __webpack_require__(298),
    keys = __webpack_require__(112);

/**
 * Gets the property names, values, and compare flags of `object`.
 *
 * @private
 * @param {Object} object The object to query.
 * @returns {Array} Returns the match data of `object`.
 */
function getMatchData(object) {
  var result = keys(object),
      length = result.length;

  while (length--) {
    var key = result[length],
        value = object[key];

    result[length] = [key, value, isStrictComparable(value)];
  }
  return result;
}

module.exports = getMatchData;


/***/ }),
/* 777 */
/***/ (function(module, exports, __webpack_require__) {

var baseIsEqual = __webpack_require__(296),
    get = __webpack_require__(88),
    hasIn = __webpack_require__(778),
    isKey = __webpack_require__(62),
    isStrictComparable = __webpack_require__(298),
    matchesStrictComparable = __webpack_require__(299),
    toKey = __webpack_require__(44);

/** Used to compose bitmasks for value comparisons. */
var COMPARE_PARTIAL_FLAG = 1,
    COMPARE_UNORDERED_FLAG = 2;

/**
 * The base implementation of `_.matchesProperty` which doesn't clone `srcValue`.
 *
 * @private
 * @param {string} path The path of the property to get.
 * @param {*} srcValue The value to match.
 * @returns {Function} Returns the new spec function.
 */
function baseMatchesProperty(path, srcValue) {
  if (isKey(path) && isStrictComparable(srcValue)) {
    return matchesStrictComparable(toKey(path), srcValue);
  }
  return function(object) {
    var objValue = get(object, path);
    return (objValue === undefined && objValue === srcValue)
      ? hasIn(object, path)
      : baseIsEqual(srcValue, objValue, COMPARE_PARTIAL_FLAG | COMPARE_UNORDERED_FLAG);
  };
}

module.exports = baseMatchesProperty;


/***/ }),
/* 778 */
/***/ (function(module, exports, __webpack_require__) {

var baseHasIn = __webpack_require__(779),
    hasPath = __webpack_require__(780);

/**
 * Checks if `path` is a direct or inherited property of `object`.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Object
 * @param {Object} object The object to query.
 * @param {Array|string} path The path to check.
 * @returns {boolean} Returns `true` if `path` exists, else `false`.
 * @example
 *
 * var object = _.create({ 'a': _.create({ 'b': 2 }) });
 *
 * _.hasIn(object, 'a');
 * // => true
 *
 * _.hasIn(object, 'a.b');
 * // => true
 *
 * _.hasIn(object, ['a', 'b']);
 * // => true
 *
 * _.hasIn(object, 'b');
 * // => false
 */
function hasIn(object, path) {
  return object != null && hasPath(object, path, baseHasIn);
}

module.exports = hasIn;


/***/ }),
/* 779 */
/***/ (function(module, exports) {

/**
 * The base implementation of `_.hasIn` without support for deep paths.
 *
 * @private
 * @param {Object} [object] The object to query.
 * @param {Array|string} key The key to check.
 * @returns {boolean} Returns `true` if `key` exists, else `false`.
 */
function baseHasIn(object, key) {
  return object != null && key in Object(object);
}

module.exports = baseHasIn;


/***/ }),
/* 780 */
/***/ (function(module, exports, __webpack_require__) {

var castPath = __webpack_require__(61),
    isArguments = __webpack_require__(113),
    isArray = __webpack_require__(16),
    isIndex = __webpack_require__(95),
    isLength = __webpack_require__(182),
    toKey = __webpack_require__(44);

/**
 * Checks if `path` exists on `object`.
 *
 * @private
 * @param {Object} object The object to query.
 * @param {Array|string} path The path to check.
 * @param {Function} hasFunc The function to check properties.
 * @returns {boolean} Returns `true` if `path` exists, else `false`.
 */
function hasPath(object, path, hasFunc) {
  path = castPath(path, object);

  var index = -1,
      length = path.length,
      result = false;

  while (++index < length) {
    var key = toKey(path[index]);
    if (!(result = object != null && hasFunc(object, key))) {
      break;
    }
    object = object[key];
  }
  if (result || ++index != length) {
    return result;
  }
  length = object == null ? 0 : object.length;
  return !!length && isLength(length) && isIndex(key, length) &&
    (isArray(object) || isArguments(object));
}

module.exports = hasPath;


/***/ }),
/* 781 */
/***/ (function(module, exports, __webpack_require__) {

var baseProperty = __webpack_require__(782),
    basePropertyDeep = __webpack_require__(783),
    isKey = __webpack_require__(62),
    toKey = __webpack_require__(44);

/**
 * Creates a function that returns the value at `path` of a given object.
 *
 * @static
 * @memberOf _
 * @since 2.4.0
 * @category Util
 * @param {Array|string} path The path of the property to get.
 * @returns {Function} Returns the new accessor function.
 * @example
 *
 * var objects = [
 *   { 'a': { 'b': 2 } },
 *   { 'a': { 'b': 1 } }
 * ];
 *
 * _.map(objects, _.property('a.b'));
 * // => [2, 1]
 *
 * _.map(_.sortBy(objects, _.property(['a', 'b'])), 'a.b');
 * // => [1, 2]
 */
function property(path) {
  return isKey(path) ? baseProperty(toKey(path)) : basePropertyDeep(path);
}

module.exports = property;


/***/ }),
/* 782 */
/***/ (function(module, exports) {

/**
 * The base implementation of `_.property` without support for deep paths.
 *
 * @private
 * @param {string} key The key of the property to get.
 * @returns {Function} Returns the new accessor function.
 */
function baseProperty(key) {
  return function(object) {
    return object == null ? undefined : object[key];
  };
}

module.exports = baseProperty;


/***/ }),
/* 783 */
/***/ (function(module, exports, __webpack_require__) {

var baseGet = __webpack_require__(89);

/**
 * A specialized version of `baseProperty` which supports deep paths.
 *
 * @private
 * @param {Array|string} path The path of the property to get.
 * @returns {Function} Returns the new accessor function.
 */
function basePropertyDeep(path) {
  return function(object) {
    return baseGet(object, path);
  };
}

module.exports = basePropertyDeep;


/***/ }),
/* 784 */
/***/ (function(module, exports, __webpack_require__) {

var toNumber = __webpack_require__(785);

/** Used as references for various `Number` constants. */
var INFINITY = 1 / 0,
    MAX_INTEGER = 1.7976931348623157e+308;

/**
 * Converts `value` to a finite number.
 *
 * @static
 * @memberOf _
 * @since 4.12.0
 * @category Lang
 * @param {*} value The value to convert.
 * @returns {number} Returns the converted number.
 * @example
 *
 * _.toFinite(3.2);
 * // => 3.2
 *
 * _.toFinite(Number.MIN_VALUE);
 * // => 5e-324
 *
 * _.toFinite(Infinity);
 * // => 1.7976931348623157e+308
 *
 * _.toFinite('3.2');
 * // => 3.2
 */
function toFinite(value) {
  if (!value) {
    return value === 0 ? value : 0;
  }
  value = toNumber(value);
  if (value === INFINITY || value === -INFINITY) {
    var sign = (value < 0 ? -1 : 1);
    return sign * MAX_INTEGER;
  }
  return value === value ? value : 0;
}

module.exports = toFinite;


/***/ }),
/* 785 */
/***/ (function(module, exports, __webpack_require__) {

var isObject = __webpack_require__(26),
    isSymbol = __webpack_require__(30);

/** Used as references for various `Number` constants. */
var NAN = 0 / 0;

/** Used to match leading and trailing whitespace. */
var reTrim = /^\s+|\s+$/g;

/** Used to detect bad signed hexadecimal string values. */
var reIsBadHex = /^[-+]0x[0-9a-f]+$/i;

/** Used to detect binary string values. */
var reIsBinary = /^0b[01]+$/i;

/** Used to detect octal string values. */
var reIsOctal = /^0o[0-7]+$/i;

/** Built-in method references without a dependency on `root`. */
var freeParseInt = parseInt;

/**
 * Converts `value` to a number.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to process.
 * @returns {number} Returns the number.
 * @example
 *
 * _.toNumber(3.2);
 * // => 3.2
 *
 * _.toNumber(Number.MIN_VALUE);
 * // => 5e-324
 *
 * _.toNumber(Infinity);
 * // => Infinity
 *
 * _.toNumber('3.2');
 * // => 3.2
 */
function toNumber(value) {
  if (typeof value == 'number') {
    return value;
  }
  if (isSymbol(value)) {
    return NAN;
  }
  if (isObject(value)) {
    var other = typeof value.valueOf == 'function' ? value.valueOf() : value;
    value = isObject(other) ? (other + '') : other;
  }
  if (typeof value != 'string') {
    return value === 0 ? value : +value;
  }
  value = value.replace(reTrim, '');
  var isBinary = reIsBinary.test(value);
  return (isBinary || reIsOctal.test(value))
    ? freeParseInt(value.slice(2), isBinary ? 2 : 8)
    : (reIsBadHex.test(value) ? NAN : +value);
}

module.exports = toNumber;


/***/ }),
/* 786 */
/***/ (function(module, exports, __webpack_require__) {

var baseFindIndex = __webpack_require__(189),
    baseIteratee = __webpack_require__(295),
    toInteger = __webpack_require__(300);

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeMax = Math.max,
    nativeMin = Math.min;

/**
 * This method is like `_.findIndex` except that it iterates over elements
 * of `collection` from right to left.
 *
 * @static
 * @memberOf _
 * @since 2.0.0
 * @category Array
 * @param {Array} array The array to inspect.
 * @param {Function} [predicate=_.identity] The function invoked per iteration.
 * @param {number} [fromIndex=array.length-1] The index to search from.
 * @returns {number} Returns the index of the found element, else `-1`.
 * @example
 *
 * var users = [
 *   { 'user': 'barney',  'active': true },
 *   { 'user': 'fred',    'active': false },
 *   { 'user': 'pebbles', 'active': false }
 * ];
 *
 * _.findLastIndex(users, function(o) { return o.user == 'pebbles'; });
 * // => 2
 *
 * // The `_.matches` iteratee shorthand.
 * _.findLastIndex(users, { 'user': 'barney', 'active': true });
 * // => 0
 *
 * // The `_.matchesProperty` iteratee shorthand.
 * _.findLastIndex(users, ['active', false]);
 * // => 2
 *
 * // The `_.property` iteratee shorthand.
 * _.findLastIndex(users, 'active');
 * // => 0
 */
function findLastIndex(array, predicate, fromIndex) {
  var length = array == null ? 0 : array.length;
  if (!length) {
    return -1;
  }
  var index = length - 1;
  if (fromIndex !== undefined) {
    index = toInteger(fromIndex);
    index = fromIndex < 0
      ? nativeMax(length + index, 0)
      : nativeMin(index, length - 1);
  }
  return baseFindIndex(array, baseIteratee(predicate, 3), index, true);
}

module.exports = findLastIndex;


/***/ }),
/* 787 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; }; /* This Source Code Form is subject to the terms of the Mozilla Public
                                                                                                                                                                                                                                                                   * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                                                                                                                                                                                                                                   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

exports.getNextStep = getNextStep;

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

var _types2 = __webpack_require__(788);

var _closest = __webpack_require__(255);

var _helpers = __webpack_require__(192);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function getNextStep(sourceId, pausedPosition) {
  const currentExpression = getSteppableExpression(sourceId, pausedPosition);
  if (!currentExpression) {
    return null;
  }

  const currentStatement = currentExpression.find(p => {
    return p.inList && t.isStatement(p.node);
  });

  if (!currentStatement) {
    throw new Error("Assertion failure - this should always find at least Program");
  }

  return _getNextStep(currentStatement, pausedPosition);
}

function getSteppableExpression(sourceId, pausedPosition) {
  const closestPath = (0, _closest.getClosestPath)(sourceId, pausedPosition);

  if (!closestPath) {
    return null;
  }

  if ((0, _helpers.isAwaitExpression)(closestPath) || (0, _helpers.isYieldExpression)(closestPath)) {
    return closestPath;
  }

  return closestPath.find(p => t.isAwaitExpression(p.node) || t.isYieldExpression(p.node));
}

function _getNextStep(statement, position) {
  const nextStatement = statement.getSibling(1);
  if (nextStatement) {
    return _extends({}, nextStatement.node.loc.start, {
      sourceId: position.sourceId
    });
  }

  return null;
}

/***/ }),
/* 788 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


/***/ }),
/* 789 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = getEmptyLines;

var _uniq = __webpack_require__(276);

var _uniq2 = _interopRequireDefault(_uniq);

var _difference = __webpack_require__(790);

var _difference2 = _interopRequireDefault(_difference);

var _ast = __webpack_require__(51);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const commentTokens = ["CommentBlock", "CommentLine"]; /* This Source Code Form is subject to the terms of the Mozilla Public
                                                        * License, v. 2.0. If a copy of the MPL was not distributed with this
                                                        * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function fillRange(start, end) {
  return Array(end - start + 1).fill().map((item, index) => start + index);
}

// Populates a pre-filled array of every line number,
// then removes lines which were found to be executable
function getLines(ast) {
  return fillRange(1, ast.tokens[ast.tokens.length - 1].loc.end.line);
}

// The following sequence stores lines which have executable code
// (contents other than comments or EOF, regardless of line position)
function getExecutableLines(ast) {
  const lines = ast.tokens.filter(token => !commentTokens.includes(token.type) && (!token.type || token.type.label && token.type.label != "eof")).map(token => token.loc.start.line);

  return (0, _uniq2.default)(lines);
}

function getEmptyLines(sourceId) {
  if (!sourceId) {
    return null;
  }

  const ast = (0, _ast.getAst)(sourceId);
  if (!ast || !ast.comments) {
    return [];
  }

  const executableLines = getExecutableLines(ast);
  const lines = getLines(ast);
  return (0, _difference2.default)(lines, executableLines);
}

/***/ }),
/* 790 */
/***/ (function(module, exports, __webpack_require__) {

var baseDifference = __webpack_require__(791),
    baseFlatten = __webpack_require__(293),
    baseRest = __webpack_require__(792),
    isArrayLikeObject = __webpack_require__(799);

/**
 * Creates an array of `array` values not included in the other given arrays
 * using [`SameValueZero`](http://ecma-international.org/ecma-262/7.0/#sec-samevaluezero)
 * for equality comparisons. The order and references of result values are
 * determined by the first array.
 *
 * **Note:** Unlike `_.pullAll`, this method returns a new array.
 *
 * @static
 * @memberOf _
 * @since 0.1.0
 * @category Array
 * @param {Array} array The array to inspect.
 * @param {...Array} [values] The values to exclude.
 * @returns {Array} Returns the new array of filtered values.
 * @see _.without, _.xor
 * @example
 *
 * _.difference([2, 1], [2, 3]);
 * // => [1]
 */
var difference = baseRest(function(array, values) {
  return isArrayLikeObject(array)
    ? baseDifference(array, baseFlatten(values, 1, isArrayLikeObject, true))
    : [];
});

module.exports = difference;


/***/ }),
/* 791 */
/***/ (function(module, exports, __webpack_require__) {

var SetCache = __webpack_require__(188),
    arrayIncludes = __webpack_require__(277),
    arrayIncludesWith = __webpack_require__(278),
    arrayMap = __webpack_require__(56),
    baseUnary = __webpack_require__(84),
    cacheHas = __webpack_require__(190);

/** Used as the size to enable large array optimizations. */
var LARGE_ARRAY_SIZE = 200;

/**
 * The base implementation of methods like `_.difference` without support
 * for excluding multiple arrays or iteratee shorthands.
 *
 * @private
 * @param {Array} array The array to inspect.
 * @param {Array} values The values to exclude.
 * @param {Function} [iteratee] The iteratee invoked per element.
 * @param {Function} [comparator] The comparator invoked per element.
 * @returns {Array} Returns the new array of filtered values.
 */
function baseDifference(array, values, iteratee, comparator) {
  var index = -1,
      includes = arrayIncludes,
      isCommon = true,
      length = array.length,
      result = [],
      valuesLength = values.length;

  if (!length) {
    return result;
  }
  if (iteratee) {
    values = arrayMap(values, baseUnary(iteratee));
  }
  if (comparator) {
    includes = arrayIncludesWith;
    isCommon = false;
  }
  else if (values.length >= LARGE_ARRAY_SIZE) {
    includes = cacheHas;
    isCommon = false;
    values = new SetCache(values);
  }
  outer:
  while (++index < length) {
    var value = array[index],
        computed = iteratee == null ? value : iteratee(value);

    value = (comparator || value !== 0) ? value : 0;
    if (isCommon && computed === computed) {
      var valuesIndex = valuesLength;
      while (valuesIndex--) {
        if (values[valuesIndex] === computed) {
          continue outer;
        }
      }
      result.push(value);
    }
    else if (!includes(values, computed, comparator)) {
      result.push(value);
    }
  }
  return result;
}

module.exports = baseDifference;


/***/ }),
/* 792 */
/***/ (function(module, exports, __webpack_require__) {

var identity = __webpack_require__(194),
    overRest = __webpack_require__(793),
    setToString = __webpack_require__(795);

/**
 * The base implementation of `_.rest` which doesn't validate or coerce arguments.
 *
 * @private
 * @param {Function} func The function to apply a rest parameter to.
 * @param {number} [start=func.length-1] The start position of the rest parameter.
 * @returns {Function} Returns the new function.
 */
function baseRest(func, start) {
  return setToString(overRest(func, start, identity), func + '');
}

module.exports = baseRest;


/***/ }),
/* 793 */
/***/ (function(module, exports, __webpack_require__) {

var apply = __webpack_require__(794);

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeMax = Math.max;

/**
 * A specialized version of `baseRest` which transforms the rest array.
 *
 * @private
 * @param {Function} func The function to apply a rest parameter to.
 * @param {number} [start=func.length-1] The start position of the rest parameter.
 * @param {Function} transform The rest array transform.
 * @returns {Function} Returns the new function.
 */
function overRest(func, start, transform) {
  start = nativeMax(start === undefined ? (func.length - 1) : start, 0);
  return function() {
    var args = arguments,
        index = -1,
        length = nativeMax(args.length - start, 0),
        array = Array(length);

    while (++index < length) {
      array[index] = args[start + index];
    }
    index = -1;
    var otherArgs = Array(start + 1);
    while (++index < start) {
      otherArgs[index] = args[index];
    }
    otherArgs[start] = transform(array);
    return apply(func, this, otherArgs);
  };
}

module.exports = overRest;


/***/ }),
/* 794 */
/***/ (function(module, exports) {

/**
 * A faster alternative to `Function#apply`, this function invokes `func`
 * with the `this` binding of `thisArg` and the arguments of `args`.
 *
 * @private
 * @param {Function} func The function to invoke.
 * @param {*} thisArg The `this` binding of `func`.
 * @param {Array} args The arguments to invoke `func` with.
 * @returns {*} Returns the result of `func`.
 */
function apply(func, thisArg, args) {
  switch (args.length) {
    case 0: return func.call(thisArg);
    case 1: return func.call(thisArg, args[0]);
    case 2: return func.call(thisArg, args[0], args[1]);
    case 3: return func.call(thisArg, args[0], args[1], args[2]);
  }
  return func.apply(thisArg, args);
}

module.exports = apply;


/***/ }),
/* 795 */
/***/ (function(module, exports, __webpack_require__) {

var baseSetToString = __webpack_require__(796),
    shortOut = __webpack_require__(798);

/**
 * Sets the `toString` method of `func` to return `string`.
 *
 * @private
 * @param {Function} func The function to modify.
 * @param {Function} string The `toString` result.
 * @returns {Function} Returns `func`.
 */
var setToString = shortOut(baseSetToString);

module.exports = setToString;


/***/ }),
/* 796 */
/***/ (function(module, exports, __webpack_require__) {

var constant = __webpack_require__(797),
    defineProperty = __webpack_require__(94),
    identity = __webpack_require__(194);

/**
 * The base implementation of `setToString` without support for hot loop shorting.
 *
 * @private
 * @param {Function} func The function to modify.
 * @param {Function} string The `toString` result.
 * @returns {Function} Returns `func`.
 */
var baseSetToString = !defineProperty ? identity : function(func, string) {
  return defineProperty(func, 'toString', {
    'configurable': true,
    'enumerable': false,
    'value': constant(string),
    'writable': true
  });
};

module.exports = baseSetToString;


/***/ }),
/* 797 */
/***/ (function(module, exports) {

/**
 * Creates a function that returns `value`.
 *
 * @static
 * @memberOf _
 * @since 2.4.0
 * @category Util
 * @param {*} value The value to return from the new function.
 * @returns {Function} Returns the new constant function.
 * @example
 *
 * var objects = _.times(2, _.constant({ 'a': 1 }));
 *
 * console.log(objects);
 * // => [{ 'a': 1 }, { 'a': 1 }]
 *
 * console.log(objects[0] === objects[1]);
 * // => true
 */
function constant(value) {
  return function() {
    return value;
  };
}

module.exports = constant;


/***/ }),
/* 798 */
/***/ (function(module, exports) {

/** Used to detect hot functions by number of calls within a span of milliseconds. */
var HOT_COUNT = 800,
    HOT_SPAN = 16;

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeNow = Date.now;

/**
 * Creates a function that'll short out and invoke `identity` instead
 * of `func` when it's called `HOT_COUNT` or more times in `HOT_SPAN`
 * milliseconds.
 *
 * @private
 * @param {Function} func The function to restrict.
 * @returns {Function} Returns the new shortable function.
 */
function shortOut(func) {
  var count = 0,
      lastCalled = 0;

  return function() {
    var stamp = nativeNow(),
        remaining = HOT_SPAN - (stamp - lastCalled);

    lastCalled = stamp;
    if (remaining > 0) {
      if (++count >= HOT_COUNT) {
        return arguments[0];
      }
    } else {
      count = 0;
    }
    return func.apply(undefined, arguments);
  };
}

module.exports = shortOut;


/***/ }),
/* 799 */
/***/ (function(module, exports, __webpack_require__) {

var isArrayLike = __webpack_require__(117),
    isObjectLike = __webpack_require__(21);

/**
 * This method is like `_.isArrayLike` except that it also checks if `value`
 * is an object.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Lang
 * @param {*} value The value to check.
 * @returns {boolean} Returns `true` if `value` is an array-like object,
 *  else `false`.
 * @example
 *
 * _.isArrayLikeObject([1, 2, 3]);
 * // => true
 *
 * _.isArrayLikeObject(document.body.children);
 * // => true
 *
 * _.isArrayLikeObject('abc');
 * // => false
 *
 * _.isArrayLikeObject(_.noop);
 * // => false
 */
function isArrayLikeObject(value) {
  return isObjectLike(value) && isArrayLike(value);
}

module.exports = isArrayLikeObject;


/***/ }),
/* 800 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.hasSyntaxError = hasSyntaxError;

var _ast = __webpack_require__(51);

function hasSyntaxError(input) {
  try {
    (0, _ast.parseScript)(input);
    return false;
  } catch (e) {
    return `${e.name} : ${e.message}`;
  }
} /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/***/ }),
/* 801 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getFramework = getFramework;

var _getSymbols = __webpack_require__(193);

var _getSymbols2 = _interopRequireDefault(_getSymbols);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function getFramework(sourceId) {
  if (isReactComponent(sourceId)) {
    return "React";
  }

  if (isAngularComponent(sourceId)) {
    return "Angular";
  }
}

// React

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function isReactComponent(sourceId) {
  const { imports, classes, callExpressions } = (0, _getSymbols2.default)(sourceId);
  return (importsReact(imports) || requiresReact(callExpressions)) && extendsReactComponent(classes);
}

function importsReact(imports) {
  return imports.some(importObj => importObj.source === "react" && importObj.specifiers.some(specifier => specifier === "React"));
}

function requiresReact(callExpressions) {
  return callExpressions.some(callExpression => callExpression.name === "require" && callExpression.values.some(value => value === "react"));
}

function extendsReactComponent(classes) {
  let result = false;
  classes.some(classObj => {
    if (classObj.parent.name === "Component" || classObj.parent.name === "PureComponent" || classObj.parent.property.name === "Component") {
      result = true;
    }
  });

  return result;
}

// Angular

const isAngularComponent = sourceId => {
  const { memberExpressions, identifiers } = (0, _getSymbols2.default)(sourceId);
  return identifiesAngular(identifiers) && hasAngularExpressions(memberExpressions);
};

const identifiesAngular = identifiers => {
  return identifiers.some(item => item.name == "angular");
};

const hasAngularExpressions = memberExpressions => {
  return memberExpressions.some(item => item.name == "controller" || item.name == "module");
};

/***/ }),
/* 802 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isInvalidPauseLocation = isInvalidPauseLocation;

var _types = __webpack_require__(28);

var t = _interopRequireWildcard(_types);

var _ast = __webpack_require__(51);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const STOP = {};

function isInvalidPauseLocation(location) {
  const state = {
    invalid: false,
    location
  };

  try {
    (0, _ast.traverseAst)(location.sourceId, { enter: invalidLocationVisitor }, state);
  } catch (e) {
    if (e !== STOP) {
      throw e;
    }
  }

  return state.invalid;
}

function invalidLocationVisitor(node, ancestors, state) {
  const { location } = state;

  if (node.loc.end.line < location.line) {
    return;
  }
  if (node.loc.start.line > location.line) {
    throw STOP;
  }

  if (location.line === node.loc.start.line && location.column >= node.loc.start.column && t.isFunction(node) && !t.isArrowFunctionExpression(node) && (location.line < node.body.loc.start.line || location.line === node.body.loc.start.line && location.column <= node.body.loc.start.column)) {
    // Disallow pausing _inside_ in function arguments to avoid pausing inside
    // of destructuring and other logic.
    state.invalid = true;
    throw STOP;
  }

  if (location.line === node.loc.start.line && location.column === node.loc.start.column && t.isBlockStatement(node)) {
    // Disallow pausing directly before the opening curly of a block statement.
    // Babel occasionally maps statements with unknown original positions to
    // this location.
    state.invalid = true;
    throw STOP;
  }
}

/***/ })
/******/ ]);
});