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
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef TRANSFRMX_NAMES_H
#define TRANSFRMX_NAMES_H

#include "TxString.h"

//-- Global Strings
extern const String HTML;
extern const String HTML_NS;
extern const String STYLESHEET_PI;
extern const String STYLESHEET_PI_OLD;
extern const String XML_SPACE;
extern const String XSL_MIME_TYPE;
extern const String XSLT_NS;

//-- Elements
extern const String APPLY_IMPORTS;
extern const String APPLY_TEMPLATES;
extern const String ATTRIBUTE;
extern const String ATTRIBUTE_SET;
extern const String CALL_TEMPLATE;
extern const String CHOOSE;
extern const String COMMENT;
extern const String COPY;
extern const String COPY_OF;
extern const String DECIMAL_FORMAT;
extern const String ELEMENT;
extern const String FOR_EACH;
extern const String IF;
extern const String IMPORT;
extern const String INCLUDE;
extern const String KEY;
extern const String MESSAGE;
extern const String NUMBER;
extern const String OTHERWISE;
extern const String OUTPUT;
extern const String PARAM;
extern const String PROC_INST;
extern const String PRESERVE_SPACE;
extern const String STRIP_SPACE;
extern const String SORT;
extern const String TEMPLATE;
extern const String TEXT;
extern const String VALUE_OF;
extern const String VARIABLE;
extern const String WHEN;
extern const String WITH_PARAM;


//-- Attributes
extern const String CASE_ORDER_ATTR;
extern const String CDATA_ELEMENTS;
extern const String COUNT_ATTR;
extern const String DATA_TYPE_ATTR;
extern const String DECIMAL_SEPARATOR_ATTR;
extern const String DEFAULT_SPACE_ATTR;
extern const String DIGIT_ATTR;
extern const String DOCTYPE_PUBLIC_ATTR;
extern const String DOCTYPE_SYSTEM_ATTR;
extern const String ELEMENTS_ATTR;
extern const String ENCODING_ATTR;
extern const String EXPR_ATTR;
extern const String FORMAT_ATTR;
extern const String FROM_ATTR;
extern const String GROUPING_SEPARATOR_ATTR;
extern const String HREF_ATTR;
extern const String INDENT_ATTR;
extern const String INFINITY_ATTR;
extern const String LANG_ATTR;
extern const String LEVEL_ATTR;
extern const String MATCH_ATTR;
extern const String MEDIA_TYPE_ATTR;
extern const String METHOD_ATTR;
extern const String MINUS_SIGN_ATTR;
extern const String MODE_ATTR;
extern const String NAME_ATTR;
extern const String NAMESPACE_ATTR;
extern const String NAN_ATTR;
extern const String OMIT_XMLDECL_ATTR;
extern const String ORDER_ATTR;
extern const String PATTERN_SEPARATOR_ATTR;
extern const String PER_MILLE_ATTR;
extern const String PERCENT_ATTR;
extern const String PRIORITY_ATTR;
extern const String SELECT_ATTR;
extern const String STANDALONE;
extern const String TEST_ATTR;
extern const String USE_ATTR;
extern const String USE_ATTRIBUTE_SETS_ATTR;
extern const String VALUE_ATTR;
extern const String VERSION_ATTR;
extern const String XML_LANG_ATTR;
extern const String ZERO_DIGIT_ATTR;

//-- Attribute Values
extern const String ANY_VALUE;
extern const String ASCENDING_VALUE;
extern const String DESCENDING_VALUE;
extern const String LOWER_FIRST_VALUE;
extern const String MULTIPLE_VALUE;
extern const String NO_VALUE;
extern const String NUMBER_VALUE;
extern const String PRESERVE_VALUE;
extern const String SINGLE_VALUE;
extern const String STRIP_VALUE;
extern const String TEXT_VALUE;
extern const String UPPER_FIRST_VALUE;
extern const String YES_VALUE;

//-- Stylesheet attributes
extern const String INDENT_RESULT_ATTR;
extern const String RESULT_NS_ATTR;

extern const String ANCESTOR_AXIS;
extern const String ANCESTOR_OR_SELF_AXIS;
extern const String ATTRIBUTE_AXIS;
extern const String CHILD_AXIS;
extern const String DESCENDANT_AXIS;
extern const String DESCENDANT_OR_SELF_AXIS;
extern const String FOLLOWING_AXIS;
extern const String FOLLOWING_SIBLING_AXIS;
extern const String NAMESPACE_AXIS;
extern const String PARENT_AXIS;
extern const String PRECEDING_AXIS;
extern const String PRECEDING_SIBLING_AXIS;
extern const String SELF_AXIS;


//-- NodeTest Operators
extern const String ATTRIBUTE_FNAME;
extern const String COMMENT_FNAME;
extern const String PI_FNAME;
extern const String TEXT_FNAME;
extern const String NODE_FNAME;
extern const String IDENTITY_OP;
extern const String PARENT_OP;

//-- XSLT additional functions
extern const String DOCUMENT_FN;
extern const String KEY_FN;
extern const String FORMAT_NUMBER_FN;
extern const String CURRENT_FN;
extern const String UNPARSED_ENTITY_URI_FN;
extern const String GENERATE_ID_FN;
extern const String SYSTEM_PROPERTY_FN;
extern const String ELEMENT_AVAILABLE_FN;
extern const String FUNCTION_AVAILABLE_FN;

//-- MISC
extern const String WILD_CARD;

#endif
