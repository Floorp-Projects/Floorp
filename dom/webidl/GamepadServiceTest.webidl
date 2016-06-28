/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="dom.gamepad.test.enabled"]
interface GamepadServiceTest
{
  readonly attribute unsigned long noMapping;
  readonly attribute unsigned long standardMapping;

  [Throws]
  Promise<unsigned long> addGamepad(DOMString id,
                                    unsigned long mapping,
                                    unsigned long numButtons,
                                    unsigned long numAxes);

  void removeGamepad(unsigned long index);

  void newButtonEvent(unsigned long index,
                      unsigned long button,
                      boolean pressed);

  void newButtonValueEvent(unsigned long index,
                           unsigned long button,
                           boolean pressed,
                           double value);

  void newAxisMoveEvent(unsigned long index,
                        unsigned long axis,
                        double value);
};