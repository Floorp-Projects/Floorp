/*
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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999, 2000 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *    -- Removed the trailing "s" from FOLLOWING_SIBLING_AXIS, and
 *       PRECEDING_SIBLING_AXIS to be compatible with the
 *       W3C XPath 1.0 Recommendation
 *    -- Added lang attr declaration
 *
 */

/**
 * XSL names used throughout the XSLProcessor.
 * Probably should be wrapped in a Namespace
**/
#include "Names.h"

//-- Global Strings
const String HTML              = "html";
const String HTML_NS           = "http://www.w3.org/1999/xhtml";
const String STYLESHEET_PI     = "xml-stylesheet";
const String STYLESHEET_PI_OLD = "xml:stylesheet";
const String XML_SPACE         = "xml:space";
const String XSL_MIME_TYPE     = "text/xsl";
const String XSLT_NS           = "http://www.w3.org/1999/XSL/Transform";

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
const String DECIMAL_FORMAT     = "decimal-format";
const String ELEMENT            = "element";
const String FOR_EACH           = "for-each";
const String IF                 = "if";
const String IMPORT             = "import";
const String INCLUDE            = "include";
const String KEY                = "key";
const String MESSAGE            = "message";
const String NUMBER             = "number";
const String OTHERWISE          = "otherwise";
const String OUTPUT             = "output";
const String PARAM              = "param";
const String PROC_INST          = "processing-instruction";
const String PRESERVE_SPACE     = "preserve-space";
const String SORT               = "sort";
const String STRIP_SPACE        = "strip-space";
const String TEMPLATE           = "template";
const String TEXT               = "text";
const String VALUE_OF           = "value-of";
const String VARIABLE           = "variable";
const String WHEN               = "when";
const String WITH_PARAM         = "with-param";

//-- Attributes
const String ELEMENTS_ATTR           = "elements";
const String HREF_ATTR               = "href";
const String MATCH_ATTR              = "match";
const String MODE_ATTR               = "mode";
const String NAME_ATTR               = "name";
const String SELECT_ATTR             = "select";
const String TEST_ATTR               = "test";
const String USE_ATTR                = "use";

//-- Attribute Values
const String ANY_VALUE            = "any";
const String ASCENDING_VALUE      = "ascending";
const String DESCENDING_VALUE     = "descending";
const String LOWER_FIRST_VALUE    = "lower-first";
const String MULTIPLE_VALUE       = "multiple";
const String NO_VALUE             = "no";
const String NUMBER_VALUE         = "number";
const String PRESERVE_VALUE       = "preserve";
const String SINGLE_VALUE         = "single";
const String STRIP_VALUE          = "strip";
const String TEXT_VALUE           = "text";
const String UPPER_FIRST_VALUE    = "upper-first";
const String YES_VALUE            = "yes";

//-- Stylesheet attributes
const String ANCESTOR_AXIS            =  "ancestor";
const String ANCESTOR_OR_SELF_AXIS    =  "ancestor-or-self";
const String ATTRIBUTE_AXIS           =  "attribute";
const String CHILD_AXIS               =  "child";
const String DESCENDANT_AXIS          =  "descendant";
const String DESCENDANT_OR_SELF_AXIS  =  "descendant-or-self";
const String FOLLOWING_AXIS           =  "following";
const String FOLLOWING_SIBLING_AXIS   =  "following-sibling";
const String NAMESPACE_AXIS           =  "namespace";
const String PARENT_AXIS              =  "parent";
const String PRECEDING_AXIS           =  "preceding";
const String PRECEDING_SIBLING_AXIS   =  "preceding-sibling";
const String SELF_AXIS                =  "self";

//-- NodeTest Operators
const String ATTRIBUTE_FNAME         = "@";
const String COMMENT_FNAME           = "comment";
const String PI_FNAME                = "processing-instruction";
const String TEXT_FNAME              = "text";
const String NODE_FNAME              = "node";
const String IDENTITY_OP             = ".";
const String PARENT_OP               = "..";

//-- XSLT additional functions
const String DOCUMENT_FN             = "document";
const String KEY_FN                  = "key";
const String FORMAT_NUMBER_FN        = "format-number";
const String CURRENT_FN              = "current";
const String UNPARSED_ENTITY_URI_FN  = "unparsed-entity-uri";
const String GENERATE_ID_FN          = "generate-id";
const String SYSTEM_PROPERTY_FN      = "system-property";
const String ELEMENT_AVAILABLE_FN    = "element-available";
const String FUNCTION_AVAILABLE_FN   = "function-available";

//-- MISC
const String WILD_CARD               = "*";

