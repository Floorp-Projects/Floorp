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

#include "String.h"

#ifndef MITREXSL_NAMES_H
#define MITREXSL_NAMES_H

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
extern const String ELEMENT;
extern const String FOR_EACH;
extern const String IF;
extern const String MESSAGE;
extern const String NUMBER;
extern const String OTHERWISE;
extern const String PI;
extern const String PRESERVE_SPACE;
extern const String STRIP_SPACE;
extern const String TEMPLATE;
extern const String TEXT;
extern const String VALUE_OF;
extern const String VARIABLE;
extern const String WHEN;


//-- Attributes
extern const String COUNT_ATTR;
extern const String DEFAULT_SPACE_ATTR;
extern const String ELEMENTS_ATTR;
extern const String EXPR_ATTR;
extern const String FORMAT_ATTR;
extern const String MATCH_ATTR;
extern const String MODE_ATTR;
extern const String NAME_ATTR;
extern const String NAMESPACE_ATTR;
extern const String PRIORITY_ATTR;
extern const String SELECT_ATTR;
extern const String TEST_ATTR;
extern const String USE_ATTRIBUTE_SETS_ATTR;

//-- Attribute Values
extern const String STRIP_VALUE;
extern const String PRESERVE_VALUE;
extern const String YES_VALUE;
extern const String NO_VALUE;

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

//-- Function Names
extern const String BOOLEAN_FN;
extern const String CONCAT_FN;
extern const String CONTAINS_FN;
extern const String COUNT_FN ;
extern const String FALSE_FN;
extern const String LAST_FN;
extern const String LOCAL_PART_FN;
extern const String NAME_FN;
extern const String NAMESPACE_FN;
extern const String NOT_FN;
extern const String POSITION_FN;
extern const String STARTS_WITH_FN;
extern const String STRING_FN;
extern const String STRING_LENGTH_FN;
extern const String SUBSTRING_FN;
extern const String SUBSTRING_AFTER_FN;
extern const String SUBSTRING_BEFORE_FN;
extern const String TRANSLATE_FN;
extern const String TRUE_FN;

//-- internal XSL processor functions
extern const String ERROR_FN;

extern const String WILD_CARD;

#endif
