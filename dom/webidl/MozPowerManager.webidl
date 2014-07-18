/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface MozWakeLockListener;

/**
  * The reason for the factory reset.
  * "normal" : simple factory reset.
  * "wipe"   : will also attempt to wipe all user storage areas.
  */
enum FactoryResetReason {
    "normal",
    "wipe"
};

/**
 * This interface implements navigator.mozPower
 */
interface MozPowerManager
{
    [Throws]
    void    powerOff();
    [Throws]
    void    reboot();
    void    factoryReset(optional FactoryResetReason reason = "normal");

    /**
     * The listeners are notified when a resource changes its lock state to:
     *  - unlocked
     *  - locked but not visible
     *  - locked and visible
     */
    void    addWakeLockListener(MozWakeLockListener aListener);
    void    removeWakeLockListener(MozWakeLockListener aListener);

    /**
     * Query the wake lock state of the topic.
     *
     * Possible states are:
     *
     *  - "unlocked" - nobody holds the wake lock.
     *
     *  - "locked-foreground" - at least one window holds the wake lock,
     *    and it is visible.
     *
     *  - "locked-background" - at least one window holds the wake lock,
     *    but all of them are hidden.
     *
     * @param aTopic The resource name related to the wake lock.
     */
    [Throws]
    DOMString getWakeLockState(DOMString aTopic);

    /**
     * Is the device's screen currently enabled?  This attribute controls the
     * device's screen, so setting it to false will turn off the screen.
     */
    attribute boolean screenEnabled;

    /**
     * Is the device's keypad/button backlight enabled? Setting it to false will
     * turn off the device's keypad/button backlight. And the brightness level
     * is the same as |screenBrightness|.
     */
    attribute boolean keyLightEnabled;

    /**
     * How bright is the screen's backlight, on a scale from 0 (very dim) to 1
     * (full brightness)?  Setting this attribute modifies the screen's
     * brightness.
     *
     * You can read and write this attribute even when the screen is disabled,
     * but the backlight is off while the screen is disabled.
     *
     * If you write a value of X into this attribute, the attribute may not have
     * the same value X when you later read it.  Most screens don't support as
     * many different brightness levels as there are doubles between 0 and 1, so
     * we may reduce the value's precision before storing it.
     *
     * @throw NS_ERROR_INVALID_ARG if brightness is not in the range [0, 1].
     */
    [SetterThrows]
    attribute double screenBrightness;

    /**
     * Is it possible that the device's CPU will sleep after the screen is
     * disabled?  Setting this attribute to false will prevent the device
     * entering suspend state.
     */
    attribute boolean cpuSleepAllowed;
};
