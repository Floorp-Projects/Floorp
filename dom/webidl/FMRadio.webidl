/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface FMRadio : EventTarget {
  /* Indicates if the FM radio is enabled. */
  readonly attribute boolean enabled;

  /* Indicates if RDS reception is enabled */
  readonly attribute boolean rdsEnabled;

  /* Indicates if the antenna is plugged and available. */
  readonly attribute boolean antennaAvailable;

  /**
   * Current frequency in MHz. The value will be null if the FM radio is
   * disabled.
   */
  readonly attribute double? frequency;

  /* The upper bound of frequency in MHz. */
  readonly attribute double frequencyUpperBound;

  /* The lower bound of frequency in MHz. */
  readonly attribute double frequencyLowerBound;

  /**
   * The difference in frequency between two "adjacent" channels, in MHz. That
   * is, any two radio channels' frequencies differ by at least channelWidth
   * MHz. Usually, the value is one of:
   *  - 0.05 MHz
   *  - 0.1  MHz
   *  - 0.2  MHz
   */
  readonly attribute double channelWidth;

  /**
   * This is a mask consisting of bits corresponding to
   * (1 << groupcode) that can be specified to receive
   * raw RDS groups of specific group types. Note that
   * groupcode corresponds to the upper 5 bits in block B.
   */
  attribute unsigned long rdsGroupMask;

  /**
   * The Program Identification (PI) code.
   * Available if RDS is enabled on both the station and on this device.
   * The value is null otherwise.
   */
  readonly attribute unsigned short? pi;

  /**
   * The Program Type (PTY) code.
   * Available if RDS is enabled on both the station and on this device.
   * The value is null otherwise.
   */
  readonly attribute octet? pty;

  /**
   * The Program Service (PS) name.
   * Available if RDS is enabled on the station and on this device
   */
  readonly attribute DOMString? ps;

  /**
   * The radiotext, as provided by group 2A/2B.
   * Available if RDS is enabled on the station and on this device
   */
  readonly attribute DOMString? rt;

  /**
   * The last RDS group received.
   * Available if RDS is enabled on the station and on this device
   */
  readonly attribute Uint16Array? rdsgroup;

  /* Fired when the FM radio is enabled. */
  attribute EventHandler onenabled;

  /* Fired when the FM radio is disabled. */
  attribute EventHandler ondisabled;

  /* Fired when the RDS is enabled. */
  attribute EventHandler onrdsenabled;

  /* Fired when the RDS is disabled. */
  attribute EventHandler onrdsdisabled;

  /**
   * Fired when the antenna becomes available or unavailable, i.e., fired when
   * the antennaAvailable attribute changes.
   */
  attribute EventHandler onantennaavailablechange;

  /* Fired when the FM radio's frequency is changed. */
  attribute EventHandler onfrequencychange;

  /* Fired when the PI code changes */
  attribute EventHandler onpichange;

  /* Fired when the PTY changes */
  attribute EventHandler onptychange;

  /* Fired when the PS name changes */
  attribute EventHandler onpschange;

  /* Fired when the radiotext changes */
  attribute EventHandler onrtchange;

  /* Fired when we get a new RDS group */
  attribute EventHandler onnewrdsgroup;

  /**
   * Power the FM radio off. The disabled event will be fired if this request
   * completes successfully.
   */
  DOMRequest disable();

  /**
   * Power the FM radio on, and tune the radio to the given frequency in MHz.
   * This will fail if the given frequency is out of range. The enabled event
   * and frequencychange event will be fired if this request completes
   * successfully.
   */
  DOMRequest enable(double frequency);

  /**
   * Tune the FM radio to the given frequency. This will fail if the given
   * frequency is out of range.
   *
   * Note that the FM radio may not tuned to the exact frequency given. To get
   * the frequency the radio is actually tuned to, wait for the request to fire
   * sucess (or wait for the frequencychange event to fire), and then read the
   * frequency attribute.
   */
  DOMRequest setFrequency(double frequency);

  /**
   * Tell the FM radio to seek up to the next channel. If the frequency is
   * successfully changed, the frequencychange event will be triggered.
   *
   * Only one seek is allowed at once: If the radio is seeking when the seekUp
   * is called, error will be fired.
   */
  DOMRequest seekUp();

  /**
   * Tell the FM radio to seek down to the next channel. If the frequency is
   * successfully changed, the frequencychange event will be triggered.
   *
   * Only one seek is allowed at once: If the radio is seeking when the
   * seekDown is called, error will be fired.
   */
  DOMRequest seekDown();

  /**
   * Cancel the seek action. If the radio is not currently seeking up or down,
   * error will be fired.
   */
  DOMRequest cancelSeek();

  /**
   * Enable RDS reception.
   *
   * If the radio is off, RDS will be enabled when the radio is turned on.
   */
  DOMRequest enableRDS();

  /**
   * Disable RDS reception.
   *
   * If the radio is off, RDS will not be enabled when the radio is turned on.
   */
  DOMRequest disableRDS();
};

