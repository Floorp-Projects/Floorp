/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

const nsIWindow = Components.interfaces.nsIWindow;
const nsIDrawable = Components.interfaces.nsIDrawable;
const nsIGraphicsContext = Components.interfaces.nsIGraphicsContext;

win = Components.classes["mozilla.gfx.window.2"].createInstance(nsIWindow)
win.init(null)

drawable = win.QueryInterface(nsIDrawable)

gc = Components.classes["mozilla.gfx.graphicscontext.2"].createInstance(nsIGraphicsContext)
gc.init(drawable)

drawable.drawRectangle(gc, true, 0, 0, 199, 199)

