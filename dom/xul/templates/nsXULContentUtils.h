/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A package of routines shared by the XUL content code.

 */

#ifndef nsXULContentUtils_h__
#define nsXULContentUtils_h__

#include "nsISupports.h"

class nsIAtom;
class nsIContent;
class nsIDocument;
class nsIRDFNode;
class nsIRDFResource;
class nsIRDFLiteral;
class nsIRDFService;
class nsIDateTimeFormat;
class nsICollation;

// errors to pass to LogTemplateError
#define ERROR_TEMPLATE_INVALID_QUERYPROCESSOR                           \
        "querytype attribute doesn't specify a valid query processor"
#define ERROR_TEMPLATE_INVALID_QUERYSET                                 \
        "unexpected <queryset> element"
#define ERROR_TEMPLATE_NO_MEMBERVAR                                     \
        "no member variable found. Action body should have an element with uri attribute"
#define ERROR_TEMPLATE_WHERE_NO_SUBJECT                                 \
        "<where> element is missing a subject attribute"
#define ERROR_TEMPLATE_WHERE_NO_RELATION                                \
        "<where> element is missing a rel attribute"
#define ERROR_TEMPLATE_WHERE_NO_VALUE                                   \
        "<where> element is missing a value attribute"
#define ERROR_TEMPLATE_WHERE_NO_VAR                                     \
        "<where> element must have at least one variable as a subject or value"
#define ERROR_TEMPLATE_BINDING_BAD_SUBJECT                              \
        "<binding> requires a variable for its subject attribute"
#define ERROR_TEMPLATE_BINDING_BAD_PREDICATE                            \
        "<binding> element is missing a predicate attribute"
#define ERROR_TEMPLATE_BINDING_BAD_OBJECT                               \
        "<binding> requires a variable for its object attribute"
#define ERROR_TEMPLATE_CONTENT_NOT_FIRST                                \
        "expected <content> to be first"
#define ERROR_TEMPLATE_MEMBER_NOCONTAINERVAR                            \
        "<member> requires a variable for its container attribute"
#define ERROR_TEMPLATE_MEMBER_NOCHILDVAR                                \
        "<member> requires a variable for its child attribute"
#define ERROR_TEMPLATE_TRIPLE_NO_VAR                                    \
        "<triple> should have at least one variable as a subject or object"
#define ERROR_TEMPLATE_TRIPLE_BAD_SUBJECT                               \
        "<triple> requires a variable for its subject attribute"
#define ERROR_TEMPLATE_TRIPLE_BAD_PREDICATE                             \
        "<triple> should have a non-variable value as a predicate"
#define ERROR_TEMPLATE_TRIPLE_BAD_OBJECT                                \
        "<triple> requires a variable for its object attribute"
#define ERROR_TEMPLATE_MEMBER_UNBOUND                                   \
        "neither container or child variables of <member> has a value"
#define ERROR_TEMPLATE_TRIPLE_UNBOUND                                   \
        "neither subject or object variables of <triple> has a value"
#define ERROR_TEMPLATE_BAD_XPATH                                        \
        "XPath expression in query could not be parsed"
#define ERROR_TEMPLATE_BAD_ASSIGN_XPATH                                 \
        "XPath expression in <assign> could not be parsed"
#define ERROR_TEMPLATE_BAD_BINDING_XPATH                                \
        "XPath expression in <binding> could not be parsed"
#define ERROR_TEMPLATE_STORAGE_BAD_URI                                  \
        "only profile: or file URI are allowed"
#define ERROR_TEMPLATE_STORAGE_CANNOT_OPEN_DATABASE                     \
        "cannot open given database"
#define ERROR_TEMPLATE_STORAGE_BAD_QUERY                                \
        "syntax error in the SQL query"
#define ERROR_TEMPLATE_STORAGE_UNKNOWN_QUERY_PARAMETER                   \
        "the given named parameter is unknown in the SQL query"
#define ERROR_TEMPLATE_STORAGE_WRONG_TYPE_QUERY_PARAMETER               \
        "the type of a query parameter is wrong"
#define ERROR_TEMPLATE_STORAGE_QUERY_PARAMETER_NOT_BOUND                \
        "a query parameter cannot be bound to the SQL query"

class nsXULContentUtils
{
protected:
    static nsIRDFService* gRDF;
    static nsIDateTimeFormat* gFormat;
    static nsICollation *gCollation;

    static bool gDisableXULCache;

    static int
    DisableXULCacheChangedCallback(const char* aPrefName, void* aClosure);

public:
    static nsresult
    Init();

    static nsresult
    Finish();

    static nsresult
    FindChildByTag(nsIContent *aElement,
                   int32_t aNameSpaceID,
                   nsIAtom* aTag,
                   nsIContent **aResult);

    static nsresult
    FindChildByResource(nsIContent* aElement,
                        nsIRDFResource* aResource,
                        nsIContent** aResult);

    static nsresult
    GetTextForNode(nsIRDFNode* aNode, nsAString& aResult);

    static nsresult
    GetResource(int32_t aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult);

    static nsresult
    GetResource(int32_t aNameSpaceID, const nsAString& aAttribute, nsIRDFResource** aResult);

    static nsresult
    SetCommandUpdater(nsIDocument* aDocument, nsIContent* aElement);

    /**
     * Log a message to the error console
     */
    static void
    LogTemplateError(const char* aMsg);

    static nsIRDFService*
    RDFService()
    {
        return gRDF;
    }

    static nsICollation*
    GetCollation();

#define XUL_RESOURCE(ident, uri) static nsIRDFResource* ident
#define XUL_LITERAL(ident, val)  static nsIRDFLiteral*  ident
#include "nsXULResourceList.h"
#undef XUL_RESOURCE
#undef XUL_LITERAL
};

#endif // nsXULContentUtils_h__
