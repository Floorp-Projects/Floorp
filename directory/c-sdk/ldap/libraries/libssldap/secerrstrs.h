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

/*
 * secerrstrs.h - map security errors to strings (used by errormap.c)
 *
 */

/*
 ****************************************************************************
 * On 21-June-2002, the code below this point was copied from the file
 * mozilla/security/nss/cmd/lib/SECerrs.h (tag NSS_3_4_2_RTM).
 ****************************************************************************
 */

/* General security error codes  */
/* Caller must #include "secerr.h" */

ER3(SEC_ERROR_IO,				SEC_ERROR_BASE + 0,
"An I/O error occurred during security authorization.")

ER3(SEC_ERROR_LIBRARY_FAILURE,			SEC_ERROR_BASE + 1,
"security library failure.")

ER3(SEC_ERROR_BAD_DATA,				SEC_ERROR_BASE + 2,
"security library: received bad data.")

ER3(SEC_ERROR_OUTPUT_LEN,			SEC_ERROR_BASE + 3,
"security library: output length error.")

ER3(SEC_ERROR_INPUT_LEN,			SEC_ERROR_BASE + 4,
"security library has experienced an input length error.")

ER3(SEC_ERROR_INVALID_ARGS,			SEC_ERROR_BASE + 5,
"security library: invalid arguments.")

ER3(SEC_ERROR_INVALID_ALGORITHM,		SEC_ERROR_BASE + 6,
"security library: invalid algorithm.")

ER3(SEC_ERROR_INVALID_AVA,			SEC_ERROR_BASE + 7,
"security library: invalid AVA.")

ER3(SEC_ERROR_INVALID_TIME,			SEC_ERROR_BASE + 8,
"Improperly formatted time string.")

ER3(SEC_ERROR_BAD_DER,				SEC_ERROR_BASE + 9,
"security library: improperly formatted DER-encoded message.")

ER3(SEC_ERROR_BAD_SIGNATURE,			SEC_ERROR_BASE + 10,
"Peer's certificate has an invalid signature.")

ER3(SEC_ERROR_EXPIRED_CERTIFICATE,		SEC_ERROR_BASE + 11,
"Peer's Certificate has expired.")

ER3(SEC_ERROR_REVOKED_CERTIFICATE,		SEC_ERROR_BASE + 12,
"Peer's Certificate has been revoked.")

ER3(SEC_ERROR_UNKNOWN_ISSUER,			SEC_ERROR_BASE + 13,
"Peer's Certificate issuer is not recognized.")

ER3(SEC_ERROR_BAD_KEY,				SEC_ERROR_BASE + 14,
"Peer's public key is invalid.")

ER3(SEC_ERROR_BAD_PASSWORD,			SEC_ERROR_BASE + 15,
"The security password entered is incorrect.")

ER3(SEC_ERROR_RETRY_PASSWORD,			SEC_ERROR_BASE + 16,
"New password entered incorrectly.  Please try again.")

ER3(SEC_ERROR_NO_NODELOCK,			SEC_ERROR_BASE + 17,
"security library: no nodelock.")

ER3(SEC_ERROR_BAD_DATABASE,			SEC_ERROR_BASE + 18,
"security library: bad database.")

ER3(SEC_ERROR_NO_MEMORY,			SEC_ERROR_BASE + 19,
"security library: memory allocation failure.")

ER3(SEC_ERROR_UNTRUSTED_ISSUER,			SEC_ERROR_BASE + 20,
"Peer's certificate issuer has been marked as not trusted by the user.")

ER3(SEC_ERROR_UNTRUSTED_CERT,			SEC_ERROR_BASE + 21,
"Peer's certificate has been marked as not trusted by the user.")

ER3(SEC_ERROR_DUPLICATE_CERT,			(SEC_ERROR_BASE + 22),
"Certificate already exists in your database.")

ER3(SEC_ERROR_DUPLICATE_CERT_NAME,		(SEC_ERROR_BASE + 23),
"Downloaded certificate's name duplicates one already in your database.")

ER3(SEC_ERROR_ADDING_CERT,			(SEC_ERROR_BASE + 24),
"Error adding certificate to database.")

ER3(SEC_ERROR_FILING_KEY,			(SEC_ERROR_BASE + 25),
"Error refiling the key for this certificate.")

ER3(SEC_ERROR_NO_KEY,				(SEC_ERROR_BASE + 26),
"The private key for this certificate cannot be found in key database")

ER3(SEC_ERROR_CERT_VALID,			(SEC_ERROR_BASE + 27),
"This certificate is valid.")

ER3(SEC_ERROR_CERT_NOT_VALID,			(SEC_ERROR_BASE + 28),
"This certificate is not valid.")

ER3(SEC_ERROR_CERT_NO_RESPONSE,			(SEC_ERROR_BASE + 29),
"Cert Library: No Response")

ER3(SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE,	(SEC_ERROR_BASE + 30),
"The certificate issuer's certificate has expired.  Check your system date and time.")

ER3(SEC_ERROR_CRL_EXPIRED,			(SEC_ERROR_BASE + 31),
"The CRL for the certificate's issuer has expired.  Update it or check your system data and time.")

ER3(SEC_ERROR_CRL_BAD_SIGNATURE,		(SEC_ERROR_BASE + 32),
"The CRL for the certificate's issuer has an invalid signature.")

ER3(SEC_ERROR_CRL_INVALID,			(SEC_ERROR_BASE + 33),
"New CRL has an invalid format.")

ER3(SEC_ERROR_EXTENSION_VALUE_INVALID,		(SEC_ERROR_BASE + 34),
"Certificate extension value is invalid.")

ER3(SEC_ERROR_EXTENSION_NOT_FOUND,		(SEC_ERROR_BASE + 35),
"Certificate extension not found.")

ER3(SEC_ERROR_CA_CERT_INVALID,			(SEC_ERROR_BASE + 36),
"Issuer certificate is invalid.")
   
ER3(SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID,	(SEC_ERROR_BASE + 37),
"Certificate path length constraint is invalid.")

ER3(SEC_ERROR_CERT_USAGES_INVALID,		(SEC_ERROR_BASE + 38),
"Certificate usages field is invalid.")

ER3(SEC_INTERNAL_ONLY,				(SEC_ERROR_BASE + 39),
"**Internal ONLY module**")

ER3(SEC_ERROR_INVALID_KEY,			(SEC_ERROR_BASE + 40),
"The key does not support the requested operation.")

ER3(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION,	(SEC_ERROR_BASE + 41),
"Certificate contains unknown critical extension.")

ER3(SEC_ERROR_OLD_CRL,				(SEC_ERROR_BASE + 42),
"New CRL is not later than the current one.")

ER3(SEC_ERROR_NO_EMAIL_CERT,			(SEC_ERROR_BASE + 43),
"Not encrypted or signed: you do not yet have an email certificate.")

ER3(SEC_ERROR_NO_RECIPIENT_CERTS_QUERY,		(SEC_ERROR_BASE + 44),
"Not encrypted: you do not have certificates for each of the recipients.")

ER3(SEC_ERROR_NOT_A_RECIPIENT,			(SEC_ERROR_BASE + 45),
"Cannot decrypt: you are not a recipient, or matching certificate and \
private key not found.")

ER3(SEC_ERROR_PKCS7_KEYALG_MISMATCH,		(SEC_ERROR_BASE + 46),
"Cannot decrypt: key encryption algorithm does not match your certificate.")

ER3(SEC_ERROR_PKCS7_BAD_SIGNATURE,		(SEC_ERROR_BASE + 47),
"Signature verification failed: no signer found, too many signers found, \
or improper or corrupted data.")

ER3(SEC_ERROR_UNSUPPORTED_KEYALG,		(SEC_ERROR_BASE + 48),
"Unsupported or unknown key algorithm.")

ER3(SEC_ERROR_DECRYPTION_DISALLOWED,		(SEC_ERROR_BASE + 49),
"Cannot decrypt: encrypted using a disallowed algorithm or key size.")

ER3(SEC_ERROR_NO_KRL,				(SEC_ERROR_BASE + 58),
"No KRL for this site's certificate has been found.")

ER3(SEC_ERROR_KRL_EXPIRED,			(SEC_ERROR_BASE + 59),
"The KRL for this site's certificate has expired.")

ER3(SEC_ERROR_KRL_BAD_SIGNATURE,		(SEC_ERROR_BASE + 60),
"The KRL for this site's certificate has an invalid signature.")

ER3(SEC_ERROR_REVOKED_KEY,			(SEC_ERROR_BASE + 61),
"The key for this site's certificate has been revoked.")

ER3(SEC_ERROR_KRL_INVALID,			(SEC_ERROR_BASE + 62),
"New KRL has an invalid format.")

ER3(SEC_ERROR_NEED_RANDOM,			(SEC_ERROR_BASE + 63),
"security library: need random data.")

ER3(SEC_ERROR_NO_MODULE,			(SEC_ERROR_BASE + 64),
"security library: no security module can perform the requested operation.")

ER3(SEC_ERROR_NO_TOKEN,				(SEC_ERROR_BASE + 65),
"The security card or token does not exist, needs to be initialized, or has been removed.")

ER3(SEC_ERROR_READ_ONLY,			(SEC_ERROR_BASE + 66),
"security library: read-only database.")

ER3(SEC_ERROR_NO_SLOT_SELECTED,			(SEC_ERROR_BASE + 67),
"No slot or token was selected.")

ER3(SEC_ERROR_CERT_NICKNAME_COLLISION,		(SEC_ERROR_BASE + 68),
"A certificate with the same nickname already exists.")

ER3(SEC_ERROR_KEY_NICKNAME_COLLISION,		(SEC_ERROR_BASE + 69),
"A key with the same nickname already exists.")

ER3(SEC_ERROR_SAFE_NOT_CREATED,			(SEC_ERROR_BASE + 70),
"error while creating safe object")

ER3(SEC_ERROR_BAGGAGE_NOT_CREATED,		(SEC_ERROR_BASE + 71),
"error while creating baggage object")

ER3(XP_JAVA_REMOVE_PRINCIPAL_ERROR,		(SEC_ERROR_BASE + 72),
"Couldn't remove the principal")

ER3(XP_JAVA_DELETE_PRIVILEGE_ERROR,		(SEC_ERROR_BASE + 73),
"Couldn't delete the privilege")

ER3(XP_JAVA_CERT_NOT_EXISTS_ERROR,		(SEC_ERROR_BASE + 74),
"This principal doesn't have a certificate")

ER3(SEC_ERROR_BAD_EXPORT_ALGORITHM,		(SEC_ERROR_BASE + 75),
"Required algorithm is not allowed.")

ER3(SEC_ERROR_EXPORTING_CERTIFICATES,		(SEC_ERROR_BASE + 76),
"Error attempting to export certificates.")

ER3(SEC_ERROR_IMPORTING_CERTIFICATES,		(SEC_ERROR_BASE + 77),
"Error attempting to import certificates.")

ER3(SEC_ERROR_PKCS12_DECODING_PFX,		(SEC_ERROR_BASE + 78),
"Unable to import.  Decoding error.  File not valid.")

ER3(SEC_ERROR_PKCS12_INVALID_MAC,		(SEC_ERROR_BASE + 79),
"Unable to import.  Invalid MAC.  Incorrect password or corrupt file.")

ER3(SEC_ERROR_PKCS12_UNSUPPORTED_MAC_ALGORITHM,	(SEC_ERROR_BASE + 80),
"Unable to import.  MAC algorithm not supported.")

ER3(SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE,(SEC_ERROR_BASE + 81),
"Unable to import.  Only password integrity and privacy modes supported.")

ER3(SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE,	(SEC_ERROR_BASE + 82),
"Unable to import.  File structure is corrupt.")

ER3(SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM, (SEC_ERROR_BASE + 83),
"Unable to import.  Encryption algorithm not supported.")

ER3(SEC_ERROR_PKCS12_UNSUPPORTED_VERSION,	(SEC_ERROR_BASE + 84),
"Unable to import.  File version not supported.")

ER3(SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT,(SEC_ERROR_BASE + 85),
"Unable to import.  Incorrect privacy password.")

ER3(SEC_ERROR_PKCS12_CERT_COLLISION,		(SEC_ERROR_BASE + 86),
"Unable to import.  Same nickname already exists in database.")

ER3(SEC_ERROR_USER_CANCELLED,			(SEC_ERROR_BASE + 87),
"The user pressed cancel.")

ER3(SEC_ERROR_PKCS12_DUPLICATE_DATA,		(SEC_ERROR_BASE + 88),
"Not imported, already in database.")

ER3(SEC_ERROR_MESSAGE_SEND_ABORTED,		(SEC_ERROR_BASE + 89),
"Message not sent.")

ER3(SEC_ERROR_INADEQUATE_KEY_USAGE,		(SEC_ERROR_BASE + 90),
"Certificate key usage inadequate for attempted operation.")

ER3(SEC_ERROR_INADEQUATE_CERT_TYPE,		(SEC_ERROR_BASE + 91),
"Certificate type not approved for application.")

ER3(SEC_ERROR_CERT_ADDR_MISMATCH,		(SEC_ERROR_BASE + 92),
"Address in signing certificate does not match address in message headers.")

ER3(SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY,	(SEC_ERROR_BASE + 93),
"Unable to import.  Error attempting to import private key.")

ER3(SEC_ERROR_PKCS12_IMPORTING_CERT_CHAIN,	(SEC_ERROR_BASE + 94),
"Unable to import.  Error attempting to import certificate chain.")

ER3(SEC_ERROR_PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME, (SEC_ERROR_BASE + 95),
"Unable to export.  Unable to locate certificate or key by nickname.")

ER3(SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY,	(SEC_ERROR_BASE + 96),
"Unable to export.  Private Key could not be located and exported.")

ER3(SEC_ERROR_PKCS12_UNABLE_TO_WRITE, 		(SEC_ERROR_BASE + 97),
"Unable to export.  Unable to write the export file.")

ER3(SEC_ERROR_PKCS12_UNABLE_TO_READ,		(SEC_ERROR_BASE + 98),
"Unable to import.  Unable to read the import file.")

ER3(SEC_ERROR_PKCS12_KEY_DATABASE_NOT_INITIALIZED, (SEC_ERROR_BASE + 99),
"Unable to export.  Key database corrupt or deleted.")

ER3(SEC_ERROR_KEYGEN_FAIL,			(SEC_ERROR_BASE + 100),
"Unable to generate public/private key pair.")

ER3(SEC_ERROR_INVALID_PASSWORD,			(SEC_ERROR_BASE + 101),
"Password entered is invalid.  Please pick a different one.")

ER3(SEC_ERROR_RETRY_OLD_PASSWORD,		(SEC_ERROR_BASE + 102),
"Old password entered incorrectly.  Please try again.")

ER3(SEC_ERROR_BAD_NICKNAME,			(SEC_ERROR_BASE + 103),
"Certificate nickname already in use.")

ER3(SEC_ERROR_CANNOT_MOVE_SENSITIVE_KEY, 	(SEC_ERROR_BASE + 105),
"A sensitive key cannot be moved to the slot where it is needed.")

ER3(SEC_ERROR_JS_INVALID_MODULE_NAME, 		(SEC_ERROR_BASE + 106),
"Invalid module name.")

ER3(SEC_ERROR_JS_INVALID_DLL, 			(SEC_ERROR_BASE + 107),
"Invalid module path/filename")

ER3(SEC_ERROR_JS_ADD_MOD_FAILURE, 		(SEC_ERROR_BASE + 108),
"Unable to add module")

ER3(SEC_ERROR_JS_DEL_MOD_FAILURE, 		(SEC_ERROR_BASE + 109),
"Unable to delete module")

ER3(SEC_ERROR_OLD_KRL,	     			(SEC_ERROR_BASE + 110),
"New KRL is not later than the current one.")
 
ER3(SEC_ERROR_CKL_CONFLICT,	     		(SEC_ERROR_BASE + 111),
"New CKL has different issuer than current CKL.  Delete current CKL.")

ER3(SEC_ERROR_CERT_NOT_IN_NAME_SPACE, 		(SEC_ERROR_BASE + 112),
"The Certifying Authority for this certificate is not permitted to issue a \
certificate with this name.")

ER3(SEC_ERROR_KRL_NOT_YET_VALID,		(SEC_ERROR_BASE + 113),
"The key revocation list for this certificate is not yet valid.")

ER3(SEC_ERROR_CRL_NOT_YET_VALID,		(SEC_ERROR_BASE + 114),
"The certificate revocation list for this certificate is not yet valid.")

ER3(SEC_ERROR_UNKNOWN_CERT,			(SEC_ERROR_BASE + 115),
"The requested certificate could not be found.")

ER3(SEC_ERROR_UNKNOWN_SIGNER,			(SEC_ERROR_BASE + 116),
"The signer's certificate could not be found.")

ER3(SEC_ERROR_CERT_BAD_ACCESS_LOCATION,		(SEC_ERROR_BASE + 117),
"The location for the certificate status server has invalid format.")

ER3(SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE,	(SEC_ERROR_BASE + 118),
"The OCSP response cannot be fully decoded; it is of an unknown type.")

ER3(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE,		(SEC_ERROR_BASE + 119),
"The OCSP server returned unexpected/invalid HTTP data.")

ER3(SEC_ERROR_OCSP_MALFORMED_REQUEST,		(SEC_ERROR_BASE + 120),
"The OCSP server found the request to be corrupted or improperly formed.")

ER3(SEC_ERROR_OCSP_SERVER_ERROR,		(SEC_ERROR_BASE + 121),
"The OCSP server experienced an internal error.")

ER3(SEC_ERROR_OCSP_TRY_SERVER_LATER,		(SEC_ERROR_BASE + 122),
"The OCSP server suggests trying again later.")

ER3(SEC_ERROR_OCSP_REQUEST_NEEDS_SIG,		(SEC_ERROR_BASE + 123),
"The OCSP server requires a signature on this request.")

ER3(SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST,	(SEC_ERROR_BASE + 124),
"The OCSP server has refused this request as unauthorized.")

ER3(SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS,	(SEC_ERROR_BASE + 125),
"The OCSP server returned an unrecognizable status.")

ER3(SEC_ERROR_OCSP_UNKNOWN_CERT,		(SEC_ERROR_BASE + 126),
"The OCSP server has no status for the certificate.")

ER3(SEC_ERROR_OCSP_NOT_ENABLED,			(SEC_ERROR_BASE + 127),
"You must enable OCSP before performing this operation.")

ER3(SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER,	(SEC_ERROR_BASE + 128),
"You must set the OCSP default responder before performing this operation.")

ER3(SEC_ERROR_OCSP_MALFORMED_RESPONSE,		(SEC_ERROR_BASE + 129),
"The response from the OCSP server was corrupted or improperly formed.")

ER3(SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE,	(SEC_ERROR_BASE + 130),
"The signer of the OCSP response is not authorized to give status for \
this certificate.")

ER3(SEC_ERROR_OCSP_FUTURE_RESPONSE,		(SEC_ERROR_BASE + 131),
"The OCSP response is not yet valid (contains a date in the future).")

ER3(SEC_ERROR_OCSP_OLD_RESPONSE,		(SEC_ERROR_BASE + 132),
"The OCSP response contains out-of-date information.")
