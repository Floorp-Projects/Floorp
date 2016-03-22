/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothMapBMessage_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothMapBMessage_h

#include "BluetoothCommon.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"

BEGIN_BLUETOOTH_NAMESPACE

enum BMsgParserState {
  BMSG_PARSING_STATE_INVALID,
  BMSG_PARSING_STATE_BEGIN_BMSG,
  BMSG_PARSING_STATE_VERSION,
  BMSG_PARSING_STATE_STATUS,
  BMSG_PARSING_STATE_TYPE,
  BMSG_PARSING_STATE_FOLDER,
  BMSG_PARSING_STATE_ORIGINATOR,
  BMSG_PARSING_STATE_VCARD,
  BMSG_PARSING_STATE_BEGIN_ENVELOPE,
  BMSG_PARSING_STATE_RECIPIENT,
  BMSG_PARSING_STATE_BEGIN_BODY,
  BMSG_PARSING_STATE_BEGIN_MSG,
};

class VCard {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VCard)
  VCard();

  void Parse(const nsAutoCString& aCurrLine);
  void GetName(nsACString& aName);
  void GetFormattedName(nsACString& aFormattedName);
  void GetTelephone(nsACString& aTelephone);
  void GetEmail(nsACString& aEmail);
  void Dump();

private:
  ~VCard();

  nsCString mName;
  nsCString mFormattedName;
  nsCString mTelephone;
  nsCString mEmail;
};

/* This class represents MAP bMessage. The bMessage object encapsulates
 * delivered message objects and provides additionally a suitable set properties
 * with helpful information. The general encoding characteristics as defined for
 * vCards.
 */
class BluetoothMapBMessage
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothMapBMessage)
  // Parse OBEX body and return bMessage object.
  BluetoothMapBMessage(uint8_t* aObexBody, int aLength);
  void GetRecipients(nsTArray<RefPtr<VCard>>& aRecipients);
  void GetOriginators(nsTArray<RefPtr<VCard>>& aRecipients);
  void GetBody(nsACString& aBody);
  void Dump();

private:
  ~BluetoothMapBMessage();
  void ProcessDecode(const char* aBody);
  void ParseBeginBmsg(const nsAutoCString& aCurrLine);
  void ParseVersion(const nsAutoCString& aCurrLine);
  void ParseStatus(const nsAutoCString& aCurrLine);
  void ParseType(const nsAutoCString& aCurrLine);
  void ParseFolder(const nsAutoCString& aCurrLine);
  void ParseOriginator(const nsAutoCString& aCurrLine);
  void ParseVCard(const nsAutoCString& aCurrLine);
  void ParseEnvelope(const nsAutoCString& aCurrLine);
  void ParseRecipient(const nsAutoCString& aCurrLine);
  void ParseBody(const nsAutoCString& aCurrLine);
  void ParseBMsg(const nsAutoCString& aCurrLine);

  /* Indicate whether a message has been read by MCE or not.
   * MCE shall be responsible to set this status.
   */
  bool mReadStatus;
  // Indicate bMessage location
  nsCString mFolderName;
  /* Part-ID is used when the content cannot be delivered in ones
   * bmessage-content object.
   * The first bmessage-content object's part-ID shall be 0, and the following
   * ones have part-ID incremented by 1 each.
   */
  int mPartId;
  // The encoding used by bmessage-body-content if the content is binary
  nsCString mEncoding;

  /* The character-set is used in bmessage-body-content if the content
   * is textual.
   */
  nsCString mCharset;
  // mLanguage may be used if the message includes textual content
  nsCString mLanguage;

  /* Length of the bmessage-body-content, starting with the "B" of the first
   * occurrence of "BEGIN_BMSG" and ending with the <CRLF> of the last occurrence
   * of "END:MSG"<CRLF>.
   */
  nsCString mBMsgLength;
  // mMessageBody represents bmessage-body-content
  nsCString mBMsgBody;
  // mType: EMAIL/SMS_GSM/SMS_CDMA/MMS
  nsCString mType;
  // Current parser state
  enum BMsgParserState mState;
  // Previous parser state
  enum BMsgParserState mUnwindState;

  /* Current level of bmessage-envelope. The maximum level of <bmessage-envelope>
   * encapsulation shall be 3.
   */
  int mEnvelopeLevel;

  /* Based on the formal BNF definition of the bMessage format, originators
   * and recipients may appear 0 or more times.
   * |mOriginators| represents the original sender of the messages, and
   * |mRecipients| represents the recipient of the message.
   */
  nsTArray<RefPtr<VCard>> mOriginators;
  nsTArray<RefPtr<VCard>> mRecipients;
};

END_BLUETOOTH_NAMESPACE
#endif //mozilla_dom_bluetooth_bluedroid_BluetoothMapBMessage_h
