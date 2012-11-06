/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[PrefControlled]
interface AudioListener {

    // same as OpenAL (default 1) 
    attribute float dopplerFactor;

    // in meters / second (default 343.3) 
    attribute float speedOfSound;

    // Uses a 3D cartesian coordinate system 
    void setPosition(float x, float y, float z);
    void setOrientation(float x, float y, float z, float xUp, float yUp, float zUp);
    void setVelocity(float x, float y, float z);

};

