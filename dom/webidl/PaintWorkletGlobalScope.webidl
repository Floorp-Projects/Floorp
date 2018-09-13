/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.css-houdini.org/css-paint-api-1/#paintworkletglobalscope
 */

[Global=(Worklet,PaintWorklet),Exposed=PaintWorklet]
interface PaintWorkletGlobalScope : WorkletGlobalScope {
    void registerPaint(DOMString name, VoidFunction paintCtor);
};
