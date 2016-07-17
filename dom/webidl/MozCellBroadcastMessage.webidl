/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum CellBroadcastGsmGeographicalScope {"cell-immediate", "plmn",
                                        "location-area", "cell"};
enum CellBroadcastMessageClass {"class-0", "class-1", "class-2",
                                "class-3", "user-1", "user-2", "normal"};
enum CellBroadcastEtwsWarningType {"earthquake", "tsunami",
                                   "earthquake-tsunami", "test", "other"};

[Pref="dom.cellbroadcast.enabled",
 ChromeOnly]
interface MozCellBroadcastMessage
{
  /**
   * The Service Id in the device where the message is received from.
   */
  readonly attribute unsigned long serviceId;

  /**
   * Indication of the geographical area over which the Message Code is unique,
   * and the display mode.
   *
   * Possible values are: "cell-immediate", "plmn", "location-area" and "cell".
   */
  readonly attribute CellBroadcastGsmGeographicalScope? gsmGeographicalScope;

  /**
   * The Message Code differentiates between messages from the same source and
   * type (e.g., with the same Message Identifier).
   */
  readonly attribute unsigned short messageCode;

  /**
   * Source and type of the message. For example, "Automotive Association"
   * (= source), "Traffic Reports" (= type) could correspond to one value. The
   * Message Identifier is coded in binary.
   */
  readonly attribute unsigned short messageId;

  /**
   * ISO-639-1 language code for this message. Null if unspecified.
   */
  readonly attribute DOMString? language;

  /**
   * Text message carried by the message.
   */
  readonly attribute DOMString? body;

  /**
   * Possible values are "normal", "class-0", "class-1", "class-2", "class-3",
   * "user-1", and "user-2".
   */
  readonly attribute CellBroadcastMessageClass? messageClass;

  /**
   * System time stamp at receival.
   */
  readonly attribute DOMTimeStamp timestamp;

  /**
   * Additional ETWS-specific info.
   */
  readonly attribute MozCellBroadcastEtwsInfo? etws;

  /**
   * Service Category.
   */
  readonly attribute unsigned short? cdmaServiceCategory;
};

[Pref="dom.cellbroadcast.enabled",
 ChromeOnly]
interface MozCellBroadcastEtwsInfo
{
  /**
   * Warning type. Possible values are "earthquake", "tsunami",
   * "earthquake-tsunami", "test" and "other".
   */
  readonly attribute CellBroadcastEtwsWarningType? warningType;

  /**
   * Emergency user alert indication. It is used to command mobile terminals to
   * activate emergency user alert upon the reception of ETWS primary
   * notification.
   */
  readonly attribute boolean emergencyUserAlert;

  /**
   * Message popup indication. It is used to command mobile terminals to
   * activate message popup upon the reception of ETWS primary notification.
   */
  readonly attribute boolean popup;
};
