/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/resize-observer/
 */

enum ResizeObserverBoxOptions {
    "border-box",
    "content-box",
    "device-pixel-content-box"
};

dictionary ResizeObserverOptions {
    ResizeObserverBoxOptions box = "content-box";
};

[Exposed=Window]
interface ResizeObserver {
    [Throws]
    constructor(ResizeObserverCallback callback);

    void observe(Element target, optional ResizeObserverOptions options = {});
    void unobserve(Element target);
    void disconnect();
};

callback ResizeObserverCallback = void (sequence<ResizeObserverEntry> entries, ResizeObserver observer);

[Exposed=Window]
interface ResizeObserverEntry {
    readonly attribute Element target;
    readonly attribute DOMRectReadOnly contentRect;
    // We are using [Pure, Cached, Frozen] sequence until `FrozenArray` is implemented.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1236777 for more details.
    [Frozen, Cached, Pure]
    readonly attribute sequence<ResizeObserverSize> borderBoxSize;
    [Frozen, Cached, Pure]
    readonly attribute sequence<ResizeObserverSize> contentBoxSize;
    [Frozen, Cached, Pure]
    readonly attribute sequence<ResizeObserverSize> devicePixelContentBoxSize;
};

[Exposed=Window]
interface ResizeObserverSize {
    readonly attribute unrestricted double inlineSize;
    readonly attribute unrestricted double blockSize;
};
