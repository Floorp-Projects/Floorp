/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

interface PluginTag;

[Constructor(DOMString type, optional HiddenPluginEventInit eventInit), ChromeOnly]
interface HiddenPluginEvent : Event
{
  readonly attribute PluginTag? tag;
};

dictionary HiddenPluginEventInit : EventInit
{
  PluginTag? tag = null;
};
