/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function windowUtils() {
    return content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
               .getInterface(Components.interfaces.nsIDOMWindowUtils);
}

function recvSetViewport(w, h) {

    dump("setting viewport to "+ w +"x"+ h +"\n");

    windowUtils().setCSSViewport(w, h);
}

function recvSetDisplayPort(x, y, w, h) {

    dump("setting displayPort to <"+ x +", "+ y +", "+ w +", "+ h +">\n");

    windowUtils().setDisplayPortForElement(x, y, w, h, content.document.documentElement, 0);
}

function recvSetResolution(xres, yres) {

    dump("setting xres="+ xres +" yres="+ yres +"\n");

    windowUtils().setResolution(xres, yres);
}

function recvScrollBy(dx, dy) {
    content.scrollBy(dx, dy);
}

function recvScrollTo(x, y) {
    content.scrollTo(x, y);
}

addMessageListener(
    "setViewport",
    function (m) { recvSetViewport(m.json.w, m.json.h); }
);

addMessageListener(
    "setDisplayPort",
    function (m) { recvSetDisplayPort(m.json.x, m.json.y,
                                      m.json.w, m.json.h); }
);

addMessageListener(
    "setResolution",
    function (m) { recvSetResolution(m.json.xres, m.json.yres); }
);

addMessageListener(
    "scrollBy",
    function(m) { recvScrollBy(m.json.dx, m.json.dy); }
);

addMessageListener(
    "scrollTo",
    function(m) { recvScrollTo(m.json.x, m.json.y); }
);
