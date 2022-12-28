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

[GenerateInit]
dictionary AudioParamDescriptor {
    required DOMString name;
    float defaultValue = 0;
    float minValue = -3.4028235e38;
    float maxValue = 3.4028235e38;
    // AutomationRate for AudioWorklet is not needed until bug 1504984 is
    // implemented
    // AutomationRate automationRate = "a-rate";
};
