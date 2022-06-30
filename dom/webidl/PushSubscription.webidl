/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The origin of this IDL file is
* https://w3c.github.io/push-api/
*/

interface Principal;

enum PushEncryptionKeyName
{
  "p256dh",
  "auth"
};

dictionary PushSubscriptionKeys
{
  ByteString p256dh;
  ByteString auth;
};

dictionary PushSubscriptionJSON
{
  USVString endpoint;
  // FIXME: bug 1493860: should this "= {}" be here?  For that matter, this
  // PushSubscriptionKeys thing is not even in the spec; "keys" is a record
  // there.
  PushSubscriptionKeys keys = {};
  EpochTimeStamp? expirationTime;
};

dictionary PushSubscriptionInit
{
  required USVString endpoint;
  required USVString scope;
  ArrayBuffer? p256dhKey;
  ArrayBuffer? authSecret;
  BufferSource? appServerKey;
  EpochTimeStamp? expirationTime = null;
};

[Exposed=(Window,Worker), Pref="dom.push.enabled"]
interface PushSubscription
{
  [Throws, ChromeOnly]
  constructor(PushSubscriptionInit initDict);

  readonly attribute USVString endpoint;
  readonly attribute PushSubscriptionOptions options;
  readonly attribute EpochTimeStamp? expirationTime;
  [Throws]
  ArrayBuffer? getKey(PushEncryptionKeyName name);
  [NewObject, UseCounter]
  Promise<boolean> unsubscribe();

  // Implements the custom serializer specified in Push API, section 9.
  [Throws]
  PushSubscriptionJSON toJSON();
};
