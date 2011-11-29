function init() {
    enableAsyncScrolling();
    messageManager.loadFrameScript(
        "chrome://global/content/test-ipcbrowser-content.js", true
    );
}

function browser() {
    return document.getElementById("content");
}

function frameLoader() {
    return browser().QueryInterface(Components.interfaces.nsIFrameLoaderOwner).frameLoader;
}

function viewManager() {
    return frameLoader().QueryInterface(Components.interfaces.nsIContentViewManager);
}

function rootView() {
    return viewManager().rootContentView;
}

function enableAsyncScrolling() {
    frameLoader().renderMode = Components.interfaces.nsIFrameLoader.RENDER_MODE_ASYNC_SCROLL;
}

// Functions affecting the content window.

function loadURL(url) {
    browser().setAttribute('src', url);
}

function scrollContentBy(dx, dy) {
    messageManager.sendAsyncMessage("scrollBy",
                                    { dx: dx, dy: dy });

}

function scrollContentTo(x, y) {
    messageManager.sendAsyncMessage("scrollTo",
                                    { x: x, y: y });
}

function setContentViewport(w, h) {
    messageManager.sendAsyncMessage("setViewport",
                                    { w: w, h: h });
}

function setContentDisplayPort(x, y, w, h) {
    messageManager.sendAsyncMessage("setDisplayPort",
                                    { x: x, y: y, w: w, h: h });
}

function setContentResolution(xres, yres) {
    messageManager.sendAsyncMessage("setResolution",
                                    { xres: xres, yres: yres });
}

// Functions affecting <browser>.

function scrollViewportBy(dx, dy) {
    rootView().scrollBy(dx, dy);
}

function scrollViewportTo(x, y) {
    rootView().scrollTo(x, y);
}

function setViewportScale(xs, ys) {
    rootView().setScale(xs, ys);
}

var kDelayMs = 100;
var kDurationMs = 250;
var scrolling = false;
function startAnimatedScrollBy(dx, dy) {
    if (scrolling)
        throw "don't interrupt me!";

    scrolling = true;

    var start = mozAnimationStartTime;
    var end = start + kDurationMs;
    // |- k| so that we do something in first invocation of nudge()
    var prevNow = start - 20;
    var accumDx = 0, accumDy = 0;

    var sentScrollBy = false;
    function nudgeScroll(now) {
	if (!scrolling) {
	    // we've been canceled
	    return;
	}
        var ddx = dx * (now - prevNow) / kDurationMs;
        var ddy = dy * (now - prevNow) / kDurationMs;

        ddx = Math.min(dx - accumDx, ddx);
        ddy = Math.min(dy - accumDy, ddy);
        accumDx += ddx;
        accumDy += ddy;

        rootView().scrollBy(ddx, ddy);

        if (!sentScrollBy && 100 <= (now - start)) {
            messageManager.sendAsyncMessage("scrollBy",
                                            { dx: dx, dy: dy });
            sentScrollBy = true;
        }

        if (now >= end || (accumDx >= dx && accumDy >= dy)) {
            var fixupDx = Math.max(dx - accumDx, 0);
            var fixupDy = Math.max(dy - accumDy, 0);
            rootView().scrollBy(fixupDx, fixupDy);

            scrolling = false;
        }
        else {
            mozRequestAnimationFrame(nudgeScroll);
        }

        prevNow = now;
    }

    nudgeScroll(start);
    mozRequestAnimationFrame(nudgeScroll);
}
