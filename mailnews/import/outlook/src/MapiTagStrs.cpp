/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
/*
 *  Message envelope properties
 */

case PR_ACKNOWLEDGEMENT_MODE:
	s = "PR_ACKNOWLEDGEMENT_MODE"; break;
case PR_ALTERNATE_RECIPIENT_ALLOWED:
	s = "PR_ALTERNATE_RECIPIENT_ALLOWED"; break;
case PR_AUTHORIZING_USERS:
	s = "PR_AUTHORIZING_USERS"; break;
case PR_AUTO_FORWARD_COMMENT:
	s = "PR_AUTO_FORWARD_COMMENT"; break;
case PR_AUTO_FORWARDED:
	s = "PR_AUTO_FORWARDED"; break;
case PR_CONTENT_CONFIDENTIALITY_ALGORITHM_ID:
	s = "PR_CONTENT_CONFIDENTIALITY_ALGORITHM_ID"; break;
case PR_CONTENT_CORRELATOR:
	s = "PR_CONTENT_CORRELATOR"; break;
case PR_CONTENT_IDENTIFIER:
	s = "PR_CONTENT_IDENTIFIER"; break;
case PR_CONTENT_LENGTH:
	s = "PR_CONTENT_LENGTH"; break;
case PR_CONTENT_RETURN_REQUESTED:
	s = "PR_CONTENT_RETURN_REQUESTED"; break;



case PR_CONVERSATION_KEY:
	s = "PR_CONVERSATION_KEY"; break;

case PR_CONVERSION_EITS:
	s = "PR_CONVERSION_EITS"; break;
case PR_CONVERSION_WITH_LOSS_PROHIBITED:
	s = "PR_CONVERSION_WITH_LOSS_PROHIBITED"; break;
case PR_CONVERTED_EITS:
	s = "PR_CONVERTED_EITS"; break;
case PR_DEFERRED_DELIVERY_TIME:
	s = "PR_DEFERRED_DELIVERY_TIME"; break;
case PR_DELIVER_TIME:
	s = "PR_DELIVER_TIME"; break;
case PR_DISCARD_REASON:
	s = "PR_DISCARD_REASON"; break;
case PR_DISCLOSURE_OF_RECIPIENTS:
	s = "PR_DISCLOSURE_OF_RECIPIENTS"; break;
case PR_DL_EXPANSION_HISTORY:
	s = "PR_DL_EXPANSION_HISTORY"; break;
case PR_DL_EXPANSION_PROHIBITED:
	s = "PR_DL_EXPANSION_PROHIBITED"; break;
case PR_EXPIRY_TIME:
	s = "PR_EXPIRY_TIME"; break;
case PR_IMPLICIT_CONVERSION_PROHIBITED:
	s = "PR_IMPLICIT_CONVERSION_PROHIBITED"; break;
case PR_IMPORTANCE:
	s = "PR_IMPORTANCE"; break;
case PR_IPM_ID:
	s = "PR_IPM_ID"; break;
case PR_LATEST_DELIVERY_TIME:
	s = "PR_LATEST_DELIVERY_TIME"; break;
case PR_MESSAGE_CLASS:
	s = "PR_MESSAGE_CLASS"; break;
case PR_MESSAGE_DELIVERY_ID:
	s = "PR_MESSAGE_DELIVERY_ID"; break;





case PR_MESSAGE_SECURITY_LABEL:
	s = "PR_MESSAGE_SECURITY_LABEL"; break;
case PR_OBSOLETED_IPMS:
	s = "PR_OBSOLETED_IPMS"; break;
case PR_ORIGINALLY_INTENDED_RECIPIENT_NAME:
	s = "PR_ORIGINALLY_INTENDED_RECIPIENT_NAME"; break;
case PR_ORIGINAL_EITS:
	s = "PR_ORIGINAL_EITS"; break;
case PR_ORIGINATOR_CERTIFICATE:
	s = "PR_ORIGINATOR_CERTIFICATE"; break;
case PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED:
	s = "PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED"; break;
case PR_ORIGINATOR_RETURN_ADDRESS:
	s = "PR_ORIGINATOR_RETURN_ADDRESS"; break;



case PR_PARENT_KEY:
	s = "PR_PARENT_KEY"; break;
case PR_PRIORITY:
	s = "PR_PRIORITY"; break;



case PR_ORIGIN_CHECK:
	s = "PR_ORIGIN_CHECK"; break;
case PR_PROOF_OF_SUBMISSION_REQUESTED:
	s = "PR_PROOF_OF_SUBMISSION_REQUESTED"; break;
case PR_READ_RECEIPT_REQUESTED:
	s = "PR_READ_RECEIPT_REQUESTED"; break;
case PR_RECEIPT_TIME:
	s = "PR_RECEIPT_TIME"; break;
case PR_RECIPIENT_REASSIGNMENT_PROHIBITED:
	s = "PR_RECIPIENT_REASSIGNMENT_PROHIBITED"; break;
case PR_REDIRECTION_HISTORY:
	s = "PR_REDIRECTION_HISTORY"; break;
case PR_RELATED_IPMS:
	s = "PR_RELATED_IPMS"; break;
case PR_ORIGINAL_SENSITIVITY:
	s = "PR_ORIGINAL_SENSITIVITY"; break;
case PR_LANGUAGES:
	s = "PR_LANGUAGES"; break;
case PR_REPLY_TIME:
	s = "PR_REPLY_TIME"; break;
case PR_REPORT_TAG:
	s = "PR_REPORT_TAG"; break;
case PR_REPORT_TIME:
	s = "PR_REPORT_TIME"; break;
case PR_RETURNED_IPM:
	s = "PR_RETURNED_IPM"; break;
case PR_SECURITY:
	s = "PR_SECURITY"; break;
case PR_INCOMPLETE_COPY:
	s = "PR_INCOMPLETE_COPY"; break;
case PR_SENSITIVITY:
	s = "PR_SENSITIVITY"; break;
case PR_SUBJECT:
	s = "PR_SUBJECT"; break;
case PR_SUBJECT_IPM:
	s = "PR_SUBJECT_IPM"; break;
case PR_CLIENT_SUBMIT_TIME:
	s = "PR_CLIENT_SUBMIT_TIME"; break;
case PR_REPORT_NAME:
	s = "PR_REPORT_NAME"; break;
case PR_SENT_REPRESENTING_SEARCH_KEY:
	s = "PR_SENT_REPRESENTING_SEARCH_KEY"; break;
case PR_X400_CONTENT_TYPE:
	s = "PR_X400_CONTENT_TYPE"; break;
case PR_SUBJECT_PREFIX:
	s = "PR_SUBJECT_PREFIX"; break;
case PR_NON_RECEIPT_REASON:
	s = "PR_NON_RECEIPT_REASON"; break;
case PR_RECEIVED_BY_ENTRYID:
	s = "PR_RECEIVED_BY_ENTRYID"; break;
case PR_RECEIVED_BY_NAME:
	s = "PR_RECEIVED_BY_NAME"; break;
case PR_SENT_REPRESENTING_ENTRYID:
	s = "PR_SENT_REPRESENTING_ENTRYID"; break;
case PR_SENT_REPRESENTING_NAME:
	s = "PR_SENT_REPRESENTING_NAME"; break;
case PR_RCVD_REPRESENTING_ENTRYID:
	s = "PR_RCVD_REPRESENTING_ENTRYID"; break;
case PR_RCVD_REPRESENTING_NAME:
	s = "PR_RCVD_REPRESENTING_NAME"; break;
case PR_REPORT_ENTRYID:
	s = "PR_REPORT_ENTRYID"; break;
case PR_READ_RECEIPT_ENTRYID:
	s = "PR_READ_RECEIPT_ENTRYID"; break;
case PR_MESSAGE_SUBMISSION_ID:
	s = "PR_MESSAGE_SUBMISSION_ID"; break;
case PR_PROVIDER_SUBMIT_TIME:
	s = "PR_PROVIDER_SUBMIT_TIME"; break;
case PR_ORIGINAL_SUBJECT:
	s = "PR_ORIGINAL_SUBJECT"; break;
case PR_DISC_VAL:
	s = "PR_DISC_VAL"; break;
case PR_ORIG_MESSAGE_CLASS:
	s = "PR_ORIG_MESSAGE_CLASS"; break;
case PR_ORIGINAL_AUTHOR_ENTRYID:
	s = "PR_ORIGINAL_AUTHOR_ENTRYID"; break;
case PR_ORIGINAL_AUTHOR_NAME:
	s = "PR_ORIGINAL_AUTHOR_NAME"; break;
case PR_ORIGINAL_SUBMIT_TIME:
	s = "PR_ORIGINAL_SUBMIT_TIME"; break;
case PR_REPLY_RECIPIENT_ENTRIES:
	s = "PR_REPLY_RECIPIENT_ENTRIES"; break;
case PR_REPLY_RECIPIENT_NAMES:
	s = "PR_REPLY_RECIPIENT_NAMES"; break;

case PR_RECEIVED_BY_SEARCH_KEY:
	s = "PR_RECEIVED_BY_SEARCH_KEY"; break;
case PR_RCVD_REPRESENTING_SEARCH_KEY:
	s = "PR_RCVD_REPRESENTING_SEARCH_KEY"; break;
case PR_READ_RECEIPT_SEARCH_KEY:
	s = "PR_READ_RECEIPT_SEARCH_KEY"; break;
case PR_REPORT_SEARCH_KEY:
	s = "PR_REPORT_SEARCH_KEY"; break;
case PR_ORIGINAL_DELIVERY_TIME:
	s = "PR_ORIGINAL_DELIVERY_TIME"; break;
case PR_ORIGINAL_AUTHOR_SEARCH_KEY:
	s = "PR_ORIGINAL_AUTHOR_SEARCH_KEY"; break;

case PR_MESSAGE_TO_ME:
	s = "PR_MESSAGE_TO_ME"; break;
case PR_MESSAGE_CC_ME:
	s = "PR_MESSAGE_CC_ME"; break;
case PR_MESSAGE_RECIP_ME:
	s = "PR_MESSAGE_RECIP_ME"; break;

case PR_ORIGINAL_SENDER_NAME:
	s = "PR_ORIGINAL_SENDER_NAME"; break;
case PR_ORIGINAL_SENDER_ENTRYID:
	s = "PR_ORIGINAL_SENDER_ENTRYID"; break;
case PR_ORIGINAL_SENDER_SEARCH_KEY:
	s = "PR_ORIGINAL_SENDER_SEARCH_KEY"; break;
case PR_ORIGINAL_SENT_REPRESENTING_NAME:
	s = "PR_ORIGINAL_SENT_REPRESENTING_NAME"; break;
case PR_ORIGINAL_SENT_REPRESENTING_ENTRYID:
	s = "PR_ORIGINAL_SENT_REPRESENTING_ENTRYID"; break;
case PR_ORIGINAL_SENT_REPRESENTING_SEARCH_KEY:
	s = "PR_ORIGINAL_SENT_REPRESENTING_SEARCH_KEY"; break;

case PR_START_DATE:
	s = "PR_START_DATE"; break;
case PR_END_DATE:
	s = "PR_END_DATE"; break;
case PR_OWNER_APPT_ID:
	s = "PR_OWNER_APPT_ID"; break;
case PR_RESPONSE_REQUESTED:
	s = "PR_RESPONSE_REQUESTED"; break;

case PR_SENT_REPRESENTING_ADDRTYPE:
	s = "PR_SENT_REPRESENTING_ADDRTYPE"; break;
case PR_SENT_REPRESENTING_EMAIL_ADDRESS:
	s = "PR_SENT_REPRESENTING_EMAIL_ADDRESS"; break;

case PR_ORIGINAL_SENDER_ADDRTYPE:
	s = "PR_ORIGINAL_SENDER_ADDRTYPE"; break;
case PR_ORIGINAL_SENDER_EMAIL_ADDRESS:
	s = "PR_ORIGINAL_SENDER_EMAIL_ADDRESS"; break;

case PR_ORIGINAL_SENT_REPRESENTING_ADDRTYPE:
	s = "PR_ORIGINAL_SENT_REPRESENTING_ADDRTYPE"; break;
case PR_ORIGINAL_SENT_REPRESENTING_EMAIL_ADDRESS:
	s = "PR_ORIGINAL_SENT_REPRESENTING_EMAIL_ADDRESS"; break;

case PR_CONVERSATION_TOPIC:
	s = "PR_CONVERSATION_TOPIC"; break;
case PR_CONVERSATION_INDEX:
	s = "PR_CONVERSATION_INDEX"; break;

case PR_ORIGINAL_DISPLAY_BCC:
	s = "PR_ORIGINAL_DISPLAY_BCC"; break;
case PR_ORIGINAL_DISPLAY_CC:
	s = "PR_ORIGINAL_DISPLAY_CC"; break;
case PR_ORIGINAL_DISPLAY_TO:
	s = "PR_ORIGINAL_DISPLAY_TO"; break;

case PR_RECEIVED_BY_ADDRTYPE:
	s = "PR_RECEIVED_BY_ADDRTYPE"; break;
case PR_RECEIVED_BY_EMAIL_ADDRESS:
	s = "PR_RECEIVED_BY_EMAIL_ADDRESS"; break;

case PR_RCVD_REPRESENTING_ADDRTYPE:
	s = "PR_RCVD_REPRESENTING_ADDRTYPE"; break;
case PR_RCVD_REPRESENTING_EMAIL_ADDRESS:
	s = "PR_RCVD_REPRESENTING_EMAIL_ADDRESS"; break;

case PR_ORIGINAL_AUTHOR_ADDRTYPE:
	s = "PR_ORIGINAL_AUTHOR_ADDRTYPE"; break;
case PR_ORIGINAL_AUTHOR_EMAIL_ADDRESS:
	s = "PR_ORIGINAL_AUTHOR_EMAIL_ADDRESS"; break;

case PR_ORIGINALLY_INTENDED_RECIP_ADDRTYPE:
	s = "PR_ORIGINALLY_INTENDED_RECIP_ADDRTYPE"; break;
case PR_ORIGINALLY_INTENDED_RECIP_EMAIL_ADDRESS:
	s = "PR_ORIGINALLY_INTENDED_RECIP_EMAIL_ADDRESS"; break;

case PR_TRANSPORT_MESSAGE_HEADERS:
	s = "PR_TRANSPORT_MESSAGE_HEADERS"; break;

case PR_DELEGATION:
	s = "PR_DELEGATION"; break;

case PR_TNEF_CORRELATION_KEY:
	s = "PR_TNEF_CORRELATION_KEY"; break;



/*
 *  Message content properties
 */

case PR_BODY:
	s = "PR_BODY"; break;
case PR_REPORT_TEXT:
	s = "PR_REPORT_TEXT"; break;
case PR_ORIGINATOR_AND_DL_EXPANSION_HISTORY:
	s = "PR_ORIGINATOR_AND_DL_EXPANSION_HISTORY"; break;
case PR_REPORTING_DL_NAME:
	s = "PR_REPORTING_DL_NAME"; break;
case PR_REPORTING_MTA_CERTIFICATE:
	s = "PR_REPORTING_MTA_CERTIFICATE"; break;

/*  Removed PR_REPORT_ORIGIN_AUTHENTICATION_CHECK with DCR 3865, use PR_ORIGIN_CHECK */

case PR_RTF_SYNC_BODY_CRC:
	s = "PR_RTF_SYNC_BODY_CRC"; break;
case PR_RTF_SYNC_BODY_COUNT:
	s = "PR_RTF_SYNC_BODY_COUNT"; break;
case PR_RTF_SYNC_BODY_TAG:
	s = "PR_RTF_SYNC_BODY_TAG"; break;
case PR_RTF_COMPRESSED:
	s = "PR_RTF_COMPRESSED"; break;
case PR_RTF_SYNC_PREFIX_COUNT:
	s = "PR_RTF_SYNC_PREFIX_COUNT"; break;
case PR_RTF_SYNC_TRAILING_COUNT:
	s = "PR_RTF_SYNC_TRAILING_COUNT"; break;
case PR_ORIGINALLY_INTENDED_RECIP_ENTRYID:
	s = "PR_ORIGINALLY_INTENDED_RECIP_ENTRYID"; break;

/*
 *  Reserved 0x1100-0x1200
 */


/*
 *  Message recipient properties
 */

case PR_CONTENT_INTEGRITY_CHECK:
	s = "PR_CONTENT_INTEGRITY_CHECK"; break;
case PR_EXPLICIT_CONVERSION:
	s = "PR_EXPLICIT_CONVERSION"; break;
case PR_IPM_RETURN_REQUESTED:
	s = "PR_IPM_RETURN_REQUESTED"; break;
case PR_MESSAGE_TOKEN:
	s = "PR_MESSAGE_TOKEN"; break;
case PR_NDR_REASON_CODE:
	s = "PR_NDR_REASON_CODE"; break;
case PR_NDR_DIAG_CODE:
	s = "PR_NDR_DIAG_CODE"; break;
case PR_NON_RECEIPT_NOTIFICATION_REQUESTED:
	s = "PR_NON_RECEIPT_NOTIFICATION_REQUESTED"; break;
case PR_DELIVERY_POINT:
	s = "PR_DELIVERY_POINT"; break;

case PR_ORIGINATOR_NON_DELIVERY_REPORT_REQUESTED:
	s = "PR_ORIGINATOR_NON_DELIVERY_REPORT_REQUESTED"; break;
case PR_ORIGINATOR_REQUESTED_ALTERNATE_RECIPIENT:
	s = "PR_ORIGINATOR_REQUESTED_ALTERNATE_RECIPIENT"; break;
case PR_PHYSICAL_DELIVERY_BUREAU_FAX_DELIVERY:
	s = "PR_PHYSICAL_DELIVERY_BUREAU_FAX_DELIVERY"; break;
case PR_PHYSICAL_DELIVERY_MODE:
	s = "PR_PHYSICAL_DELIVERY_MODE"; break;
case PR_PHYSICAL_DELIVERY_REPORT_REQUEST:
	s = "PR_PHYSICAL_DELIVERY_REPORT_REQUEST"; break;
case PR_PHYSICAL_FORWARDING_ADDRESS:
	s = "PR_PHYSICAL_FORWARDING_ADDRESS"; break;
case PR_PHYSICAL_FORWARDING_ADDRESS_REQUESTED:
	s = "PR_PHYSICAL_FORWARDING_ADDRESS_REQUESTED"; break;
case PR_PHYSICAL_FORWARDING_PROHIBITED:
	s = "PR_PHYSICAL_FORWARDING_PROHIBITED"; break;
case PR_PHYSICAL_RENDITION_ATTRIBUTES:
	s = "PR_PHYSICAL_RENDITION_ATTRIBUTES"; break;
case PR_PROOF_OF_DELIVERY:
	s = "PR_PROOF_OF_DELIVERY"; break;
case PR_PROOF_OF_DELIVERY_REQUESTED:
	s = "PR_PROOF_OF_DELIVERY_REQUESTED"; break;
case PR_RECIPIENT_CERTIFICATE:
	s = "PR_RECIPIENT_CERTIFICATE"; break;
case PR_RECIPIENT_NUMBER_FOR_ADVICE:
	s = "PR_RECIPIENT_NUMBER_FOR_ADVICE"; break;
case PR_RECIPIENT_TYPE:
	s = "PR_RECIPIENT_TYPE"; break;
case PR_REGISTERED_MAIL_TYPE:
	s = "PR_REGISTERED_MAIL_TYPE"; break;
case PR_REPLY_REQUESTED:
	s = "PR_REPLY_REQUESTED"; break;
case PR_REQUESTED_DELIVERY_METHOD:
	s = "PR_REQUESTED_DELIVERY_METHOD"; break;
case PR_SENDER_ENTRYID:
	s = "PR_SENDER_ENTRYID"; break;
case PR_SENDER_NAME:
	s = "PR_SENDER_NAME"; break;
case PR_SUPPLEMENTARY_INFO:
	s = "PR_SUPPLEMENTARY_INFO"; break;
case PR_TYPE_OF_MTS_USER:
	s = "PR_TYPE_OF_MTS_USER"; break;
case PR_SENDER_SEARCH_KEY:
	s = "PR_SENDER_SEARCH_KEY"; break;
case PR_SENDER_ADDRTYPE:
	s = "PR_SENDER_ADDRTYPE"; break;
case PR_SENDER_EMAIL_ADDRESS:
	s = "PR_SENDER_EMAIL_ADDRESS"; break;

/*
 *  Message non-transmittable properties
 */

/*
 * The two tags, PR_MESSAGE_RECIPIENTS and PR_MESSAGE_ATTACHMENTS,
 * are to be used in the exclude list passed to
 * IMessage::CopyTo when the caller wants either the recipients or attachments
 * of the message to not get copied.  It is also used in the ProblemArray
 * return from IMessage::CopyTo when an error is encountered copying them
 */

case PR_CURRENT_VERSION:
	s = "PR_CURRENT_VERSION"; break;
case PR_DELETE_AFTER_SUBMIT:
	s = "PR_DELETE_AFTER_SUBMIT"; break;
case PR_DISPLAY_BCC:
	s = "PR_DISPLAY_BCC"; break;
case PR_DISPLAY_CC:
	s = "PR_DISPLAY_CC"; break;
case PR_DISPLAY_TO:
	s = "PR_DISPLAY_TO"; break;
case PR_PARENT_DISPLAY:
	s = "PR_PARENT_DISPLAY"; break;
case PR_MESSAGE_DELIVERY_TIME:
	s = "PR_MESSAGE_DELIVERY_TIME"; break;
case PR_MESSAGE_FLAGS:
	s = "PR_MESSAGE_FLAGS"; break;
case PR_MESSAGE_SIZE:
	s = "PR_MESSAGE_SIZE"; break;
case PR_PARENT_ENTRYID:
	s = "PR_PARENT_ENTRYID"; break;
case PR_SENTMAIL_ENTRYID:
	s = "PR_SENTMAIL_ENTRYID"; break;
case PR_CORRELATE:
	s = "PR_CORRELATE"; break;
case PR_CORRELATE_MTSID:
	s = "PR_CORRELATE_MTSID"; break;
case PR_DISCRETE_VALUES:
	s = "PR_DISCRETE_VALUES"; break;
case PR_RESPONSIBILITY:
	s = "PR_RESPONSIBILITY"; break;
case PR_SPOOLER_STATUS:
	s = "PR_SPOOLER_STATUS"; break;
case PR_TRANSPORT_STATUS:
	s = "PR_TRANSPORT_STATUS"; break;
case PR_MESSAGE_RECIPIENTS:
	s = "PR_MESSAGE_RECIPIENTS"; break;
case PR_MESSAGE_ATTACHMENTS:
	s = "PR_MESSAGE_ATTACHMENTS"; break;
case PR_SUBMIT_FLAGS:
	s = "PR_SUBMIT_FLAGS"; break;
case PR_RECIPIENT_STATUS:
	s = "PR_RECIPIENT_STATUS"; break;
case PR_TRANSPORT_KEY:
	s = "PR_TRANSPORT_KEY"; break;
case PR_MSG_STATUS:
	s = "PR_MSG_STATUS"; break;
case PR_MESSAGE_DOWNLOAD_TIME:
	s = "PR_MESSAGE_DOWNLOAD_TIME"; break;
case PR_CREATION_VERSION:
	s = "PR_CREATION_VERSION"; break;
case PR_MODIFY_VERSION:
	s = "PR_MODIFY_VERSION"; break;
case PR_HASATTACH:
	s = "PR_HASATTACH"; break;
case PR_BODY_CRC:
	s = "PR_BODY_CRC"; break;
case PR_NORMALIZED_SUBJECT:
	s = "PR_NORMALIZED_SUBJECT"; break;
case PR_RTF_IN_SYNC:
	s = "PR_RTF_IN_SYNC"; break;
case PR_ATTACH_SIZE:
	s = "PR_ATTACH_SIZE"; break;
case PR_ATTACH_NUM:
	s = "PR_ATTACH_NUM"; break;
case PR_PREPROCESS:
	s = "PR_PREPROCESS"; break;

/* PR_ORIGINAL_DISPLAY_TO, _CC, and _BCC moved to transmittible range 03/09/95 */

case PR_ORIGINATING_MTA_CERTIFICATE:
	s = "PR_ORIGINATING_MTA_CERTIFICATE"; break;
case PR_PROOF_OF_SUBMISSION:
	s = "PR_PROOF_OF_SUBMISSION"; break;


/*
 * The range of non-message and non-recipient property IDs (0x3000 - 0x3FFF) is
 * further broken down into ranges to make assigning new property IDs easier.
 *
 *  From    To      Kind of property
 *  --------------------------------
 *  3000    32FF    MAPI_defined common property
 *  3200    33FF    MAPI_defined form property
 *  3400    35FF    MAPI_defined message store property
 *  3600    36FF    MAPI_defined Folder or AB Container property
 *  3700    38FF    MAPI_defined attachment property
 *  3900    39FF    MAPI_defined address book property
 *  3A00    3BFF    MAPI_defined mailuser property
 *  3C00    3CFF    MAPI_defined DistList property
 *  3D00    3DFF    MAPI_defined Profile Section property
 *  3E00    3EFF    MAPI_defined Status property
 *  3F00    3FFF    MAPI_defined display table property
 */

/*
 *  Properties common to numerous MAPI objects.
 *
 *  Those properties that can appear on messages are in the
 *  non-transmittable range for messages. They start at the high
 *  end of that range and work down.
 *
 *  Properties that never appear on messages are defined in the common
 *  property range (see above).
 */

/*
 * properties that are common to multiple objects (including message objects)
 * -- these ids are in the non-transmittable range
 */

case PR_ENTRYID:
	s = "PR_ENTRYID"; break;
case PR_OBJECT_TYPE:
	s = "PR_OBJECT_TYPE"; break;
case PR_ICON:
	s = "PR_ICON"; break;
case PR_MINI_ICON:
	s = "PR_MINI_ICON"; break;
case PR_STORE_ENTRYID:
	s = "PR_STORE_ENTRYID"; break;
case PR_STORE_RECORD_KEY:
	s = "PR_STORE_RECORD_KEY"; break;
case PR_RECORD_KEY:
	s = "PR_RECORD_KEY"; break;
case PR_MAPPING_SIGNATURE:
	s = "PR_MAPPING_SIGNATURE"; break;
case PR_ACCESS_LEVEL:
	s = "PR_ACCESS_LEVEL"; break;
case PR_INSTANCE_KEY:
	s = "PR_INSTANCE_KEY"; break;
case PR_ROW_TYPE:
	s = "PR_ROW_TYPE"; break;
case PR_ACCESS:
	s = "PR_ACCESS"; break;

/*
 * properties that are common to multiple objects (usually not including message objects)
 * -- these ids are in the transmittable range
 */

case PR_ROWID:
	s = "PR_ROWID"; break;
case PR_DISPLAY_NAME:
	s = "PR_DISPLAY_NAME"; break;
case PR_ADDRTYPE:
	s = "PR_ADDRTYPE"; break;
case PR_EMAIL_ADDRESS:
	s = "PR_EMAIL_ADDRESS"; break;
case PR_COMMENT:
	s = "PR_COMMENT"; break;
case PR_DEPTH:
	s = "PR_DEPTH"; break;
case PR_PROVIDER_DISPLAY:
	s = "PR_PROVIDER_DISPLAY"; break;
case PR_CREATION_TIME:
	s = "PR_CREATION_TIME"; break;
case PR_LAST_MODIFICATION_TIME:
	s = "PR_LAST_MODIFICATION_TIME"; break;
case PR_RESOURCE_FLAGS:
	s = "PR_RESOURCE_FLAGS"; break;
case PR_PROVIDER_DLL_NAME:
	s = "PR_PROVIDER_DLL_NAME"; break;
case PR_SEARCH_KEY:
	s = "PR_SEARCH_KEY"; break;
case PR_PROVIDER_UID:
	s = "PR_PROVIDER_UID"; break;
case PR_PROVIDER_ORDINAL:
	s = "PR_PROVIDER_ORDINAL"; break;

/*
 *  MAPI Form properties
 */
case PR_FORM_VERSION:
	s = "PR_FORM_VERSION"; break;
case PR_FORM_CLSID:
	s = "PR_FORM_CLSID"; break;
case PR_FORM_CONTACT_NAME:
	s = "PR_FORM_CONTACT_NAME"; break;
case PR_FORM_CATEGORY:
	s = "PR_FORM_CATEGORY"; break;
case PR_FORM_CATEGORY_SUB:
	s = "PR_FORM_CATEGORY_SUB"; break;
case PR_FORM_HOST_MAP:
	s = "PR_FORM_HOST_MAP"; break;
case PR_FORM_HIDDEN:
	s = "PR_FORM_HIDDEN"; break;
case PR_FORM_DESIGNER_NAME:
	s = "PR_FORM_DESIGNER_NAME"; break;
case PR_FORM_DESIGNER_GUID:
	s = "PR_FORM_DESIGNER_GUID"; break;
case PR_FORM_MESSAGE_BEHAVIOR:
	s = "PR_FORM_MESSAGE_BEHAVIOR"; break;

/*
 *  Message store properties
 */

case PR_DEFAULT_STORE:
	s = "PR_DEFAULT_STORE"; break;
case PR_STORE_SUPPORT_MASK:
	s = "PR_STORE_SUPPORT_MASK"; break;
case PR_STORE_STATE:
	s = "PR_STORE_STATE"; break;

case PR_IPM_SUBTREE_SEARCH_KEY:
	s = "PR_IPM_SUBTREE_SEARCH_KEY"; break;
case PR_IPM_OUTBOX_SEARCH_KEY:
	s = "PR_IPM_OUTBOX_SEARCH_KEY"; break;
case PR_IPM_WASTEBASKET_SEARCH_KEY:
	s = "PR_IPM_WASTEBASKET_SEARCH_KEY"; break;
case PR_IPM_SENTMAIL_SEARCH_KEY:
	s = "PR_IPM_SENTMAIL_SEARCH_KEY"; break;
case PR_MDB_PROVIDER:
	s = "PR_MDB_PROVIDER"; break;
case PR_RECEIVE_FOLDER_SETTINGS:
	s = "PR_RECEIVE_FOLDER_SETTINGS"; break;

case PR_VALID_FOLDER_MASK:
	s = "PR_VALID_FOLDER_MASK"; break;
case PR_IPM_SUBTREE_ENTRYID:
	s = "PR_IPM_SUBTREE_ENTRYID"; break;

case PR_IPM_OUTBOX_ENTRYID:
	s = "PR_IPM_OUTBOX_ENTRYID"; break;
case PR_IPM_WASTEBASKET_ENTRYID:
	s = "PR_IPM_WASTEBASKET_ENTRYID"; break;
case PR_IPM_SENTMAIL_ENTRYID:
	s = "PR_IPM_SENTMAIL_ENTRYID"; break;
case PR_VIEWS_ENTRYID:
	s = "PR_VIEWS_ENTRYID"; break;
case PR_COMMON_VIEWS_ENTRYID:
	s = "PR_COMMON_VIEWS_ENTRYID"; break;
case PR_FINDER_ENTRYID:
	s = "PR_FINDER_ENTRYID"; break;

/* Proptags 0x35E8-0x35FF reserved for folders "guaranteed" by PR_VALID_FOLDER_MASK */


/*
 *  Folder and AB Container properties
 */

case PR_CONTAINER_FLAGS:
	s = "PR_CONTAINER_FLAGS"; break;
case PR_FOLDER_TYPE:
	s = "PR_FOLDER_TYPE"; break;
case PR_CONTENT_COUNT:
	s = "PR_CONTENT_COUNT"; break;
case PR_CONTENT_UNREAD:
	s = "PR_CONTENT_UNREAD"; break;
case PR_CREATE_TEMPLATES:
	s = "PR_CREATE_TEMPLATES"; break;
case PR_DETAILS_TABLE:
	s = "PR_DETAILS_TABLE"; break;
case PR_SEARCH:
	s = "PR_SEARCH"; break;
case PR_SELECTABLE:
	s = "PR_SELECTABLE"; break;
case PR_SUBFOLDERS:
	s = "PR_SUBFOLDERS"; break;
case PR_STATUS:
	s = "PR_STATUS"; break;
case PR_ANR:
	s = "PR_ANR"; break;
case PR_CONTENTS_SORT_ORDER:
	s = "PR_CONTENTS_SORT_ORDER"; break;
case PR_CONTAINER_HIERARCHY:
	s = "PR_CONTAINER_HIERARCHY"; break;
case PR_CONTAINER_CONTENTS:
	s = "PR_CONTAINER_CONTENTS"; break;
case PR_FOLDER_ASSOCIATED_CONTENTS:
	s = "PR_FOLDER_ASSOCIATED_CONTENTS"; break;
case PR_DEF_CREATE_DL:
	s = "PR_DEF_CREATE_DL"; break;
case PR_DEF_CREATE_MAILUSER:
	s = "PR_DEF_CREATE_MAILUSER"; break;
case PR_CONTAINER_CLASS:
	s = "PR_CONTAINER_CLASS"; break;
case PR_CONTAINER_MODIFY_VERSION:
	s = "PR_CONTAINER_MODIFY_VERSION"; break;
case PR_AB_PROVIDER_ID:
	s = "PR_AB_PROVIDER_ID"; break;
case PR_DEFAULT_VIEW_ENTRYID:
	s = "PR_DEFAULT_VIEW_ENTRYID"; break;
case PR_ASSOC_CONTENT_COUNT:
	s = "PR_ASSOC_CONTENT_COUNT"; break;

/* Reserved 0x36C0-0x36FF */

/*
 *  Attachment properties
 */

case PR_ATTACHMENT_X400_PARAMETERS:
	s = "PR_ATTACHMENT_X400_PARAMETERS"; break;
case PR_ATTACH_DATA_OBJ:
	s = "PR_ATTACH_DATA_OBJ"; break;
case PR_ATTACH_DATA_BIN:
	s = "PR_ATTACH_DATA_BIN"; break;
case PR_ATTACH_ENCODING:
	s = "PR_ATTACH_ENCODING"; break;
case PR_ATTACH_EXTENSION:
	s = "PR_ATTACH_EXTENSION"; break;
case PR_ATTACH_FILENAME:
	s = "PR_ATTACH_FILENAME"; break;
case PR_ATTACH_METHOD:
	s = "PR_ATTACH_METHOD"; break;
case PR_ATTACH_LONG_FILENAME:
	s = "PR_ATTACH_LONG_FILENAME"; break;
case PR_ATTACH_PATHNAME:
	s = "PR_ATTACH_PATHNAME"; break;
case PR_ATTACH_RENDERING:
	s = "PR_ATTACH_RENDERING"; break;
case PR_ATTACH_TAG:
	s = "PR_ATTACH_TAG"; break;
case PR_RENDERING_POSITION:
	s = "PR_RENDERING_POSITION"; break;
case PR_ATTACH_TRANSPORT_NAME:
	s = "PR_ATTACH_TRANSPORT_NAME"; break;
case PR_ATTACH_LONG_PATHNAME:
	s = "PR_ATTACH_LONG_PATHNAME"; break;
case PR_ATTACH_MIME_TAG:
	s = "PR_ATTACH_MIME_TAG"; break;
case PR_ATTACH_ADDITIONAL_INFO:
	s = "PR_ATTACH_ADDITIONAL_INFO"; break;

/*
 *  AB Object properties
 */

case PR_DISPLAY_TYPE:
	s = "PR_DISPLAY_TYPE"; break;
case PR_TEMPLATEID:
	s = "PR_TEMPLATEID"; break;
case PR_PRIMARY_CAPABILITY:
	s = "PR_PRIMARY_CAPABILITY"; break;


/*
 *  Mail user properties
 */
case PR_7BIT_DISPLAY_NAME:
	s = "PR_7BIT_DISPLAY_NAME"; break;
case PR_ACCOUNT:
	s = "PR_ACCOUNT"; break;
case PR_ALTERNATE_RECIPIENT:
	s = "PR_ALTERNATE_RECIPIENT"; break;
case PR_CALLBACK_TELEPHONE_NUMBER:
	s = "PR_CALLBACK_TELEPHONE_NUMBER"; break;
case PR_CONVERSION_PROHIBITED:
	s = "PR_CONVERSION_PROHIBITED"; break;
case PR_DISCLOSE_RECIPIENTS:
	s = "PR_DISCLOSE_RECIPIENTS"; break;
case PR_GENERATION:
	s = "PR_GENERATION"; break;
case PR_GIVEN_NAME:
	s = "PR_GIVEN_NAME"; break;
case PR_GOVERNMENT_ID_NUMBER:
	s = "PR_GOVERNMENT_ID_NUMBER"; break;
case PR_BUSINESS_TELEPHONE_NUMBER:
	s = "PR_BUSINESS_TELEPHONE_NUMBER or PR_OFFICE_TELEPHONE_NUMBER"; break;
case PR_HOME_TELEPHONE_NUMBER:
	s = "PR_HOME_TELEPHONE_NUMBER"; break;
case PR_INITIALS:
	s = "PR_INITIALS"; break;
case PR_KEYWORD:
	s = "PR_KEYWORD"; break;
case PR_LANGUAGE:
	s = "PR_LANGUAGE"; break;
case PR_LOCATION:
	s = "PR_LOCATION"; break;
case PR_MAIL_PERMISSION:
	s = "PR_MAIL_PERMISSION"; break;
case PR_MHS_COMMON_NAME:
	s = "PR_MHS_COMMON_NAME"; break;
case PR_ORGANIZATIONAL_ID_NUMBER:
	s = "PR_ORGANIZATIONAL_ID_NUMBER"; break;
case PR_SURNAME:
	s = "PR_SURNAME"; break;
case PR_ORIGINAL_ENTRYID:
	s = "PR_ORIGINAL_ENTRYID"; break;
case PR_ORIGINAL_DISPLAY_NAME:
	s = "PR_ORIGINAL_DISPLAY_NAME"; break;
case PR_ORIGINAL_SEARCH_KEY:
	s = "PR_ORIGINAL_SEARCH_KEY"; break;
case PR_POSTAL_ADDRESS:
	s = "PR_POSTAL_ADDRESS"; break;
case PR_COMPANY_NAME:
	s = "PR_COMPANY_NAME"; break;
case PR_TITLE:
	s = "PR_TITLE"; break;
case PR_DEPARTMENT_NAME:
	s = "PR_DEPARTMENT_NAME"; break;
case PR_OFFICE_LOCATION:
	s = "PR_OFFICE_LOCATION"; break;
case PR_PRIMARY_TELEPHONE_NUMBER:
	s = "PR_PRIMARY_TELEPHONE_NUMBER"; break;
case PR_BUSINESS2_TELEPHONE_NUMBER:
	s = "PR_BUSINESS2_TELEPHONE_NUMBER or PR_OFFICE2_TELEPHONE_NUMBER"; break;
case PR_MOBILE_TELEPHONE_NUMBER:
	s = "PR_MOBILE_TELEPHONE_NUMBER or PR_CELLULAR_TELEPHONE_NUMBER"; break;
case PR_RADIO_TELEPHONE_NUMBER:
	s = "PR_RADIO_TELEPHONE_NUMBER"; break;
case PR_CAR_TELEPHONE_NUMBER:
	s = "PR_CAR_TELEPHONE_NUMBER"; break;
case PR_OTHER_TELEPHONE_NUMBER:
	s = "PR_OTHER_TELEPHONE_NUMBER"; break;
case PR_TRANSMITABLE_DISPLAY_NAME:
	s = "PR_TRANSMITABLE_DISPLAY_NAME"; break;
case PR_PAGER_TELEPHONE_NUMBER:
	s = "PR_PAGER_TELEPHONE_NUMBER or PR_BEEPER_TELEPHONE_NUMBER"; break;
case PR_USER_CERTIFICATE:
	s = "PR_USER_CERTIFICATE"; break;
case PR_PRIMARY_FAX_NUMBER:
	s = "PR_PRIMARY_FAX_NUMBER"; break;
case PR_BUSINESS_FAX_NUMBER:
	s = "PR_BUSINESS_FAX_NUMBER"; break;
case PR_HOME_FAX_NUMBER:
	s = "PR_HOME_FAX_NUMBER"; break;
case PR_COUNTRY:
	s = "PR_COUNTRY or PR_BUSINESS_ADDRESS_COUNTRY"; break;

case PR_LOCALITY:
	s = "PR_LOCALITY or PR_BUSINESS_ADDRESS_CITY"; break;

case PR_STATE_OR_PROVINCE:
	s = "PR_STATE_OR_PROVINCE or PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE"; break;

case PR_STREET_ADDRESS:
	s = "PR_STREET_ADDRESS or PR_BUSINESS_ADDRESS_STREET"; break;

case PR_POSTAL_CODE:
	s = "PR_POSTAL_CODE or PR_BUSINESS_ADDRESS_POSTAL_CODE"; break;


case PR_POST_OFFICE_BOX:
	s = "PR_POST_OFFICE_BOX or PR_BUSINESS_ADDRESS_POST_OFFICE_BOX"; break;


case PR_TELEX_NUMBER:
	s = "PR_TELEX_NUMBER"; break;
case PR_ISDN_NUMBER:
	s = "PR_ISDN_NUMBER"; break;
case PR_ASSISTANT_TELEPHONE_NUMBER:
	s = "PR_ASSISTANT_TELEPHONE_NUMBER"; break;
case PR_HOME2_TELEPHONE_NUMBER:
	s = "PR_HOME2_TELEPHONE_NUMBER"; break;
case PR_ASSISTANT:
	s = "PR_ASSISTANT"; break;
case PR_SEND_RICH_INFO:
	s = "PR_SEND_RICH_INFO"; break;

case PR_WEDDING_ANNIVERSARY:
	s = "PR_WEDDING_ANNIVERSARY"; break;
case PR_BIRTHDAY:
	s = "PR_BIRTHDAY"; break;


case PR_HOBBIES:
	s = "PR_HOBBIES"; break;

case PR_MIDDLE_NAME:
	s = "PR_MIDDLE_NAME"; break;

case PR_DISPLAY_NAME_PREFIX:
	s = "PR_DISPLAY_NAME_PREFIX"; break;

case PR_PROFESSION:
	s = "PR_PROFESSION"; break;

case PR_PREFERRED_BY_NAME:
	s = "PR_PREFERRED_BY_NAME"; break;

case PR_SPOUSE_NAME:
	s = "PR_SPOUSE_NAME"; break;

case PR_COMPUTER_NETWORK_NAME:
	s = "PR_COMPUTER_NETWORK_NAME"; break;

case PR_CUSTOMER_ID:
	s = "PR_CUSTOMER_ID"; break;

case PR_TTYTDD_PHONE_NUMBER:
	s = "PR_TTYTDD_PHONE_NUMBER"; break;

case PR_FTP_SITE:
	s = "PR_FTP_SITE"; break;

case PR_GENDER:
	s = "PR_GENDER"; break;

case PR_MANAGER_NAME:
	s = "PR_MANAGER_NAME"; break;

case PR_NICKNAME:
	s = "PR_NICKNAME"; break;

case PR_PERSONAL_HOME_PAGE:
	s = "PR_PERSONAL_HOME_PAGE"; break;


case PR_BUSINESS_HOME_PAGE:
	s = "PR_BUSINESS_HOME_PAGE"; break;

case PR_CONTACT_VERSION:
	s = "PR_CONTACT_VERSION"; break;
case PR_CONTACT_ENTRYIDS:
	s = "PR_CONTACT_ENTRYIDS"; break;

case PR_CONTACT_ADDRTYPES:
	s = "PR_CONTACT_ADDRTYPES"; break;

case PR_CONTACT_DEFAULT_ADDRESS_INDEX:
	s = "PR_CONTACT_DEFAULT_ADDRESS_INDEX"; break;

case PR_CONTACT_EMAIL_ADDRESSES:
	s = "PR_CONTACT_EMAIL_ADDRESSES"; break;


case PR_COMPANY_MAIN_PHONE_NUMBER:
	s = "PR_COMPANY_MAIN_PHONE_NUMBER"; break;

case PR_CHILDRENS_NAMES:
	s = "PR_CHILDRENS_NAMES"; break;



case PR_HOME_ADDRESS_CITY:
	s = "PR_HOME_ADDRESS_CITY"; break;

case PR_HOME_ADDRESS_COUNTRY:
	s = "PR_HOME_ADDRESS_COUNTRY"; break;

case PR_HOME_ADDRESS_POSTAL_CODE:
	s = "PR_HOME_ADDRESS_POSTAL_CODE"; break;

case PR_HOME_ADDRESS_STATE_OR_PROVINCE:
	s = "PR_HOME_ADDRESS_STATE_OR_PROVINCE"; break;

case PR_HOME_ADDRESS_STREET:
	s = "PR_HOME_ADDRESS_STREET"; break;

case PR_HOME_ADDRESS_POST_OFFICE_BOX:
	s = "PR_HOME_ADDRESS_POST_OFFICE_BOX"; break;

case PR_OTHER_ADDRESS_CITY:
	s = "PR_OTHER_ADDRESS_CITY"; break;

case PR_OTHER_ADDRESS_COUNTRY:
	s = "PR_OTHER_ADDRESS_COUNTRY"; break;

case PR_OTHER_ADDRESS_POSTAL_CODE:
	s = "PR_OTHER_ADDRESS_POSTAL_CODE"; break;

case PR_OTHER_ADDRESS_STATE_OR_PROVINCE:
	s = "PR_OTHER_ADDRESS_STATE_OR_PROVINCE"; break;

case PR_OTHER_ADDRESS_STREET:
	s = "PR_OTHER_ADDRESS_STREET"; break;

case PR_OTHER_ADDRESS_POST_OFFICE_BOX:
	s = "PR_OTHER_ADDRESS_POST_OFFICE_BOX"; break;


/*
 *  Profile section properties
 */

case PR_STORE_PROVIDERS:
	s = "PR_STORE_PROVIDERS"; break;
case PR_AB_PROVIDERS:
	s = "PR_AB_PROVIDERS"; break;
case PR_TRANSPORT_PROVIDERS:
	s = "PR_TRANSPORT_PROVIDERS"; break;

case PR_DEFAULT_PROFILE:
	s = "PR_DEFAULT_PROFILE"; break;
case PR_AB_SEARCH_PATH:
	s = "PR_AB_SEARCH_PATH"; break;
case PR_AB_DEFAULT_DIR:
	s = "PR_AB_DEFAULT_DIR"; break;
case PR_AB_DEFAULT_PAB:
	s = "PR_AB_DEFAULT_PAB"; break;

case PR_FILTERING_HOOKS:
	s = "PR_FILTERING_HOOKS"; break;
case PR_SERVICE_NAME:
	s = "PR_SERVICE_NAME"; break;
case PR_SERVICE_DLL_NAME:
	s = "PR_SERVICE_DLL_NAME"; break;
case PR_SERVICE_ENTRY_NAME:
	s = "PR_SERVICE_ENTRY_NAME"; break;
case PR_SERVICE_UID:
	s = "PR_SERVICE_UID"; break;
case PR_SERVICE_EXTRA_UIDS:
	s = "PR_SERVICE_EXTRA_UIDS"; break;
case PR_SERVICES:
	s = "PR_SERVICES"; break;
case PR_SERVICE_SUPPORT_FILES:
	s = "PR_SERVICE_SUPPORT_FILES"; break;
case PR_SERVICE_DELETE_FILES:
	s = "PR_SERVICE_DELETE_FILES"; break;
case PR_AB_SEARCH_PATH_UPDATE:
	s = "PR_AB_SEARCH_PATH_UPDATE"; break;
case PR_PROFILE_NAME:
	s = "PR_PROFILE_NAME"; break;

/*
 *  Status object properties
 */

case PR_IDENTITY_DISPLAY:
	s = "PR_IDENTITY_DISPLAY"; break;
case PR_IDENTITY_ENTRYID:
	s = "PR_IDENTITY_ENTRYID"; break;
case PR_RESOURCE_METHODS:
	s = "PR_RESOURCE_METHODS"; break;
case PR_RESOURCE_TYPE:
	s = "PR_RESOURCE_TYPE"; break;
case PR_STATUS_CODE:
	s = "PR_STATUS_CODE"; break;
case PR_IDENTITY_SEARCH_KEY:
	s = "PR_IDENTITY_SEARCH_KEY"; break;
case PR_OWN_STORE_ENTRYID:
	s = "PR_OWN_STORE_ENTRYID"; break;
case PR_RESOURCE_PATH:
	s = "PR_RESOURCE_PATH"; break;
case PR_STATUS_STRING:
	s = "PR_STATUS_STRING"; break;
case PR_X400_DEFERRED_DELIVERY_CANCEL:
	s = "PR_X400_DEFERRED_DELIVERY_CANCEL"; break;
case PR_HEADER_FOLDER_ENTRYID:
	s = "PR_HEADER_FOLDER_ENTRYID"; break;
case PR_REMOTE_PROGRESS:
	s = "PR_REMOTE_PROGRESS"; break;
case PR_REMOTE_PROGRESS_TEXT:
	s = "PR_REMOTE_PROGRESS_TEXT"; break;
case PR_REMOTE_VALIDATE_OK:
	s = "PR_REMOTE_VALIDATE_OK"; break;

/*
 * Display table properties
 */

case PR_CONTROL_FLAGS:
	s = "PR_CONTROL_FLAGS"; break;
case PR_CONTROL_STRUCTURE:
	s = "PR_CONTROL_STRUCTURE"; break;
case PR_CONTROL_TYPE:
	s = "PR_CONTROL_TYPE"; break;
case PR_DELTAX:
	s = "PR_DELTAX"; break;
case PR_DELTAY:
	s = "PR_DELTAY"; break;
case PR_XPOS:
	s = "PR_XPOS"; break;
case PR_YPOS:
	s = "PR_YPOS"; break;
case PR_CONTROL_ID:
	s = "PR_CONTROL_ID"; break;
case PR_INITIAL_DETAILS_PANE:
	s = "PR_INITIAL_DETAILS_PANE"; break;
/*
 * Secure property id range
 */

case PROP_ID_SECURE_MIN:
	s = "PROP_ID_SECURE_MIN"; break;
case PROP_ID_SECURE_MAX:
	s = "PROP_ID_SECURE_MAX"; break;
