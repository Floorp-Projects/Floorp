/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

const nsIPixmap = Components.interfaces.nsIPixmap;
const nsIWindow = Components.interfaces.nsIWindow;
const nsIDrawable = Components.interfaces.nsIDrawable;
const nsIGraphicsContext = Components.interfaces.nsIGraphicsContext;

runapprun = Components.classes["run"].createInstance(Components.interfaces.nsIRunAppRun)

pixmap = Components.classes["mozilla.gfx.pixmap.2"].createInstance(nsIPixmap)
pixmap.init(null, 400, 400, 24)

drawable = pixmap.QueryInterface(nsIDrawable)

gc = Components.classes["mozilla.gfx.graphicscontext.2"].createInstance(nsIGraphicsContext)
gc.init(drawable)

drawable.drawRectangle(gc, true, 0, 0, 199, 199)

win = Components.classes["mozilla.gfx.window.2"].createInstance(nsIWindow)
win.init(null)

win.show()

drawable.copyTo(gc,
                win.QueryInterface(nsIDrawable),
                0, 0,
                0, 0, 199, 199)

runapprun.go()
