/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_INFO_LOADER_H
#define GFX_FONT_INFO_LOADER_H

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsITimer.h"

// helper class for loading in font info spaced out at regular intervals

class gfxFontInfoLoader {
public:

    // state transitions:
    //   initial ---StartLoader with delay---> timer on delay
    //   initial ---StartLoader without delay---> timer on interval
    //   timer on delay ---LoaderTimerFire---> timer on interval
    //   timer on delay ---CancelLoader---> timer off
    //   timer on interval ---CancelLoader---> timer off
    //   timer off ---StartLoader with delay---> timer on delay
    //   timer off ---StartLoader without delay---> timer on interval
    typedef enum {
        stateInitial,
        stateTimerOnDelay,
        stateTimerOnInterval,
        stateTimerOff
    } TimerState;

    gfxFontInfoLoader() :
        mInterval(0), mState(stateInitial)
    {
    }

    virtual ~gfxFontInfoLoader();

    // start timer with an initial delay, then call Run method at regular intervals
    void StartLoader(uint32_t aDelay, uint32_t aInterval);

    // cancel the timer and cleanup
    void CancelLoader();

protected:
    class ShutdownObserver : public nsIObserver
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOBSERVER

        ShutdownObserver(gfxFontInfoLoader *aLoader)
            : mLoader(aLoader)
        { }

        virtual ~ShutdownObserver()
        { }

    protected:
        gfxFontInfoLoader *mLoader;
    };

    // Init - initialization at start time after initial delay
    virtual void InitLoader() = 0;

    // Run - called at intervals, return true to indicate done
    virtual bool RunLoader() = 0;

    // Finish - cleanup after done
    virtual void FinishLoader() = 0;

    // Timer interval callbacks
    static void LoaderTimerCallback(nsITimer *aTimer, void *aThis) {
        gfxFontInfoLoader *loader = static_cast<gfxFontInfoLoader*>(aThis);
        loader->LoaderTimerFire();
    }

    void LoaderTimerFire();

    void RemoveShutdownObserver();

    nsCOMPtr<nsITimer> mTimer;
    nsCOMPtr<nsIObserver> mObserver;
    uint32_t mInterval;
    TimerState mState;
};

#endif /* GFX_FONT_INFO_LOADER_H */
