/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://webaudio.github.io/web-audio-api/#dictdef-audioparamdescriptor
 *
 * Copyright © 2018 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

dictionary AudioParamDescriptor {
    required DOMString name;
    float defaultValue = 0;
    // FIXME: we use 3.402823466e38 instead of 3.4028235e38 to workaround a bug
    // in Visual Studio compiler (see bug 1501709 for more information).
    // We should put back 3.4028235e38 once MSVC support is dropped (i.e. once
    // bug 1512504 is fixed)
    float minValue = -3.402823466e38;
    float maxValue = 3.402823466e38;
    // AutomationRate for AudioWorklet is not needed until bug 1504984 is
    // implemented
    // AutomationRate automationRate = "a-rate";
};

