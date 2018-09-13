/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
interface URI;

[Constructor(DOMString type, optional PopupBlockedEventInit eventInitDict)]
interface PopupBlockedEvent : Event
{
  readonly attribute Window? requestingWindow;
  readonly attribute URI? popupWindowURI;
  readonly attribute DOMString? popupWindowName;
  readonly attribute DOMString? popupWindowFeatures;
};

dictionary PopupBlockedEventInit : EventInit
{
  Window? requestingWindow = null;
  URI? popupWindowURI = null;
  DOMString popupWindowName = "";
  DOMString popupWindowFeatures = "";
};
