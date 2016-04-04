/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothMapBMessage.h"
#include "base/basictypes.h"
#include "plstr.h"

BEGIN_BLUETOOTH_NAMESPACE

namespace {
  static const char kMapCrlf[] = "\r\n";
}

BluetoothMapBMessage::BluetoothMapBMessage(uint8_t* aObexBody, int aLength)
  : mReadStatus(false)
  , mPartId(0)
  , mState(BMSG_PARSING_STATE_INVALID)
  , mUnwindState(BMSG_PARSING_STATE_INVALID)
  , mEnvelopeLevel(0)
{
  const nsCString body = nsCString(reinterpret_cast<const char*>(aObexBody),
                                   aLength);
  ProcessDecode(body.get());
}

BluetoothMapBMessage::~BluetoothMapBMessage()
{}

/**
 * Read one line by one line.
 *
 * @param aNextLineStart [in][out] On input, aNextLineStart is the start of
 *        the current line. On output, aNextLineStart is the start of the next
 *        line.
 * @param aLine [out] The next line read.
 */
static int
ReadLine(const char* & aNextLineStart, nsACString& aLine)
{
  aLine.Truncate();
  for (;;) {
    const char* eol = PL_strpbrk(aNextLineStart, kMapCrlf);

    if (!eol) { // Reached end of file before newline
      eol = aNextLineStart + strlen(aNextLineStart);
    }

    aLine.Append(aNextLineStart, eol - aNextLineStart);

    if (*eol == '\r') {
      ++eol;
    }
    if (*eol == '\n') {
      ++eol;
    }

    aNextLineStart = eol;

    if (*eol != ' ') {
      // not a continuation
      return 0;
    }
  }
}

static bool
ExtractParameter(const nsAutoCString& aCurrLine,
                 const char* aPattern, nsACString& aParam)
{
  aParam.Truncate();
  if (aCurrLine.Find(aPattern, false, 0, PL_strlen(aPattern)) == kNotFound) {
    return false;
  }

  int32_t colonPos = aCurrLine.FindChar(':');
  aParam = nsDependentCSubstring(aCurrLine, colonPos + 1, aCurrLine.Length());
  return true;
}

/*
 * |ProcessDecode| parses bMessage based on following MAP 3.1.3 Message Format.
 * The state transition follows by BMsgParserState.
 * BNF:
 * <bmessage-object>::= {
 * "BEGIN:BMSG" <CRLF>
 * <bmessage-property>
 * [<bmessage-originator>]*
 * <bmessage-envelope>
 * "END:BMSG" <CRLF>
 * }
 *
 * <bmessage-envelope> ::= {
 * "BEGIN:BENV" <CRLF>
 * [<bmessage-recipient>]*
 * <bmessage-envelope> | <bmessage-content>
 * "END:BENV" <CRLF>
 * }
 *
 * The state transition follows:
 * BEGIN_BMSG->VERSION->STATUS->TYPE->FOLDER->ORIGINATOR->VCARD->ENVELOPE->
 * VCARD->RECIPIENT->BODY->BMSG
 */
void
BluetoothMapBMessage::ProcessDecode(const char* aBuf)
{
  mState = BMSG_PARSING_STATE_BEGIN_BMSG;

  static void (BluetoothMapBMessage::* const ParserActions[])(const
    nsAutoCString&) = {
    nullptr, /* BMSG_PARSING_STATE_INVALID */
    &BluetoothMapBMessage::ParseBeginBmsg,
    &BluetoothMapBMessage::ParseVersion,
    &BluetoothMapBMessage::ParseStatus,
    &BluetoothMapBMessage::ParseType,
    &BluetoothMapBMessage::ParseFolder,
    &BluetoothMapBMessage::ParseOriginator,
    &BluetoothMapBMessage::ParseVCard,
    &BluetoothMapBMessage::ParseEnvelope,
    &BluetoothMapBMessage::ParseRecipient,
    &BluetoothMapBMessage::ParseBody,
    &BluetoothMapBMessage::ParseBMsg
  };

  for (;;) {
    nsAutoCString curLine;
    if (ReadLine(aBuf, curLine) == -1) {
      // Cannot find eol symbol
      return;
    }
    if (curLine.IsEmpty()) {
      // Blank line or eof, exit
      return;
    }

    MOZ_ASSERT(mState != BMSG_PARSING_STATE_INVALID);

    // Parse bMessage
    (this->*(ParserActions[mState]))(curLine);
  }
}

void
BluetoothMapBMessage::ParseBeginBmsg(const nsAutoCString& aCurrLine)
{
  if (aCurrLine.EqualsLiteral("BEGIN:BMSG")) {
    mState = BMSG_PARSING_STATE_VERSION;
  }
}

void
BluetoothMapBMessage::ParseVersion(const nsAutoCString& aCurrLine)
{
  nsCString version;
  if (ExtractParameter(aCurrLine, "VERSION:", version)) {
    /* The value for this property shall be "VERSION:1.0",
     * based on Message Access Profile 3.1.3.
     */
    if (version.EqualsLiteral("1.0")) {
      mState = BMSG_PARSING_STATE_STATUS;
    }
  }
}

void
BluetoothMapBMessage::ParseStatus(const nsAutoCString& aCurrLine)
{
  nsCString status;
  if (ExtractParameter(aCurrLine, "STATUS:", status)) {
    // only READ or UNREAD status
    mReadStatus = status.EqualsLiteral("READ");
    mState = BMSG_PARSING_STATE_TYPE;
  }
}

void
BluetoothMapBMessage::ParseType(const nsAutoCString& aCurrLine)
{
  nsCString type;
  if (ExtractParameter(aCurrLine, "TYPE:", type)) {
    mType = type;
    mState = BMSG_PARSING_STATE_FOLDER;
  }
}

void
BluetoothMapBMessage::ParseFolder(const nsAutoCString& aCurrLine)
{
  nsCString folder;
  if (ExtractParameter(aCurrLine, "FOLDER:", folder)) {
    mFolderName = folder;
  }

  mState = BMSG_PARSING_STATE_ORIGINATOR;
}

void
BluetoothMapBMessage::ParseOriginator(const nsAutoCString& aCurrLine)
{
  if (aCurrLine.EqualsLiteral("BEGIN:VCARD")) {
    mOriginators.AppendElement(new VCard());

    // We may parse vCard multiple times
    mUnwindState = mState;
    mState = BMSG_PARSING_STATE_VCARD;
  } else if (aCurrLine.EqualsLiteral("BEGIN:BENV")) {
    mState = BMSG_PARSING_STATE_BEGIN_ENVELOPE;
  }
}

void
BluetoothMapBMessage::ParseVCard(const nsAutoCString& aCurrLine)
{
  if (aCurrLine.EqualsLiteral("END:VCARD")) {
    mState = mUnwindState;
    return;
  }

  if (mUnwindState == BMSG_PARSING_STATE_ORIGINATOR &&
      !mOriginators.IsEmpty()) {
    mOriginators.LastElement()->Parse(aCurrLine);
  } else if (mUnwindState == BMSG_PARSING_STATE_RECIPIENT &&
             !mRecipients.IsEmpty()) {
    mRecipients.LastElement()->Parse(aCurrLine);
  }
}

void
BluetoothMapBMessage::ParseEnvelope(const nsAutoCString& aCurrLine)
{
  if(!aCurrLine.EqualsLiteral("BEGIN:VCARD") &&
     !aCurrLine.EqualsLiteral("BEGIN:BENV")) {
    mState = BMSG_PARSING_STATE_BEGIN_BODY;
    return;
  }

  if (aCurrLine.EqualsLiteral("BEGIN:BENV")) {
    // nested BENV
    // TODO: Check nested BENV envelope level for Email use case
    ++mEnvelopeLevel;
  }

  mRecipients.AppendElement(new VCard());

  mState = BMSG_PARSING_STATE_VCARD;
  // In case there are many recipients
  mUnwindState = BMSG_PARSING_STATE_RECIPIENT;
}

void
BluetoothMapBMessage::ParseRecipient(const nsAutoCString& aCurrLine)
{
  /* After parsing vCard, check whether it's bMessage BODY or VCARD.
   * bmessage-recipient may appear more than once.
   */
  if (aCurrLine.EqualsLiteral("BEGIN:BBODY")) {
    mState = BMSG_PARSING_STATE_BEGIN_BODY;
  } else if (aCurrLine.EqualsLiteral("BEGIN:VCARD")) {
    mState = BMSG_PARSING_STATE_VCARD;
  }
}

/* |ParseBody| parses based on the following BNF format:
 * <bmessage-body-property>::=[<bmessage-body-encoding-property>]
 * [<bmessage-body-charset-property>]
 * [<bmessage-body-language-property>]
 * <bmessage-body-content-length-property>
 *
 * <bmessage-content>::= {
 * "BEGIN:BBODY"<CRLF>
 * [<bmessage-body-part-ID> <CRLF>]
 * <bmessage-body-property>
 * <bmessage-body-content>* <CRLF>
 * "END:BBODY"<CRLF>
 * }
 */
void
BluetoothMapBMessage::ParseBody(const nsAutoCString& aCurrLine)
{
  nsCString param;
  if (ExtractParameter(aCurrLine, "PARTID::", param)) {
    nsresult rv;
    mPartId = param.ToInteger(&rv);

    if (NS_FAILED(rv)) {
      BT_LOGR("Failed to convert PARTID, error: 0x%x",
              static_cast<uint32_t>(rv));
    }
  } else if (ExtractParameter(aCurrLine, "ENCODING:", param)) {
    mEncoding = param;
  } else if (ExtractParameter(aCurrLine, "CHARSET:", param)) {
    mCharset = param;
  } else if (ExtractParameter(aCurrLine, "LANGUAGE:", param)) {
    mLanguage = param;
  } else if (ExtractParameter(aCurrLine, "LENGTH:", param)) {
    mBMsgLength = param;
  } else if (aCurrLine.EqualsLiteral("BEGIN:MSG")) {
    // Parse <bmessage-body-content>
    mState = BMSG_PARSING_STATE_BEGIN_MSG;
  }
}

/* |ParseBMsg| parses based on the following BNF format:
 * <bmessage-body-content>::={
 * "BEGIN:MSG"<CRLF>
 * 'message'<CRLF>
 * "END:MSG"<CRLF>
 * }
 */
void
BluetoothMapBMessage::ParseBMsg(const nsAutoCString& aCurrLine)
{
  /* TODO: Support binary message content
   * For SMS: currently only UTF-8 is supported for textual content.
   */
  if (aCurrLine.EqualsLiteral("END:MSG") ||
      aCurrLine.EqualsLiteral("BEGIN:MSG")) {
    /* Set state to STATE_BEGIN_MSG due to <bmessage-body-content> may appear
     * many times.
     */
    mState = BMSG_PARSING_STATE_BEGIN_MSG;
    return;
  }

  mBMsgBody += aCurrLine.get();
  // restore <CRLF>
  mBMsgBody.AppendLiteral(kMapCrlf);
}

void
BluetoothMapBMessage::GetRecipients(nsTArray<RefPtr<VCard>>& aRecipients)
{
  aRecipients = mRecipients;
}

void
BluetoothMapBMessage::GetOriginators(nsTArray<RefPtr<VCard>>& aOriginators)
{
  aOriginators = mOriginators;
}

void
BluetoothMapBMessage::GetBody(nsACString& aBody)
{
  aBody = mBMsgBody;
}

void
BluetoothMapBMessage::Dump()
{
  BT_LOGR("Dump: message body %s", mBMsgBody.get());
  BT_LOGR("Dump: read status %s", mReadStatus? "true" : "false");
  BT_LOGR("Dump: folder %s", mFolderName.get());
  BT_LOGR("Dump encoding %s", mEncoding.get());
  BT_LOGR("Dump charset: %s" , mCharset.get());
  BT_LOGR("Dump language: %s", mLanguage.get());
  BT_LOGR("Dump length: %s", mBMsgLength.get());
  BT_LOGR("Dump type: %s ", mType.get());
}

VCard::VCard()
{}

VCard::~VCard()
{}

void
VCard::Parse(const nsAutoCString& aCurrLine)
{
  nsCString param;
  if (ExtractParameter(aCurrLine, "N:", param)) {
    mName = param;
  } else if (ExtractParameter(aCurrLine, "FN:", param)) {
    mFormattedName = param;
  } else if (ExtractParameter(aCurrLine, "TEL:", param)) {
    mTelephone = param;
  } else if (ExtractParameter(aCurrLine, "Email:", param)) {
    mEmail = param;
  }
}

void
VCard::GetTelephone(nsACString& aTelephone)
{
  aTelephone = mTelephone;
}

void
VCard::GetName(nsACString& aName)
{
  aName = mName;
}

void
VCard::GetFormattedName(nsACString& aFormattedName)
{
  aFormattedName = mFormattedName;
}

void
VCard::GetEmail(nsACString& aEmail)
{
  aEmail = mEmail;
}

void
VCard::Dump()
{
  BT_LOGR("Dump: Name %s", mName.get());
  BT_LOGR("Dump: FormattedName %s", mFormattedName.get());
  BT_LOGR("Dump: Telephone %s", mTelephone.get());
  BT_LOGR("Dump: Email %s", mEmail.get());
}

END_BLUETOOTH_NAMESPACE
