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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _LDAPROT_H
#define _LDAPROT_H

#ifdef __cplusplus
extern "C" {
#endif

#define LDAP_VERSION1	1
#define LDAP_VERSION2	2
#define LDAP_VERSION3	3
#define LDAP_VERSION	LDAP_VERSION3

#define COMPAT20
#define COMPAT30
#if defined(COMPAT20) || defined(COMPAT30)
#define COMPAT
#endif

#define LDAP_URL_PREFIX		"ldap://"
#define LDAP_URL_PREFIX_LEN	7
#define LDAPS_URL_PREFIX	"ldaps://"
#define LDAPS_URL_PREFIX_LEN	8
#define LDAP_REF_STR		"Referral:\n"
#define LDAP_REF_STR_LEN	10

/* 
 * specific LDAP instantiations of BER types we know about
 */

/* general stuff */
#define LDAP_TAG_MESSAGE	0x30L	/* tag is 16 + constructed bit */
#define OLD_LDAP_TAG_MESSAGE	0x10L	/* forgot the constructed bit  */
#define LDAP_TAG_MSGID		0x02L   /* INTEGER */
#define LDAP_TAG_LDAPDN		0x04L	/* OCTET STRING */
#define LDAP_TAG_CONTROLS	0xa0L	/* context specific + constructed + 0 */
#define LDAP_TAG_REFERRAL	0xa3L	/* context specific + constructed + 3 */
#define LDAP_TAG_NEWSUPERIOR	0x80L	/* context specific + primitive + 0 */
#define LDAP_TAG_MRA_OID	0x81L	/* context specific + primitive + 1 */
#define LDAP_TAG_MRA_TYPE	0x82L	/* context specific + primitive + 2 */
#define LDAP_TAG_MRA_VALUE	0x83L	/* context specific + primitive + 3 */
#define LDAP_TAG_MRA_DNATTRS	0x84L	/* context specific + primitive + 4 */
#define LDAP_TAG_EXOP_REQ_OID	0x80L	/* context specific + primitive + 0 */
#define LDAP_TAG_EXOP_REQ_VALUE	0x81L	/* context specific + primitive + 1 */
#define LDAP_TAG_EXOP_RES_OID	0x8aL	/* context specific + primitive + 10 */
#define LDAP_TAG_EXOP_RES_VALUE	0x8bL	/* context specific + primitive + 11 */
#define LDAP_TAG_SK_MATCHRULE   0x80L   /* context specific + primitive + 0 */
#define LDAP_TAG_SK_REVERSE 	0x81L	/* context specific + primitive + 1 */
#define LDAP_TAG_SR_ATTRTYPE    0x80L   /* context specific + primitive + 0 */
#define LDAP_TAG_SASL_RES_CREDS	0x87L	/* context specific + primitive + 7 */
#define LDAP_TAG_VLV_BY_INDEX	0xa0L	/* context specific + constructed + 0 */
#define LDAP_TAG_VLV_BY_VALUE	0x81L	/* context specific + primitive + 1 */
#define LDAP_TAG_PWP_WARNING	0xA0L   /* context specific + constructed */
#define LDAP_TAG_PWP_SECSLEFT	0x80L   /* context specific + primitive */
#define LDAP_TAG_PWP_GRCLOGINS	0x81L   /* context specific + primitive + 1 */
#define LDAP_TAG_PWP_ERROR	0x81L   /* context specific + primitive + 1 */
#define LDAP_TAG_PWDMOD_REQ_ID  0x80L   /* context specific + primitive + 0 */
#define LDAP_TAG_PWDMOD_REQ_OLD 0x81L   /* context specific + primitive + 1 */
#define LDAP_TAG_PWDMOD_REQ_NEW 0x82L   /* context specific + primitive + 2 */
#define LDAP_TAG_PWDMOD_RES_GEN 0x80L   /* context specific + primitive + 0 */

/* possible operations a client can invoke */
#define LDAP_REQ_BIND		0x60L	/* application + constructed + 0 */
#define LDAP_REQ_UNBIND		0x42L	/* application + primitive   + 2 */
#define LDAP_REQ_SEARCH		0x63L	/* application + constructed + 3 */
#define LDAP_REQ_MODIFY		0x66L	/* application + constructed + 6 */
#define LDAP_REQ_ADD		0x68L	/* application + constructed + 8 */
#define LDAP_REQ_DELETE		0x4aL	/* application + primitive   + 10 */
#define LDAP_REQ_MODRDN		0x6cL	/* application + constructed + 12 */
#define LDAP_REQ_MODDN		0x6cL	/* application + constructed + 12 */
#define LDAP_REQ_RENAME		0x6cL	/* application + constructed + 12 */
#define LDAP_REQ_COMPARE	0x6eL	/* application + constructed + 14 */
#define LDAP_REQ_ABANDON	0x50L	/* application + primitive   + 16 */
#define LDAP_REQ_EXTENDED	0x77L	/* application + constructed + 23 */

/* U-M LDAP release 3.0 compatibility stuff */
#define LDAP_REQ_UNBIND_30	0x62L
#define LDAP_REQ_DELETE_30	0x6aL
#define LDAP_REQ_ABANDON_30	0x70L

/*
 * old broken stuff for backwards compatibility - forgot application tag
 * and constructed/primitive bit
 */
#define OLD_LDAP_REQ_BIND	0x00L
#define OLD_LDAP_REQ_UNBIND	0x02L
#define OLD_LDAP_REQ_SEARCH	0x03L
#define OLD_LDAP_REQ_MODIFY	0x06L
#define OLD_LDAP_REQ_ADD	0x08L
#define OLD_LDAP_REQ_DELETE	0x0aL
#define OLD_LDAP_REQ_MODRDN	0x0cL
#define OLD_LDAP_REQ_MODDN	0x0cL
#define OLD_LDAP_REQ_COMPARE	0x0eL
#define OLD_LDAP_REQ_ABANDON	0x10L

/* old broken stuff for backwards compatibility */
#define OLD_LDAP_RES_BIND		0x01L
#define OLD_LDAP_RES_SEARCH_ENTRY	0x04L
#define OLD_LDAP_RES_SEARCH_RESULT	0x05L
#define OLD_LDAP_RES_MODIFY		0x07L
#define OLD_LDAP_RES_ADD		0x09L
#define OLD_LDAP_RES_DELETE		0x0bL
#define OLD_LDAP_RES_MODRDN		0x0dL
#define OLD_LDAP_RES_MODDN		0x0dL
#define OLD_LDAP_RES_COMPARE		0x0fL

/* U-M LDAP 3.0 compatibility auth methods */
#define LDAP_AUTH_SIMPLE_30	0xa0L	/* context specific + constructed */
#define LDAP_AUTH_KRBV41_30	0xa1L	/* context specific + constructed */
#define LDAP_AUTH_KRBV42_30	0xa2L	/* context specific + constructed */

/* old broken stuff */
#define OLD_LDAP_AUTH_SIMPLE	0x00L
#define OLD_LDAP_AUTH_KRBV4	0x01L
#define OLD_LDAP_AUTH_KRBV42	0x02L

/* U-M LDAP 3.0 compatibility filter types */
#define LDAP_FILTER_PRESENT_30	0xa7L	/* context specific + constructed */

/* filter types */
#define LDAP_FILTER_AND		0xa0L	/* context specific + constructed + 0 */
#define LDAP_FILTER_OR		0xa1L	/* context specific + constructed + 1 */
#define LDAP_FILTER_NOT		0xa2L	/* context specific + constructed + 2 */
#define LDAP_FILTER_EQUALITY	0xa3L	/* context specific + constructed + 3 */
#define LDAP_FILTER_SUBSTRINGS	0xa4L	/* context specific + constructed + 4 */
#define LDAP_FILTER_GE		0xa5L	/* context specific + constructed + 5 */
#define LDAP_FILTER_LE		0xa6L	/* context specific + constructed + 6 */
#define LDAP_FILTER_PRESENT	0x87L	/* context specific + primitive   + 7 */
#define LDAP_FILTER_APPROX	0xa8L	/* context specific + constructed + 8 */
#define LDAP_FILTER_EXTENDED	0xa9L	/* context specific + constructed + 0 */

/* old broken stuff */
#define OLD_LDAP_FILTER_AND		0x00L
#define OLD_LDAP_FILTER_OR		0x01L
#define OLD_LDAP_FILTER_NOT		0x02L
#define OLD_LDAP_FILTER_EQUALITY	0x03L
#define OLD_LDAP_FILTER_SUBSTRINGS	0x04L
#define OLD_LDAP_FILTER_GE		0x05L
#define OLD_LDAP_FILTER_LE		0x06L
#define OLD_LDAP_FILTER_PRESENT		0x07L
#define OLD_LDAP_FILTER_APPROX		0x08L

/* substring filter component types */
#define LDAP_SUBSTRING_INITIAL	0x80L	/* context specific + primitive + 0 */
#define LDAP_SUBSTRING_ANY	0x81L	/* context specific + primitive + 1 */
#define LDAP_SUBSTRING_FINAL	0x82L	/* context specific + primitive + 2 */

/* extended filter component types */
#define LDAP_FILTER_EXTENDED_OID	0x81L	/* context spec. + prim. + 1 */
#define LDAP_FILTER_EXTENDED_TYPE	0x82L	/* context spec. + prim. + 2 */
#define LDAP_FILTER_EXTENDED_VALUE	0x83L	/* context spec. + prim. + 3 */
#define LDAP_FILTER_EXTENDED_DNATTRS	0x84L	/* context spec. + prim. + 4 */

/* U-M LDAP 3.0 compatibility substring filter component types */
#define LDAP_SUBSTRING_INITIAL_30	0xa0L	/* context specific */
#define LDAP_SUBSTRING_ANY_30		0xa1L	/* context specific */
#define LDAP_SUBSTRING_FINAL_30		0xa2L	/* context specific */

/* old broken stuff */
#define OLD_LDAP_SUBSTRING_INITIAL	0x00L
#define OLD_LDAP_SUBSTRING_ANY		0x01L
#define OLD_LDAP_SUBSTRING_FINAL	0x02L

#ifdef __cplusplus
}
#endif
#endif /* _LDAPROT_H */
