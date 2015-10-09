/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * MAP Application Parameters
 */
enum MessageType
{
  "no-filtering",
  "sms",
  "email",
  "mms"
};

enum ReadStatus
{
  "no-filtering",
  "unread",
  "read"
};

enum Priority
{
  "no-filtering",
  "high-priority",
  "non-priority"
};

enum ParameterMask
{
  "subject",
  "datetime",
  "sender_name",
  "sender_addressing",
  "recipient_name",
  "recipient_addressing",
  "type",
  "size",
  "reception_status",
  "text",
  "attachment_size",
  "priority",
  "read",
  "sent",
  "protected",
  "replyto_addressing",
};

enum FilterCharset
{
  "native",
  "utf-8"
};

enum StatusIndicators
{
  "readstatus",
  "deletedstatus"
};
