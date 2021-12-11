/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.fxtf.org/geometry/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=(Window,Worker),
 Serializable]
interface DOMQuad {
    constructor(optional DOMPointInit p1 = {}, optional DOMPointInit p2 = {},
                optional DOMPointInit p3 = {}, optional DOMPointInit p4 = {});
    constructor(DOMRectReadOnly rect);

    [NewObject] static DOMQuad fromRect(optional DOMRectInit other = {});
    [NewObject] static DOMQuad fromQuad(optional DOMQuadInit other = {});

    [SameObject] readonly attribute DOMPoint p1;
    [SameObject] readonly attribute DOMPoint p2;
    [SameObject] readonly attribute DOMPoint p3;
    [SameObject] readonly attribute DOMPoint p4;
    [NewObject] DOMRectReadOnly getBounds();

    [Default] object toJSON();
};

dictionary DOMQuadInit {
    DOMPointInit p1 = {};
    DOMPointInit p2 = {};
    DOMPointInit p3 = {};
    DOMPointInit p4 = {};
};
