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

/* CSStoJS tranlation code */

#include "xp.h"
#include "css.h"
#include "cssI.h"

/* ALLOW_IMPORT
 * Switch allows or stops the translation of @import rules.
 */
#define ALLOW_IMPORT 0

/* ALLOW_FONTDEF
 * Switch allows or stops the translation of @fontdef rules.
 */
#define ALLOW_FONTDEF 1

/* ALLOW_PRIORITY
 * Switch allows or stops the translation of "!important" in the value.
 */
#define ALLOW_PRIORITY 0

/* ALLOW_PSEUDO_ELEMENTS
 * Switch allows or prevents the translation of rules with pseudo-elements
 * :first-line and :first-letter in the selector.
 */
#define ALLOW_PSEUDO_ELEMENTS 0

/* ALLOW_ACTIVE_PSEUDO_CLASS
 * Switch allows or prevents the translation of rules with pseudo-class
 * :active in the selector.
 */
#define ALLOW_ACTIVE_PSEUDO_CLASS 0

#define STYLE_INITIAL_BUFSIZ 1024
#define STYLE_INCR_BUFSIZ    1024

/* property type values */
#define CSS_PROPERTY_TYPE_ASSIGNMENT (1)
#define CSS_PROPERTY_TYPE_CALL       (2)
/* pseudo class type values */
#define CSS_PSEUDO_CLASS_NONE		(0)
#define CSS_PSEUDO_CLASS_LINK		(1)
#define CSS_PSEUDO_CLASS_ACTIVE		(2)
#define CSS_PSEUDO_CLASS_VISITED	(3)

typedef struct {
    char * buffer;
    int32  buffer_size;   /* size of buffer in bytes */
    int32  buffer_count;  /* bytes used */
    int32  prior_count;   /* buffer_count at beginning of rule */
    int32  pseudo_class_state; /* holds temporary state during translation */
    XP_Bool ignore_rule;  /* during translation, may decide to ignore a rule */
} *StyleBuffer, StyleBufferRec;

XP_BEGIN_PROTOS

static XP_Bool ConvertStyleSheet(css_node stylesheet, StyleBuffer sb);
static void ConvertRule(css_node rule, StyleBuffer sb);
static void ConvertSelector(css_node selector, css_node pseudo_element,
                            StyleBuffer sb);
static void ConvertContextualSelector(css_node selector, css_node pseudo_element,
                                      StyleBuffer sb);
static void ConvertSingleSelector(css_node simple, css_node pseudo_element,
                                  StyleBuffer sb);
static void ConvertDeclaration(css_node declaration_list, StyleBuffer sb);
static void ConvertProperty(const char * css1_property, StyleBuffer sb,
                            int * property_type_return);
static void ConvertValue(css_node value, StyleBuffer sb, int property_type);
static void ConvertURLValue(char * url_value, StyleBuffer sb);
static void StripURLValue(char ** url_return, int * url_length_return);
static void ConvertStringValue(char * string_value, StyleBuffer sb);
static void StyleBufferWrite(char * str, int len, StyleBuffer sb);
static void AnteRule(StyleBuffer sb);
static void IgnoreRule(StyleBuffer sb);
static void PostRule(StyleBuffer sb);
static void SaltPseudoClass(const char * pseudo_class, StyleBuffer sb);
static int32 GetPseudoClass(StyleBuffer sb);

#if (ALLOW_IMPORT || ALLOW_FONTDEF)
static void ConvertToLink(css_node ruleset, int rel_type, StyleBuffer sb);
static void EchoCSS(char * src, int32 src_count, StyleBuffer sb);
#endif

XP_END_PROTOS

static char * input_buffer;
static int32  input_buffer_count;   /* bytes used, not including NULL */
static int32  input_buffer_index;

/* result and max_to_read are created and passed as int, not int32, on Win16. */
void css_GetBuf(char * buf, int *result, int max_to_read)
{
    int i = 0;

    if (input_buffer) {

        while ((i < max_to_read) && (input_buffer_count > input_buffer_index))
	    buf[i++] = input_buffer[input_buffer_index++];

	if (0 == i) {
	    input_buffer = NULL;
	    input_buffer_count = input_buffer_index = 0;
	}
    }

    *result = i;
    return;
}

/* Java and JavaScript */
static const char * reserved_words[] = {
    "abstract",
    "boolean",
    "break",
    "byte",
    "case",
    "catch",
    "char",
    "class",
    "const",
    "continue",
    "default",
    "delete",
    "do",
    "double",
    "else",
    "extends",
    "false",
    "final",
    "finally",
    "float",
    "for",
    "function",
    "goto",
    "if",
    "implements",
    "import",
    "in",
    "instanceof",
    "int",
    "interface",
    "long",
    "native",
    "new",
    "null",
    "package",
    "private",
    "protected",
    "public",
    "return",
    "short",
    "static",
    "super",
    "switch",
    "synchronized",
    "this",
    "throw",
    "throws",
    "transient",
    "true",
    "try",
    "typeof",
    "var",
    "void",
    "volatile",
    "while",
    "with"
};

static XP_Bool IsReservedWord(const char * css_name)
{
    int lower = 0;
    int upper = (sizeof(reserved_words) / sizeof(char *)) - 1;
    int x;
    int32 result;

    while (upper >= lower) {
        x = (lower + upper) / 2;
        result = XP_STRCMP(css_name, reserved_words[x]);
        if (result < 0)
            upper = x - 1;
        else if (result > 0)
            lower = x + 1;
        else
            return 1;
    }
    return 0;
}

/* CSS_ConvertToJSCompatibleName
 * converts characters which are illegal in JavaScript names.
 *
 *  Anne-Marie ==> Anne_Marie     "-" (dash) always converts to "_"
 *  5NewportRd ==> _5NewportRd    1st character a digit prepend "_"
 *  Convert lower to upper case when second parameter is true.
 *  When second parameter is false, prepend "_" to reserved words.
 * Returns NULL on error.
 */
PUBLIC char *
CSS_ConvertToJSCompatibleName(char *css_name, XP_Bool uppercase_it)
{
	int32 len;
	char *new_name;
	char *cp;
        XP_Bool reserved = 0;

	XP_ASSERT(css_name);

	if (!css_name)
		return NULL;

	len = XP_STRLEN(css_name);
        if (! uppercase_it)
            reserved = IsReservedWord(css_name);
	if (reserved || XP_IS_DIGIT(*css_name))
		len++;

	cp = new_name = XP_ALLOC((len+1) * sizeof(char));
	if (!new_name)
		return NULL;

	if (reserved || XP_IS_DIGIT(*css_name)) {
		*cp = '_';
		cp++;
	}

	for(; *css_name; css_name++, cp++)
		if (*css_name == '-')
            *cp = '_';
        else 
            *cp = uppercase_it ? (XP_TO_UPPER(*css_name)) : *css_name;

	*cp = '\0';

	return new_name;
}


PUBLIC
void CSS_ConvertToJS(char * src, int32 src_count, char ** dst, int32 * dst_count)
{
    StyleBuffer sb;
    XP_Bool translated;

    *dst = NULL;
    *dst_count = 0;

    if (NULL == src || 0 == src_count)
        return;

    input_buffer = src;
    input_buffer_count = src_count;
    input_buffer_index = 0;

    css_tree_root = NULL;
    if (css_parse() != 0)
        return;

    sb = XP_NEW_ZAP(StyleBufferRec);
    if (sb == NULL)
        return;

    translated = ConvertStyleSheet(css_tree_root, sb);
    css_FreeNode(css_tree_root);
#if ALLOW_IMPORT
    if (! translated)
        EchoCSS(src, src_count, sb);
#endif

    /* The caller should free *dst. */
    *dst = sb->buffer;
    *dst_count = sb->buffer_count;
    XP_DELETE(sb);
}


/* Release all subtree and the node */
void css_FreeNode(css_node vp)
{ 
    if (vp) {
        css_FreeNode(vp->left);
        css_FreeNode(vp->right);
        if (vp->string)
            XP_FREE(vp->string);
        XP_DELETE(vp);
    }
}


static XP_Bool ConvertStyleSheet(css_node stylesheet, StyleBuffer sb)
{

#if (ALLOW_IMPORT || ALLOW_FONTDEF)
    XP_Bool translate_rules = 1;

    /* CSS1 requires @import productions occurring after
     * ruleset productions to be ignored.  This is implemented
     * here.  The restriction is not enforced by the parser.
     */
    for (; stylesheet && (stylesheet->node_id != NODE_RULESET_LIST);
         stylesheet = stylesheet->left) {
#if ALLOW_IMPORT
        if (NODE_IMPORT_LIST == stylesheet->node_id) {
            ConvertToLink(stylesheet->right, stylesheet->node_id, sb);
            translate_rules = 0;
        }
#endif
#if ALLOW_FONTDEF
        if (NODE_FONTDEF_LIST == stylesheet->node_id) {
            ConvertToLink(stylesheet->right, stylesheet->node_id, sb);
        }
#endif
    }
#if (ALLOW_IMPORT)
    if (! translate_rules)
        return 0;
#endif
#endif

    for (; stylesheet; stylesheet = stylesheet->left) {
        if (stylesheet->node_id == NODE_RULESET_LIST)
            ConvertRule(stylesheet->right, sb);
    }

    return 1;
}


/* Generate the JavaScript corresponding to one CSS1 rule. */
static void ConvertRule(css_node rule, StyleBuffer sb)
{
    css_node selector_list, selector;
    css_node declaration_list;

    if (! rule || rule->node_id != NODE_RULESET)
        return;

    for (selector_list = rule->left; selector_list; 
         selector_list = selector_list->left) {

        selector = selector_list->right;
        if (! selector)
            continue;

        for (declaration_list = rule->right; declaration_list;
             declaration_list = declaration_list->left) {

            if (declaration_list->right) {
                AnteRule(sb);
                ConvertSelector(selector->left, selector->right, sb);
                ConvertDeclaration(declaration_list->right, sb);
                PostRule(sb);
            }
        }
    }
}

static void AnteRule(StyleBuffer sb)
{
    sb->prior_count = sb->buffer_count;
    sb->pseudo_class_state = CSS_PSEUDO_CLASS_NONE;
}

static void PostRule(StyleBuffer sb)
{
    if (sb->ignore_rule) {
        sb->buffer_count = sb->prior_count;
        sb->ignore_rule = 0;
        if (0 == sb->buffer_count)
            sb->buffer_count = 1;
        sb->buffer[sb->buffer_count-1] = '\0';
    }
}

static void StyleBufferWrite(char * str, int len, StyleBuffer sb)
{

    if (str == NULL)
        return;

    if (len <= 0) {
        len = strlen(str);
        if (len <= 0)
            return;
    }

    if (sb->buffer == NULL) {
        sb->buffer = (char *) XP_ALLOC(STYLE_INITIAL_BUFSIZ);
        if (sb->buffer) {
            sb->buffer_size = STYLE_INITIAL_BUFSIZ;
            *sb->buffer = '\0';
            sb->buffer_count = 1;
        }
    }

    if (sb->buffer_count + len > sb->buffer_size) {
        sb->buffer = (char *)
            XP_REALLOC(sb->buffer, sb->buffer_size + STYLE_INCR_BUFSIZ);
        if (sb->buffer) {
            sb->buffer_size += STYLE_INCR_BUFSIZ;
        } else {
            sb->buffer_size = sb->buffer_count = 0;
        }
    }

    if (sb->buffer_count + len <= sb->buffer_size) {
        (void) strncpy(sb->buffer + sb->buffer_count - 1, str, len);
        sb->buffer_count += len;
        sb->buffer[sb->buffer_count-1] = '\0';
    }
}


static void ConvertSelector(css_node selector, css_node pseudo_element,
                            StyleBuffer sb)
{
    if (selector->node_id == NODE_SELECTOR_CONTEXTUAL) {
        StyleBufferWrite("contextual(", 11, sb);
        ConvertContextualSelector(selector, pseudo_element, sb);
        StyleBufferWrite(")", 1, sb);
    } else {
        ConvertSingleSelector(selector, pseudo_element, sb);
    }
}


static void ConvertContextualSelector(css_node selector,
                                      css_node pseudo_element, StyleBuffer sb)
{
    if (selector->left) {
        /* pseudo_elements may be present only on the final context object */
        ConvertContextualSelector(selector->left, NULL, sb);
        StyleBufferWrite(", ", 2, sb);
    }

    ConvertSingleSelector(selector->right, pseudo_element, sb);
}


static void ConvertSingleSelector(css_node selector, css_node pseudo_element,
                                  StyleBuffer sb)
{
    char * str1;
    char * str2;

    if (! selector)
        return;

    if (selector->node_id == NODE_WILD) {
        /* CSS1 Section 7.1 states: "A ruleset that [has] a selector
         * string that is not valid CSS1 is skipped."  (sic)
         */
        IgnoreRule(sb);
    }

    if (! selector->left)
        return;

    str1 = selector->left->string;
    str2 = selector->right ? selector->right->string : NULL;

    switch (selector->node_id) {

    case NODE_SIMPLE_SELECTOR_NAME_ONLY:

        /* "document.tags.<TAG>" */
        StyleBufferWrite("document.tags.", 14, sb);
        str1 = CSS_ConvertToJSCompatibleName(str1, TRUE);
        StyleBufferWrite(str1, 0, sb);
        XP_FREE(str1);
        break;

    case NODE_SIMPLE_SELECTOR_DOT_AND_CLASS:

        /* "document.classes.<class>.all" */
        StyleBufferWrite("document.classes.", 17, sb);
        str1 = CSS_ConvertToJSCompatibleName(str1, FALSE);
        StyleBufferWrite(str1, 0, sb);
        XP_FREE(str1);
        StyleBufferWrite(".all", 4, sb);
        break;

    case NODE_SIMPLE_SELECTOR_ID_SELECTOR:

        /* "document.ids.<id>" */
        StyleBufferWrite("document.ids.", 13, sb);
        str1 = CSS_ConvertToJSCompatibleName(str1, FALSE);
        StyleBufferWrite(str1, 0, sb);
        XP_FREE(str1);
        break;

    case NODE_SIMPLE_SELECTOR_NAME_AND_CLASS:

        /* "document.classes.<class>.<TAG>" */
        StyleBufferWrite("document.classes.", 17, sb);
        str2 = CSS_ConvertToJSCompatibleName(str2, FALSE);
        StyleBufferWrite(str2, 0, sb);
        XP_FREE(str2);
        StyleBufferWrite(".", 1, sb);
        str1 = CSS_ConvertToJSCompatibleName(str1, TRUE);
        StyleBufferWrite(str1, 0, sb);
        XP_FREE(str1);
        break;

    case NODE_SIMPLE_SELECTOR_NAME_PSEUDO_CLASS:

        /* "document.tags.<TAG> */
        /* By design, prepend the pseudoclass name to the property
         * name when the property is 'color'; otherwise, ignore the
         * pseudoclass name.
         */
        StyleBufferWrite("document.tags.", 14, sb);
        str1 = CSS_ConvertToJSCompatibleName(str1, TRUE);
        StyleBufferWrite(str1, 0, sb);
        XP_FREE(str1);
        SaltPseudoClass(str2, sb);
        break;

    case NODE_SIMPLE_SELECTOR_NAME_CLASS_PSEUDO_CLASS:

        /* "document.classes.<class>.<TAG>" */
        /* By design, prepend the pseudoclass name to the property
         * name when the property is 'color'; otherwise, ignore the
         * pseudoclass name.
         */
        StyleBufferWrite("document.classes.", 17, sb);
        str1 = CSS_ConvertToJSCompatibleName(str1, FALSE);
        StyleBufferWrite(str1, 0, sb);
        XP_FREE(str1);
        StyleBufferWrite(".", 1, sb);
        str1 = CSS_ConvertToJSCompatibleName(selector->string, TRUE);
        StyleBufferWrite(str1, 0, sb);
        XP_FREE(str1);
        SaltPseudoClass(str2, sb);
        break;

    default:
        break;
    }

    if (pseudo_element) {
#if ! ALLOW_PSEUDO_ELEMENTS
        IgnoreRule(sb);
#endif
        StyleBufferWrite(".", 1, sb);
        ConvertProperty(pseudo_element->string, sb, NULL);
    }
}

static void IgnoreRule(StyleBuffer sb)
{
    sb->ignore_rule = 1;
}

static void SaltPseudoClass(const char * pseudo_class, StyleBuffer sb)
{
    if (! pseudo_class)
        return;

    /* The parser always gives these strings in lower case. */
    if (0 == XP_STRCMP(pseudo_class, "link"))
        sb->pseudo_class_state = CSS_PSEUDO_CLASS_LINK;
    else if (0 == XP_STRCMP(pseudo_class, "visited"))
        sb->pseudo_class_state = CSS_PSEUDO_CLASS_VISITED;
    else if (0 == XP_STRCMP(pseudo_class, "active")) {
#if (! ALLOW_ACTIVE_PSEUDO_CLASS)
        IgnoreRule(sb);
#endif
        sb->pseudo_class_state = CSS_PSEUDO_CLASS_ACTIVE;
    }
}

static int32 GetPseudoClass(StyleBuffer sb)
{
    return sb->pseudo_class_state;
}

/* Pseudo-elements as well as property names are converted by this. */
static void ConvertProperty(const char * css1_property, StyleBuffer sb, 
                            int * property_type_return)
{
    XP_Bool upper;
    char ch[2];
    int32 pseudo_class;

    ch[1] = '\0';

    if (property_type_return)
        *property_type_return = CSS_PROPERTY_TYPE_ASSIGNMENT;

    /* The CSS1 float property is translated to align because float is
     * a reserved word in JavaScript.
     */
    if (0 == XP_STRCASECMP(css1_property, "float"))
        StyleBufferWrite("align", 5, sb);
    else {
        if (property_type_return) {
            if (   (0 == XP_STRCASECMP(css1_property, "margin"))
                || (0 == XP_STRCASECMP(css1_property, "padding"))
                || (0 == XP_STRCASECMP(css1_property, "border-width")))
                *property_type_return = CSS_PROPERTY_TYPE_CALL;
        }

        pseudo_class = GetPseudoClass(sb);

        if (0 == XP_STRCASECMP(css1_property, "margin"))
            StyleBufferWrite("margins", 7, sb);
        else if (0 == XP_STRCASECMP(css1_property, "padding"))
            StyleBufferWrite("paddings", 8, sb);
        else if (0 == XP_STRCASECMP(css1_property, "border-width"))
            StyleBufferWrite("borderWidths", 12, sb);
        else if ((CSS_PSEUDO_CLASS_NONE != pseudo_class)
                 && (0 == XP_STRCASECMP(css1_property, "color"))) {
            switch (pseudo_class) {
            case CSS_PSEUDO_CLASS_LINK:
            default:
                StyleBufferWrite("linkColor", 9, sb);
                break;
            case CSS_PSEUDO_CLASS_ACTIVE:
                StyleBufferWrite("activeColor", 11, sb);
                break;
            case CSS_PSEUDO_CLASS_VISITED:
                StyleBufferWrite("visitedColor", 12, sb);
                break;
            }
        }
        else {
            for (upper=0; *css1_property; css1_property++) {

                if (*css1_property == '-') {
                    upper = 1;
                    continue;
                }
       
                ch[0] = (upper ? toupper(*css1_property)
                         : tolower(*css1_property));
                upper = 0;
                StyleBufferWrite(ch, 1, sb);
            }
        }
    }
}


static void ConvertDeclaration(css_node declaration, StyleBuffer sb)
{
    int property_type;

    if (declaration && 
        (declaration->node_id == NODE_DECLARATION_PROPERTY_EXPR ||
         declaration->node_id == NODE_DECLARATION_PROPERTY_EXPR_PRIO)) {
        StyleBufferWrite(".", 1, sb);
        ConvertProperty(declaration->left->string, sb, &property_type);
        ConvertValue(declaration->right, sb, property_type);
#if ALLOW_PRIORITY
        if (declaration->node_id == NODE_DECLARATION_PROPERTY_EXPR_PRIO)
            StyleBufferWrite(" !important", 11, sb);
#endif
        StyleBufferWrite("\n", 1, sb);
    }
}


static void ConvertValue(css_node value, StyleBuffer sb, int property_type)
{
    css_node term;
    XP_Bool comma;


    if (CSS_PROPERTY_TYPE_ASSIGNMENT == property_type)
        StyleBufferWrite(" = \"", 4, sb);
    else
        StyleBufferWrite("(", 1, sb);

    for (comma=FALSE; value; value=value->left, comma = TRUE) {

        if (CSS_PROPERTY_TYPE_CALL == property_type && comma)
            StyleBufferWrite(",", 1, sb);

        if (value->node_id == NODE_EMPTY_OP)
            StyleBufferWrite(" ", 1, sb);

        if (CSS_PROPERTY_TYPE_CALL == property_type)
            StyleBufferWrite("\"", 1, sb);

        if (value->node_id == NODE_EXPR_OP)
            StyleBufferWrite(value->string, 0, sb);

        term = value->right;
        if (term) {
            if (term->node_id == NODE_STRING)
                ConvertStringValue(term->string, sb);
            else if (NODE_URL == term->node_id)
                ConvertURLValue(term->string, sb);
            else {
                StyleBufferWrite(term->string, 0, sb);
                if (term->node_id == NODE_UNARY_OP)
                    StyleBufferWrite(term->left->string, 0, sb);
            }
        }

        if (CSS_PROPERTY_TYPE_CALL == property_type)
            StyleBufferWrite("\"", 1, sb);
    }

    if (CSS_PROPERTY_TYPE_ASSIGNMENT == property_type)
        StyleBufferWrite("\"", 1, sb);
    else
        StyleBufferWrite(")", 1, sb);
}


static void ConvertStringValue(char * string_value, StyleBuffer sb)
{
    for (; *string_value; string_value++) {
        if (*string_value == '"')
            StyleBufferWrite("\\", 1, sb);
        StyleBufferWrite(string_value, 1, sb);
    }
}

static void StripURLValue(char ** url_return, int * url_length_return)
{
    char * ptr;
    int len;

    if (! url_return || ! *url_return ||
        ! url_length_return || ! *url_length_return)
        return;

    ptr = *url_return;
    len = *url_length_return;

    /* Skip leading whitespace */
    while (len > 0 && isspace(*ptr)) {
        ptr++;
        len--;
    }

    /* Skip trailing whitespace */
    while (len > 0 && isspace(*(ptr + len - 1)))
        len--;

    /* Skip enclosing quotation marks */
    if (('"' == *ptr || '\'' == *ptr ) &&
        (len > 1) &&
        (*ptr == *(ptr + len - 1))) {
        ptr++;
        len -= 2;
    }

    *url_return = ptr;
    *url_length_return = len;
}

static void ConvertURLValue(char * url_value, StyleBuffer sb)
{
    char * ptr = url_value;
    int len;

    len = strlen(ptr);
    if (len < 6)
        return;

    /* Don't count the 5 characters for 'url()' */
    len -= 5;
    StyleBufferWrite("url(", 4, sb);
    ptr += 4;

    StripURLValue(&ptr, &len);

    if (len > 0)
        StyleBufferWrite(ptr, len, sb);

    StyleBufferWrite(")", 1, sb);
}

#if (ALLOW_IMPORT || ALLOW_FONTDEF)

static char * open_call   = "document.write('";
static char * close_call   = "');\n";

static void ConvertToLink(css_node rule, int rel_type, StyleBuffer sb)
{
    int len;
    char * ptr;
    char ch;
    char hexbuf[4];

    if (rule && rule->string) {
        if ((NODE_IMPORT_URL == rule->node_id) || 
            (NODE_IMPORT_STRING == rule->node_id)) {

            StyleBufferWrite(open_call, 0, sb);

            if (NODE_IMPORT_LIST == rel_type)
                StyleBufferWrite("<LINK REL=stylesheet ", 21, sb);
            else
                StyleBufferWrite("<LINK REL=fontdef ", 18, sb);

            /* Copy the url */
            len = XP_STRLEN(rule->string);
            ptr = NULL;
            if ((NODE_IMPORT_URL == rule->node_id) && (len > 5)) {
                /* Copy the characters inside the parens */
                ptr = rule->string + 4;
                len -= 5;
                StripURLValue(&ptr, &len);
                if (len <= 0)
                    ptr = NULL;
            } else {
                /* Copy the characters inside the quotes */
                ptr = rule->string + 1;
                len -= 2;
            }
            if (NULL != ptr) {
                if (NODE_IMPORT_LIST == rel_type)
                    StyleBufferWrite("HREF=", 5, sb);
                else
                    StyleBufferWrite("SRC=", 4, sb);
                StyleBufferWrite(ptr, len, sb);
            }

            StyleBufferWrite(">');\n", 5, sb);
        }
    }
}

static char * open_style = "<style type=text/css>\\n";
static char * close_style = "</style>";
static char * disable_at_rules = "HI { donna : lou }\\n";

static void EchoCSS(char * src, int32 src_count, StyleBuffer sb)
{
    char ch;
    char * start;
    char * end;
    int len;

    if (! src || src_count <= 0) return;

    StyleBufferWrite(open_call, 0, sb);
    StyleBufferWrite(open_style, 0, sb);
    StyleBufferWrite(close_call, 0, sb);

    /* Don't recurse on the @-rules! 
     * Write a harmless rule at the beginning of the style sheet.
     * The lexer, parser, and finally the translator will see it.
     * The translator will skip @ rules which occur after any
     * ordinary rule.  CSS requires that behavior for @import;
     * @fontdef voluntarily observes the same restriction; there
     * are no other recognized @ rules.
     */
    StyleBufferWrite(open_call, 0, sb);
    StyleBufferWrite(disable_at_rules, 0, sb);
    StyleBufferWrite(close_call, 0, sb);

    /* Echo the entire CSS buffer, escaping some characters. */
    StyleBufferWrite(open_call, 0, sb);
    start = src;
    for (end = start; --src_count >=0; end++) {

        ch = *end;
        if (('\n' == ch) || ('\'' == ch) || ('\f' == ch) ||
            ('\v' == ch) || ('\r' == ch)) {
            len = end - start;
            if (len) {
                StyleBufferWrite(start, len, sb);
                start = end;
            }

            switch (ch) {
            case '\n': StyleBufferWrite("\\n", 2, sb);	break;
            case '\'': StyleBufferWrite("\\'", 2, sb);	break;
            case '\f': StyleBufferWrite("\\f", 2, sb);	break;
            case '\v': StyleBufferWrite("\\v", 2, sb);	break;
            case '\r': StyleBufferWrite("\\r", 2, sb);	break;
            default  : break;
            }
            
            start = end + 1;
        }
    }
    len = end - start;
    if (len) {
        StyleBufferWrite(start, len, sb);
    }
    StyleBufferWrite(close_call, 0, sb);

    StyleBufferWrite(open_call, 0, sb);
    StyleBufferWrite(close_style, 0, sb);
    StyleBufferWrite(close_call, 0, sb);

}
#endif

#ifdef TEST_CSS_TRANSLATION
#define TEST_CSS_INITIAL_BUFSIZ 4096
#define TEST_CSS_INCR_BUFSIZ    4096

void main()
{
    int ch;
    char *b;
    int32 src_count;
    char *dst;
    int32 dst_count;
    char * src;
    int src_size;
    int done;

        
    done = ch = 0;
    src_size = src_count = 0;

    src = (char *) XP_ALLOC(TEST_CSS_INITIAL_BUFSIZ);
    if (src) src_size = TEST_CSS_INITIAL_BUFSIZ;

    while (!done) {

        for (b = src + src_count; src_count < src_size; src_count++, b++) {
            *b = ch = getc(stdin);
            /* Either the null character or EOF will serve to terminate. */
            if (ch == EOF || ch == '\0') {
                done = 1;
                /* We don't need to add this character to the src_count */
                break;
            }
        }

        if (!done) {
            src_size += TEST_CSS_INCR_BUFSIZ;
            src = (char *) XP_REALLOC(src, src_size);
            if (!src) {
                src_size = src_count = 0;
                printf("css test: memory allocation failure\n");
                done = 1;
            }
        }
    }

    /* The src_count need not include the terminating NULL or EOF */
    CSS_ConvertToJS(src, src_count, &dst, &dst_count);
    printf("%s", dst);
    XP_FREE(dst);
}

#endif
