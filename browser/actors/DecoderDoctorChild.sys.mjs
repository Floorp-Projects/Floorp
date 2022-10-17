/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class DecoderDoctorChild extends JSWindowActorChild {
  // Observes 'decoder-doctor-notification'. This actor handles most such notifications, but does not deal with notifications with the 'cannot-play' type, which is handled
  // @param aSubject the nsPIDOMWindowInner associated with the notification.
  // @param aTopic should be "decoder-doctor-notification".
  // @param aData json data that contains analysis information from Decoder Doctor:
  // - 'type' is the type of issue, it determines which text to show in the
  //   infobar.
  // - 'isSolved' is true when the notification actually indicates the
  //   resolution of that issue, to be reported as telemetry.
  // - 'decoderDoctorReportId' is the Decoder Doctor issue identifier, to be
  //   used here as key for the telemetry (counting infobar displays,
  //   "Learn how" buttons clicks, and resolutions) and for the prefs used
  //   to store at-issue formats.
  // - 'formats' contains a comma-separated list of formats (or key systems)
  //   that suffer the issue. These are kept in a pref, which the backend
  //   uses to later find when an issue is resolved.
  // - 'decodeIssue' is a description of the decode error/warning.
  // - 'resourceURL' is the resource with the issue.
  observe(aSubject, aTopic, aData) {
    this.sendAsyncMessage("DecoderDoctor:Notification", aData);
  }
}
