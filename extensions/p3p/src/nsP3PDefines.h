/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#ifndef nsP3PDefines_h__
#define nsP3PDefines_h__

#define P3P_PREF                                "P3P."
#define P3P_PREF_ENABLED                        P3P_PREF"enabled"
#define P3P_PREF_WARNINGNOTPRIVATE              P3P_PREF"warningnotprivate"
#define P3P_PREF_WARNINGPARTIALPRIVACY          P3P_PREF"warningpartialprivacy"
#define P3P_PREF_WARNINGPOSTTONOTPRIVATE        P3P_PREF"warningposttonotprivate"
#define P3P_PREF_WARNINGPOSTTOBROKENPOLICY      P3P_PREF"warningposttobrokenpolicy"
#define P3P_PREF_WARNINGPOSTTONOPOLICY          P3P_PREF"warningposttonopolicy"

// HTTP headers and values related to P3P
//   Example: P3P: policyref="http://www.anysite.com/policyrefs.xml"
//
//   Note: Mozilla converts all header keys to lower case when stored
//         so lower case key values are required
#define P3P_HTTPEXT_P3P_KEY            "p3p"
#define P3P_HTTPEXT_POLICYREF_KEY      "policyref="

#define P3P_HTTP_USERAGENT_KEY         "user-agent"
#define P3P_HTTP_REFERER_KEY           "referer"
#define P3P_HTTP_COOKIE_KEY            "Cookie"
#define P3P_HTTP_SETCOOKIE_KEY         "set-cookie"
#define P3P_HTTP_CONTENTTYPE_KEY       "content-type"
#define P3P_HTTP_PRAGMA_KEY            "pragma"
#define P3P_HTTP_CACHECONTROL_KEY      "cache-control"
#define P3P_HTTP_EXPIRES_KEY           "expires"
#define P3P_HTTP_DATE_KEY              "date"

#define P3P_CACHECONTROL_NOCACHE       "no-cache"
#define P3P_CACHECONTROL_MAXAGE        "max-age"
#define P3P_PRAGMA_NOCACHE             "no-cache"

#define P3P_DOCUMENT_DEFAULT_LIFETIME  86400          // 24 hours in seconds

// HTML/XML LINK tags related to P3P
//   Example: <LINK rel="P3Pv1"
//                  href="http://www.anysite.com/policyrefs.xml">
#define P3P_LINK_REL              "rel"
#define P3P_LINK_P3P_VALUE        "\"P3Pv1\""
#define P3P_LINK_HREF             "href"

// Constants used to indicate where the reference to the PolicyRefFile occurred
#define P3P_REFERENCE_WELL_KNOWN  0
#define P3P_REFERENCE_HTTP_HEADER 4
#define P3P_REFERENCE_LINK_TAG    8

// Constant to define the P3P well known location
#define P3P_WELL_KNOWN_LOCATION   "/w3c/p3p.xml"

// Constant used to indicate any request METHOD
#define P3P_ANY_METHOD            "ANYMETHOD"

// Constants used to identify the type of XML Processor
#define P3P_DOCUMENT_POLICYREFFILE          4
#define P3P_DOCUMENT_POLICY                 8
#define P3P_DOCUMENT_DATASCHEMA             12

// Constants used to define the state of the privacy check
#define P3P_CHECK_WELLKNOWN_POLICYREFFILE   0
#define P3P_CHECK_SPECIFIED_POLICYREFFILE   4
#define P3P_CHECK_EXPIRED_POLICYREFFILE     8
#define P3P_CHECK_READING_POLICYREFFILE     12
#define P3P_CHECK_POLICY_REFERENCE          16
#define P3P_CHECK_POLICY                    20
#define P3P_CHECK_COMPLETE                  24
#define P3P_CHECK_INITIAL_STATE             P3P_CHECK_WELLKNOWN_POLICYREFFILE

// Constants used to indicate Asynchronous vs. Synchronous read mode
#define P3P_ASYNC_READ            0
#define P3P_SYNC_READ             4

// Constants used to indicate the type of check being performed
#define P3P_OBJECT_CHECK          0
#define P3P_FORM_SUBMISSION_CHECK 4

// Return values used for Privacy Check
#define P3P_PRIVACY_NONE          NS_ERROR_GENERATE_SUCCESS( 0, 0 )
#define P3P_PRIVACY_IN_PROGRESS   NS_ERROR_GENERATE_SUCCESS( 0, 4 )
#define P3P_PRIVACY_NOT_MET       NS_ERROR_GENERATE_SUCCESS( 0, 8 )
#define P3P_PRIVACY_PARTIAL       NS_ERROR_GENERATE_SUCCESS( 0, 12 )
#define P3P_PRIVACY_MET           NS_ERROR_GENERATE_SUCCESS( 0, 16 )

// P3P XML tag optionality defines
#define P3P_TAG_OPTIONAL          PR_TRUE
#define P3P_TAG_MANDATORY         PR_FALSE

// P3P Policy Ref File tags
#define P3P_META_TAG              "META"
#define P3P_POLICYREFERENCES_TAG  "POLICY-REFERENCES"
#define P3P_EXPIRY_TAG            "EXPIRY"
#define P3P_RDF_TAG               "RDF"
#define P3P_POLICIES_TAG          "POLICIES"
#define P3P_POLICYREF_TAG         "POLICY-REF"
#define P3P_INCLUDE_TAG           "INCLUDE"
#define P3P_EXCLUDE_TAG           "EXCLUDE"
#define P3P_EMBEDDEDINCLUDE_TAG   "EMBEDDED-INCLUDE"
#define P3P_EMBEDDEDEXCLUDE_TAG   "EMBEDDED-EXCLUDE"
#define P3P_METHOD_TAG            "METHOD"

// P3P Policy tags
#define P3P_POLICY_TAG            "POLICY"
#define P3P_ENTITY_TAG            "ENTITY"
#define P3P_ACCESS_TAG            "ACCESS"
#define P3P_DISPUTESGROUP_TAG     "DISPUTES-GROUP"
#define P3P_DISPUTES_TAG          "DISPUTES"
#define P3P_IMG_TAG               "IMG"
#define P3P_REMEDIES_TAG          "REMEDIES"
#define P3P_STATEMENT_TAG         "STATEMENT"
#define P3P_CONSEQUENCE_TAG       "CONSEQUENCE"
#define P3P_RECIPIENT_TAG         "RECIPIENT"
#define P3P_PURPOSE_TAG           "PURPOSE"
#define P3P_RETENTION_TAG         "RETENTION"
#define P3P_DATAGROUP_TAG         "DATA-GROUP"
#define P3P_DATA_TAG              "DATA"

// P3P Data Schema tags
#define P3P_DATASCHEMA_TAG        "DATASCHEMA"
#define P3P_DATASTRUCT_TAG        "DATA-STRUCT"
#define P3P_DATADEF_TAG           "DATA-DEF"

// P3P multi-use tags
#define P3P_EXTENSION_TAG         "EXTENSION"
#define P3P_CATEGORIES_TAG        "CATEGORIES"
#define P3P_LONGDESCRIPTION_TAG   "LONG-DESCRIPTION"

// P3P ACCESS tags
#define P3P_ACCESS_NONIDENT                 "nonident"
#define P3P_ACCESS_IDENT_CONTACT            "ident_contact"
#define P3P_ACCESS_OTHER_IDENT              "other_ident"
#define P3P_ACCESS_CONTACT_AND_OTHER        "contact_and_other"
#define P3P_ACCESS_ALL                      "all"
#define P3P_ACCESS_NONE                     "none"

// P3P REMEDIES tags
#define P3P_REMEDIES_CORRECT                "correct"
#define P3P_REMEDIES_MONEY                  "money"
#define P3P_REMEDIES_LAW                    "law"

// P3P RECIPIENT tags
#define P3P_RECIPIENT_OURS                  "ours"
#define P3P_RECIPIENT_SAME                  "same"
#define P3P_RECIPIENT_OTHERRECIPIENT        "other-recipient"
#define P3P_RECIPIENT_DELIVERY              "delivery"
#define P3P_RECIPIENT_PUBLIC                "public"
#define P3P_RECIPIENT_UNRELATED             "unrelated"

// P3P RECIPIENT-DESCRIPTION tag
#define P3P_RECIPIENTDESCRIPTION_TAG        "recipient-description"

// P3P PURPOSE tags
#define P3P_PURPOSE_CURRENT                 "current"
#define P3P_PURPOSE_ADMIN                   "admin"
#define P3P_PURPOSE_DEVELOP                 "develop"
#define P3P_PURPOSE_CUSTOMIZATION           "customization"
#define P3P_PURPOSE_TAILORING               "tailoring"
#define P3P_PURPOSE_PSEUDOANALYSIS          "pseudo-analysis"
#define P3P_PURPOSE_PSEUDODECISION          "pseudo-decision"
#define P3P_PURPOSE_INDIVIDUALANALYSIS      "individual-analysis"
#define P3P_PURPOSE_INDIVIDUALDECISION      "individual-decision"
#define P3P_PURPOSE_CONTACT                 "contact"
#define P3P_PURPOSE_HISTORICAL              "historical"
#define P3P_PURPOSE_TELEMARKETING           "telemarketing"
#define P3P_PURPOSE_OTHERPURPOSE            "other-purpose"

// P3P RETENTION tags
#define P3P_RETENTION_NORETENTION           "no-retention"
#define P3P_RETENTION_STATEDPURPOSE         "stated-purpose"
#define P3P_RETENTION_LEGALREQUIREMENT      "legal-requirement"
#define P3P_RETENTION_INDEFINITELY          "indefinitely"
#define P3P_RETENTION_BUSINESSPRACTICES     "business-practices"

// P3P CATEGORIES tags
#define P3P_CATEGORIES_PHYSICAL             "physical"
#define P3P_CATEGORIES_ONLINE               "online"
#define P3P_CATEGORIES_UNIQUEID             "uniqueid"
#define P3P_CATEGORIES_PURCHASE             "purchase"
#define P3P_CATEGORIES_FINANCIAL            "financial"
#define P3P_CATEGORIES_COMPUTER             "computer"
#define P3P_CATEGORIES_NAVIGATION           "navigation"
#define P3P_CATEGORIES_INTERACTIVE          "interactive"
#define P3P_CATEGORIES_DEMOGRAPHIC          "demographic"
#define P3P_CATEGORIES_CONTENT              "content"
#define P3P_CATEGORIES_STATE                "state"
#define P3P_CATEGORIES_POLITICAL            "political"
#define P3P_CATEGORIES_HEALTH               "health"
#define P3P_CATEGORIES_PREFERENCE           "preference"
#define P3P_CATEGORIES_LOCATION             "location"
#define P3P_CATEGORIES_GOVERNMENT           "government"
#define P3P_CATEGORIES_OTHER                "other"

// P3P tag attributes
#define P3P_DATE_ATTRIBUTE                  "date"
#define P3P_MAXAGE_ATTRIBUTE                "max-age"
#define P3P_ABOUT_ATTRIBUTE                 "about"
#define P3P_DISCURI_ATTRIBUTE               "discuri"
#define P3P_OPTURI_ATTRIBUTE                "opturi"
#define P3P_NAME_ATTRIBUTE                  "name"
#define P3P_RESOLUTIONTYPE_ATTRIBUTE        "resolution-type"
#define P3P_SERVICE_ATTRIBUTE               "service"
#define P3P_VERIFICATION_ATTRIBUTE          "verification"
#define P3P_SHORTDESCRIPTION_ATTRIBUTE      "short-description"
#define P3P_SRC_ATTRIBUTE                   "src"
#define P3P_WIDTH_ATTRIBUTE                 "width"
#define P3P_HEIGHT_ATTRIBUTE                "height"
#define P3P_ALT_ATTRIBUTE                   "alt"
#define P3P_REQUIRED_ATTRIBUTE              "required"
#define P3P_BASE_ATTRIBUTE                  "base"
#define P3P_REF_ATTRIBUTE                   "ref"
#define P3P_OPTIONAL_ATTRIBUTE              "optional"
#define P3P_STRUCTREF_ATTRIBUTE             "structref"

// P3P tag attribute values
#define P3P_SERVICE_VALUE                   "service"
#define P3P_INDEPENDENT_VALUE               "independent"
#define P3P_COURT_VALUE                     "court"
#define P3P_LAW_VALUE                       "law"
#define P3P_YES_VALUE                       "yes"
#define P3P_NO_VALUE                        "no"
#define P3P_ALWAYS_VALUE                    "always"
#define P3P_OPTIN_VALUE                     "opt_in"
#define P3P_OPTOUT_VALUE                    "opt_out"

// P3P Related NameSpaces for the Policy Ref File and Policy File
#define P3P_P3P_NAMESPACE                   "http://www.w3.org/2000/09/15/P3Pv1"
#define P3P_RDF_NAMESPACE                   "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

// P3P Base DataSchema URI
#define P3P_BASE_DATASCHEMA                 "http://www.w3.org/TR/P3P/base"
#define P3P_BASE_DATASCHEMA_LOCAL_NAME      "P3PBaseDataSchema.xml"

#define P3P_PROPERTIES_FILE       "chrome://communicator/locale/P3P.properties"

#define P3P_PRINT_REQUEST_HEADERS( _httpchannel )                                            \
  { nsCOMPtr<nsIURI>   pURIDebug;                                                            \
    nsXPIDLCString     xcsURISpecDebug;                                                      \
    nsCOMPtr<nsIAtom>  pMethodDebug;                                                         \
    nsAutoString       sMethodDebug;                                                         \
    nsCAutoString      csMethodDebug;                                                        \
                                                                                             \
    _httpchannel->GetURI( getter_AddRefs( pURIDebug ) );                                     \
    pURIDebug->GetSpec( getter_Copies( xcsURISpecDebug ) );                                  \
    printf( "P3P:  Getting Document: %s\n", (const char *)xcsURISpecDebug );                 \
                                                                                             \
    pHTTPChannel->GetRequestMethod( getter_AddRefs( pMethodDebug ) );                        \
    pMethodDebug->ToString( sMethodDebug );                                                  \
    csMethodDebug.AssignWithConversion( sMethodDebug );                                      \
    printf( "P3P:    Request Method: %s\n", (const char *)csMethodDebug );                   \
                                                                                             \
    nsCOMPtr<nsISimpleEnumerator>  pHeadersDebug;                                            \
    nsCOMPtr<nsIHTTPHeader>        pHeaderDebug;                                             \
    nsXPIDLCString                 xcsKeyDebug, xcsValueDebug;                               \
    PRBool                         bMoreElementsDebug;                                       \
                                                                                             \
    rv = _httpchannel->GetRequestHeaderEnumerator( getter_AddRefs( pHeadersDebug ) );        \
    if (NS_SUCCEEDED( rv )) {                                                                \
      rv = pHeadersDebug->HasMoreElements(&bMoreElementsDebug );                             \
      while (NS_SUCCEEDED( rv ) && bMoreElementsDebug) {                                     \
        rv = pHeadersDebug->GetNext( getter_AddRefs( pHeaderDebug ) );                       \
        if (NS_SUCCEEDED( rv )) {                                                            \
          rv = pHeaderDebug->GetFieldName( getter_Copies( xcsKeyDebug ) );                   \
          rv = pHeaderDebug->GetValue( getter_Copies( xcsValueDebug ) );                     \
          printf( "P3P:  Header - %s: %s\n", (const char *)xcsKeyDebug,                      \
                                             (const char *)xcsValueDebug );                  \
        }                                                                                    \
        rv = pHeadersDebug->HasMoreElements(&bMoreElementsDebug );                           \
      }                                                                                      \
    }                                                                                        \
  }

#define P3P_PRINT_RESPONSE_HEADERS( _httpchannel )                                           \
  { nsCOMPtr<nsIURI>  pURIDebug;                                                             \
    nsXPIDLCString    xcsURISpecDebug, xcsStatusDebug;                                       \
    PRUint32          uiStatusDebug;                                                         \
                                                                                             \
    _httpchannel->GetURI( getter_AddRefs( pURIDebug ) );                                     \
    pURIDebug->GetSpec( getter_Copies( xcsURISpecDebug ) );                                  \
    printf( "P3P:  Receiving Document: %s\n", (const char *)xcsURISpecDebug );               \
                                                                                             \
    _httpchannel->GetResponseStatus(&uiStatusDebug );                                        \
    _httpchannel->GetResponseString( getter_Copies( xcsStatusDebug ) );                      \
    printf( "P3P:    Response: %i %s\n", uiStatusDebug, (const char *)xcsStatusDebug );      \
                                                                                             \
    nsCOMPtr<nsISimpleEnumerator>  pHeadersDebug;                                            \
    nsCOMPtr<nsIHTTPHeader>        pHeaderDebug;                                             \
    nsXPIDLCString                 xcsKeyDebug, xcsValueDebug;                               \
    PRBool                         bMoreElementsDebug;                                       \
                                                                                             \
    rv = _httpchannel->GetResponseHeaderEnumerator( getter_AddRefs( pHeadersDebug ) );       \
    if (NS_SUCCEEDED( rv )) {                                                                \
      rv = pHeadersDebug->HasMoreElements(&bMoreElementsDebug );                             \
      while (NS_SUCCEEDED( rv ) && bMoreElementsDebug) {                                     \
        rv = pHeadersDebug->GetNext( getter_AddRefs( pHeaderDebug ) );                       \
        if (NS_SUCCEEDED( rv )) {                                                            \
          rv = pHeaderDebug->GetFieldName( getter_Copies( xcsKeyDebug ) );                   \
          rv = pHeaderDebug->GetValue( getter_Copies( xcsValueDebug ) );                     \
          printf( "P3P:  Header - %s: %s\n", (const char *)xcsKeyDebug,                      \
                                             (const char *)xcsValueDebug );                  \
        }                                                                                    \
        rv = pHeadersDebug->HasMoreElements(&bMoreElementsDebug );                           \
      }                                                                                      \
    }                                                                                        \
  }


#endif                                           /* nsP3PDefines_h__          */
