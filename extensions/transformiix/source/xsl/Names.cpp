/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * XSL names used throughout the XSLProcessor.
 * Probably should be wrapped in a Namespace
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
#include "Names.h"

//-- Global Strings
const String HTML              = "html";
const String HTML_NS           = "http://www.w3.org/TR/REC-html";
const String STYLESHEET_PI     = "xml-stylesheet";
const String STYLESHEET_PI_OLD = "xml:stylesheet";
const String XML_SPACE         = "xml:space";
const String XSL_MIME_TYPE     = "text/xsl";
const String XSLT_NS           = "http://www.w3.org/XSL/Transform/";

//-- Elements
const String APPLY_IMPORTS      = "apply-imports";
const String APPLY_TEMPLATES    = "apply-templates";
const String ATTRIBUTE          = "attribute";
const String ATTRIBUTE_SET      = "attribute-set";
const String CALL_TEMPLATE      = "call-template";
const String CHOOSE             = "choose";
const String COMMENT            = "comment";
const String COPY               = "copy";
const String COPY_OF            = "copy-of";
const String ELEMENT            = "element";
const String FOR_EACH           = "for-each";
const String IF                 = "if";
const String MESSAGE            = "message";
const String NUMBER             = "number";
const String OTHERWISE          = "otherwise";
const String PI                 = "processing-instruction";
const String PRESERVE_SPACE     = "preserve-space";
const String STRIP_SPACE        = "strip-space";
const String TEMPLATE           = "template";
const String TEXT               = "text";
const String VALUE_OF           = "value-of";
const String VARIABLE           = "variable";
const String WHEN               = "when";


//-- Attributes
const String COUNT_ATTR              = "count";
const String DEFAULT_SPACE_ATTR      = "default-space";
const String ELEMENTS_ATTR           = "elements";
const String EXPR_ATTR               = "expr";
const String FORMAT_ATTR             = "format";
const String MATCH_ATTR              = "match";
const String MODE_ATTR               = "mode";
const String NAME_ATTR               = "name";
const String NAMESPACE_ATTR          = "namespace";
const String PRIORITY_ATTR           = "priority";
const String SELECT_ATTR             = "select";
const String TEST_ATTR               = "test";
const String USE_ATTRIBUTE_SETS_ATTR = "use-attribute-sets";

//-- Attribute Values
const String STRIP_VALUE          = "strip";
const String PRESERVE_VALUE       = "preserve";
const String YES_VALUE            = "yes";
const String NO_VALUE             = "no";

//-- Stylesheet attributes
const String INDENT_RESULT_ATTR   = "indent-result";
const String RESULT_NS_ATTR       = "result-ns";

const String ANCESTOR_AXIS            =  "ancestor";
const String ANCESTOR_OR_SELF_AXIS    =  "ancestor-or-self";
const String ATTRIBUTE_AXIS           =  "attribute";
const String CHILD_AXIS               =  "child";
const String DESCENDANT_AXIS          =  "descendant";
const String DESCENDANT_OR_SELF_AXIS  =  "descendant-or-self";
const String FOLLOWING_AXIS           =  "following";
const String FOLLOWING_SIBLING_AXIS   =  "following-siblings";
const String NAMESPACE_AXIS           =  "namespace";
const String PARENT_AXIS              =  "parent";
const String PRECEDING_AXIS           =  "preceding";
const String PRECEDING_SIBLING_AXIS   =  "preceding-siblings";
const String SELF_AXIS                =  "self";


//-- NodeTest Operators
const String ATTRIBUTE_FNAME         = "@";
const String COMMENT_FNAME           = "comment";
const String PI_FNAME                = "pi";
const String TEXT_FNAME              = "text";
const String NODE_FNAME              = "node";
const String IDENTITY_OP             = ".";
const String PARENT_OP               = "..";

//-- Function Names
const String BOOLEAN_FN              = "boolean";
const String CONCAT_FN               = "concat";
const String CONTAINS_FN             = "contains";
const String COUNT_FN                = "count";
const String FALSE_FN                = "false";
const String LAST_FN                 = "last";
const String LOCAL_PART_FN           = "local-part";
const String NAME_FN                 = "name";
const String NAMESPACE_FN            = "namespace";
const String NOT_FN                  = "not";
const String POSITION_FN             = "position";
const String STARTS_WITH_FN          = "starts-with";
const String STRING_FN               = "string";
const String STRING_LENGTH_FN        = "string-length";
const String SUBSTRING_FN            = "substring";
const String SUBSTRING_AFTER_FN      = "substring-after";
const String SUBSTRING_BEFORE_FN     = "substring-before";
const String TRANSLATE_FN            = "translate";
const String TRUE_FN                 = "true";

//-- internal XSL processor functions
const String ERROR_FN                = "error";

const String WILD_CARD               = "*";

