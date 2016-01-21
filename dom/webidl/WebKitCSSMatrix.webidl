/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://compat.spec.whatwg.org/#webkitcssmatrix-interface
 */

[Constructor,
 Constructor(DOMString transformList),
 Constructor(WebKitCSSMatrix other),
 Exposed=Window,
 Func="mozilla::dom::WebKitCSSMatrix::FeatureEnabled"]
interface WebKitCSSMatrix : DOMMatrix {
    // Mutable transform methods
    [Throws]
    WebKitCSSMatrix setMatrixValue(DOMString transformList);

    // Immutable transform methods
    WebKitCSSMatrix multiply(WebKitCSSMatrix other);
    WebKitCSSMatrix inverse();
    WebKitCSSMatrix translate(unrestricted double tx,
                              unrestricted double ty,
                              unrestricted double tz);
    WebKitCSSMatrix scale(optional unrestricted double scaleX = 1,
                          optional unrestricted double scaleY,
                          optional unrestricted double scaleZ = 1);
    WebKitCSSMatrix rotate(optional unrestricted double rotX = 0,
                           optional unrestricted double rotY,
                           optional unrestricted double rotZ);
    WebKitCSSMatrix rotateAxisAngle(unrestricted double x,
                                    unrestricted double y,
                                    unrestricted double z,
                                    unrestricted double angle);
    WebKitCSSMatrix skewX(unrestricted double sx);
    WebKitCSSMatrix skewY(unrestricted double sy);
};
